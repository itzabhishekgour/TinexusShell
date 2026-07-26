#include "common/logger.hpp"
#include "common/version.hpp"

int main(int argc, char** argv) {
    tinexus::log::set_component_name("tinexus-notifications");
    tinexus::log::info("Starting tinexus-notifications v{}", tinexus::VERSION_STRING);
    return 0;
}
