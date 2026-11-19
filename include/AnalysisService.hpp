#ifndef CURRENCY_ARBITRAGE_ANALYSIS_SERVICE_HPP
#define CURRENCY_ARBITRAGE_ANALYSIS_SERVICE_HPP

#include <cstddef>
#include <istream>
#include <limits>
#include <optional>
#include <string>
#include <vector>

/// Outcome category of one analysis run.
enum class AnalysisStatus {
    Arbitrage,
    NoArbitrage,
    Error
};

/// One conversion in the detected cycle, with the running amount of capital.
struct AnalysisStep {
    std::string from;
    std::string to;
    double rate{0.0};
    double amount_before{0.0};
    double amount_after{0.0};
};

/// Details of a detected arbitrage cycle. All money amounts are denominated in `currency`.
struct AnalysisDetails {
    std::string currency;                ///< Currency of starting/final capital and profit (first cycle node).
    std::vector<std::string> cycle;      ///< e.g. ["USD", "EUR", "GBP", "USD"]
    std::vector<AnalysisStep> steps;
    double multiplier{1.0};              ///< Product of rates around the cycle (from the detector).
    double profit_percent{0.0};          ///< (multiplier - 1) * 100 (from the detector).
    double starting_capital{0.0};
    double final_capital{0.0};           ///< amount_after of the last step.
    double profit{0.0};                  ///< final_capital - starting_capital.
};

/// Why an analysis failed.
struct AnalysisError {
    std::string type;                    ///< "csv", "input", "usage" or "internal".
    std::string message;                 ///< Raw message, no "CSV Error (line N):" prefix.
    std::size_t line{0};                 ///< CSV line number, 0 when not line-specific.
};

struct AnalysisResult {
    AnalysisStatus status{AnalysisStatus::Error};
    std::optional<std::size_t> currency_count;   ///< Set once the CSV loaded successfully.
    std::optional<std::size_t> rate_count;       ///< Set once the CSV loaded successfully.
    double starting_capital{std::numeric_limits<double>::quiet_NaN()}; ///< NaN if invalid/unknown.
    std::optional<AnalysisDetails> analysis;     ///< Only for Arbitrage.
    std::optional<AnalysisError> error;          ///< Only for Error.
};

/**
 * @brief Runs the existing CSV loader and Bellman-Ford detector and packages the outcome
 *        as structured data, plus JSON serialization of that data.
 *
 * Contains no arbitrage logic of its own: detection is ArbitrageDetector::detect_arbitrage,
 * and only the per-step running amounts are derived here from the detected cycle.
 */
class AnalysisService {
public:
    /// Analyzes CSV content read from a stream. Never throws for bad input; failures are
    /// returned as AnalysisStatus::Error.
    static AnalysisResult analyze_stream(std::istream& csv, double starting_capital);

    /// Analyzes a CSV file on disk (same contract as analyze_stream).
    static AnalysisResult analyze_file(const std::string& path, double starting_capital);

    /// Builds an Error result (used for failures that happen before analysis, e.g. bad CLI input).
    static AnalysisResult make_error(
        const std::string& type,
        const std::string& message,
        std::size_t line = 0,
        double starting_capital = std::numeric_limits<double>::quiet_NaN()
    );

    /// Serializes a result to JSON (indent 0 = compact).
    static std::string to_json(const AnalysisResult& result, int indent = 2);
};

#endif // CURRENCY_ARBITRAGE_ANALYSIS_SERVICE_HPP
