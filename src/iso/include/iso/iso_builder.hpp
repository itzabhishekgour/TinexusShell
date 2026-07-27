#ifndef TINEXUS_ISO_ISO_BUILDER_HPP
#define TINEXUS_ISO_ISO_BUILDER_HPP

#include <string>
#include <vector>

namespace tinexus::iso {

class IsoBuilder {
public:
    IsoBuilder() = default;
    ~IsoBuilder() = default;

    bool build_iso(const std::string& output_iso = "Tinexus-0.1.0-alpha.iso", bool dry_run = false);
};

} // namespace tinexus::iso

#endif // TINEXUS_ISO_ISO_BUILDER_HPP
