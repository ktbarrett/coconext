#include <nanobind/nanobind.h>

namespace nb = nanobind;

void register_logic(nb::module_& m);
void register_range(nb::module_& m);

NB_MODULE(_pycoconext, m) {
    register_logic(m);
    register_range(m);
}
