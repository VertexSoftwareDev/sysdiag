#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstring>
#include <thread>
#include <vector>

#include "core/cli.hpp"
#include "core/cpu_usage.hpp"
#include "core/text.hpp"
#include "platform/windows/collectors.hpp"
#include "platform/windows/win_util.hpp"

#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
#define SYSDIAG_HAS_CPUID 1
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <cpuid.h>
#endif
#else
#define SYSDIAG_HAS_CPUID 0
#endif

namespace sysdiag::win {
namespace {

constexpr const wchar_t* kCpuRegistryKey = L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";

#if SYSDIAG_HAS_CPUID
std::array<std::uint32_t, 4> cpuid(std::uint32_t leaf) noexcept {
    std::array<std::uint32_t, 4> regs{};
#if defined(_MSC_VER)
    std::array<int, 4> raw{};
    ::__cpuid(raw.data(), static_cast<int>(leaf));
    for (std::size_t i = 0; i < regs.size(); ++i) {
        regs[i] = static_cast<std::uint32_t>(raw[i]);
    }
#else
    unsigned int a = 0, b = 0, c = 0, d = 0;
    if (::__get_cpuid(leaf, &a, &b, &c, &d) != 0) {
        regs = {a, b, c, d};
    }
#endif
    return regs;
}

// Processor brand string from CPUID leaves 0x80000002..4 (EAX,EBX,ECX,EDX each).
std::optional<std::string> cpuid_brand_string() {
    if (cpuid(0x80000000U)[0] < 0x80000004U) {
        return std::nullopt;
    }
    std::array<char, 3 * 16 + 1> brand{};
    for (std::uint32_t i = 0; i < 3; ++i) {
        const auto regs = cpuid(0x80000002U + i);
        std::memcpy(brand.data() + i * 16, regs.data(), 16);
    }
    const auto trimmed = text::trim(std::string_view(brand.data(), ::strnlen(brand.data(), 48)));
    if (trimmed.empty()) {
        return std::nullopt;
    }
    return std::string(trimmed);
}
#endif

struct Topology {
    std::uint32_t physical_cores = 0;
    std::uint32_t packages = 0;
};

// GetLogicalProcessorInformationEx handles >64 logical processors and
// multiple processor groups, unlike GetSystemInfo.
std::optional<Topology> query_topology(CpuInfo& info) {
    DWORD length = 0;
    if (::GetLogicalProcessorInformationEx(RelationAll, nullptr, &length) != FALSE ||
        ::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        info.errors.push_back(win32_error("GetLogicalProcessorInformationEx", ::GetLastError()));
        return std::nullopt;
    }

    // std::vector<std::byte> storage comes from ::operator new, which is
    // aligned for any fundamental type, so the struct casts below are valid.
    std::vector<std::byte> buffer;
    bool ok = false;
    for (int attempt = 0; attempt < 3 && !ok; ++attempt) {
        buffer.resize(length);
        auto* data = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data());
        if (::GetLogicalProcessorInformationEx(RelationAll, data, &length) != FALSE) {
            ok = true;
        } else if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            break;
        }
    }
    if (!ok) {
        info.errors.push_back(win32_error("GetLogicalProcessorInformationEx", ::GetLastError()));
        return std::nullopt;
    }

    constexpr std::size_t kHeaderSize = sizeof(LOGICAL_PROCESSOR_RELATIONSHIP) + sizeof(DWORD);
    Topology topology;
    std::size_t offset = 0;
    const std::size_t total = std::min<std::size_t>(length, buffer.size());
    while (offset + kHeaderSize <= total) {
        const auto* entry = reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(
            buffer.data() + offset);
        const std::size_t size = entry->Size;
        if (size < kHeaderSize || size > total - offset) {
            break;  // malformed record: stop rather than read out of bounds
        }
        if (entry->Relationship == RelationProcessorCore) {
            ++topology.physical_cores;
        } else if (entry->Relationship == RelationProcessorPackage) {
            ++topology.packages;
        }
        offset += size;
    }
    if (topology.physical_cores == 0) {
        info.errors.push_back(
            {"parse processor topology", std::nullopt, "no processor core records were returned"});
        return std::nullopt;
    }
    return topology;
}

std::uint64_t to_uint64(const FILETIME& ft) noexcept {
    return (static_cast<std::uint64_t>(ft.dwHighDateTime) << 32U) | ft.dwLowDateTime;
}

std::optional<CpuTimes> read_system_times(CpuInfo& info) {
    FILETIME idle{};
    FILETIME kernel{};
    FILETIME user{};
    if (::GetSystemTimes(&idle, &kernel, &user) == FALSE) {
        info.errors.push_back(win32_error("GetSystemTimes", ::GetLastError()));
        return std::nullopt;
    }
    return CpuTimes{to_uint64(idle), to_uint64(kernel), to_uint64(user)};
}

void measure_usage(CpuInfo& info, std::chrono::milliseconds interval) {
    // Defensive: the CLI already validates, but collectors may be called directly.
    interval = std::clamp(interval, cli::kMinSampleInterval, cli::kMaxSampleInterval);

    const auto first = read_system_times(info);
    if (!first) {
        return;
    }
    std::this_thread::sleep_for(interval);
    const auto second = read_system_times(info);
    if (!second) {
        return;
    }
    info.sample_interval_ms = static_cast<std::uint32_t>(interval.count());
    info.usage_percent = compute_cpu_usage(*first, *second);
    if (!info.usage_percent) {
        info.errors.push_back({"compute CPU usage", std::nullopt,
                               "processor time counters did not advance between samples"});
    }
}

}  // namespace

CpuInfo collect_cpu(std::chrono::milliseconds sample_interval) {
    CpuInfo info;

    const auto name =
        read_registry_string(HKEY_LOCAL_MACHINE, kCpuRegistryKey, L"ProcessorNameString");
    if (name.value) {
        const std::string utf8 = to_utf8(*name.value);
        if (const auto trimmed = text::trim(utf8); !trimmed.empty()) {
            info.name = std::string(trimmed);
        }
    }
#if SYSDIAG_HAS_CPUID
    if (!info.name) {
        info.name = cpuid_brand_string();
    }
#endif
    if (!info.name) {
        info.errors.push_back(
            win32_error("read processor name (registry)",
                        name.error != ERROR_SUCCESS ? name.error : ERROR_INVALID_DATA));
    }

    if (const auto vendor =
            read_registry_string(HKEY_LOCAL_MACHINE, kCpuRegistryKey, L"VendorIdentifier");
        vendor.value) {
        info.vendor = std::string(text::trim(to_utf8(*vendor.value)));
    }

    if (const auto mhz = read_registry_dword(HKEY_LOCAL_MACHINE, kCpuRegistryKey, L"~MHz");
        mhz.value && *mhz.value > 0) {
        info.base_frequency_mhz = static_cast<std::uint32_t>(*mhz.value);
    }

    if (const auto topology = query_topology(info)) {
        info.physical_cores = topology->physical_cores;
        if (topology->packages > 0) {
            info.packages = topology->packages;
        }
    }

    const DWORD logical = ::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    if (logical == 0) {
        info.errors.push_back(win32_error("GetActiveProcessorCount", ::GetLastError()));
    } else {
        info.logical_processors = static_cast<std::uint32_t>(logical);
    }

    measure_usage(info, sample_interval);
    return info;
}

}  // namespace sysdiag::win
