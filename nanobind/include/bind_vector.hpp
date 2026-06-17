#ifndef NB_BIND_VECTOR_HPP
#define NB_BIND_VECTOR_HPP

#include <coconext/types/vector.hpp>

#include <nanobind/make_iterator.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include <stdexcept>
#include <vector>

namespace nb = nanobind;

namespace coconext_nb {

template <typename VectorType>
void bind_array(nb::module_& m, char const* name) {
    using ValueT = typename VectorType::value_type;

    nb::class_<VectorType> cl(m, name);
    cl.def(
        "__init__",
        [](VectorType* a, std::vector<ValueT> const& vec) { new (a) VectorType(vec); },
        nb::arg("value")
    );

    cl.def(
        "__init__",
        [](VectorType* a, std::vector<ValueT> const& vec, coconext::types::Range range) {
            new (a) VectorType(vec, range);
        },
        nb::arg("value"),
        nb::arg("range")
    );

    cl.def_prop_ro("range", [](VectorType const& a) { return a.range(); })
        .def_prop_ro("left", [](VectorType const& a) { return a.range().left; })
        .def_prop_ro("right", [](VectorType const& a) { return a.range().right; })
        .def_prop_ro("direction", [](VectorType const& a) { return a.range().direction; });

    cl.def("__len__", [](VectorType const& a) { return a.size(); })

        .def(
            "__iter__",
            [](VectorType& a) {
                return nb::make_iterator<nb::rv_policy::reference_internal>(
                    nb::type<VectorType>(), "iterator", a.begin(), a.end()
                );
            },
            nb::keep_alive<0, 1>()
        )

        .def("__eq__", [](VectorType const& a, VectorType const& b) { return a == b; })
        .def("__ne__", [](VectorType const& a, VectorType const& b) { return !(a == b); })

        .def(
            "__contains__",
            [](VectorType const& a, ValueT const& val) { return a.index(val).has_value(); }
        )

        .def("index", [](VectorType const& a, ValueT const& val) {
            auto idx = a.index(val);
            if (!idx.has_value()) {
                throw nb::value_error("Value not found in array");
            }
            return idx.value();
        });

    cl.def("__getitem__", [](VectorType& a, int idx) -> ValueT {
        try {
            return a[idx];
        } catch (std::out_of_range const&) {
            throw nb::index_error("Array index out of range");
        }
    });

    cl.def("__setitem__", [](VectorType& a, int idx, ValueT const& val) {
        try {
            a[idx] = val;
        } catch (std::out_of_range const&) {
            throw nb::index_error("Array index out of range assignment");
        }
    });
}

}  // namespace coconext_nb

#endif  // NB_BIND_VECTOR_HPP
