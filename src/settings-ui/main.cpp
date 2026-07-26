#include "common/logger.hpp"
#include "common/version.hpp"

int main(int argc, char** argv) {
    tinexus::log::set_component_name("tinexus-settings-ui");
    tinexus::log::info("Starting tinexus-settings-ui v{}", tinexus::VERSION_STRING);
    return 0;
}
