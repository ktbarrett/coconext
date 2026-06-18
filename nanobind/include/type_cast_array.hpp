#ifndef NB_TYPE_CAST_ARRAY_HPP
#define NB_TYPE_CAST_ARRAY_HPP

#include <nanobind/nanobind.h>
#include <optional>
#include <stdexcept>
#include <vector>

#include <coconext/types/array.hpp>
#include <coconext/types/range.hpp>

namespace nanobind::detail {

template <typename T, coconext::types::Range R>
struct type_caster<coconext::types::detail::Array<T, R>> {
  private:
    using Value = coconext::types::detail::Array<T, R>;
    std::optional<Value> value;

  public:
    static constexpr auto Name =
        const_name("cocotb.types.Array[") + make_caster<T>::Name + const_name("]");

    template <typename T_>
    using Cast = movable_cast_t<T_>;

    // Python -> C++ (Dynamic Python Array -> Static C++ Array)
    bool from_python(handle src, uint8_t flags, cleanup_list* cleanup) noexcept {
        try {
            if (!hasattr(src, "range")) {
                return false;
            }

            object py_range = src.attr("range");
            int left = cast<int>(py_range.attr("left"));
            int right = cast<int>(py_range.attr("right"));

            coconext::types::Range py_c_range{left, right};

            if (py_c_range != R) {
                return false;
            }

            if (!isinstance<iterable>(src)) {
                return false;
            }

            constexpr size_t N = R.length();
            std::array<T, N> temp;

            make_caster<T> item_caster;
            size_t count = 0;

            for (handle item : borrow<iterable>(src)) {
                if (count >= N) {
                    return false;
                }

                if (!item_caster.from_python(item, flags, cleanup)) {
                    return false;
                }

                temp[count] = std::move(item_caster.operator Cast<T>());
                count++;
            }

            if (count != N) {
                return false;
            }

            value.emplace(std::move(temp));
            return true;
        } catch (...) {
            return false;
        }
    }

    // C++ -> Python (Static C++ Array -> Dynamic Python Array)
    static handle from_cpp(
        Value const& src, rv_policy policy, cleanup_list* cleanup
    ) noexcept {
        try {
            module_ cocotb_types = module_::import_("cocotb.types");
            object py_Array = cocotb_types.attr("Array");
            object py_Range = cocotb_types.attr("Range");

            list py_list;
            for (auto const& item : src) {
                py_list.append(nanobind::cast(item, policy));
            }

            object cpp_range = nanobind::cast(src.range(), policy);
            int left = nanobind::cast<int>(cpp_range.attr("left"));
            int right = nanobind::cast<int>(cpp_range.attr("right"));

            object pure_py_range = py_Range(left, right);
            object result = py_Array(py_list, nanobind::arg("range") = pure_py_range);
            return result.release();

        } catch (python_error& e) {
            e.restore();
            return handle();
        } catch (std::exception const& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
            return handle();
        } catch (...) {
            PyErr_SetString(PyExc_RuntimeError, "Unknown C++ exception inside from_cpp");
            return handle();
        }
    }

    explicit operator Value*() { return &(*value); }
    explicit operator Value&() { return *value; }
    explicit operator Value&&() { return std::move(*value); }
};

}  // namespace nanobind::detail

#endif  // NB_TYPE_CAST_ARRAY_HPP
