#include <gtest/gtest.h>

#include <chrono>
#include <new>
#include <stdexcept>

#include "core/report_builder.hpp"
#include "support/sample_report.hpp"

using namespace std::chrono;
using sysdiag::build_report;
using sysdiag::format_utc_timestamp;
using sysdiag::Probe;
namespace cli = sysdiag::cli;

namespace {

Probe fake_probe() {
    Probe p;
    p.os = [] { return sysdiag::test::sample_os(); };
    p.cpu = [](milliseconds interval) {
        auto cpu = sysdiag::test::sample_cpu();
        cpu.sample_interval_ms = static_cast<std::uint32_t>(interval.count());
        return cpu;
    };
    p.memory = [] { return sysdiag::test::sample_memory(); };
    p.disk = [] { return sysdiag::test::sample_disk(); };
    p.gpu = [] { return sysdiag::test::sample_gpu(); };
    p.network = [] { return sysdiag::test::sample_network(); };
    p.clock = [] { return system_clock::time_point{seconds{1'767'323'045}}; };
    return p;
}

cli::Options only(std::initializer_list<cli::Section> sections) {
    cli::Options o;
    o.sections = {};
    for (const auto s : sections) {
        o.sections.add(s);
    }
    return o;
}

}  // namespace

TEST(ReportBuilder, CollectsEverythingByDefault) {
    const auto report = build_report(cli::Options{}, fake_probe());
    EXPECT_TRUE(report.os.has_value());
    EXPECT_TRUE(report.cpu.has_value());
    EXPECT_TRUE(report.memory.has_value());
    EXPECT_TRUE(report.disk.has_value());
    EXPECT_TRUE(report.gpu.has_value());
    EXPECT_TRUE(report.network.has_value());
    EXPECT_EQ(report.generated_at_utc, "2026-01-02T03:04:05Z");
    EXPECT_FALSE(report.tool_version.empty());
}

TEST(ReportBuilder, OnlyRequestedCollectorsRun) {
    int calls = 0;
    auto probe = fake_probe();
    probe.gpu = [&] {
        ++calls;
        return sysdiag::GpuSection{};
    };
    const auto report = build_report(only({cli::Section::Memory}), probe);
    EXPECT_EQ(calls, 0);
    EXPECT_TRUE(report.memory.has_value());
    EXPECT_FALSE(report.cpu.has_value());
    EXPECT_FALSE(report.gpu.has_value());
}

TEST(ReportBuilder, PassesSampleInterval) {
    cli::Options options = only({cli::Section::Cpu});
    options.cpu_sample_interval = milliseconds(1234);
    const auto report = build_report(options, fake_probe());
    ASSERT_TRUE(report.cpu.has_value());
    EXPECT_EQ(report.cpu->sample_interval_ms.value_or(0), 1234U);
}

TEST(ReportBuilder, ThrowingCollectorOnlyAffectsItsSection) {
    auto probe = fake_probe();
    probe.cpu = [](milliseconds) -> sysdiag::CpuInfo {
        throw std::runtime_error("driver exploded");
    };
    probe.disk = []() -> sysdiag::DiskSection { throw std::bad_alloc(); };
    probe.gpu = []() -> sysdiag::GpuSection { throw 42; };

    sysdiag::SystemReport report;
    EXPECT_NO_THROW(report = build_report(cli::Options{}, probe));

    ASSERT_TRUE(report.cpu.has_value());
    EXPECT_FALSE(report.cpu->name.has_value());
    ASSERT_EQ(report.cpu->errors.size(), 1U);
    EXPECT_EQ(report.cpu->errors[0].operation, "collect cpu");
    EXPECT_EQ(report.cpu->errors[0].message, "driver exploded");

    ASSERT_TRUE(report.disk.has_value());
    EXPECT_EQ(report.disk->errors.size(), 1U);
    ASSERT_TRUE(report.gpu.has_value());
    ASSERT_EQ(report.gpu->errors.size(), 1U);
    EXPECT_EQ(report.gpu->errors[0].message, "unknown internal error");

    // Unaffected sections are intact.
    ASSERT_TRUE(report.memory.has_value());
    EXPECT_TRUE(report.memory->total_bytes.has_value());
    ASSERT_TRUE(report.network.has_value());
    EXPECT_EQ(report.network->adapters.size(), 1U);
}

TEST(ReportBuilder, MissingCollectorIsReportedNotFatal) {
    Probe probe;  // nothing wired, no clock
    const auto report = build_report(only({cli::Section::Os, cli::Section::Network}), probe);
    ASSERT_TRUE(report.os.has_value());
    ASSERT_EQ(report.os->errors.size(), 1U);
    EXPECT_EQ(report.os->errors[0].message, "not supported on this platform");
    ASSERT_TRUE(report.network.has_value());
    EXPECT_EQ(report.network->errors.size(), 1U);
    EXPECT_EQ(report.generated_at_utc.size(), 20U);  // real clock used
}

TEST(ReportBuilder, PartialSectionDataIsPreserved) {
    auto probe = fake_probe();
    probe.memory = [] {
        sysdiag::MemoryInfo mem;
        mem.errors.push_back({"GlobalMemoryStatusEx", 1U, "failed"});
        return mem;
    };
    const auto report = build_report(only({cli::Section::Memory}), probe);
    ASSERT_TRUE(report.memory.has_value());
    EXPECT_FALSE(report.memory->total_bytes.has_value());
    EXPECT_EQ(report.memory->errors.size(), 1U);
}

TEST(Timestamp, FormatsUtc) {
    EXPECT_EQ(format_utc_timestamp(system_clock::time_point{}), "1970-01-01T00:00:00Z");
    EXPECT_EQ(format_utc_timestamp(system_clock::time_point{seconds{951'782'400}}),
              "2000-02-29T00:00:00Z");  // leap day
    EXPECT_EQ(
        format_utc_timestamp(system_clock::time_point{seconds{1'767'323'045} + milliseconds{999}}),
        "2026-01-02T03:04:05Z");  // sub-second part is truncated
}
