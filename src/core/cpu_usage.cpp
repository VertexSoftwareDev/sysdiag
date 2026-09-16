#include "core/cpu_usage.hpp"

#include <algorithm>
#include <limits>

namespace sysdiag {

std::optional<double> compute_cpu_usage(const CpuTimes& first, const CpuTimes& second) noexcept {
    if (second.idle < first.idle || second.kernel < first.kernel || second.user < first.user) {
        return std::nullopt;
    }
    const std::uint64_t idle = second.idle - first.idle;
    const std::uint64_t kernel = second.kernel - first.kernel;
    const std::uint64_t user = second.user - first.user;

    if (kernel > std::numeric_limits<std::uint64_t>::max() - user) {
        return std::nullopt;
    }
    const std::uint64_t total = kernel + user;
    if (total == 0) {
        return std::nullopt;  // no time elapsed between samples
    }
    // Idle can never legitimately exceed total; clamp inconsistent data to 0% busy.
    const std::uint64_t busy = total > idle ? total - idle : 0;
    const double usage = static_cast<double>(busy) / static_cast<double>(total) * 100.0;
    return std::clamp(usage, 0.0, 100.0);
}

}  // namespace sysdiag
