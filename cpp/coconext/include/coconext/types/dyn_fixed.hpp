#ifndef COCONEXT_DYN_FIXED_HPP
#define COCONEXT_DYN_FIXED_HPP

#include <algorithm>
#include <cmath>
#include <coconext/types/dyn_int_base.hpp>
#include <coconext/types/logic_array.hpp>
#include <coconext/types/range.hpp>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace coconext::types::detail {

class DynSfixed;

namespace dyn_fixed_detail {

inline void validate_range(Range range) {
    if (range.direction != Direction::DOWNTO) {
        throw std::invalid_argument("Fixed-point range must have DOWNTO direction");
    }
    if (range.length() == 0) {
        throw std::invalid_argument("Fixed-point range must not be empty");
    }
}

inline Range::value_type checked_add(Range::value_type lhs, Range::value_type rhs) {
    if ((rhs > 0 && lhs > std::numeric_limits<Range::value_type>::max() - rhs)
        || (rhs < 0 && lhs < std::numeric_limits<Range::value_type>::min() - rhs))
    {
        throw std::overflow_error("Fixed-point range calculation overflow");
    }
    return lhs + rhs;
}

inline Range::value_type checked_sub(Range::value_type lhs, Range::value_type rhs) {
    if ((rhs < 0 && lhs > std::numeric_limits<Range::value_type>::max() + rhs)
        || (rhs > 0 && lhs < std::numeric_limits<Range::value_type>::min() + rhs))
    {
        throw std::overflow_error("Fixed-point range calculation overflow");
    }
    return lhs - rhs;
}

inline Range add_range(Range lhs, Range rhs) {
    return {
        checked_add(std::max(lhs.left, rhs.left), 1),
        Direction::DOWNTO,
        std::min(lhs.right, rhs.right)
    };
}

inline Range multiply_range(Range lhs, Range rhs) {
    return {
        checked_add(checked_add(lhs.left, rhs.left), 1),
        Direction::DOWNTO,
        checked_add(lhs.right, rhs.right)
    };
}

inline size_t shift_distance(Range::value_type high, Range::value_type low) {
    if (high < low) {
        throw std::invalid_argument("Invalid fixed-point alignment");
    }
    return static_cast<size_t>(high) - static_cast<size_t>(low);
}

inline DynUInt align_unsigned(DynUInt const& value, size_t width, size_t shift) {
    DynUInt aligned(width, value);
    return aligned << shift;
}

inline DynSInt align_signed(DynSInt const& value, size_t width, size_t shift) {
    DynSInt aligned(width, value);
    return aligned << shift;
}

inline bool unsigned_greater(DynUInt const& lhs, DynUInt const& rhs) {
    size_t width = std::max(lhs.width(), rhs.width());
    DynUInt lhs_aligned(width, lhs);
    DynUInt rhs_aligned(width, rhs);
    return rhs_aligned < lhs_aligned;
}

inline bool unsigned_equal(DynUInt const& lhs, DynUInt const& rhs) {
    size_t width = std::max(lhs.width(), rhs.width());
    return DynUInt(width, lhs) == DynUInt(width, rhs);
}

inline DynUInt rounded_unsigned_quotient(
    DynUInt const& dividend, DynUInt const& divisor, size_t result_width
) {
    if (divisor.popcount() == 0) {
        throw std::domain_error("Division by zero");
    }
    auto [quotient, remainder] = divrem(dividend, divisor);
    size_t compare_width = std::max(remainder.width(), divisor.width()) + 1;
    DynUInt doubled(compare_width, remainder);
    doubled = doubled << 1;
    DynUInt extended_divisor(compare_width, divisor);
    bool round_up = unsigned_greater(doubled, extended_divisor)
                 || (unsigned_equal(doubled, extended_divisor) && quotient.width() > 0
                     && quotient.get_bit(0));
    if (round_up) {
        quotient = DynUInt(
            quotient.width(), quotient + DynUInt(quotient.width(), std::uint64_t{1})
        );
    }
    return DynUInt(result_width, quotient);
}

inline DynUInt signed_magnitude(DynSInt const& value) {
    size_t width = value.width() + 1;
    DynSInt extended(width, value);
    if (value.is_negative()) {
        extended = DynSInt(width, -value);
    }
    return DynUInt(width, extended);
}

inline DynSInt signed_from_magnitude(
    DynUInt const& magnitude, bool negative, size_t result_width
) {
    DynSInt result(result_width, magnitude);
    if (!negative) {
        return result;
    }
    return DynSInt(result_width, DynSInt(result_width) - result);
}

inline DynUInt rescale_unsigned_wrap(
    DynUInt const& value, Range::value_type source_right, Range target
) {
    if (source_right >= target.right) {
        size_t shift = shift_distance(source_right, target.right);
        size_t aligned_width = value.width() + shift;
        auto aligned = align_unsigned(value, aligned_width, shift);
        return DynUInt(target.length(), aligned);
    }
    size_t shift = shift_distance(target.right, source_right);
    return DynUInt(target.length(), value >> shift);
}

inline DynSInt rescale_signed_wrap(
    DynSInt const& value, Range::value_type source_right, Range target
) {
    if (source_right >= target.right) {
        size_t shift = shift_distance(source_right, target.right);
        size_t aligned_width = value.width() + shift;
        auto aligned = align_signed(value, aligned_width, shift);
        return DynSInt(target.length(), aligned);
    }

    size_t shift = shift_distance(target.right, source_right);
    if (shift >= value.width()) {
        return DynSInt(target.length(), std::int64_t{0});
    }
    DynSInt shifted = value >> shift;
    if (value.is_negative()) {
        bool discarded = false;
        for (size_t i = 0; i < shift; ++i) {
            if (value.get_bit(i)) {
                discarded = true;
                break;
            }
        }
        if (discarded) {
            shifted = DynSInt(
                shifted.width(), shifted + DynSInt(shifted.width(), std::int64_t{1})
            );
        }
    }
    return DynSInt(target.length(), shifted);
}

inline double scaled_double(std::string const& raw, Range::value_type right) {
    double value = std::strtod(raw.c_str(), nullptr);
    if (right > std::numeric_limits<int>::max()) {
        return value == 0.0 ? value
                            : std::copysign(std::numeric_limits<double>::infinity(), value);
    }
    if (right < std::numeric_limits<int>::min()) {
        return std::copysign(0.0, value);
    }
    return std::ldexp(value, static_cast<int>(right));
}

}  // namespace dyn_fixed_detail

class DynUfixed {
  public:
    DynUfixed(Range range, DynUInt raw) : range_(range), value_(std::move(raw)) {
        dyn_fixed_detail::validate_range(range_);
        if (value_.width() != range_.length()) {
            throw std::invalid_argument("Raw Ufixed width does not match its range");
        }
    }

    explicit DynUfixed(BitVector&& source)
        : DynUfixed(source.range(), bits(std::move(source))) {}

    Range range() const noexcept { return range_; }
    size_t size() const noexcept { return range_.length(); }
    size_t width() const noexcept { return size(); }

    std::string raw_decimal() const { return value_.to_decimal_string(); }
    std::string raw_binary() const { return value_.to_binary_string(); }

    explicit operator bool() const noexcept { return value_.popcount() != 0; }
    explicit operator double() const noexcept {
        return dyn_fixed_detail::scaled_double(raw_decimal(), range_.right);
    }

    bool index(Range::value_type index) const {
        if (!contains(range_, index)) {
            throw std::out_of_range("Ufixed index out of bounds");
        }
        return value_.get_bit(dyn_fixed_detail::shift_distance(index, range_.right));
    }

    void set_index(Range::value_type index, bool bit) {
        if (!contains(range_, index)) {
            throw std::out_of_range("Ufixed index out of bounds");
        }
        value_.set_bit(dyn_fixed_detail::shift_distance(index, range_.right), bit);
    }

    bool operator==(DynUfixed const& rhs) const noexcept {
        return range_ == rhs.range_ && value_ == rhs.value_;
    }

    bool operator<(DynUfixed const& rhs) const {
        require_same_range(rhs);
        return value_ < rhs.value_;
    }
    bool operator<=(DynUfixed const& rhs) const {
        require_same_range(rhs);
        return value_ <= rhs.value_;
    }
    bool operator>(DynUfixed const& rhs) const {
        require_same_range(rhs);
        return value_ > rhs.value_;
    }
    bool operator>=(DynUfixed const& rhs) const {
        require_same_range(rhs);
        return value_ >= rhs.value_;
    }

    DynUfixed operator<<(size_t amount) const {
        return DynUfixed(range_, value_ << amount);
    }
    DynUfixed operator>>(size_t amount) const {
        return DynUfixed(range_, value_ >> amount);
    }
    DynUfixed& operator<<=(size_t amount) {
        value_ = value_ << amount;
        return *this;
    }
    DynUfixed& operator>>=(size_t amount) {
        value_ = value_ >> amount;
        return *this;
    }

    DynSfixed operator+() const;
    DynSfixed operator-() const;

    DynUfixed operator+(DynUfixed const& rhs) const {
        Range result_range = dyn_fixed_detail::add_range(range_, rhs.range_);
        size_t lhs_shift =
            dyn_fixed_detail::shift_distance(range_.right, result_range.right);
        size_t rhs_shift =
            dyn_fixed_detail::shift_distance(rhs.range_.right, result_range.right);
        auto lhs =
            dyn_fixed_detail::align_unsigned(value_, result_range.length(), lhs_shift);
        auto rhs_value =
            dyn_fixed_detail::align_unsigned(rhs.value_, result_range.length(), rhs_shift);
        return DynUfixed(result_range, DynUInt(result_range.length(), lhs + rhs_value));
    }

    DynSfixed operator-(DynUfixed const& rhs) const;

    DynUfixed operator*(DynUfixed const& rhs) const {
        Range result_range = dyn_fixed_detail::multiply_range(range_, rhs.range_);
        return DynUfixed(result_range, value_ * rhs.value_);
    }

    DynUfixed operator/(DynUfixed const& rhs) const {
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }
        Range result_range{
            dyn_fixed_detail::checked_sub(range_.left, rhs.range_.right),
            Direction::DOWNTO,
            dyn_fixed_detail::checked_sub(
                dyn_fixed_detail::checked_sub(range_.right, rhs.range_.left), 1
            )
        };
        size_t shift = rhs.size();
        size_t numerator_width = value_.width() + shift;
        auto numerator = dyn_fixed_detail::align_unsigned(value_, numerator_width, shift);
        auto quotient = dyn_fixed_detail::rounded_unsigned_quotient(
            numerator, rhs.value_, result_range.length()
        );
        return DynUfixed(result_range, std::move(quotient));
    }

    DynUfixed operator%(DynUfixed const& rhs) const {
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }
        Range::value_type right = std::min(range_.right, rhs.range_.right);
        size_t lhs_shift = dyn_fixed_detail::shift_distance(range_.right, right);
        size_t rhs_shift = dyn_fixed_detail::shift_distance(rhs.range_.right, right);
        size_t compute_width = std::max(size() + lhs_shift, rhs.size() + rhs_shift);
        auto lhs = dyn_fixed_detail::align_unsigned(value_, compute_width, lhs_shift);
        auto divisor =
            dyn_fixed_detail::align_unsigned(rhs.value_, compute_width, rhs_shift);
        auto remainder = lhs % divisor;
        Range result_range{
            std::min(range_.left, rhs.range_.left), Direction::DOWNTO, right
        };
        return DynUfixed(result_range, DynUInt(result_range.length(), remainder));
    }

    DynUfixed& operator+=(DynUfixed const& rhs) {
        auto result = *this + rhs;
        value_ = dyn_fixed_detail::rescale_unsigned_wrap(
            bits(result), result.range_.right, range_
        );
        return *this;
    }

    DynUfixed& operator-=(DynUfixed const& rhs);

    DynUfixed& operator*=(DynUfixed const& rhs) {
        auto result = *this * rhs;
        value_ = dyn_fixed_detail::rescale_unsigned_wrap(
            bits(result), result.range_.right, range_
        );
        return *this;
    }

    DynUfixed& operator/=(DynUfixed const& rhs) {
        auto result = *this / rhs;
        value_ = dyn_fixed_detail::rescale_unsigned_wrap(
            bits(result), result.range_.right, range_
        );
        return *this;
    }

    DynUfixed& operator%=(DynUfixed const& rhs) {
        auto result = *this % rhs;
        value_ = dyn_fixed_detail::rescale_unsigned_wrap(
            bits(result), result.range_.right, range_
        );
        return *this;
    }

  private:
    void require_same_range(DynUfixed const& rhs) const {
        if (range_ != rhs.range_) {
            throw std::invalid_argument("Ufixed comparison requires equal ranges");
        }
    }

    friend class DynSfixed;
    friend struct bits_fn;

    Range range_;
    DynUInt value_;
};

class DynSfixed {
  public:
    DynSfixed(Range range, DynSInt raw) : range_(range), value_(std::move(raw)) {
        dyn_fixed_detail::validate_range(range_);
        if (value_.width() != range_.length()) {
            throw std::invalid_argument("Raw Sfixed width does not match its range");
        }
    }

    explicit DynSfixed(BitVector&& source)
        : DynSfixed(source.range(), DynSInt(bits(std::move(source)))) {}

    Range range() const noexcept { return range_; }
    size_t size() const noexcept { return range_.length(); }
    size_t width() const noexcept { return size(); }

    std::string raw_decimal() const { return value_.to_decimal_string(true); }
    std::string raw_binary() const { return value_.to_binary_string(); }

    explicit operator bool() const noexcept { return value_.popcount() != 0; }
    explicit operator double() const noexcept {
        return dyn_fixed_detail::scaled_double(raw_decimal(), range_.right);
    }

    bool index(Range::value_type index) const {
        if (!contains(range_, index)) {
            throw std::out_of_range("Sfixed index out of bounds");
        }
        return value_.get_bit(dyn_fixed_detail::shift_distance(index, range_.right));
    }

    void set_index(Range::value_type index, bool bit) {
        if (!contains(range_, index)) {
            throw std::out_of_range("Sfixed index out of bounds");
        }
        value_.set_bit(dyn_fixed_detail::shift_distance(index, range_.right), bit);
    }

    bool operator==(DynSfixed const& rhs) const noexcept {
        return range_ == rhs.range_ && value_ == rhs.value_;
    }

    bool operator<(DynSfixed const& rhs) const {
        require_same_range(rhs);
        return value_ < rhs.value_;
    }
    bool operator<=(DynSfixed const& rhs) const {
        require_same_range(rhs);
        return value_ <= rhs.value_;
    }
    bool operator>(DynSfixed const& rhs) const {
        require_same_range(rhs);
        return value_ > rhs.value_;
    }
    bool operator>=(DynSfixed const& rhs) const {
        require_same_range(rhs);
        return value_ >= rhs.value_;
    }

    DynSfixed operator<<(size_t amount) const {
        return DynSfixed(range_, value_ << amount);
    }
    DynSfixed operator>>(size_t amount) const {
        return DynSfixed(range_, value_ >> amount);
    }
    DynSfixed& operator<<=(size_t amount) {
        value_ = value_ << amount;
        return *this;
    }
    DynSfixed& operator>>=(size_t amount) {
        value_ = value_ >> amount;
        return *this;
    }

    DynSfixed operator+() const { return *this; }

    DynSfixed operator-() const {
        Range result_range{
            dyn_fixed_detail::checked_add(range_.left, 1), Direction::DOWNTO, range_.right
        };
        return DynSfixed(result_range, -value_);
    }

    DynSfixed abs() const {
        Range result_range{
            dyn_fixed_detail::checked_add(range_.left, 1), Direction::DOWNTO, range_.right
        };
        auto magnitude = dyn_fixed_detail::signed_magnitude(value_);
        return DynSfixed(result_range, DynSInt(result_range.length(), magnitude));
    }

    DynSfixed operator+(DynSfixed const& rhs) const {
        Range result_range = dyn_fixed_detail::add_range(range_, rhs.range_);
        size_t lhs_shift =
            dyn_fixed_detail::shift_distance(range_.right, result_range.right);
        size_t rhs_shift =
            dyn_fixed_detail::shift_distance(rhs.range_.right, result_range.right);
        auto lhs = dyn_fixed_detail::align_signed(value_, result_range.length(), lhs_shift);
        auto rhs_value =
            dyn_fixed_detail::align_signed(rhs.value_, result_range.length(), rhs_shift);
        return DynSfixed(result_range, DynSInt(result_range.length(), lhs + rhs_value));
    }

    DynSfixed operator-(DynSfixed const& rhs) const {
        Range result_range = dyn_fixed_detail::add_range(range_, rhs.range_);
        size_t lhs_shift =
            dyn_fixed_detail::shift_distance(range_.right, result_range.right);
        size_t rhs_shift =
            dyn_fixed_detail::shift_distance(rhs.range_.right, result_range.right);
        auto lhs = dyn_fixed_detail::align_signed(value_, result_range.length(), lhs_shift);
        auto rhs_value =
            dyn_fixed_detail::align_signed(rhs.value_, result_range.length(), rhs_shift);
        return DynSfixed(result_range, DynSInt(result_range.length(), lhs - rhs_value));
    }

    DynSfixed operator*(DynSfixed const& rhs) const {
        Range result_range = dyn_fixed_detail::multiply_range(range_, rhs.range_);
        return DynSfixed(result_range, value_ * rhs.value_);
    }

    DynSfixed operator/(DynSfixed const& rhs) const {
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }
        Range result_range{
            dyn_fixed_detail::checked_add(
                dyn_fixed_detail::checked_sub(range_.left, rhs.range_.right), 1
            ),
            Direction::DOWNTO,
            dyn_fixed_detail::checked_sub(range_.right, rhs.range_.left)
        };
        bool negative = value_.is_negative() != rhs.value_.is_negative();
        auto lhs_magnitude = dyn_fixed_detail::signed_magnitude(value_);
        auto rhs_magnitude = dyn_fixed_detail::signed_magnitude(rhs.value_);
        size_t shift = rhs.size() - 1;
        size_t numerator_width = lhs_magnitude.width() + shift;
        auto numerator =
            dyn_fixed_detail::align_unsigned(lhs_magnitude, numerator_width, shift);
        auto quotient = dyn_fixed_detail::rounded_unsigned_quotient(
            numerator, rhs_magnitude, result_range.length()
        );
        return DynSfixed(
            result_range,
            dyn_fixed_detail::signed_from_magnitude(
                quotient, negative, result_range.length()
            )
        );
    }

    DynSfixed operator%(DynSfixed const& rhs) const {
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }
        Range::value_type right = std::min(range_.right, rhs.range_.right);
        size_t lhs_shift = dyn_fixed_detail::shift_distance(range_.right, right);
        size_t rhs_shift = dyn_fixed_detail::shift_distance(rhs.range_.right, right);
        size_t compute_width = std::max(size() + lhs_shift, rhs.size() + rhs_shift) + 1;
        auto lhs = dyn_fixed_detail::align_signed(value_, compute_width, lhs_shift);
        auto divisor = dyn_fixed_detail::align_signed(rhs.value_, compute_width, rhs_shift);
        auto remainder = lhs % divisor;
        Range result_range{
            std::min(range_.left, rhs.range_.left), Direction::DOWNTO, right
        };
        return DynSfixed(result_range, DynSInt(result_range.length(), remainder));
    }

    DynSfixed& operator+=(DynSfixed const& rhs) {
        auto result = *this + rhs;
        value_ = dyn_fixed_detail::rescale_signed_wrap(
            bits(result), result.range_.right, range_
        );
        return *this;
    }

    DynSfixed& operator-=(DynSfixed const& rhs) {
        auto result = *this - rhs;
        value_ = dyn_fixed_detail::rescale_signed_wrap(
            bits(result), result.range_.right, range_
        );
        return *this;
    }

    DynSfixed& operator*=(DynSfixed const& rhs) {
        auto result = *this * rhs;
        value_ = dyn_fixed_detail::rescale_signed_wrap(
            bits(result), result.range_.right, range_
        );
        return *this;
    }

    DynSfixed& operator/=(DynSfixed const& rhs) {
        auto result = *this / rhs;
        value_ = dyn_fixed_detail::rescale_signed_wrap(
            bits(result), result.range_.right, range_
        );
        return *this;
    }

    DynSfixed& operator%=(DynSfixed const& rhs) {
        auto result = *this % rhs;
        value_ = dyn_fixed_detail::rescale_signed_wrap(
            bits(result), result.range_.right, range_
        );
        return *this;
    }

  private:
    void require_same_range(DynSfixed const& rhs) const {
        if (range_ != rhs.range_) {
            throw std::invalid_argument("Sfixed comparison requires equal ranges");
        }
    }

    friend struct bits_fn;

    Range range_;
    DynSInt value_;
};

inline DynSfixed DynUfixed::operator+() const {
    Range result_range{
        dyn_fixed_detail::checked_add(range_.left, 1), Direction::DOWNTO, range_.right
    };
    return DynSfixed(result_range, DynSInt(result_range.length(), value_));
}

inline DynSfixed DynUfixed::operator-() const {
    Range result_range{
        dyn_fixed_detail::checked_add(range_.left, 1), Direction::DOWNTO, range_.right
    };
    return DynSfixed(result_range, -value_);
}

inline DynSfixed DynUfixed::operator-(DynUfixed const& rhs) const {
    Range result_range = dyn_fixed_detail::add_range(range_, rhs.range_);
    size_t lhs_shift = dyn_fixed_detail::shift_distance(range_.right, result_range.right);
    size_t rhs_shift =
        dyn_fixed_detail::shift_distance(rhs.range_.right, result_range.right);
    auto lhs = DynSInt(
        result_range.length(),
        dyn_fixed_detail::align_unsigned(value_, result_range.length(), lhs_shift)
    );
    auto rhs_value = DynSInt(
        result_range.length(),
        dyn_fixed_detail::align_unsigned(rhs.value_, result_range.length(), rhs_shift)
    );
    return DynSfixed(result_range, DynSInt(result_range.length(), lhs - rhs_value));
}

inline DynUfixed& DynUfixed::operator-=(DynUfixed const& rhs) {
    auto difference = *this - rhs;
    if (bits(difference).is_negative()) {
        throw std::overflow_error(
            "Compound Ufixed subtraction does not allow a negative result"
        );
    }
    value_ = dyn_fixed_detail::rescale_unsigned_wrap(
        DynUInt(bits(difference).width(), bits(difference)),
        difference.range().right,
        range_
    );
    return *this;
}

}  // namespace coconext::types::detail

#endif  // COCONEXT_DYN_FIXED_HPP
