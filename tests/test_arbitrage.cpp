#include "CurrencyGraph.hpp"
#include "ArbitrageDetector.hpp"

#include <iostream>
#include <cassert>
#include <cmath>

void test_insufficient_graph() {
    std::cout << "[RUN] test_insufficient_graph..." << std::endl;

    // 0 vertices
    CurrencyGraph empty_graph;
    assert(!detect_arbitrage(empty_graph).has_value());

    // 1 vertex, 0 edges
    CurrencyGraph one_vertex;
    one_vertex.get_or_register_currency("USD");
    assert(!detect_arbitrage(one_vertex).has_value());

    // 2 vertices, 0 edges
    CurrencyGraph two_vertices;
    two_vertices.get_or_register_currency("USD");
    two_vertices.get_or_register_currency("EUR");
    assert(!detect_arbitrage(two_vertices).has_value());

    // 2 vertices, 1 edge (cannot form cycle)
    two_vertices.add_edge("USD", "EUR", 0.95);
    assert(!detect_arbitrage(two_vertices).has_value());

    std::cout << "[PASS] test_insufficient_graph" << std::endl;
}

void test_no_arbitrage_equilibrium() {
    std::cout << "[RUN] test_no_arbitrage_equilibrium..." << std::endl;
    CurrencyGraph graph;

    // Sub-1.0 cycle products (loss-making conversions due to spread)
    graph.add_edge("USD", "EUR", 0.90);
    graph.add_edge("EUR", "USD", 1.10);  // 0.90 * 1.10 = 0.99

    graph.add_edge("EUR", "GBP", 0.85);
    graph.add_edge("GBP", "EUR", 1.15);  // 0.85 * 1.15 = 0.9775

    graph.add_edge("GBP", "USD", 1.28);
    graph.add_edge("USD", "GBP", 0.76);  // 1.28 * 0.76 = 0.9728

    // 3-way cycles:
    // USD -> EUR -> GBP -> USD = 0.90 * 0.85 * 1.28 = 0.9792 < 1.0
    // USD -> GBP -> EUR -> USD = 0.76 * 1.15 * 1.10 = 0.9614 < 1.0
    auto result = detect_arbitrage(graph);
    assert(!result.has_value());

    std::cout << "[PASS] test_no_arbitrage_equilibrium" << std::endl;
}

void test_profitable_arbitrage_3cycle() {
    std::cout << "[RUN] test_profitable_arbitrage_3cycle..." << std::endl;
    CurrencyGraph graph;

    // USD -> EUR -> GBP -> USD: 0.90 * 0.85 * 1.35 = 1.03275 (+3.275%)
    graph.add_edge("USD", "EUR", 0.90);
    graph.add_edge("EUR", "GBP", 0.85);
    graph.add_edge("GBP", "USD", 1.35);

    auto result = detect_arbitrage(graph);
    assert(result.has_value());

    const auto& cycle = result.value();
    assert(cycle.path.size() == 4);
    assert(cycle.path.front() == cycle.path.back());
    assert(cycle.edge_indices.size() == 3);

    // Multiplier must match product of original rates
    const double expected_multiplier = 0.90 * 0.85 * 1.35;
    assert(std::abs(cycle.compound_multiplier - expected_multiplier) < 1e-9);

    const double expected_profit = (expected_multiplier - 1.0) * 100.0;
    assert(std::abs(cycle.profit_percentage - expected_profit) < 1e-9);
    assert(cycle.profit_percentage > 3.0);

    // Verify directional integrity
    for (std::size_t i = 0; i < cycle.edge_indices.size(); ++i) {
        const Edge& edge = graph.get_edge(cycle.edge_indices[i]);
        assert(graph.get_currency_code(edge.from) == cycle.path[i]);
        assert(graph.get_currency_code(edge.to) == cycle.path[i + 1]);
    }

    std::cout << "[PASS] test_profitable_arbitrage_3cycle" << std::endl;
}

void test_disconnected_component_arbitrage() {
    std::cout << "[RUN] test_disconnected_component_arbitrage..." << std::endl;
    CurrencyGraph graph;

    // Component 1: Equilibrium, no arbitrage
    graph.add_edge("AAA", "BBB", 1.00);
    graph.add_edge("BBB", "AAA", 0.95);

    // Component 2: Disconnected from Component 1, contains arbitrage
    // XXX -> YYY -> ZZZ -> XXX: 1.20 * 1.10 * 1.05 = 1.386 (+38.6%)
    graph.add_edge("XXX", "YYY", 1.20);
    graph.add_edge("YYY", "ZZZ", 1.10);
    graph.add_edge("ZZZ", "XXX", 1.05);

    auto result = detect_arbitrage(graph);
    assert(result.has_value());

    const auto& cycle = result.value();
    assert(cycle.path.front() == cycle.path.back());
    assert(cycle.compound_multiplier > 1.30);
    assert(cycle.profit_percentage > 30.0);

    // Confirm the cycle is entirely within Component 2
    for (const auto& code : cycle.path) {
        assert(code == "XXX" || code == "YYY" || code == "ZZZ");
    }

    std::cout << "[PASS] test_disconnected_component_arbitrage" << std::endl;
}

void test_multiple_currencies_and_edges() {
    std::cout << "[RUN] test_multiple_currencies_and_edges..." << std::endl;
    CurrencyGraph graph;

    // 5 currencies with multiple interconnects
    graph.add_edge("USD", "EUR", 0.92);
    graph.add_edge("EUR", "USD", 1.08);

    graph.add_edge("USD", "JPY", 150.0);
    graph.add_edge("JPY", "USD", 0.0066);

    graph.add_edge("EUR", "CHF", 0.95);
    graph.add_edge("CHF", "EUR", 1.05);

    graph.add_edge("JPY", "CHF", 0.0060);
    graph.add_edge("CHF", "JPY", 166.0);

    // Introduce arbitrage between EUR, CHF, JPY:
    // EUR -> CHF (0.95) * CHF -> JPY (166.0) * JPY -> USD (0.0066) * USD -> EUR (1.05)
    // 0.95 * 166.0 * 0.0066 * 1.05 = 1.09249 (+9.25%)
    graph.add_edge("USD", "EUR", 1.05); // Alternative edge

    auto result = detect_arbitrage(graph);
    assert(result.has_value());

    const auto& cycle = result.value();
    assert(cycle.path.front() == cycle.path.back());
    assert(cycle.profit_percentage > 0.0);
    assert(cycle.edge_indices.size() >= 2);

    // Verify all edges in cycle exist and align
    for (std::size_t i = 0; i < cycle.edge_indices.size(); ++i) {
        const Edge& edge = graph.get_edge(cycle.edge_indices[i]);
        assert(graph.get_currency_code(edge.from) == cycle.path[i]);
        assert(graph.get_currency_code(edge.to) == cycle.path[i + 1]);
    }

    std::cout << "[PASS] test_multiple_currencies_and_edges" << std::endl;
}

void test_path_reconstruction_and_precision() {
    std::cout << "[RUN] test_path_reconstruction_and_precision..." << std::endl;
    CurrencyGraph graph;

    // 2-currency cycle: A -> B -> A: 1.10 * 1.10 = 1.21 (+21%)
    graph.add_edge("ALPHA", "BETA", 1.10);
    graph.add_edge("BETA", "ALPHA", 1.10);

    auto result = detect_arbitrage(graph);
    assert(result.has_value());

    const auto& cycle = result.value();
    assert(cycle.path.size() == 3);
    assert(cycle.path[0] == cycle.path[2]);
    assert(cycle.edge_indices.size() == 2);
    assert(std::abs(cycle.compound_multiplier - 1.21) < 1e-9);
    assert(std::abs(cycle.profit_percentage - 21.0) < 1e-9);

    // Test rejection of numerically insignificant or neutral cycle (1.000000000)
    CurrencyGraph neutral_graph;
    neutral_graph.add_edge("X", "Y", 1.25);
    neutral_graph.add_edge("Y", "X", 0.80); // 1.25 * 0.80 = 1.0000000 (0% profit)

    auto neutral_result = detect_arbitrage(neutral_graph);
    assert(!neutral_result.has_value());

    std::cout << "[PASS] test_path_reconstruction_and_precision" << std::endl;
}

// Regression Test 1: Self-loop arbitrage (V=1, E=1 and V>1, E>=1)
void test_self_loop_arbitrage() {
    std::cout << "[RUN] test_self_loop_arbitrage..." << std::endl;

    // Profitable self loop: USD -> USD @ 1.05
    CurrencyGraph loop_graph;
    loop_graph.add_edge("USD", "USD", 1.05);

    auto result = detect_arbitrage(loop_graph);
    assert(result.has_value());

    const auto& cycle = result.value();
    assert(cycle.path.size() == 2);
    assert(cycle.path[0] == "USD" && cycle.path[1] == "USD");
    assert(cycle.edge_indices.size() == 1);
    assert(cycle.edge_indices[0] == 0);
    assert(std::abs(cycle.compound_multiplier - 1.05) < 1e-9);
    assert(std::abs(cycle.profit_percentage - 5.0) < 1e-9);

    // Unprofitable self loop: EUR -> EUR @ 0.98
    CurrencyGraph loss_loop;
    loss_loop.add_edge("EUR", "EUR", 0.98);
    assert(!detect_arbitrage(loss_loop).has_value());

    std::cout << "[PASS] test_self_loop_arbitrage" << std::endl;
}

// Regression Test 2: Parallel edges selection (must select the edge with higher rate)
void test_parallel_edges_selection() {
    std::cout << "[RUN] test_parallel_edges_selection..." << std::endl;
    CurrencyGraph graph;

    // Edge 0: A -> B @ 1.10
    // Edge 1: A -> B @ 1.25 (parallel edge with better rate)
    // Edge 2: B -> A @ 0.90
    // Expected optimal cycle: Edge 1 and Edge 2 -> 1.25 * 0.90 = 1.125 (+12.5%)
    // (Suboptimal cycle with Edge 0: 1.10 * 0.90 = 0.99 < 1.0, not profitable)
    graph.add_edge("CUR_A", "CUR_B", 1.10);
    graph.add_edge("CUR_A", "CUR_B", 1.25);
    graph.add_edge("CUR_B", "CUR_A", 0.90);

    auto result = detect_arbitrage(graph);
    assert(result.has_value());

    const auto& cycle = result.value();
    assert(cycle.edge_indices.size() == 2);
    // Must select edge index 1 (1.25 rate) and edge index 2 (0.90 rate), never edge index 0 (1.10 rate)
    bool has_edge_1 = (cycle.edge_indices[0] == 1 || cycle.edge_indices[1] == 1);
    bool has_edge_2 = (cycle.edge_indices[0] == 2 || cycle.edge_indices[1] == 2);
    bool has_edge_0 = (cycle.edge_indices[0] == 0 || cycle.edge_indices[1] == 0);
    assert(has_edge_1 && has_edge_2);
    assert(!has_edge_0);
    assert(std::abs(cycle.compound_multiplier - 1.125) < 1e-9);
    assert(std::abs(cycle.profit_percentage - 12.5) < 1e-9);

    std::cout << "[PASS] test_parallel_edges_selection" << std::endl;
}

// Regression Test 3: Very small and very large rates
void test_extreme_rates() {
    std::cout << "[RUN] test_extreme_rates..." << std::endl;
    CurrencyGraph graph;

    // Rate 1: 1e-8 (e.g. micro-crypto to USD)
    // Rate 2: 2e8
    // Multiplier: 1e-8 * 2e8 = 2.0 (+100% profit)
    graph.add_edge("SATOSHI", "FIAT", 1e-8);
    graph.add_edge("FIAT", "SATOSHI", 2e8);

    auto result = detect_arbitrage(graph);
    assert(result.has_value());

    const auto& cycle = result.value();
    assert(std::abs(cycle.compound_multiplier - 2.0) < 1e-9);
    assert(std::abs(cycle.profit_percentage - 100.0) < 1e-9);

    std::cout << "[PASS] test_extreme_rates" << std::endl;
}

// Regression Test 4: Multi-candidate recovery when one cycle is below threshold
void test_multi_candidate_shadowed_arbitrage() {
    std::cout << "[RUN] test_multi_candidate_shadowed_arbitrage..." << std::endl;
    CurrencyGraph graph;

    // Component 1: Tiny cycle (e.g. 0.001% profit)
    // 1.00001
    graph.add_edge("T1", "T2", 1.00001);
    graph.add_edge("T2", "T1", 1.0);

    // Component 2: Significant cycle (20% profit)
    // 1.20
    graph.add_edge("S1", "S2", 1.20);
    graph.add_edge("S2", "S1", 1.0);

    // Set threshold at 5% (0.05). Component 1 should be skipped, Component 2 returned!
    auto result = detect_arbitrage(graph, 0.05);
    assert(result.has_value());

    const auto& cycle = result.value();
    assert(cycle.profit_percentage >= 19.9);
    assert(cycle.path[0] == "S1" || cycle.path[0] == "S2");

    std::cout << "[PASS] test_multi_candidate_shadowed_arbitrage" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Starting ArbitrageDetector Unit Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    test_insufficient_graph();
    test_no_arbitrage_equilibrium();
    test_profitable_arbitrage_3cycle();
    test_disconnected_component_arbitrage();
    test_multiple_currencies_and_edges();
    test_path_reconstruction_and_precision();
    test_self_loop_arbitrage();
    test_parallel_edges_selection();
    test_extreme_rates();
    test_multi_candidate_shadowed_arbitrage();

    std::cout << "========================================" << std::endl;
    std::cout << "All ArbitrageDetector Tests Passed!" << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
