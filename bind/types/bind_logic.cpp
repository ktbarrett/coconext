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

static Logic to_logic(const nb::int_& value) {
    static nb::int_ zero = nb::int_(0);
    static nb::int_ one = nb::int_(1);
    if (value.equal(zero)) {
        return '0'_l;
    } else if (value.equal(one)) {
        return '1'_l;
    } else {
        throw std::invalid_argument("Invalid logic string");
    }
}

static Bit to_bit(const nb::int_& value) {
    static nb::int_ zero = nb::int_(0);
    static nb::int_ one = nb::int_(1);
    if (value.equal(zero)) {
        return '0'_b;
    } else if (value.equal(one)) {
        return '1'_b;
    } else {
        throw std::invalid_argument("Invalid bit string");
    }
}

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
        .def("__init__",
             [](Logic* self, std::string_view value) {
                 new (self) Logic(to_logic(value));
             })
        .def("__init__",
             [](Logic* self, nb::int_ value) {
                 new (self) Logic(to_logic(value));
             })
        .def(nb::init<const Logic&>())
        .def(nb::init<const Bit&>())
        .def("__str__", &to_string)
        .def("__index__", &to_int)
        .def("__bool__", [](const Logic& self) { return to_int(self) != 0; })
        .def("__repr__",
             [](const Logic& self) {
                 auto res = std::string("Logic('");
                 res += to_string(self);
                 res += "')";
                 return res;
             })
        .def("__len__", [](const Logic&) { return 1; })
        .def("__eq__",
             nb::overload_cast<const Logic&, const Logic&>(&operator==),
             nb::is_operator())
        .def(
            "__eq__",
            [](const Logic& self, nb::int_ other) {
                try {
                    return self == to_logic(other);
                } catch (const std::invalid_argument&) {
                    return false;
                }
            },
            nb::is_operator())
        .def(
            "__eq__",
            [](const Logic& self, std::string_view other) {
                try {
                    return self == to_logic(other);
                } catch (const std::invalid_argument&) {
                    return false;
                }
            },
            nb::is_operator())
        .def("__or__",
             nb::overload_cast<const Logic&, const Logic&>(&operator|),
             nb::is_operator())
        .def("__and__",
             nb::overload_cast<const Logic&, const Logic&>(&operator&),
             nb::is_operator())
        .def("__xor__",
             nb::overload_cast<const Logic&, const Logic&>(&operator^),
             nb::is_operator())
        .def("__invert__", nb::overload_cast<const Logic&>(&operator~),
             nb::is_operator())
        .def_prop_ro("is_resolvable", &is_01)
        .def("resolve",
             [](const Logic& value, std::string_view method) {
                 return resolve(value, string_to_resolve_method(method));
             })
        .def("resolve",
             [](const Logic& self, ResolveMethod method) {
                 return resolve(self, method);
             })
        .def("__copy__", [](const Logic& self) { return Logic(self); })
        .def("__deepcopy__", [](const Logic& self, nb::dict /* memo */) {
            return Logic(self);
        });

    nb::class_<Bit, Logic>(m, "Bit")
        .def("__init__",
             [](Bit* self, std::string_view value) {
                 new (self) Bit(to_bit(value));
             })
        .def("__init__",
             [](Bit* self, nb::int_ value) { new (self) Bit(to_bit(value)); })
        .def("__init__",
             [](Bit* self, const Logic& value) {
                 new (self) Bit(to_bit(value));
             })
        .def(nb::init<const Bit&>())
        .def("__repr__",
             [](const Bit& self) {
                 auto res = std::string("Bit('");
                 res += to_string(self);
                 res += "')";
                 return res;
             })
        .def("__or__", nb::overload_cast<const Bit&, const Bit&>(&operator|),
             nb::is_operator())
        .def("__or__",
             nb::overload_cast<const Logic&, const Logic&>(&operator|),
             nb::is_operator())
        .def("__and__", nb::overload_cast<const Bit&, const Bit&>(&operator&),
             nb::is_operator())
        .def("__and__",
             nb::overload_cast<const Logic&, const Logic&>(&operator&),
             nb::is_operator())
        .def(
            // not sure why overload_cast won't work here.
            "__and__", [](const Bit& a, const Logic& b) { return a & b; },
            nb::is_operator())
        .def("__xor__", nb::overload_cast<const Bit&, const Bit&>(&operator^),
             nb::is_operator())
        .def("__xor__",
             nb::overload_cast<const Logic&, const Logic&>(&operator^),
             nb::is_operator())
        .def("__invert__", nb::overload_cast<const Bit&>(&operator~),
             nb::is_operator())
        .def("resolve",
             [](const Bit& value, std::string_view method) {
                 return resolve(value, string_to_resolve_method(method));
             })
        .def("resolve",
             [](const Bit& self, ResolveMethod method) {
                 return resolve(self, method);
             })
        .def("__copy__", [](const Bit& self) { return Bit(self); })
        .def("__deepcopy__",
             [](const Bit& self, nb::dict /* memo */) { return Bit(self); });
}
