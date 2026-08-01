#ifndef COCONEXT_POINTERS_HPP
#define COCONEXT_POINTERS_HPP

#include <cstddef>

namespace coconext::detail {

// CRTP intrusive refcount. Derived must define destroy() -- Future uses `delete this`,
// Task destroys its coroutine frame, TaskManager uses `delete this`.
template <class Derived>
class IntrusiveRefcounted {
  public:
    void inc_ref() noexcept { ++ref_count_; }
    void dec_ref() noexcept {
        if (--ref_count_ == 0) {
            static_cast<Derived*>(this)->destroy();
        }
    }

  private:
    size_t ref_count_{0};
};

}  // namespace coconext::detail

#endif  // COCONEXT_POINTERS_HPP
