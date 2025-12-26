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
            return '0'_l;
        case Logic::H:
            return '1'_l;
        default:
            throw std::invalid_argument("Logic value is not resolvable");
        }
    case ResolveMethod::WEAK:
        switch (value.value()) {
        case Logic::_0:
        case Logic::_1:
            return value;
        case Logic::L:
            return '0'_l;
        case Logic::H:
            return '1'_l;
        case Logic::W:
            return 'X'_l;
        default:
            return value;
        }
    case ResolveMethod::ZEROS:
        switch (value.value()) {
        case Logic::_0:
        case Logic::_1:
            return value;
        case Logic::L:
            return '0'_l;
        case Logic::H:
            return '1'_l;
        default:
            return '0'_l;
        }
    case ResolveMethod::ONES:
        switch (value.value()) {
        case Logic::_0:
        case Logic::_1:
            return value;
        case Logic::L:
            return '0'_l;
        case Logic::H:
            return '1'_l;
        default:
            return '1'_l;
        }
    case ResolveMethod::RANDOM: {
        switch (value.value()) {
        case Logic::_0:
        case Logic::_1:
            return value;
        case Logic::L:
            return '0'_l;
        case Logic::H:
            return '1'_l;
        default:
            return (get_rng()() % 2 == 0) ? '0'_l : '1'_l;
        }
    }
    default:
        throw std::invalid_argument("Unknown resolve method");
    }
}

}  // namespace coconext::types
