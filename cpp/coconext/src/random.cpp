#include <coconext/random.hpp>
#include <random>

// TODO should we share the same rng between everything in coconext?
static std::mt19937_64 rng;

std::mt19937_64& get_rng() { return rng; }

namespace coconext::random {

void seed(unsigned long seed) { rng.seed(seed); }

}  // namespace coconext::random
