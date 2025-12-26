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

nanobind_add_module(
    types
    STABLE_ABI
    NB_STATIC
    LTO
    bind/types/bind.cpp
    bind/types/bind_logic.cpp
    bind/types/bind_range.cpp
)
target_link_libraries(types PRIVATE coconext)

install(TARGETS types DESTINATION coconext)

nanobind_add_stub(
  types_stub
  INSTALL_TIME
  MODULE coconext.types
  OUTPUT coconext/types.pyi
  PYTHON_PATH "."
)
