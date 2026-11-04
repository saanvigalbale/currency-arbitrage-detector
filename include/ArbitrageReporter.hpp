#ifndef CURRENCY_ARBITRAGE_REPORTER_HPP
#define CURRENCY_ARBITRAGE_REPORTER_HPP

#include "ArbitrageCycle.hpp"
#include "CurrencyGraph.hpp"

#include <string>
#include <cstddef>

/**
 * @brief Formats application banners, detected arbitrage opportunities, and execution summaries.
 */
class ArbitrageReporter {
public:
    static constexpr double DEFAULT_STARTING_CAPITAL = 1000.0;

    /**
     * @brief Formats the standard application header banner with title and description.
     */
    static std::string format_header();

    /**
     * @brief Formats a detected arbitrage cycle into a structured, readable report with ASCII separators.
     *
     * @param cycle The detected arbitrage cycle.
     * @param graph The CurrencyGraph containing vertices and edge metadata.
     * @param starting_capital The initial capital amount for conversion simulation.
     * @return Formatted multi-line string report.
     */
    static std::string format_opportunity(
        const ArbitrageCycle& cycle,
        const CurrencyGraph& graph,
        double starting_capital = DEFAULT_STARTING_CAPITAL
    );

    /**
     * @brief Returns a structured message when no arbitrage opportunity is detected.
     */
    static std::string format_no_arbitrage();

    /**
     * @brief Returns a comprehensive help message explaining purpose, syntax, and CSV format.
     */
    static std::string format_help();

    /**
     * @brief Returns a concise usage message when no arguments are provided.
     */
    static std::string format_usage();

    /**
     * @brief Formats a structured, user-friendly error block for a CSV loading failure.
     *
     * @param filepath The path of the CSV file that failed to load.
     * @param reason The detailed reason string from RateLoaderException::what().
     * @param line_number CSV line number where the error occurred (0 if not line-specific).
     */
    static std::string format_csv_error(
        const std::string& filepath,
        const std::string& reason,
        std::size_t line_number = 0
    );

    /**
     * @brief Formats a list of available CSV sample files in the data directory and their descriptions.
     */
    static std::string format_sample_list();
};

#endif // CURRENCY_ARBITRAGE_REPORTER_HPP
