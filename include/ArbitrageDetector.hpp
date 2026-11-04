#ifndef CURRENCY_ARBITRAGE_DETECTOR_HPP
#define CURRENCY_ARBITRAGE_DETECTOR_HPP

#include "CurrencyGraph.hpp"
#include "ArbitrageCycle.hpp"

#include <optional>

/**
 * @brief Arbitrage detection engine using the Bellman-Ford algorithm on logarithmic weights.
 *
 * NOTE: This implementation detects and returns a single profitable arbitrage cycle
 * if one exists in the graph. It does not enumerate every possible negative cycle.
 */
class ArbitrageDetector {
public:
    /// Default minimum net multiplier threshold above 1.0 to consider a cycle profitable
    static constexpr double DEFAULT_MIN_PROFIT_THRESHOLD = 1e-6;

    /**
     * @brief Detects a profitable currency arbitrage cycle within the provided graph.
     *
     * Runs Bellman-Ford negative cycle detection using the precomputed logarithmic edge
     * weights. Vertex distances are initialized to 0.0 to detect cycles across any
     * connected or disconnected component. If a negative cycle is detected, predecessors
     * are traversed to extract the directed cycle, and profit is computed using original
     * exchange rates.
     *
     * @param graph The currency graph to analyze.
     * @param min_profit_threshold Minimum threshold for (compound_multiplier - 1.0) to reject numerical noise.
     * @return std::optional<ArbitrageCycle> containing cycle details if profitable arbitrage exists,
     *         std::nullopt otherwise.
     */
    static std::optional<ArbitrageCycle> detect_arbitrage(
        const CurrencyGraph& graph,
        double min_profit_threshold = DEFAULT_MIN_PROFIT_THRESHOLD
    );
};

/**
 * @brief Convenience free function for arbitrage detection.
 */
inline std::optional<ArbitrageCycle> detect_arbitrage(
    const CurrencyGraph& graph,
    double min_profit_threshold = ArbitrageDetector::DEFAULT_MIN_PROFIT_THRESHOLD
) {
    return ArbitrageDetector::detect_arbitrage(graph, min_profit_threshold);
}

#endif // CURRENCY_ARBITRAGE_DETECTOR_HPP
