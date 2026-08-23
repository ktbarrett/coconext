#include <coconext/sim_objects.hpp>
#include <gpi.h>

namespace coconext::sim_objects {

bool has_registered_impl() noexcept { return ::gpi_has_registered_impl(); }

}  // namespace coconext::sim_objects
