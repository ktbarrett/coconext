#ifndef COCONEXT_INT_HPP
#define COCONEXT_INT_HPP

#include <coconext/types/int_base.hpp>

// TODO these macros must explicitly
// deny unimplemented operations for BigInts.

#define COCONEXT_DEFINE_UINT_BINARY_OP(OP)                                                 \
    constexpr UInt operator OP(const UInt& rhs) const {                                    \
        auto native_result = this->storage.raw() OP rhs.storage.raw();                     \
        return UInt(detail::Storage<Bits, false>(native_result));                          \
    }

#define COCONEXT_DEFINE_UINT_UNARY_OP(OP)                                                  \
    constexpr UInt operator OP() const {                                                   \
        auto native_result = OP this->storage.raw();                                       \
        return UInt(detail::Storage<Bits, false>(native_result));                          \
    }

#define COCONEXT_DEFINE_UINT_COMPARE_OP(OP)                                                \
    constexpr bool operator OP(const UInt& rhs) const {                                    \
        return this->storage.raw() OP rhs.storage.raw();                                   \
    }

#define COCONEXT_DEFINE_SINT_BINARY_OP(OP)                                                 \
    constexpr SInt operator OP(const SInt& rhs) const {                                    \
        using RawT = std::decay_t<decltype(this->storage.raw())>;                          \
        if constexpr (std::is_integral_v<RawT>) {                                          \
            using SignedT = std::make_signed_t<RawT>;                                      \
            auto native_result = static_cast<SignedT>(this->storage.raw())                 \
                OP static_cast<SignedT>(rhs.storage.raw());                                \
            return SInt(detail::Storage<Bits, true>(native_result));                       \
        } else {                                                                           \
            auto native_result = this->storage.raw() OP rhs.storage.raw();                 \
            return SInt(detail::Storage<Bits, true>(native_result));                       \
        }                                                                                  \
    }

#define COCONEXT_DEFINE_SINT_UNARY_OP(OP)                                                  \
    constexpr SInt operator OP() const {                                                   \
        auto native_result = OP this->storage.raw();                                       \
        return SInt(detail::Storage<Bits, true>(native_result));                           \
    }

#define COCONEXT_DEFINE_SINT_COMPARE_OP(OP)                                                \
    constexpr bool operator OP(const SInt& rhs) const {                                    \
        using RawT = std::decay_t<decltype(this->storage.raw())>;                          \
        if constexpr (std::is_integral_v<RawT>) {                                          \
            using SignedT = std::make_signed_t<RawT>;                                      \
            return static_cast<SignedT>(this->storage.raw())                               \
                OP static_cast<SignedT>(rhs.storage.raw());                                \
        } else {                                                                           \
            return this->storage.raw() OP rhs.storage.raw();                               \
        }                                                                                  \
    }

namespace coconext::types {

template <size_t Bits>
class UInt {
  public:
    static constexpr size_t num_bits = Bits;
    static constexpr bool is_signed = false;

  private:
    [[no_unique_address]] detail::Storage<Bits, is_signed> storage;

  public:
    constexpr UInt() = default;

    constexpr UInt(detail::Storage<Bits, is_signed> const& val) : storage(val) {}

    constexpr UInt(detail::Storage<Bits, is_signed>&& val) noexcept
        : storage(std::move(val)) {}

    COCONEXT_DEFINE_BINARY_OP(UInt, +)
    COCONEXT_DEFINE_BINARY_OP(UInt, -)
    COCONEXT_DEFINE_BINARY_OP(UInt, *)
    COCONEXT_DEFINE_BINARY_OP(UInt, /)
    COCONEXT_DEFINE_BINARY_OP(UInt, %)
    COCONEXT_DEFINE_BINARY_OP(UInt, &)
    COCONEXT_DEFINE_BINARY_OP(UInt, |)
    COCONEXT_DEFINE_BINARY_OP(UInt, ^)
    COCONEXT_DEFINE_BINARY_OP(UInt, <<)
    COCONEXT_DEFINE_BINARY_OP(UInt, >>)

    COCONEXT_DEFINE_UNARY_OP(UInt, ~)

    COCONEXT_DEFINE_COMPARE_OP(UInt, ==)
    COCONEXT_DEFINE_COMPARE_OP(UInt, !=)
    COCONEXT_DEFINE_COMPARE_OP(UInt, <)
    COCONEXT_DEFINE_COMPARE_OP(UInt, >)
    COCONEXT_DEFINE_COMPARE_OP(UInt, <=)
    COCONEXT_DEFINE_COMPARE_OP(UInt, >=)

    constexpr detail::Storage<Bits, is_signed> const& get_backend() const noexcept {
        return storage;
    }
    // All common features for BitArray, Unsigned, Signed, Ufixed, and Sfixed
    // TODO
};

template <size_t Bits>
class SInt {
  public:
    static constexpr size_t num_bits = Bits;
    static constexpr bool is_signed = true;

  private:
    [[no_unique_address]] detail::Storage<Bits, is_signed> storage;

  public:
    constexpr SInt() = default;

    constexpr SInt(detail::Storage<Bits, is_signed> const& val) : storage(val) {}

    constexpr SInt(detail::Storage<Bits, is_signed>&& val) noexcept
        : storage(std::move(val)) {}

    COCONEXT_DEFINE_BINARY_OP(SInt, +)
    COCONEXT_DEFINE_BINARY_OP(SInt, -)
    COCONEXT_DEFINE_BINARY_OP(SInt, *)
    COCONEXT_DEFINE_BINARY_OP(SInt, /)
    COCONEXT_DEFINE_BINARY_OP(SInt, %)
    COCONEXT_DEFINE_BINARY_OP(SInt, &)
    COCONEXT_DEFINE_BINARY_OP(SInt, |)
    COCONEXT_DEFINE_BINARY_OP(SInt, ^)
    COCONEXT_DEFINE_BINARY_OP(SInt, <<)
    COCONEXT_DEFINE_BINARY_OP(SInt, >>)

    COCONEXT_DEFINE_UNARY_OP(SInt, ~)

    COCONEXT_DEFINE_COMPARE_OP(SInt, ==)
    COCONEXT_DEFINE_COMPARE_OP(SInt, !=)
    COCONEXT_DEFINE_COMPARE_OP(SInt, <)
    COCONEXT_DEFINE_COMPARE_OP(SInt, >)
    COCONEXT_DEFINE_COMPARE_OP(SInt, <=)
    COCONEXT_DEFINE_COMPARE_OP(SInt, >=)

    constexpr detail::Storage<Bits, is_signed> const& get_backend() const noexcept {
        return storage;
    }

    // All common features for BitArray, Unsigned, Signed, Ufixed, and Sfixed
    // TODO
};

}  // namespace coconext::types

#undef COCONEXT_DEFINE_BINARY_OP
#undef COCONEXT_DEFINE_UNARY_OP
#undef COCONEXT_DEFINE_COMPARE_OP

#endif

#endif
