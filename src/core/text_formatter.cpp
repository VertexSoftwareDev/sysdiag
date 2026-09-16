#include "core/formatters.hpp"

#include <chrono>
#include <string_view>

#include "core/hw_names.hpp"
#include "core/text.hpp"
#include "core/units.hpp"
#include "sysdiag/version.hpp"

namespace sysdiag {
namespace {

constexpr std::string_view kUnavailable = "n/a";
constexpr std::size_t kLabelWidth = 22;
constexpr std::size_t kMaxValueBytes = 256;

// Values originate from the OS/drivers; never print them raw to a terminal.
std::string clean(std::string_view value) {
    return text::sanitize_for_display(value, kMaxValueBytes);
}

class TextBuilder {
  public:
    void heading(std::string_view title) {
        if (!out_.empty()) {
            out_.push_back('\n');
        }
        out_.append(title);
        out_.push_back('\n');
    }

    void row(std::string_view label, std::string_view value, std::size_t indent = 2) {
        out_.append(indent, ' ');
        out_.append(label);
        const std::size_t width = kLabelWidth > indent - 2 ? kLabelWidth - (indent - 2) : 0;
        if (label.size() < width) {
            out_.append(width - label.size(), ' ');
        } else {
            out_.push_back(' ');
        }
        out_.append(clean(value));
        out_.push_back('\n');
    }

    void row(std::string_view label, const std::string& value, std::size_t indent = 2) {
        row(label, std::string_view(value), indent);
    }

    void row(std::string_view label, const char* value, std::size_t indent = 2) {
        row(label, std::string_view(value), indent);
    }

    void row(std::string_view label, const std::optional<std::string>& value,
             std::size_t indent = 2) {
        row(label, value ? std::string_view(*value) : kUnavailable, indent);
    }

    void line(std::string_view value, std::size_t indent = 2) {
        out_.append(indent, ' ');
        out_.append(value);
        out_.push_back('\n');
    }

    void errors(const std::vector<CollectionError>& errors) {
        for (const auto& e : errors) {
            std::string msg = "! " + clean(e.operation) + " failed: " + clean(e.message);
            if (e.code) {
                // Win32 codes read best in decimal, HRESULTs in hex.
                msg += *e.code > 0xFFFFU ? " (" + names::hex_id(*e.code) + ")"
                                         : " (error " + std::to_string(*e.code) + ")";
            }
            line(msg);
        }
    }

    [[nodiscard]] std::string take() { return std::move(out_); }

  private:
    std::string out_;
};

std::optional<std::string> opt_number(const std::optional<std::uint32_t>& v) {
    return v ? std::optional<std::string>(std::to_string(*v)) : std::nullopt;
}

std::optional<std::string> opt_bytes(const std::optional<std::uint64_t>& v) {
    return v ? std::optional<std::string>(units::format_bytes(*v)) : std::nullopt;
}

std::string bytes_with_percent(std::uint64_t bytes, const std::optional<double>& pct) {
    std::string s = units::format_bytes(bytes);
    if (pct) {
        s += " (" + units::format_percent(*pct) + ")";
    }
    return s;
}

void render_os(TextBuilder& t, const OsInfo& os) {
    t.heading("Operating System");
    t.row("Product", os.product_name);

    std::optional<std::string> version;
    if (os.build_number) {
        std::string build = std::to_string(*os.build_number);
        if (os.update_revision) {
            build += "." + std::to_string(*os.update_revision);
        }
        version =
            os.display_version ? *os.display_version + " (build " + build + ")" : "build " + build;
    } else if (os.display_version) {
        version = os.display_version;
    }
    t.row("Version", version);
    t.row("Edition", os.edition);
    if (os.major_version && os.minor_version) {
        t.row("Kernel version",
              std::to_string(*os.major_version) + "." + std::to_string(*os.minor_version));
    } else {
        t.row("Kernel version", std::optional<std::string>{});
    }

    std::optional<std::string> arch = os.os_architecture;
    if (arch && os.process_architecture && *os.process_architecture != *arch) {
        *arch += " (this process: " + *os.process_architecture + ")";
    }
    t.row("Architecture", arch);
    t.row("Computer name", os.computer_name);
    t.row("Uptime", os.uptime_seconds
                        ? std::optional<std::string>(units::format_duration(std::chrono::seconds(
                              static_cast<std::chrono::seconds::rep>(*os.uptime_seconds))))
                        : std::nullopt);
    t.errors(os.errors);
}

void render_cpu(TextBuilder& t, const CpuInfo& cpu) {
    t.heading("CPU");
    t.row("Name", cpu.name);
    t.row("Vendor", cpu.vendor);
    if (cpu.packages && *cpu.packages > 1) {
        t.row("Sockets", opt_number(cpu.packages));
    }
    t.row("Physical cores", opt_number(cpu.physical_cores));
    t.row("Logical processors", opt_number(cpu.logical_processors));
    t.row("Base frequency",
          cpu.base_frequency_mhz
              ? std::optional<std::string>(std::to_string(*cpu.base_frequency_mhz) + " MHz")
              : std::nullopt);
    std::optional<std::string> usage;
    if (cpu.usage_percent) {
        usage = units::format_percent(*cpu.usage_percent);
        if (cpu.sample_interval_ms) {
            *usage += " (sampled over " + std::to_string(*cpu.sample_interval_ms) + " ms)";
        }
    }
    t.row("Usage", usage);
    t.errors(cpu.errors);
}

void render_memory(TextBuilder& t, const MemoryInfo& mem) {
    t.heading("Memory");
    t.row("Total", opt_bytes(mem.total_bytes));
    t.row("Used", mem.used_bytes ? std::optional<std::string>(
                                       bytes_with_percent(*mem.used_bytes, mem.usage_percent))
                                 : std::nullopt);
    t.row("Available", opt_bytes(mem.available_bytes));
    t.errors(mem.errors);
}

void render_disk(TextBuilder& t, const DiskSection& disk) {
    t.heading("Disks");
    if (disk.volumes.empty() && disk.errors.empty()) {
        t.line("(no volumes found)");
    }
    for (const auto& v : disk.volumes) {
        std::string header = v.root + " [" + v.drive_type;
        if (v.file_system) {
            header += ", " + *v.file_system;
        }
        header += "]";
        if (v.label && !v.label->empty()) {
            header += " \"" + *v.label + "\"";
        }
        if (!v.ready) {
            header += " - not ready (no media or unavailable)";
        }
        t.line(clean(header));
        if (!v.ready) {
            continue;
        }
        t.row("Capacity", opt_bytes(v.total_bytes), 4);
        t.row("Used",
              v.used_bytes
                  ? std::optional<std::string>(bytes_with_percent(*v.used_bytes, v.usage_percent))
                  : std::nullopt,
              4);
        t.row("Free", opt_bytes(v.free_bytes), 4);
    }
    t.errors(disk.errors);
}

void render_gpu(TextBuilder& t, const GpuSection& gpu) {
    t.heading("GPU");
    if (gpu.adapters.empty() && gpu.errors.empty()) {
        t.line("(no hardware graphics adapters found)");
    }
    for (std::size_t i = 0; i < gpu.adapters.size(); ++i) {
        const auto& a = gpu.adapters[i];
        t.line(clean("[" + std::to_string(i) + "] " + a.name));
        t.row("Vendor", a.vendor, 4);
        t.row("PCI id", names::hex_id(a.vendor_id) + ":" + names::hex_id(a.device_id), 4);
        t.row("Dedicated VRAM", units::format_bytes(a.dedicated_video_memory_bytes), 4);
        if (a.dedicated_system_memory_bytes != 0) {
            t.row("Dedicated sys memory", units::format_bytes(a.dedicated_system_memory_bytes), 4);
        }
        t.row("Shared sys memory", units::format_bytes(a.shared_system_memory_bytes), 4);
    }
    t.errors(gpu.errors);
}

void render_network(TextBuilder& t, const NetworkSection& net) {
    t.heading("Network");
    if (net.adapters.empty() && net.errors.empty()) {
        t.line("(no active network adapters found)");
    }
    for (const auto& a : net.adapters) {
        t.line(clean(a.description.empty() ? a.name : a.name + " (" + a.description + ")"));
        t.row("Type", a.type, 4);
        t.row("MAC address", a.mac_address, 4);
        if (a.ipv4_addresses.empty()) {
            t.row("IPv4", "none", 4);
        }
        for (const auto& ip : a.ipv4_addresses) {
            t.row("IPv4", ip, 4);
        }
        for (const auto& ip : a.ipv6_addresses) {
            t.row("IPv6", ip, 4);
        }
        for (const auto& gw : a.gateways) {
            t.row("Gateway", gw, 4);
        }
        std::optional<std::string> speed;
        if (a.transmit_bps && a.receive_bps && *a.transmit_bps != *a.receive_bps) {
            speed = units::format_bits_per_second(*a.transmit_bps) + " up / " +
                    units::format_bits_per_second(*a.receive_bps) + " down";
        } else if (a.transmit_bps || a.receive_bps) {
            speed =
                units::format_bits_per_second(a.transmit_bps ? *a.transmit_bps : *a.receive_bps);
        }
        t.row("Link speed", speed, 4);
    }
    t.errors(net.errors);
}

}  // namespace

std::string format_text(const SystemReport& report) {
    TextBuilder t;
    t.heading(std::string(kToolName) + " " + report.tool_version + " - report generated " +
              report.generated_at_utc);
    if (report.os) {
        render_os(t, *report.os);
    }
    if (report.cpu) {
        render_cpu(t, *report.cpu);
    }
    if (report.memory) {
        render_memory(t, *report.memory);
    }
    if (report.disk) {
        render_disk(t, *report.disk);
    }
    if (report.gpu) {
        render_gpu(t, *report.gpu);
    }
    if (report.network) {
        render_network(t, *report.network);
    }
    return t.take();
}

}  // namespace sysdiag
