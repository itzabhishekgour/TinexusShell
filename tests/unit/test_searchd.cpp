#include <iostream>
#include <cassert>
#include <cmath>
#include "common/logger.hpp"
#include "indexer/desktop_entry.hpp"
#include "indexer/ram_snapshot.hpp"
#include "searchd/normalizer.hpp"
#include "searchd/intent_detector.hpp"
#include "searchd/ranking_stage.hpp"
#include "searchd/providers/calculator_provider.hpp"
#include "searchd/providers/system_provider.hpp"
#include "searchd/provider_manager.hpp"
#include "searchd/session.hpp"

void test_desktop_parser_sanitize() {
    std::string raw_exec = "firefox %u %F --new-window";
    std::string sanitized = tinexus::indexer::DesktopParser::sanitize_exec(raw_exec);
    assert(sanitized == "firefox   --new-window" || sanitized == "firefox --new-window");
    std::cout << "[PASS] test_desktop_parser_sanitize\n";
}

void test_query_normalizer() {
    auto norm = tinexus::searchd::QueryNormalizer::instance().normalize("  calc: 15 * 4  ");
    assert(norm.has_prefix == true);
    assert(norm.prefix_tag == "calc");
    assert(norm.clean_query == "15 * 4");

    auto alias_norm = tinexus::searchd::QueryNormalizer::instance().normalize("chrome");
    assert(alias_norm.expanded_query == "google chrome");
    std::cout << "[PASS] test_query_normalizer\n";
}

void test_intent_detector() {
    auto norm = tinexus::searchd::QueryNormalizer::instance().normalize("2 + 2");
    auto intent = tinexus::searchd::IntentDetector::instance().classify(norm);
    assert(intent.primary == tinexus::searchd::PrimaryIntent::Calculator);
    assert(intent.confidences["calc"] > 0.9f);
    std::cout << "[PASS] test_intent_detector\n";
}

void test_calculator_provider() {
    auto val = tinexus::searchd::CalculatorProvider::evaluate("15 * 4");
    assert(val.has_value());
    assert(std::abs(*val - 60.0) < 0.0001);

    auto val_div = tinexus::searchd::CalculatorProvider::evaluate("100 / 4");
    assert(val_div.has_value());
    assert(std::abs(*val_div - 25.0) < 0.0001);

    std::cout << "[PASS] test_calculator_provider\n";
}

void test_trigram_ranking() {
    float score1 = tinexus::searchd::RankingPipeline::calculate_trigram_score("firefox", "firefox web browser");
    assert(score1 > 0.4f);

    int lev = tinexus::searchd::RankingPipeline::calculate_levenshtein_distance("chrome", "chrom");
    assert(lev == 1);
    std::cout << "[PASS] test_trigram_ranking\n";
}

void test_search_pipeline() {
    tinexus::searchd::ProviderManager manager;
    manager.register_provider(std::make_shared<tinexus::searchd::CalculatorProvider>());
    manager.register_provider(std::make_shared<tinexus::searchd::SystemProvider>());

    tinexus::searchd::SearchSession session("sys: lock");
    auto norm = tinexus::searchd::QueryNormalizer::instance().normalize("sys: lock");
    auto results = manager.execute_search(norm, 6, session);

    assert(!results.empty());
    assert(results[0].title == "Lock Screen");

    session.complete();
    assert(session.total_latency_us() < 50000); // Must be < 50ms in test environment
    std::cout << "[PASS] test_search_pipeline\n";
}

int main() {
    tinexus::log::set_component_name("unit_test_searchd");
    tinexus::log::info("Running unit test suite for tinexus-searchd and indexer...");

    test_desktop_parser_sanitize();
    test_query_normalizer();
    test_intent_detector();
    test_calculator_provider();
    test_trigram_ranking();
    test_search_pipeline();

    tinexus::log::info("All searchd unit tests passed successfully!");
    return 0;
}
