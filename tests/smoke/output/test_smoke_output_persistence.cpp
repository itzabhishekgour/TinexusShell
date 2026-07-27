#include <cassert>
#include <iostream>
#include "comp/output/output_layout.hpp"

using namespace tinexus::comp;

int main() {
    std::cout << "[+] Running smoke_output_persistence test suite..." << std::endl;

    auto& layout = OutputLayout::instance();
    layout.add_output(OutputSpec{"HDMI-A-1", 0, 0, 1920, 1080, 60, 1.0f, 0, true, true});
    layout.add_output(OutputSpec{"DP-1", 1920, 0, 2560, 1440, 144, 1.25f, 0, true, false});

    std::string toml = layout.serialize_toml(1);
    assert(toml.find("version = 1") != std::string::npos);
    assert(toml.find("name = \"HDMI-A-1\"") != std::string::npos);
    assert(toml.find("name = \"DP-1\"") != std::string::npos);

    assert(layout.deserialize_toml(toml) == true);

    std::cout << "[+] smoke_output_persistence: ALL ASSERTIONS PASSED (100%)" << std::endl;
    return 0;
}
