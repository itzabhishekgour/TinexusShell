#include "../../src/app-installer/StoreWidget.hpp"
#include <txui/render/Canvas.hpp>
#include <txui/render/PixmanBackend.hpp>
#include <txui/render/Painter.hpp>
#include <txui/render/ImageWriter.hpp>
#include <iostream>
#include <cassert>

using namespace txui;
using namespace tinexus::store;

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << " TxUI App Store Visual Verification Harness       " << std::endl;
    std::cout << "==================================================" << std::endl;

    auto store = make_ref<StoreWidget>();
    assert(!store->apps().empty());
    std::cout << "[PASS] Catalog initialized with " << store->apps().size() << " applications." << std::endl;

    // Test Category Filtering
    store->set_category("Utilities");
    auto util_apps = store->filtered_apps();
    assert(!util_apps.empty());
    bool found_ksnip = false;
    for (const auto* app : util_apps) {
        if (app->id == "org.ksnip.Ksnip") found_ksnip = true;
    }
    assert(found_ksnip);
    (void)found_ksnip;
    std::cout << "[PASS] Category 'Utilities' correctly filters and contains Ksnip." << std::endl;

    // Test Search Filtering
    store->set_search_query("ksnip");
    auto search_results = store->filtered_apps();
    assert(search_results.size() >= 1);
    assert(search_results[0]->id == "org.ksnip.Ksnip");
    std::cout << "[PASS] Search 'ksnip' correctly isolates Ksnip." << std::endl;

    // Test Render & Layout Passes
    store->set_category("Discover");
    store->set_search_query("");
    const uint32_t W = 960, H = 640;
    Canvas canvas(W, H);
    canvas.clear(Color(13, 14, 18, 255));
    PixmanBackend backend;

    Constraints constraints(0, W, 0, H);
    store->measure(constraints);
    store->layout(Rect(0, 0, W, H));

    CommandBuffer cmds;
    Painter painter(cmds);
    painter.begin_frame();
    store->paint(painter);
    painter.end_frame();
    backend.execute(cmds, canvas);
    bool saved = ImageWriter::save_png(canvas, "build/app_store_view.png");
    (void)saved;
    std::cout << "[PASS] Headless layout and paint passes executed successfully." << std::endl;

    // Test Trigger Install State Transition
    store->trigger_install("org.ksnip.Ksnip");
    bool is_installing = false;
    for (const auto& a : store->apps()) {
        if (a.id == "org.ksnip.Ksnip") {
            if (a.state == InstallState::Installing) is_installing = true;
            break;
        }
    }
    assert(is_installing);
    (void)is_installing;
    std::cout << "[PASS] Triggering install on Ksnip correctly transitions state to Installing." << std::endl;

    std::cout << "All Tinexus App Store verification tests passed!" << std::endl;
    return 0;
}
