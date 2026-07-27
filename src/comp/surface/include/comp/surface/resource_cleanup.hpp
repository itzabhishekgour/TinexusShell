#ifndef TINEXUS_COMP_RESOURCE_CLEANUP_HPP
#define TINEXUS_COMP_RESOURCE_CLEANUP_HPP

#include "comp/surface/surface_state.hpp"

namespace tinexus::comp {

class ResourceCleanup {
public:
    static bool destroy_surface_resources(SurfaceState& surface);
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_RESOURCE_CLEANUP_HPP
