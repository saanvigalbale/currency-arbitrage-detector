// Tests for JsonWriter and AnalysisService (structured results + JSON output).
// Uses an always-on CHECK macro (not assert) so the checks also run in Release builds.
#include "AnalysisService.hpp"
#include "JsonWriter.hpp"
#include "mini_json.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int g_checks = 0;
int g_failures = 0;

#define CHECK(cond)                                                                   \
    do {                                                                              \
        ++g_checks;                                                                   \
        if (!(cond)) {                                                                \
            ++g_failures;                                                             \
            std::cout << "  FAIL " << __FILE__ << ":" << __LINE__ << "  " << #cond << std::endl; \
        }                                                                             \
    } while (0)

bool near(double a, double b, double tol = 1e-9) {
    return std::fabs(a - b) <= tol * std::max(1.0, std::fabs(b));
}

template <typename Fn>
bool throws_logic_error(Fn fn) {
    try { fn(); } catch (const std::logic_error&) { return true; } catch (...) { return false; }
    return false;
}

constexpr const char* SAMPLE_CSV =
    "source_currency,target_currency,rate\n"
    "USD,EUR,0.92\n"
    "EUR,GBP,0.85\n"
    "GBP,USD,1.28\n"
    "USD,JPY,150.5\n"
    "JPY,EUR,0.0061\n";

constexpr const char* EQUILIBRIUM_CSV =
    "source_currency,target_currency,rate\n"
    "USD,EUR,0.90\nEUR,USD,1.10\nEUR,GBP,0.85\nGBP,EUR,1.15\nGBP,USD,1.25\nUSD,GBP,0.78\n";

AnalysisResult analyze(const std::string& csv, double capital) {
    std::istringstream in(csv);
    return AnalysisService::analyze_stream(in, capital);
}

minijson::Value json_of(const AnalysisResult& r) {
    return minijson::parse(AnalysisService::to_json(r));
}

// ---------------------------------------------------------------- JsonWriter

void test_writer_structure() {
    std::cout << "[RUN] test_writer_structure" << std::endl;
    JsonWriter w(0);
    w.begin_object();
    w.key("a").integer(1);
    w.key("b").begin_array().boolean(true).null().string("x").number(1.5).end_array();
    w.key("c").begin_object().end_object();
    w.key("d").begin_array().end_array();
    w.end_object();
    CHECK(w.str() == "{\"a\":1,\"b\":[true,null,\"x\",1.5],\"c\":{},\"d\":[]}");

    JsonWriter pretty(2);
    pretty.begin_object();
    pretty.key("k").begin_array().integer(1).integer(2).end_array();
    pretty.end_object();
    const auto parsed = minijson::parse(pretty.str());
    CHECK(parsed.at("k").items.size() == 2 && parsed.at("k").items[1].number == 2.0);
    CHECK(pretty.str().find('\n') != std::string::npos);
    std::cout << "[DONE] test_writer_structure" << std::endl;
}

void test_writer_misuse_throws() {
    std::cout << "[RUN] test_writer_misuse_throws" << std::endl;
    CHECK(throws_logic_error([] { JsonWriter w; w.begin_object(); w.string("no key"); }));
    CHECK(throws_logic_error([] { JsonWriter w; w.begin_array(); w.key("k"); }));
    CHECK(throws_logic_error([] { JsonWriter w; w.begin_object(); w.key("a"); w.key("b"); }));
    CHECK(throws_logic_error([] { JsonWriter w; w.begin_object(); w.key("a"); w.end_object(); }));
    CHECK(throws_logic_error([] { JsonWriter w; w.end_object(); }));
    CHECK(throws_logic_error([] { JsonWriter w; w.begin_array(); w.end_object(); }));
    CHECK(throws_logic_error([] { JsonWriter w; w.integer(1); w.integer(2); }));
    CHECK(throws_logic_error([] { JsonWriter w; w.begin_object(); (void)w.str(); }));
    CHECK(throws_logic_error([] { JsonWriter w; (void)w.str(); }));
    std::cout << "[DONE] test_writer_misuse_throws" << std::endl;
}

void test_string_escaping() {
    std::cout << "[RUN] test_string_escaping" << std::endl;
    CHECK(JsonWriter::escape("plain") == "plain");
    CHECK(JsonWriter::escape("a\"b") == "a\\\"b");
    CHECK(JsonWriter::escape("a\\b") == "a\\\\b");
    CHECK(JsonWriter::escape("l1\nl2\r\t") == "l1\\nl2\\r\\t");
    CHECK(JsonWriter::escape(std::string("\x01\x1f", 2)) == "\\u0001\\u001f");
    CHECK(JsonWriter::escape(std::string("\0", 1)) == "\\u0000");
    CHECK(JsonWriter::escape("\x7f") == "\\u007f");
    CHECK(JsonWriter::escape("</script>") == "</script>");           // no HTML mangling needed
    CHECK(JsonWriter::escape("\xE2\x82\xAC") == "\xE2\x82\xAC");     // euro sign passes through as UTF-8
    CHECK(JsonWriter::escape("\xF0\x9F\x92\xB1") == "\xF0\x9F\x92\xB1"); // 4-byte sequence

    // Invalid UTF-8 is replaced by U+FFFD so the document stays valid.
    CHECK(JsonWriter::escape("\xFF") == "\\ufffd");
    CHECK(JsonWriter::escape("\xE2\x82") == "\\ufffd\\ufffd");       // truncated sequence
    CHECK(JsonWriter::escape("\xC0\x80") == "\\ufffd\\ufffd");       // overlong
    CHECK(JsonWriter::escape("\xED\xA0\x80") == "\\ufffd\\ufffd\\ufffd"); // surrogate
    CHECK(JsonWriter::escape("A\x80Z") == "A\\ufffdZ");              // stray continuation byte

    // Round trip: every nasty string survives write -> strict parse unchanged.
    const std::string nasty = std::string("quote\" back\\ nl\n tab\t ctl\x01 nul\0 end", 34);
    JsonWriter w(0);
    w.begin_object().key(nasty).string(nasty).end_object();
    const auto v = minijson::parse(w.str());
    CHECK(v.members.size() == 1);
    CHECK(v.members[0].first == nasty);
    CHECK(v.at(nasty).text == nasty);

    JsonWriter bad(0);
    bad.begin_array().string("x\xFFy").end_array();
    CHECK(minijson::parse(bad.str()).items[0].text == "x\xEF\xBF\xBD" "y");
    std::cout << "[DONE] test_string_escaping" << std::endl;
}

void test_number_formatting() {
    std::cout << "[RUN] test_number_formatting" << std::endl;
    CHECK(JsonWriter::format_number(0.1) == "0.10000000000000001");   // 17 significant digits
    CHECK(JsonWriter::format_number(0.92) == "0.92000000000000004");
    CHECK(JsonWriter::format_number(50000.0) == "50000");
    CHECK(JsonWriter::format_number(std::numeric_limits<double>::quiet_NaN()) == "null");
    CHECK(JsonWriter::format_number(std::numeric_limits<double>::infinity()) == "null");
    CHECK(JsonWriter::format_number(-std::numeric_limits<double>::infinity()) == "null");

    const double samples[] = {0.0, -0.5, 1.0 / 3.0, 0.0061, 150.5, 1e-300, 1e300, 5e-324,
                              std::numeric_limits<double>::max(), 123456789.123456789, 1.000960000000000027};
    for (double d : samples) {
        const std::string text = JsonWriter::format_number(d);
        const auto v = minijson::parse(text);                 // must be a valid JSON number
        CHECK(v.type == minijson::Value::Type::Number);
        CHECK(v.number == d);                                  // and round-trip exactly
    }
    std::cout << "[DONE] test_number_formatting" << std::endl;
}

// ---------------------------------------------------------- AnalysisService

void test_arbitrage_result_and_json() {
    std::cout << "[RUN] test_arbitrage_result_and_json" << std::endl;
    const double capital = 50000.0;
    const AnalysisResult r = analyze(SAMPLE_CSV, capital);
    CHECK(r.status == AnalysisStatus::Arbitrage);
    CHECK(r.analysis.has_value() && !r.error.has_value());

    const auto j = json_of(r);
    CHECK(j.at("status").text == "arbitrage");
    CHECK(j.at("meta").at("currencies").number == 4.0);
    CHECK(j.at("meta").at("rates").number == 5.0);
    CHECK(j.at("meta").at("starting_capital").number == capital);
    CHECK(j.at("error").is_null());

    const auto& a = j.at("analysis");
    const std::string currency = a.at("currency").text;
    const auto& cycle = a.at("cycle").items;
    const auto& steps = a.at("steps").items;

    // Cycle shape: closed loop, denominated in its first currency.
    CHECK(cycle.size() >= 3);
    CHECK(cycle.front().text == cycle.back().text);
    CHECK(currency == cycle.front().text);
    CHECK(steps.size() == cycle.size() - 1);

    // Known fixture result (USD -> EUR -> GBP -> USD).
    CHECK(currency == "USD");
    CHECK(cycle.size() == 4 && cycle[1].text == "EUR" && cycle[2].text == "GBP");

    // Steps chain together, and running amounts follow before * rate = after exactly.
    double product = 1.0;
    CHECK(steps[0].at("amount_before").number == capital);
    for (std::size_t i = 0; i < steps.size(); ++i) {
        CHECK(steps[i].at("step").number == static_cast<double>(i + 1));
        CHECK(steps[i].at("from").text == cycle[i].text);
        CHECK(steps[i].at("to").text == cycle[i + 1].text);
        const double rate = steps[i].at("rate").number;
        const double before = steps[i].at("amount_before").number;
        const double after = steps[i].at("amount_after").number;
        CHECK(after == before * rate);
        if (i > 0) CHECK(before == steps[i - 1].at("amount_after").number);
        product *= rate;
    }
    CHECK(steps[0].at("rate").number == 0.92);
    CHECK(steps[1].at("rate").number == 0.85);
    CHECK(steps[2].at("rate").number == 1.28);

    // Totals.
    const double multiplier = a.at("multiplier").number;
    CHECK(near(multiplier, product, 1e-12));
    CHECK(near(multiplier, 0.92 * 0.85 * 1.28, 1e-12));
    CHECK(near(a.at("profit_percent").number, (multiplier - 1.0) * 100.0, 1e-9));
    CHECK(a.at("starting_capital").number == capital);
    CHECK(a.at("final_capital").number == steps.back().at("amount_after").number);
    CHECK(near(a.at("final_capital").number, 50048.0, 1e-9));
    CHECK(near(a.at("profit").number, 48.0, 1e-9));
    CHECK(a.at("profit").number == a.at("final_capital").number - capital);
    std::cout << "[DONE] test_arbitrage_result_and_json" << std::endl;
}

void test_two_currency_cycle_amounts() {
    std::cout << "[RUN] test_two_currency_cycle_amounts" << std::endl;
    const AnalysisResult r = analyze("A,B,2\nB,A,0.6\n", 100.0);
    CHECK(r.status == AnalysisStatus::Arbitrage);
    const auto j = json_of(r);
    const auto& a = j.at("analysis");
    CHECK(a.at("cycle").items.size() == 3);
    CHECK(a.at("steps").items.size() == 2);
    CHECK(near(a.at("multiplier").number, 1.2));
    CHECK(near(a.at("profit_percent").number, 20.0));
    CHECK(near(a.at("final_capital").number, 120.0));
    CHECK(near(a.at("profit").number, 20.0));
    // Denominated in whichever currency the cycle starts with, and says so.
    CHECK(a.at("currency").text == a.at("cycle").items.front().text);
    CHECK(a.at("steps").items[0].at("from").text == a.at("currency").text);
    std::cout << "[DONE] test_two_currency_cycle_amounts" << std::endl;
}

void test_no_arbitrage_json() {
    std::cout << "[RUN] test_no_arbitrage_json" << std::endl;
    const AnalysisResult r = analyze(EQUILIBRIUM_CSV, 1000.0);
    CHECK(r.status == AnalysisStatus::NoArbitrage);
    const auto j = json_of(r);
    CHECK(j.at("status").text == "no_arbitrage");
    CHECK(j.at("analysis").is_null());
    CHECK(j.at("error").is_null());
    CHECK(j.at("meta").at("currencies").number == 3.0);
    CHECK(j.at("meta").at("rates").number == 6.0);
    CHECK(j.at("meta").at("starting_capital").number == 1000.0);
    std::cout << "[DONE] test_no_arbitrage_json" << std::endl;
}

void test_error_json_with_line_number() {
    std::cout << "[RUN] test_error_json_with_line_number" << std::endl;
    const AnalysisResult r = analyze("USD,EUR,0.92\nEUR,GBP,0.85\nGBP,USD,-1\n", 1000.0);
    CHECK(r.status == AnalysisStatus::Error);
    const auto j = json_of(r);
    CHECK(j.at("status").text == "error");
    CHECK(j.at("analysis").is_null());
    CHECK(j.at("meta").at("currencies").is_null());
    CHECK(j.at("meta").at("rates").is_null());
    CHECK(j.at("meta").at("starting_capital").number == 1000.0);
    const auto& e = j.at("error");
    CHECK(e.at("type").text == "csv");
    CHECK(e.at("line").number == 3.0);
    CHECK(e.at("message").text == "Rate must be strictly positive: '-1'");
    CHECK(e.at("message").text.find("CSV Error") == std::string::npos);  // no duplicated prefix
    std::cout << "[DONE] test_error_json_with_line_number" << std::endl;
}

void test_error_cases() {
    std::cout << "[RUN] test_error_cases" << std::endl;

    // Malformed FIRST data row: line-specific error, not swallowed as a header.
    {
        const auto j = json_of(analyze("USD,EUR,abc\n", 1000.0));
        CHECK(j.at("status").text == "error");
        CHECK(j.at("error").at("line").number == 1.0);
        CHECK(j.at("error").at("message").text.find("Cannot parse rate as number: 'abc'") != std::string::npos);
    }
    // Empty input: file-level error, line is null.
    {
        const auto j = json_of(analyze("", 1000.0));
        CHECK(j.at("status").text == "error");
        CHECK(j.at("error").at("line").is_null());
        CHECK(j.at("error").at("message").text.find("No valid exchange-rate rows found") != std::string::npos);
    }
    // Header only.
    {
        const auto j = json_of(analyze("source_currency,target_currency,rate\n", 1000.0));
        CHECK(j.at("error").at("line").is_null());
    }
    // Wrong column count keeps its line number.
    {
        const auto j = json_of(analyze("USD,EUR,0.9\nEUR,GBP\n", 1000.0));
        CHECK(j.at("error").at("line").number == 2.0);
        CHECK(j.at("error").at("message").text.find("Expected exactly 3 fields") != std::string::npos);
    }
    // Missing file.
    {
        const auto r = AnalysisService::analyze_file("definitely_missing_file_123.csv", 1000.0);
        const auto j = json_of(r);
        CHECK(j.at("status").text == "error");
        CHECK(j.at("error").at("type").text == "csv");
        CHECK(j.at("error").at("line").is_null());
        CHECK(j.at("error").at("message").text.find("Could not open file") != std::string::npos);
    }
    // Invalid starting capital: rejected before any parsing, capital reported as null.
    const double bad_caps[] = {0.0, -5.0, std::numeric_limits<double>::quiet_NaN(),
                               std::numeric_limits<double>::infinity()};
    for (double cap : bad_caps) {
        const auto r = analyze(SAMPLE_CSV, cap);
        CHECK(r.status == AnalysisStatus::Error);
        const auto j = json_of(r);
        CHECK(j.at("error").at("type").text == "input");
        CHECK(j.at("meta").at("starting_capital").is_null());
        CHECK(j.at("analysis").is_null());
    }
    std::cout << "[DONE] test_error_cases" << std::endl;
}

void test_json_escaping_in_error_messages() {
    std::cout << "[RUN] test_json_escaping_in_error_messages" << std::endl;
    // The offending currency code is echoed in the message, so hostile bytes must be escaped.
    {
        const auto j = json_of(analyze("US\"D,EUR,1.5\n", 1000.0));
        CHECK(j.at("status").text == "error");
        CHECK(j.at("error").at("message").text.find("US\"D") != std::string::npos);
    }
    {
        const auto j = json_of(analyze("US\\D,EUR,1.5\n", 1000.0));
        CHECK(j.at("error").at("message").text.find("US\\D") != std::string::npos);
    }
    {
        const auto j = json_of(analyze(std::string("USD,EU\x01R,1.5\n", 14), 1000.0));
        CHECK(j.at("error").at("message").text.find("EU\x01R") != std::string::npos);
    }
    {
        const std::string raw = AnalysisService::to_json(analyze("US\xFF" "D,EUR,1.5\n", 1000.0));
        const auto j = minijson::parse(raw);   // still valid JSON
        CHECK(j.at("error").at("message").text.find("US\xEF\xBF\xBD" "D") != std::string::npos);
    }
    {
        // Messages built before analysis (CLI usage errors) go through the same escaping.
        const auto r = AnalysisService::make_error("usage", "Unknown option '\"--x\"\n'");
        const auto j = json_of(r);
        CHECK(j.at("error").at("message").text == "Unknown option '\"--x\"\n'");
        CHECK(j.at("error").at("type").text == "usage");
    }
    std::cout << "[DONE] test_json_escaping_in_error_messages" << std::endl;
}

void test_bom_through_analysis() {
    std::cout << "[RUN] test_bom_through_analysis" << std::endl;
    const auto j = json_of(analyze(std::string("\xEF\xBB\xBF") + SAMPLE_CSV, 1000.0));
    CHECK(j.at("status").text == "arbitrage");
    CHECK(j.at("analysis").at("cycle").items.front().text == "USD");  // no BOM glued to the code
    std::cout << "[DONE] test_bom_through_analysis" << std::endl;
}

void test_compact_and_pretty_agree() {
    std::cout << "[RUN] test_compact_and_pretty_agree" << std::endl;
    const AnalysisResult r = analyze(SAMPLE_CSV, 1000.0);
    const std::string compact = AnalysisService::to_json(r, 0);
    CHECK(compact.find('\n') == std::string::npos);
    const auto c = minijson::parse(compact);
    const auto p = minijson::parse(AnalysisService::to_json(r, 2));
    CHECK(c.at("analysis").at("multiplier").number == p.at("analysis").at("multiplier").number);
    CHECK(c.at("analysis").at("steps").items.size() == p.at("analysis").at("steps").items.size());
    std::cout << "[DONE] test_compact_and_pretty_agree" << std::endl;
}

} // anonymous namespace

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Starting JSON / AnalysisService Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    test_writer_structure();
    test_writer_misuse_throws();
    test_string_escaping();
    test_number_formatting();
    test_arbitrage_result_and_json();
    test_two_currency_cycle_amounts();
    test_no_arbitrage_json();
    test_error_json_with_line_number();
    test_error_cases();
    test_json_escaping_in_error_messages();
    test_bom_through_analysis();
    test_compact_and_pretty_agree();

    std::cout << "========================================" << std::endl;
    std::cout << g_checks << " checks, " << g_failures << " failures" << std::endl;
    std::cout << (g_failures == 0 ? "All JSON / AnalysisService Tests Passed!" : "SOME TESTS FAILED") << std::endl;
    std::cout << "========================================" << std::endl;
    return g_failures == 0 ? 0 : 1;
}
