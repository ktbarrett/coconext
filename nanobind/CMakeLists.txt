add_library(coconext_nb INTERFACE)
target_include_directories(coconext_nb INTERFACE include)
target_compile_features(coconext_nb INTERFACE cxx_std_20)
target_link_libraries(coconext_nb INTERFACE coconext)

nanobind_add_module(
    _coconext
    STABLE_ABI
    NB_STATIC
    LTO
    src/bind.cpp
    src/types/bind_logic.cpp
    src/types/bind_range.cpp
)
target_link_libraries(_coconext PRIVATE coconext_nb)

install(TARGETS _coconext DESTINATION .)

nanobind_add_stub(
  _coconext_stub
  INSTALL_TIME
  MODULE _coconext
  OUTPUT _coconext.pyi
  PYTHON_PATH "."
  DEPENDS _coconext
)
