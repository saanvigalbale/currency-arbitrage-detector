#include "RateLoader.hpp"

#include <fstream>
#include <sstream>
#include <vector>
#include <cctype>
#include <cmath>
#include <algorithm>

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

bool is_header_row(const std::string& src, const std::string& tgt, const std::string& rate_str) {
    const std::string lower_rate = to_lower_string(rate_str);
    if (lower_rate == "rate" || lower_rate == "exchange_rate" || lower_rate == "price") {
        return true;
    }

    const std::string lower_src = to_lower_string(src);
    if (lower_src == "source_currency" || lower_src == "source" || lower_src == "from") {
        return true;
    }

    const std::string lower_tgt = to_lower_string(tgt);
    if (lower_tgt == "target_currency" || lower_tgt == "target" || lower_tgt == "to") {
        return true;
    }

    // If the rate column cannot be parsed as a number at all, it's a header
    try {
        std::size_t idx = 0;
        std::stod(rate_str, &idx);
        if (idx != rate_str.size()) {
            return true;
        }
    } catch (...) {
        return true;
    }

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

std::size_t RateLoader::load_from_csv(const std::string& filepath, CurrencyGraph& graph) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw RateLoaderException("Could not open file: " + filepath, 0);
    }

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
        throw RateLoaderException("I/O error occurred while reading file: " + filepath, line_number);
    }

    if (edges_loaded == 0) {
        throw RateLoaderException(
            "No valid exchange-rate rows found. "
            "The file may be empty or contain only a header row: " + filepath,
            0
        );
    }

    return edges_loaded;
}
