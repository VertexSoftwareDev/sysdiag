#include <gtest/gtest.h>

#include <limits>

#include "core/cpu_usage.hpp"

using sysdiag::compute_cpu_usage;
using sysdiag::CpuTimes;

TEST(CpuUsage, IdentialSamplesAreUnknown) {
    // First-measurement special case: no elapsed time means no meaningful value.
    const CpuTimes t{100, 500, 300};
    EXPECT_FALSE(compute_cpu_usage(t, t).has_value());
}

TEST(CpuUsage, FullyIdleIsZero) {
    // Kernel time includes idle time: 1000 idle ticks show up in kernel too.
    const auto usage = compute_cpu_usage({0, 0, 0}, {1000, 1000, 0});
    ASSERT_TRUE(usage.has_value());
    EXPECT_DOUBLE_EQ(*usage, 0.0);
}

TEST(CpuUsage, FullyBusyIsHundred) {
    const auto usage = compute_cpu_usage({0, 0, 0}, {0, 400, 600});
    ASSERT_TRUE(usage.has_value());
    EXPECT_DOUBLE_EQ(*usage, 100.0);
}

TEST(CpuUsage, MixedLoad) {
    // total = 600 + 400 = 1000, idle = 250 -> busy = 750 -> 75%
    const auto usage = compute_cpu_usage({1000, 2000, 3000}, {1250, 2600, 3400});
    ASSERT_TRUE(usage.has_value());
    EXPECT_DOUBLE_EQ(*usage, 75.0);
}

TEST(CpuUsage, CountersGoingBackwardsAreRejected) {
    EXPECT_FALSE(compute_cpu_usage({10, 10, 10}, {5, 20, 20}).has_value());
    EXPECT_FALSE(compute_cpu_usage({10, 10, 10}, {20, 5, 20}).has_value());
    EXPECT_FALSE(compute_cpu_usage({10, 10, 10}, {20, 20, 5}).has_value());
}

TEST(CpuUsage, IdleGreaterThanTotalClampsToZero) {
    const auto usage = compute_cpu_usage({0, 0, 0}, {5000, 100, 100});
    ASSERT_TRUE(usage.has_value());
    EXPECT_DOUBLE_EQ(*usage, 0.0);
}

TEST(CpuUsage, OverflowingTotalIsRejected) {
    constexpr auto kMax = std::numeric_limits<std::uint64_t>::max();
    EXPECT_FALSE(compute_cpu_usage({0, 0, 0}, {0, kMax, 1}).has_value());
}

TEST(CpuUsage, ResultAlwaysWithinRange) {
    for (std::uint64_t idle = 0; idle <= 1000; idle += 50) {
        const auto usage = compute_cpu_usage({0, 0, 0}, {idle, 700, 300});
        ASSERT_TRUE(usage.has_value());
        EXPECT_GE(*usage, 0.0);
        EXPECT_LE(*usage, 100.0);
    }
}
