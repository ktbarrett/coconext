add_library(coconext SHARED
    src/logic.cpp
    src/random.cpp
)

option(COCONEXT_USE_APINT "Use LLVM APInt backend" OFF)
if(COCONEXT_USE_APINT)
    target_compile_definitions(coconext PUBLIC COCONEXT_USE_APINT)
endif()

target_include_directories(coconext PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:coconext/include>)
target_compile_features(coconext PUBLIC cxx_std_20)
set_target_properties(coconext PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

# Needed for MacOS.
set_target_properties(coconext PROPERTIES
    MACOSX_RPATH TRUE
    INSTALL_NAME_DIR "@rpath")

# -- libgpi --------------------------------------------------------------------
#
# This is some CMake to pull in libgpi since cocotb doesn't provide a CMake
# package yet.

# Use cocotb_tools.config to get libgpi binary dir and include dir
execute_process(
    COMMAND ${Python_EXECUTABLE} -c
        "from cocotb_tools.config import libs_dir, share_dir; print(libs_dir); print(share_dir / 'include')"
    OUTPUT_VARIABLE _coconext_cocotb_paths
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY)
string(REPLACE "\n" ";" _coconext_cocotb_paths "${_coconext_cocotb_paths}")
list(GET _coconext_cocotb_paths 0 COCOTB_LIBS_DIR)
list(GET _coconext_cocotb_paths 1 COCOTB_INCLUDE_DIR)

# Create a CMake target for libgpi. cocotb installs libgpi as .so on every platform except Windows (including MacOS).
if(WIN32)
    set(_coconext_libgpi_suffix ".dll")
else()
    set(_coconext_libgpi_suffix ".so")
endif()
add_library(cocotb_gpi SHARED IMPORTED)
set_target_properties(cocotb_gpi PROPERTIES
    IMPORTED_LOCATION "${COCOTB_LIBS_DIR}/libgpi${_coconext_libgpi_suffix}"
    INTERFACE_INCLUDE_DIRECTORIES "${COCOTB_INCLUDE_DIR}")

# -- libcoconext_gpi -----------------------------------------------------------
#
# For parts of the library that have a dependency on libgpi and a running
# simulation, e.g. handles and GPI triggers.

add_library(coconext_gpi SHARED
    src/sim_objects.cpp
)
set_target_properties(coconext_gpi PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
target_link_libraries(coconext_gpi
    PRIVATE
        cocotb_gpi
    PUBLIC
        coconext)

# Needed for MacOS.
set_target_properties(coconext_gpi PROPERTIES
    MACOSX_RPATH TRUE
    INSTALL_NAME_DIR "@rpath")

# RPATH to the current dir to pick up libcoconext.
if(APPLE)
    set(_coconext_origin "@loader_path")
else()
    set(_coconext_origin "$ORIGIN")
endif()
set_target_properties(coconext_gpi PROPERTIES
    INSTALL_RPATH "${_coconext_origin}"
    BUILD_RPATH "${_coconext_origin}")

# -- install -------------------------------------------------------------------

install(TARGETS coconext coconext_gpi
    EXPORT coconextTargets
    LIBRARY DESTINATION coconext/lib
    RUNTIME DESTINATION coconext/lib
    ARCHIVE DESTINATION coconext/lib)
install(DIRECTORY include/coconext DESTINATION coconext/include)
