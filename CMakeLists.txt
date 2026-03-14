cmake_minimum_required(VERSION 3.15)

project(coconext LANGUAGES CXX)

# To make coconext usable as a C++ library,
# this should become a shared library at some point.
add_library(coconext STATIC
    cpp/src/logic.cpp
    cpp/src/random.cpp
)
target_include_directories(coconext PUBLIC cpp/include)
target_compile_features(coconext PUBLIC cxx_std_20)
set_target_properties(coconext PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

find_package(Python 3.9
  REQUIRED COMPONENTS Interpreter Development.Module
  OPTIONAL_COMPONENTS Development.SABIModule)

find_package(nanobind CONFIG REQUIRED)

add_library(coconext_nb INTERFACE)
target_include_directories(coconext_nb INTERFACE nanobind/include)
target_compile_features(coconext PUBLIC cxx_std_20)
target_link_libraries(coconext_nb INTERFACE coconext)

nanobind_add_module(
    types
    STABLE_ABI
    NB_STATIC
    LTO
    nanobind/src/types/bind.cpp
    nanobind/src/types/bind_logic.cpp
    nanobind/src/types/bind_range.cpp
    nanobind/src/types/bind_array.cpp
)
target_link_libraries(types PRIVATE coconext_nb)

install(TARGETS types DESTINATION coconext)

nanobind_add_stub(
  types_stub
  INSTALL_TIME
  MODULE coconext.types
  OUTPUT coconext/types.pyi
  PYTHON_PATH "."
)
