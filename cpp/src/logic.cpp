#include <coconext/types/logic.hpp>
#include <random>
#include <stdexcept>

#include "./random.hpp"

using namespace coconext::types;

namespace coconext::types {

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

Logic Logic::resolve(ResolveMethod method) const {
    switch (method) {
    case ResolveMethod::ERROR:
        switch (value_) {
        case _0:
        case _1:
            return *this;
        default:
            throw std::invalid_argument("Logic value is not resolvable");
        }
    case ResolveMethod::WEAK:
        switch (value_) {
        case _0:
        case _1:
            return *this;
        case L:
            return _0;
        case H:
            return _1;
        default:
            throw std::invalid_argument("Logic value is not resolvable under WEAK");
        }
    case ResolveMethod::ZEROS:
        switch (value_) {
        case _0:
        case _1:
            return *this;
        case L:
            return _0;
        case H:
            return _1;
        default:
            return _0;
        }
    case ResolveMethod::ONES:
        switch (value_) {
        case _0:
        case _1:
            return *this;
        case L:
            return _0;
        case H:
            return _1;
        default:
            return _1;
        }
    case ResolveMethod::RANDOM: {
        switch (value_) {
        case _0:
        case _1:
            return *this;
        case L:
            return _0;
        case H:
            return _1;
        default: {
            auto& rng = get_rng();
            return (rng() % 2 == 0) ? _0 : _1;
        }
        }
    }
    default:
        throw std::invalid_argument("Unknown resolve method");
    }
}

}  // namespace coconext::types
