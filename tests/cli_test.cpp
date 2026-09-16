#include <gtest/gtest.h>

#include <initializer_list>
#include <string_view>
#include <vector>

#include "core/cli.hpp"

namespace cli = sysdiag::cli;
using cli::Section;

namespace {

cli::ParseResult parse(std::initializer_list<std::string_view> args) {
    const std::vector<std::string_view> v(args);
    return cli::parse_arguments(v);
}

cli::Request expect_request(const cli::ParseResult& result) {
    if (const auto* error = std::get_if<cli::Error>(&result)) {
        ADD_FAILURE() << "unexpected parse error: " << error->message;
        return {};
    }
    return std::get<cli::Request>(result);
}

std::string expect_error(const cli::ParseResult& result) {
    if (const auto* error = std::get_if<cli::Error>(&result)) {
        return error->message;
    }
    ADD_FAILURE() << "expected a parse error";
    return {};
}

}  // namespace

TEST(Cli, NoArgumentsSelectsEverythingAsText) {
    const auto req = expect_request(parse({}));
    EXPECT_EQ(req.command, cli::Command::Run);
    EXPECT_TRUE(req.options.sections == cli::SectionSet::all());
    EXPECT_EQ(req.options.format, cli::OutputFormat::Text);
    EXPECT_EQ(req.options.cpu_sample_interval, cli::kDefaultSampleInterval);
}

TEST(Cli, SingleSection) {
    const auto req = expect_request(parse({"cpu"}));
    EXPECT_TRUE(req.options.sections.contains(Section::Cpu));
    EXPECT_FALSE(req.options.sections.contains(Section::Memory));
    EXPECT_FALSE(req.options.sections.contains(Section::Os));
}

TEST(Cli, MultipleSectionsAndAliases) {
    const auto req = expect_request(parse({"ram", "net", "disk"}));
    EXPECT_TRUE(req.options.sections.contains(Section::Memory));
    EXPECT_TRUE(req.options.sections.contains(Section::Network));
    EXPECT_TRUE(req.options.sections.contains(Section::Disk));
    EXPECT_FALSE(req.options.sections.contains(Section::Gpu));
}

TEST(Cli, SectionNamesAreCaseInsensitive) {
    const auto req = expect_request(parse({"GPU", "Memory"}));
    EXPECT_TRUE(req.options.sections.contains(Section::Gpu));
    EXPECT_TRUE(req.options.sections.contains(Section::Memory));
}

TEST(Cli, DuplicateSectionsAreHarmless) {
    const auto req = expect_request(parse({"cpu", "cpu"}));
    EXPECT_TRUE(req.options.sections.contains(Section::Cpu));
}

TEST(Cli, AllKeyword) {
    const auto req = expect_request(parse({"cpu", "all"}));
    EXPECT_TRUE(req.options.sections == cli::SectionSet::all());
}

TEST(Cli, JsonFlag) {
    const auto req = expect_request(parse({"disk", "--json"}));
    EXPECT_EQ(req.options.format, cli::OutputFormat::Json);
    EXPECT_TRUE(req.options.sections.contains(Section::Disk));
}

TEST(Cli, JsonFlagTwiceIsAnError) {
    EXPECT_NE(expect_error(parse({"--json", "--json"})).find("more than once"), std::string::npos);
}

TEST(Cli, HelpAndVersion) {
    EXPECT_EQ(expect_request(parse({"--help"})).command, cli::Command::Help);
    EXPECT_EQ(expect_request(parse({"-h"})).command, cli::Command::Help);
    EXPECT_EQ(expect_request(parse({"/?"})).command, cli::Command::Help);
    EXPECT_EQ(expect_request(parse({"--version"})).command, cli::Command::Version);
    EXPECT_EQ(expect_request(parse({"-V"})).command, cli::Command::Version);
}

TEST(Cli, HelpWinsOverInvalidArguments) {
    EXPECT_EQ(expect_request(parse({"bogus", "--help"})).command, cli::Command::Help);
    EXPECT_EQ(expect_request(parse({"--version", "--help"})).command, cli::Command::Help);
}

TEST(Cli, SampleIntervalBothSyntaxes) {
    EXPECT_EQ(expect_request(parse({"--sample-ms", "1000"})).options.cpu_sample_interval.count(),
              1000);
    EXPECT_EQ(expect_request(parse({"--sample-ms=250"})).options.cpu_sample_interval.count(), 250);
}

TEST(Cli, SampleIntervalBoundaries) {
    EXPECT_EQ(expect_request(parse({"--sample-ms=100"})).options.cpu_sample_interval.count(), 100);
    EXPECT_EQ(expect_request(parse({"--sample-ms=5000"})).options.cpu_sample_interval.count(),
              5000);
    EXPECT_NE(expect_error(parse({"--sample-ms=99"})).find("between"), std::string::npos);
    EXPECT_NE(expect_error(parse({"--sample-ms=5001"})).find("between"), std::string::npos);
    EXPECT_NE(expect_error(parse({"--sample-ms=0"})).find("between"), std::string::npos);
}

TEST(Cli, SampleIntervalRejectsMalformedValues) {
    for (const std::string_view bad : {"abc", "-5", "+5", "10ms", "1.5", " 100", "100 ", "0x64"}) {
        const auto message = expect_error(parse({"--sample-ms", bad}));
        EXPECT_FALSE(message.empty()) << "value: " << bad;
    }
}

TEST(Cli, SampleIntervalOverflow) {
    EXPECT_NE(expect_error(parse({"--sample-ms=99999999999999999999"})).find("out of range"),
              std::string::npos);
}

TEST(Cli, SampleIntervalMissingValue) {
    EXPECT_NE(expect_error(parse({"--sample-ms"})).find("requires a value"), std::string::npos);
    EXPECT_NE(expect_error(parse({"--sample-ms="})).find("requires a value"), std::string::npos);
}

TEST(Cli, SampleIntervalDoesNotSwallowNextOption) {
    EXPECT_FALSE(expect_error(parse({"--sample-ms", "--json"})).empty());
}

TEST(Cli, SampleIntervalGivenTwice) {
    EXPECT_NE(expect_error(parse({"--sample-ms=200", "--sample-ms=300"})).find("more than once"),
              std::string::npos);
}

TEST(Cli, UnknownOptionAndSection) {
    EXPECT_NE(expect_error(parse({"--verbose"})).find("unknown option '--verbose'"),
              std::string::npos);
    EXPECT_NE(expect_error(parse({"cpux"})).find("unknown section 'cpux'"), std::string::npos);
    EXPECT_FALSE(expect_error(parse({""})).empty());
}

TEST(Cli, ErrorMessagesNeutraliseControlCharacters) {
    const std::string message = expect_error(parse({"\x1b[31mred"}));
    EXPECT_EQ(message.find('\x1b'), std::string::npos);
    EXPECT_NE(message.find("?[31mred"), std::string::npos);
}

TEST(Cli, ErrorMessagesTruncateHugeInput) {
    const std::string huge(100'000, 'x');
    const std::string message = expect_error(parse({huge}));
    EXPECT_LT(message.size(), 200U);
}

TEST(Cli, SectionNames) {
    EXPECT_EQ(cli::section_name(Section::Os), "os");
    EXPECT_EQ(cli::section_name(Section::Network), "network");
}

TEST(Cli, HelpTextMentionsEveryOption) {
    const std::string help = cli::help_text();
    for (const std::string_view needle : {"--json", "--sample-ms", "--help", "--version", "cpu",
                                          "memory", "disk", "gpu", "network", "os"}) {
        EXPECT_NE(help.find(needle), std::string::npos) << needle;
    }
}

TEST(Cli, VersionText) {
    EXPECT_EQ(cli::version_text().rfind("sysdiag ", 0), 0U);
}
