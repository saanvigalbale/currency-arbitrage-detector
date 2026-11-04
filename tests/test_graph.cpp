#include "CurrencyGraph.hpp"
#include "ArbitrageCycle.hpp"

#include <iostream>
#include <cassert>
#include <cmath>
#include <limits>

void test_currency_registration() {
    std::cout << "[RUN] test_currency_registration..." << std::endl;
    CurrencyGraph graph;

    assert(graph.vertex_count() == 0);
    assert(!graph.has_currency("USD"));

    std::size_t usd_id = graph.get_or_register_currency("USD");
    assert(usd_id == 0);
    assert(graph.vertex_count() == 1);
    assert(graph.has_currency("USD"));
    assert(graph.get_currency_code(usd_id) == "USD");

    // Registering the same currency returns the existing ID without incrementing count
    std::size_t usd_id_again = graph.get_or_register_currency("USD");
    assert(usd_id == usd_id_again);
    assert(graph.vertex_count() == 1);

    // Register a second currency
    std::size_t eur_id = graph.get_or_register_currency("EUR");
    assert(eur_id == 1);
    assert(graph.vertex_count() == 2);
    assert(graph.get_currency_code(eur_id) == "EUR");

    // Check optional lookup
    auto opt_id = graph.get_currency_id("USD");
    assert(opt_id.has_value() && opt_id.value() == 0);

    auto missing = graph.get_currency_id("NONEXISTENT");
    assert(!missing.has_value());

    // Empty code must throw
    bool threw_empty = false;
    try {
        graph.get_or_register_currency("");
    } catch (const std::invalid_argument&) {
        threw_empty = true;
    }
    assert(threw_empty);

    std::cout << "[PASS] test_currency_registration" << std::endl;
}

void test_rate_validation() {
    std::cout << "[RUN] test_rate_validation..." << std::endl;
    CurrencyGraph graph;

    // Test static validator directly
    assert(CurrencyGraph::is_valid_rate(1.0));
    assert(CurrencyGraph::is_valid_rate(0.0001));
    assert(CurrencyGraph::is_valid_rate(150000.5));
    assert(!CurrencyGraph::is_valid_rate(0.0));
    assert(!CurrencyGraph::is_valid_rate(-1.5));
    assert(!CurrencyGraph::is_valid_rate(std::numeric_limits<double>::quiet_NaN()));
    assert(!CurrencyGraph::is_valid_rate(std::numeric_limits<double>::infinity()));
    assert(!CurrencyGraph::is_valid_rate(-std::numeric_limits<double>::infinity()));

    // Test add_edge throws on invalid rates
    auto expect_throw = [&](const std::string& from, const std::string& to, double rate) {
        bool threw = false;
        try {
            graph.add_edge(from, to, rate);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        assert(threw);
    };

    expect_throw("A", "B", 0.0);
    expect_throw("A", "B", -0.95);
    expect_throw("A", "B", std::numeric_limits<double>::quiet_NaN());
    expect_throw("A", "B", std::numeric_limits<double>::infinity());
    expect_throw("A", "B", -std::numeric_limits<double>::infinity());

    std::cout << "[PASS] test_rate_validation" << std::endl;
}

void test_edge_storage_and_adjacency() {
    std::cout << "[RUN] test_edge_storage_and_adjacency..." << std::endl;
    CurrencyGraph graph;

    // Add directed edge: AAA -> BBB at 1.25
    std::size_t e0 = graph.add_edge("AAA", "BBB", 1.25);
    assert(e0 == 0);
    assert(graph.edge_count() == 1);
    assert(graph.vertex_count() == 2);

    const Edge& edge0 = graph.get_edge(e0);
    assert(edge0.from == 0);
    assert(edge0.to == 1);
    assert(edge0.original_rate == 1.25);
    assert(std::abs(edge0.weight - (-std::log(1.25))) < 1e-12);

    // Add opposite direction with different rate (simulating bid/ask spread)
    std::size_t e1 = graph.add_edge("BBB", "AAA", 0.78);
    assert(e1 == 1);
    assert(graph.edge_count() == 2);

    const Edge& edge1 = graph.get_edge(e1);
    assert(edge1.from == 1);
    assert(edge1.to == 0);
    assert(edge1.original_rate == 0.78);
    assert(std::abs(edge1.weight - (-std::log(0.78))) < 1e-12);

    // Verify adjacency list contains indices, not duplicated Edge objects
    const auto& outgoing_from_aaa = graph.get_outgoing_edge_indices(0);
    assert(outgoing_from_aaa.size() == 1);
    assert(outgoing_from_aaa[0] == 0);

    const auto& outgoing_from_bbb = graph.get_outgoing_edge_indices(1);
    assert(outgoing_from_bbb.size() == 1);
    assert(outgoing_from_bbb[0] == 1);

    // Add a second outgoing edge from AAA to a new currency CCC
    std::size_t e2 = graph.add_edge("AAA", "CCC", 2.50);
    assert(e2 == 2);
    assert(graph.edge_count() == 3);
    assert(graph.vertex_count() == 3);

    const auto& updated_outgoing_aaa = graph.get_outgoing_edge_indices(0);
    assert(updated_outgoing_aaa.size() == 2);
    assert(updated_outgoing_aaa[0] == 0);
    assert(updated_outgoing_aaa[1] == 2);

    // Verify single storage: all edges accessible via get_edges()
    const auto& all_edges = graph.get_edges();
    assert(all_edges.size() == 3);
    assert(all_edges[0].original_rate == 1.25);
    assert(all_edges[1].original_rate == 0.78);
    assert(all_edges[2].original_rate == 2.50);

    std::cout << "[PASS] test_edge_storage_and_adjacency" << std::endl;
}

void test_arbitrage_cycle_struct() {
    std::cout << "[RUN] test_arbitrage_cycle_struct..." << std::endl;
    ArbitrageCycle cycle;
    cycle.path = {"X", "Y", "Z", "X"};
    cycle.edge_indices = {0, 1, 2};
    cycle.compound_multiplier = 1.05;
    cycle.profit_percentage = 5.0;

    assert(cycle.path.size() == 4);
    assert(cycle.edge_indices.size() == 3);
    assert(cycle.compound_multiplier == 1.05);
    assert(cycle.profit_percentage == 5.0);

    std::cout << "[PASS] test_arbitrage_cycle_struct" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Starting CurrencyGraph Unit Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    test_currency_registration();
    test_rate_validation();
    test_edge_storage_and_adjacency();
    test_arbitrage_cycle_struct();

    std::cout << "========================================" << std::endl;
    std::cout << "All Unit Tests Passed Successfully!" << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
