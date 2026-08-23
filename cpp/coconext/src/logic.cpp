#include <coconext/types/logic.hpp>
#include <optional>
#include <random>

#include "./random.hpp"

using namespace coconext::types;

namespace coconext::types {

std::optional<Bit> Logic::resolve(ResolveMethod method) const noexcept {
    switch (method) {
    case ResolveMethod::ERROR:
        switch (value_) {
        case _0:
            return Bit::_0;
        case _1:
            return Bit::_1;
        default:
            return std::nullopt;
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
            return std::nullopt;
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
    case ResolveMethod::RANDOM:
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
    return std::nullopt;
}

}  // namespace coconext::types
