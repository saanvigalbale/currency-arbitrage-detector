#ifndef CURRENCY_ARBITRAGE_JSON_WRITER_HPP
#define CURRENCY_ARBITRAGE_JSON_WRITER_HPP

#include <cstddef>
#include <string>
#include <vector>

/**
 * @brief Minimal, dependency-free JSON text builder.
 *
 * Guarantees syntactically valid JSON output: strings are escaped (including control
 * characters), invalid UTF-8 byte sequences are replaced by U+FFFD, and non-finite
 * numbers are written as null. Misuse (a value without a key inside an object, unbalanced
 * begin/end, more than one root value) throws std::logic_error instead of emitting bad JSON.
 */
class JsonWriter {
public:
    /// @param indent Spaces per nesting level; 0 produces compact single-line output.
    explicit JsonWriter(int indent = 2);

    JsonWriter& begin_object();
    JsonWriter& end_object();
    JsonWriter& begin_array();
    JsonWriter& end_array();

    /// Writes an object key. Must be followed by exactly one value.
    JsonWriter& key(const std::string& name);

    JsonWriter& string(const std::string& value);
    /// Numbers use 17 significant digits (round-trips any double); NaN/Inf become null.
    JsonWriter& number(double value);
    JsonWriter& integer(std::size_t value);
    JsonWriter& boolean(bool value);
    JsonWriter& null();

    /// Returns the finished document. Throws std::logic_error if it is incomplete.
    std::string str() const;

    /// Escapes a string for use between JSON quotes (quotes themselves not included).
    static std::string escape(const std::string& raw);

    /// Formats a double with 17 significant digits in the classic locale ("null" if non-finite).
    static std::string format_number(double value);

private:
    struct Level {
        bool is_object;
        std::size_t count;
    };

    void before_value();
    void newline_and_indent();

    int indent_;
    std::string out_;
    std::vector<Level> stack_;
    bool key_pending_{false};
    bool root_written_{false};
};

#endif // CURRENCY_ARBITRAGE_JSON_WRITER_HPP
