#ifndef TINEXUS_SETTINGS_SCHEMA_VALIDATOR_HPP
#define TINEXUS_SETTINGS_SCHEMA_VALIDATOR_HPP

#include <string>
#include <vector>

namespace tinexus::settings {

class SchemaValidator {
public:
    static bool validate_theme(const std::string& theme);
    static bool validate_scale(float scale);
    static bool validate_wallpaper_mode(const std::string& mode);
};

} // namespace tinexus::settings

#endif // TINEXUS_SETTINGS_SCHEMA_VALIDATOR_HPP
