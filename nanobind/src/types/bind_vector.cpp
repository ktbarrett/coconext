#include <coconext/types/vector.hpp>

#include <bind_vector.hpp>

#include <nanobind/nanobind.h>
#include <string>

namespace nb = nanobind;

void register_vector(nb::module_& m) {
    coconext_nb::bind_array<coconext::types::Vector<int>>(m, "IntVector");
    coconext_nb::bind_array<coconext::types::Vector<std::string>>(m, "StringVector");
}
