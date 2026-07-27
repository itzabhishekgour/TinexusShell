#include <cassert>
#include <iostream>
#include "session/autostart_parser.hpp"

using namespace tinexus::session;

int main() {
    std::cout << "[+] Running smoke_autostart_order test suite..." << std::endl;

    AutostartEntry e1{"App1", "app1", "Tinexus", "", "", false};
    AutostartEntry e2{"App2", "app2", "", "KDE", "", false};
    AutostartEntry e3{"App3", "app3", "", "", "", true}; // Hidden

    assert(e1.should_autostart("Tinexus") == true);
    assert(e1.should_autostart("GNOME") == false);

    assert(e2.should_autostart("Tinexus") == true);
    assert(e2.should_autostart("KDE") == false);

    assert(e3.should_autostart("Tinexus") == false);

    std::cout << "[+] smoke_autostart_order: ALL ASSERTIONS PASSED (100%)" << std::endl;
    return 0;
}
