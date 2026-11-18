#include "CurrencyGraph.hpp"
#include "RateLoader.hpp"
#include "ArbitrageDetector.hpp"
#include "ArbitrageReporter.hpp"

#include <iostream>
#include <string>
#include <cmath>

int main(int argc, char* argv[]) {
    std::cout << ArbitrageReporter::format_header() << std::endl;

    // No arguments provided: show concise usage
    if (argc < 2) {
        std::cout << ArbitrageReporter::format_usage();
        return 0;
    }

    const std::string first_arg = argv[1];

    // Check for help flags
    if (first_arg == "-h" || first_arg == "--help") {
        std::cout << ArbitrageReporter::format_help();
        return 0;
    }

    // Check for list samples
    if (first_arg == "--list-samples") {
        if (argc > 2) {
            std::cerr << "Error: Option '--list-samples' does not accept additional arguments.\n";
            return 1;
        }
        std::cout << ArbitrageReporter::format_sample_list();
        return 0;
    }

    // Check for unknown options
    if (!first_arg.empty() && first_arg[0] == '-') {
        std::cerr << "Error: Unknown option '" << first_arg
                  << "'. Use -h/--help for usage instructions.\n";
        return 1;
    }

    // Check for excess arguments
    if (argc > 3) {
        std::cerr << "Error: Too many arguments provided. Expected at most 2 arguments (<path-to-rates.csv> [starting_capital]).\n"
                  << "Use -h or --help for usage instructions.\n";
        return 1;
    }

    const std::string filepath = first_arg;
    double starting_capital = ArbitrageReporter::DEFAULT_STARTING_CAPITAL;

    // Parse optional starting capital if supplied
    if (argc == 3) {
        const std::string cap_str = argv[2];
        try {
            std::size_t idx = 0;
            const double val = std::stod(cap_str, &idx);
            if (idx != cap_str.size() || std::isnan(val) || std::isinf(val) || val <= 0.0) {
                std::cerr << "Error: Starting capital must be a strictly positive number. Provided: '"
                          << cap_str << "'\n";
                return 1;
            }
            starting_capital = val;
        } catch (...) {
            std::cerr << "Error: Starting capital must be a strictly positive number. Provided: '"
                      << cap_str << "'\n";
            return 1;
        }
    }

    CurrencyGraph graph;

    // Load exchange rates from CSV
    try {
        const std::size_t loaded_edges = RateLoader::load_from_csv(filepath, graph);
        std::cout << "Loaded " << loaded_edges << " exchange rates across "
                  << graph.vertex_count() << " currencies from: " << filepath << "\n\n";
    } catch (const RateLoaderException& ex) {
        std::cerr << ArbitrageReporter::format_csv_error(filepath, ex.what(), ex.line_number());
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Unexpected error: " << ex.what() << "\n";
        return 1;
    }

    // Run Arbitrage Detection
    const auto cycle_opt = ArbitrageDetector::detect_arbitrage(graph);

    if (cycle_opt.has_value()) {
        std::cout << ArbitrageReporter::format_opportunity(cycle_opt.value(), graph, starting_capital);
    } else {
        std::cout << ArbitrageReporter::format_no_arbitrage();
    }

    return 0;
}
