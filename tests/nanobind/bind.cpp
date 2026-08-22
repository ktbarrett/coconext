#include <coconext/types/vector.hpp>
#include <coconext_nb/types/bind_vector.hpp>
#include <nanobind/nanobind.h>
#include <string>

namespace nb = nanobind;
using namespace coconext::types;

void init_test_vector_caster(nb::module_& m);
void init_test_array_caster(nb::module_& m);

NB_MODULE(nanobind_tests, m) {
    m.attr("Range") = nb::module_::import_("coconext.types").attr("Range");

    coconext_nb::bind_vector<Vector<int>>(m, "IntVector");
    coconext_nb::bind_vector<Vector<std::string>>(m, "StringVector");

    init_test_vector_caster(m);
    init_test_array_caster(m);
}
