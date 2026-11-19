// Keep assert() active even in Release builds (CMAKE_BUILD_TYPE=Release defines NDEBUG),
// otherwise every check in this file would silently compile to nothing.
#undef NDEBUG
#include "CurrencyGraph.hpp"
#include "RateLoader.hpp"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <cassert>
#include <cmath>
#include <sstream>

namespace {

struct TempCSVFile {
    std::string path;

    TempCSVFile(const std::string& filename, const std::string& content) {
        path = (std::filesystem::temp_directory_path() / filename).string();
        std::ofstream out(path, std::ios::binary);
        out << content;
    }

    ~TempCSVFile() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
};

} // anonymous namespace

void test_valid_csv() {
    std::cout << "[RUN] test_valid_csv..." << std::endl;
    TempCSVFile temp("test_valid.csv",
        "USD,EUR,0.92\n"
        "EUR,GBP,0.85\n"
        "GBP,USD,1.28\n"
        "USD,JPY,150.5\n"
    );

    CurrencyGraph graph;
    std::size_t loaded = RateLoader::load_from_csv(temp.path, graph);

    assert(loaded == 4);
    assert(graph.vertex_count() == 4);
    assert(graph.edge_count() == 4);
    assert(graph.has_currency("USD"));
    assert(graph.has_currency("EUR"));
    assert(graph.has_currency("GBP"));
    assert(graph.has_currency("JPY"));

    std::cout << "[PASS] test_valid_csv" << std::endl;
}

void test_header_handling() {
    std::cout << "[RUN] test_header_handling..." << std::endl;
    TempCSVFile temp("test_header.csv",
        "source_currency,target_currency,rate\n"
        "USD,EUR,0.92\n"
        "EUR,GBP,0.85\n"
    );

    CurrencyGraph graph;
    std::size_t loaded = RateLoader::load_from_csv(temp.path, graph);

    assert(loaded == 2);
    assert(graph.vertex_count() == 3);
    assert(graph.edge_count() == 2);

    std::cout << "[PASS] test_header_handling" << std::endl;
}

void test_whitespace_around_fields() {
    std::cout << "[RUN] test_whitespace_around_fields..." << std::endl;
    TempCSVFile temp("test_ws.csv",
        "  USD  ,   EUR   ,   0.9200   \r\n"
        "\tEUR\t,\tGBP\t,\t0.85\t\n"
    );

    CurrencyGraph graph;
    std::size_t loaded = RateLoader::load_from_csv(temp.path, graph);

    assert(loaded == 2);
    assert(graph.has_currency("USD"));
    assert(graph.has_currency("EUR"));
    assert(graph.has_currency("GBP"));

    const Edge& e0 = graph.get_edge(0);
    assert(e0.original_rate == 0.92);

    std::cout << "[PASS] test_whitespace_around_fields" << std::endl;
}

void test_blank_lines() {
    std::cout << "[RUN] test_blank_lines..." << std::endl;
    TempCSVFile temp("test_blanks.csv",
        "\n"
        "   \t  \n"
        "source,target,rate\n"
        "\n"
        "USD,EUR,0.92\n"
        "\n"
        " \r\n"
        "EUR,GBP,0.85\n"
        "\n"
    );

    CurrencyGraph graph;
    std::size_t loaded = RateLoader::load_from_csv(temp.path, graph);

    assert(loaded == 2);
    assert(graph.edge_count() == 2);

    std::cout << "[PASS] test_blank_lines" << std::endl;
}

void test_invalid_number_of_columns() {
    std::cout << "[RUN] test_invalid_number_of_columns..." << std::endl;

    // Too few columns on line 2
    {
        TempCSVFile temp("test_too_few.csv",
            "USD,EUR,0.92\n"
            "EUR,GBP\n"
        );
        CurrencyGraph graph;
        bool threw = false;
        try {
            RateLoader::load_from_csv(temp.path, graph);
        } catch (const RateLoaderException& ex) {
            threw = true;
            assert(ex.line_number() == 2);
        }
        assert(threw);
    }

    // Too many columns on line 1
    {
        TempCSVFile temp("test_too_many.csv",
            "USD,EUR,0.92,EXTRA_FIELD\n"
        );
        CurrencyGraph graph;
        bool threw = false;
        try {
            RateLoader::load_from_csv(temp.path, graph);
        } catch (const RateLoaderException& ex) {
            threw = true;
            assert(ex.line_number() == 1);
        }
        assert(threw);
    }

    std::cout << "[PASS] test_invalid_number_of_columns" << std::endl;
}

void test_invalid_currency_codes() {
    std::cout << "[RUN] test_invalid_currency_codes..." << std::endl;

    // Empty source currency on line 1
    {
        TempCSVFile temp("test_empty_src.csv",
            ",EUR,0.92\n"
        );
        CurrencyGraph graph;
        bool threw = false;
        try {
            RateLoader::load_from_csv(temp.path, graph);
        } catch (const RateLoaderException& ex) {
            threw = true;
            assert(ex.line_number() == 1);
        }
        assert(threw);
    }

    // Disallowed character in target currency on line 2
    {
        TempCSVFile temp("test_bad_char.csv",
            "USD,EUR,0.92\n"
            "EUR,GBP$,0.85\n"
        );
        CurrencyGraph graph;
        bool threw = false;
        try {
            RateLoader::load_from_csv(temp.path, graph);
        } catch (const RateLoaderException& ex) {
            threw = true;
            assert(ex.line_number() == 2);
        }
        assert(threw);
    }

    std::cout << "[PASS] test_invalid_currency_codes" << std::endl;
}

void test_zero_and_negative_rates() {
    std::cout << "[RUN] test_zero_and_negative_rates..." << std::endl;

    // Zero rate on line 1
    {
        TempCSVFile temp("test_zero_rate.csv",
            "USD,EUR,0.0\n"
        );
        CurrencyGraph graph;
        bool threw = false;
        try {
            RateLoader::load_from_csv(temp.path, graph);
        } catch (const RateLoaderException& ex) {
            threw = true;
            assert(ex.line_number() == 1);
        }
        assert(threw);
    }

    // Negative rate on line 2
    {
        TempCSVFile temp("test_neg_rate.csv",
            "USD,EUR,0.92\n"
            "EUR,GBP,-0.85\n"
        );
        CurrencyGraph graph;
        bool threw = false;
        try {
            RateLoader::load_from_csv(temp.path, graph);
        } catch (const RateLoaderException& ex) {
            threw = true;
            assert(ex.line_number() == 2);
        }
        assert(threw);
    }

    std::cout << "[PASS] test_zero_and_negative_rates" << std::endl;
}

void test_nan_and_infinity() {
    std::cout << "[RUN] test_nan_and_infinity..." << std::endl;

    // NaN rate on line 2
    {
        TempCSVFile temp("test_nan.csv",
            "USD,EUR,0.92\n"
            "EUR,GBP,NaN\n"
        );
        CurrencyGraph graph;
        bool threw = false;
        try {
            RateLoader::load_from_csv(temp.path, graph);
        } catch (const RateLoaderException& ex) {
            threw = true;
            assert(ex.line_number() == 2);
        }
        assert(threw);
    }

    // Infinity rate on line 1
    {
        TempCSVFile temp("test_inf.csv",
            "USD,EUR,inf\n"
        );
        CurrencyGraph graph;
        bool threw = false;
        try {
            RateLoader::load_from_csv(temp.path, graph);
        } catch (const RateLoaderException& ex) {
            threw = true;
            assert(ex.line_number() == 1);
        }
        assert(threw);
    }

    std::cout << "[PASS] test_nan_and_infinity" << std::endl;
}

void test_missing_file() {
    std::cout << "[RUN] test_missing_file..." << std::endl;
    CurrencyGraph graph;
    bool threw = false;
    try {
        RateLoader::load_from_csv("non_existent_file_xyz_123.csv", graph);
    } catch (const RateLoaderException& ex) {
        threw = true;
        assert(ex.line_number() == 0);
    }
    assert(threw);

    std::cout << "[PASS] test_missing_file" << std::endl;
}

void test_multiple_edges_same_currencies() {
    std::cout << "[RUN] test_multiple_edges_same_currencies..." << std::endl;
    TempCSVFile temp("test_multi_edges.csv",
        "USD,EUR,0.91\n"
        "USD,EUR,0.93\n"
        "EUR,USD,1.08\n"
        "EUR,USD,1.10\n"
    );

    CurrencyGraph graph;
    std::size_t loaded = RateLoader::load_from_csv(temp.path, graph);

    assert(loaded == 4);
    assert(graph.vertex_count() == 2);
    assert(graph.edge_count() == 4);

    auto usd_id = graph.get_currency_id("USD").value();
    const auto& outgoing_usd = graph.get_outgoing_edge_indices(usd_id);
    assert(outgoing_usd.size() == 2);
    assert(graph.get_edge(outgoing_usd[0]).original_rate == 0.91);
    assert(graph.get_edge(outgoing_usd[1]).original_rate == 0.93);

    std::cout << "[PASS] test_multiple_edges_same_currencies" << std::endl;
}

void test_empty_csv() {
    std::cout << "[RUN] test_empty_csv..." << std::endl;

    // Completely empty file: must throw, line_number 0 (file-level)
    TempCSVFile temp("test_empty.csv", "");
    CurrencyGraph graph;
    bool threw = false;
    try {
        RateLoader::load_from_csv(temp.path, graph);
    } catch (const RateLoaderException& ex) {
        threw = true;
        assert(ex.line_number() == 0);
        // Message must mention the absence of valid rows
        const std::string msg(ex.what());
        assert(msg.find("No valid exchange-rate rows found") != std::string::npos);
    }
    assert(threw);
    assert(graph.edge_count() == 0);

    std::cout << "[PASS] test_empty_csv" << std::endl;
}

void test_header_only_csv() {
    std::cout << "[RUN] test_header_only_csv..." << std::endl;

    // File contains only the header row: must throw, line_number 0 (file-level)
    TempCSVFile temp("test_header_only.csv",
        "source_currency,target_currency,rate\n"
    );
    CurrencyGraph graph;
    bool threw = false;
    try {
        RateLoader::load_from_csv(temp.path, graph);
    } catch (const RateLoaderException& ex) {
        threw = true;
        assert(ex.line_number() == 0);
        const std::string msg(ex.what());
        assert(msg.find("No valid exchange-rate rows found") != std::string::npos);
    }
    assert(threw);
    assert(graph.edge_count() == 0);

    // Blank-lines-only file (not empty bytes, but no data) must also throw
    TempCSVFile temp_blanks("test_blanks_only.csv", "\n\n   \n\t\n");
    CurrencyGraph graph2;
    bool threw2 = false;
    try {
        RateLoader::load_from_csv(temp_blanks.path, graph2);
    } catch (const RateLoaderException& ex) {
        threw2 = true;
        assert(ex.line_number() == 0);
    }
    assert(threw2);

    std::cout << "[PASS] test_header_only_csv" << std::endl;
}

void test_utf8_bom_with_header() {
    std::cout << "[RUN] test_utf8_bom_with_header..." << std::endl;
    TempCSVFile temp("test_bom_header.csv",
        "\xEF\xBB\xBF" "source_currency,target_currency,rate\r\n"
        "USD,EUR,0.92\r\n"
        "EUR,GBP,0.85\r\n"
    );

    CurrencyGraph graph;
    std::size_t loaded = RateLoader::load_from_csv(temp.path, graph);

    assert(loaded == 2);
    assert(graph.vertex_count() == 3);
    assert(graph.edge_count() == 2);
    assert(graph.has_currency("USD"));
    assert(graph.has_currency("EUR"));
    assert(graph.has_currency("GBP"));

    const auto usd = graph.get_currency_id("USD").value();
    const auto eur = graph.get_currency_id("EUR").value();
    const auto gbp = graph.get_currency_id("GBP").value();

    const Edge& e0 = graph.get_edge(0);
    assert(e0.from == usd && e0.to == eur && e0.original_rate == 0.92);
    const Edge& e1 = graph.get_edge(1);
    assert(e1.from == eur && e1.to == gbp && e1.original_rate == 0.85);

    std::cout << "[PASS] test_utf8_bom_with_header" << std::endl;
}

void test_utf8_bom_without_header() {
    std::cout << "[RUN] test_utf8_bom_without_header..." << std::endl;
    // BOM glued to the first currency code of a headerless file
    TempCSVFile temp("test_bom_no_header.csv",
        "\xEF\xBB\xBF" "USD,EUR,0.92\n"
        "EUR,GBP,0.85\n"
    );

    CurrencyGraph graph;
    std::size_t loaded = RateLoader::load_from_csv(temp.path, graph);

    assert(loaded == 2);
    assert(graph.vertex_count() == 3);
    // Currency must be exactly "USD", not "\xEF\xBB\xBFUSD"
    assert(graph.has_currency("USD"));
    assert(graph.get_edge(0).original_rate == 0.92);
    assert(graph.get_edge(0).from == graph.get_currency_id("USD").value());

    std::cout << "[PASS] test_utf8_bom_without_header" << std::endl;
}

void test_utf8_bom_only_stripped_at_file_start() {
    std::cout << "[RUN] test_utf8_bom_only_stripped_at_file_start..." << std::endl;

    // A BOM on line 2 is data, not a file marker: it must still be rejected,
    // and the error must report line 2 (line numbering unaffected by the BOM on line 1).
    TempCSVFile temp("test_bom_midfile.csv",
        "\xEF\xBB\xBF" "USD,EUR,0.92\n"
        "\xEF\xBB\xBF" "EUR,GBP,0.85\n"
    );
    CurrencyGraph graph;
    bool threw = false;
    try {
        RateLoader::load_from_csv(temp.path, graph);
    } catch (const RateLoaderException& ex) {
        threw = true;
        assert(ex.line_number() == 2);
    }
    assert(threw);

    // A BOM-only first line is just a blank line; later lines keep their numbers.
    TempCSVFile temp_blank("test_bom_blank_first.csv",
        "\xEF\xBB\xBF" "\n"
        "USD,EUR,0.92\n"
        "EUR,GBP,abc\n"
    );
    CurrencyGraph graph2;
    bool threw2 = false;
    try {
        RateLoader::load_from_csv(temp_blank.path, graph2);
    } catch (const RateLoaderException& ex) {
        threw2 = true;
        assert(ex.line_number() == 3);
    }
    assert(threw2);

    std::cout << "[PASS] test_utf8_bom_only_stripped_at_file_start" << std::endl;
}

// ---------------------------------------------------------------------------
// Stream loader, header detection and message() tests
// ---------------------------------------------------------------------------

namespace {

// Runs the stream loader and returns the exception (or nullptr-equivalent via flag).
struct LoadOutcome {
    bool threw{false};
    std::size_t line{0};
    std::string what;
    std::string message;
    std::size_t loaded{0};
};

LoadOutcome load_stream_text(const std::string& text, CurrencyGraph& graph) {
    LoadOutcome out;
    std::istringstream in(text);
    try {
        out.loaded = RateLoader::load_from_stream(in, graph);
    } catch (const RateLoaderException& ex) {
        out.threw = true;
        out.line = ex.line_number();
        out.what = ex.what();
        out.message = ex.message();
    }
    return out;
}

} // anonymous namespace

void test_stream_with_header() {
    std::cout << "[RUN] test_stream_with_header..." << std::endl;
    CurrencyGraph graph;
    auto r = load_stream_text(
        "source_currency,target_currency,rate\n"
        "USD,EUR,0.92\n"
        "EUR,GBP,0.85\n", graph);
    assert(!r.threw);
    assert(r.loaded == 2);
    assert(graph.edge_count() == 2 && graph.vertex_count() == 3);
    assert(graph.get_edge(0).original_rate == 0.92);
    std::cout << "[PASS] test_stream_with_header" << std::endl;
}

void test_stream_without_header() {
    std::cout << "[RUN] test_stream_without_header..." << std::endl;
    CurrencyGraph graph;
    auto r = load_stream_text("USD,EUR,0.92\r\nEUR,GBP,0.85\r\n", graph);
    assert(!r.threw);
    assert(r.loaded == 2);
    assert(graph.has_currency("USD") && graph.has_currency("GBP"));
    std::cout << "[PASS] test_stream_without_header" << std::endl;
}

void test_stream_utf8_bom() {
    std::cout << "[RUN] test_stream_utf8_bom..." << std::endl;
    {
        CurrencyGraph graph;
        auto r = load_stream_text("\xEF\xBB\xBF" "source,target,rate\nUSD,EUR,0.92\n", graph);
        assert(!r.threw && r.loaded == 1);
        assert(graph.has_currency("USD") && graph.has_currency("EUR"));
    }
    {
        CurrencyGraph graph;
        auto r = load_stream_text("\xEF\xBB\xBF" "USD,EUR,0.92\nEUR,GBP,0.85\n", graph);
        assert(!r.threw && r.loaded == 2);
        assert(graph.has_currency("USD"));   // exactly "USD", no BOM bytes attached
    }
    {
        // BOM in front of a malformed first row: still a line-1 error, not a header.
        CurrencyGraph graph;
        auto r = load_stream_text("\xEF\xBB\xBF" "USD,EUR,abc\n", graph);
        assert(r.threw && r.line == 1);
    }
    std::cout << "[PASS] test_stream_utf8_bom" << std::endl;
}

void test_stream_matches_file_loader() {
    std::cout << "[RUN] test_stream_matches_file_loader..." << std::endl;
    const std::string content =
        "\xEF\xBB\xBF" "From,To,Price\n"
        "\n"
        "USD,EUR,0.92\n"
        "  EUR , GBP , 0.85 \r\n"
        "GBP,USD,1.28\n";
    TempCSVFile temp("test_stream_vs_file.csv", content);

    CurrencyGraph from_file;
    const std::size_t n_file = RateLoader::load_from_csv(temp.path, from_file);
    CurrencyGraph from_stream;
    auto r = load_stream_text(content, from_stream);

    assert(!r.threw);
    assert(n_file == 3 && r.loaded == 3);
    assert(from_file.vertex_count() == from_stream.vertex_count());
    assert(from_file.edge_count() == from_stream.edge_count());
    for (std::size_t i = 0; i < from_file.edge_count(); ++i) {
        const Edge& a = from_file.get_edge(i);
        const Edge& b = from_stream.get_edge(i);
        assert(a.from == b.from && a.to == b.to && a.original_rate == b.original_rate);
    }
    std::cout << "[PASS] test_stream_matches_file_loader" << std::endl;
}

void test_malformed_first_row_is_reported() {
    std::cout << "[RUN] test_malformed_first_row_is_reported..." << std::endl;

    // Unparseable rate on the very first data row (previously swallowed as a "header").
    {
        CurrencyGraph graph;
        auto r = load_stream_text("USD,EUR,abc\nEUR,GBP,0.85\n", graph);
        assert(r.threw && r.line == 1);
        assert(r.message.find("Cannot parse rate as number: 'abc'") != std::string::npos);
    }
    // Empty rate field on the first row.
    {
        CurrencyGraph graph;
        auto r = load_stream_text("USD,EUR,\n", graph);
        assert(r.threw && r.line == 1);
        assert(r.message.find("Rate field is empty") != std::string::npos);
    }
    // Trailing garbage after a number.
    {
        CurrencyGraph graph;
        auto r = load_stream_text("USD,EUR,0.9x\n", graph);
        assert(r.threw && r.line == 1);
        assert(r.message.find("Trailing non-numeric characters") != std::string::npos);
    }
    // Leading blank lines still count: the bad row is on line 3.
    {
        CurrencyGraph graph;
        auto r = load_stream_text("\n   \nUSD,EUR,abc\n", graph);
        assert(r.threw && r.line == 3);
    }
    // A real header followed by a bad row reports the bad row's line.
    {
        CurrencyGraph graph;
        auto r = load_stream_text("source,target,rate\nUSD,EUR,abc\n", graph);
        assert(r.threw && r.line == 2);
    }
    // Bad currency code on the first row is also a line-1 error.
    {
        CurrencyGraph graph;
        auto r = load_stream_text("US$,EUR,0.9\n", graph);
        assert(r.threw && r.line == 1);
        assert(r.message.find("Invalid source currency code") != std::string::npos);
    }
    // Same behaviour through the file loader.
    {
        TempCSVFile temp("test_first_row_bad_rate.csv", "USD,EUR,abc\n");
        CurrencyGraph graph;
        bool threw = false;
        try {
            RateLoader::load_from_csv(temp.path, graph);
        } catch (const RateLoaderException& ex) {
            threw = true;
            assert(ex.line_number() == 1);
        }
        assert(threw);
    }
    std::cout << "[PASS] test_malformed_first_row_is_reported" << std::endl;
}

void test_valid_header_formats_still_accepted() {
    std::cout << "[RUN] test_valid_header_formats_still_accepted..." << std::endl;
    const char* headers[] = {
        "source_currency,target_currency,rate",
        "source,target,rate",
        "Source Currency,Target Currency,Rate",
        "SOURCE_CURRENCY, TARGET_CURRENCY , EXCHANGE_RATE",
        "from,to,price",
        "from_currency,to_currency,fx_rate",
        "base,quote,rate",
        "base_currency,quote_currency,whatever",
        "Currency A,Currency B,Rate",
    };
    for (const char* h : headers) {
        CurrencyGraph graph;
        auto r = load_stream_text(std::string(h) + "\nUSD,EUR,0.92\n", graph);
        assert(!r.threw);
        assert(r.loaded == 1);
        assert(graph.edge_count() == 1);
    }
    // A valid first data row is not mistaken for a header.
    {
        CurrencyGraph graph;
        auto r = load_stream_text("USD,EUR,0.92\n", graph);
        assert(!r.threw && r.loaded == 1);
    }
    // Header column-count rule is unchanged: header rows still need 3 fields.
    {
        CurrencyGraph graph;
        auto r = load_stream_text("source,target\nUSD,EUR,0.92\n", graph);
        assert(r.threw && r.line == 1);
    }
    std::cout << "[PASS] test_valid_header_formats_still_accepted" << std::endl;
}

void test_exception_message_getter() {
    std::cout << "[RUN] test_exception_message_getter..." << std::endl;
    RateLoaderException with_line("Rate must be strictly positive: '-1'", 7);
    assert(with_line.line_number() == 7);
    assert(with_line.message() == "Rate must be strictly positive: '-1'");
    assert(std::string(with_line.what()) == "CSV Error (line 7): Rate must be strictly positive: '-1'");

    RateLoaderException file_level("Could not open file: x.csv");
    assert(file_level.line_number() == 0);
    assert(file_level.message() == "Could not open file: x.csv");
    assert(std::string(file_level.what()) == "CSV Error: Could not open file: x.csv");

    // Through the loader: message() never carries the prefix, what() always does.
    CurrencyGraph graph;
    auto r = load_stream_text("USD,EUR,0.9\nEUR,GBP,-2\n", graph);
    assert(r.threw && r.line == 2);
    assert(r.message.find("CSV Error") == std::string::npos);
    assert(r.what == "CSV Error (line 2): " + r.message);
    std::cout << "[PASS] test_exception_message_getter" << std::endl;
}

void test_stream_diagnostics_and_line_numbers() {
    std::cout << "[RUN] test_stream_diagnostics_and_line_numbers..." << std::endl;
    {   // wrong field count, line numbers skip over blank lines and CRLF
        CurrencyGraph graph;
        auto r = load_stream_text("USD,EUR,0.9\r\n\r\nEUR,GBP\r\n", graph);
        assert(r.threw && r.line == 3);
        assert(r.message.find("Expected exactly 3 fields") != std::string::npos);
    }
    {   // NaN / inf / zero keep their line numbers
        CurrencyGraph g1, g2, g3;
        assert(load_stream_text("USD,EUR,0.9\nEUR,GBP,NaN\n", g1).line == 2);
        assert(load_stream_text("USD,EUR,inf\n", g2).line == 1);
        assert(load_stream_text("USD,EUR,0.9\nEUR,GBP,0.0\n", g3).line == 2);
    }
    {   // empty / header-only / blank-only: file-level (line 0), no file name in the text
        CurrencyGraph g1, g2, g3;
        auto a = load_stream_text("", g1);
        auto b = load_stream_text("source,target,rate\n", g2);
        auto c = load_stream_text("\n  \n\t\n", g3);
        assert(a.threw && a.line == 0 && b.threw && b.line == 0 && c.threw && c.line == 0);
        assert(a.message.find("No valid exchange-rate rows found") != std::string::npos);
        assert(b.message.find("No valid exchange-rate rows found") != std::string::npos);
    }
    std::cout << "[PASS] test_stream_diagnostics_and_line_numbers" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Starting RateLoader Unit Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    test_valid_csv();
    test_header_handling();
    test_whitespace_around_fields();
    test_blank_lines();
    test_invalid_number_of_columns();
    test_invalid_currency_codes();
    test_zero_and_negative_rates();
    test_nan_and_infinity();
    test_missing_file();
    test_multiple_edges_same_currencies();
    test_empty_csv();
    test_header_only_csv();
    test_utf8_bom_with_header();
    test_utf8_bom_without_header();
    test_utf8_bom_only_stripped_at_file_start();
    test_stream_with_header();
    test_stream_without_header();
    test_stream_utf8_bom();
    test_stream_matches_file_loader();
    test_malformed_first_row_is_reported();
    test_valid_header_formats_still_accepted();
    test_exception_message_getter();
    test_stream_diagnostics_and_line_numbers();

    std::cout << "========================================" << std::endl;
    std::cout << "All RateLoader Tests Passed!" << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
