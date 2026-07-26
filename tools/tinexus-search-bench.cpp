#include <iostream>
#include <vector>
#include <chrono>
#include <numeric>
#include <algorithm>
#include "common/logger.hpp"
#include "indexer/desktop_entry.hpp"
#include "indexer/ram_snapshot.hpp"
#include "searchd/normalizer.hpp"
#include "searchd/intent_detector.hpp"
#include "searchd/ranking_stage.hpp"
#include "searchd/providers/app_provider.hpp"
#include "searchd/provider_manager.hpp"
#include "searchd/session.hpp"

void benchmark_dataset(size_t item_count, double sla_target_ms) {
    std::cout << "\n----------------------------------------\n";
    std::cout << "Benchmarking Dataset Size: " << item_count << " items\n";

    std::vector<tinexus::indexer::DesktopEntry> synthetic_entries;
    synthetic_entries.reserve(item_count);

    for (size_t i = 0; i < item_count; ++i) {
        tinexus::indexer::DesktopEntry entry;
        entry.desktop_id = "app_org_example_app_" + std::to_string(i);
        entry.name = "Synthetic App " + std::to_string(i) + (i % 5 == 0 ? " Web Browser" : " Text Editor");
        entry.generic_name = "Utility Tool";
        entry.exec = "example-app-" + std::to_string(i);
        entry.icon = "application-x-executable";
        synthetic_entries.push_back(std::move(entry));
    }

    tinexus::indexer::RamSnapshot::instance().set_all_entries(synthetic_entries);

    tinexus::searchd::ProviderManager manager;
    manager.register_provider(std::make_shared<tinexus::searchd::AppProvider>());
    tinexus::searchd::RankingPipeline ranker;

    std::vector<std::string> test_queries = {"synth", "web", "editor", "app_100", "nonexistent"};
    std::vector<double> latencies_ms;

    const size_t iterations = 100;
    for (size_t iter = 0; iter < iterations; ++iter) {
        const auto& q = test_queries[iter % test_queries.size()];
        tinexus::searchd::SearchSession session(q);

        auto start = std::chrono::high_resolution_clock::now();
        auto norm = tinexus::searchd::QueryNormalizer::instance().normalize(q);
        auto results = manager.execute_search(norm, 6, session);
        ranker.rank(norm, results);
        if (results.size() > 6) results.resize(6);
        auto end = std::chrono::high_resolution_clock::now();

        double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
        latencies_ms.push_back(elapsed_ms);
    }

    std::sort(latencies_ms.begin(), latencies_ms.end());
    double p50 = latencies_ms[latencies_ms.size() / 2];
    size_t idx95 = static_cast<size_t>(static_cast<double>(latencies_ms.size()) * 0.95);
    double p95 = latencies_ms[idx95];
    double avg = std::accumulate(latencies_ms.begin(), latencies_ms.end(), 0.0) / static_cast<double>(latencies_ms.size());

    std::cout << "Average Latency: " << avg << " ms\n";
    std::cout << "P50 Latency:     " << p50 << " ms\n";
    std::cout << "P95 Latency:     " << p95 << " ms (Target: < " << sla_target_ms << " ms)\n";

    if (p95 <= sla_target_ms) {
        std::cout << "[PASSED SLA TARGET]\n";
    } else {
        std::cout << "[WARNING: EXCEEDED SLA TARGET]\n";
    }
}

int main() {
    tinexus::log::set_component_name("tinexus-search-bench");
    tinexus::log::info("Starting Tinexus Search Engine Synthetic Benchmark Suite...");

    benchmark_dataset(100, 1.0);
    benchmark_dataset(1000, 2.0);
    benchmark_dataset(5000, 5.0);
    benchmark_dataset(10000, 8.0);

    return 0;
}
