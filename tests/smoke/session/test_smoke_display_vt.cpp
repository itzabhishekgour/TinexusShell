#include <cassert>
#include <iostream>
#include "displayd/vt_manager.hpp"

using namespace tinexus::displayd;

int main() {
    std::cout << "[+] Running smoke_display_vt test suite..." << std::endl;

    auto& vtm = VTManager::instance();
    int32_t free_vt = vtm.find_free_vt();
    assert(free_vt == 7);

    assert(vtm.activate_vt(free_vt) == true);
    assert(vtm.current_vt() == 7);
    assert(vtm.release_vt(free_vt) == true);

    std::cout << "[+] smoke_display_vt: ALL ASSERTIONS PASSED (100%)" << std::endl;
    return 0;
}
