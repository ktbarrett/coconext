find_package(Python 3.9
    REQUIRED COMPONENTS Interpreter Development.Module
    OPTIONAL_COMPONENTS Development.SABIModule)
find_package(nanobind CONFIG REQUIRED)

nanobind_add_module(
    _pycoconext
    STABLE_ABI
    NB_STATIC
    LTO
    _pycoconext/bind.cpp
    _pycoconext/types/bind_logic.cpp
    _pycoconext/types/bind_logic_array.cpp
    _pycoconext/types/bind_range.cpp
)
target_link_libraries(_pycoconext PRIVATE coconext::coconext_nb)
coconext_enable_developer_mode(_pycoconext)

install(TARGETS _pycoconext DESTINATION .)

# Ship the checked-in type stub next to the extension. The dev_build flow
# regenerates this file and checks that the checked-in version is current.
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/_pycoconext.pyi" DESTINATION .)
