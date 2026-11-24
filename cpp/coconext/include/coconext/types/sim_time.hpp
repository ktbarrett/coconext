#ifndef COCONEXT_TYPES_SIM_TIME_HPP
#define COCONEXT_TYPES_SIM_TIME_HPP

#include <chrono>
#include <cstdint>
#include <limits>
#include <ratio>
#include <stdexcept>

namespace coconext::detail {

inline constexpr auto max_time_literal{
    static_cast<unsigned long long>(std::numeric_limits<std::int64_t>::max())
};

}  // namespace coconext::detail

namespace coconext::types {

// femto for default sim time unit & smallest supported unit
using SimTime = std::chrono::duration<std::int64_t, std::femto>;
using femtoseconds = SimTime;
using picoseconds = std::chrono::duration<std::int64_t, std::pico>;
using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::chrono::nanoseconds;
using std::chrono::seconds;

}  // namespace coconext::types

namespace coconext::literals {

using namespace std::chrono_literals;

consteval coconext::types::picoseconds operator""_ps(unsigned long long ps) {
    if (ps > detail::max_time_literal) {
        throw std::out_of_range("ps value larger than std::int64_t::max()");
    }

    return coconext::types::picoseconds(static_cast<std::int64_t>(ps));
}

consteval std::chrono::duration<long double, std::pico> operator""_ps(long double ps) {
    return std::chrono::duration<long double, std::pico>(ps);
}

consteval coconext::types::SimTime operator""_fs(unsigned long long fs) {
    if (fs > detail::max_time_literal) {
        throw std::out_of_range("fs value larger than std::int64_t::max()");
    }

    return coconext::types::SimTime(static_cast<std::int64_t>(fs));
}

consteval std::chrono::duration<long double, std::femto> operator""_fs(long double fs) {
    return std::chrono::duration<long double, std::femto>(fs);
}

}  // namespace coconext::literals

#endif  // COCONEXT_TYPES_SIM_TIME_HPP
