// Plain data model shared by collectors and formatters.
//
// Every value that the operating system may fail to report is std::optional:
// "unknown" is represented explicitly instead of with magic numbers. Each
// section carries the list of problems encountered while collecting it, so a
// partial failure never aborts the whole report.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace sysdiag {

struct CollectionError {
    std::string operation;              // what we were trying to do, e.g. "GlobalMemoryStatusEx"
    std::optional<std::uint32_t> code;  // OS error code, when one exists
    std::string message;                // human-readable description
};

struct CpuInfo {
    std::optional<std::string> name;
    std::optional<std::string> vendor;
    std::optional<std::uint32_t> packages;  // physical sockets
    std::optional<std::uint32_t> physical_cores;
    std::optional<std::uint32_t> logical_processors;
    std::optional<std::uint32_t> base_frequency_mhz;  // as reported by firmware/registry
    std::optional<double> usage_percent;              // 0..100, averaged over the sample interval
    std::optional<std::uint32_t> sample_interval_ms;
    std::vector<CollectionError> errors;
};

struct MemoryInfo {
    std::optional<std::uint64_t> total_bytes;
    std::optional<std::uint64_t> available_bytes;
    std::optional<std::uint64_t> used_bytes;
    std::optional<double> usage_percent;
    std::vector<CollectionError> errors;
};

struct DiskVolume {
    std::string root;        // e.g. "C:\"
    std::string drive_type;  // "fixed", "removable", "network", ...
    bool ready = false;      // false e.g. for an empty card reader
    std::optional<std::string> label;
    std::optional<std::string> file_system;
    std::optional<std::uint64_t> total_bytes;
    std::optional<std::uint64_t> free_bytes;
    std::optional<std::uint64_t> used_bytes;
    std::optional<double> usage_percent;
};

struct DiskSection {
    std::vector<DiskVolume> volumes;
    std::vector<CollectionError> errors;
};

struct GpuAdapter {
    std::string name;
    std::uint32_t vendor_id = 0;
    std::uint32_t device_id = 0;
    std::string vendor;  // derived from vendor_id
    std::uint64_t dedicated_video_memory_bytes = 0;
    std::uint64_t dedicated_system_memory_bytes = 0;
    std::uint64_t shared_system_memory_bytes = 0;
};

struct GpuSection {
    std::vector<GpuAdapter> adapters;
    std::vector<CollectionError> errors;
};

struct OsInfo {
    std::optional<std::string> product_name;
    std::optional<std::string> display_version;  // e.g. "23H2"
    std::optional<std::string> edition;          // e.g. "Professional"
    std::optional<std::uint32_t> major_version;
    std::optional<std::uint32_t> minor_version;
    std::optional<std::uint32_t> build_number;
    std::optional<std::uint32_t> update_revision;     // UBR
    std::optional<std::string> os_architecture;       // native machine
    std::optional<std::string> process_architecture;  // this executable
    std::optional<std::string> computer_name;
    std::optional<std::uint64_t> uptime_seconds;
    std::vector<CollectionError> errors;
};

struct NetworkAdapter {
    std::string name;         // friendly name, e.g. "Wi-Fi"
    std::string description;  // driver description
    std::string type;         // "ethernet", "wifi", ...
    std::optional<std::string> mac_address;
    std::optional<std::uint64_t> transmit_bps;
    std::optional<std::uint64_t> receive_bps;
    std::vector<std::string> ipv4_addresses;  // CIDR notation
    std::vector<std::string> ipv6_addresses;  // CIDR notation
    std::vector<std::string> gateways;
};

struct NetworkSection {
    std::vector<NetworkAdapter> adapters;
    std::vector<CollectionError> errors;
};

struct SystemReport {
    std::string tool_version;
    std::string generated_at_utc;  // ISO 8601
    std::optional<OsInfo> os;
    std::optional<CpuInfo> cpu;
    std::optional<MemoryInfo> memory;
    std::optional<DiskSection> disk;
    std::optional<GpuSection> gpu;
    std::optional<NetworkSection> network;
};

}  // namespace sysdiag
