#include <cassert>
#include <iostream>
#include <memory>
#include "comp/server/protocol_dispatcher.hpp"

using namespace tinexus::comp;

class TestProtocolModule : public ProtocolModule {
public:
    explicit TestProtocolModule(std::string mod_name) : m_name(std::move(mod_name)) {}
    std::string name() const override { return m_name; }
    bool register_globals(struct wl_display* /*display*/) override {
        m_initialized = true;
        return true;
    }
    void destroy() override {
        m_destroyed = true;
    }
    bool is_initialized() const noexcept { return m_initialized; }
    bool is_destroyed() const noexcept { return m_destroyed; }

private:
    std::string m_name;
    bool m_initialized{false};
    bool m_destroyed{false};
};

int main() {
    std::cout << "[+] Running smoke_protocol_dispatch test suite..." << std::endl;

    auto& dispatcher = ProtocolDispatcher::instance();
    auto mod1 = std::make_shared<TestProtocolModule>("zwlr_layer_shell_v1");
    auto mod2 = std::make_shared<TestProtocolModule>("xdg_shell");

    assert(dispatcher.register_module(mod1) == true);
    assert(dispatcher.register_module(mod2) == true);

    // Duplicate registration must fail
    assert(dispatcher.register_module(mod1) == false);

    assert(dispatcher.has_module("zwlr_layer_shell_v1") == true);
    assert(dispatcher.has_module("xdg_shell") == true);
    assert(dispatcher.has_module("non_existent_module") == false);

    assert(dispatcher.get_module("zwlr_layer_shell_v1") == mod1);

    // Init all modules
    assert(dispatcher.init_all(nullptr) == true);
    assert(mod1->is_initialized() == true);
    assert(mod2->is_initialized() == true);

    // Destroy all modules
    dispatcher.destroy_all();
    assert(mod1->is_destroyed() == true);
    assert(mod2->is_destroyed() == true);
    assert(dispatcher.registered_modules().empty());

    std::cout << "[+] smoke_protocol_dispatch: ALL ASSERTIONS PASSED (100%)" << std::endl;
    return 0;
}
