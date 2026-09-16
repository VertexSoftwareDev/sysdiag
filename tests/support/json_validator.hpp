// Strict RFC 8259 JSON syntax checker used by the tests.
//
// Deliberately independent from JsonWriter so that the writer is checked
// against a separate implementation rather than against itself.
#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace sysdiag::test {

class JsonValidator {
  public:
    // Returns an empty string when `doc` is valid, otherwise a description.
    static std::string validate(std::string_view doc) {
        JsonValidator v(doc);
        v.skip_ws();
        if (!v.value(0)) {
            return v.error_;
        }
        v.skip_ws();
        if (v.pos_ != doc.size()) {
            return v.fail("trailing characters");
        }
        return {};
    }

  private:
    static constexpr int kMaxDepth = 64;

    explicit JsonValidator(std::string_view doc) : s_(doc) {}

    bool fail_bool(const std::string& what) {
        fail(what);
        return false;
    }
    std::string fail(const std::string& what) {
        if (error_.empty()) {
            error_ = what + " at offset " + std::to_string(pos_);
        }
        return error_;
    }

    [[nodiscard]] bool eof() const { return pos_ >= s_.size(); }
    [[nodiscard]] char peek() const { return eof() ? '\0' : s_[pos_]; }

    void skip_ws() {
        while (!eof() && (peek() == ' ' || peek() == '\t' || peek() == '\n' || peek() == '\r')) {
            ++pos_;
        }
    }

    bool literal(std::string_view word) {
        if (s_.substr(pos_, word.size()) != word) {
            return fail_bool("invalid literal");
        }
        pos_ += word.size();
        return true;
    }

    static bool is_digit(char c) { return c >= '0' && c <= '9'; }
    static bool is_hex(char c) {
        return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    }

    bool number() {
        if (peek() == '-') {
            ++pos_;
        }
        if (peek() == '0') {
            ++pos_;
        } else if (is_digit(peek())) {
            while (is_digit(peek())) {
                ++pos_;
            }
        } else {
            return fail_bool("invalid number");
        }
        if (peek() == '.') {
            ++pos_;
            if (!is_digit(peek())) {
                return fail_bool("digit expected after '.'");
            }
            while (is_digit(peek())) {
                ++pos_;
            }
        }
        if (peek() == 'e' || peek() == 'E') {
            ++pos_;
            if (peek() == '+' || peek() == '-') {
                ++pos_;
            }
            if (!is_digit(peek())) {
                return fail_bool("digit expected in exponent");
            }
            while (is_digit(peek())) {
                ++pos_;
            }
        }
        return true;
    }

    bool string() {
        ++pos_;  // opening quote
        while (!eof()) {
            const auto c = static_cast<unsigned char>(s_[pos_]);
            if (c == '"') {
                ++pos_;
                return true;
            }
            if (c < 0x20) {
                return fail_bool("unescaped control character in string");
            }
            if (c == '\\') {
                ++pos_;
                const char e = peek();
                if (e == 'u') {
                    for (int i = 1; i <= 4; ++i) {
                        if (pos_ + static_cast<std::size_t>(i) >= s_.size() ||
                            !is_hex(s_[pos_ + static_cast<std::size_t>(i)])) {
                            return fail_bool("invalid \\u escape");
                        }
                    }
                    pos_ += 5;
                    continue;
                }
                if (std::string_view("\"\\/bfnrt").find(e) == std::string_view::npos || eof()) {
                    return fail_bool("invalid escape");
                }
            }
            ++pos_;
        }
        return fail_bool("unterminated string");
    }

    bool value(int depth) {
        if (depth > kMaxDepth) {
            return fail_bool("nesting too deep");
        }
        switch (peek()) {
            case '{': return object(depth);
            case '[': return array(depth);
            case '"': return string();
            case 't': return literal("true");
            case 'f': return literal("false");
            case 'n': return literal("null");
            default: return number();
        }
    }

    bool array(int depth) {
        ++pos_;
        skip_ws();
        if (peek() == ']') {
            ++pos_;
            return true;
        }
        while (true) {
            skip_ws();
            if (!value(depth + 1)) {
                return false;
            }
            skip_ws();
            if (peek() == ',') {
                ++pos_;
                continue;
            }
            if (peek() == ']') {
                ++pos_;
                return true;
            }
            return fail_bool("',' or ']' expected");
        }
    }

    bool object(int depth) {
        ++pos_;
        skip_ws();
        if (peek() == '}') {
            ++pos_;
            return true;
        }
        while (true) {
            skip_ws();
            if (peek() != '"') {
                return fail_bool("object key expected");
            }
            if (!string()) {
                return false;
            }
            skip_ws();
            if (peek() != ':') {
                return fail_bool("':' expected");
            }
            ++pos_;
            skip_ws();
            if (!value(depth + 1)) {
                return false;
            }
            skip_ws();
            if (peek() == ',') {
                ++pos_;
                continue;
            }
            if (peek() == '}') {
                ++pos_;
                return true;
            }
            return fail_bool("',' or '}' expected");
        }
    }

    std::string_view s_;
    std::size_t pos_ = 0;
    std::string error_;
};

}  // namespace sysdiag::test
