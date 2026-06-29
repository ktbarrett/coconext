#include "../../nanobind/include/type_cast_array.hpp"
#include <coconext/types/array.hpp>
#include <nanobind/nanobind.h>

namespace nb = nanobind;
using namespace coconext::types;

using TestArray4 = Array<int, 3, Direction::DOWNTO, 0>;

TestArray4 element_wise_add_array(TestArray4 const& a, TestArray4 const& b) {
    TestArray4 res;

    auto a_it = a.begin();
    auto b_it = b.begin();
    auto res_it = res.begin();

    for (; a_it != a.end(); ++a_it, ++b_it, ++res_it) {
        *res_it = *a_it + *b_it;
    }

    return res;
}

void init_test_array_caster(nb::module_& m) {
    nb::module_ sm = m.def_submodule("test_array_caster_ext", "Array caster tests");

    sm.def(
        "element_wise_add_array",
        &element_wise_add_array,
        "Add two fixed-size Arrays element-wise",
        nb::arg("a"),
        nb::arg("b")
    );
}
