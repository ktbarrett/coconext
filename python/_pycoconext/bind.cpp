// Native implementation of the _pycoconext extension module.
#include <nanobind/nanobind.h>

namespace nb = nanobind;

void register_logic(nb::module_& m);
void register_logic_array(nb::module_& m);
void register_range(nb::module_& m);
void register_unsigned(nb::module_& m);
void register_signed(nb::module_& m);
void register_sfixed(nb::module_& m);
void register_ufixed(nb::module_& m);

NB_MODULE(_pycoconext, m) {
    register_logic(m);
    register_range(m);
    // LogicArray bindings reference `Logic` and `Bit` via m.attr() at binding
    // time, so must run after register_logic.
    register_logic_array(m);
    register_unsigned(m);
    register_signed(m);
    register_sfixed(m);
    register_ufixed(m);
}
