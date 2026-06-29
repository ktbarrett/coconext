#include "../../nanobind/include/bind_vector.hpp"
#include <coconext/types/vector.hpp>
#include <nanobind/nanobind.h>
#include <string>

namespace nb = nanobind;
using namespace coconext::types;

void register_range(nb::module_& m);
void init_test_vector_caster(nb::module_& m);
void init_test_array_caster(nb::module_& m);

NB_MODULE(nanobind_tests, m) {

    register_range(m);

    coconext_nb::bind_array<Vector<int>>(m, "IntVector");
    coconext_nb::bind_array<Vector<std::string>>(m, "StringVector");

    init_test_vector_caster(m);
    init_test_array_caster(m);
}
