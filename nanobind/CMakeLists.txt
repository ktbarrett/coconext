add_library(coconext_nb INTERFACE)
target_include_directories(coconext_nb INTERFACE include)
target_compile_features(coconext PUBLIC cxx_std_20)
target_link_libraries(coconext_nb INTERFACE coconext)

nanobind_add_module(
    types
    STABLE_ABI
    NB_STATIC
    LTO
    src/types/bind.cpp
    src/types/bind_logic.cpp
    src/types/bind_range.cpp
)
target_link_libraries(types PRIVATE coconext_nb)

install(TARGETS types DESTINATION coconext)

nanobind_add_stub(
  types_stub
  INSTALL_TIME
  MODULE coconext.types
  OUTPUT coconext/types.pyi
  PYTHON_PATH "."
)
