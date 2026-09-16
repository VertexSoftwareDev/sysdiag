#include "core/hw_names.hpp"

#include <algorithm>
#include <limits>

namespace sysdiag::names {
namespace {

std::string hex(std::uint32_t value, int min_digits) {
    static constexpr std::string_view kDigits = "0123456789ABCDEF";
    std::string out;
    do {
        out.insert(out.begin(), kDigits[value & 0xFU]);
        value >>= 4U;
    } while (value != 0);
    while (static_cast<int>(out.size()) < min_digits) {
        out.insert(out.begin(), '0');
    }
    return out;
}

}  // namespace

std::string hex_id(std::uint32_t value) {
    return "0x" + hex(value, 4);
}

std::string machine_architecture(std::uint16_t machine) {
    switch (machine) {
        case 0x8664: return "x64";
        case 0xAA64: return "ARM64";
        case 0x014C: return "x86";
        case 0x01C4: return "ARM";
        case 0x0200: return "IA64";
        default: return "Unknown (0x" + hex(machine, 4) + ")";
    }
}

std::string gpu_vendor(std::uint32_t vendor_id) {
    switch (vendor_id) {
        case 0x10DE: return "NVIDIA";
        case 0x1002:
        case 0x1022: return "AMD";
        case 0x8086:
        case 0x8087: return "Intel";
        case 0x1414: return "Microsoft";
        case 0x5143: return "Qualcomm";
        case 0x15AD: return "VMware";
        case 0x80EE: return "Oracle (VirtualBox)";
        case 0x1AF4: return "Red Hat (VirtIO)";
        default: return "Unknown (0x" + hex(vendor_id, 4) + ")";
    }
}

std::string interface_type(std::uint32_t if_type) {
    switch (if_type) {
        case 6: return "ethernet";
        case 71: return "wifi";
        case 24: return "loopback";
        case 23: return "ppp";
        case 131: return "tunnel";
        case 144: return "firewire";
        case 243:
        case 244: return "cellular";
        default: return "other";
    }
}

bool has_hardware_address(std::uint32_t if_type) noexcept {
    switch (if_type) {
        case 23:   // PPP
        case 24:   // software loopback
        case 131:  // tunnel (Teredo, 6to4, IP-HTTPS, ...)
            return false;
        default: return true;
    }
}

std::string drive_type(std::uint32_t type) {
    switch (type) {
        case 2: return "removable";
        case 3: return "fixed";
        case 4: return "network";
        case 5: return "optical";
        case 6: return "ramdisk";
        default: return "unknown";
    }
}

std::string normalize_windows_product_name(std::string_view product_name,
                                           std::optional<std::uint32_t> build) {
    constexpr std::string_view kWin10 = "Windows 10";
    std::string name(product_name);
    if (build && *build >= 22000U && name.starts_with(kWin10)) {
        name.replace(0, kWin10.size(), "Windows 11");
    }
    return name;
}

std::optional<std::string> format_mac(std::span<const std::uint8_t> bytes) {
    if (bytes.empty() ||
        std::all_of(bytes.begin(), bytes.end(), [](std::uint8_t b) { return b == 0; })) {
        return std::nullopt;
    }
    std::string out;
    out.reserve(bytes.size() * 3);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i != 0) {
            out.push_back('-');
        }
        out.append(hex(bytes[i], 2));
    }
    return out;
}

std::string format_ipv4(const std::array<std::uint8_t, 4>& octets, std::uint32_t prefix_length) {
    std::string out = std::to_string(octets[0]) + "." + std::to_string(octets[1]) + "." +
                      std::to_string(octets[2]) + "." + std::to_string(octets[3]);
    if (prefix_length <= 32U) {
        out += "/" + std::to_string(prefix_length);
    }
    return out;
}

std::optional<std::uint64_t> link_speed(std::uint64_t raw) noexcept {
    if (raw == 0 || raw == std::numeric_limits<std::uint64_t>::max()) {
        return std::nullopt;
    }
    return raw;
}

}  // namespace sysdiag::names
