#include "../../nanobind/include/type_cast_vector.hpp"
#include <coconext/types/vector.hpp>
#include <nanobind/nanobind.h>

namespace nb = nanobind;
using namespace coconext::types;

Vector<int> element_wise_add(Vector<int> const& a, Vector<int> const& b) {
    if (a.range() != b.range()) {
        throw std::invalid_argument("Lengths/Ranges must match");
    }

    Vector<int> res(a.range());
    auto a_it = a.begin();
    auto b_it = b.begin();
    auto res_it = res.begin();

    for (; a_it != a.end(); ++a_it, ++b_it, ++res_it) {
        *res_it = *a_it + *b_it;
    }

    return res;
}

void init_test_vector_caster(nb::module_& m) {
    nb::module_ sm = m.def_submodule("test_vector_caster_ext", "Vector caster tests");

    sm.def(
        "element_wise_add",
        &element_wise_add,
        "Add two Vectors element-wise",
        nb::arg("a"),
        nb::arg("b")
    );
}
