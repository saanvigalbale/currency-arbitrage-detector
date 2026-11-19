#include "AnalysisService.hpp"

#include "ArbitrageDetector.hpp"
#include "CurrencyGraph.hpp"
#include "JsonWriter.hpp"
#include "RateLoader.hpp"

#include <cmath>
#include <fstream>
#include <functional>

namespace {

bool is_valid_capital(double capital) {
    return std::isfinite(capital) && capital > 0.0;
}

AnalysisResult invalid_capital_result() {
    return AnalysisService::make_error(
        "input", "Starting capital must be a strictly positive number."
    );
}

AnalysisResult analyze_with_loader(
    double starting_capital,
    const std::function<std::size_t(CurrencyGraph&)>& load
) {
    if (!is_valid_capital(starting_capital)) {
        return invalid_capital_result();
    }

    CurrencyGraph graph;
    std::size_t rates_loaded = 0;
    try {
        rates_loaded = load(graph);
    } catch (const RateLoaderException& ex) {
        return AnalysisService::make_error("csv", ex.message(), ex.line_number(), starting_capital);
    } catch (const std::exception& ex) {
        return AnalysisService::make_error("internal", ex.what(), 0, starting_capital);
    }

    AnalysisResult result;
    result.currency_count = graph.vertex_count();
    result.rate_count = rates_loaded;
    result.starting_capital = starting_capital;

    const auto cycle_opt = ArbitrageDetector::detect_arbitrage(graph);
    if (!cycle_opt.has_value()) {
        result.status = AnalysisStatus::NoArbitrage;
        return result;
    }

    const ArbitrageCycle& cycle = cycle_opt.value();
    AnalysisDetails details;
    details.currency = cycle.path.empty() ? std::string() : cycle.path.front();
    details.cycle = cycle.path;
    details.multiplier = cycle.compound_multiplier;
    details.profit_percent = cycle.profit_percentage;
    details.starting_capital = starting_capital;

    double amount = starting_capital;
    for (std::size_t edge_index : cycle.edge_indices) {
        const Edge& edge = graph.get_edge(edge_index);
        AnalysisStep step;
        step.from = graph.get_currency_code(edge.from);
        step.to = graph.get_currency_code(edge.to);
        step.rate = edge.original_rate;
        step.amount_before = amount;
        amount *= edge.original_rate;
        step.amount_after = amount;
        details.steps.push_back(std::move(step));
    }
    details.final_capital = amount;
    details.profit = amount - starting_capital;

    result.status = AnalysisStatus::Arbitrage;
    result.analysis = std::move(details);
    return result;
}

const char* status_name(AnalysisStatus status) {
    switch (status) {
        case AnalysisStatus::Arbitrage:   return "arbitrage";
        case AnalysisStatus::NoArbitrage: return "no_arbitrage";
        case AnalysisStatus::Error:       return "error";
    }
    return "error";
}

} // anonymous namespace

AnalysisResult AnalysisService::analyze_stream(std::istream& csv, double starting_capital) {
    return analyze_with_loader(starting_capital, [&csv](CurrencyGraph& graph) {
        return RateLoader::load_from_stream(csv, graph);
    });
}

AnalysisResult AnalysisService::analyze_file(const std::string& path, double starting_capital) {
    return analyze_with_loader(starting_capital, [&path](CurrencyGraph& graph) {
        return RateLoader::load_from_csv(path, graph);
    });
}

AnalysisResult AnalysisService::make_error(
    const std::string& type,
    const std::string& message,
    std::size_t line,
    double starting_capital
) {
    AnalysisResult result;
    result.status = AnalysisStatus::Error;
    result.starting_capital = starting_capital;
    result.error = AnalysisError{type, message, line};
    return result;
}

std::string AnalysisService::to_json(const AnalysisResult& result, int indent) {
    JsonWriter w(indent);
    w.begin_object();
    w.key("status").string(status_name(result.status));

    w.key("meta").begin_object();
    w.key("currencies");
    if (result.currency_count) w.integer(*result.currency_count); else w.null();
    w.key("rates");
    if (result.rate_count) w.integer(*result.rate_count); else w.null();
    w.key("starting_capital").number(result.starting_capital);  // null when NaN
    w.end_object();

    w.key("analysis");
    if (result.analysis) {
        const AnalysisDetails& a = *result.analysis;
        w.begin_object();
        w.key("currency").string(a.currency);
        w.key("cycle").begin_array();
        for (const auto& code : a.cycle) w.string(code);
        w.end_array();
        w.key("steps").begin_array();
        for (std::size_t i = 0; i < a.steps.size(); ++i) {
            const AnalysisStep& s = a.steps[i];
            w.begin_object();
            w.key("step").integer(i + 1);
            w.key("from").string(s.from);
            w.key("to").string(s.to);
            w.key("rate").number(s.rate);
            w.key("amount_before").number(s.amount_before);
            w.key("amount_after").number(s.amount_after);
            w.end_object();
        }
        w.end_array();
        w.key("multiplier").number(a.multiplier);
        w.key("profit_percent").number(a.profit_percent);
        w.key("starting_capital").number(a.starting_capital);
        w.key("final_capital").number(a.final_capital);
        w.key("profit").number(a.profit);
        w.end_object();
    } else {
        w.null();
    }

    w.key("error");
    if (result.error) {
        w.begin_object();
        w.key("type").string(result.error->type);
        w.key("line");
        if (result.error->line > 0) w.integer(result.error->line); else w.null();
        w.key("message").string(result.error->message);
        w.end_object();
    } else {
        w.null();
    }

    w.end_object();
    return w.str();
}
