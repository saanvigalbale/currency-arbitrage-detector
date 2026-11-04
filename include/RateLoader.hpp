#ifndef CURRENCY_ARBITRAGE_RATE_LOADER_HPP
#define CURRENCY_ARBITRAGE_RATE_LOADER_HPP

#include "CurrencyGraph.hpp"

#include <string>
#include <stdexcept>
#include <cstddef>

/**
 * @brief Exception thrown when CSV parsing or validation fails.
 */
class RateLoaderException : public std::runtime_error {
public:
    RateLoaderException(const std::string& message, std::size_t line_number = 0)
        : std::runtime_error(format_message(message, line_number)),
          line_number_(line_number) {}

    std::size_t line_number() const noexcept { return line_number_; }

private:
    static std::string format_message(const std::string& msg, std::size_t line) {
        if (line > 0) {
            return "CSV Error (line " + std::to_string(line) + "): " + msg;
        }
        return "CSV Error: " + msg;
    }

    std::size_t line_number_;
};

/**
 * @brief Reads exchange rates from CSV files into a CurrencyGraph.
 */
class RateLoader {
public:
    /**
     * @brief Loads exchange rates from a CSV file into the provided CurrencyGraph.
     *
     * Expected CSV format: source_currency,target_currency,rate
     * Supports optional header, trims whitespace around fields, and ignores blank lines.
     *
     * @param filepath Path to the CSV file.
     * @param graph CurrencyGraph reference where edges and currencies will be added.
     * @return Number of exchange-rate edges successfully added to the graph.
     * @throws RateLoaderException on missing file, I/O error, or malformed data.
     */
    static std::size_t load_from_csv(const std::string& filepath, CurrencyGraph& graph);
};

#endif // CURRENCY_ARBITRAGE_RATE_LOADER_HPP
