cmake_minimum_required(VERSION 3.15...3.26)

project(coconext LANGUAGES CXX)

find_package(Python 3.8
  REQUIRED COMPONENTS Interpreter Development.Module
  OPTIONAL_COMPONENTS Development.SABIModule)

find_package(nanobind CONFIG REQUIRED)

nanobind_add_module(
    types
    STABLE_ABI
    NB_STATIC
    LTO
    src/coconext/types/hello.cpp
)

install(TARGETS types LIBRARY DESTINATION coconext)
