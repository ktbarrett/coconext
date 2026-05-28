#include <nanobind/nanobind.h>
#include <nanobind/operators.h>        // Required for operator overloading
#include <nanobind/stl/string.h>       // Required for std::string conversions
#include <nanobind/stl/string_view.h>  // Required for std::string_view conversions

#include <coconext/types/concepts.hpp>
#include <coconext/types/logic.hpp>
#include <type_traits>

namespace nb = nanobind;
using namespace nb::literals;

using namespace coconext::types;

static ResolveMethod string_to_resolve_method(std::string_view method) {
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
        throw std::invalid_argument("Unknown resolve method");
    }
}

void register_logic(nb::module_& m) {
    // Using nb::self on the operators confuses nanobind when there is
    // inheritance involved. Therefore, we use lambda functions to define the
    // operators. MUST REMEMBER TO ADD nb::is_operator() TO THE DEFINITIONS TOO.

    nb::enum_<ResolveMethod>(m, "ResolveMethod")
        .value("ERROR", ResolveMethod::ERROR)
        .value("WEAK", ResolveMethod::WEAK)
        .value("ZEROS", ResolveMethod::ZEROS)
        .value("ONES", ResolveMethod::ONES)
        .value("RANDOM", ResolveMethod::RANDOM);

    nb::class_<Logic>(m, "Logic")
        .def(nb::init<Logic const&>())
        .def(
            "__init__",
            [](Logic* self, Bit const& value) { new (self) Logic(to_logic(value)); }
        )
        .def(
            "__init__",
            [](Logic* self, std::string_view value) { new (self) Logic(to_logic(value)); }
        )
        .def(
            "__init__",
            [](Logic* self, long long value) { new (self) Logic(to_logic(value)); }
        )
        .def("__str__", [](Logic const& self) { return to_string(self); })
        .def("__index__", &to_int)
        .def("__bool__", [](Logic const& self) { return to_int(self) != 0; })
        .def(
            "__repr__",
            [](Logic const& self) {
                auto res = std::string("Logic('");
                res += to_string(self);
                res += "')";
                return res;
            }
        )
        .def("__len__", [](Logic const&) { return 1; })
        .def(
            "__eq__",
            [](Logic const& lhs, Logic const& rhs) { return lhs == rhs; },
            nb::is_operator()
        )
        .def(
            "__eq__",
            [](Logic const& lhs, Bit const& rhs) { return lhs == rhs; },
            nb::is_operator()
        )
        .def(
            "__eq__",
            [](Logic const& self, long long other) {
                try {
                    return self == to_logic(other);
                } catch (std::invalid_argument const&) {
                    return false;
                }
            },
            nb::is_operator()
        )
        .def(
            "__eq__",
            [](Logic const& self, std::string_view other) {
                try {
                    return self == to_logic(other);
                } catch (std::invalid_argument const&) {
                    return false;
                }
            },
            nb::is_operator()
        )
        .def(
            "__or__",
            [](Logic const& lhs, Logic const& rhs) { return lhs | rhs; },
            nb::is_operator()
        )
        .def(
            "__or__", [](Logic const& a, Bit const& b) { return a | b; }, nb::is_operator()
        )
        .def(
            "__and__",
            [](Logic const& lhs, Logic const& rhs) { return lhs & rhs; },
            nb::is_operator()
        )
        .def(
            "__and__", [](Logic const& a, Bit const& b) { return a & b; }, nb::is_operator()
        )
        .def(
            "__xor__",
            [](Logic const& lhs, Logic const& rhs) { return lhs ^ rhs; },
            nb::is_operator()
        )
        .def(
            "__xor__", [](Logic const& a, Bit const& b) { return a ^ b; }, nb::is_operator()
        )
        .def("__invert__", nb::overload_cast<Logic const&>(&operator~), nb::is_operator())
        .def_prop_ro("is_resolvable", &Logic::is_resolvable)
        .def(
            "resolve",
            [](Logic const& self, std::string_view method) {
                return self.resolve(string_to_resolve_method(method));
            }
        )
        .def("resolve", &Logic::resolve)
        .def("__copy__", [](Logic const& self) { return Logic(self); })
        .def("__deepcopy__", [](Logic const& self, nb::dict /* memo */) {
            return Logic(self);
        });

    nb::class_<Bit>(m, "Bit")
        .def(nb::init<Bit const&>())
        .def(
            "__init__", [](Bit* self, Logic const& value) { new (self) Bit(to_bit(value)); }
        )
        .def(
            "__init__",
            [](Bit* self, std::string_view value) { new (self) Bit(to_bit(value)); }
        )
        .def("__init__", [](Bit* self, long long value) { new (self) Bit(to_bit(value)); })
        .def("__str__", [](Bit const& self) { return to_string(self); })
        .def("__index__", &to_int)
        .def("__bool__", [](Bit const& self) { return to_int(self) != 0; })
        .def(
            "__repr__",
            [](Bit const& self) {
                auto res = std::string("Bit('");
                res += to_string(self);
                res += "')";
                return res;
            }
        )
        .def("__len__", [](Bit const&) { return 1; })
        .def(
            "__eq__",
            [](Bit const& lhs, Bit const& rhs) { return lhs == rhs; },
            nb::is_operator()
        )
        .def(
            "__eq__",
            [](Bit const& lhs, Logic const& rhs) { return lhs == rhs; },
            nb::is_operator()
        )
        .def(
            "__eq__",
            [](Bit const& self, long long other) {
                try {
                    return self == to_bit(other);
                } catch (std::invalid_argument const&) {
                    return false;
                }
            },
            nb::is_operator()
        )
        .def(
            "__eq__",
            [](Bit const& self, std::string_view other) {
                try {
                    return self == to_bit(other);
                } catch (std::invalid_argument const&) {
                    return false;
                }
            },
            nb::is_operator()
        )
        .def(
            "__or__",
            [](Bit const& lhs, Bit const& rhs) { return lhs | rhs; },
            nb::is_operator()
        )
        .def(
            "__or__", [](Bit const& a, Logic const& b) { return a | b; }, nb::is_operator()
        )
        .def(
            "__and__",
            [](Bit const& lhs, Bit const& rhs) { return lhs & rhs; },
            nb::is_operator()
        )
        .def(
            "__and__",
            [](Bit const& lhs, Logic const& rhs) { return lhs & rhs; },
            nb::is_operator()
        )
        .def(
            "__xor__",
            [](Bit const& lhs, Bit const& rhs) { return lhs ^ rhs; },
            nb::is_operator()
        )
        .def(
            "__xor__",
            [](Bit const& lhs, Logic const& rhs) { return lhs ^ rhs; },
            nb::is_operator()
        )
        .def(
            "__invert__", [](Bit const& self) { return ~self; }, nb::is_operator()
        )
        .def_prop_ro("is_resolvable", &Bit::is_resolvable)
        .def(
            "resolve",
            [](Bit const& self, std::string_view method) {
                return self.resolve(string_to_resolve_method(method));
            }
        )
        .def("resolve", &Bit::resolve)
        .def("__copy__", [](Bit const& self) { return Bit(self); })
        .def("__deepcopy__", [](Bit const& self, nb::dict /* memo */) {
            return Bit(self);
        });
}
