#include <nanobind/make_iterator.h>  // Required for making iterators
#include <nanobind/nanobind.h>
#include <nanobind/operators.h>        // Required for operator overloading
#include <nanobind/stl/optional.h>     // Required for std::optional conversions
#include <nanobind/stl/string.h>       // Required for std::string conversions
#include <nanobind/stl/string_view.h>  // Required for std::string_view conversions

#include <coconext/types/array.hpp>
#include <coconext/types/range.hpp>
#include <coconext_nb/types.hpp>  // Include the Array<nb::object> specialization
#include <stdexcept>

namespace nb = nanobind;
using namespace nb::literals;

using namespace coconext::types;

using index_type = typename Array<nb::object>::index_type;

void register_array(nb::module_& m) {
    nb::class_<Array<nb::object>>(m, "Array")
        .def(
            "__init__",
            [](Array<nb::object>& self, nb::iterable value) {
                new (&self) Array<nb::object>(value);
            },
            "value"_a.noconvert())
        .def(
            "__init__",
            [](Array<nb::object>& self, nb::iterable value, Range range) {
                new (&self) Array<nb::object>(value, range);
            },
            "value"_a.noconvert(), "range"_a.noconvert())
        .def(
            "__init__",
            [](Array<nb::object>& self, nb::iterable value, size_t range) {
                new (&self) Array<nb::object>(value, range);
            },
            "value"_a.noconvert(), "range"_a.noconvert())
        .def_prop_rw("range", &Array<nb::object>::range,
                     &Array<nb::object>::set_range)
        .def_prop_ro(
            "left",
            [](const Array<nb::object>& self) { return self.range().left(); })
        .def_prop_ro("direction",
                     [](const Array<nb::object>& self) {
                         return to_string(self.range().direction());
                     })
        .def_prop_ro(
            "right",
            [](const Array<nb::object>& self) { return self.range().right(); })
        .def(
            "__len__",
            [](const Array<nb::object>& self) { return self.range().length(); })
        .def(
            "__eq__",
            [](const Array<nb::object>& self, const Array<nb::object>& other) {
                return self == other;
            },
            ""_a.noconvert(), nb::is_operator())
        .def(
            "__eq__",
            [](const Array<nb::object>& self, const nb::sequence& other) {
                Array<nb::object> other_array(other);
                return self == other_array;
            },
            ""_a.noconvert(), nb::is_operator())
        .def(
            "__iter__",
            [](const Array<nb::object>& self) {
                return nb::make_iterator(nb::type<Array<nb::object>>(),
                                         "iterator", self.begin(), self.end());
            },
            nb::keep_alive<0, 1>())
        .def(
            "__reversed__",
            [](const Array<nb::object>& self) {
                return nb::make_iterator(nb::type<Array<nb::object>>(),
                                         "reverse_iterator", self.rbegin(),
                                         self.rend());
            },
            nb::keep_alive<0, 1>())
        .def(
            "__contains__",
            [](const Array<nb::object>& self, const nb::object& item) {
                for (const auto& elem : self) {
                    if (elem.equal(item)) {
                        return true;
                    }
                }
                return false;
            },
            ""_a.noconvert())
        .def(
            "__getitem__",
            [](const Array<nb::object>& self, index_type index) {
                return self[index];
            },
            ""_a.noconvert())
        .def("__getitem__",
             [](const Array<nb::object>& self, nb::slice slice) {
                 auto maybe_start = nb::object(slice.attr("start"));
                 auto maybe_stop = nb::object(slice.attr("stop"));
                 auto step = nb::object(slice.attr("step"));
                 if (!step.is_none()) {
                     throw nb::type_error("Slicing with step is not supported");
                 }
                 auto start = maybe_start.is_none()
                                  ? self.range().left()
                                  : nb::cast<index_type>(maybe_start);
                 auto stop = maybe_stop.is_none()
                                 ? self.range().right()
                                 : nb::cast<index_type>(maybe_stop);
                 auto sliced_obj = self(start, stop);
                 return Array<nb::object>(sliced_obj, sliced_obj.range());
             })
        .def(
            "__setitem__",
            [](Array<nb::object>& self, index_type index,
               const nb::object& value) { self[index] = value; },
            ""_a.noconvert(), ""_a.noconvert())
        .def("__setitem__",
             [](Array<nb::object>& self, nb::slice slice, nb::iterable values) {
                 auto maybe_start = slice.attr("start");
                 auto maybe_stop = slice.attr("stop");
                 auto step = slice.attr("step");
                 if (!step.is_none()) {
                     throw nb::type_error("Slicing with step is not supported");
                 }
                 auto start = maybe_start.is_none()
                                  ? self.range().left()
                                  : nb::cast<index_type>(maybe_start);
                 auto stop = maybe_stop.is_none()
                                 ? self.range().right()
                                 : nb::cast<index_type>(maybe_stop);
                 auto values_list = nb::list(values);
                 auto slice_obj = self(start, stop);
                 if (values_list.size() != slice_obj.range().length()) {
                     throw std::invalid_argument(
                         "Attempt to assign sequence of size " +
                         std::to_string(values_list.size()) +
                         " to slice of size " +
                         std::to_string(slice_obj.range().length()));
                 }
                 std::copy(values_list.begin(), values_list.end(),
                           slice_obj.begin());
             })
        .def(
            "index",
            [](const Array<nb::object>& self, nb::object value,
               std::optional<index_type> start,
               std::optional<index_type> stop) {
                index_type real_start =
                    start.has_value() ? start.value() : self.range().left();
                index_type real_stop =
                    stop.has_value() ? stop.value() : self.range().right();
                auto slice = self(real_start, real_stop);
                auto it = find(slice, value);
                if (it == self.end()) {
                    throw nb::value_error("The value is not in the Array");
                }
                return std::distance(self.begin(), it);
            },
            ""_a.noconvert(), ""_a = nb::none(), ""_a = nb::none())
        .def("count",
             [](const Array<nb::object>& self, nb::object value) {
                 size_t count = 0;
                 for (const auto& elem : self) {
                     if (elem.equal(value)) {
                         count++;
                     }
                 }
                 return count;
             })
        .def("__repr__",
             [](const Array<nb::object>& self) { return to_string(self); })
        .def("__copy__", [](const Array<nb::object>& self) { return self; })
        .def("__deepcopy__",
             [](const Array<nb::object>& self, nb::dict) { return self; });
}
