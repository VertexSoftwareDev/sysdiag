// Overflow-safe arithmetic and unit formatting helpers.
#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace sysdiag::units {

inline constexpr std::uint64_t kKiB = 1024ULL;
inline constexpr std::uint64_t kMiB = kKiB * 1024ULL;
inline constexpr std::uint64_t kGiB = kMiB * 1024ULL;
inline constexpr std::uint64_t kTiB = kGiB * 1024ULL;

// a - b, or 0 if b > a.
[[nodiscard]] constexpr std::uint64_t saturating_sub(std::uint64_t a, std::uint64_t b) noexcept {
    return a > b ? a - b : 0;
}

// part / whole * 100, clamped to [0, 100]. nullopt when whole == 0.
[[nodiscard]] std::optional<double> percent(std::uint64_t part, std::uint64_t whole) noexcept;

struct Usage {
    std::uint64_t used = 0;
    std::optional<double> percent;
};

// Derives used space from total and free. A free value larger than total
// (which inconsistent OS data can produce) is clamped rather than wrapping.
[[nodiscard]] Usage usage_from_free(std::uint64_t total, std::uint64_t free) noexcept;

// Locale-independent fixed-point formatting ("12.3"). Non-finite -> "n/a".
[[nodiscard]] std::string format_fixed(double value, int precision);

// Binary (IEC) units: "512 B", "1.5 KiB", "15.9 GiB".
[[nodiscard]] std::string format_bytes(std::uint64_t bytes);

// "12.3%"
[[nodiscard]] std::string format_percent(double value);

// Decimal network units: "100 Mbps", "866.7 Mbps", "2.5 Gbps".
[[nodiscard]] std::string format_bits_per_second(std::uint64_t bps);

// "3d 4h 12m", "5h 0m", "12m", "42s".
[[nodiscard]] std::string format_duration(std::chrono::seconds duration);

}  // namespace sysdiag::units
