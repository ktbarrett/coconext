#ifndef COCONEXT_GPI_TRIGGERS_HPP
#define COCONEXT_GPI_TRIGGERS_HPP

#include <coconext/future.hpp>
#include <gpi.h>
#include <optional>

namespace coconext::simulator {

class ReadOnly {
  private:
    struct SingletonState {
        std::optional<coconext::Future<void>> future_;
    };
    inline static SingletonState singleton_state_;

    static void gpi_callback() noexcept {
        if (singleton_state_.future_) {
            auto future_to_fire = *singleton_state_.future_;
            singleton_state_.future_ = std::nullopt;
            future_to_fire.set_void();
        }
    }

  public:
    ReadOnly() noexcept = default;

    [[nodiscard]] auto operator co_await() noexcept {
        if (!singleton_state_.future_) {
            singleton_state_.future_ = Future<void>{};
            gpi_register_readonly_callback(&ReadOnly::gpi_callback);
        }
        return singleton_state_.future_->operator co_await();
    }
};

class ReadWrite {
  private:
    struct SingletonState {
        std::optional<coconext::Future<void>> future_;
    };
    inline static SingletonState rw_singleton_state_;

    static void gpi_callback() noexcept {
        if (rw_singleton_state_.future_) {
            auto future_to_fire = *rw_singleton_state_.future_;
            rw_singleton_state_.future_ = std::nullopt;
            future_to_fire.set_void();
        }
    }

  public:
    ReadWrite() noexcept = default;

    [[nodiscard]] auto operator co_await() noexcept {
        if (!rw_singleton_state_.future_) {
            rw_singleton_state_.future_ = Future<void>{};
            gpi_register_readwrite_callback(&ReadWrite::gpi_callback);
        }
        return rw_singleton_state_.future_->operator co_await();
    }
};

}  // namespace coconext::simulator

#endif  // COCONEXT_GPI_TRIGGERS_HPP
