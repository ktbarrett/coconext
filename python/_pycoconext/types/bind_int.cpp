// Python bindings for coconext unsigned dynamic type.
#include <coconext/types/concepts.hpp>
#include <coconext/types/dyn_signed.hpp>
#include <coconext/types/range.hpp>

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>  // IWYU pragma: keep

#include <cstddef>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>

namespace nb = nanobind;
using namespace nb::literals;

using namespace coconext::types;
using namespace coconext::types::detail;

auto python_div = [](DynSigned const& a, DynSigned const& b) {
    if (!static_cast<bool>(b)) {
        throw std::domain_error("Division by zero");
    }

    DynSigned q = a / b;
    DynSigned r = a - (q * b);

    if (static_cast<bool>(r)) {
        if ((a < DynSigned(DynBits{a.width(), 0}))
            != (b < DynSigned(DynBits{a.width(), 0})))
        {
            q -= DynSigned(DynBits{a.width(), 1});
        }
    }
    return q;
};

void register_unsigned(nb::module_& m) {
    nb::class_<DynUnsigned>(m, "Unsigned")
        .def(
            "__init__",
            [](DynUnsigned* self, size_t width, int64_t v) {
                new (self) DynUnsigned(width, v);
            }
        )

        .def(
            "__getitem__",
            [](DynUnsigned const& self, Range::value_type index) {
                return self.index(index);
            }
        )

        .def("__neg__", [](DynUnsigned const& self) { return -self; })
        .def("__pos__", [](DynUnsigned const& self) { return +self; })

        .def(
            "__format__",
            [](DynUnsigned const& self, std::string spec) {
                auto const& val = bits(self);
                std::string str_r;

                if (spec.empty() || spec.back() == 'd') {
                    str_r = val.to_decimal_string();
                } else if (spec.back() == 'b') {
                    str_r = val.to_binary_string();
                } else if (spec.back() == 'x' || spec.back() == 'X') {
                    str_r = val.to_hexadecimal_string();
                } else if (spec.back() == 'o') {
                    str_r = val.to_octal_string();
                } else {
                    throw std::invalid_argument("Invalid format specifier for Unsigned");
                }

                size_t width = self.width();
                size_t left_index = width > 0 ? width - 1 : 0;

                return std::format("Unsigned[{} downto 0]{{{}}}", left_index, str_r);
            },
            "format_spec"_a = ""
        )

        .def(
            "__eq__",
            [](DynUnsigned const& self, DynUnsigned const& other) { return self == other; }
        )
        .def(
            "__eq__",
            [](DynUnsigned const& self, nb::int_ other) {
                try {
                    return self == DynUnsigned(self.width(), nb::cast<uint64_t>(other));
                } catch (...) {
                    return false;
                }
            },
            nb::is_operator()
        )

        .def(
            "__lt__",
            [](DynUnsigned const& self, DynUnsigned const& other) { return self < other; }
        )
        .def(
            "__gt__",
            [](DynUnsigned const& self, DynUnsigned const& other) { return self > other; }
        )
        .def(
            "__le__",
            [](DynUnsigned const& self, DynUnsigned const& other) { return self <= other; }
        )
        .def(
            "__ge__",
            [](DynUnsigned const& self, DynUnsigned const& other) { return self >= other; }
        )

        .def(
            "__lshift__",
            [](DynUnsigned const& self, size_t const& shift_amount) {
                return self << shift_amount;
            }
        )
        .def(
            "__lshift__",
            [](DynUnsigned const& self, DynUnsigned const& shift_amount) {
                return self << shift_amount;
            }
        )
        .def(
            "__lshift__",
            [](DynUnsigned const& self, DynSigned const& shift_amount) {
                return self << shift_amount;
            }
        )

        .def(
            "__ilshift__",
            [](DynUnsigned& self, size_t const& shift_amount) {
                return self <<= shift_amount;
            }
        )
        .def(
            "__ilshift__",
            [](DynUnsigned& self, DynUnsigned const& shift_amount) {
                return self <<= shift_amount;
            }
        )
        .def(
            "__ilshift__",
            [](DynUnsigned& self, DynSigned const& shift_amount) {
                return self <<= shift_amount;
            }
        )

        .def(
            "__rshift__",
            [](DynUnsigned const& self, size_t shift_amount) {
                return self >> shift_amount;
            }
        )
        .def(
            "__rshift__",
            [](DynUnsigned const& self, DynUnsigned shift_amount) {
                return self >> shift_amount;
            }
        )
        .def(
            "__rshift__",
            [](DynUnsigned const& self, DynSigned shift_amount) {
                return self >> shift_amount;
            }
        )

        .def(
            "__irshift__",
            [](DynUnsigned& self, size_t shift_amount) { return self >>= shift_amount; }
        )
        .def(
            "__irshift__",
            [](DynUnsigned& self, DynUnsigned shift_amount) {
                return self >>= shift_amount;
            }
        )
        .def(
            "__irshift__",
            [](DynUnsigned& self, DynSigned shift_amount) { return self >>= shift_amount; }
        )

        .def(
            "__int__", [](DynUnsigned const& self) { return static_cast<long long>(self); }
        )
        .def("__len__", [](DynUnsigned const& self) { return self.width(); })
        .def("__bool__", [](DynUnsigned const& self) { return static_cast<bool>(self); })

        .def(
            "__add__",
            [](DynUnsigned const& self, DynUnsigned const& other) { return self + other; }
        )
        .def(
            "__iadd__",
            [](DynUnsigned& self, DynUnsigned const& other) { return self += other; }
        )
        .def(
            "__iadd__",
            [](DynUnsigned& self, int64_t const& other) { return self += other; }
        )
        .def(
            "__iadd__",
            [](DynUnsigned& self, DynSigned const& other) { return self += other; }
        )
        .def(
            "__mul__",
            [](DynUnsigned const& self, DynUnsigned const& other) { return self * other; }
        )
        .def(
            "__imul__",
            [](DynUnsigned& self, DynUnsigned const& other) { return self *= other; }
        )
        .def(
            "__imul__",
            [](DynUnsigned& self, int64_t const& other) { return self *= other; }
        )
        .def(
            "__imul__",
            [](DynUnsigned& self, DynSigned const& other) { return self *= other; }
        )
        .def(
            "__truediv__",
            [](DynUnsigned const& self, DynUnsigned const& other) { return self / other; }
        )
        .def(
            "__itruediv__",
            [](DynUnsigned& self, DynUnsigned const& other) { return self /= other; }
        )
        .def(
            "__itruediv__",
            [](DynUnsigned& self, int64_t const& other) { return self /= other; }
        )
        .def(
            "__itruediv__",
            [](DynUnsigned& self, DynSigned const& other) { return self /= other; }
        )
        .def(
            "__floordiv__",
            [](DynUnsigned const& self, DynUnsigned const& other) { return self / other; }
        )
        .def(
            "__ifloordiv__",
            [](DynUnsigned& self, DynUnsigned const& other) { return self /= other; }
        )
        .def(
            "__ifloordiv__",
            [](DynUnsigned& self, int64_t const& other) { return self /= other; }
        )
        .def(
            "__ifloordiv__",
            [](DynUnsigned& self, DynSigned const& other) { return self /= other; }
        )
        .def(
            "__mod__",
            [](DynUnsigned const& self, DynUnsigned const& other) { return self % other; }
        )
        .def(
            "__imod__",
            [](DynUnsigned& self, DynUnsigned const& other) { return self %= other; }
        )
        .def(
            "__imod__",
            [](DynUnsigned& self, int64_t const& other) { return self %= other; }
        )
        .def(
            "__imod__",
            [](DynUnsigned& self, DynSigned const& other) { return self %= other; }
        )
        .def(
            "__sub__",
            [](DynUnsigned const& self, DynUnsigned const& other) { return self - other; }
        )
        .def(
            "__isub__",
            [](DynUnsigned& self, DynUnsigned const& other) { return self -= other; }
        )
        .def(
            "__isub__",
            [](DynUnsigned& self, int64_t const& other) { return self -= other; }
        )
        .def("__isub__", [](DynUnsigned& self, DynSigned const& other) {
            return self -= other;
        });
}

void register_signed(nb::module_& m) {
    nb::class_<DynSigned>(m, "Signed")
        .def(
            "__init__",
            [](DynSigned* self, size_t width, int64_t v) { new (self) DynSigned(width, v); }
        )

        .def(
            "__getitem__",
            [](DynSigned const& self, Range::value_type index) { return self.index(index); }
        )

        .def("__neg__", [](DynSigned const& self) { return -self; })
        .def("__pos__", [](DynSigned const& self) { return +self; })

        .def(
            "__format__",
            [](DynSigned const& self, std::string spec) {
                auto const& val = bits(self);
                std::string str_r;

                if (spec.empty() || spec.back() == 'd') {
                    str_r = val.to_decimal_string(true);
                } else if (spec.back() == 'b') {
                    str_r = val.to_binary_string();
                } else if (spec.back() == 'x' || spec.back() == 'X') {
                    str_r = val.to_hexadecimal_string();
                } else if (spec.back() == 'o') {
                    str_r = val.to_octal_string();
                } else {
                    throw std::invalid_argument("Invalid format specifier for Unsigned");
                }

                size_t width = self.width();
                size_t left_index = width > 0 ? width - 1 : 0;

                return std::format("Signed[{} downto 0]{{{}}}", left_index, str_r);
            },
            "format_spec"_a = ""
        )

        .def(
            "__eq__",
            [](DynSigned const& self, DynSigned const& other) { return self == other; }
        )
        .def(
            "__eq__",
            [](DynSigned const& self, nb::int_ other) {
                try {
                    return self == DynSigned(self.width(), nb::cast<int64_t>(other));
                } catch (...) {
                    return false;
                }
            },
            nb::is_operator()
        )

        .def(
            "__lt__",
            [](DynSigned const& self, DynSigned const& other) { return self < other; }
        )
        .def(
            "__gt__",
            [](DynSigned const& self, DynSigned const& other) { return self > other; }
        )
        .def(
            "__le__",
            [](DynSigned const& self, DynSigned const& other) { return self <= other; }
        )
        .def(
            "__ge__",
            [](DynSigned const& self, DynSigned const& other) { return self >= other; }
        )

        .def(
            "__lshift__",
            [](DynSigned const& self, size_t const& shift_amount) {
                return self << shift_amount;
            }
        )
        .def(
            "__lshift__",
            [](DynSigned const& self, DynUnsigned const& shift_amount) {
                return self << shift_amount;
            }
        )
        .def(
            "__lshift__",
            [](DynSigned const& self, DynSigned const& shift_amount) {
                return self << shift_amount;
            }
        )

        .def(
            "__ilshift__",
            [](DynSigned& self, size_t const& shift_amount) {
                return self <<= shift_amount;
            }
        )
        .def(
            "__ilshift__",
            [](DynSigned& self, DynUnsigned const& shift_amount) {
                return self <<= shift_amount;
            }
        )
        .def(
            "__ilshift__",
            [](DynSigned& self, DynSigned const& shift_amount) {
                return self <<= shift_amount;
            }
        )

        .def(
            "__rshift__",
            [](DynSigned const& self, size_t shift_amount) { return self >> shift_amount; }
        )
        .def(
            "__rshift__",
            [](DynSigned const& self, DynUnsigned shift_amount) {
                return self >> shift_amount;
            }
        )
        .def(
            "__rshift__",
            [](DynSigned const& self, DynSigned shift_amount) {
                return self >> shift_amount;
            }
        )

        .def(
            "__irshift__",
            [](DynSigned& self, size_t shift_amount) { return self >>= shift_amount; }
        )
        .def(
            "__irshift__",
            [](DynSigned& self, DynUnsigned shift_amount) { return self >>= shift_amount; }
        )
        .def(
            "__irshift__",
            [](DynSigned& self, DynSigned shift_amount) { return self >>= shift_amount; }
        )

        .def("__int__", [](DynSigned const& self) { return static_cast<long long>(self); })
        .def("__len__", [](DynSigned const& self) { return self.width(); })
        .def("__bool__", [](DynSigned const& self) { return static_cast<bool>(self); })

        .def(
            "__add__",
            [](DynSigned const& self, DynSigned const& other) { return self + other; }
        )
        .def(
            "__iadd__",
            [](DynSigned& self, DynUnsigned const& other) { return self += other; }
        )
        .def(
            "__iadd__", [](DynSigned& self, int64_t const& other) { return self += other; }
        )
        .def(
            "__iadd__",
            [](DynSigned& self, DynSigned const& other) { return self += other; }
        )
        .def(
            "__mul__",
            [](DynSigned const& self, DynSigned const& other) { return self * other; }
        )
        .def(
            "__imul__",
            [](DynSigned& self, DynUnsigned const& other) { return self *= other; }
        )
        .def(
            "__imul__", [](DynSigned& self, int64_t const& other) { return self *= other; }
        )
        .def(
            "__imul__",
            [](DynSigned& self, DynSigned const& other) { return self *= other; }
        )
        .def("__truediv__", python_div, nb::is_operator())
        .def(
            "__itruediv__",
            [](DynSigned& self, DynUnsigned const& other) { return self /= other; }
        )
        .def(
            "__itruediv__",
            [](DynSigned& self, int64_t const& other) { return self /= other; }
        )
        .def(
            "__itruediv__",
            [](DynSigned& self, DynSigned const& other) { return self /= other; }
        )
        .def("__floordiv__", python_div, nb::is_operator())
        .def(
            "__ifloordiv__",
            [](DynSigned& self, DynUnsigned const& other) { return self /= other; }
        )
        .def(
            "__ifloordiv__",
            [](DynSigned& self, int64_t const& other) { return self /= other; }
        )
        .def(
            "__ifloordiv__",
            [](DynSigned& self, DynSigned const& other) { return self /= other; }
        )
        .def(
            "__mod__", [](DynSigned& self, DynSigned const& other) { return self % other; }
        )
        .def(
            "__imod__",
            [](DynSigned& self, DynSigned const& other) { return self %= other; }
        )
        .def(
            "__imod__",
            [](DynSigned& self, DynUnsigned const& other) { return self %= other; }
        )
        .def(
            "__imod__", [](DynSigned& self, int64_t const& other) { return self %= other; }
        )
        .def(
            "__sub__",
            [](DynSigned const& self, DynSigned const& other) { return self - other; }
        )
        .def(
            "__isub__",
            [](DynSigned& self, DynUnsigned const& other) { return self -= other; }
        )
        .def(
            "__isub__", [](DynSigned& self, int64_t const& other) { return self -= other; }
        )
        .def("__isub__", [](DynSigned& self, DynSigned const& other) {
            return self -= other;
        });
}
