# Header-only helpers for exposing coconext C++ types through nanobind.
add_library(coconext_nb INTERFACE)
add_library(coconext::coconext_nb ALIAS coconext_nb)
target_sources(coconext_nb INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/coconext_nb/types/bind_vector.hpp>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/coconext_nb/types/cast_array.hpp>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/coconext_nb/types/cast_vector.hpp>)
target_include_directories(coconext_nb INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:coconext/include>)
target_compile_features(coconext_nb INTERFACE cxx_std_20)
target_link_libraries(coconext_nb INTERFACE coconext)

install(TARGETS coconext_nb
    EXPORT coconextTargets)
install(DIRECTORY include/ DESTINATION coconext/include)
