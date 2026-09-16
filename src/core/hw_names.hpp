// Pure mapping/formatting helpers for values returned by Windows APIs.
// They take plain integers/bytes so they can be tested on any platform.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace sysdiag::names {

// "0x10DE" style hexadecimal identifier (at least 4 digits).
[[nodiscard]] std::string hex_id(std::uint32_t value);

// IMAGE_FILE_MACHINE_* value -> "x64", "ARM64", ...
[[nodiscard]] std::string machine_architecture(std::uint16_t machine);

// PCI vendor id -> "NVIDIA", "AMD", "Intel", ... or "Unknown (0x1234)".
[[nodiscard]] std::string gpu_vendor(std::uint32_t vendor_id);

// IANA ifType (IF_TYPE_*) -> "ethernet", "wifi", ...
[[nodiscard]] std::string interface_type(std::uint32_t if_type);

// False for interface types whose "physical address" is synthetic (tunnels,
// PPP, loopback), where showing a MAC address would be misleading.
[[nodiscard]] bool has_hardware_address(std::uint32_t if_type) noexcept;

// GetDriveType result -> "fixed", "removable", ...
[[nodiscard]] std::string drive_type(std::uint32_t drive_type);

// Windows 11 still reports "Windows 10 ..." as ProductName in the registry;
// the build number (>= 22000) is the reliable discriminator.
[[nodiscard]] std::string normalize_windows_product_name(std::string_view product_name,
                                                         std::optional<std::uint32_t> build);

// "AA-BB-CC-DD-EE-FF" (Windows style). nullopt for empty or all-zero input.
[[nodiscard]] std::optional<std::string> format_mac(std::span<const std::uint8_t> bytes);

// "192.168.1.10/24" (prefix omitted when > 32).
[[nodiscard]] std::string format_ipv4(const std::array<std::uint8_t, 4>& octets,
                                      std::uint32_t prefix_length);

// Link speed as reported by GetAdaptersAddresses; 0 and UINT64_MAX mean unknown.
[[nodiscard]] std::optional<std::uint64_t> link_speed(std::uint64_t raw) noexcept;

}  // namespace sysdiag::names
