// Orchestrates collection: runs only the requested collectors and isolates
// failures so one broken section never prevents the others from reporting.
//
// Collectors are injected as std::function so tests can supply fakes
// (including ones that throw) without touching the operating system.
#pragma once

#include <chrono>
#include <functional>
#include <string>

#include "core/cli.hpp"
#include "core/model.hpp"

namespace sysdiag {

struct Probe {
    std::function<OsInfo()> os;
    std::function<CpuInfo(std::chrono::milliseconds sample_interval)> cpu;
    std::function<MemoryInfo()> memory;
    std::function<DiskSection()> disk;
    std::function<GpuSection()> gpu;
    std::function<NetworkSection()> network;
    std::function<std::chrono::system_clock::time_point()> clock;
};

[[nodiscard]] SystemReport build_report(const cli::Options& options, const Probe& probe);

// "2026-09-16T08:30:05Z"
[[nodiscard]] std::string format_utc_timestamp(std::chrono::system_clock::time_point tp);

}  // namespace sysdiag
