// Keep assert() active even in Release builds (CMAKE_BUILD_TYPE=Release defines NDEBUG),
// otherwise every check in this file would silently compile to nothing.
#undef NDEBUG
#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <cmath>

#include "mini_json.hpp"

namespace {

struct ProcessResult {
    int exit_code{0};
    std::string output;
};

ProcessResult execute_cli(const std::string& exe_path, const std::string& args) {
    ProcessResult result;
#ifdef _WIN32
    std::string full_cmd = "\"\"" + exe_path + "\" " + args + " 2>&1\"";
    FILE* pipe = _popen(full_cmd.c_str(), "r");
#else
    std::string full_cmd = "\"" + exe_path + "\" " + args + " 2>&1";
    FILE* pipe = popen(full_cmd.c_str(), "r");
#endif

    if (!pipe) {
        throw std::runtime_error("Failed to open pipe for command: " + full_cmd);
    }

    std::array<char, 256> buffer;
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result.output += buffer.data();
    }

#ifdef _WIN32
    result.exit_code = _pclose(pipe);
#else
    result.exit_code = pclose(pipe);
#endif

    return result;
}

std::string find_executable() {
    const std::vector<std::string> candidates = {
        "currency_arbitrage.exe",
        "./currency_arbitrage.exe",
        "currency_arbitrage",
        "./currency_arbitrage",
        "build/currency_arbitrage.exe",
        "./build/currency_arbitrage.exe",
        "build/currency_arbitrage",
        "./build/currency_arbitrage",
        "../currency_arbitrage.exe",
        "../currency_arbitrage"
    };

    for (const auto& path : candidates) {
        if (std::filesystem::exists(path)) {
            return std::filesystem::absolute(path).string();
        }
    }

    throw std::runtime_error("Could not find currency_arbitrage executable.");
}

std::string find_sample_csv() {
    const std::vector<std::string> candidates = {
        "data/sample_rates.csv",
        "../data/sample_rates.csv",
        "../../data/sample_rates.csv"
    };

    for (const auto& path : candidates) {
        if (std::filesystem::exists(path)) {
            return std::filesystem::absolute(path).string();
        }
    }

    throw std::runtime_error("Could not find data/sample_rates.csv.");
}

} // anonymous namespace

void test_cli_no_arguments(const std::string& exe) {
    std::cout << "[RUN] test_cli_no_arguments..." << std::endl;
    auto res = execute_cli(exe, "");
    assert(res.exit_code == 0);
    assert(res.output.find("Usage: currency_arbitrage <path-to-rates.csv> [starting_capital]") != std::string::npos);
    assert(res.output.find("-h/--help") != std::string::npos);
    std::cout << "[PASS] test_cli_no_arguments" << std::endl;
}

void test_cli_help_flags(const std::string& exe) {
    std::cout << "[RUN] test_cli_help_flags..." << std::endl;

    // Test -h
    auto res_short = execute_cli(exe, "-h");
    assert(res_short.exit_code == 0);
    assert(res_short.output.find("Currency Arbitrage Detector") != std::string::npos);
    assert(res_short.output.find("Command Syntax:") != std::string::npos);
    assert(res_short.output.find("Default: 1000") != std::string::npos);
    assert(res_short.output.find("Expected CSV Format:") != std::string::npos);

    // Test --help
    auto res_long = execute_cli(exe, "--help");
    assert(res_long.exit_code == 0);
    assert(res_long.output.find("Currency Arbitrage Detector") != std::string::npos);
    assert(res_long.output.find("source_currency,target_currency,rate") != std::string::npos);

    std::cout << "[PASS] test_cli_help_flags" << std::endl;
}

void test_cli_unknown_option(const std::string& exe) {
    std::cout << "[RUN] test_cli_unknown_option..." << std::endl;
    auto res = execute_cli(exe, "-v");
    assert(res.exit_code != 0);
    assert(res.output.find("Error: Unknown option '-v'") != std::string::npos);
    assert(res.output.find("Use -h/--help for usage instructions") != std::string::npos);
    std::cout << "[PASS] test_cli_unknown_option" << std::endl;
}

void test_cli_too_many_arguments(const std::string& exe, const std::string& sample_csv) {
    std::cout << "[RUN] test_cli_too_many_arguments..." << std::endl;
    auto res = execute_cli(exe, "\"" + sample_csv + "\" 1000 extra_arg");
    assert(res.exit_code != 0);
    assert(res.output.find("Error: Too many arguments provided") != std::string::npos);
    std::cout << "[PASS] test_cli_too_many_arguments" << std::endl;
}

void test_cli_csv_only(const std::string& exe, const std::string& sample_csv) {
    std::cout << "[RUN] test_cli_csv_only..." << std::endl;
    auto res = execute_cli(exe, "\"" + sample_csv + "\"");
    assert(res.exit_code == 0);
    assert(res.output.find("Loaded 5 exchange rates across 4 currencies") != std::string::npos);
    assert(res.output.find("Arbitrage Opportunity Found!") != std::string::npos);
    assert(res.output.find("Starting Capital: 1000.00 USD") != std::string::npos);
    std::cout << "[PASS] test_cli_csv_only" << std::endl;
}

void test_cli_csv_with_custom_capital(const std::string& exe, const std::string& sample_csv) {
    std::cout << "[RUN] test_cli_csv_with_custom_capital..." << std::endl;
    auto res = execute_cli(exe, "\"" + sample_csv + "\" 50000");
    assert(res.exit_code == 0);
    assert(res.output.find("Arbitrage Opportunity Found!") != std::string::npos);
    assert(res.output.find("Starting Capital: 50000.00 USD") != std::string::npos);
    assert(res.output.find("Final Amount: 50048.00 USD") != std::string::npos);
    assert(res.output.find("Estimated Profit Amount: +48.00 USD") != std::string::npos);
    std::cout << "[PASS] test_cli_csv_with_custom_capital" << std::endl;
}

void test_cli_list_samples(const std::string& exe) {
    std::cout << "[RUN] test_cli_list_samples..." << std::endl;
    auto res = execute_cli(exe, "--list-samples");
    assert(res.exit_code == 0);
    assert(res.output.find("Available Sample Datasets") != std::string::npos);
    assert(res.output.find("sample_rates.csv") != std::string::npos);
    assert(res.output.find("equilibrium_rates.csv") != std::string::npos);
    assert(res.output.find("Demonstrates an exchange-rate dataset containing an arbitrage opportunity.") != std::string::npos);
    assert(res.output.find("Demonstrates exchange rates without an arbitrage opportunity.") != std::string::npos);

    // Test with unexpected extra arguments
    auto res_extra = execute_cli(exe, "--list-samples extra");
    assert(res_extra.exit_code != 0);
    assert(res_extra.output.find("Error: Option '--list-samples' does not accept additional arguments.") != std::string::npos);

    std::cout << "[PASS] test_cli_list_samples" << std::endl;
}

void test_cli_invalid_csv(const std::string& exe) {
    std::cout << "[RUN] test_cli_invalid_csv..." << std::endl;

    // --- Case 1: CSV with an invalid (negative) rate on line 2 ---
    const std::string bad_csv_path =
        (std::filesystem::temp_directory_path() / "test_cli_bad_rate.csv").string();
    {
        std::ofstream out(bad_csv_path, std::ios::binary);
        out << "source_currency,target_currency,rate\n"
            << "USD,EUR,0.92\n"
            << "EUR,GBP,-0.85\n";   // negative rate — invalid
    }

#ifdef _WIN32
    const std::string quoted_path = "\"" + bad_csv_path + "\"";
#else
    const std::string quoted_path = "\"" + bad_csv_path + "\"";
#endif

    auto res_bad = execute_cli(exe, quoted_path);

    // Must exit with non-zero code
    assert(res_bad.exit_code != 0);
    // Structured diagnostic heading must appear
    assert(res_bad.output.find("CSV Load Error") != std::string::npos);
    // File path must be echoed
    assert(res_bad.output.find("test_cli_bad_rate.csv") != std::string::npos);
    // Specific reason must be surfaced
    assert(res_bad.output.find("Rate must be strictly positive") != std::string::npos);
    // Suggestion block must appear
    assert(res_bad.output.find("Suggestion:") != std::string::npos);

    std::filesystem::remove(bad_csv_path);

    // --- Case 2: Completely empty CSV ---
    const std::string empty_csv_path =
        (std::filesystem::temp_directory_path() / "test_cli_empty.csv").string();
    {
        std::ofstream out(empty_csv_path, std::ios::binary);
        // write nothing
    }

    auto res_empty = execute_cli(exe, "\"" + empty_csv_path + "\"");

    assert(res_empty.exit_code != 0);
    assert(res_empty.output.find("CSV Load Error") != std::string::npos);
    assert(res_empty.output.find("No valid exchange-rate rows found") != std::string::npos);

    std::filesystem::remove(empty_csv_path);

    std::cout << "[PASS] test_cli_invalid_csv" << std::endl;
}

namespace {

std::string quote(const std::string& path) {
    return "\"" + path + "\"";
}

std::string write_temp_csv(const std::string& name, const std::string& content) {
    const std::string path = (std::filesystem::temp_directory_path() / name).string();
    std::ofstream out(path, std::ios::binary);
    out << content;
    return path;
}

} // anonymous namespace

void test_cli_json_arbitrage(const std::string& exe, const std::string& sample_csv) {
    std::cout << "[RUN] test_cli_json_arbitrage..." << std::endl;

    // Flag after the file, default capital.
    auto res = execute_cli(exe, quote(sample_csv) + " --json");
    assert(res.exit_code == 0);
    assert(res.output.find("Currency Arbitrage Detector") == std::string::npos);   // no banner
    assert(res.output.find("Loaded ") == std::string::npos);                       // no text preamble
    auto j = minijson::parse(res.output);                                          // stdout is one JSON document
    assert(j.at("status").text == "arbitrage");
    assert(j.at("meta").at("starting_capital").number == 1000.0);
    assert(j.at("analysis").at("currency").text == "USD");
    assert(j.at("analysis").at("steps").items.size() == 3);
    assert(std::fabs(j.at("analysis").at("final_capital").number - 1000.96) < 1e-9);

    // Flag first, custom capital.
    auto res2 = execute_cli(exe, "--json " + quote(sample_csv) + " 50000");
    assert(res2.exit_code == 0);
    auto j2 = minijson::parse(res2.output);
    assert(j2.at("analysis").at("starting_capital").number == 50000.0);
    assert(std::fabs(j2.at("analysis").at("final_capital").number - 50048.0) < 1e-6);
    assert(std::fabs(j2.at("analysis").at("profit").number - 48.0) < 1e-6);

    std::cout << "[PASS] test_cli_json_arbitrage" << std::endl;
}

void test_cli_json_no_arbitrage(const std::string& exe, const std::string& sample_csv) {
    std::cout << "[RUN] test_cli_json_no_arbitrage..." << std::endl;
    const std::string equilibrium =
        (std::filesystem::path(sample_csv).parent_path() / "equilibrium_rates.csv").string();
    auto res = execute_cli(exe, quote(equilibrium) + " --json");
    assert(res.exit_code == 0);
    auto j = minijson::parse(res.output);
    assert(j.at("status").text == "no_arbitrage");
    assert(j.at("analysis").is_null());
    assert(j.at("error").is_null());
    std::cout << "[PASS] test_cli_json_no_arbitrage" << std::endl;
}

void test_cli_json_errors(const std::string& exe, const std::string& sample_csv) {
    std::cout << "[RUN] test_cli_json_errors..." << std::endl;

    // CSV error keeps its line number.
    const std::string bad = write_temp_csv("test_cli_json_bad.csv",
        "source_currency,target_currency,rate\nUSD,EUR,0.92\nEUR,GBP,-0.85\n");
    auto res = execute_cli(exe, "--json " + quote(bad));
    assert(res.exit_code != 0);
    auto j = minijson::parse(res.output);
    assert(j.at("status").text == "error");
    assert(j.at("error").at("type").text == "csv");
    assert(j.at("error").at("line").number == 3.0);
    assert(res.output.find("CSV Load Error") == std::string::npos);   // text diagnostics not mixed in
    std::filesystem::remove(bad);

    // Malformed first data row.
    const std::string first = write_temp_csv("test_cli_json_first_row.csv", "USD,EUR,abc\n");
    auto res_first = execute_cli(exe, quote(first) + " --json");
    assert(res_first.exit_code != 0);
    assert(minijson::parse(res_first.output).at("error").at("line").number == 1.0);

    // Same file in text mode reports the line instead of "no valid rows".
    auto res_text = execute_cli(exe, quote(first));
    assert(res_text.exit_code != 0);
    assert(res_text.output.find("CSV Load Error") != std::string::npos);
    assert(res_text.output.find("Line:   1") != std::string::npos);
    assert(res_text.output.find("Cannot parse rate as number") != std::string::npos);
    std::filesystem::remove(first);

    // Invalid capital, missing path, unknown option, too many arguments: all JSON errors.
    auto bad_cap = execute_cli(exe, quote(sample_csv) + " abc --json");
    assert(bad_cap.exit_code != 0);
    assert(minijson::parse(bad_cap.output).at("error").at("type").text == "input");

    auto no_path = execute_cli(exe, "--json");
    assert(no_path.exit_code != 0);
    assert(minijson::parse(no_path.output).at("error").at("type").text == "usage");

    auto unknown = execute_cli(exe, "--json -x");
    assert(unknown.exit_code != 0);
    assert(minijson::parse(unknown.output).at("error").at("message").text.find("Unknown option '-x'") != std::string::npos);

    auto too_many = execute_cli(exe, quote(sample_csv) + " 100 extra --json");
    assert(too_many.exit_code != 0);
    assert(minijson::parse(too_many.output).at("error").at("type").text == "usage");

    auto missing = execute_cli(exe, "does_not_exist_abc.csv --json");
    assert(missing.exit_code != 0);
    assert(minijson::parse(missing.output).at("error").at("message").text.find("Could not open file") != std::string::npos);

    std::cout << "[PASS] test_cli_json_errors" << std::endl;
}

void test_cli_text_mode_unchanged_by_json_support(const std::string& exe, const std::string& sample_csv) {
    std::cout << "[RUN] test_cli_text_mode_unchanged_by_json_support..." << std::endl;
    auto res = execute_cli(exe, quote(sample_csv) + " 50000");
    assert(res.exit_code == 0);
    assert(res.output.find("Currency Arbitrage Detector") != std::string::npos);
    assert(res.output.find("Arbitrage Opportunity Found!") != std::string::npos);
    assert(res.output.find("USD -> EUR -> GBP -> USD") != std::string::npos);
    assert(res.output.find("Final Amount: 50048.00 USD") != std::string::npos);
    assert(res.output.find("\"status\"") == std::string::npos);   // no JSON leaks into text mode

    auto help = execute_cli(exe, "--help");
    assert(help.exit_code == 0);
    assert(help.output.find("--json") != std::string::npos);        // documented
    std::cout << "[PASS] test_cli_text_mode_unchanged_by_json_support" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Starting Actual CLI Executable Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    const std::string exe = find_executable();
    const std::string sample_csv = find_sample_csv();
    std::cout << "Testing executable: " << exe << std::endl;
    std::cout << "Using sample CSV:   " << sample_csv << std::endl;

    test_cli_no_arguments(exe);
    test_cli_help_flags(exe);
    test_cli_list_samples(exe);
    test_cli_unknown_option(exe);
    test_cli_too_many_arguments(exe, sample_csv);
    test_cli_csv_only(exe, sample_csv);
    test_cli_csv_with_custom_capital(exe, sample_csv);
    test_cli_invalid_csv(exe);
    test_cli_json_arbitrage(exe, sample_csv);
    test_cli_json_no_arbitrage(exe, sample_csv);
    test_cli_json_errors(exe, sample_csv);
    test_cli_text_mode_unchanged_by_json_support(exe, sample_csv);

    std::cout << "========================================" << std::endl;
    std::cout << "All Actual CLI Executable Tests Passed!" << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
