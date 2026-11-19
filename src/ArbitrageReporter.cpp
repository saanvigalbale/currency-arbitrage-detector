#include "ArbitrageReporter.hpp"

#include <sstream>
#include <iomanip>
#include <filesystem>
#include <vector>
#include <utility>

namespace {
constexpr const char* BANNER_LINE = "============================================================";
constexpr const char* SECTION_SEP = "------------------------------------------------------------";
} // anonymous namespace

std::string ArbitrageReporter::format_header() {
    std::ostringstream oss;
    oss << BANNER_LINE << "\n"
        << "                Currency Arbitrage Detector\n"
        << "  Graph-Based Foreign Exchange Arbitrage Detection System\n"
        << BANNER_LINE << "\n";
    return oss.str();
}

std::string ArbitrageReporter::format_opportunity(
    const ArbitrageCycle& cycle,
    const CurrencyGraph& graph,
    double starting_capital
) {
    std::ostringstream oss;

    oss << SECTION_SEP << "\n"
        << "                Arbitrage Opportunity Found!\n"
        << SECTION_SEP << "\n\n";

    // 1. Conversion Cycle
    oss << "Conversion Cycle:\n  ";
    for (std::size_t i = 0; i < cycle.path.size(); ++i) {
        oss << cycle.path[i];
        if (i + 1 < cycle.path.size()) {
            oss << " -> ";
        }
    }
    oss << "\n\n";

    // 2. Exchange Details
    oss << "Exchange Details:\n";
    for (std::size_t step = 0; step < cycle.edge_indices.size(); ++step) {
        const Edge& edge = graph.get_edge(cycle.edge_indices[step]);
        const std::string& from_code = graph.get_currency_code(edge.from);
        const std::string& to_code = graph.get_currency_code(edge.to);
        oss << "  Step " << (step + 1) << ": 1 " << from_code << " -> " << edge.original_rate << " " << to_code << "\n";
    }
    oss << "\n";

    // 3. Multiplier and Profit Percentage
    oss << "Yield Metrics:\n";
    oss << std::fixed << std::setprecision(6);
    oss << "  Compound Multiplier: " << cycle.compound_multiplier << "\n";
    oss << std::setprecision(4);
    oss << "  Estimated Profit: " << cycle.profit_percentage << "%\n\n";

    // 4. Starting Capital, Final Amount, and Profit Amount
    const std::string& start_currency = cycle.path.empty() ? "" : cycle.path.front();
    const double final_amount = starting_capital * cycle.compound_multiplier;
    const double profit_amount = final_amount - starting_capital;

    oss << "Simulated Returns:\n";
    oss << std::setprecision(2);
    oss << "  Starting Capital: " << starting_capital << " " << start_currency << "\n";
    oss << "  Final Amount: " << final_amount << " " << start_currency << "\n";
    oss << "  Estimated Profit Amount: +" << profit_amount << " " << start_currency << "\n";
    oss << SECTION_SEP << "\n";

    return oss.str();
}

std::string ArbitrageReporter::format_no_arbitrage() {
    std::ostringstream oss;
    oss << SECTION_SEP << "\n"
        << "                 No Arbitrage Detected\n"
        << SECTION_SEP << "\n"
        << "No arbitrage opportunity detected in the supplied exchange rates.\n"
        << SECTION_SEP << "\n";
    return oss.str();
}

std::string ArbitrageReporter::format_help() {
    std::ostringstream oss;
    oss << "Currency Arbitrage Detector\n"
        << "============================\n"
        << "Detects arbitrage opportunities in foreign exchange markets by modeling\n"
        << "exchange rates as a directed graph and identifying negative weight cycles\n"
        << "using the Bellman-Ford algorithm on -log(exchange_rate).\n\n"
        << "Command Syntax:\n"
        << "  currency_arbitrage <path-to-rates.csv> [starting_capital]\n"
        << "  currency_arbitrage <path-to-rates.csv> [starting_capital] --json\n"
        << "  currency_arbitrage --list-samples\n"
        << "  currency_arbitrage -h | --help\n\n"
        << "Arguments:\n"
        << "  <path-to-rates.csv>    Path to the CSV file containing exchange rates (required).\n"
        << "  [starting_capital]     Initial capital amount for conversion simulation (optional).\n"
        << "                         Must be a strictly positive number.\n"
        << "                         Default: " << DEFAULT_STARTING_CAPITAL << "\n\n"
        << "Options:\n"
        << "  --json                 Print the result as a single JSON document (no banner).\n"
        << "                         Exit code is 0 for arbitrage/no arbitrage, 1 for errors.\n"
        << "  --list-samples         List available sample datasets and their descriptions.\n"
        << "  -h, --help             Display this help message and exit.\n\n"
        << "Expected CSV Format:\n"
        << "  The CSV file must contain rows with three comma-separated fields:\n"
        << "    source_currency,target_currency,rate\n\n"
        << "  - Header row is supported and automatically detected.\n"
        << "  - Blank lines and whitespace around fields are ignored.\n"
        << "  - Exchange rates must be finite, non-zero positive numbers.\n\n"
        << "CSV Example:\n"
        << "  source_currency,target_currency,rate\n"
        << "  USD,EUR,0.92\n"
        << "  EUR,GBP,0.85\n"
        << "  GBP,USD,1.35\n\n"
        << "Usage Examples:\n"
        << "  currency_arbitrage --list-samples\n"
        << "  currency_arbitrage data/sample_rates.csv\n"
        << "  currency_arbitrage data/sample_rates.csv 50000\n";
    return oss.str();
}

std::string ArbitrageReporter::format_usage() {
    std::ostringstream oss;
    oss << "Usage: currency_arbitrage <path-to-rates.csv> [starting_capital]\n"
        << "       currency_arbitrage --list-samples\n"
        << "       currency_arbitrage -h | --help\n\n"
        << "Detects currency arbitrage opportunities from a CSV exchange-rate file.\n"
        << "Use --list-samples to view built-in datasets, or -h/--help for detailed instructions.\n";
    return oss.str();
}

std::string ArbitrageReporter::format_csv_error(
    const std::string& filepath,
    const std::string& reason,
    std::size_t line_number
) {
    std::ostringstream oss;
    oss << SECTION_SEP << "\n"
        << "                  CSV Load Error\n"
        << SECTION_SEP << "\n"
        << "  File:   " << filepath << "\n";

    if (line_number > 0) {
        oss << "  Line:   " << line_number << "\n";
    }

    oss << "  Reason: " << reason << "\n\n"
        << "  Suggestion: Verify the CSV contains rows in the format:\n"
        << "              source_currency,target_currency,rate\n"
        << "              Rates must be finite, positive numbers.\n"
        << "              Run with -h or --help for full format details.\n"
        << SECTION_SEP << "\n";
    return oss.str();
}

std::string ArbitrageReporter::format_sample_list() {
    std::ostringstream oss;
    oss << SECTION_SEP << "\n"
        << "                 Available Sample Datasets\n"
        << SECTION_SEP << "\n";

    const std::vector<std::pair<std::string, std::string>> known_samples = {
        {"sample_rates.csv", "Demonstrates an exchange-rate dataset containing an arbitrage opportunity."},
        {"equilibrium_rates.csv", "Demonstrates exchange rates without an arbitrage opportunity."}
    };

    // Locate data directory without hardcoding absolute paths
    const std::vector<std::string> candidates = {"data", "../data", "../../data"};
    std::string data_dir_found;
    for (const auto& c : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(c, ec) && std::filesystem::is_directory(c, ec)) {
            data_dir_found = c;
            break;
        }
    }

    if (data_dir_found.empty()) {
        oss << "Notice: Sample data directory 'data/' could not be located.\n"
            << SECTION_SEP << "\n";
        return oss.str();
    }

    oss << "Located in directory: " << data_dir_found << "/\n\n";

    bool any_sample_found = false;
    for (const auto& [filename, desc] : known_samples) {
        std::string sample_path = data_dir_found + "/" + filename;
        std::error_code ec;
        bool exists = std::filesystem::exists(sample_path, ec) && std::filesystem::is_regular_file(sample_path, ec);

        oss << "  * " << filename << "\n"
            << "    Description: " << desc << "\n"
            << "    Status:      " << (exists ? "Available" : "Not Found") << "\n";
        if (exists) {
            oss << "    Path:        " << sample_path << "\n";
            any_sample_found = true;
        }
        oss << "\n";
    }

    if (any_sample_found) {
        oss << "Usage Example:\n"
            << "  currency_arbitrage " << data_dir_found << "/sample_rates.csv\n";
    }
    oss << SECTION_SEP << "\n";

    return oss.str();
}
