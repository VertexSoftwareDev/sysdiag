#include "core/units.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <string_view>

namespace sysdiag::units {

std::optional<double> percent(std::uint64_t part, std::uint64_t whole) noexcept {
    if (whole == 0) {
        return std::nullopt;
    }
    const double ratio = static_cast<double>(part) / static_cast<double>(whole);
    return std::clamp(ratio * 100.0, 0.0, 100.0);
}

Usage usage_from_free(std::uint64_t total, std::uint64_t free) noexcept {
    const std::uint64_t clamped_free = std::min(free, total);
    const std::uint64_t used = total - clamped_free;
    return Usage{used, percent(used, total)};
}

std::string format_fixed(double value, int precision) {
    if (!std::isfinite(value)) {
        return "n/a";
    }
    std::array<char, 64> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value,
                                      std::chars_format::fixed, precision);
    if (result.ec != std::errc{}) {
        return "n/a";
    }
    std::string text(buffer.data(), result.ptr);
    if (text == "-0.0" || text == "-0") {  // avoid printing negative zero
        text.erase(0, 1);
    }
    return text;
}

std::string format_bytes(std::uint64_t bytes) {
    static constexpr std::array<std::string_view, 7> kUnits = {"B",   "KiB", "MiB", "GiB",
                                                               "TiB", "PiB", "EiB"};
    if (bytes < kKiB) {
        return std::to_string(bytes) + " B";
    }
    std::size_t unit = 0;
    double value = static_cast<double>(bytes);
    while (value >= 1024.0 && unit + 1 < kUnits.size()) {
        value /= 1024.0;
        ++unit;
    }
    // Rounding can push e.g. 1023.96 MiB to "1024.0 MiB"; promote instead.
    if (std::round(value * 10.0) / 10.0 >= 1024.0 && unit + 1 < kUnits.size()) {
        value /= 1024.0;
        ++unit;
    }
    return format_fixed(value, 1) + " " + std::string(kUnits[unit]);
}

std::string format_percent(double value) {
    return format_fixed(value, 1) + "%";
}

std::string format_bits_per_second(std::uint64_t bps) {
    struct Scale {
        std::uint64_t factor;
        std::string_view suffix;
    };
    static constexpr std::array<Scale, 4> kScales = {{{1'000'000'000'000ULL, "Tbps"},
                                                      {1'000'000'000ULL, "Gbps"},
                                                      {1'000'000ULL, "Mbps"},
                                                      {1'000ULL, "Kbps"}}};
    for (const auto& scale : kScales) {
        if (bps >= scale.factor) {
            if (bps % scale.factor == 0) {
                return std::to_string(bps / scale.factor) + " " + std::string(scale.suffix);
            }
            const double value = static_cast<double>(bps) / static_cast<double>(scale.factor);
            return format_fixed(value, 1) + " " + std::string(scale.suffix);
        }
    }
    return std::to_string(bps) + " bps";
}

std::string format_duration(std::chrono::seconds duration) {
    const auto total = duration.count();
    if (total < 0) {
        return "n/a";
    }
    const auto days = total / 86'400;
    const auto hours = (total % 86'400) / 3'600;
    const auto minutes = (total % 3'600) / 60;
    if (days > 0) {
        return std::to_string(days) + "d " + std::to_string(hours) + "h " +
               std::to_string(minutes) + "m";
    }
    if (hours > 0) {
        return std::to_string(hours) + "h " + std::to_string(minutes) + "m";
    }
    if (minutes > 0) {
        return std::to_string(minutes) + "m";
    }
    return std::to_string(total) + "s";
}

}  // namespace sysdiag::units
