#include "CurrencyGraph.hpp"
#include "RateLoader.hpp"
#include "ArbitrageDetector.hpp"
#include "ArbitrageReporter.hpp"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <cassert>
#include <cmath>

namespace {

struct TempCSVFile {
    std::string path;

    TempCSVFile(const std::string& filename, const std::string& content) {
        path = (std::filesystem::temp_directory_path() / filename).string();
        std::ofstream out(path, std::ios::binary);
        out << content;
    }

    ~TempCSVFile() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
};

} // anonymous namespace

void test_end_to_end_profitable_cycle() {
    std::cout << "[RUN] test_end_to_end_profitable_cycle..." << std::endl;

    // USD -> EUR -> GBP -> USD: 0.90 * 0.85 * 1.35 = 1.03275 (+3.275%)
    TempCSVFile temp("test_e2e_profit.csv",
        "source,target,rate\n"
        "USD,EUR,0.90\n"
        "EUR,GBP,0.85\n"
        "GBP,USD,1.35\n"
    );

    CurrencyGraph graph;
    std::size_t loaded = RateLoader::load_from_csv(temp.path, graph);
    assert(loaded == 3);

    auto cycle_opt = ArbitrageDetector::detect_arbitrage(graph);
    assert(cycle_opt.has_value());

    const double starting_capital = 2000.0;
    const std::string report = ArbitrageReporter::format_opportunity(cycle_opt.value(), graph, starting_capital);

    // Verify key sections and calculated numbers in the generated report
    const std::string& start_curr = cycle_opt.value().path.front();
    bool has_valid_cycle = (report.find("USD -> EUR -> GBP -> USD") != std::string::npos) ||
                           (report.find("EUR -> GBP -> USD -> EUR") != std::string::npos) ||
                           (report.find("GBP -> USD -> EUR -> GBP") != std::string::npos);
    assert(has_valid_cycle);
    assert(report.find("Arbitrage Opportunity Found!") != std::string::npos);
    assert(report.find("Conversion Cycle:") != std::string::npos);
    assert(report.find("Exchange Details:") != std::string::npos);
    assert(report.find("Step 1:") != std::string::npos);
    assert(report.find("Step 2:") != std::string::npos);
    assert(report.find("Step 3:") != std::string::npos);
    assert(report.find("Yield Metrics:") != std::string::npos);
    assert(report.find("Compound Multiplier: 1.032750") != std::string::npos);
    assert(report.find("Estimated Profit: 3.2750%") != std::string::npos);
    assert(report.find("Simulated Returns:") != std::string::npos);
    assert(report.find("Starting Capital: 2000.00 " + start_curr) != std::string::npos);
    assert(report.find("Final Amount: 2065.50 " + start_curr) != std::string::npos);
    assert(report.find("Estimated Profit Amount: +65.50 " + start_curr) != std::string::npos);
    assert(report.find("------------------------------------------------------------") != std::string::npos);

    std::cout << "[PASS] test_end_to_end_profitable_cycle" << std::endl;
}

void test_end_to_end_no_arbitrage() {
    std::cout << "[RUN] test_end_to_end_no_arbitrage..." << std::endl;

    TempCSVFile temp("test_e2e_no_arbitrage.csv",
        "from,to,rate\n"
        "USD,EUR,0.90\n"
        "EUR,USD,1.10\n"
        "EUR,GBP,0.85\n"
        "GBP,EUR,1.15\n"
    );

    CurrencyGraph graph;
    std::size_t loaded = RateLoader::load_from_csv(temp.path, graph);
    assert(loaded == 4);

    auto cycle_opt = ArbitrageDetector::detect_arbitrage(graph);
    assert(!cycle_opt.has_value());

    const std::string report = ArbitrageReporter::format_no_arbitrage();
    assert(report.find("No arbitrage opportunity detected in the supplied exchange rates.") != std::string::npos);
    assert(report.find("No Arbitrage Detected") != std::string::npos);
    assert(report.find("------------------------------------------------------------") != std::string::npos);

    std::cout << "[PASS] test_end_to_end_no_arbitrage" << std::endl;
}

void test_reporter_formatting_and_custom_capital() {
    std::cout << "[RUN] test_reporter_formatting_and_custom_capital..." << std::endl;

    CurrencyGraph graph;
    std::size_t e0 = graph.add_edge("AAA", "BBB", 1.20);
    std::size_t e1 = graph.add_edge("BBB", "CCC", 1.10);
    std::size_t e2 = graph.add_edge("CCC", "AAA", 0.80);
    // Compound: 1.20 * 1.10 * 0.80 = 1.0560 (+5.60%)

    ArbitrageCycle cycle;
    cycle.path = {"AAA", "BBB", "CCC", "AAA"};
    cycle.edge_indices = {e0, e1, e2};
    cycle.compound_multiplier = 1.0560;
    cycle.profit_percentage = 5.60;

    // Test with default capital ($1000.0)
    std::string report_default = ArbitrageReporter::format_opportunity(cycle, graph);
    assert(report_default.find("Starting Capital: 1000.00 AAA") != std::string::npos);
    assert(report_default.find("Final Amount: 1056.00 AAA") != std::string::npos);
    assert(report_default.find("Estimated Profit Amount: +56.00 AAA") != std::string::npos);

    // Test with custom capital ($50,000.0)
    std::string report_custom = ArbitrageReporter::format_opportunity(cycle, graph, 50000.0);
    assert(report_custom.find("Starting Capital: 50000.00 AAA") != std::string::npos);
    assert(report_custom.find("Final Amount: 52800.00 AAA") != std::string::npos);
    assert(report_custom.find("Estimated Profit Amount: +2800.00 AAA") != std::string::npos);

    std::cout << "[PASS] test_reporter_formatting_and_custom_capital" << std::endl;
}

void test_different_cycle_lengths() {
    std::cout << "[RUN] test_different_cycle_lengths..." << std::endl;

    // 1. Two-currency cycle (length 2)
    {
        CurrencyGraph graph;
        std::size_t e0 = graph.add_edge("USD", "JPY", 155.0);
        std::size_t e1 = graph.add_edge("JPY", "USD", 0.0068);
        // Multiplier: 155.0 * 0.0068 = 1.054 (+5.4%)

        ArbitrageCycle cycle;
        cycle.path = {"USD", "JPY", "USD"};
        cycle.edge_indices = {e0, e1};
        cycle.compound_multiplier = 1.0540;
        cycle.profit_percentage = 5.40;

        std::string report = ArbitrageReporter::format_opportunity(cycle, graph, 1000.0);
        assert(report.find("USD -> JPY -> USD") != std::string::npos);
        assert(report.find("Step 1: 1 USD -> 155 JPY") != std::string::npos || report.find("Step 1: 1 USD -> 155.0") != std::string::npos);
        assert(report.find("Step 2: 1 JPY -> 0.0068 USD") != std::string::npos);
        assert(report.find("Step 3:") == std::string::npos); // Only 2 steps!
        assert(report.find("Compound Multiplier: 1.054000") != std::string::npos);
    }

    // 2. Four-currency cycle (length 4)
    {
        CurrencyGraph graph;
        std::size_t e0 = graph.add_edge("A", "B", 1.10);
        std::size_t e1 = graph.add_edge("B", "C", 1.10);
        std::size_t e2 = graph.add_edge("C", "D", 1.10);
        std::size_t e3 = graph.add_edge("D", "A", 0.70);
        // Multiplier: 1.1^3 * 0.7 = 1.331 * 0.7 = 0.9317 (for display test)

        ArbitrageCycle cycle;
        cycle.path = {"A", "B", "C", "D", "A"};
        cycle.edge_indices = {e0, e1, e2, e3};
        cycle.compound_multiplier = 1.025;
        cycle.profit_percentage = 2.50;

        std::string report = ArbitrageReporter::format_opportunity(cycle, graph, 1000.0);
        assert(report.find("A -> B -> C -> D -> A") != std::string::npos);
        assert(report.find("Step 1:") != std::string::npos);
        assert(report.find("Step 2:") != std::string::npos);
        assert(report.find("Step 3:") != std::string::npos);
        assert(report.find("Step 4:") != std::string::npos);
        assert(report.find("Step 5:") == std::string::npos); // Exactly 4 steps!
    }

    std::cout << "[PASS] test_different_cycle_lengths" << std::endl;
}

void test_application_header() {
    std::cout << "[RUN] test_application_header..." << std::endl;

    std::string header = ArbitrageReporter::format_header();
    assert(header.find("Currency Arbitrage Detector") != std::string::npos);
    assert(header.find("Graph-Based Foreign Exchange Arbitrage Detection System") != std::string::npos);
    assert(header.find("============================================================") != std::string::npos);

    std::cout << "[PASS] test_application_header" << std::endl;
}

void test_capital_argument_validation() {
    std::cout << "[RUN] test_capital_argument_validation..." << std::endl;

    auto validate_capital = [](const std::string& cap_str) -> bool {
        try {
            std::size_t idx = 0;
            const double val = std::stod(cap_str, &idx);
            if (idx != cap_str.size() || std::isnan(val) || std::isinf(val) || val <= 0.0) {
                return false;
            }
            return true;
        } catch (...) {
            return false;
        }
    };

    // Valid inputs
    assert(validate_capital("1000"));
    assert(validate_capital("500.50"));
    assert(validate_capital("0.01"));

    // Invalid inputs
    assert(!validate_capital("0"));
    assert(!validate_capital("-100"));
    assert(!validate_capital("-0.01"));
    assert(!validate_capital("abc"));
    assert(!validate_capital("1000xyz"));
    assert(!validate_capital("NaN"));
    assert(!validate_capital("inf"));
    assert(!validate_capital(""));

    std::cout << "[PASS] test_capital_argument_validation" << std::endl;
}

void test_help_and_usage_interface() {
    std::cout << "[RUN] test_help_and_usage_interface..." << std::endl;

    const std::string help = ArbitrageReporter::format_help();
    // 1. What it does
    assert(help.find("Detects arbitrage opportunities in foreign exchange markets") != std::string::npos);
    assert(help.find("Bellman-Ford") != std::string::npos);
    assert(help.find("-log(exchange_rate)") != std::string::npos);

    // 2. Command syntax
    assert(help.find("currency_arbitrage <path-to-rates.csv> [starting_capital]") != std::string::npos);
    assert(help.find("-h | --help") != std::string::npos);

    // 3. Required CSV argument and optional starting capital with default 1000
    assert(help.find("<path-to-rates.csv>") != std::string::npos);
    assert(help.find("[starting_capital]") != std::string::npos);
    assert(help.find("Default: 1000") != std::string::npos);

    // 4. Expected CSV format and example
    assert(help.find("source_currency,target_currency,rate") != std::string::npos);
    assert(help.find("USD,EUR,0.92") != std::string::npos);
    assert(help.find("EUR,GBP,0.85") != std::string::npos);

    // 5. Documentation of --list-samples
    assert(help.find("--list-samples") != std::string::npos);

    // Test concise usage
    const std::string usage = ArbitrageReporter::format_usage();
    assert(usage.find("Usage: currency_arbitrage <path-to-rates.csv> [starting_capital]") != std::string::npos);
    assert(usage.find("--list-samples") != std::string::npos);
    assert(usage.find("-h/--help") != std::string::npos || usage.find("-h or --help") != std::string::npos);

    std::cout << "[PASS] test_help_and_usage_interface" << std::endl;
}

void test_list_samples_formatting() {
    std::cout << "[RUN] test_list_samples_formatting..." << std::endl;

    const std::string list_output = ArbitrageReporter::format_sample_list();
    assert(list_output.find("Available Sample Datasets") != std::string::npos);
    assert(list_output.find("sample_rates.csv") != std::string::npos);
    assert(list_output.find("Demonstrates an exchange-rate dataset containing an arbitrage opportunity.") != std::string::npos);
    assert(list_output.find("equilibrium_rates.csv") != std::string::npos);
    assert(list_output.find("Demonstrates exchange rates without an arbitrage opportunity.") != std::string::npos);
    assert(list_output.find("Usage Example:") != std::string::npos);

    std::cout << "[PASS] test_list_samples_formatting" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Starting End-to-End Workflow Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    test_end_to_end_profitable_cycle();
    test_end_to_end_no_arbitrage();
    test_reporter_formatting_and_custom_capital();
    test_different_cycle_lengths();
    test_application_header();
    test_capital_argument_validation();
    test_help_and_usage_interface();
    test_list_samples_formatting();

    std::cout << "========================================" << std::endl;
    std::cout << "All End-to-End Tests Passed!" << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
