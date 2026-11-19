#include "RateLoader.hpp"

#include <fstream>
#include <istream>
#include <sstream>
#include <vector>
#include <cctype>
#include <cmath>
#include <algorithm>
#include <utility>

namespace {

std::string trim(const std::string& str) {
    const auto first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

bool is_blank_line(const std::string& line) {
    return std::all_of(line.begin(), line.end(), [](unsigned char c) {
        return std::isspace(c);
    });
}

std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> fields;
    std::size_t start = 0;
    std::size_t pos = 0;
    while ((pos = line.find(',', start)) != std::string::npos) {
        fields.push_back(line.substr(start, pos - start));
        start = pos + 1;
    }
    fields.push_back(line.substr(start));
    return fields;
}

std::string to_lower_string(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return str;
}

// Lower-cases and maps spaces to underscores so "Source Currency" matches "source_currency".
std::string normalize_header_token(std::string str) {
    str = to_lower_string(std::move(str));
    std::replace(str.begin(), str.end(), ' ', '_');
    return str;
}

bool is_header_row(const std::string& src, const std::string& tgt, const std::string& rate_str) {
    const std::string rate = normalize_header_token(rate_str);
    if (rate == "rate" || rate == "exchange_rate" || rate == "fx_rate" || rate == "price") {
        return true;
    }

    const std::string source = normalize_header_token(src);
    if (source == "source_currency" || source == "source" || source == "from" ||
        source == "from_currency" || source == "base" || source == "base_currency") {
        return true;
    }

    const std::string target = normalize_header_token(tgt);
    if (target == "target_currency" || target == "target" || target == "to" ||
        target == "to_currency" || target == "quote" || target == "quote_currency") {
        return true;
    }

    // A first row is a header only when one of its columns carries a recognised header
    // keyword. An unparseable rate (e.g. 'USD,EUR,abc') is malformed data and must be
    // reported with its line number by the normal validation, not silently skipped.
    return false;
}

bool is_valid_currency_code(const std::string& code) {
    if (code.empty()) {
        return false;
    }
    for (char c : code) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (!std::isalnum(uc) && c != '_' && c != '-' && c != '.') {
            return false;
        }
    }
    return true;
}

double parse_rate(const std::string& str, std::size_t line_number) {
    if (str.empty()) {
        throw RateLoaderException("Rate field is empty", line_number);
    }

    std::size_t idx = 0;
    double val = 0.0;
    try {
        val = std::stod(str, &idx);
    } catch (const std::exception&) {
        throw RateLoaderException("Cannot parse rate as number: '" + str + "'", line_number);
    }

    if (idx != str.size()) {
        throw RateLoaderException("Trailing non-numeric characters after rate: '" + str + "'", line_number);
    }

    if (std::isnan(val)) {
        throw RateLoaderException("Rate is NaN: '" + str + "'", line_number);
    }

    if (std::isinf(val)) {
        throw RateLoaderException("Rate is infinite: '" + str + "'", line_number);
    }

    if (val <= 0.0) {
        throw RateLoaderException("Rate must be strictly positive: '" + str + "'", line_number);
    }

    return val;
}

} // anonymous namespace

namespace {

// Shared implementation. `source_label` is the file path when loading from a file and
// empty when loading from an arbitrary stream; it only affects the wording of
// file-level messages.
std::size_t load_rows(std::istream& file, CurrencyGraph& graph, const std::string& source_label) {
    std::string line;
    std::size_t line_number = 0;
    std::size_t edges_loaded = 0;
    bool first_non_empty_line = true;

    while (std::getline(file, line)) {
        ++line_number;

        // Strip a UTF-8 BOM (EF BB BF), only at the very start of the file
        if (line_number == 1 && line.size() >= 3 &&
            static_cast<unsigned char>(line[0]) == 0xEF &&
            static_cast<unsigned char>(line[1]) == 0xBB &&
            static_cast<unsigned char>(line[2]) == 0xBF) {
            line.erase(0, 3);
        }

        // Strip carriage return if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // Ignore blank lines
        if (is_blank_line(line)) {
            continue;
        }

        const auto fields = split_csv_line(line);
        if (fields.size() != 3) {
            throw RateLoaderException(
                "Expected exactly 3 fields (source,target,rate), found " + std::to_string(fields.size()),
                line_number
            );
        }

        const std::string source = trim(fields[0]);
        const std::string target = trim(fields[1]);
        const std::string rate_str = trim(fields[2]);

        // Process header on the first non-empty line
        if (first_non_empty_line) {
            first_non_empty_line = false;
            if (is_header_row(source, target, rate_str)) {
                continue;
            }
        }

        // Validate currency codes
        if (!is_valid_currency_code(source)) {
            throw RateLoaderException("Invalid source currency code: '" + source + "'", line_number);
        }
        if (!is_valid_currency_code(target)) {
            throw RateLoaderException("Invalid target currency code: '" + target + "'", line_number);
        }

        // Parse and validate rate
        const double rate = parse_rate(rate_str, line_number);
        if (!CurrencyGraph::is_valid_rate(rate)) {
            throw RateLoaderException("Invalid rate value: '" + rate_str + "'", line_number);
        }

        graph.add_edge(source, target, rate);
        ++edges_loaded;
    }

    if (file.bad()) {
        throw RateLoaderException(
            source_label.empty()
                ? std::string("I/O error occurred while reading input")
                : "I/O error occurred while reading file: " + source_label,
            line_number
        );
    }

    if (edges_loaded == 0) {
        throw RateLoaderException(
            source_label.empty()
                ? std::string("No valid exchange-rate rows found. "
                              "The input may be empty or contain only a header row.")
                : "No valid exchange-rate rows found. "
                  "The file may be empty or contain only a header row: " + source_label,
            0
        );
    }

    return edges_loaded;
}

} // anonymous namespace

std::size_t RateLoader::load_from_csv(const std::string& filepath, CurrencyGraph& graph) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw RateLoaderException("Could not open file: " + filepath, 0);
    }
    return load_rows(file, graph, filepath);
}

std::size_t RateLoader::load_from_stream(std::istream& input, CurrencyGraph& graph) {
    return load_rows(input, graph, "");
}
