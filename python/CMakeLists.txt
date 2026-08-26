find_package(Python 3.9
    REQUIRED COMPONENTS Interpreter Development Development.Module
    OPTIONAL_COMPONENTS Development.SABIModule)

add_library(simulator SHARED
    _pygpi/bind.cpp
    _pygpi/embed.cpp
    _pygpi/logging.cpp
)
cocotb_configure_native_target(simulator)
target_include_directories(simulator PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/_pygpi"
    "${PROJECT_SOURCE_DIR}/cpp/gpi/include"
    "${PROJECT_SOURCE_DIR}/cpp/gpi/common"
)
target_compile_definitions(simulator PRIVATE PYGPI_EXPORTS)
target_link_libraries(simulator PRIVATE gpi Python::Python)

if(SKBUILD_STATE STREQUAL "editable")
    set_target_properties(simulator PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${COCOTB_PYTHON_PACKAGE_DIR}"
        RUNTIME_OUTPUT_DIRECTORY "${COCOTB_PYTHON_PACKAGE_DIR}"
    )
else()
    set_target_properties(simulator PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/cocotb"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/cocotb"
    )
endif()

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
    _pycoconext/types/bind_int.cpp
)
target_link_libraries(_pycoconext PRIVATE coconext::coconext_nb)
coconext_enable_developer_mode(_pycoconext)

install(TARGETS _pycoconext DESTINATION .)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/_pycoconext.pyi" DESTINATION .)

if(WIN32)
    set(COCOTB_PYTHON_EXTENSION_SUFFIX ".${Python_SOABI}.pyd")
else()
    set(COCOTB_PYTHON_EXTENSION_SUFFIX ".${Python_SOABI}.so")
endif()
set_target_properties(simulator PROPERTIES
    PREFIX ""
    SUFFIX "${COCOTB_PYTHON_EXTENSION_SUFFIX}"
)

if(COCOTB_INSTALL_ARTIFACTS)
    install(
        TARGETS simulator
        RUNTIME DESTINATION cocotb
        LIBRARY DESTINATION cocotb
    )
endif()
