add_library(coconext SHARED
    src/logic.cpp
    src/random.cpp
)
add_library(coconext::coconext ALIAS coconext)

target_include_directories(coconext PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:coconext/include>)
target_compile_features(coconext PUBLIC cxx_std_20)
set_target_properties(coconext PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
coconext_enable_developer_mode(coconext)

# Needed for macOS.
set_target_properties(coconext PROPERTIES
    MACOSX_RPATH TRUE
    INSTALL_NAME_DIR "@rpath")

# Parts of coconext which require a registered simulator interface.
add_library(coconext_gpi SHARED
    src/sim_objects.cpp
)
add_library(coconext::coconext_gpi ALIAS coconext_gpi)
set_target_properties(coconext_gpi PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
target_link_libraries(coconext_gpi
    PRIVATE
        cocotb::gpi
    PUBLIC
        coconext)
coconext_enable_developer_mode(coconext_gpi)

# Needed for macOS.
set_target_properties(coconext_gpi PROPERTIES
    MACOSX_RPATH TRUE
    INSTALL_NAME_DIR "@rpath")

# Locate libcoconext next to libcoconext_gpi at runtime.
if(APPLE)
    set(_coconext_origin "@loader_path")
else()
    set(_coconext_origin "$ORIGIN")
endif()
set_target_properties(coconext_gpi PROPERTIES
    INSTALL_RPATH "${_coconext_origin}"
    BUILD_RPATH "${_coconext_origin}")

install(TARGETS coconext
    EXPORT coconextTargets
    LIBRARY DESTINATION coconext/lib
    RUNTIME DESTINATION coconext/lib
    ARCHIVE DESTINATION coconext/lib)

# In editable builds, cocotb's GPI target is intentionally not part of an
# install export. Keep coconext_gpi out of the corresponding coconext export
# while still installing the library for runtime use.
if(COCOTB_INSTALL_ARTIFACTS)
    install(TARGETS coconext_gpi
        EXPORT coconextTargets
        LIBRARY DESTINATION coconext/lib
        RUNTIME DESTINATION coconext/lib
        ARCHIVE DESTINATION coconext/lib)
else()
    install(TARGETS coconext_gpi
        LIBRARY DESTINATION coconext/lib
        RUNTIME DESTINATION coconext/lib
        ARCHIVE DESTINATION coconext/lib)
endif()
install(DIRECTORY include/ DESTINATION coconext/include)
