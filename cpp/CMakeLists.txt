# To make coconext usable as a C++ library,
# this should become a shared library at some point.
add_library(coconext STATIC
    src/logic.cpp
    src/random.cpp
)
target_include_directories(coconext PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:coconext/include>)
target_compile_features(coconext PUBLIC cxx_std_20)
set_target_properties(coconext PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

install(TARGETS coconext
    EXPORT coconextTargets
    ARCHIVE DESTINATION coconext/lib)
install(DIRECTORY include/coconext DESTINATION coconext/include)


add_executable(coconext_tests
    tests/test_logic.cpp
    tests/test_range.cpp
    tests/test_array.cpp)
target_link_libraries(coconext_tests PRIVATE coconext GTest::gtest_main)

gtest_discover_tests(coconext_tests)
