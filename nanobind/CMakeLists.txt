add_library(coconext_nb INTERFACE)
target_include_directories(coconext_nb INTERFACE include)
target_compile_features(coconext_nb INTERFACE cxx_std_20)
target_link_libraries(coconext_nb INTERFACE coconext)

nanobind_add_module(
    _pycoconext
    STABLE_ABI
    NB_STATIC
    LTO
    src/bind.cpp
    src/types/bind_logic.cpp
    src/types/bind_range.cpp
)
target_link_libraries(_pycoconext PRIVATE coconext_nb)

install(TARGETS _pycoconext DESTINATION .)

# Ship the checked-in type stub next to the extension.
# The dev_build flow re-generates this file and there are check which fail if
# the checked in version becomes out-of-date.
install(FILES "${CMAKE_SOURCE_DIR}/python/_pycoconext.pyi" DESTINATION .)
