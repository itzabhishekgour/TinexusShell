#ifndef TINEXUS_RELEASE_RELEASE_PIPELINE_HPP
#define TINEXUS_RELEASE_RELEASE_PIPELINE_HPP

#include <string>

namespace tinexus::release {

class ReleasePipeline {
public:
    ReleasePipeline() = default;
    ~ReleasePipeline() = default;

    bool run_release_pipeline(const std::string& version = "v0.1.0-alpha", bool dry_run = false);
};

} // namespace tinexus::release

#endif // TINEXUS_RELEASE_RELEASE_PIPELINE_HPP
