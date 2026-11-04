#include "ArbitrageDetector.hpp"

#include <vector>
#include <limits>
#include <cmath>
#include <utility>

std::optional<ArbitrageCycle> ArbitrageDetector::detect_arbitrage(
    const CurrencyGraph& graph,
    double min_profit_threshold
) {
    const std::size_t V = graph.vertex_count();
    const auto& edges = graph.get_edges();
    const std::size_t E = edges.size();

    // A graph must have at least 1 vertex and 1 edge (e.g. a self-loop)
    if (V < 1 || E < 1) {
        return std::nullopt;
    }

    // Distance vector initialized to 0.0 to detect negative cycles across all components
    std::vector<double> dist(V, 0.0);

    // Track the incoming predecessor edge index for each vertex
    std::vector<std::size_t> predecessor_edge(V, std::numeric_limits<std::size_t>::max());

    constexpr double RELAX_EPSILON = 1e-12;

    // Track all vertices relaxed in the final (V-th) iteration
    std::vector<std::size_t> relaxed_in_last_iter;
    std::vector<bool> in_relaxed_list(V, false);

    // Run V iterations of Bellman-Ford
    for (std::size_t iter = 0; iter < V; ++iter) {
        bool any_relaxed = false;

        for (std::size_t e_idx = 0; e_idx < E; ++e_idx) {
            const auto& edge = edges[e_idx];
            if (dist[edge.from] + edge.weight < dist[edge.to] - RELAX_EPSILON) {
                dist[edge.to] = dist[edge.from] + edge.weight;
                predecessor_edge[edge.to] = e_idx;
                any_relaxed = true;

                if (iter == V - 1 && !in_relaxed_list[edge.to]) {
                    in_relaxed_list[edge.to] = true;
                    relaxed_in_last_iter.push_back(edge.to);
                }
            }
        }

        // If no relaxation occurred in an earlier round, distances have converged
        if (!any_relaxed) {
            return std::nullopt;
        }
    }

    // Attempt cycle reconstruction from candidate vertices relaxed on the V-th iteration.
    // Iterating over all relaxed candidates ensures that a sub-threshold or neutral cycle
    // does not hide a genuinely profitable arbitrage cycle in another component.
    for (std::size_t candidate : relaxed_in_last_iter) {
        std::size_t cycle_vertex = candidate;
        bool step_back_failed = false;

        // Follow predecessor edges V times to ensure landing inside a negative cycle
        for (std::size_t i = 0; i < V; ++i) {
            const std::size_t p_edge = predecessor_edge[cycle_vertex];
            if (p_edge == std::numeric_limits<std::size_t>::max()) {
                step_back_failed = true;
                break;
            }
            cycle_vertex = edges[p_edge].from;
        }

        if (step_back_failed) {
            continue;
        }

        // Extract the cycle by walking backwards until a vertex repeats
        std::vector<int> step_visited(V, -1);
        std::vector<std::size_t> path_vertices;
        std::vector<std::size_t> path_edges;

        std::size_t trace = cycle_vertex;
        bool extraction_failed = false;

        while (step_visited[trace] == -1) {
            step_visited[trace] = static_cast<int>(path_vertices.size());
            path_vertices.push_back(trace);

            const std::size_t p_edge = predecessor_edge[trace];
            if (p_edge == std::numeric_limits<std::size_t>::max()) {
                extraction_failed = true;
                break;
            }
            path_edges.push_back(p_edge);
            trace = edges[p_edge].from;
        }

        if (extraction_failed) {
            continue;
        }

        const int cycle_start_step = step_visited[trace];
        if (cycle_start_step < 0) {
            continue;
        }

        // Reverse the cycle edge slice to get edges in forward traversal order
        std::vector<std::size_t> cycle_edges;
        for (int i = static_cast<int>(path_edges.size()) - 1; i >= cycle_start_step; --i) {
            cycle_edges.push_back(path_edges[i]);
        }

        if (cycle_edges.empty()) {
            continue;
        }

        // Reconstruct the forward currency path
        std::vector<std::string> path;
        path.push_back(graph.get_currency_code(edges[cycle_edges[0]].from));
        for (std::size_t e_idx : cycle_edges) {
            path.push_back(graph.get_currency_code(edges[e_idx].to));
        }

        // Compute compound multiplier using original exchange rates
        double compound_multiplier = 1.0;
        for (std::size_t e_idx : cycle_edges) {
            compound_multiplier *= edges[e_idx].original_rate;
        }

        // Guard against numerical instability, NaN, infinity, or sub-threshold profit
        if (std::isnan(compound_multiplier) || std::isinf(compound_multiplier)) {
            continue;
        }

        if (compound_multiplier <= 1.0 + min_profit_threshold) {
            continue;
        }

        const double profit_percentage = (compound_multiplier - 1.0) * 100.0;

        ArbitrageCycle cycle;
        cycle.path = std::move(path);
        cycle.edge_indices = std::move(cycle_edges);
        cycle.compound_multiplier = compound_multiplier;
        cycle.profit_percentage = profit_percentage;

        return cycle;
    }

    return std::nullopt;
}
