#include "common/logger.hpp"
#include "common/version.hpp"

int main(int argc, char** argv) {
    tinexus::log::set_component_name("tinexus-wallpaper");
    tinexus::log::info("Starting tinexus-wallpaper v{}", tinexus::VERSION_STRING);
    return 0;
}
