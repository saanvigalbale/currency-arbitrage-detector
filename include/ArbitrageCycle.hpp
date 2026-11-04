#ifndef CURRENCY_ARBITRAGE_CYCLE_HPP
#define CURRENCY_ARBITRAGE_CYCLE_HPP

#include <vector>
#include <string>
#include <cstddef>

/**
 * @brief Represents a detected cycle with profit details.
 */
struct ArbitrageCycle {
    std::vector<std::string> path;         ///< Sequence of currency symbols in the cycle (e.g. ["USD", "EUR", "GBP", "USD"])
    std::vector<std::size_t> edge_indices; ///< Indices of edges traversed in the graph
    double compound_multiplier{1.0};       ///< Product of original exchange rates along the cycle
    double profit_percentage{0.0};         ///< Profit percentage: (compound_multiplier - 1.0) * 100.0
};

#endif // CURRENCY_ARBITRAGE_CYCLE_HPP
