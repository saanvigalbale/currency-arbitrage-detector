#ifndef CURRENCY_ARBITRAGE_EDGE_HPP
#define CURRENCY_ARBITRAGE_EDGE_HPP

#include <cstddef>

/**
 * @brief Represents a directed edge between two currency vertices.
 * 
 * Each edge captures a directional conversion from a source currency to a
 * destination currency with an exchange rate and its negative log-weight.
 */
struct Edge {
    std::size_t from;       ///< Source currency vertex index
    std::size_t to;         ///< Destination currency vertex index
    double original_rate;   ///< Raw exchange rate (e.g. 1 unit of 'from' = original_rate units of 'to')
    double weight;          ///< Transformed weight: -std::log(original_rate)
};

#endif // CURRENCY_ARBITRAGE_EDGE_HPP
