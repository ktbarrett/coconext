// Python bindings for coconext unsigned dynamic type.
#include <cstddef>
#include <cstdint>
#include <nanobind/nanobind.h>

#include <coconext/types/dyn_signed.hpp>

namespace nb = nanobind;
using namespace nb::literals;

using namespace coconext::types::detail;

void register_unsigned(nb::module_& m) {
    nb::class_<DynUnsigned>(m, "Unsigned")
        .def(
            "__init__",
            [](DynUnsigned* self, size_t width, int64_t v) {
                new (self) DynUnsigned(width, v);
            }
        )

        .def(
            "__eq__",
            [](DynUnsigned const& self, DynUnsigned const& other) { return self == other; }
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
            "__int__", [](DynUnsigned const& self) { return static_cast<long long>(self); }
        )
        .def("__len__", [](DynUnsigned const& self) { return self.get_width(); })
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
            "__mul__",
            [](DynUnsigned const& self, DynUnsigned const& other) { return self * other; }
        )
        .def(
            "__truediv__",
            [](DynUnsigned const& self, DynUnsigned const& other) { return self / other; }
        )
        .def(
            "__floordiv__",
            [](DynUnsigned const& self, DynUnsigned const& other) { return self / other; }
        )
        .def(
            "__mod__",
            [](DynUnsigned const& self, DynUnsigned const& other) { return self % other; }
        )
        .def("__sub__", [](DynUnsigned const& self, DynUnsigned const& other) {
            return self - other;
        });
}

void register_signed(nb::module_& m) {
    nb::class_<DynSigned>(m, "Signed")
        .def(
            "__init__",
            [](DynSigned* self, size_t width, int64_t v) { new (self) DynSigned(width, v); }
        )
        .def("__int__", [](DynSigned const& self) { return static_cast<long long>(self); })
        .def("__len__", [](DynSigned const& self) { return self.get_width(); })
        // .def("__bool__", [](const DynSigned& self) {
        //     return static_cast<bool>(self);
        // })
        // .def("__add__", [](const DynSigned& self, const DynSigned& other) {
        //     return self + other;
        // })
        // .def("__mul__", [](const DynSigned& self, const DynSigned& other) {
        //     return self * other;
        // })
        // .def("__truediv__", [](const DynSigned& self, const DynSigned& other) {
        //     return self / other;
        // })
        // .def("__floordiv__", [](const DynSigned& self, const DynSigned& other) {
        //     return self / other;
        // })
        // .def("__mod__", [](const DynSigned& self, const DynSigned& other) {
        //     return self % other;
        // })
        ;
}
