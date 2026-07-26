#include "settings/schema_validator.hpp"

namespace tinexus::settings {

bool SchemaValidator::validate_theme(const std::string& theme) {
    return theme == "dark" || theme == "light" || theme == "auto";
}

bool SchemaValidator::validate_scale(float scale) {
    return scale == 1.0f || scale == 1.25f || scale == 1.5f || scale == 2.0f;
}

bool SchemaValidator::validate_wallpaper_mode(const std::string& mode) {
    return mode == "fill" || mode == "fit" || mode == "stretch" || mode == "center";
}

} // namespace tinexus::settings
