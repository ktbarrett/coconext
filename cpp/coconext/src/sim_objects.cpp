#include <gpi.h>

#include <coconext/sim_objects.hpp>

namespace coconext::sim_objects {

bool has_registered_impl() noexcept { return ::gpi_has_registered_impl(); }

}  // namespace coconext::sim_objects
