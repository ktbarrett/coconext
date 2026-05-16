cmake_minimum_required(VERSION 3.15)

project(coconext LANGUAGES CXX)

find_package(Python 3.9
  REQUIRED COMPONENTS Interpreter Development.Module
  OPTIONAL_COMPONENTS Development.SABIModule)

find_package(nanobind CONFIG REQUIRED)

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
