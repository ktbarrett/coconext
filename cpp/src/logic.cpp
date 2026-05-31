#include <coconext/random.hpp>
#include <coconext/types/logic.hpp>
#include <random>

#include "./random.hpp"

using namespace coconext::types;

namespace coconext::types {

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
        case W:
            return X;
        default:
            return *this;
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
