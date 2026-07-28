#include <txui/core/Object.hpp>
#include <txui/core/Ref.hpp>
#include <txui/core/UUID.hpp>
#include <txui/core/Version.hpp>
#include <txui/core/Logger.hpp>
#include <iostream>

namespace {

class DummyNode : public txui::Object {
public:
    txui::UUID id{txui::UUID::generate()};

    DummyNode() noexcept {
        txui::log_message(txui::LogLevel::Info, "DummyNode created with UUID");
    }

    ~DummyNode() override {
        txui::log_message(txui::LogLevel::Info, "DummyNode destroyed");
    }
};

} // namespace

int main() {
    std::cout << "=== txui-demo-01-core (Version " << txui::TXUI_VERSION_STRING << ") ===" << std::endl;

    {
        auto node1 = txui::make_ref<DummyNode>();
        std::cout << "node1 UUID: " << node1->id.value() << ", ref_count=" << node1->ref_count() << std::endl;

        auto node2 = node1;
        std::cout << "after copy node2, ref_count=" << node1->ref_count() << std::endl;
    }

    std::cout << "=== core demo completed successfully ===" << std::endl;
    return 0;
}
