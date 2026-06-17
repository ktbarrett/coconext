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

nanobind_add_stub(
  _pycoconext_stub
  INSTALL_TIME
  MODULE _pycoconext
  OUTPUT _pycoconext.pyi
  PYTHON_PATH "."
  DEPENDS _pycoconext
)
