// NOLINTBEGIN(misc-include-cleaner)
#include <nanobind/make_iterator.h>
#include <nanobind/nanobind.h>
#include <nanobind/operators.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>

#include <algorithm>
#include <cctype>
#include <coconext/types/direction.hpp>
#include <coconext/types/logic.hpp>
#include <coconext/types/logic_array.hpp>
#include <coconext/types/range.hpp>
#include <cstdint>
#include <format>
#include <iterator>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace nb = nanobind;
using namespace nb::literals;

using namespace coconext::types;

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
    std::vector<Elem> items;
    for (auto const& v : value) {
        items.push_back(nb::cast<Elem>(elem_class(v)));
    }
    return items;
}

}  // namespace

static LogicVector logic_array_from_unsigned(
    int64_t value, Range range, std::string_view on_overflow
) {
    if (on_overflow != "error" && on_overflow != "wrap") {
        throw nb::value_error("on_overflow only accepts [error] or [wrap]");
    }

    if (value < 0) {
        throw nb::value_error("Expected unsigned integer, got negative value");
    }

    uint64_t u_value = static_cast<uint64_t>(value);
    int width = range.length();

    if (width == 0) {
        throw nb::value_error(
            "Unsigned integer will not fit in a LogicArray with bounds of length 0"
        );
    }

    if (on_overflow == "wrap") {
        if (width < 64) {
            u_value %= (1ULL << width);
        }
    } else {
        // default on_overflow value is "error"
        if (width < 64 && u_value >= (1ULL << width)) {
            throw nb::value_error(
                "Unsigned integer will not fit in a LogicArray with given bounds"
            );
        }
    }

    uint64_t mask = (width >= 64) ? ~0ULL : (1ULL << width) - 1;
    uint64_t masked_value = u_value & mask;
    std::string s = std::format("{:0{}b}", masked_value, width);

    return LogicVector(s, range);
}

static LogicVector logic_array_from_signed(
    int64_t value, Range range, std::string_view on_overflow
) {
    if (on_overflow != "error" && on_overflow != "wrap") {
        throw nb::value_error("on_overflow only accepts [error] or [wrap]");
    }

    int width = range.length();

    if (width == 0) {
        throw nb::value_error(
            "Signed integer will not fit in a LogicArray with bounds of length 0"
        );
    }

    if (on_overflow == "error") {
        if (width < 64) {
            int64_t limit = 1LL << (width - 1);
            if (value < -limit || value >= limit) {
                throw nb::value_error(
                    "Signed integer will not fit in a LogicArray with given bounds"
                );
            }
        }
    }

    uint64_t mask = (width >= 64) ? ~0ULL : (1ULL << width) - 1;
    uint64_t masked_value = static_cast<uint64_t>(value) & mask;
    std::string s = std::format("{:0{}b}", masked_value, width);

    return LogicVector(s, range);
}

static nb::object logic_array_to_unsigned(LogicVector const& self) {
    auto resolved_opt = resolve(self);
    if (!resolved_opt.has_value()) {
        throw nb::value_error(
            "Cannot convert LogicArray to integer: contains non-resolvable bits"
        );
    }

    int width = self.size();
    if (width == 0) {
        throw nb::value_error("Cannot convert empty LogicArray to integer");
    }

    std::string binary_str = to_string(*resolved_opt);

    nb::object py_int = nb::module_::import_("builtins").attr("int");
    return py_int(binary_str, 2);
}

static nb::object logic_array_to_signed(LogicVector const& self) {
    nb::object val = logic_array_to_unsigned(self);
    int width = self.size();

    nb::object py_int = nb::module_::import_("builtins").attr("int");
    nb::object one = py_int(1);
    nb::object zero = py_int(0);

    nb::object sign_bit_mask = one.attr("__lshift__")(width - 1);
    nb::object is_negative = val.attr("__and__")(sign_bit_mask);

    if (!is_negative.equal(zero)) {
        nb::object extension_mask = one.attr("__lshift__")(width);
        val = val.attr("__sub__")(extension_mask);
    }

    return val;
}

static LogicVector logic_array_from_bytes(
    nb::object value_obj, nb::object range_obj, std::string_view byteorder
) {
    auto const size = nb::len(value_obj);
    auto const expected_width = static_cast<size_t>(size) * 8;

    auto range = parse_range_arg(range_obj);
    if (!range.has_value()) {
        range =
            Range(static_cast<Range::value_type>(expected_width) - 1, Direction::DOWNTO, 0);
    } else if (range->length() != expected_width) {
        throw nb::value_error("Range must be exactly equal to bytes width");
    }

    nb::object py_builtins = nb::module_::import_("builtins");
    nb::object py_int = py_builtins.attr("int");
    nb::object py_format = py_builtins.attr("format");
    nb::object int_val = py_int.attr("from_bytes")(value_obj, byteorder);
    std::string fmt_spec = std::format("0{}b", expected_width);
    nb::object bin_str_obj = py_format(int_val, fmt_spec.c_str());
    std::string bin_str = nb::cast<std::string>(bin_str_obj);

    return LogicVector(bin_str, *range);
}

static nb::bytes logic_array_to_bytes(LogicVector const& self, std::string_view byteorder) {
    if (byteorder != "big" && byteorder != "little") {
        throw nb::value_error("byteorder must be either 'big' or 'little'");
    }

    auto resolved_opt = resolve(self);
    if (!resolved_opt.has_value()) {
        throw nb::value_error(
            "Cannot convert LogicArray to bytes: contains non-resolvable bits ('X', 'Z', "
            "etc.)"
        );
    }

    std::string bin_str = to_string(self);
    int bit_len = self.size();

    int num_bytes = (bit_len + 7) / 8;
    if (num_bytes == 0) {
        return nb::bytes("", 0);
    }

    int pad_len = num_bytes * 8 - bit_len;
    std::string padded;
    padded.reserve(num_bytes * 8);
    padded.append(pad_len, '0');
    padded.append(bin_str);

    std::string out_bytes;
    out_bytes.resize(num_bytes);

    for (int i = 0; i < num_bytes; ++i) {
        uint8_t b = 0;
        for (int j = 0; j < 8; ++j) {
            b = (b << 1) | (padded[i * 8 + j] - '0');
        }

        if (byteorder == "big") {
            out_bytes[i] = static_cast<char>(b);
        } else {
            out_bytes[num_bytes - 1 - i] = static_cast<char>(b);
        }
    }

    return nb::bytes(out_bytes.data(), num_bytes);
}

static std::string format_bit_vector(BitVector const& resolved, std::string_view spec) {
    std::string binary_str = to_string(resolved);
    uint64_t val = std::stoull(binary_str, nullptr, 2);

    bool alt = false;
    char group = '\0';
    char type = '\0';

    for (char c : spec) {
        if (c == '#') {
            alt = true;
        } else if (c == '_' || c == ',') {
            group = c;
        } else if (std::isalpha(c)) {
            type = c;
        }
    }

    if (type == '\0') {
        type = 'd';
    }

    if (type != 'b' && type != 'd' && type != 'o' && type != 'x' && type != 'X') {
        throw nb::value_error("Invalid format specifier");
    }

    int num_bits = resolved.size();
    int pad_width = 0;
    int group_size = 4;

    if (type == 'b') {
        pad_width = num_bits;
    } else if (type == 'o') {
        pad_width = (num_bits + 2) / 3;
        pad_width = (num_bits + 2) / 3;

        int expected_python_width = pad_width;
        if (alt) {
            expected_python_width += 2;
        }
        if (group != '\0') {
            expected_python_width += (pad_width - 1) / 3;
        }

        while (true) {
            int current_width = pad_width;
            if (alt) {
                current_width += 2;
            }
            if (group != '\0') {
                current_width += (pad_width - 1) / 4;
            }
            if (current_width >= expected_python_width) {
                break;
            }
            pad_width++;
        }
    } else if (type == 'x' || type == 'X') {
        pad_width = (num_bits + 3) / 4;
    } else if (type == 'd') {
        pad_width = (num_bits * 301 + 999) / 1000;
        group_size = 3;
    }

    std::string raw;
    if (type == 'b') {
        raw = std::format("{:0{}b}", val, pad_width);
    } else if (type == 'o') {
        raw = std::format("{:0{}o}", val, pad_width);
    } else if (type == 'x') {
        raw = std::format("{:0{}x}", val, pad_width);
    } else if (type == 'X') {
        raw = std::format("{:0{}X}", val, pad_width);
    } else if (type == 'd') {
        raw = std::format("{:0{}d}", val, pad_width);
    }

    std::string grouped;
    if (group != '\0') {
        int n = raw.size();
        for (int i = 0; i < n; ++i) {
            grouped.push_back(raw[i]);
            int remaining = n - 1 - i;
            if (remaining > 0 && remaining % group_size == 0) {
                grouped.push_back(group);
            }
        }
    } else {
        grouped = raw;
    }

    std::string prefix;
    if (alt) {
        if (type == 'b') {
            prefix = "0b";
        } else if (type == 'o') {
            prefix = "0o";
        } else if (type == 'x') {
            prefix = "0x";
        } else if (type == 'X') {
            prefix = "0X";
        } else if (type == 'd') {
            prefix = "0d";
        }
    }

    return prefix + grouped;
}

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
                        throw nb::type_error("int construction requires a range");
                    }

                    int width = range->length();
                    int64_t v;

                    v = nb::cast<int64_t>(value);

                    if (width < 64) {
                        if (v < 0) {
                            uint64_t abs_v = (v == std::numeric_limits<int64_t>::min())
                                               ? (1ULL << 63)
                                               : static_cast<uint64_t>(-v);

                            if (width == 0 || abs_v > (1ULL << (width - 1))) {
                                throw nb::value_error(
                                    "Value cannot fit in specified number of bits"
                                );
                            }
                        } else {
                            if (static_cast<uint64_t>(v) >= (1ULL << width)) {
                                throw nb::value_error(
                                    "Value cannot fit in specified number of bits"
                                );
                            }
                        }
                    }
                    uint64_t mask = (width >= 64) ? ~0ULL : (1ULL << width) - 1;
                    uint64_t v_2s_comp = static_cast<uint64_t>(v) & mask;
                    std::string s = std::format("{:0{}b}", v_2s_comp, width);

                    new (self) LogicVector(s, *range);
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

        // --class method conversions------
        .def_static(
            "from_unsigned",
            [](nb::object value_obj, nb::object range_obj, std::string_view on_overflow) {
                auto range = parse_range_arg(range_obj);

                if (!nb::isinstance<nb::int_>(value_obj)) {
                    throw nb::type_error("Expected int for parameter 'value'");
                }

                int64_t value = nb::cast<int64_t>(value_obj);

                return logic_array_from_unsigned(value, *range, on_overflow);
            },
            "value"_a,
            "range"_a,
            nb::kw_only(),
            "on_overflow"_a = "error"
        )

        .def_static(
            "from_signed",
            [](nb::object value_obj, nb::object range_obj, std::string_view on_overflow) {
                auto range = parse_range_arg(range_obj);

                if (!nb::isinstance<nb::int_>(value_obj)) {
                    throw nb::type_error("Expected int for parameter 'value'");
                }

                int64_t value = nb::cast<int64_t>(value_obj);

                return logic_array_from_signed(value, *range, on_overflow);
            },
            "value"_a,
            "range"_a,
            nb::kw_only(),
            "on_overflow"_a = "error"
        )

        .def_static(
            "from_bytes",
            &logic_array_from_bytes,
            "value"_a,
            "range"_a = nb::none(),
            nb::kw_only(),
            "byteorder"_a
        )

        // -- to conversions -----
        .def("to_unsigned", &logic_array_to_unsigned)
        .def("to_signed", &logic_array_to_signed)
        .def("to_bytes", &logic_array_to_bytes, nb::kw_only(), "byteorder"_a)

        // TODO all these deprecations should be removed
        // --cocotb deprecarted-------
        .def_prop_rw(
            "integer",
            [](LogicVector const& self) {
                PyErr_WarnEx(
                    PyExc_DeprecationWarning,
                    "`logic_array.integer` getter is deprecated. Use "
                    "`logic_array.to_unsigned()` instead.",
                    1
                );

                return logic_array_to_unsigned(self);
            },

            [](LogicVector& self, int value) {
                PyErr_WarnEx(
                    PyExc_DeprecationWarning,
                    "`logic_array.integer = value` setter is deprecated. Use "
                    "`logic_array[:] = value` instead.",
                    1
                );
                LogicVector new_vec =
                    logic_array_from_unsigned(value, self.range(), "error");
                std::ranges::copy(new_vec, self.begin());
            }
        )

        .def_prop_rw(
            "signed_integer",
            [](LogicVector const& self) {
                PyErr_WarnEx(
                    PyExc_DeprecationWarning,
                    "`logic_array.signed_integer` getter is deprecated. Use "
                    "`logic_array.to_signed()` instead.",
                    1
                );

                return logic_array_to_signed(self);
            },

            [](LogicVector& self, int value) {
                PyErr_WarnEx(
                    PyExc_DeprecationWarning,
                    "`logic_array.signed_integer = value` setter is deprecated. "
                    "Use `logic_array[:] = LogicArray.from_signed(value, "
                    "len(logic_array))` instead.",
                    1
                );
                LogicVector new_vec = logic_array_from_signed(value, self.range(), "error");
                std::ranges::copy(new_vec, self.begin());
            }
        )

        .def_prop_rw(
            "binstr",
            [](LogicVector const& self) {
                PyErr_WarnEx(
                    PyExc_DeprecationWarning,
                    "`logic_array.binstr` getter is deprecated. Use `str(logic_array)` "
                    "instead.",
                    1
                );

                return to_string(self);
            },

            [](LogicVector& self, std::string value) {
                PyErr_WarnEx(
                    PyExc_DeprecationWarning,
                    "`logic_array.binstr = value` setter is deprecated. Use "
                    "`logic_array[:] = value` instead.",
                    1
                );
                std::string normalized = normalize_logic_string(value);
                LogicVector new_vec(normalized);
                if (new_vec.size() != self.size()) {
                    throw nb::value_error("String length must match the LogicArray length");
                }
                std::ranges::copy(new_vec, self.begin());
            }
        )

        .def_prop_rw(
            "buff",
            [](LogicVector const& self) {
                PyErr_WarnEx(
                    PyExc_DeprecationWarning,
                    "`logic_array.buff` getter is deprecated. "
                    "Use `logic_array.to_bytes(byteorder=\"big\")` instead.",
                    1
                );

                return logic_array_to_bytes(self, "big");
            },

            [](LogicVector& self, nb::object value) {
                PyErr_WarnEx(
                    PyExc_DeprecationWarning,
                    "`logic_array.buff = value` setter is deprecated. "
                    "Use `logic_array[:] = LogicArray.from_bytes(value, len(logic_array), "
                    "byteorder=\"big\")` instead.",
                    1
                );

                LogicVector new_vec =
                    logic_array_from_bytes(value, nb::cast(self.range()), "big");
                std::ranges::copy(new_vec, self.begin());
            }
        )

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
            [](LogicVector const& self, nb::int_ other) {
                if (!resolve(self, ResolveMethod::ERROR).has_value()) {
                    return false;
                }

                try {
                    if (logic_array_to_signed(self).equal(other)) {
                        return true;
                    }

                    if (logic_array_to_unsigned(self).equal(other)) {
                        return true;
                    }
                } catch (...) {
                    return false;
                }

                return false;
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
        .def("__int__", &logic_array_to_unsigned)
        .def(
            "__bool__",
            [](LogicVector const& self) {
                auto resolved_opt = resolve(self, ResolveMethod::ERROR);
                if (!resolved_opt.has_value()) {
                    throw nb::value_error(
                        "Cannot convert LogicArray to bool: contains non-resolvable bits"
                    );
                }

                std::string s = to_string(*resolved_opt);
                return s.find('1') != std::string::npos;
            }
        )
        .def("__index__", &logic_array_to_unsigned)
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
        .def(
            "__format__",
            [](LogicVector const& self, std::string_view spec) -> std::string {
                if (spec.empty()) {
                    return to_string(self);
                }
                auto resolved_opt = resolve(self, ResolveMethod::ERROR);
                if (!resolved_opt.has_value()) {
                    throw nb::value_error(
                        "Cannot format LogicArray: contains non-resolvable bits"
                    );
                }
                return format_bit_vector(*resolved_opt, spec);
            }
        )
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
                        throw nb::type_error("int construction requires a range");
                    }

                    int width = range->length();
                    int64_t v = nb::cast<int64_t>(value);

                    if (width < 64) {
                        if (v < 0) {
                            uint64_t abs_v = (v == std::numeric_limits<int64_t>::min())
                                               ? (1ULL << 63)
                                               : static_cast<uint64_t>(-v);

                            if (width == 0 || abs_v > (1ULL << (width - 1))) {
                                throw nb::value_error(
                                    "Value cannot fit in specified number of bits"
                                );
                            }
                        } else {
                            if (static_cast<uint64_t>(v) >= (1ULL << width)) {
                                throw nb::value_error(
                                    "Value cannot fit in specified number of bits"
                                );
                            }
                        }
                    }
                    uint64_t mask = (width >= 64) ? ~0ULL : (1ULL << width) - 1;
                    uint64_t v_2s_comp = static_cast<uint64_t>(v) & mask;
                    std::string s = std::format("{:0{}b}", v_2s_comp, width);

                    new (self) BitVector(s, *range);
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
            [](BitVector& self) {
                return nb::make_iterator(
                    nb::type<BitVector>(), "BitArrayIterator", self.begin(), self.end()
                );
            },
            nb::keep_alive<0, 1>()
        )
        .def(
            "__reversed__",
            [](BitVector& self) {
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
        .def(
            "__format__",
            [](BitVector const& self, std::string_view spec) -> std::string {
                if (spec.empty()) {
                    return to_string(self);
                }
                return format_bit_vector(self, spec);
            }
        )
        .def("__copy__", [](BitVector const& self) { return BitVector(self); })
        .def("__deepcopy__", [](BitVector const& self, nb::dict /* memo */) {
            return BitVector(self);
        });
}

// NOLINTEND(misc-include-cleaner)
