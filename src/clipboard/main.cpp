#include "common/logger.hpp"
#include "common/version.hpp"

int main(int argc, char** argv) {
    tinexus::log::set_component_name("tinexus-clipboard");
    tinexus::log::info("Starting tinexus-clipboard v{}", tinexus::VERSION_STRING);
    return 0;
}
