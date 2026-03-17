cmake_minimum_required(VERSION 3.15)

project(coconext LANGUAGES CXX)

find_package(Python 3.9
  REQUIRED COMPONENTS Interpreter Development.Module
  OPTIONAL_COMPONENTS Development.SABIModule)

find_package(nanobind CONFIG REQUIRED)

# Add libraries
add_subdirectory(cpp)
add_subdirectory(nanobind)
