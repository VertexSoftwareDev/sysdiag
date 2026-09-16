// CPU utilisation from two cumulative time samples.
//
// Windows' GetSystemTimes reports cumulative idle, kernel and user time where
// *kernel time includes idle time*. Utilisation over an interval is therefore
//     busy  = (kernel_delta + user_delta) - idle_delta
//     usage = busy / (kernel_delta + user_delta)
// Kept free of OS calls so it can be unit-tested.
#pragma once

#include <cstdint>
#include <optional>

namespace sysdiag {

struct CpuTimes {
    std::uint64_t idle = 0;
    std::uint64_t kernel = 0;  // includes idle
    std::uint64_t user = 0;
};

// Returns utilisation in [0, 100], or nullopt when the samples cannot produce
// a meaningful value (no elapsed time, counters moved backwards, overflow).
[[nodiscard]] std::optional<double> compute_cpu_usage(const CpuTimes& first,
                                                      const CpuTimes& second) noexcept;

}  // namespace sysdiag
