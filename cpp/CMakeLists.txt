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

# On macOS the install name is @rpath-relative so a preloaded image
# satisfies the dependency by install-name match rather than by search path.
# Looks like RPATH, is actually not.
set_target_properties(coconext PROPERTIES
    MACOSX_RPATH TRUE
    INSTALL_NAME_DIR "@rpath")

install(TARGETS coconext
    EXPORT coconextTargets
    LIBRARY DESTINATION coconext/lib
    RUNTIME DESTINATION coconext/lib
    ARCHIVE DESTINATION coconext/lib)
install(DIRECTORY include/coconext DESTINATION coconext/include)
