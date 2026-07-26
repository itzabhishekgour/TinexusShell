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

int main(int argc, char** argv) {
    tinexus::log::set_component_name("tinexus-searchd");
    tinexus::log::info("Starting tinexus-searchd v{} - Search Engine & Ranking Pipeline Daemon", tinexus::VERSION_STRING);

    // Initialize Provider Manager and register built-in providers
    tinexus::searchd::ProviderManager manager;
    manager.register_provider(std::make_shared<tinexus::searchd::AppProvider>());
    manager.register_provider(std::make_shared<tinexus::searchd::CalculatorProvider>());
    manager.register_provider(std::make_shared<tinexus::searchd::SystemProvider>());

    tinexus::searchd::SearchCache cache(512);
    tinexus::searchd::RankingPipeline ranker;

    tinexus::log::info("Search Engine Daemon ready. Multi-stage pipeline active.");

    // Demonstration run for verification
    std::vector<std::string> test_queries = {"firefox", "calc: 15 * 4", "sys: lock", "chrome"};
    for (const auto& q : test_queries) {
        tinexus::searchd::SearchSession session(q);

        // Stage 1: Normalize
        auto norm = tinexus::searchd::QueryNormalizer::instance().normalize(q);
        session.mark_stage("normalize");

        // Stage 2: Intent Detection
        auto intent = tinexus::searchd::IntentDetector::instance().classify(norm);
        session.mark_stage("intent");

        // Stage 3: Cache Check
        uint64_t gen = tinexus::indexer::RamSnapshot::instance().generation();
        auto cached = cache.get(norm.clean_query, gen);

        std::vector<tinexus::searchd::SearchResult> results;
        if (cached) {
            session.set_cache_hit(true);
            results = *cached;
        } else {
            session.set_cache_hit(false);
            // Stage 4: Provider Fan-out
            results = manager.execute_search(norm, 6, session);
            session.mark_stage("provider_fanout");

            // Stage 5: Multi-stage Ranking
            ranker.rank(norm, results);
            session.mark_stage("ranking");

            // Top-6 Cutoff
            if (results.size() > 6) {
                results.resize(6);
            }

            cache.put(norm.clean_query, gen, results);
        }

        // Stage 6: IPC Payload Batching
        auto payload = tinexus::searchd::WireSerializer::serialize_batch(results, session.total_latency_us());
        session.mark_stage("serialization");

        session.complete();
        tinexus::log::info("Search Query: '{}' -> {} results in {} us", q, results.size(), session.total_latency_us());
    }

    return 0;
}
