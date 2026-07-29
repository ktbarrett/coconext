#include <nanobind/nanobind.h>

namespace nb = nanobind;

void register_logic(nb::module_& m);
void register_logic_array(nb::module_& m);
void register_range(nb::module_& m);

NB_MODULE(_pycoconext, m) {
    register_logic(m);
    register_range(m);
    // LogicArray bindings reference `Logic` and `Bit` via m.attr() at binding
    // time, so must run after register_logic.
    register_logic_array(m);
}
