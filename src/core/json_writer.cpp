#include "core/json_writer.hpp"

#include <array>
#include <cmath>
#include <stdexcept>

#include "core/text.hpp"
#include "core/units.hpp"

namespace sysdiag {

std::string JsonWriter::escape(std::string_view value) {
    static constexpr std::string_view kHex = "0123456789abcdef";
    const std::string valid = text::sanitize_utf8(value);
    std::string out;
    out.reserve(valid.size() + 2);
    for (const char ch : valid) {
        const auto c = static_cast<unsigned char>(ch);
        switch (ch) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20U) {
                    out += "\\u00";
                    out.push_back(kHex[(c >> 4U) & 0xFU]);
                    out.push_back(kHex[c & 0xFU]);
                } else {
                    out.push_back(ch);
                }
        }
    }
    return out;
}

void JsonWriter::newline_indent() {
    if (!pretty_) {
        return;
    }
    out_.push_back('\n');
    out_.append(stack_.size() * 2, ' ');
}

void JsonWriter::before_value() {
    if (stack_.empty()) {
        if (root_written_) {
            throw std::logic_error("JsonWriter: multiple root values");
        }
        root_written_ = true;
        return;
    }
    Frame& top = stack_.back();
    if (top.is_object) {
        if (!top.awaiting_value) {
            throw std::logic_error("JsonWriter: value inside object requires a key");
        }
        top.awaiting_value = false;
        return;
    }
    if (top.has_items) {
        out_.push_back(',');
    }
    top.has_items = true;
    newline_indent();
}

void JsonWriter::key(std::string_view name) {
    if (stack_.empty() || !stack_.back().is_object) {
        throw std::logic_error("JsonWriter: key outside of an object");
    }
    Frame& top = stack_.back();
    if (top.awaiting_value) {
        throw std::logic_error("JsonWriter: key written twice without a value");
    }
    if (top.has_items) {
        out_.push_back(',');
    }
    top.has_items = true;
    top.awaiting_value = true;
    newline_indent();
    out_.push_back('"');
    out_ += escape(name);
    out_ += pretty_ ? "\": " : "\":";
}

void JsonWriter::begin_object() {
    before_value();
    out_.push_back('{');
    stack_.push_back(Frame{true, false, false});
}

void JsonWriter::begin_array() {
    before_value();
    out_.push_back('[');
    stack_.push_back(Frame{false, false, false});
}

void JsonWriter::close(bool is_object) {
    if (stack_.empty() || stack_.back().is_object != is_object) {
        throw std::logic_error("JsonWriter: mismatched close");
    }
    if (stack_.back().awaiting_value) {
        throw std::logic_error("JsonWriter: key without value");
    }
    const bool had_items = stack_.back().has_items;
    stack_.pop_back();
    if (had_items) {
        newline_indent();
    }
    out_.push_back(is_object ? '}' : ']');
}

void JsonWriter::end_object() {
    close(true);
}
void JsonWriter::end_array() {
    close(false);
}

void JsonWriter::string(std::string_view value) {
    before_value();
    out_.push_back('"');
    out_ += escape(value);
    out_.push_back('"');
}

void JsonWriter::number(std::uint64_t value) {
    before_value();
    out_ += std::to_string(value);
}

void JsonWriter::number(std::int64_t value) {
    before_value();
    out_ += std::to_string(value);
}

void JsonWriter::number(double value, int precision) {
    if (!std::isfinite(value)) {
        null();
        return;
    }
    before_value();
    out_ += units::format_fixed(value, precision);
}

void JsonWriter::boolean(bool value) {
    before_value();
    out_ += value ? "true" : "false";
}

void JsonWriter::null() {
    before_value();
    out_ += "null";
}

void JsonWriter::field(std::string_view name, std::string_view value) {
    key(name);
    string(value);
}

void JsonWriter::field(std::string_view name, const std::optional<std::string>& value) {
    key(name);
    value ? string(*value) : null();
}

void JsonWriter::field(std::string_view name, std::uint64_t value) {
    key(name);
    number(value);
}

void JsonWriter::field(std::string_view name, const std::optional<std::uint64_t>& value) {
    key(name);
    value ? number(*value) : null();
}

void JsonWriter::field(std::string_view name, const std::optional<std::uint32_t>& value) {
    key(name);
    value ? number(std::uint64_t{*value}) : null();
}

void JsonWriter::field(std::string_view name, const std::optional<double>& value, int precision) {
    key(name);
    value ? number(*value, precision) : null();
}

void JsonWriter::bool_field(std::string_view name, bool value) {
    key(name);
    boolean(value);
}

void JsonWriter::string_array(std::string_view name, const std::vector<std::string>& values) {
    key(name);
    begin_array();
    for (const auto& v : values) {
        string(v);
    }
    end_array();
}

std::string JsonWriter::finish() {
    if (!stack_.empty() || !root_written_) {
        throw std::logic_error("JsonWriter: document is incomplete");
    }
    if (pretty_) {
        out_.push_back('\n');
    }
    return std::move(out_);
}

}  // namespace sysdiag
