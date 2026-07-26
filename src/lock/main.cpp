#include "common/logger.hpp"
#include "common/version.hpp"

int main(int argc, char** argv) {
    tinexus::log::set_component_name("tinexus-lock");
    tinexus::log::info("Starting tinexus-lock v{}", tinexus::VERSION_STRING);
    return 0;
}
