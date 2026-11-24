#ifndef COCONEXT_TYPES_SIM_TIME_HPP
#define COCONEXT_TYPES_SIM_TIME_HPP

#include <chrono>
#include <cstdint>
#include <ratio>

namespace coconext::types {

using SimTime = std::chrono::duration<int64_t, std::femto>; // default bc smallest supported unit
using femtoseconds = SimTime;
using picoseconds = std::chrono::duration<int64_t, std::pico>;

} // namespace coconext::types

namespace coconext::literals {

constexpr coconext::types::picoseconds operator""_ps(unsigned long long ps)
{
    return coconext::types::picoseconds(static_cast<int64_t>(ps));
}

constexpr std::chrono::duration<long double, std::pico> operator""_ps(long double ps)
{
    return std::chrono::duration<long double, std::pico>(ps);
}

constexpr coconext::types::SimTime operator""_fs(unsigned long long fs)
{
    return coconext::types::SimTime(static_cast<int64_t>(fs));
}

constexpr std::chrono::duration<long double, std::femto> operator""_fs(long double fs)
{
    return std::chrono::duration<long double, std::femto>(fs);
}

} // namespace coconext::literals

// formatting 

#endif // COCONEXT_TYPES_SIM_TIME_HPP
