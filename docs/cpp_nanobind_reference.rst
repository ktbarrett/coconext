C++ Nanobind Library Reference Manual
#####################################

The ``coconext::coconext_nb`` CMake target provides header-only helpers for
using coconext C++ types in a nanobind extension module. Link it alongside a
target created by ``nanobind_add_module``::

   nanobind_add_module(my_module bindings.cpp)
   target_link_libraries(my_module PRIVATE coconext::coconext_nb)

Type casters
============

Include the header for the coconext type that should be converted to and from
its cocotb Python equivalent::

   #include <coconext_nb/types/cast_array.hpp>
   #include <coconext_nb/types/cast_vector.hpp>

Vector bindings
===============

To expose a ``coconext::types::Vector`` as a distinct Python class instead of
using the automatic caster, include the separate binding header::

   #include <coconext_nb/types/bind_vector.hpp>

   coconext_nb::bind_vector<coconext::types::Vector<int>>(m, "IntVector");

As with nanobind's own ``stl/vector.h`` and ``stl/bind_vector.h``, do not make
the automatic vector caster and the class binding visible for the same C++
type in one extension module.
