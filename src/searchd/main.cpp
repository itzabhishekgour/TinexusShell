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
#include "ipc_server.hpp"
#include "indexer/ram_snapshot.hpp"
#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>
#include <atomic>

namespace {
std::atomic<bool> g_running{true};

void signal_handler(int signal) {
    tinexus::log::info("[searchd] Signal {} received — shutting down cleanly", signal);
    g_running = false;
}
} // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    tinexus::log::set_component_name("tinexus-searchd");
    tinexus::log::info("Starting tinexus-searchd v{} — Search Engine & Ranking Pipeline Daemon",
                       tinexus::VERSION_STRING);

    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGPIPE, SIG_IGN);

    // -----------------------------------------------------------------------
    // Initialize the multi-provider search engine
    // -----------------------------------------------------------------------
    tinexus::searchd::ProviderManager manager;
    manager.register_provider(std::make_shared<tinexus::searchd::AppProvider>());
    manager.register_provider(std::make_shared<tinexus::searchd::CalculatorProvider>());
    manager.register_provider(std::make_shared<tinexus::searchd::SystemProvider>());

    tinexus::searchd::SearchCache   cache(512);   // LRU cache — 512 entries
    tinexus::searchd::RankingPipeline ranker;

    tinexus::log::info("[searchd] {} search providers registered", 3);
    tinexus::log::info("[searchd] Cache size: 512 entries | Perf target: ≤30ms T2 hardware");

    // -----------------------------------------------------------------------
    // Connect to tinexus-ipcd and register as "searchd"
    // IpcServer starts a background thread that handles SEARCH_QUERY messages
    // from the launcher through the ipcd broker.
    // -----------------------------------------------------------------------
    tinexus::searchd::IpcServer ipc_server(manager, ranker, cache);
    bool ipc_connected = ipc_server.start();

    if (ipc_connected) {
        tinexus::log::info("[searchd] IPC bridge active — ready to serve launcher queries");
    } else {
        tinexus::log::warn("[searchd] Running WITHOUT IPC (ipcd not available) — "
                           "queries will not be served until ipcd starts");
    }

    tinexus::log::info("[searchd] Daemon ready. D-Bus name: io.tinexus.shell.Search1");

    // -----------------------------------------------------------------------
    // Main thread idle loop — signal handler sets g_running = false
    // -----------------------------------------------------------------------
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    tinexus::log::info("[searchd] Shutting down...");
    ipc_server.stop();
    tinexus::log::info("[searchd] tinexus-searchd shutdown complete.");
    return 0;
}

