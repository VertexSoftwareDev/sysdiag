#include "core/cli.hpp"

#include <charconv>
#include <optional>

#include "core/text.hpp"
#include "sysdiag/version.hpp"

namespace sysdiag::cli {
namespace {

struct SectionAlias {
    std::string_view name;
    Section section;
};

constexpr std::array kSectionAliases = {
    SectionAlias{"os", Section::Os},           SectionAlias{"cpu", Section::Cpu},
    SectionAlias{"memory", Section::Memory},   SectionAlias{"ram", Section::Memory},
    SectionAlias{"disk", Section::Disk},       SectionAlias{"gpu", Section::Gpu},
    SectionAlias{"network", Section::Network}, SectionAlias{"net", Section::Network},
};

constexpr std::string_view kSampleOption = "--sample-ms";

std::optional<Section> lookup_section(std::string_view word) noexcept {
    for (const auto& alias : kSectionAliases) {
        if (text::iequals(word, alias.name)) {
            return alias.section;
        }
    }
    return std::nullopt;
}

bool is_help(std::string_view arg) noexcept {
    return arg == "-h" || arg == "--help" || arg == "/?";
}

bool is_version(std::string_view arg) noexcept {
    return arg == "-V" || arg == "--version";
}

std::variant<std::chrono::milliseconds, Error> parse_sample_interval(std::string_view value) {
    const auto quoted = "'" + text::sanitize_for_display(value) + "'";
    if (value.empty()) {
        return Error{"option " + std::string(kSampleOption) + " requires a value"};
    }
    std::uint32_t parsed = 0;
    const auto* const end = value.data() + value.size();
    const auto [ptr, ec] = std::from_chars(value.data(), end, parsed);
    if (ec == std::errc::result_out_of_range) {
        return Error{"value " + quoted + " for " + std::string(kSampleOption) + " is out of range"};
    }
    if (ec != std::errc{} || ptr != end) {
        return Error{"invalid value " + quoted + " for " + std::string(kSampleOption) +
                     " (expected a whole number of milliseconds)"};
    }
    const std::chrono::milliseconds interval{parsed};
    if (interval < kMinSampleInterval || interval > kMaxSampleInterval) {
        return Error{"value " + quoted + " for " + std::string(kSampleOption) +
                     " must be between " + std::to_string(kMinSampleInterval.count()) + " and " +
                     std::to_string(kMaxSampleInterval.count())};
    }
    return interval;
}

}  // namespace

std::string_view section_name(Section section) noexcept {
    switch (section) {
        case Section::Os: return "os";
        case Section::Cpu: return "cpu";
        case Section::Memory: return "memory";
        case Section::Disk: return "disk";
        case Section::Gpu: return "gpu";
        case Section::Network: return "network";
    }
    return "unknown";
}

ParseResult parse_arguments(std::span<const std::string_view> args) {
    // --help / --version take precedence over everything else, as is conventional.
    for (const auto arg : args) {
        if (is_help(arg)) {
            return Request{Command::Help, {}};
        }
    }
    for (const auto arg : args) {
        if (is_version(arg)) {
            return Request{Command::Version, {}};
        }
    }

    Options options;
    SectionSet selected;
    bool json_seen = false;
    bool sample_seen = false;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view arg = args[i];

        if (arg == "--json") {
            if (json_seen) {
                return Error{"option --json was given more than once"};
            }
            json_seen = true;
            options.format = OutputFormat::Json;
            continue;
        }

        if (arg == kSampleOption || arg.starts_with(std::string(kSampleOption) + "=")) {
            if (sample_seen) {
                return Error{"option " + std::string(kSampleOption) + " was given more than once"};
            }
            sample_seen = true;
            std::string_view value;
            if (arg.size() > kSampleOption.size()) {
                value = arg.substr(kSampleOption.size() + 1);
            } else if (i + 1 < args.size()) {
                value = args[++i];
            }
            auto parsed = parse_sample_interval(value);
            if (auto* error = std::get_if<Error>(&parsed)) {
                return std::move(*error);
            }
            options.cpu_sample_interval = std::get<std::chrono::milliseconds>(parsed);
            continue;
        }

        if (arg.starts_with("-")) {
            return Error{"unknown option '" + text::sanitize_for_display(arg) + "'"};
        }

        if (text::iequals(arg, "all")) {
            selected = SectionSet::all();
            continue;
        }

        if (const auto section = lookup_section(arg)) {
            selected.add(*section);
            continue;
        }

        return Error{"unknown section '" + text::sanitize_for_display(arg) +
                     "' (valid: all, os, cpu, memory|ram, disk, gpu, network|net)"};
    }

    if (!selected.empty()) {
        options.sections = selected;
    }
    return Request{Command::Run, options};
}

std::string help_text() {
    std::string help;
    help += std::string(kToolName) + " " + std::string(kToolVersion) +
            " - Windows system diagnostics\n\n";
    help += "Usage:\n";
    help += "  sysdiag [SECTION...] [--json] [--sample-ms <ms>]\n";
    help += "  sysdiag --help | --version\n\n";
    help += "Sections (default: all):\n";
    help += "  all            Every section below\n";
    help += "  os             Windows version, architecture, computer name, uptime\n";
    help += "  cpu            Processor model, cores, threads, current utilisation\n";
    help += "  memory, ram    Physical memory usage\n";
    help += "  disk           Mounted volumes and their capacity\n";
    help += "  gpu            Graphics adapters and video memory\n";
    help += "  network, net   Active network adapters and addresses\n\n";
    help += "Options:\n";
    help += "  --json            Print machine-readable JSON instead of text\n";
    help += "  --sample-ms <ms>  CPU utilisation sampling window, " +
            std::to_string(kMinSampleInterval.count()) + "-" +
            std::to_string(kMaxSampleInterval.count()) + " (default " +
            std::to_string(kDefaultSampleInterval.count()) + ")\n";
    help += "  -h, --help        Show this help and exit\n";
    help += "  -V, --version     Show version information and exit\n\n";
    help += "Examples:\n";
    help += "  sysdiag\n";
    help += "  sysdiag cpu memory\n";
    help += "  sysdiag disk --json\n";
    help += "  sysdiag cpu --sample-ms 1000\n\n";
    help += "Exit status:\n";
    help += "  0  success (individual values may still be unavailable; see output)\n";
    help += "  1  unexpected internal or output error\n";
    help += "  2  invalid command-line usage\n";
    return help;
}

std::string version_text() {
    return std::string(kToolName) + " " + std::string(kToolVersion) + "\n";
}

}  // namespace sysdiag::cli
