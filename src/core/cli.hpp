// Command-line parsing. Pure logic: takes already-decoded UTF-8 arguments.
#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <variant>

namespace sysdiag::cli {

// Declaration order is the order sections are printed in.
enum class Section : std::uint8_t { Os, Cpu, Memory, Disk, Gpu, Network };

inline constexpr std::array kAllSections = {Section::Os,   Section::Cpu, Section::Memory,
                                            Section::Disk, Section::Gpu, Section::Network};

[[nodiscard]] std::string_view section_name(Section section) noexcept;

class SectionSet {
  public:
    constexpr void add(Section s) noexcept { bits_ |= mask(s); }
    [[nodiscard]] constexpr bool contains(Section s) const noexcept {
        return (bits_ & mask(s)) != 0;
    }
    [[nodiscard]] constexpr bool empty() const noexcept { return bits_ == 0; }
    [[nodiscard]] static constexpr SectionSet all() noexcept {
        SectionSet set;
        for (const Section s : kAllSections) {
            set.add(s);
        }
        return set;
    }
    friend constexpr bool operator==(SectionSet, SectionSet) = default;

  private:
    static constexpr std::uint32_t mask(Section s) noexcept {
        return std::uint32_t{1} << static_cast<std::uint32_t>(s);
    }
    std::uint32_t bits_ = 0;
};

enum class OutputFormat : std::uint8_t { Text, Json };

inline constexpr std::chrono::milliseconds kDefaultSampleInterval{500};
inline constexpr std::chrono::milliseconds kMinSampleInterval{100};
inline constexpr std::chrono::milliseconds kMaxSampleInterval{5000};

struct Options {
    SectionSet sections = SectionSet::all();
    OutputFormat format = OutputFormat::Text;
    std::chrono::milliseconds cpu_sample_interval = kDefaultSampleInterval;
};

enum class Command : std::uint8_t { Run, Help, Version };

struct Request {
    Command command = Command::Run;
    Options options;
};

struct Error {
    std::string message;  // safe to print: user input inside is sanitized
};

using ParseResult = std::variant<Request, Error>;

// `args` excludes the program name (argv[0]).
[[nodiscard]] ParseResult parse_arguments(std::span<const std::string_view> args);

[[nodiscard]] std::string help_text();
[[nodiscard]] std::string version_text();

}  // namespace sysdiag::cli
