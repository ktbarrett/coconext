cmake_minimum_required(VERSION 3.20)

project(coconext_tests LANGUAGES CXX)

find_package(coconext CONFIG REQUIRED)

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

add_executable(coconext_tests
    test_logic.cpp
    test_range.cpp
    test_array.cpp
    test_static_array.cpp)
target_link_libraries(coconext_tests PRIVATE coconext::coconext GTest::gtest_main)

gtest_discover_tests(coconext_tests)
