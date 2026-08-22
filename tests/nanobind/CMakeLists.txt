cmake_minimum_required(VERSION 3.20)

project(nanobind_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Python 3.9
    REQUIRED COMPONENTS Interpreter Development.Module
    OPTIONAL_COMPONENTS Development.SABIModule)
find_package(nanobind CONFIG REQUIRED)
find_package(coconext CONFIG REQUIRED)

nanobind_add_module(
    nanobind_tests
    STABLE_ABI
    NB_STATIC
    bind.cpp
    type_cast_vector.cpp
    type_cast_array.cpp
)

target_link_libraries(nanobind_tests PRIVATE coconext::coconext_nb)
