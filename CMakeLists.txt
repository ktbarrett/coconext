cmake_minimum_required(VERSION 3.15)

project(coconext LANGUAGES CXX)

find_package(Python 3.9
  REQUIRED COMPONENTS Interpreter Development.Module
  OPTIONAL_COMPONENTS Development.SABIModule)

find_package(nanobind CONFIG REQUIRED)

include(FetchContent)
include(CTest)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.17.0
  FIND_PACKAGE_ARGS NAMES GTest
)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

# Add libraries
add_subdirectory(cpp)
add_subdirectory(nanobind)

include(CMakePackageConfigHelpers)
install(EXPORT coconextTargets
    FILE coconextTargets.cmake
    NAMESPACE coconext::
    DESTINATION coconext/share/cmake/coconext)
configure_package_config_file(
    cmake/coconextConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/coconextConfig.cmake
    INSTALL_DESTINATION coconext/share/cmake/coconext)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/coconextConfig.cmake
    DESTINATION coconext/share/cmake/coconext)
