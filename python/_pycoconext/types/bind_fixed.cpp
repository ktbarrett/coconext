// Python bindings for coconext dynamic fixed-point types.
#include <coconext/types/dyn_fixed.hpp>

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>       // IWYU pragma: keep
#include <nanobind/stl/string_view.h>  // IWYU pragma: keep

#include <cmath>
#include <cstddef>
#include <format>
#include <stdexcept>
#include <string>

namespace nb = nanobind;
using namespace nb::literals;

using namespace coconext::types;
using namespace coconext::types::detail;

namespace {

template <bool SignedRepresentation>
BitVector bit_array_from_integer(nb::object const& value, Range range) {
    dyn_fixed_detail::validate_range(range);

    nb::int_ zero(0);
    nb::int_ one(1);
    nb::object modulus = one << nb::int_(range.length());
    if constexpr (SignedRepresentation) {
        nb::object limit = one << nb::int_(range.length() - 1);
        if (value < -limit || !(value < limit)) {
            throw std::overflow_error("Sfixed raw value does not fit its range");
        }
    } else if (value < zero || !(value < modulus)) {
        throw std::overflow_error("Ufixed raw value does not fit its range");
    }

    nb::object encoded = value < zero ? value + modulus : value;
    std::string format_spec = std::format("0{}b", range.length());
    nb::object binary =
        nb::module_::import_("builtins").attr("format")(encoded, format_spec.c_str());
    return BitVector(nb::cast<std::string>(binary), range);
}

nb::object scaled_integer(nb::int_ value, Range::value_type right) {
    if (right <= 0) {
        return value << nb::int_(static_cast<size_t>(-right));
    }

    nb::object divisor = nb::int_(1) << nb::int_(static_cast<size_t>(right));
    nb::tuple quotient_remainder = nb::cast<nb::tuple>(
        nb::module_::import_("builtins").attr("divmod")(value, divisor)
    );
    nb::object quotient = quotient_remainder[0];
    nb::object remainder = quotient_remainder[1];
    if (!remainder.equal(nb::int_(0))) {
        throw std::overflow_error("Integer is not aligned to the fixed-point resolution");
    }
    return quotient;
}

nb::object rounded_scaled_float(nb::float_ value, Range::value_type right) {
    nb::tuple ratio = nb::cast<nb::tuple>(value.attr("as_integer_ratio")());
    nb::object numerator = ratio[0];
    nb::object denominator = ratio[1];

    if (right < 0) {
        numerator = numerator << nb::int_(static_cast<size_t>(-right));
    } else if (right > 0) {
        denominator = denominator << nb::int_(static_cast<size_t>(right));
    }

    nb::int_ zero(0);
    bool negative = numerator < zero;
    nb::object magnitude = negative ? -numerator : numerator;
    nb::tuple quotient_remainder = nb::cast<nb::tuple>(
        nb::module_::import_("builtins").attr("divmod")(magnitude, denominator)
    );
    nb::object quotient = quotient_remainder[0];
    nb::object remainder = quotient_remainder[1];

    nb::object doubled_remainder = remainder << nb::int_(1);
    bool greater_than_half = denominator < doubled_remainder;
    bool exactly_half = doubled_remainder.equal(denominator);

    nb::int_ one(1);
    nb::object low_bit = quotient & one;
    if (greater_than_half || (exactly_half && !low_bit.equal(zero))) {
        quotient = quotient + one;
    }
    return negative ? -quotient : quotient;
}

nb::object signed_limit(size_t width, bool minimum) {
    nb::object magnitude = nb::int_(1) << nb::int_(width - 1);
    if (minimum) {
        return -magnitude;
    }
    return magnitude - nb::int_(1);
}

DynSfixed make_sfixed(Range range, nb::int_ value) {
    auto raw = scaled_integer(value, range.right);
    return as<DynSfixed>(bit_array_from_integer<true>(raw, range));
}

DynSfixed make_sfixed(Range range, nb::float_ value) {
    double native = nb::cast<double>(value);
    if (std::isnan(native)) {
        throw std::domain_error("Cannot convert NaN to Sfixed");
    }
    if (std::isinf(native)) {
        auto raw = signed_limit(range.length(), native < 0);
        return as<DynSfixed>(bit_array_from_integer<true>(raw, range));
    }

    auto raw = rounded_scaled_float(value, range.right);
    try {
        return as<DynSfixed>(bit_array_from_integer<true>(raw, range));
    } catch (std::overflow_error const&) {
        auto saturated = signed_limit(range.length(), native < 0);
        return as<DynSfixed>(bit_array_from_integer<true>(saturated, range));
    }
}

DynUfixed make_ufixed(Range range, nb::int_ value) {
    nb::int_ zero(0);
    if (value < zero) {
        throw std::overflow_error("Cannot construct Ufixed from a negative integer");
    }
    auto raw = scaled_integer(value, range.right);
    return as<DynUfixed>(bit_array_from_integer<false>(raw, range));
}

DynUfixed make_ufixed(Range range, nb::float_ value) {
    double native = nb::cast<double>(value);
    if (std::isnan(native)) {
        throw std::domain_error("Cannot convert NaN to Ufixed");
    }
    if (native < 0) {
        throw std::overflow_error("Cannot construct Ufixed from a negative float");
    }
    if (std::isinf(native)) {
        nb::object raw = (nb::int_(1) << nb::int_(range.length())) - nb::int_(1);
        return as<DynUfixed>(bit_array_from_integer<false>(raw, range));
    }

    auto raw = rounded_scaled_float(value, range.right);
    try {
        return as<DynUfixed>(bit_array_from_integer<false>(raw, range));
    } catch (std::overflow_error const&) {
        nb::object saturated = (nb::int_(1) << nb::int_(range.length())) - nb::int_(1);
        return as<DynUfixed>(bit_array_from_integer<false>(saturated, range));
    }
}

template <typename Fixed>
nb::int_ fixed_to_python_int(Fixed const& value) {
    nb::object raw = nb::int_(nb::str(value.raw_decimal().c_str()));
    auto right = value.range().right;
    if (right >= 0) {
        nb::object result = raw << nb::int_(static_cast<size_t>(right));
        return nb::borrow<nb::int_>(result);
    }

    nb::object divisor = nb::int_(1) << nb::int_(static_cast<size_t>(-right));
    bool negative = raw < nb::int_(0);
    nb::object magnitude = negative ? -raw : raw;
    nb::tuple quotient_remainder = nb::cast<nb::tuple>(
        nb::module_::import_("builtins").attr("divmod")(magnitude, divisor)
    );
    nb::object quotient = quotient_remainder[0];
    if (negative) {
        quotient = -quotient;
    }
    return nb::borrow<nb::int_>(quotient);
}

template <typename Fixed>
std::string fixed_repr(char const* name, Fixed const& value) {
    auto range = value.range();
    return std::format(
        "{}(raw={}, range=Range({}, 'downto', {}))",
        name,
        value.raw_decimal(),
        range.left,
        range.right
    );
}

}  // namespace

void register_sfixed(nb::module_& m) {
    nb::class_<DynSfixed> cls(m, "Sfixed");
    cls.def(
           "__init__",
           [](DynSfixed* self, nb::int_ value, Range range) {
               new (self) DynSfixed(make_sfixed(range, value));
           },
           "value"_a,
           "range"_a
    )
        .def(
            "__init__",
            [](DynSfixed* self, nb::float_ value, Range range) {
                new (self) DynSfixed(make_sfixed(range, value));
            },
            "value"_a,
            "range"_a
        )
        .def(
            "__init__",
            [](DynSfixed* self, Range range, nb::int_ value) {
                new (self) DynSfixed(make_sfixed(range, value));
            },
            "range"_a,
            "value"_a
        )
        .def(
            "__init__",
            [](DynSfixed* self, Range range, nb::float_ value) {
                new (self) DynSfixed(make_sfixed(range, value));
            },
            "range"_a,
            "value"_a
        )
        .def("__pos__", [](DynSfixed const& self) { return +self; })
        .def("__neg__", [](DynSfixed const& self) { return -self; })
        .def("__abs__", &DynSfixed::abs)
        .def(
            "__sub__",
            [](DynSfixed const& self, DynSfixed const& other) { return self - other; }
        )
        .def(
            "__isub__",
            [](DynSfixed& self, DynSfixed const& other) -> DynSfixed& {
                return self -= other;
            }
        )
        .def_prop_ro("range", [](DynSfixed const& self) { return self.range(); })
        .def_prop_ro("left", [](DynSfixed const& self) { return self.range().left; })
        .def_prop_ro("right", [](DynSfixed const& self) { return self.range().right; })
        .def_prop_ro(
            "direction",
            [](DynSfixed const& self) { return to_string(self.range().direction); }
        )
        .def("__len__", &DynSfixed::size)
        .def("__bool__", [](DynSfixed const& self) { return static_cast<bool>(self); })
        .def("__int__", [](DynSfixed const& self) { return fixed_to_python_int(self); })
        .def("__float__", [](DynSfixed const& self) { return static_cast<double>(self); })
        .def("__getitem__", &DynSfixed::index)
        .def("__setitem__", &DynSfixed::set_index)
        .def("__repr__", [](DynSfixed const& self) { return fixed_repr("Sfixed", self); })
        .def(
            "__eq__",
            [](DynSfixed const& self, DynSfixed const& other) { return self == other; }
        )
        .def(
            "__lt__",
            [](DynSfixed const& self, DynSfixed const& other) { return self < other; }
        )
        .def(
            "__le__",
            [](DynSfixed const& self, DynSfixed const& other) { return self <= other; }
        )
        .def(
            "__gt__",
            [](DynSfixed const& self, DynSfixed const& other) { return self > other; }
        )
        .def(
            "__ge__",
            [](DynSfixed const& self, DynSfixed const& other) { return self >= other; }
        )
        .def(
            "__lshift__",
            [](DynSfixed const& self, size_t amount) { return self << amount; }
        )
        .def(
            "__rshift__",
            [](DynSfixed const& self, size_t amount) { return self >> amount; }
        )
        .def(
            "__ilshift__",
            [](DynSfixed& self, size_t amount) -> DynSfixed& { return self <<= amount; }
        )
        .def(
            "__irshift__",
            [](DynSfixed& self, size_t amount) -> DynSfixed& { return self >>= amount; }
        )
        .def(
            "__add__",
            [](DynSfixed const& self, DynSfixed const& other) { return self + other; }
        )
        .def(
            "__iadd__",
            [](DynSfixed& self, DynSfixed const& other) -> DynSfixed& {
                return self += other;
            }
        )
        .def(
            "__mul__",
            [](DynSfixed const& self, DynSfixed const& other) { return self * other; }
        )
        .def(
            "__imul__",
            [](DynSfixed& self, DynSfixed const& other) -> DynSfixed& {
                return self *= other;
            }
        )
        .def(
            "__truediv__",
            [](DynSfixed const& self, DynSfixed const& other) { return self / other; }
        )
        .def(
            "__itruediv__",
            [](DynSfixed& self, DynSfixed const& other) -> DynSfixed& {
                return self /= other;
            }
        )
        .def(
            "__mod__",
            [](DynSfixed const& self, DynSfixed const& other) { return self % other; }
        )
        .def("__imod__", [](DynSfixed& self, DynSfixed const& other) -> DynSfixed& {
            return self %= other;
        });
}

void register_ufixed(nb::module_& m) {
    nb::class_<DynUfixed> cls(m, "Ufixed");
    cls.def(
           "__init__",
           [](DynUfixed* self, nb::int_ value, Range range) {
               new (self) DynUfixed(make_ufixed(range, value));
           },
           "value"_a,
           "range"_a
    )
        .def(
            "__init__",
            [](DynUfixed* self, nb::float_ value, Range range) {
                new (self) DynUfixed(make_ufixed(range, value));
            },
            "value"_a,
            "range"_a
        )
        .def(
            "__init__",
            [](DynUfixed* self, Range range, nb::int_ value) {
                new (self) DynUfixed(make_ufixed(range, value));
            },
            "range"_a,
            "value"_a
        )
        .def(
            "__init__",
            [](DynUfixed* self, Range range, nb::float_ value) {
                new (self) DynUfixed(make_ufixed(range, value));
            },
            "range"_a,
            "value"_a
        )
        .def("__pos__", [](DynUfixed const& self) { return +self; })
        .def("__neg__", [](DynUfixed const& self) { return -self; })
        .def(
            "__sub__",
            [](DynUfixed const& self, DynUfixed const& other) { return self - other; }
        )
        .def(
            "__isub__",
            [](DynUfixed& self, DynUfixed const& other) -> DynUfixed& {
                return self -= other;
            }
        )
        .def_prop_ro("range", [](DynUfixed const& self) { return self.range(); })
        .def_prop_ro("left", [](DynUfixed const& self) { return self.range().left; })
        .def_prop_ro("right", [](DynUfixed const& self) { return self.range().right; })
        .def_prop_ro(
            "direction",
            [](DynUfixed const& self) { return to_string(self.range().direction); }
        )
        .def("__len__", &DynUfixed::size)
        .def("__bool__", [](DynUfixed const& self) { return static_cast<bool>(self); })
        .def("__int__", [](DynUfixed const& self) { return fixed_to_python_int(self); })
        .def("__float__", [](DynUfixed const& self) { return static_cast<double>(self); })
        .def("__getitem__", &DynUfixed::index)
        .def("__setitem__", &DynUfixed::set_index)
        .def("__repr__", [](DynUfixed const& self) { return fixed_repr("Ufixed", self); })
        .def(
            "__eq__",
            [](DynUfixed const& self, DynUfixed const& other) { return self == other; }
        )
        .def(
            "__lt__",
            [](DynUfixed const& self, DynUfixed const& other) { return self < other; }
        )
        .def(
            "__le__",
            [](DynUfixed const& self, DynUfixed const& other) { return self <= other; }
        )
        .def(
            "__gt__",
            [](DynUfixed const& self, DynUfixed const& other) { return self > other; }
        )
        .def(
            "__ge__",
            [](DynUfixed const& self, DynUfixed const& other) { return self >= other; }
        )
        .def(
            "__lshift__",
            [](DynUfixed const& self, size_t amount) { return self << amount; }
        )
        .def(
            "__rshift__",
            [](DynUfixed const& self, size_t amount) { return self >> amount; }
        )
        .def(
            "__ilshift__",
            [](DynUfixed& self, size_t amount) -> DynUfixed& { return self <<= amount; }
        )
        .def(
            "__irshift__",
            [](DynUfixed& self, size_t amount) -> DynUfixed& { return self >>= amount; }
        )
        .def(
            "__add__",
            [](DynUfixed const& self, DynUfixed const& other) { return self + other; }
        )
        .def(
            "__iadd__",
            [](DynUfixed& self, DynUfixed const& other) -> DynUfixed& {
                return self += other;
            }
        )
        .def(
            "__mul__",
            [](DynUfixed const& self, DynUfixed const& other) { return self * other; }
        )
        .def(
            "__imul__",
            [](DynUfixed& self, DynUfixed const& other) -> DynUfixed& {
                return self *= other;
            }
        )
        .def(
            "__truediv__",
            [](DynUfixed const& self, DynUfixed const& other) { return self / other; }
        )
        .def(
            "__itruediv__",
            [](DynUfixed& self, DynUfixed const& other) -> DynUfixed& {
                return self /= other;
            }
        )
        .def(
            "__mod__",
            [](DynUfixed const& self, DynUfixed const& other) { return self % other; }
        )
        .def("__imod__", [](DynUfixed& self, DynUfixed const& other) -> DynUfixed& {
            return self %= other;
        });
}
