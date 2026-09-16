// Small string utilities used across the core library.
#pragma once

#include <string>
#include <string_view>

namespace sysdiag::text {

// Removes leading/trailing ASCII whitespace (CPU brand strings are padded).
[[nodiscard]] std::string_view trim(std::string_view value) noexcept;

// ASCII-only case-insensitive comparison (locale-independent).
[[nodiscard]] bool iequals(std::string_view a, std::string_view b) noexcept;

// Returns a copy in which every invalid UTF-8 sequence is replaced by U+FFFD.
[[nodiscard]] std::string sanitize_utf8(std::string_view value);

// Makes untrusted text (e.g. a command-line argument) safe to echo to a
// terminal: invalid UTF-8 is replaced, control characters become '?', and
// the result is truncated to max_chars bytes (on a code point boundary).
[[nodiscard]] std::string sanitize_for_display(std::string_view value, std::size_t max_bytes = 64);

}  // namespace sysdiag::text
