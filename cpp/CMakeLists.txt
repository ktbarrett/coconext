# To make coconext usable as a C++ library,
# this should become a shared library at some point.
add_library(coconext STATIC
    src/logic.cpp
    src/random.cpp
)
target_include_directories(coconext PUBLIC include)
target_compile_features(coconext PUBLIC cxx_std_20)
set_target_properties(coconext PROPERTIES POSITION_INDEPENDENT_CODE TRUE)


add_executable(coconext_tests
    tests/test_logic.cpp
    tests/test_range.cpp
    tests/test_array.cpp)
target_link_libraries(coconext_tests PRIVATE coconext GTest::gtest_main)

gtest_discover_tests(coconext_tests)
