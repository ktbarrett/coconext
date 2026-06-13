#include <coconext/types/logic.hpp>
#include <random>
#include <stdexcept>
#include <string>

#include "./random.hpp"

using namespace coconext::types;

namespace coconext::types {

namespace {

// Process-wide default. Single-threaded mutation is the expected usage pattern
// (set once per test or regression); not synchronized.
ResolveMethod g_default_resolve_method = ResolveMethod::WEAK;

}  // namespace

ResolveMethod get_default_resolve_method() noexcept { return g_default_resolve_method; }

void set_default_resolve_method(ResolveMethod method) noexcept {
    g_default_resolve_method = method;
}

bool Logic::is_resolvable(ResolveMethod method) const noexcept {
    switch (method) {
    case ResolveMethod::ERROR:
        return value_ == _0 || value_ == _1;
    case ResolveMethod::WEAK:
        return value_ == _0 || value_ == _1 || value_ == L || value_ == H;
    case ResolveMethod::ZEROS:
    case ResolveMethod::ONES:
    case ResolveMethod::RANDOM:
        return true;
    }
    return false;
}

Bit Logic::resolve(ResolveMethod method) const {
    switch (method) {
    case ResolveMethod::ERROR:
        switch (value_) {
        case _0:
            return Bit::_0;
        case _1:
            return Bit::_1;
        default:
            throw std::invalid_argument(
                "Logic value '" + std::string(to_string(*this))
                + "' is not resolvable under ERROR"
            );
        }
    case ResolveMethod::WEAK:
        switch (value_) {
        case _0:
        case L:
            return Bit::_0;
        case _1:
        case H:
            return Bit::_1;
        default:
            throw std::invalid_argument(
                "Logic value '" + std::string(to_string(*this))
                + "' is not resolvable under WEAK"
            );
        }
    case ResolveMethod::ZEROS:
        switch (value_) {
        case _0:
        case L:
            return Bit::_0;
        case _1:
        case H:
            return Bit::_1;
        default:
            return Bit::_0;
        }
    case ResolveMethod::ONES:
        switch (value_) {
        case _0:
        case L:
            return Bit::_0;
        case _1:
        case H:
            return Bit::_1;
        default:
            return Bit::_1;
        }
    case ResolveMethod::RANDOM: {
        switch (value_) {
        case _0:
        case L:
            return Bit::_0;
        case _1:
        case H:
            return Bit::_1;
        default: {
            auto& rng = get_rng();
            return (rng() % 2 == 0) ? Bit::_0 : Bit::_1;
        }
        }
    }
    }
    throw std::invalid_argument("Unknown resolve method");
}

}  // namespace coconext::types
