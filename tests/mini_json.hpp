// Tiny strict JSON parser used only by the tests, so they can verify that emitted JSON is
// syntactically valid and inspect its structure instead of matching substrings.
#ifndef TESTS_MINI_JSON_HPP
#define TESTS_MINI_JSON_HPP

#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace minijson {

struct Value {
    enum class Type { Null, Bool, Number, String, Array, Object };
    Type type{Type::Null};
    bool boolean{false};
    double number{0.0};
    std::string text;
    std::vector<Value> items;
    std::vector<std::pair<std::string, Value>> members;

    bool is_null() const { return type == Type::Null; }
    bool has(const std::string& key) const {
        for (const auto& m : members) if (m.first == key) return true;
        return false;
    }
    const Value& at(const std::string& key) const {
        for (const auto& m : members) if (m.first == key) return m.second;
        throw std::runtime_error("missing JSON key: " + key);
    }
};

class Parser {
public:
    explicit Parser(const std::string& s) : s_(s) {}

    Value parse_document() {
        skip_ws();
        Value v = parse_value();
        skip_ws();
        if (pos_ != s_.size()) fail("trailing characters after JSON document");
        return v;
    }

private:
    [[noreturn]] void fail(const std::string& why) const {
        throw std::runtime_error("invalid JSON at offset " + std::to_string(pos_) + ": " + why);
    }
    void skip_ws() {
        while (pos_ < s_.size() && (s_[pos_] == ' ' || s_[pos_] == '\n' || s_[pos_] == '\t' || s_[pos_] == '\r')) ++pos_;
    }
    char peek() const { return pos_ < s_.size() ? s_[pos_] : '\0'; }
    void expect(char c) {
        if (peek() != c) fail(std::string("expected '") + c + "'");
        ++pos_;
    }
    bool consume_literal(const char* lit) {
        const std::string l(lit);
        if (s_.compare(pos_, l.size(), l) == 0) { pos_ += l.size(); return true; }
        return false;
    }

    Value parse_value() {
        Value v;
        const char c = peek();
        if (c == '{') return parse_object();
        if (c == '[') return parse_array();
        if (c == '"') { v.type = Value::Type::String; v.text = parse_string(); return v; }
        if (consume_literal("true"))  { v.type = Value::Type::Bool; v.boolean = true; return v; }
        if (consume_literal("false")) { v.type = Value::Type::Bool; v.boolean = false; return v; }
        if (consume_literal("null"))  { return v; }
        if (c == '-' || (c >= '0' && c <= '9')) return parse_number();
        fail("unexpected character");
    }

    Value parse_number() {
        const std::size_t start = pos_;
        if (peek() == '-') ++pos_;
        if (peek() == '0') { ++pos_; }
        else if (peek() >= '1' && peek() <= '9') { while (peek() >= '0' && peek() <= '9') ++pos_; }
        else fail("bad number");
        if (peek() == '.') {
            ++pos_;
            if (!(peek() >= '0' && peek() <= '9')) fail("bad fraction");
            while (peek() >= '0' && peek() <= '9') ++pos_;
        }
        if (peek() == 'e' || peek() == 'E') {
            ++pos_;
            if (peek() == '+' || peek() == '-') ++pos_;
            if (!(peek() >= '0' && peek() <= '9')) fail("bad exponent");
            while (peek() >= '0' && peek() <= '9') ++pos_;
        }
        Value v;
        v.type = Value::Type::Number;
        v.number = std::strtod(s_.substr(start, pos_ - start).c_str(), nullptr);
        return v;
    }

    static void append_utf8(std::string& out, unsigned long cp) {
        if (cp < 0x80) out += static_cast<char>(cp);
        else if (cp < 0x800) { out += static_cast<char>(0xC0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { out += static_cast<char>(0xE0 | (cp >> 12)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
        else { out += static_cast<char>(0xF0 | (cp >> 18)); out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
    }

    unsigned long parse_hex4() {
        unsigned long v = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = peek();
            v <<= 4;
            if (c >= '0' && c <= '9') v |= static_cast<unsigned long>(c - '0');
            else if (c >= 'a' && c <= 'f') v |= static_cast<unsigned long>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= static_cast<unsigned long>(c - 'A' + 10);
            else fail("bad \\u escape");
            ++pos_;
        }
        return v;
    }

    std::string parse_string() {
        expect('"');
        std::string out;
        while (true) {
            if (pos_ >= s_.size()) fail("unterminated string");
            const unsigned char c = static_cast<unsigned char>(s_[pos_]);
            if (c == '"') { ++pos_; return out; }
            if (c < 0x20) fail("raw control character in string");
            if (c == '\\') {
                ++pos_;
                const char e = peek();
                ++pos_;
                switch (e) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    case 'u': {
                        unsigned long cp = parse_hex4();
                        if (cp >= 0xD800 && cp <= 0xDBFF) {
                            if (!(peek() == '\\' && pos_ + 1 < s_.size() && s_[pos_ + 1] == 'u')) fail("lone surrogate");
                            pos_ += 2;
                            const unsigned long lo = parse_hex4();
                            if (lo < 0xDC00 || lo > 0xDFFF) fail("bad low surrogate");
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        }
                        append_utf8(out, cp);
                        break;
                    }
                    default: fail("bad escape");
                }
            } else {
                out += static_cast<char>(c);
                ++pos_;
            }
        }
    }

    Value parse_array() {
        expect('[');
        Value v;
        v.type = Value::Type::Array;
        skip_ws();
        if (peek() == ']') { ++pos_; return v; }
        while (true) {
            skip_ws();
            v.items.push_back(parse_value());
            skip_ws();
            if (peek() == ',') { ++pos_; continue; }
            expect(']');
            return v;
        }
    }

    Value parse_object() {
        expect('{');
        Value v;
        v.type = Value::Type::Object;
        skip_ws();
        if (peek() == '}') { ++pos_; return v; }
        while (true) {
            skip_ws();
            std::string key = parse_string();
            skip_ws();
            expect(':');
            skip_ws();
            Value member = parse_value();
            v.members.emplace_back(std::move(key), std::move(member));
            skip_ws();
            if (peek() == ',') { ++pos_; continue; }
            expect('}');
            return v;
        }
    }

    const std::string& s_;
    std::size_t pos_{0};
};

/// Parses a complete JSON document; throws std::runtime_error if it is not valid JSON.
inline Value parse(const std::string& text) {
    return Parser(text).parse_document();
}

} // namespace minijson

#endif // TESTS_MINI_JSON_HPP
