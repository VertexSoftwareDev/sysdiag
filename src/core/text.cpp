#include "core/text.hpp"

#include <cstdint>

namespace sysdiag::text {
namespace {

constexpr std::string_view kReplacement = "\xEF\xBF\xBD";  // U+FFFD

bool is_space(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

char ascii_lower(char c) noexcept {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

bool is_continuation(unsigned char c) noexcept {
    return (c & 0xC0U) == 0x80U;
}

// Length of the valid UTF-8 sequence starting at `pos`, or 0 if invalid.
// Rejects overlong encodings, surrogates and code points above U+10FFFF.
std::size_t valid_sequence_length(std::string_view s, std::size_t pos) noexcept {
    const auto byte = [&](std::size_t i) { return static_cast<unsigned char>(s[i]); };
    const unsigned char lead = byte(pos);
    const std::size_t remaining = s.size() - pos;

    if (lead < 0x80U) {
        return 1;
    }
    if (lead >= 0xC2U && lead <= 0xDFU) {
        return (remaining >= 2 && is_continuation(byte(pos + 1))) ? 2 : 0;
    }
    if (lead >= 0xE0U && lead <= 0xEFU) {
        if (remaining < 3 || !is_continuation(byte(pos + 1)) || !is_continuation(byte(pos + 2))) {
            return 0;
        }
        const unsigned char second = byte(pos + 1);
        if (lead == 0xE0U && second < 0xA0U) {
            return 0;  // overlong
        }
        if (lead == 0xEDU && second > 0x9FU) {
            return 0;  // UTF-16 surrogate
        }
        return 3;
    }
    if (lead >= 0xF0U && lead <= 0xF4U) {
        if (remaining < 4 || !is_continuation(byte(pos + 1)) || !is_continuation(byte(pos + 2)) ||
            !is_continuation(byte(pos + 3))) {
            return 0;
        }
        const unsigned char second = byte(pos + 1);
        if (lead == 0xF0U && second < 0x90U) {
            return 0;  // overlong
        }
        if (lead == 0xF4U && second > 0x8FU) {
            return 0;  // above U+10FFFF
        }
        return 4;
    }
    return 0;
}

}  // namespace

std::string_view trim(std::string_view value) noexcept {
    while (!value.empty() && is_space(value.front())) {
        value.remove_prefix(1);
    }
    while (!value.empty() && is_space(value.back())) {
        value.remove_suffix(1);
    }
    return value;
}

bool iequals(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (ascii_lower(a[i]) != ascii_lower(b[i])) {
            return false;
        }
    }
    return true;
}

std::string sanitize_utf8(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    std::size_t pos = 0;
    while (pos < value.size()) {
        const std::size_t len = valid_sequence_length(value, pos);
        if (len == 0) {
            out.append(kReplacement);
            ++pos;
        } else {
            out.append(value.substr(pos, len));
            pos += len;
        }
    }
    return out;
}

std::string sanitize_for_display(std::string_view value, std::size_t max_bytes) {
    std::string out;
    std::size_t pos = 0;
    bool truncated = false;
    while (pos < value.size()) {
        const std::size_t len = valid_sequence_length(value, pos);
        std::string_view piece;
        if (len == 0) {
            piece = kReplacement;
        } else if (len == 1) {
            const auto c = static_cast<unsigned char>(value[pos]);
            piece = (c < 0x20U || c == 0x7FU) ? std::string_view("?") : value.substr(pos, 1);
        } else {
            // Reject C1 control characters (U+0080..U+009F), which some
            // terminals interpret as escape sequences.
            const bool c1 = len == 2 && static_cast<unsigned char>(value[pos]) == 0xC2U &&
                            static_cast<unsigned char>(value[pos + 1]) < 0xA0U;
            piece = c1 ? std::string_view("?") : value.substr(pos, len);
        }
        if (out.size() + piece.size() > max_bytes) {
            truncated = true;
            break;
        }
        out.append(piece);
        pos += (len == 0) ? 1 : len;
    }
    if (truncated) {
        out.append("...");
    }
    return out;
}

}  // namespace sysdiag::text
