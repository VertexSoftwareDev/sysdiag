// Hand-built reports used as fakes for formatter and builder tests.
#pragma once

#include "core/model.hpp"
#include "core/units.hpp"

namespace sysdiag::test {

inline OsInfo sample_os() {
    OsInfo os;
    os.product_name = "Windows 11 Pro";
    os.display_version = "23H2";
    os.edition = "Professional";
    os.major_version = 10;
    os.minor_version = 0;
    os.build_number = 22631;
    os.update_revision = 3880;
    os.os_architecture = "x64";
    os.process_architecture = "x64";
    os.computer_name = "DEV-PC";
    os.uptime_seconds = 93784;
    return os;
}

inline CpuInfo sample_cpu() {
    CpuInfo cpu;
    cpu.name = "AMD Ryzen 7 5800X 8-Core Processor";
    cpu.vendor = "AuthenticAMD";
    cpu.packages = 1;
    cpu.physical_cores = 8;
    cpu.logical_processors = 16;
    cpu.base_frequency_mhz = 3800;
    cpu.usage_percent = 12.34;
    cpu.sample_interval_ms = 500;
    return cpu;
}

inline MemoryInfo sample_memory() {
    MemoryInfo mem;
    mem.total_bytes = 32ULL * units::kGiB;
    mem.available_bytes = 20ULL * units::kGiB;
    mem.used_bytes = 12ULL * units::kGiB;
    mem.usage_percent = 37.5;
    return mem;
}

inline DiskSection sample_disk() {
    DiskSection disk;
    DiskVolume c;
    c.root = "C:\\";
    c.drive_type = "fixed";
    c.ready = true;
    c.label = "Windows";
    c.file_system = "NTFS";
    c.total_bytes = 500ULL * units::kGiB;
    c.free_bytes = 125ULL * units::kGiB;
    c.used_bytes = 375ULL * units::kGiB;
    c.usage_percent = 75.0;
    disk.volumes.push_back(c);

    DiskVolume d;
    d.root = "D:\\";
    d.drive_type = "optical";
    d.ready = false;
    disk.volumes.push_back(d);
    return disk;
}

inline GpuSection sample_gpu() {
    GpuSection gpu;
    GpuAdapter a;
    a.name = "NVIDIA GeForce RTX 3070";
    a.vendor_id = 0x10DE;
    a.device_id = 0x2484;
    a.vendor = "NVIDIA";
    a.dedicated_video_memory_bytes = 8ULL * units::kGiB;
    a.shared_system_memory_bytes = 16ULL * units::kGiB;
    gpu.adapters.push_back(a);
    return gpu;
}

inline NetworkSection sample_network() {
    NetworkSection net;
    NetworkAdapter a;
    a.name = "Ethernet";
    a.description = "Intel(R) Ethernet Connection";
    a.type = "ethernet";
    a.mac_address = "00-1A-2B-3C-4D-5E";
    a.transmit_bps = 1'000'000'000;
    a.receive_bps = 1'000'000'000;
    a.ipv4_addresses = {"192.168.1.10/24"};
    a.ipv6_addresses = {"fe80::1/64"};
    a.gateways = {"192.168.1.1"};
    net.adapters.push_back(a);
    return net;
}

inline SystemReport sample_report() {
    SystemReport r;
    r.tool_version = "1.0.0";
    r.generated_at_utc = "2026-01-02T03:04:05Z";
    r.os = sample_os();
    r.cpu = sample_cpu();
    r.memory = sample_memory();
    r.disk = sample_disk();
    r.gpu = sample_gpu();
    r.network = sample_network();
    return r;
}

}  // namespace sysdiag::test
