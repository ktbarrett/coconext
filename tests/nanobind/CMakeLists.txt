cmake_minimum_required(VERSION 3.20)

project(nanobind_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Python 3.9 COMPONENTS Interpreter Development REQUIRED)
find_package(nanobind CONFIG REQUIRED)

nanobind_add_module(
    nanobind_tests
    STABLE_ABI
    NB_STATIC
    ../../nanobind/src/types/bind_range.cpp
    bind.cpp
    type_cast_vector.cpp
    type_cast_array.cpp
)

target_include_directories(nanobind_tests PRIVATE ../../cpp/include)
