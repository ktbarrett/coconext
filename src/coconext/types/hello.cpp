#include <nanobind/nanobind.h>

NB_MODULE(types, m) {
    m.def("hello", []() { return "Hello world!"; });
}
