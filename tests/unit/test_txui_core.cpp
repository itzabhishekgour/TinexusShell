#include <txui/core/Object.hpp>
#include <txui/core/Ref.hpp>
#include <txui/core/UUID.hpp>
#include <txui/core/Version.hpp>
#include <txui/core/Assert.hpp>
#include <iostream>
#include <cstdlib>

namespace {

class TestNode : public txui::Object {
public:
    int value{42};
};

} // namespace

int main() {
    auto node1 = txui::make_ref<TestNode>();
    if (node1->ref_count() != 1) {
        std::cerr << "FAIL: node1 ref_count should be 1" << std::endl;
        return EXIT_FAILURE;
    }

    {
        auto node2 = node1;
        if (node1->ref_count() != 2) {
            std::cerr << "FAIL: node1 ref_count should be 2 after copy" << std::endl;
            return EXIT_FAILURE;
        }
    }

    if (node1->ref_count() != 1) {
        std::cerr << "FAIL: node1 ref_count should be 1 after scope exit" << std::endl;
        return EXIT_FAILURE;
    }

    txui::UUID id1 = txui::UUID::generate();
    txui::UUID id2 = txui::UUID::generate();
    if (id1 == id2 || !id1.is_valid()) {
        std::cerr << "FAIL: UUID generation error" << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "PASS: test_txui_core passed successfully" << std::endl;
    return EXIT_SUCCESS;
}
