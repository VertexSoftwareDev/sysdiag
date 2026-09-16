#include "core/report_builder.hpp"

#include <exception>
#include <new>
#include <utility>

#include "sysdiag/version.hpp"

namespace sysdiag {
namespace {

template <typename SectionT, typename Fn, typename... Args>
SectionT run_guarded(std::string_view section_name, const Fn& fn, Args&&... args) {
    const std::string operation = "collect " + std::string(section_name);
    if (!fn) {
        SectionT empty{};
        empty.errors.push_back({operation, std::nullopt, "not supported on this platform"});
        return empty;
    }
    // A collector should report problems through `errors`, but an exception
    // (e.g. std::bad_alloc) must still only affect its own section.
    try {
        return fn(std::forward<Args>(args)...);
    } catch (const std::exception& e) {
        SectionT failed{};
        failed.errors.push_back({operation, std::nullopt, e.what()});
        return failed;
    } catch (...) {
        SectionT failed{};
        failed.errors.push_back({operation, std::nullopt, "unknown internal error"});
        return failed;
    }
}

std::string two_digits(unsigned value) {
    std::string s = std::to_string(value);
    return s.size() < 2 ? "0" + s : s;
}

}  // namespace

std::string format_utc_timestamp(std::chrono::system_clock::time_point tp) {
    using namespace std::chrono;
    const auto secs = floor<seconds>(tp);
    const auto day_point = floor<days>(secs);
    const year_month_day ymd{day_point};
    const hh_mm_ss hms{secs - day_point};
    if (!ymd.ok()) {
        return "unknown";
    }
    const int year = static_cast<int>(ymd.year());
    std::string y = std::to_string(year);
    if (year >= 0 && y.size() < 4) {
        y.insert(0, 4 - y.size(), '0');
    }
    return y + "-" + two_digits(static_cast<unsigned>(ymd.month())) + "-" +
           two_digits(static_cast<unsigned>(ymd.day())) + "T" +
           two_digits(static_cast<unsigned>(hms.hours().count())) + ":" +
           two_digits(static_cast<unsigned>(hms.minutes().count())) + ":" +
           two_digits(static_cast<unsigned>(hms.seconds().count())) + "Z";
}

SystemReport build_report(const cli::Options& options, const Probe& probe) {
    using cli::Section;
    SystemReport report;
    report.tool_version = std::string(kToolVersion);
    const auto now = probe.clock ? probe.clock() : std::chrono::system_clock::now();
    report.generated_at_utc = format_utc_timestamp(now);

    const auto& wanted = options.sections;
    if (wanted.contains(Section::Os)) {
        report.os = run_guarded<OsInfo>("os", probe.os);
    }
    if (wanted.contains(Section::Cpu)) {
        report.cpu = run_guarded<CpuInfo>("cpu", probe.cpu, options.cpu_sample_interval);
    }
    if (wanted.contains(Section::Memory)) {
        report.memory = run_guarded<MemoryInfo>("memory", probe.memory);
    }
    if (wanted.contains(Section::Disk)) {
        report.disk = run_guarded<DiskSection>("disk", probe.disk);
    }
    if (wanted.contains(Section::Gpu)) {
        report.gpu = run_guarded<GpuSection>("gpu", probe.gpu);
    }
    if (wanted.contains(Section::Network)) {
        report.network = run_guarded<NetworkSection>("network", probe.network);
    }
    return report;
}

}  // namespace sysdiag
