#include "CurrencyGraph.hpp"

#include <cmath>
#include <string>

bool CurrencyGraph::is_valid_rate(double rate) noexcept {
    return !std::isnan(rate) && !std::isinf(rate) && rate > 0.0;
}

std::size_t CurrencyGraph::get_or_register_currency(const std::string& code) {
    if (code.empty()) {
        throw std::invalid_argument("Currency code cannot be empty.");
    }

    auto it = currency_to_id_.find(code);
    if (it != currency_to_id_.end()) {
        return it->second;
    }

    const std::size_t new_id = id_to_currency_.size();
    currency_to_id_[code] = new_id;
    id_to_currency_.push_back(code);
    adj_.emplace_back(); // Initialize empty outgoing edge list for this vertex

    return new_id;
}

std::optional<std::size_t> CurrencyGraph::get_currency_id(const std::string& code) const noexcept {
    auto it = currency_to_id_.find(code);
    if (it != currency_to_id_.end()) {
        return it->second;
    }
    return std::nullopt;
}

const std::string& CurrencyGraph::get_currency_code(std::size_t id) const {
    if (id >= id_to_currency_.size()) {
        throw std::out_of_range("Vertex ID out of range: " + std::to_string(id));
    }
    return id_to_currency_[id];
}

bool CurrencyGraph::has_currency(const std::string& code) const noexcept {
    return currency_to_id_.find(code) != currency_to_id_.end();
}

std::size_t CurrencyGraph::add_edge(const std::string& from_code, const std::string& to_code, double rate) {
    if (!is_valid_rate(rate)) {
        throw std::invalid_argument("Exchange rate must be a finite, positive number. Provided: " + std::to_string(rate));
    }

    const std::size_t from_id = get_or_register_currency(from_code);
    const std::size_t to_id = get_or_register_currency(to_code);

    return add_edge(from_id, to_id, rate);
}

std::size_t CurrencyGraph::add_edge(std::size_t from_id, std::size_t to_id, double rate) {
    if (from_id >= id_to_currency_.size()) {
        throw std::out_of_range("Source vertex ID out of range: " + std::to_string(from_id));
    }
    if (to_id >= id_to_currency_.size()) {
        throw std::out_of_range("Destination vertex ID out of range: " + std::to_string(to_id));
    }
    if (!is_valid_rate(rate)) {
        throw std::invalid_argument("Exchange rate must be a finite, positive number. Provided: " + std::to_string(rate));
    }

    const double weight = -std::log(rate);
    const std::size_t edge_index = edges_.size();

    edges_.push_back(Edge{from_id, to_id, rate, weight});
    adj_[from_id].push_back(edge_index);

    return edge_index;
}

std::size_t CurrencyGraph::vertex_count() const noexcept {
    return id_to_currency_.size();
}

std::size_t CurrencyGraph::edge_count() const noexcept {
    return edges_.size();
}

const std::vector<Edge>& CurrencyGraph::get_edges() const noexcept {
    return edges_;
}

const Edge& CurrencyGraph::get_edge(std::size_t edge_index) const {
    if (edge_index >= edges_.size()) {
        throw std::out_of_range("Edge index out of range: " + std::to_string(edge_index));
    }
    return edges_[edge_index];
}

const std::vector<std::size_t>& CurrencyGraph::get_outgoing_edge_indices(std::size_t vertex_id) const {
    if (vertex_id >= adj_.size()) {
        throw std::out_of_range("Vertex ID out of range: " + std::to_string(vertex_id));
    }
    return adj_[vertex_id];
}

void CurrencyGraph::clear() noexcept {
    currency_to_id_.clear();
    id_to_currency_.clear();
    edges_.clear();
    adj_.clear();
}
