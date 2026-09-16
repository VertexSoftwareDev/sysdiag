#include <algorithm>

#include "core/units.hpp"
#include "platform/windows/collectors.hpp"
#include "platform/windows/win_util.hpp"

namespace sysdiag::win {

MemoryInfo collect_memory() {
    MemoryInfo info;
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (::GlobalMemoryStatusEx(&status) == FALSE) {
        info.errors.push_back(win32_error("GlobalMemoryStatusEx", ::GetLastError()));
        return info;
    }
    const auto usage = units::usage_from_free(status.ullTotalPhys, status.ullAvailPhys);
    info.total_bytes = status.ullTotalPhys;
    info.available_bytes = std::min(status.ullAvailPhys, status.ullTotalPhys);
    info.used_bytes = usage.used;
    info.usage_percent = usage.percent;
    return info;
}

}  // namespace sysdiag::win
