#include <gtest/gtest.h>

#include <limits>

#include "core/units.hpp"

namespace units = sysdiag::units;

namespace {
constexpr auto kMax = std::numeric_limits<std::uint64_t>::max();
}

TEST(Units, SaturatingSubNeverWraps) {
    EXPECT_EQ(units::saturating_sub(10, 3), 7U);
    EXPECT_EQ(units::saturating_sub(3, 10), 0U);
    EXPECT_EQ(units::saturating_sub(0, kMax), 0U);
    EXPECT_EQ(units::saturating_sub(kMax, 0), kMax);
}

TEST(Units, PercentOfZeroIsUnknown) {
    EXPECT_FALSE(units::percent(0, 0).has_value());
    EXPECT_FALSE(units::percent(123, 0).has_value());
}

TEST(Units, PercentBasicValues) {
    EXPECT_DOUBLE_EQ(*units::percent(0, 100), 0.0);
    EXPECT_DOUBLE_EQ(*units::percent(50, 100), 50.0);
    EXPECT_DOUBLE_EQ(*units::percent(100, 100), 100.0);
    EXPECT_NEAR(*units::percent(1, 3), 33.333333, 1e-5);
}

TEST(Units, PercentIsClampedAndHandlesHugeValues) {
    EXPECT_DOUBLE_EQ(*units::percent(200, 100), 100.0);
    EXPECT_DOUBLE_EQ(*units::percent(kMax, kMax), 100.0);
    EXPECT_NEAR(*units::percent(kMax / 2, kMax), 50.0, 1e-9);
}

TEST(Units, UsageFromFree) {
    const auto usage = units::usage_from_free(1000, 250);
    EXPECT_EQ(usage.used, 750U);
    ASSERT_TRUE(usage.percent.has_value());
    EXPECT_DOUBLE_EQ(*usage.percent, 75.0);
}

TEST(Units, UsageFromFreeClampsInconsistentFreeValue) {
    const auto usage = units::usage_from_free(1000, 5000);
    EXPECT_EQ(usage.used, 0U);
    EXPECT_DOUBLE_EQ(*usage.percent, 0.0);
}

TEST(Units, UsageFromFreeWithZeroTotal) {
    const auto usage = units::usage_from_free(0, 0);
    EXPECT_EQ(usage.used, 0U);
    EXPECT_FALSE(usage.percent.has_value());
}

TEST(Units, UsageFromFreeAtLimits) {
    const auto full = units::usage_from_free(kMax, 0);
    EXPECT_EQ(full.used, kMax);
    EXPECT_DOUBLE_EQ(*full.percent, 100.0);
    const auto empty = units::usage_from_free(kMax, kMax);
    EXPECT_EQ(empty.used, 0U);
}

TEST(Units, FormatBytesSmallValues) {
    EXPECT_EQ(units::format_bytes(0), "0 B");
    EXPECT_EQ(units::format_bytes(1), "1 B");
    EXPECT_EQ(units::format_bytes(1023), "1023 B");
}

TEST(Units, FormatBytesBinaryUnits) {
    EXPECT_EQ(units::format_bytes(units::kKiB), "1.0 KiB");
    EXPECT_EQ(units::format_bytes(1536), "1.5 KiB");
    EXPECT_EQ(units::format_bytes(units::kMiB), "1.0 MiB");
    EXPECT_EQ(units::format_bytes(units::kGiB), "1.0 GiB");
    EXPECT_EQ(units::format_bytes(16ULL * units::kGiB), "16.0 GiB");
    EXPECT_EQ(units::format_bytes(2ULL * units::kTiB), "2.0 TiB");
}

TEST(Units, FormatBytesTypicalRamAndDisk) {
    EXPECT_EQ(units::format_bytes(17'079'205'888ULL), "15.9 GiB");    // "16 GB" RAM
    EXPECT_EQ(units::format_bytes(512'110'190'592ULL), "476.9 GiB");  // "512 GB" SSD
}

TEST(Units, FormatBytesPromotesWhenRoundingReaches1024) {
    // 1023.96 MiB would print as "1024.0 MiB" without promotion.
    EXPECT_EQ(units::format_bytes(units::kGiB - 40 * units::kKiB), "1.0 GiB");
    EXPECT_EQ(units::format_bytes(units::kMiB - 1), "1.0 MiB");
}

TEST(Units, FormatBytesMaximum) {
    EXPECT_EQ(units::format_bytes(kMax), "16.0 EiB");
}

TEST(Units, FormatFixedIsLocaleIndependentAndSafe) {
    EXPECT_EQ(units::format_fixed(12.345, 1), "12.3");
    EXPECT_EQ(units::format_fixed(0.0, 1), "0.0");
    EXPECT_EQ(units::format_fixed(-0.0, 1), "0.0");
    EXPECT_EQ(units::format_fixed(-0.04, 1), "0.0");
    EXPECT_EQ(units::format_fixed(100.0, 1), "100.0");
    EXPECT_EQ(units::format_fixed(std::numeric_limits<double>::quiet_NaN(), 1), "n/a");
    EXPECT_EQ(units::format_fixed(std::numeric_limits<double>::infinity(), 1), "n/a");
}

TEST(Units, FormatPercent) {
    EXPECT_EQ(units::format_percent(37.64), "37.6%");
}

TEST(Units, FormatBitsPerSecond) {
    EXPECT_EQ(units::format_bits_per_second(0), "0 bps");
    EXPECT_EQ(units::format_bits_per_second(999), "999 bps");
    EXPECT_EQ(units::format_bits_per_second(100'000'000), "100 Mbps");
    EXPECT_EQ(units::format_bits_per_second(1'000'000'000), "1 Gbps");
    EXPECT_EQ(units::format_bits_per_second(2'500'000'000), "2.5 Gbps");
    EXPECT_EQ(units::format_bits_per_second(866'700'000), "866.7 Mbps");
    EXPECT_EQ(units::format_bits_per_second(kMax), "18446744.1 Tbps");
}

TEST(Units, FormatDuration) {
    using std::chrono::seconds;
    EXPECT_EQ(units::format_duration(seconds(0)), "0s");
    EXPECT_EQ(units::format_duration(seconds(59)), "59s");
    EXPECT_EQ(units::format_duration(seconds(60)), "1m");
    EXPECT_EQ(units::format_duration(seconds(3600)), "1h 0m");
    EXPECT_EQ(units::format_duration(seconds(90061)), "1d 1h 1m");
    EXPECT_EQ(units::format_duration(seconds(-5)), "n/a");
}
