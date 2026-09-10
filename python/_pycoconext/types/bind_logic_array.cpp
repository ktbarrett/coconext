// NOLINTBEGIN(misc-include-cleaner)
#include <nanobind/make_iterator.h>
#include <nanobind/nanobind.h>
#include <nanobind/operators.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>

#include <algorithm>
#include <coconext/types/direction.hpp>
#include <coconext/types/dyn_signed.hpp>
#include <coconext/types/logic.hpp>
#include <coconext/types/logic_array.hpp>
#include <coconext/types/range.hpp>
#include <cstdint>
#include <format>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nb = nanobind;
using namespace nb::literals;

using namespace coconext::types;
using namespace coconext::types::detail;

namespace {

ResolveMethod string_to_resolve_method(std::string_view method) {
    if (method == "error") {
        return ResolveMethod::ERROR;
    } else if (method == "weak") {
        return ResolveMethod::WEAK;
    } else if (method == "zeros") {
        return ResolveMethod::ZEROS;
    } else if (method == "ones") {
        return ResolveMethod::ONES;
    } else if (method == "random") {
        return ResolveMethod::RANDOM;
    } else {
        throw nb::value_error("Unknown resolve method");
    }
}

// Parse the optional `range` arg of the constructor: a Range, an int (length,
// DOWNTO default), or None.
std::optional<Range> parse_range_arg(nb::object const& range_obj) {
    if (range_obj.is_none()) {
        return std::nullopt;
    }
    // TODO profile try/catch instead, maybe upstream a cast -> std::optional
    if (nb::isinstance<Range>(range_obj)) {
        return nb::cast<Range>(range_obj);
    }
    if (nb::isinstance<nb::int_>(range_obj)) {
        auto n = nb::cast<int64_t>(range_obj);
        if (n < 0) {
            throw nb::value_error("range length must be non-negative");
        }
        return Range{static_cast<Range::value_type>(n) - 1, Direction::DOWNTO, 0};
    }
    throw nb::type_error("Expected Range or int for parameter 'range'");
}

// Strip '_' and uppercase a string literal value (cocotb convention).
std::string normalize_logic_string(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c != '_') {
            out += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
    }
    return out;
}

// Parse a Python iterable into a std::vector of Logic/Bit.
template <typename Elem>
std::vector<Elem> parse_iterable(nb::iterable const& value, nb::object const& elem_class) {
    // TODO iterable -> list -> vector faster?
    std::vector<Elem> items;
    for (auto const& v : value) {
        items.push_back(nb::cast<Elem>(elem_class(v)));
    }
    return items;
}

// -- Python int <-> bit patterns ----------------------------------------------

enum class IntFit {
    UNSIGNED,
    SIGNED,
    EITHER,
    WRAP
};

IntFit parse_on_overflow(std::string_view on_overflow, IntFit strict) {
    if (on_overflow == "error") {
        return strict;
    }
    if (on_overflow == "wrap") {
        return IntFit::WRAP;
    }
    throw nb::value_error("Invalid value for on_overflow. Expected 'error' or 'wrap'.");
}

// Two's-complement pattern of a Python int in `width` bits. The range check
// runs on the Python int, so the value may be wider than any native type.
DynUInt bits_from_pyint(nb::handle value, size_t width, IntFit fit, std::string_view what) {
    nb::object v = nb::borrow(value);
    if (width == 0) {
        throw nb::value_error(std::format("{} will not fit in a null range", what).c_str());
    }
    nb::object const modulus = nb::int_(1) << nb::int_(width);
    if (fit != IntFit::WRAP) {
        nb::object const half = modulus >> nb::int_(1);
        nb::object lo = -half;
        nb::object hi = modulus;
        if (fit == IntFit::UNSIGNED) {
            lo = nb::int_(0);
        } else if (fit == IntFit::SIGNED) {
            hi = half;
        }
        if (v < lo || v >= hi) {
            throw nb::value_error(
                std::format(
                    "{} {} will not fit in {} bits",
                    what,
                    nb::cast<std::string>(nb::repr(v)),
                    width
                )
                    .c_str()
            );
        }
    }
    nb::object const bits = v & (modulus - nb::int_(1));
    if (width <= 64) {
        return DynUInt(width, nb::cast<uint64_t>(bits));
    }
    // TODO int -> bytes -> DynUInt?
    nb::str hex = nb::steal<nb::str>(PyNumber_ToBase(bits.ptr(), 16));
    if (!hex.is_valid()) {
        throw nb::python_error();
    }
    return DynUInt(width, nb::cast<std::string_view>(hex));
}

nb::int_ pyint_from_bits(DynUInt const& bits) {
    if (bits.width() == 0) {
        return nb::int_(0);
    }
    if (bits.width() <= 64) {
        return nb::int_(bits.to_native_integer<uint64_t>());
    }
    // TODO DynUInt -> bytes -> int faster?
    std::string const hex = bits.to_hexadecimal_string();
    PyObject* result = PyLong_FromString(hex.c_str(), nullptr, 16);
    if (result == nullptr) {
        throw nb::python_error();
    }
    return nb::steal<nb::int_>(result);
}

nb::int_ pyint_from_bits_signed(DynUInt const& bits) {
    size_t const width = bits.width();
    if (width <= 64) {
        return nb::int_(DynSInt(width, bits).to_native_integer<int64_t>());
    }
    // TODO DynUInt -> bytes -> int faster?
    nb::int_ value = pyint_from_bits(bits);
    if (bits.get_bit(width - 1)) {
        return nb::borrow<nb::int_>(value - (nb::int_(1) << nb::int_(width)));
    }
    return value;
}

template <typename VectorT>
VectorT vector_from_bits(DynUInt&& bits, Range range) {
    if constexpr (std::is_same_v<VectorT, BitVector>) {
        return BitVector(range, std::move(bits));
    } else {
        return LogicVector(BitVector(range, std::move(bits)), range);
    }
}

template <typename VectorT>
constexpr char const* py_name() {
    return std::is_same_v<VectorT, BitVector> ? "BitArray" : "LogicArray";
}

// Resolved bit pattern, or nullopt when the vector holds non-0/1 values.
template <typename VectorT>
std::optional<DynUInt> bits_of(VectorT const& self) {
    if constexpr (std::is_same_v<VectorT, BitVector>) {
        return storage(self);
    } else {
        auto resolved = resolve(self);
        if (!resolved.has_value()) {
            return std::nullopt;
        }
        return storage(std::move(*resolved));
    }
}

template <typename VectorT>
DynUInt checked_bits(VectorT const& self) {
    if (self.size() == 0) {
        throw nb::value_error("Cannot convert null vector to integer");
    }
    auto bits = bits_of(self);
    if (!bits.has_value()) {
        throw nb::value_error(
            std::format(
                "Can't convert {} to int: it contains non-0/1 values", py_name<VectorT>()
            )
                .c_str()
        );
    }
    return std::move(*bits);
}

template <typename VectorT>
nb::int_ to_unsigned(VectorT const& self) {
    return pyint_from_bits(checked_bits(self));
}

template <typename VectorT>
nb::int_ to_signed(VectorT const& self) {
    return pyint_from_bits_signed(checked_bits(self));
}

template <typename VectorT>
bool truthy(VectorT const& self) {
    if (self.size() == 0) {
        return false;
    }
    return checked_bits(self).popcount() != 0;
}

LogicVector from_unsigned_pyint(
    nb::int_ const& value, Range range, std::string_view on_overflow
) {
    IntFit const fit = parse_on_overflow(on_overflow, IntFit::UNSIGNED);
    if (value < nb::int_(0)) {
        throw nb::value_error("Expected unsigned integer, got negative value");
    }
    return vector_from_bits<LogicVector>(
        bits_from_pyint(value, range.length(), fit, "Unsigned integer"), range
    );
}

LogicVector from_signed_pyint(
    nb::int_ const& value, Range range, std::string_view on_overflow
) {
    IntFit const fit = parse_on_overflow(on_overflow, IntFit::SIGNED);
    return vector_from_bits<LogicVector>(
        bits_from_pyint(value, range.length(), fit, "Signed integer"), range
    );
}

template <typename VectorT>
VectorT from_bytes(
    nb::object const& value, nb::object const& range_obj, std::string_view byteorder
) {
    size_t const width = nb::len(value) * 8;
    auto range = parse_range_arg(range_obj);
    if (!range.has_value()) {
        range = logic_downto_range(width);
    } else if (range->length() != width) {
        throw nb::value_error("Range must be exactly equal to bytes width");
    }
    nb::handle const int_type(reinterpret_cast<PyObject*>(&PyLong_Type));
    nb::object const as_int = int_type.attr("from_bytes")(value, byteorder);
    return vector_from_bits<VectorT>(
        bits_from_pyint(as_int, width, IntFit::WRAP, "Bytes"), *range
    );
}

template <typename VectorT>
nb::bytes to_bytes(VectorT const& self, std::string_view byteorder) {
    if (byteorder != "big" && byteorder != "little") {
        throw nb::value_error("byteorder must be either 'big' or 'little'");
    }
    return nb::cast<nb::bytes>(
        to_unsigned(self).attr("to_bytes")((self.size() + 7) / 8, byteorder)
    );
}

// Mirrors cocotb's LogicArray.__format__: pad to the width of the array,
// then let Python apply the alternate form and digit grouping.
template <typename VectorT>
std::string format_vector(VectorT const& self, std::string_view spec) {
    if (spec.empty()) {
        return to_string(self);
    }
    std::string alternate;
    size_t base_len = 0;
    if (spec.starts_with('#')) {
        alternate = "#";
        spec.remove_prefix(1);
        base_len = 2;
    }
    std::string grouping;
    if (spec.starts_with('_') || spec.starts_with(',')) {
        grouping = spec.front();
        spec.remove_prefix(1);
    }
    if (spec != "b" && spec != "x" && spec != "X" && spec != "d" && spec != "o") {
        throw nb::value_error(
            std::format("Unsupported format specifier: '{}'", spec).c_str()
        );
    }
    nb::int_ const value = to_unsigned(self);
    size_t const n = self.size();
    size_t length = 0;
    std::string prefix;
    if (spec == "b") {
        length = n + (grouping.empty() ? 0 : (n - 1) / 4) + base_len;
    } else if (spec == "d") {
        length = (n + 9) / 10;
        length += (grouping.empty() ? 0 : (length - 1) / 3) + base_len;
        if (!alternate.empty()) {
            prefix = "0d";
            alternate.clear();
        }
    } else if (spec == "o") {
        length = (n + 2) / 3 + base_len;
        length += grouping.empty() ? 0 : (length - 1) / 4;
    } else {
        length = (n + 3) / 4;
        length += (grouping.empty() ? 0 : (length - 1) / 4) + base_len;
    }
    std::string const py_spec = std::format("{}0{}{}{}", alternate, length, grouping, spec);
    nb::str out =
        nb::steal<nb::str>(PyObject_Format(value.ptr(), nb::str(py_spec.c_str()).ptr()));
    if (!out.is_valid()) {
        throw nb::python_error();
    }
    return prefix + nb::cast<std::string>(out);
}

}  // namespace

void register_logic_array(nb::module_& m) {
    nb::object logic_class = m.attr("Logic");
    nb::object bit_class = m.attr("Bit");

    // -- LogicArray ----------------------------------------------------------

    nb::class_<LogicVector>(m, "LogicArray")
        .def(
            "__init__",
            [logic_class](
                LogicVector* self, nb::object const& value, nb::object const& range_obj
            ) {
                auto range = parse_range_arg(range_obj);

                if (nb::isinstance<nb::str>(value)) {
                    auto normalized =
                        normalize_logic_string(nb::cast<std::string_view>(value));
                    if (range.has_value()) {
                        if (range->length() != normalized.size()) {
                            throw nb::value_error(
                                "String literal length does not match range length"
                            );
                        }
                        new (self) LogicVector(normalized, *range);
                    } else {
                        new (self) LogicVector(normalized);
                    }
                } else if (nb::isinstance<LogicVector>(value)) {
                    auto const& other = nb::cast<LogicVector const&>(value);
                    if (range.has_value()) {
                        if (range->length() != other.range().length()) {
                            throw nb::value_error(
                                "Length of source does not match range length"
                            );
                        }
                        new (self) LogicVector(other, *range);
                    } else {
                        new (self) LogicVector(other);
                    }
                } else if (nb::isinstance<nb::int_>(value)) {
                    if (!range.has_value()) {
                        throw nb::type_error("Missing required arguments: 'range'");
                    }
                    new (self) LogicVector(
                        vector_from_bits<LogicVector>(
                            bits_from_pyint(
                                value, range->length(), IntFit::EITHER, "Value"
                            ),
                            *range
                        )
                    );
                } else if (nb::isinstance<nb::iterable>(value)) {
                    auto items =
                        parse_iterable<Logic>(nb::cast<nb::iterable>(value), logic_class);
                    if (range.has_value()) {
                        if (range->length() != items.size()) {
                            throw nb::value_error(
                                "Iterable length does not match range length"
                            );
                        }
                        new (self) LogicVector(items, *range);
                    } else {
                        new (self) LogicVector(items);
                    }
                } else {
                    throw nb::type_error("Unsupported type for LogicVector construction");
                }
            },
            "value"_a,
            "range"_a = nb::none()
        )

        .def_static(
            "from_unsigned",
            [](nb::int_ const& value,
               nb::object const& range_obj,
               std::string_view on_overflow) {
                auto range = parse_range_arg(range_obj);
                if (!range.has_value()) {
                    throw nb::type_error("Missing required arguments: 'range'");
                }
                return from_unsigned_pyint(value, *range, on_overflow);
            },
            "value"_a,
            "range"_a,
            nb::kw_only(),
            "on_overflow"_a = "error"
        )
        .def_static(
            "from_signed",
            [](nb::int_ const& value,
               nb::object const& range_obj,
               std::string_view on_overflow) {
                auto range = parse_range_arg(range_obj);
                if (!range.has_value()) {
                    throw nb::type_error("Missing required arguments: 'range'");
                }
                return from_signed_pyint(value, *range, on_overflow);
            },
            "value"_a,
            "range"_a,
            nb::kw_only(),
            "on_overflow"_a = "error"
        )
        .def_static(
            "from_bytes",
            &from_bytes<LogicVector>,
            "value"_a,
            "range"_a = nb::none(),
            nb::kw_only(),
            "byteorder"_a
        )

        // -- to conversions -----
        .def("to_unsigned", &to_unsigned<LogicVector>)
        .def("to_signed", &to_signed<LogicVector>)
        .def("to_bytes", &to_bytes<LogicVector>, nb::kw_only(), "byteorder"_a)

        // -- range / left / direction / right / is_resolvable ---------------
        // NOTE: set_range is removed; Vectors are immutable in size.
        .def_prop_rw(
            "range",
            [](LogicVector const& self) { return self.range(); },
            [](LogicVector& self, nb::object new_range_obj) {
                if (!nb::isinstance<Range>(new_range_obj)) {
                    throw nb::type_error("range argument must be of type 'Range'");
                }

                Range new_range = nb::cast<Range>(new_range_obj);
                if (new_range.length() != self.size()) {
                    throw nb::value_error("Range size mismatch");
                }

                self = LogicVector(self, new_range);
            }
        )
        .def_prop_ro("left", [](LogicVector const& self) { return self.range().left; })
        .def_prop_ro("right", [](LogicVector const& self) { return self.range().right; })
        .def_prop_ro(
            "direction",
            [](LogicVector const& self) { return to_string(self.range().direction); }
        )
        .def_prop_ro(
            "is_resolvable",
            [](LogicVector const& self) {
                return resolve(self, ResolveMethod::ERROR).has_value();
            }
        )

        // -- Container protocol ---------------------------------------------
        .def("__len__", [](LogicVector const& self) { return self.range().length(); })
        .def(
            "__iter__",
            [](LogicVector& self) {
                return nb::make_iterator(
                    nb::type<LogicVector>(), "LogicArrayIterator", self.begin(), self.end()
                );
            },
            nb::keep_alive<0, 1>()
        )
        .def(
            "__reversed__",
            [](LogicVector& self) {
                return nb::make_iterator(
                    nb::type<LogicVector>(),
                    "LogicArrayReverseIterator",
                    self.rbegin(),
                    self.rend()
                );
            },
            nb::keep_alive<0, 1>()
        )
        .def(
            "__contains__",
            [](LogicVector const& self, Logic const& v) {
                return std::ranges::find(self, v) != self.end();
            }
        )

        // -- Indexing -------------------------------------------------------
        .def(
            "__getitem__",
            [](LogicVector const& self, Range::value_type idx) { return self[idx]; },
            nb::arg().noconvert()
        )
        .def(
            "__getitem__",
            [](LogicVector& self, nb::slice slice) {
                auto r = self.range();
                Range::value_type start =
                    slice.attr("start").is_none()
                        ? r.left
                        : nb::cast<Range::value_type>(slice.attr("start"));
                Range::value_type stop =
                    slice.attr("stop").is_none()
                        ? r.right
                        : nb::cast<Range::value_type>(slice.attr("stop"));

                if (!slice.attr("step").is_none()) {
                    throw nb::index_error("do not specify step");
                }
                if (r.direction == Direction::DOWNTO && start < stop) {
                    throw nb::index_error(
                        "slice direction does not match array direction (expected start >= "
                        "stop for DOWNTO)"
                    );
                }
                if (r.direction == Direction::TO && start > stop) {
                    throw nb::index_error(
                        "slice direction does not match array direction (expected start <= "
                        "stop for TO)"
                    );
                }

                Range sub{start, r.direction, stop};
                auto slice_view = self[sub];
                return LogicVector(slice_view, sub);
            }
        )
        .def(
            "__setitem__",
            [logic_class](
                LogicVector& self, Range::value_type idx, nb::object const& value
            ) { self[idx] = nb::cast<Logic>(logic_class(value)); },
            nb::arg().noconvert(),
            nb::arg()
        )
        .def(
            "__setitem__",
            [logic_class](LogicVector& self, nb::slice slice, nb::object const& value) {
                auto r = self.range();
                Range::value_type start =
                    slice.attr("start").is_none()
                        ? r.left
                        : nb::cast<Range::value_type>(slice.attr("start"));
                Range::value_type stop =
                    slice.attr("stop").is_none()
                        ? r.right
                        : nb::cast<Range::value_type>(slice.attr("stop"));

                if (!slice.attr("step").is_none()) {
                    throw nb::index_error("do not specify step");
                }

                if (r.direction == Direction::DOWNTO && start < stop) {
                    throw nb::index_error(
                        "slice direction does not match array direction (expected start >= "
                        "stop for DOWNTO)"
                    );
                }
                if (r.direction == Direction::TO && start > stop) {
                    throw nb::index_error(
                        "slice direction does not match array direction (expected start <= "
                        "stop for TO)"
                    );
                }

                Range sub{start, r.direction, stop};
                auto slice_view = self[sub];
                nb::handle la_class = nb::type<LogicVector>();
                auto rhs = nb::cast<LogicVector>(la_class(value, nb::cast(sub.length())));
                slice_view = rhs;
            }
        )

        // -- list-like search methods ---------------------------------------
        .def(
            "index",
            [logic_class](
                LogicVector const& self,
                nb::object const& v,
                std::optional<int64_t> start,
                std::optional<int64_t> stop
            ) {
                Logic logic_v;
                try {
                    logic_v = nb::cast<Logic>(logic_class(v));
                } catch (...) {
                    PyErr_Clear();
                    throw nb::value_error("value not in array");
                }

                auto r = self.range();

                auto to_offset = [&](std::optional<int64_t> idx, int64_t def) {
                    if (!idx) {
                        return def;
                    }
                    int64_t off = (r.direction == Direction::DOWNTO) ? (r.left - *idx)
                                                                     : (*idx - r.left);
                    return std::max<int64_t>(
                        0, std::min<int64_t>(off, static_cast<int64_t>(self.size()))
                    );
                };

                auto start_it = self.begin() + to_offset(start, 0);
                auto stop_it = self.begin() + to_offset(stop, self.size());

                auto it = std::find(start_it, stop_it, logic_v);

                if (it == stop_it) {
                    throw nb::value_error("value not in array");
                }

                int64_t found_off = std::distance(self.begin(), it);
                return (r.direction == Direction::DOWNTO) ? (r.left - found_off)
                                                          : (r.left + found_off);
            },
            "value"_a,
            "start"_a = nb::none(),
            "stop"_a = nb::none()
        )
        .def(
            "count",
            [](LogicVector const& self, Logic const& v) {
                return std::ranges::count(self, v);
            }
        )

        // -- Bitwise --------------------------------------------------------
        .def(
            "__and__",
            [](LogicVector const& a, LogicVector const& b) { return a & b; },
            nb::is_operator()
        )
        .def(
            "__or__",
            [](LogicVector const& a, LogicVector const& b) { return a | b; },
            nb::is_operator()
        )
        .def(
            "__xor__",
            [](LogicVector const& a, LogicVector const& b) { return a ^ b; },
            nb::is_operator()
        )
        .def(
            "__invert__", [](LogicVector const& a) { return ~a; }, nb::is_operator()
        )

        // -- Comparison -----------------------------------------------------
        .def(
            "__eq__",
            [](LogicVector const& self, LogicVector const& other) {
                if (self.size() != other.size()) {
                    return false;
                }
                return std::equal(self.begin(), self.end(), other.begin());
            },
            nb::is_operator()
        )
        .def(
            "__eq__",
            [](LogicVector const& self, std::string_view other) {
                return to_string(self) == normalize_logic_string(other);
            },
            nb::is_operator()
        )
        .def(
            "__eq__",
            [](LogicVector const& self, nb::int_ const& other) {
                if (self.size() == 0) {
                    return false;
                }
                auto bits = bits_of(self);
                if (!bits.has_value()) {
                    return false;
                }
                nb::int_ const mine = other < nb::int_(0) ? pyint_from_bits_signed(*bits)
                                                          : pyint_from_bits(*bits);
                return mine.equal(other);
            },
            nb::is_operator()
        )
        .def(
            "__eq__",
            [](LogicVector const& self, nb::handle other) -> nb::object {
                if (!nb::isinstance<nb::list>(other) && !nb::isinstance<nb::tuple>(other)) {
                    return nb::not_implemented();
                }
                try {
                    nb::handle la_class = nb::type<LogicVector>();
                    auto rhs = nb::cast<LogicVector>(
                        la_class(nb::cast<nb::object>(other), nb::none())
                    );
                    return nb::cast(self == rhs);
                } catch (...) {
                    return nb::cast(false);
                }
            },
            nb::is_operator()
        )

        // -- Resolution -----------------------------------------------------
        .def(
            "resolve",
            [](LogicVector const& self, std::string_view resolver) {
                auto method = string_to_resolve_method(resolver);

                Vector<Logic> resolved{self.range()};
                auto out = resolved.begin();

                for (auto const& v : self) {
                    auto r = v.resolve(method);
                    Logic l;

                    if (!r) {
                        if (resolver == "weak") {
                            if (v == Logic("W")) {
                                l = Logic("X");
                            } else {
                                l = v;
                            }
                        } else {
                            throw nb::value_error(
                                "Cannot resolve LogicArray with the given resolver."
                            );
                        }
                    } else {
                        l = *r;
                    }

                    *out++ = l;
                }

                return resolved;
            },
            "resolver"_a
        )

        // -- Special methods ------------------------------------------------
        .def("__str__", [](LogicVector const& self) { return to_string(self); })
        .def("__int__", &to_unsigned<LogicVector>)
        .def("__bool__", &truthy<LogicVector>)
        .def("__index__", &to_unsigned<LogicVector>)
        .def(
            "__repr__",
            [](LogicVector const& self) {
                return std::format(
                    "LogicArray('{}', Range({}, '{}', {}))",
                    to_string(self),
                    self.range().left,
                    to_string(self.range().direction),
                    self.range().right
                );
            }
        )
        .def("__format__", &format_vector<LogicVector>)
        // TODO this should be implemented
        .def(
            "__copy__",
            [](LogicVector const&) -> LogicVector {
                PyErr_SetString(
                    PyExc_NotImplementedError, "copy.copy on LogicArray is not supported"
                );
                throw nb::python_error();
            }
        )
        .def("__deepcopy__", [](LogicVector const& self, nb::dict /* memo */) {
            return LogicVector(self);
        });

    // -- BitArray ------------------------------------------------------------

    nb::class_<BitVector>(m, "BitArray")
        .def(
            "__init__",
            [bit_class](
                BitVector* self, nb::object const& value, nb::object const& range_obj
            ) {
                auto range = parse_range_arg(range_obj);

                if (nb::isinstance<nb::str>(value)) {
                    auto normalized =
                        normalize_logic_string(nb::cast<std::string_view>(value));
                    if (range.has_value()) {
                        if (range->length() != normalized.size()) {
                            throw nb::value_error(
                                "String literal length does not match range length"
                            );
                        }
                        new (self) BitVector(normalized, *range);
                    } else {
                        new (self) BitVector(normalized);
                    }
                } else if (nb::isinstance<BitVector>(value)) {
                    auto const& other = nb::cast<BitVector const&>(value);
                    if (range.has_value()) {
                        if (range->length() != other.range().length()) {
                            throw nb::value_error(
                                "Length of source does not match range length"
                            );
                        }
                        new (self) BitVector(other, *range);
                    } else {
                        new (self) BitVector(other);
                    }
                } else if (nb::isinstance<nb::int_>(value)) {
                    if (!range.has_value()) {
                        throw nb::type_error("Missing required arguments: 'range'");
                    }
                    new (self) BitVector(
                        vector_from_bits<BitVector>(
                            bits_from_pyint(
                                value, range->length(), IntFit::EITHER, "Value"
                            ),
                            *range
                        )
                    );
                } else if (nb::isinstance<nb::iterable>(value)) {
                    auto items =
                        parse_iterable<Bit>(nb::cast<nb::iterable>(value), bit_class);
                    if (range.has_value()) {
                        if (range->length() != items.size()) {
                            throw nb::value_error(
                                "Iterable length does not match range length"
                            );
                        }
                        new (self) BitVector(items, *range);
                    } else {
                        new (self) BitVector(items);
                    }
                } else {
                    throw nb::type_error("Unsupported type for LogicVector construction");
                }
            },
            "value"_a,
            "range"_a = nb::none()
        )
        .def_prop_ro("range", [](BitVector const& self) { return self.range(); })
        .def_prop_ro("left", [](BitVector const& self) { return self.range().left; })
        .def_prop_ro("right", [](BitVector const& self) { return self.range().right; })
        .def_prop_ro(
            "direction",
            [](BitVector const& self) { return to_string(self.range().direction); }
        )
        .def("__len__", [](BitVector const& self) { return self.range().length(); })
        .def(
            "__iter__",
            [](BitVector const& self) {
                return nb::make_iterator(
                    nb::type<BitVector>(), "BitArrayIterator", self.begin(), self.end()
                );
            },
            nb::keep_alive<0, 1>()
        )
        .def(
            "__reversed__",
            [](BitVector const& self) {
                return nb::make_iterator(
                    nb::type<BitVector>(),
                    "BitArrayReverseIterator",
                    self.rbegin(),
                    self.rend()
                );
            },
            nb::keep_alive<0, 1>()
        )
        .def(
            "__contains__",
            [](BitVector const& self, Bit const& v) {
                return std::ranges::find(self, v) != self.end();
            }
        )
        .def(
            "__getitem__",
            [](BitVector const& self, Range::value_type idx) { return self[idx]; },
            nb::arg().noconvert()
        )
        .def(
            "__getitem__",
            [](BitVector& self, nb::slice slice) {
                auto r = self.range();
                Range::value_type start;
                Range::value_type stop;
                if (slice.attr("start").is_none()) {
                    start = r.left;
                } else {
                    start = nb::cast<Range::value_type>(slice.attr("start"));
                }
                if (slice.attr("stop").is_none()) {
                    stop = r.right;
                } else {
                    stop = nb::cast<Range::value_type>(slice.attr("stop"));
                }
                if (!slice.attr("step").is_none()) {
                    throw nb::index_error("do not specify step");
                }

                Range sub{start, r.direction, stop};
                auto slice_view = self[sub];
                return BitVector(slice_view, sub);
            }
        )
        .def(
            "__setitem__",
            [bit_class](BitVector& self, Range::value_type idx, nb::object const& value) {
                self[idx] = nb::cast<Bit>(bit_class(value));
            },
            nb::arg().noconvert(),
            nb::arg()
        )
        .def(
            "__setitem__",
            [bit_class](BitVector& self, nb::slice slice, nb::object const& value) {
                auto r = self.range();
                Range::value_type start;
                Range::value_type stop;
                if (slice.attr("start").is_none()) {
                    start = r.left;
                } else {
                    start = nb::cast<Range::value_type>(slice.attr("start"));
                }
                if (slice.attr("stop").is_none()) {
                    stop = r.right;
                } else {
                    stop = nb::cast<Range::value_type>(slice.attr("stop"));
                }
                if (!slice.attr("step").is_none()) {
                    throw nb::index_error("do not specify step");
                }

                Range sub{start, r.direction, stop};
                auto slice_view = self[sub];
                nb::handle ba_class = nb::type<BitVector>();
                auto rhs = nb::cast<BitVector>(ba_class(value, nb::cast(sub.length())));
                slice_view = rhs;
            }
        )
        .def(
            "index",
            [bit_class](
                BitVector const& self,
                nb::object const& v,
                std::optional<int64_t> start,
                std::optional<int64_t> stop
            ) {
                Bit bit_v;
                try {
                    bit_v = nb::cast<Bit>(bit_class(v));
                } catch (...) {
                    PyErr_Clear();
                    throw nb::value_error("value not in array");
                }

                auto r = self.range();

                auto to_offset = [&](std::optional<int64_t> idx, int64_t def) {
                    if (!idx) {
                        return def;
                    }
                    int64_t off = (r.direction == Direction::DOWNTO) ? (r.left - *idx)
                                                                     : (*idx - r.left);
                    return std::max<int64_t>(
                        0, std::min<int64_t>(off, static_cast<int64_t>(self.size()))
                    );
                };

                auto start_it = self.begin() + to_offset(start, 0);
                auto stop_it = self.begin() + to_offset(stop, self.size());

                auto it = std::find(start_it, stop_it, bit_v);

                if (it == stop_it) {
                    throw nb::value_error("value not in array");
                }

                int64_t found_off = std::distance(self.begin(), it);
                return (r.direction == Direction::DOWNTO) ? (r.left - found_off)
                                                          : (r.left + found_off);
            },
            "value"_a,
            "start"_a = nb::none(),
            "stop"_a = nb::none()
        )
        .def(
            "count",
            [](BitVector const& self, Bit const& v) { return std::ranges::count(self, v); }
        )
        .def(
            "__and__",
            [](BitVector const& a, BitVector const& b) { return a & b; },
            nb::is_operator()
        )
        .def(
            "__or__",
            [](BitVector const& a, BitVector const& b) { return a | b; },
            nb::is_operator()
        )
        .def(
            "__xor__",
            [](BitVector const& a, BitVector const& b) { return a ^ b; },
            nb::is_operator()
        )
        .def(
            "__invert__", [](BitVector const& a) { return ~a; }, nb::is_operator()
        )
        .def(
            "__eq__",
            [](BitVector const& self, BitVector const& other) {
                if (self.size() != other.size()) {
                    return false;
                }
                return std::equal(self.begin(), self.end(), other.begin());
            },
            nb::is_operator()
        )
        .def(
            "__eq__",
            [](BitVector const& self, std::string_view other) {
                return to_string(self) == normalize_logic_string(other);
            },
            nb::is_operator()
        )
        .def(
            "__eq__",
            [](BitVector const& self, nb::handle other) {
                if (!nb::isinstance<nb::list>(other) && !nb::isinstance<nb::tuple>(other)) {
                    return false;
                }
                nb::handle ba_class = nb::type<BitVector>();
                auto rhs =
                    nb::cast<BitVector>(ba_class(nb::cast<nb::object>(other), nb::none()));
                return self == rhs;
            },
            nb::is_operator()
        )
        .def(
            "resolve",
            [](BitVector const& self, std::string_view resolver) {
                auto result =
                    coconext::types::resolve(self, string_to_resolve_method(resolver));
                return result.value();
            },
            "resolver"_a
        )
        .def("__str__", [](BitVector const& self) { return to_string(self); })
        .def(
            "__repr__",
            [](BitVector const& self) {
                return std::format(
                    "BitArray('{}', Range({}, '{}', {}))",
                    to_string(self),
                    self.range().left,
                    to_string(self.range().direction),
                    self.range().right
                );
            }
        )
        .def("__format__", &format_vector<BitVector>)
        .def("__copy__", [](BitVector const& self) { return BitVector(self); })
        .def("__deepcopy__", [](BitVector const& self, nb::dict /* memo */) {
            return BitVector(self);
        });
}

// NOLINTEND(misc-include-cleaner)
