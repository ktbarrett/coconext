#include <nanobind/make_iterator.h>  // Required for making iterators
#include <nanobind/nanobind.h>
#include <nanobind/operators.h>        // Required for operator overloading
#include <nanobind/stl/string.h>       // Required for std::string conversions
#include <nanobind/stl/string_view.h>  // Required for std::string_view conversions

#include <coconext/types/concepts.hpp>
#include <coconext/types/range.hpp>
#include <cstdint>

namespace nb = nanobind;
using namespace nb::literals;

using namespace coconext::types;

void register_range(nb::module_& m) {
    nb::object range_func = nb::module_::import_("builtins").attr("range");

    nb::enum_<Direction::value_type>(m, "Direction")
        .value("TO", Direction::TO)
        .value("DOWNTO", Direction::DOWNTO);

    nb::class_<Range>(m, "Range")
        .def(nb::init<int32_t, Direction::value_type, int32_t>(),
             "left"_a.noconvert(), "direction"_a, "right"_a.noconvert())
        .def(
            "__init__",
            [](Range* self, int32_t left, std::string_view direction,
               int32_t right) {
                Direction dir = to_direction(direction);
                new (self) Range(left, dir, right);
            },
            "left"_a.noconvert(), "direction"_a, "right"_a.noconvert())
        .def(nb::init<int32_t, int32_t>(), "left"_a.noconvert(),
             "right"_a.noconvert())
        .def("to_range",
             [](const Range& self) {
                 auto step_c = (self.direction() == Direction::TO) ? 1 : -1;
                 auto start = nb::cast(self.left());
                 auto stop = nb::cast(self.right() + step_c);
                 auto step = nb::cast(step_c);
                 auto range_type =
                     nb::module_::import_("builtins").attr("range");
                 return range_type(start, stop, step);
             })
        .def_static(
            "from_range",
            [](const nb::object& range_obj) {
                auto start = nb::cast<int32_t>(range_obj.attr("start"));
                auto stop = nb::cast<int32_t>(range_obj.attr("stop"));
                auto step = nb::cast<int32_t>(range_obj.attr("step"));
                Direction direction;
                if (step == 1) {
                    direction = Direction::TO;
                } else if (step == -1) {
                    direction = Direction::DOWNTO;
                } else {
                    throw std::invalid_argument("Invalid step value");
                }
                int32_t right = stop - step;
                return Range(start, direction, right);
            },
            "range"_a)
        .def_prop_ro("left", &Range::left)
        .def_prop_ro("right", &Range::right)
        .def_prop_ro(
            "direction",
            [](const Range& self) { return to_string(self.direction()); })
        .def("__len__", [](const Range& self) { return self.length(); })
        .def(
            "__contains__",
            [](const Range& self, int32_t index) {
                return (self.direction() == Direction::TO)
                           ? (index >= self.left() && index <= self.right())
                           : (index <= self.left() && index >= self.right());
            },
            nb::arg().noconvert())
        .def("__iter__",
             [range_func](const Range& self) {
                 return nb::iter(range_func(
                     self.left(),
                     self.right() +
                         ((self.direction() == Direction::TO) ? 1 : -1),
                     (self.direction() == Direction::TO) ? 1 : -1));
             })
        .def("__reversed__",
             [range_func](const Range& self) {
                 return nb::iter(range_func(
                     self.right(),
                     self.left() -
                         ((self.direction() == Direction::TO) ? 1 : -1),
                     (self.direction() == Direction::TO) ? -1 : 1));
             })
        .def("__getitem__", &Range::operator[], nb::arg().noconvert())
        .def(
            "__getitem__",
            [](const Range& self, nb::slice slice) {
                auto [start, stop, step, len] = slice.compute(self.length());
                if (step != 1) {
                    throw std::invalid_argument(
                        "Slicing with step is not supported");
                }
                return self(start, stop);
            },
            nb::arg().noconvert())
        .def(nb::self == nb::self, nb::arg().noconvert())
        .def("__hash__",
             [](const Range& self) { return std::hash<Range>()(self); })
        .def("__repr__", [](const Range& self) { return to_string(self); })
        .def(
            "index",
            [](const Range& self, int32_t value) {
                auto it = find(self, value);
                if (it == self.end()) {
                    throw nb::value_error("Value not found in range");
                }
                return std::distance(self.begin(), it);
            },
            nb::arg().noconvert())
        .def(
            "count",
            [](const Range& self, int32_t value) noexcept {
                // TODO Handle the fact that non-integer values should return 0
                // rather than throwing
                return (self.direction() == Direction::TO)
                           ? (value >= self.left() && value <= self.right() ? 1
                                                                            : 0)
                           : (value <= self.left() && value >= self.right()
                                  ? 1
                                  : 0);
            },
            nb::arg().noconvert())
        .def("__copy__", [](const Range& self) { return Range(self); })
        .def("__deepcopy__",
             [](const Range& self, const nb::object&) { return Range(self); });
}
