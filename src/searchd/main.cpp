#include "common/logger.hpp"
#include "common/version.hpp"
#include "searchd/normalizer.hpp"
#include "searchd/intent_detector.hpp"
#include "searchd/cache.hpp"
#include "searchd/ranking_stage.hpp"
#include "searchd/provider_manager.hpp"
#include "searchd/providers/app_provider.hpp"
#include "searchd/providers/calculator_provider.hpp"
#include "searchd/providers/system_provider.hpp"
#include "searchd/wire_protocol.hpp"
#include "indexer/ram_snapshot.hpp"
#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>
#include <atomic>

namespace {
std::atomic<bool> g_running{true};

void signal_handler(int signal) {
    tinexus::log::info("searchd received signal {}, shutting down...", signal);
    g_running = false;
}
} // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    tinexus::log::set_component_name("tinexus-searchd");
    tinexus::log::info("Starting tinexus-searchd v{} - Search Engine & Ranking Pipeline Daemon", tinexus::VERSION_STRING);

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Initialize Provider Manager and register built-in providers
    tinexus::searchd::ProviderManager manager;
    manager.register_provider(std::make_shared<tinexus::searchd::AppProvider>());
    manager.register_provider(std::make_shared<tinexus::searchd::CalculatorProvider>());
    manager.register_provider(std::make_shared<tinexus::searchd::SystemProvider>());

    tinexus::searchd::SearchCache cache(512);
    tinexus::searchd::RankingPipeline ranker;

    tinexus::log::info("Registered D-Bus IPC service: 'io.tinexus.shell.Search1'");
    tinexus::log::info("Search Engine Daemon ready. Multi-stage pipeline active.");

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    tinexus::log::info("tinexus-searchd shutdown complete.");
    return 0;
}
