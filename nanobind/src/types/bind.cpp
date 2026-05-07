#include <nanobind/nanobind.h>

#include <coconext/types/concepts.hpp>

namespace nb = nanobind;

using namespace coconext::types;

void register_logic(nb::module_& m);
void register_range(nb::module_& m);

NB_MODULE(types, m) {
    register_logic(m);
    register_range(m);
}
