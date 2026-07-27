#include "pkg/package_manifest.hpp"
#include "common/logger.hpp"
#include <sstream>

namespace tinexus::pkg {

PackageManifest ManifestParser::parse_string(const std::string& manifest_content) {
    PackageManifest manifest;
    std::istringstream ss(manifest_content);
    std::string line;

    while (std::getline(ss, line)) {
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;

        std::string key = line.substr(0, colon);
        std::string val = line.substr(colon + 1);

        // Trim leading spaces
        while (!val.empty() && (val.front() == ' ' || val.front() == '\t')) val.erase(0, 1);

        if (key == "name") manifest.name = val;
        else if (key == "version") manifest.version = val;
        else if (key == "arch") manifest.architecture = val;
        else if (key == "sha256") manifest.sha256_checksum = val;
        else if (key == "signature") manifest.ed25519_signature = val;
        else if (key == "depends") manifest.dependencies.push_back(val);
        else if (key == "file") manifest.files.push_back(val);
    }

    log::info("ManifestParser: Parsed manifest for '{}-{}' (Files: {})", manifest.name, manifest.version, manifest.files.size());
    return manifest;
}

} // namespace tinexus::pkg
