cmake_minimum_required(VERSION 3.20)

project(coconext_python_tests LANGUAGES CXX)

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/CoconextDeveloperMode.cmake)

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

target_compile_features(nanobind_tests PRIVATE cxx_std_20)
target_link_libraries(nanobind_tests PRIVATE coconext::coconext_nb)
coconext_enable_developer_mode(nanobind_tests)
