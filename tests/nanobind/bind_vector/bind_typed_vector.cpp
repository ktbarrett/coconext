#include "../../../nanobind/include/bind_vector.hpp"
#include <coconext/types/vector.hpp>
#include <nanobind/nanobind.h>
#include <string>

namespace nb = nanobind;

NB_MODULE(nanobind_tests, m) {
    coconext_nb::bind_array<coconext::types::Vector<int>>(m, "IntVector");
    coconext_nb::bind_array<coconext::types::Vector<std::string>>(m, "StringVector");
}
