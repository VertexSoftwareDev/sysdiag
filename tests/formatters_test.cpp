#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

#include "core/formatters.hpp"
#include "support/json_validator.hpp"
#include "support/sample_report.hpp"

using sysdiag::format_json;
using sysdiag::format_text;
using sysdiag::test::JsonValidator;
using sysdiag::test::sample_report;

namespace {

bool contains(const std::string& haystack, std::string_view needle) {
    return haystack.find(needle) != std::string::npos;
}

// Asserts that the needles appear in the given order.
void expect_in_order(const std::string& doc, const std::vector<std::string_view>& needles) {
    std::size_t pos = 0;
    for (const auto needle : needles) {
        const std::size_t found = doc.find(needle, pos);
        ASSERT_NE(found, std::string::npos) << "missing or out of order: " << needle;
        pos = found + needle.size();
    }
}

}  // namespace

TEST(JsonFormatter, FullReportIsValidJson) {
    const std::string doc = format_json(sample_report());
    EXPECT_EQ(JsonValidator::validate(doc), "") << doc;
}

TEST(JsonFormatter, OutputIsDeterministic) {
    EXPECT_EQ(format_json(sample_report()), format_json(sample_report()));
}

TEST(JsonFormatter, TopLevelKeyOrderIsStable) {
    expect_in_order(format_json(sample_report()),
                    {R"("schema_version": 1)", R"("tool")", R"("generated_at")", R"("os")",
                     R"("cpu")", R"("memory")", R"("disk")", R"("gpu")", R"("network")"});
}

TEST(JsonFormatter, ValuesUseRawUnits) {
    const std::string doc = format_json(sample_report());
    EXPECT_TRUE(contains(doc, R"("total_bytes": 34359738368)"));
    EXPECT_TRUE(contains(doc, R"("usage_percent": 12.3)"));
    EXPECT_TRUE(contains(doc, R"("vendor_id": 4318)"));
    EXPECT_TRUE(contains(doc, R"("root": "C:\\")"));
    EXPECT_TRUE(contains(doc, R"("ipv4_addresses": [)"));
    EXPECT_TRUE(contains(doc, R"("generated_at": "2026-01-02T03:04:05Z")"));
}

TEST(JsonFormatter, UnrequestedSectionsAreOmitted) {
    auto report = sample_report();
    report.os.reset();
    report.gpu.reset();
    const std::string doc = format_json(report);
    EXPECT_EQ(JsonValidator::validate(doc), "");
    EXPECT_FALSE(contains(doc, R"("os":)"));
    EXPECT_FALSE(contains(doc, R"("gpu":)"));
    EXPECT_TRUE(contains(doc, R"("cpu":)"));
}

TEST(JsonFormatter, MissingValuesAreNullAndErrorsAreListed) {
    sysdiag::SystemReport report;
    report.tool_version = "1.0.0";
    report.generated_at_utc = "2026-01-02T03:04:05Z";
    sysdiag::MemoryInfo mem;
    mem.errors.push_back({"GlobalMemoryStatusEx", 5U, "Access is denied"});
    report.memory = mem;
    sysdiag::CpuInfo cpu;
    cpu.errors.push_back({"collect cpu", std::nullopt, "boom \"quoted\"\n"});
    report.cpu = cpu;

    const std::string doc = format_json(report);
    EXPECT_EQ(JsonValidator::validate(doc), "") << doc;
    EXPECT_TRUE(contains(doc, R"("total_bytes": null)"));
    EXPECT_TRUE(contains(doc, R"("usage_percent": null)"));
    EXPECT_TRUE(contains(doc, R"("operation": "GlobalMemoryStatusEx")"));
    EXPECT_TRUE(contains(doc, R"("code": 5)"));
    EXPECT_TRUE(contains(doc, R"("code": null)"));
    EXPECT_TRUE(contains(doc, R"(boom \"quoted\"\n)"));
}

TEST(JsonFormatter, EmptySectionsHaveEmptyArrays) {
    sysdiag::SystemReport report;
    report.disk = sysdiag::DiskSection{};
    report.network = sysdiag::NetworkSection{};
    const std::string doc = format_json(report);
    EXPECT_EQ(JsonValidator::validate(doc), "");
    EXPECT_TRUE(contains(doc, R"("volumes": [])"));
    EXPECT_TRUE(contains(doc, R"("adapters": [])"));
    EXPECT_TRUE(contains(doc, R"("errors": [])"));
}

TEST(JsonFormatter, HostileStringsStayValid) {
    auto report = sample_report();
    report.os->computer_name = std::string("evil\"\\\x01\x1b\xFF", 9);
    report.gpu->adapters[0].name = "\xC0\x80";
    const std::string doc = format_json(report);
    EXPECT_EQ(JsonValidator::validate(doc), "") << doc;
}

TEST(TextFormatter, ContainsHumanReadableValues) {
    const std::string out = format_text(sample_report());
    for (const std::string_view needle : {"Operating System",
                                          "Windows 11 Pro",
                                          "23H2 (build 22631.3880)",
                                          "1d 2h 3m",
                                          "CPU",
                                          "AMD Ryzen 7 5800X",
                                          "12.3% (sampled over 500 ms)",
                                          "Memory",
                                          "32.0 GiB",
                                          "12.0 GiB (37.5%)",
                                          "Disks",
                                          "C:\\ [fixed, NTFS] \"Windows\"",
                                          "375.0 GiB (75.0%)",
                                          "D:\\ [optical] - not ready",
                                          "GPU",
                                          "[0] NVIDIA GeForce RTX 3070",
                                          "0x10DE:0x2484",
                                          "Dedicated VRAM      8.0 GiB",
                                          "Network",
                                          "Ethernet (Intel(R) Ethernet Connection)",
                                          "192.168.1.10/24",
                                          "fe80::1/64",
                                          "192.168.1.1",
                                          "1 Gbps"}) {
        EXPECT_TRUE(contains(out, needle)) << "missing: " << needle << "\n" << out;
    }
}

TEST(TextFormatter, SectionOrder) {
    expect_in_order(format_text(sample_report()),
                    {"Operating System", "CPU", "Memory", "Disks", "GPU", "Network"});
}

TEST(TextFormatter, UnavailableValuesAndErrors) {
    sysdiag::SystemReport report;
    report.tool_version = "1.0.0";
    sysdiag::MemoryInfo mem;
    mem.errors.push_back({"GlobalMemoryStatusEx", 5U, "Access is denied"});
    report.memory = mem;
    sysdiag::GpuSection gpu;
    gpu.errors.push_back({"CreateDXGIFactory1", 0x887A0004U, "unsupported"});
    report.gpu = gpu;

    const std::string out = format_text(report);
    EXPECT_TRUE(contains(out, "Total                 n/a"));
    EXPECT_TRUE(contains(out, "! GlobalMemoryStatusEx failed: Access is denied (error 5)"));
    EXPECT_TRUE(contains(out, "! CreateDXGIFactory1 failed: unsupported (0x887A0004)"));
    EXPECT_FALSE(contains(out, "no hardware graphics adapters"));  // error explains it instead
}

TEST(TextFormatter, EmptyListsAreExplained) {
    sysdiag::SystemReport report;
    report.disk = sysdiag::DiskSection{};
    report.gpu = sysdiag::GpuSection{};
    report.network = sysdiag::NetworkSection{};
    const std::string out = format_text(report);
    EXPECT_TRUE(contains(out, "(no volumes found)"));
    EXPECT_TRUE(contains(out, "(no hardware graphics adapters found)"));
    EXPECT_TRUE(contains(out, "(no active network adapters found)"));
}

TEST(TextFormatter, AsymmetricLinkSpeedAndWow64) {
    auto report = sample_report();
    report.network->adapters[0].transmit_bps = 866'700'000;
    report.network->adapters[0].receive_bps = 1'200'000'000;
    report.os->process_architecture = "x86";
    const std::string out = format_text(report);
    EXPECT_TRUE(contains(out, "866.7 Mbps up / 1.2 Gbps down"));
    EXPECT_TRUE(contains(out, "x64 (this process: x86)"));
}

TEST(TextFormatter, ControlCharactersFromOsAreNeutralised) {
    auto report = sample_report();
    report.cpu->name = "Evil\x1b]0;pwned\x07";
    report.network->adapters[0].name = "net\x1b[2J";
    const std::string out = format_text(report);
    EXPECT_FALSE(contains(out, "\x1b"));
    EXPECT_FALSE(contains(out, "\x07"));
}
