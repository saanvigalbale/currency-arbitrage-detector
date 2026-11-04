#include "CurrencyGraph.hpp"
#include "RateLoader.hpp"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <cassert>
#include <cmath>

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

    std::cout << "========================================" << std::endl;
    std::cout << "All RateLoader Tests Passed!" << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
