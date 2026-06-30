#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>       // IWYU pragma: keep -- std::string caster
#include <nanobind/stl/string_view.h>  // IWYU pragma: keep -- std::string_view caster

#include <algorithm>
#include <coconext/types/direction.hpp>
#include <coconext/types/hash.hpp>
#include <coconext/types/range.hpp>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iterator>
#include <stdexcept>
#include <string_view>

namespace nb = nanobind;
using namespace nb::literals;

using namespace coconext::types;

Range slice_range(Range const& r, size_t start, size_t stop) {
    auto new_left = r[start];
    auto new_right = r[stop - 1];
    return Range(new_left, r.direction, new_right);
}

void register_range(nb::module_& m) {
    nb::object range_func = nb::module_::import_("builtins").attr("range");

    nb::enum_<Direction>(m, "Direction")
        .value("TO", Direction::TO)
        .value("DOWNTO", Direction::DOWNTO);

    nb::class_<Range>(m, "Range")
        .def(
            nb::init<int32_t, Direction, int32_t>(),
            "left"_a.noconvert(),
            "direction"_a,
            "right"_a.noconvert()
        )
        .def(
            "__init__",
            [](Range* self, int32_t left, std::string_view direction, int32_t right) {
                Direction dir = to_direction(direction);
                new (self) Range(left, dir, right);
            },
            "left"_a.noconvert(),
            "direction"_a,
            "right"_a.noconvert()
        )
        .def(nb::init<int32_t, int32_t>(), "left"_a.noconvert(), "right"_a.noconvert())
        .def(
            "to_range",
            [](Range const& self) {
                auto step_c = (self.direction == Direction::TO) ? 1 : -1;
                auto start = nb::cast(self.left);
                auto stop = nb::cast(self.right + step_c);
                auto step = nb::cast(step_c);
                auto range_type = nb::module_::import_("builtins").attr("range");
                return range_type(start, stop, step);
            }
        )
        .def_static(
            "from_range",
            [](nb::object const& range_obj) {
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
            "range"_a
        )
        .def_ro("left", &Range::left)
        .def_ro("right", &Range::right)
        .def_prop_ro(
            "direction", [](Range const& self) { return to_string(self.direction); }
        )
        .def("__len__", [](Range const& self) { return self.length(); })
        .def(
            "__contains__",
            [](Range const& self, int32_t index) {
                return (self.direction == Direction::TO)
                         ? (index >= self.left && index <= self.right)
                         : (index <= self.left && index >= self.right);
            },
            nb::arg().noconvert()
        )
        .def(
            "__iter__",
            [range_func](Range const& self) {
                return nb::iter(range_func(
                    self.left,
                    self.right + ((self.direction == Direction::TO) ? 1 : -1),
                    (self.direction == Direction::TO) ? 1 : -1
                ));
            }
        )
        .def(
            "__reversed__",
            [range_func](Range const& self) {
                return nb::iter(range_func(
                    self.right,
                    self.left - ((self.direction == Direction::TO) ? 1 : -1),
                    (self.direction == Direction::TO) ? -1 : 1
                ));
            }
        )
        .def("__getitem__", &Range::operator[], nb::arg().noconvert())
        .def(
            "__getitem__",
            [](Range const& self, nb::slice slice) {
                auto [start, stop, step, len] = slice.compute(self.length());
                if (step != 1) {
                    throw std::invalid_argument("Slicing with step is not supported");
                }
                return slice_range(self, start, stop);
            },
            nb::arg().noconvert()
        )
        // Python __eq__/__hash__ mirror cocotb's same-sequence semantics while C++ Range
        // equality is structural.
        .def(
            "__eq__",
            [](Range const& self, nb::handle other) -> nb::object {
                if (!nb::isinstance<Range>(other)) {
                    return nb::cast<nb::object>(nb::not_implemented());
                }
                auto const& rhs = nb::cast<Range const&>(other);
                auto const len = self.length();
                if (len != rhs.length()) {
                    return nb::cast(false);
                }
                if (len == 0) {
                    return nb::cast(true);
                }
                if (self.left != rhs.left) {
                    return nb::cast(false);
                }
                if (len == 1) {
                    return nb::cast(true);
                }
                return nb::cast(self.direction == rhs.direction);
            }
        )
        .def(
            "__hash__",
            [](Range const& self) {
                auto const len = self.length();
                if (len == 0) {
                    return std::hash<size_t>{}(0);
                }
                if (len == 1) {
                    return std::hash<Range::value_type>{}(self.left);
                }
                return detail::hash_combine(self.left, self.right, self.direction);
            }
        )
        .def(
            "__repr__",
            [](Range const& self) {
                return std::format(
                    "Range({}, '{}', {})", self.left, to_string(self.direction), self.right
                );
            }
        )
        .def(
            "index",
            [](Range const& self, int32_t value) {
                auto it = find(self, value);
                if (it == self.end()) {
                    throw nb::value_error("Value not found in range");
                }
                return std::distance(self.begin(), it);
            },
            nb::arg().noconvert()
        )
        .def(
            "index",
            [](Range const& self, int32_t value, int32_t start) {
                auto len = static_cast<int32_t>(self.length());

                start = std::clamp(start, int32_t(0), len);

                if (start >= len) {
                    throw nb::value_error("Value not found in range");
                }

                auto sliced = slice_range(self, start, len);

                auto it = find(sliced, value);

                if (it == sliced.end()) {
                    throw nb::value_error("Value not found in range");
                }

                return start + std::distance(sliced.begin(), it);
            },
            nb::arg().noconvert(),
            nb::arg().noconvert()
        )
        .def(
            "index",
            [](Range const& self, int32_t value, int32_t start, int32_t stop) {
                auto len = static_cast<int32_t>(self.length());

                // Python-compatible clamping
                start = std::clamp(start, int32_t(0), len);
                stop = std::clamp(stop, int32_t(0), len);

                // Empty search region
                if (start >= stop) {
                    throw nb::value_error("Value not found in range");
                }

                auto sliced = slice_range(self, start, stop);

                auto it = find(sliced, value);

                if (it == sliced.end()) {
                    throw nb::value_error("Value not found in range");
                }

                return start + std::distance(sliced.begin(), it);
            },
            nb::arg().noconvert(),
            nb::arg().noconvert(),
            nb::arg().noconvert()
        )
        .def(
            "count",
            [](Range const& self, int32_t value) noexcept {
                // TODO Handle the fact that non-integer values should return 0
                // rather than throwing
                return (self.direction == Direction::TO)
                         ? (value >= self.left && value <= self.right ? 1 : 0)
                         : (value <= self.left && value >= self.right ? 1 : 0);
            },
            nb::arg().noconvert()
        )
        .def("__copy__", [](Range const& self) { return Range(self); })
        .def("__deepcopy__", [](Range const& self, nb::object const&) {
            return Range(self);
        });
}
