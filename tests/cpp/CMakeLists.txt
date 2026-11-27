cmake_minimum_required(VERSION 3.20)

project(coconext_tests LANGUAGES CXX)

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/CoconextDeveloperMode.cmake)

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
    test_static_array.cpp
    test_logic_array.cpp
    test_dyn_int.cpp
    test_dyn_fixed.cpp
    test_int.cpp
    test_reverse.cpp
    test_unsigned.cpp
    test_signed.cpp
    test_intrusive_deque.cpp
    test_not_null.cpp
    test_future.cpp
    test_scheduler_binding.cpp
    test_scheduler_run.cpp
    test_task_manager.cpp
    test_sfixed.cpp
    test_sim_time.cpp
    test_ufixed.cpp
    includes/array.cpp
    includes/bit.cpp
    includes/bit_array.cpp
    includes/bit_vector.cpp
    includes/direction.cpp
    includes/dyn_sfixed.cpp
    includes/dyn_signed.cpp
    includes/dyn_ufixed.cpp
    includes/dyn_unsigned.cpp
    includes/logic.cpp
    includes/logic_array.cpp
    includes/logic_vector.cpp
    includes/range.cpp
    includes/resize_mode.cpp
    includes/sfixed.cpp
    includes/signed.cpp
    includes/sim_time.cpp
    includes/ufixed.cpp
    includes/unsigned.cpp
    includes/vector.cpp)

target_link_libraries(coconext_tests PRIVATE coconext::coconext GTest::gtest_main)
coconext_enable_developer_mode(coconext_tests)

gtest_discover_tests(coconext_tests)
