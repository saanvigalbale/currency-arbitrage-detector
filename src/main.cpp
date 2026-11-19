#include "CurrencyGraph.hpp"
#include "RateLoader.hpp"
#include "ArbitrageDetector.hpp"
#include "ArbitrageReporter.hpp"
#include "AnalysisService.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <cmath>

namespace {

// Same rule as the text-mode capital check below: a strictly positive, finite number.
bool parse_capital(const std::string& text, double& out) {
    try {
        std::size_t idx = 0;
        const double val = std::stod(text, &idx);
        if (idx != text.size() || std::isnan(val) || std::isinf(val) || val <= 0.0) {
            return false;
        }
        out = val;
        return true;
    } catch (...) {
        return false;
    }
}

// --json mode: stdout carries exactly one JSON document (no banner or other text).
// Exit code is 0 for "arbitrage" and "no_arbitrage", 1 for "error".
int run_json_mode(const std::vector<std::string>& args) {
    auto emit = [](const AnalysisResult& result) {
        std::cout << AnalysisService::to_json(result) << "\n";
        return result.status == AnalysisStatus::Error ? 1 : 0;
    };

    if (args.empty()) {
        return emit(AnalysisService::make_error(
            "usage", "Missing CSV file path. Usage: currency_arbitrage <path-to-rates.csv> [starting_capital] --json"));
    }
    if (!args[0].empty() && args[0][0] == '-') {
        return emit(AnalysisService::make_error(
            "usage", "Unknown option '" + args[0] + "'. Use -h/--help for usage instructions."));
    }
    if (args.size() > 2) {
        return emit(AnalysisService::make_error(
            "usage", "Too many arguments provided. Expected at most 2 arguments (<path-to-rates.csv> [starting_capital])."));
    }

    double capital = ArbitrageReporter::DEFAULT_STARTING_CAPITAL;
    if (args.size() == 2 && !parse_capital(args[1], capital)) {
        return emit(AnalysisService::make_error(
            "input", "Starting capital must be a strictly positive number. Provided: '" + args[1] + "'"));
    }

    return emit(AnalysisService::analyze_file(args[0], capital));
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    // Machine-readable mode: '--json' may appear anywhere among the arguments.
    {
        bool json_mode = false;
        std::vector<std::string> positional;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--json") {
                json_mode = true;
            } else {
                positional.push_back(arg);
            }
        }
        if (json_mode) {
            return run_json_mode(positional);
        }
    }

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
