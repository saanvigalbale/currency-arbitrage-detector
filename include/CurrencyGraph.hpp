#ifndef CURRENCY_ARBITRAGE_GRAPH_HPP
#define CURRENCY_ARBITRAGE_GRAPH_HPP

#include "Edge.hpp"

#include <vector>
#include <string>
#include <unordered_map>
#include <optional>
#include <stdexcept>
#include <cstddef>

/**
 * @brief Directed graph where vertices represent currencies and edges represent exchange rates.
 */
class CurrencyGraph {
public:
    CurrencyGraph() = default;

    /**
     * @brief Registers a currency code dynamically if not already present.
     * @param code Currency symbol/code (e.g., "USD", "EUR", "BTC").
     * @return Internal zero-based vertex ID assigned to the currency.
     * @throws std::invalid_argument if currency code is empty.
     */
    std::size_t get_or_register_currency(const std::string& code);

    /**
     * @brief Gets the internal vertex ID for a currency code if registered.
     * @param code Currency symbol/code to look up.
     * @return Optional containing vertex ID if found, std::nullopt otherwise.
     */
    std::optional<std::size_t> get_currency_id(const std::string& code) const noexcept;

    /**
     * @brief Gets the currency symbol/code for a given vertex ID.
     * @param id Vertex ID.
     * @return Reference to the currency code string.
     * @throws std::out_of_range if id is out of bounds.
     */
    const std::string& get_currency_code(std::size_t id) const;

    /**
     * @brief Checks if a currency symbol is registered.
     * @param code Currency symbol to check.
     */
    bool has_currency(const std::string& code) const noexcept;

    /**
     * @brief Validates an exchange rate.
     * Rejects zero, negative, NaN, and infinite values.
     * @param rate Value to validate.
     * @return true if valid (finite and strictly positive), false otherwise.
     */
    static bool is_valid_rate(double rate) noexcept;

    /**
     * @brief Adds a directed edge between currencies specified by code.
     * Automatically registers currencies if not already present.
     * @param from_code Source currency code.
     * @param to_code Destination currency code.
     * @param rate Conversion exchange rate (1 unit of from_code = rate units of to_code).
     * @return The index of the newly added edge in the edge list.
     * @throws std::invalid_argument if rate is non-positive, NaN, infinite, or code is empty.
     */
    std::size_t add_edge(const std::string& from_code, const std::string& to_code, double rate);

    /**
     * @brief Adds a directed edge between currencies specified by existing vertex IDs.
     * @param from_id Source currency vertex ID.
     * @param to_id Destination currency vertex ID.
     * @param rate Conversion exchange rate.
     * @return The index of the newly added edge in the edge list.
     * @throws std::out_of_range if either vertex ID is invalid.
     * @throws std::invalid_argument if rate is non-positive, NaN, or infinite.
     */
    std::size_t add_edge(std::size_t from_id, std::size_t to_id, double rate);

    /**
     * @brief Returns the total number of registered currency vertices.
     */
    std::size_t vertex_count() const noexcept;

    /**
     * @brief Returns the total number of edges stored in the graph.
     */
    std::size_t edge_count() const noexcept;

    /**
     * @brief Returns the master list of all edges.
     */
    const std::vector<Edge>& get_edges() const noexcept;

    /**
     * @brief Retrieves a specific edge by its edge index.
     * @param edge_index Index into the edge list.
     * @throws std::out_of_range if edge_index is invalid.
     */
    const Edge& get_edge(std::size_t edge_index) const;

    /**
     * @brief Returns outgoing edge indices for a given vertex.
     * @param vertex_id Vertex ID.
     * @throws std::out_of_range if vertex_id is invalid.
     */
    const std::vector<std::size_t>& get_outgoing_edge_indices(std::size_t vertex_id) const;

    /**
     * @brief Clears all vertices and edges from the graph.
     */
    void clear() noexcept;

private:
    std::unordered_map<std::string, std::size_t> currency_to_id_;
    std::vector<std::string> id_to_currency_;
    std::vector<Edge> edges_;
    std::vector<std::vector<std::size_t>> adj_; // adj_[u] stores indices into edges_
};

#endif // CURRENCY_ARBITRAGE_GRAPH_HPP
