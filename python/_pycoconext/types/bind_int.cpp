// Python bindings for coconext unsigned dynamic type.
#include <coconext/types/concepts.hpp>
#include <coconext/types/direction.hpp>
#include <coconext/types/dyn_signed.hpp>
#include <coconext/types/logic.hpp>
#include <coconext/types/range.hpp>

#include <nanobind/make_iterator.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>       // IWYU pragma: keep
#include <nanobind/stl/string_view.h>  // IWYU pragma: keep

// #include <Python.h>
#include <cstddef>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>
#include <utility>

namespace nb = nanobind;
using namespace nb::literals;

using namespace coconext::types;
using namespace coconext::types::detail;

namespace {

template <typename Integer>
Integer integer_from_python(nb::int_ value, Range range) {
    nb::object const limit = nb::int_(1) << nb::int_(range.length());
    nb::object lower = nb::int_(0);
    nb::object upper = limit;
    if constexpr (std::same_as<Integer, DynSigned>) {
        upper = limit >> nb::int_(1);
        lower = -upper;
    }
    if (range.length() == 0 || value < lower || value >= upper) {
        throw std::overflow_error("Integer value does not fit in provided range");
    }
    return Integer(nb::cast<std::string>(nb::str(value)), range);
}

}  // namespace

auto python_div = [](DynSigned const& a, DynSigned const& b) {
    if (!static_cast<bool>(b)) {
        throw std::domain_error("Division by zero");
    }

    DynSigned q = a / b;
    DynSigned r = a - (q * b);

    if (static_cast<bool>(r)) {
        if ((a < DynSigned(0, a.size())) != (b < DynSigned(0, b.size()))) {
            q -= DynSigned(1, q.size());
        }
    }
    return q;
};

auto python_imod = [](DynSigned& lhs, DynSigned const& rhs) -> DynSigned& {
    auto result = mod(lhs, rhs);
    lhs = DynSigned(DynSInt(storage(result), lhs.size()), lhs.range());
    return lhs;
};

void register_unsigned(nb::module_& m) {
    nb::class_<DynUnsigned>(m, "Unsigned")
        .def(
            "__init__",
            [](DynUnsigned* self, int64_t value, Range range) {
                new (self) DynUnsigned(value, range);
            },
            "value"_a,
            "range"_a
        )
        .def(
            "__init__",
            [](DynUnsigned* self, nb::int_ value, Range range) {
                new (self) DynUnsigned(integer_from_python<DynUnsigned>(value, range));
            },
            "value"_a,
            "range"_a
        )
        .def_prop_ro("range", &DynUnsigned::range)
        .def_prop_ro("left", [](DynUnsigned const& self) { return self.range().left; })
        .def_prop_ro("right", [](DynUnsigned const& self) { return self.range().right; })
        .def_prop_ro(
            "direction",
            [](DynUnsigned const& self) { return to_string(self.range().direction); }
        )
        .def(
            "__init__",
            [](DynUnsigned* self, int64_t v, size_t width) {
                new (self) DynUnsigned(v, width);
            }
        )
        // python int has infinite precision
        .def(
            "__init__",
            [](DynUnsigned* self, nb::int_ value_obj, size_t width) {
                nb::str py_str = nb::str(value_obj);
                std::string dec_str = nb::cast<std::string>(py_str);
                new (self) DynUnsigned(dec_str, width);
            }
        )

        .def(
            "__or__",
            [](DynUnsigned const& self, DynUnsigned const& other) { return self | other; }
        )
        .def(
            "__and__",
            [](DynUnsigned const& self, DynUnsigned const& other) { return self & other; }
        )
        .def(
            "__xor__",
            [](DynUnsigned const& self, DynUnsigned const& other) { return self ^ other; }
        )
        .def("__invert__", [](DynUnsigned const& self) { return ~self; })

        .def(
            "__getitem__",
            [](DynUnsigned const& self, Range::value_type index) {
                return self.index(index);
            }
        )

        .def("__neg__", [](DynUnsigned const& self) { return -self; })
        .def("__pos__", [](DynUnsigned const& self) { return +self; })

        .def(
            "__setitem__",
            [](DynUnsigned& self, Range::value_type index, nb::object const& value) {
                self[index] = nb::cast<Bit>(nb::type<Bit>()(value));
            }
        )

        .def(
            "__format__",
            [](DynUnsigned const& self, std::string spec) {
                auto const& val = storage(self);
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

                return std::format("Unsigned{}{{{}}}", self.range(), str_r);
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
                    return self == DynUnsigned(nb::cast<uint64_t>(other), self.range());
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
            "__int__",
            [](DynUnsigned const& self) {
                std::string dec_str = storage(self).to_hexadecimal_string();
                PyObject* py_long = PyLong_FromString(  // NOLINT(misc-include-cleaner)
                    dec_str.c_str(), nullptr, 16
                );

                if (!py_long) {
                    throw nb::python_error();
                }

                return nb::steal<nb::int_>(py_long);
            }
        )

        .def("__len__", [](DynUnsigned const& self) { return self.size(); })
        .def(
            "__iter__",
            [](DynUnsigned const& self) {
                return nb::make_iterator(
                    nb::type<DynUnsigned>(), "UnsignedIterator", self.begin(), self.end()
                );
            },
            nb::keep_alive<0, 1>()
        )
        .def(
            "__reversed__",
            [](DynUnsigned const& self) {
                return nb::make_iterator(
                    nb::type<DynUnsigned>(),
                    "UnsignedReverseIterator",
                    self.rbegin(),
                    self.rend()
                );
            },
            nb::keep_alive<0, 1>()
        )
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
            [](DynSigned* self, int64_t value, Range range) {
                new (self) DynSigned(value, range);
            },
            "value"_a,
            "range"_a
        )
        .def(
            "__init__",
            [](DynSigned* self, nb::int_ value, Range range) {
                new (self) DynSigned(integer_from_python<DynSigned>(value, range));
            },
            "value"_a,
            "range"_a
        )
        .def_prop_ro("range", &DynSigned::range)
        .def_prop_ro("left", [](DynSigned const& self) { return self.range().left; })
        .def_prop_ro("right", [](DynSigned const& self) { return self.range().right; })
        .def_prop_ro(
            "direction",
            [](DynSigned const& self) { return to_string(self.range().direction); }
        )
        .def(
            "__init__",
            [](DynSigned* self, int64_t v, size_t width) { new (self) DynSigned(v, width); }
        )
        .def(
            "__init__",
            [](DynSigned* self, nb::int_ value_obj, size_t width) {
                nb::str py_str = nb::str(value_obj);
                std::string dec_str = nb::cast<std::string>(py_str);

                DynSigned temp(dec_str, width);

                if (width > 0) {
                    bool str_is_negative = (!dec_str.empty() && dec_str[0] == '-');
                    bool val_is_negative = storage(temp).get_bit(width - 1);
                    if (str_is_negative != val_is_negative) {
                        throw std::invalid_argument(
                            "Signed value does not fit in provided width"
                        );
                    }
                }

                new (self) DynSigned(std::move(temp));
            }
        )

        .def(
            "__or__",
            [](DynSigned const& self, DynSigned const& other) { return self | other; }
        )
        .def(
            "__and__",
            [](DynSigned const& self, DynSigned const& other) { return self & other; }
        )
        .def(
            "__xor__",
            [](DynSigned const& self, DynSigned const& other) { return self ^ other; }
        )
        .def("__invert__", [](DynSigned const& self) { return ~self; })

        .def(
            "__getitem__",
            [](DynSigned const& self, Range::value_type index) { return self.index(index); }
        )

        .def("__neg__", [](DynSigned const& self) { return -self; })
        .def("__pos__", [](DynSigned const& self) { return +self; })

        .def(
            "__setitem__",
            [](DynSigned& self, Range::value_type index, nb::object const& value) {
                self[index] = nb::cast<Bit>(nb::type<Bit>()(value));
            }
        )

        .def(
            "__format__",
            [](DynSigned const& self, std::string spec) {
                auto const& val = storage(self);
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

                return std::format("Signed{}{{{}}}", self.range(), str_r);
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
                    return self == DynSigned(nb::cast<int64_t>(other), self.range());
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

        .def(
            "__int__",
            [](DynSigned const& self) {
                std::string dec_str = storage(self).to_decimal_string(true);
                PyObject* py_long = PyLong_FromString(dec_str.c_str(), nullptr, 10);

                if (!py_long) {
                    throw nb::python_error();
                }

                return nb::steal<nb::int_>(py_long);
            }
        )

        .def("__len__", [](DynSigned const& self) { return self.size(); })
        .def(
            "__iter__",
            [](DynSigned const& self) {
                return nb::make_iterator(
                    nb::type<DynSigned>(), "SignedIterator", self.begin(), self.end()
                );
            },
            nb::keep_alive<0, 1>()
        )
        .def(
            "__reversed__",
            [](DynSigned const& self) {
                return nb::make_iterator(
                    nb::type<DynSigned>(),
                    "SignedReverseIterator",
                    self.rbegin(),
                    self.rend()
                );
            },
            nb::keep_alive<0, 1>()
        )
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
            "__mod__",
            [](DynSigned const& self, DynSigned const& other) { return mod(self, other); }
        )
        .def("__imod__", python_imod)
        .def(
            "__imod__",
            [](DynSigned& self, DynUnsigned const& other) {
                return python_imod(self, +other);
            }
        )
        .def(
            "__imod__",
            [](DynSigned& self, int64_t const& other) {
                return python_imod(self, DynSigned(other, 64));
            }
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
