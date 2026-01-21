#include <coconext/random.hpp>
#include <coconext/types/logic.hpp>
#include <random>

#include "./random.hpp"

using namespace coconext::types;

namespace coconext::types {

Logic resolve(const Logic& value, ResolveMethod method) {
    switch (method) {
    case ResolveMethod::ERROR:
        switch (value.value()) {
        case Logic::_0:
        case Logic::_1:
            return value;
        case Logic::L:
            return Logic::_0;
        case Logic::H:
            return Logic::_1;
        default:
            throw std::invalid_argument("Logic value is not resolvable");
        }
    case ResolveMethod::WEAK:
        switch (value.value()) {
        case Logic::_0:
        case Logic::_1:
            return value;
        case Logic::L:
            return Logic::_0;
        case Logic::H:
            return Logic::_1;
        case Logic::W:
            return Logic::X;
        default:
            return value;
        }
    case ResolveMethod::ZEROS:
        switch (value.value()) {
        case Logic::_0:
        case Logic::_1:
            return value;
        case Logic::L:
            return Logic::_0;
        case Logic::H:
            return Logic::_1;
        default:
            return Logic::_0;
        }
    case ResolveMethod::ONES:
        switch (value.value()) {
        case Logic::_0:
        case Logic::_1:
            return value;
        case Logic::L:
            return Logic::_0;
        case Logic::H:
            return Logic::_1;
        default:
            return Logic::_1;
        }
    case ResolveMethod::RANDOM: {
        switch (value.value()) {
        case Logic::_0:
        case Logic::_1:
            return value;
        case Logic::L:
            return Logic::_0;
        case Logic::H:
            return Logic::_1;
        default:
            auto& rng = get_rng();
            return (rng() % 2 == 0) ? Logic::_0 : Logic::_1;
        }
    }
    default:
        throw std::invalid_argument("Unknown resolve method");
    }
}

}  // namespace coconext::types
