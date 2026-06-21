#ifndef NB_TYPE_CAST_VECTOR_HPP
#define NB_TYPE_CAST_VECTOR_HPP

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <optional>
#include <stdexcept>
#include <vector>

#include <coconext/types/direction.hpp>
#include <coconext/types/range.hpp>
#include <coconext/types/vector.hpp>

namespace nanobind::detail {

using namespace coconext::types;

template <typename T>
struct type_caster<Vector<T>> {
  private:
    using Value = Vector<T>;
    using index_t = typename Value::index_type;
    std::optional<Value> value;

  public:
    static constexpr auto Name =
        const_name("cocotb.types.Array[") + make_caster<T>::Name + const_name("]");

    template <typename T_>
    using Cast = movable_cast_t<T_>;

    // Python -> C++ (Array to Vector)
    bool from_python(handle src, uint8_t flags, cleanup_list* cleanup) noexcept {
        try {
            if (!hasattr(src, "range")) {
                return false;
            }

            object py_range = src.attr("range");
            index_t left = cast<index_t>(py_range.attr("left"));
            index_t right = cast<index_t>(py_range.attr("right"));
            std::string dir_str = cast<std::string>(py_range.attr("direction"));
            auto direction = to_direction(dir_str);

            Range c_range{left, direction, right};

            if (!isinstance<iterable>(src)) {
                return false;
            }

            value.emplace(c_range);
            make_caster<T> item_caster;
            auto it = value->begin();
            size_t count = 0;
            size_t expected_len = c_range.length();

            for (handle item : borrow<iterable>(src)) {
                if (count >= expected_len) {
                    value.reset();
                    return false;
                }

                if (!item_caster.from_python(
                        item, flags & ~nanobind::detail::cast_flags::convert, cleanup
                    ))
                {
                    value.reset();
                    return false;
                }

                *it = std::move(item_caster.operator Cast<T>());
                ++it;
                ++count;
            }

            if (count != expected_len) {
                value.reset();
                return false;
            }

            return true;
        } catch (std::exception const& e) {
            fprintf(stderr, "C++ Exception caught: %s\n", e.what());
            return false;
        } catch (...) {
            return false;
        }
    }

    // C++ -> Python (Vector to Array)
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

            index_t left = nanobind::cast<index_t>(cpp_range.attr("left"));
            index_t right = nanobind::cast<index_t>(cpp_range.attr("right"));
            std::string_view py_dir_str = to_string(src.range().direction);

            object pure_py_range = py_Range(left, std::string{py_dir_str}, right);

            object result = py_Array(py_list, nanobind::arg("range") = pure_py_range);
            return result.release();

        } catch (python_error& e) {
            // If a Python exception occurred, restore it so pytest shows the exact
            // traceback
            e.restore();
            return handle();
        } catch (std::exception const& e) {
            // Surface C++ exceptions (like nanobind::cast_error) directly to Python
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

#endif  // NB_TYPE_CAST_VECTOR_HPP
