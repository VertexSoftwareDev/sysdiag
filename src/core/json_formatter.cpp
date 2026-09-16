#include "core/formatters.hpp"

#include "core/json_writer.hpp"
#include "sysdiag/version.hpp"

// Document layout (stable key order; sections that were not requested are
// omitted; values that could not be collected are null; every section has an
// "errors" array describing what went wrong).

namespace sysdiag {
namespace {

void write_errors(JsonWriter& w, const std::vector<CollectionError>& errors) {
    w.key("errors");
    w.begin_array();
    for (const auto& e : errors) {
        w.begin_object();
        w.field("operation", e.operation);
        w.key("code");
        e.code ? w.number(std::uint64_t{*e.code}) : w.null();
        w.field("message", e.message);
        w.end_object();
    }
    w.end_array();
}

void write_os(JsonWriter& w, const OsInfo& os) {
    w.key("os");
    w.begin_object();
    w.field("product_name", os.product_name);
    w.field("display_version", os.display_version);
    w.field("edition", os.edition);
    w.field("major_version", os.major_version);
    w.field("minor_version", os.minor_version);
    w.field("build_number", os.build_number);
    w.field("update_revision", os.update_revision);
    w.field("os_architecture", os.os_architecture);
    w.field("process_architecture", os.process_architecture);
    w.field("computer_name", os.computer_name);
    w.field("uptime_seconds", os.uptime_seconds);
    write_errors(w, os.errors);
    w.end_object();
}

void write_cpu(JsonWriter& w, const CpuInfo& cpu) {
    w.key("cpu");
    w.begin_object();
    w.field("name", cpu.name);
    w.field("vendor", cpu.vendor);
    w.field("packages", cpu.packages);
    w.field("physical_cores", cpu.physical_cores);
    w.field("logical_processors", cpu.logical_processors);
    w.field("base_frequency_mhz", cpu.base_frequency_mhz);
    w.field("usage_percent", cpu.usage_percent, 1);
    w.field("sample_interval_ms", cpu.sample_interval_ms);
    write_errors(w, cpu.errors);
    w.end_object();
}

void write_memory(JsonWriter& w, const MemoryInfo& mem) {
    w.key("memory");
    w.begin_object();
    w.field("total_bytes", mem.total_bytes);
    w.field("used_bytes", mem.used_bytes);
    w.field("available_bytes", mem.available_bytes);
    w.field("usage_percent", mem.usage_percent, 1);
    write_errors(w, mem.errors);
    w.end_object();
}

void write_disk(JsonWriter& w, const DiskSection& disk) {
    w.key("disk");
    w.begin_object();
    w.key("volumes");
    w.begin_array();
    for (const auto& v : disk.volumes) {
        w.begin_object();
        w.field("root", v.root);
        w.field("drive_type", v.drive_type);
        w.bool_field("ready", v.ready);
        w.field("label", v.label);
        w.field("file_system", v.file_system);
        w.field("total_bytes", v.total_bytes);
        w.field("used_bytes", v.used_bytes);
        w.field("free_bytes", v.free_bytes);
        w.field("usage_percent", v.usage_percent, 1);
        w.end_object();
    }
    w.end_array();
    write_errors(w, disk.errors);
    w.end_object();
}

void write_gpu(JsonWriter& w, const GpuSection& gpu) {
    w.key("gpu");
    w.begin_object();
    w.key("adapters");
    w.begin_array();
    for (const auto& a : gpu.adapters) {
        w.begin_object();
        w.field("name", a.name);
        w.field("vendor", a.vendor);
        w.field("vendor_id", std::uint64_t{a.vendor_id});
        w.field("device_id", std::uint64_t{a.device_id});
        w.field("dedicated_video_memory_bytes", a.dedicated_video_memory_bytes);
        w.field("dedicated_system_memory_bytes", a.dedicated_system_memory_bytes);
        w.field("shared_system_memory_bytes", a.shared_system_memory_bytes);
        w.end_object();
    }
    w.end_array();
    write_errors(w, gpu.errors);
    w.end_object();
}

void write_network(JsonWriter& w, const NetworkSection& net) {
    w.key("network");
    w.begin_object();
    w.key("adapters");
    w.begin_array();
    for (const auto& a : net.adapters) {
        w.begin_object();
        w.field("name", a.name);
        w.field("description", a.description);
        w.field("type", a.type);
        w.field("mac_address", a.mac_address);
        w.field("transmit_bps", a.transmit_bps);
        w.field("receive_bps", a.receive_bps);
        w.string_array("ipv4_addresses", a.ipv4_addresses);
        w.string_array("ipv6_addresses", a.ipv6_addresses);
        w.string_array("gateways", a.gateways);
        w.end_object();
    }
    w.end_array();
    write_errors(w, net.errors);
    w.end_object();
}

}  // namespace

std::string format_json(const SystemReport& report) {
    JsonWriter w(/*pretty=*/true);
    w.begin_object();
    w.field("schema_version", kJsonSchemaVersion);
    w.key("tool");
    w.begin_object();
    w.field("name", kToolName);
    w.field("version", report.tool_version);
    w.end_object();
    w.field("generated_at", report.generated_at_utc);
    if (report.os) {
        write_os(w, *report.os);
    }
    if (report.cpu) {
        write_cpu(w, *report.cpu);
    }
    if (report.memory) {
        write_memory(w, *report.memory);
    }
    if (report.disk) {
        write_disk(w, *report.disk);
    }
    if (report.gpu) {
        write_gpu(w, *report.gpu);
    }
    if (report.network) {
        write_network(w, *report.network);
    }
    w.end_object();
    return w.finish();
}

}  // namespace sysdiag
