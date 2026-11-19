#include "JsonWriter.hpp"

#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace {

constexpr const char* HEX = "0123456789abcdef";

void append_unicode_escape(std::string& out, unsigned int code_unit) {
    out += "\\u";
    out += HEX[(code_unit >> 12) & 0xF];
    out += HEX[(code_unit >> 8) & 0xF];
    out += HEX[(code_unit >> 4) & 0xF];
    out += HEX[code_unit & 0xF];
}

bool is_continuation(unsigned char c) {
    return (c & 0xC0) == 0x80;
}

// Returns the byte length (2-4) of a well-formed multi-byte UTF-8 sequence starting at
// raw[i], or 0 if the sequence is invalid (bad lead byte, truncated, overlong, surrogate,
// or beyond U+10FFFF).
std::size_t valid_multibyte_length(const std::string& raw, std::size_t i) {
    const unsigned char b0 = static_cast<unsigned char>(raw[i]);
    std::size_t need = 0;
    if (b0 >= 0xC2 && b0 <= 0xDF) need = 2;
    else if (b0 >= 0xE0 && b0 <= 0xEF) need = 3;
    else if (b0 >= 0xF0 && b0 <= 0xF4) need = 4;
    else return 0;

    if (i + need > raw.size()) return 0;
    for (std::size_t k = 1; k < need; ++k) {
        if (!is_continuation(static_cast<unsigned char>(raw[i + k]))) return 0;
    }

    const unsigned char b1 = static_cast<unsigned char>(raw[i + 1]);
    if (b0 == 0xE0 && b1 < 0xA0) return 0;  // overlong
    if (b0 == 0xED && b1 >= 0xA0) return 0;  // UTF-16 surrogates
    if (b0 == 0xF0 && b1 < 0x90) return 0;  // overlong
    if (b0 == 0xF4 && b1 >= 0x90) return 0;  // > U+10FFFF
    return need;
}

} // anonymous namespace

JsonWriter::JsonWriter(int indent) : indent_(indent < 0 ? 0 : indent) {}

std::string JsonWriter::escape(const std::string& raw) {
    std::string out;
    out.reserve(raw.size() + 8);
    for (std::size_t i = 0; i < raw.size();) {
        const unsigned char c = static_cast<unsigned char>(raw[i]);
        if (c < 0x80) {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\b': out += "\\b";  break;
                case '\f': out += "\\f";  break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                default:
                    if (c < 0x20 || c == 0x7F) {
                        append_unicode_escape(out, c);
                    } else {
                        out += static_cast<char>(c);
                    }
            }
            ++i;
            continue;
        }
        const std::size_t len = valid_multibyte_length(raw, i);
        if (len == 0) {
            out += "\\ufffd";  // replacement character for an invalid byte
            ++i;
        } else {
            out.append(raw, i, len);
            i += len;
        }
    }
    return out;
}

std::string JsonWriter::format_number(double value) {
    if (!std::isfinite(value)) {
        return "null";
    }
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << std::setprecision(17) << value;
    return oss.str();
}

void JsonWriter::newline_and_indent() {
    if (indent_ > 0) {
        out_ += '\n';
        out_.append(stack_.size() * static_cast<std::size_t>(indent_), ' ');
    }
}

void JsonWriter::before_value() {
    if (stack_.empty()) {
        if (root_written_) {
            throw std::logic_error("JsonWriter: only one root value is allowed");
        }
        root_written_ = true;
        return;
    }
    Level& top = stack_.back();
    if (top.is_object) {
        if (!key_pending_) {
            throw std::logic_error("JsonWriter: object value written without a key");
        }
        key_pending_ = false;
    } else {
        if (top.count > 0) out_ += ',';
        newline_and_indent();
        ++top.count;
    }
}

JsonWriter& JsonWriter::begin_object() {
    before_value();
    out_ += '{';
    stack_.push_back({true, 0});
    return *this;
}

JsonWriter& JsonWriter::end_object() {
    if (stack_.empty() || !stack_.back().is_object || key_pending_) {
        throw std::logic_error("JsonWriter: unbalanced end_object");
    }
    const bool had_members = stack_.back().count > 0;
    stack_.pop_back();
    if (had_members) newline_and_indent();
    out_ += '}';
    return *this;
}

JsonWriter& JsonWriter::begin_array() {
    before_value();
    out_ += '[';
    stack_.push_back({false, 0});
    return *this;
}

JsonWriter& JsonWriter::end_array() {
    if (stack_.empty() || stack_.back().is_object) {
        throw std::logic_error("JsonWriter: unbalanced end_array");
    }
    const bool had_elements = stack_.back().count > 0;
    stack_.pop_back();
    if (had_elements) newline_and_indent();
    out_ += ']';
    return *this;
}

JsonWriter& JsonWriter::key(const std::string& name) {
    if (stack_.empty() || !stack_.back().is_object || key_pending_) {
        throw std::logic_error("JsonWriter: key is only valid inside an object, before a value");
    }
    Level& top = stack_.back();
    if (top.count > 0) out_ += ',';
    newline_and_indent();
    ++top.count;
    out_ += '"';
    out_ += escape(name);
    out_ += indent_ > 0 ? "\": " : "\":";
    key_pending_ = true;
    return *this;
}

JsonWriter& JsonWriter::string(const std::string& value) {
    before_value();
    out_ += '"';
    out_ += escape(value);
    out_ += '"';
    return *this;
}

JsonWriter& JsonWriter::number(double value) {
    before_value();
    out_ += format_number(value);
    return *this;
}

JsonWriter& JsonWriter::integer(std::size_t value) {
    before_value();
    out_ += std::to_string(value);
    return *this;
}

JsonWriter& JsonWriter::boolean(bool value) {
    before_value();
    out_ += value ? "true" : "false";
    return *this;
}

JsonWriter& JsonWriter::null() {
    before_value();
    out_ += "null";
    return *this;
}

std::string JsonWriter::str() const {
    if (!stack_.empty() || key_pending_ || !root_written_) {
        throw std::logic_error("JsonWriter: document is incomplete");
    }
    return out_;
}
