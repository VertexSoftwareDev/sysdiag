// Windows implementations of the section collectors.
//
// Contract: collectors report OS failures through the section's `errors`
// list and fill whatever they could obtain. They only throw on programming
// errors or allocation failure (build_report isolates those per section).
#pragma once

#include <chrono>

#include "core/model.hpp"
#include "core/report_builder.hpp"

namespace sysdiag::win {

[[nodiscard]] OsInfo collect_os();
[[nodiscard]] CpuInfo collect_cpu(std::chrono::milliseconds sample_interval);
[[nodiscard]] MemoryInfo collect_memory();
[[nodiscard]] DiskSection collect_disks();
[[nodiscard]] GpuSection collect_gpus();
[[nodiscard]] NetworkSection collect_network();

// Probe wired to the collectors above.
[[nodiscard]] Probe make_probe();

}  // namespace sysdiag::win
