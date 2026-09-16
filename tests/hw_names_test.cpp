#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>

#include "core/hw_names.hpp"

namespace names = sysdiag::names;

TEST(HwNames, HexId) {
    EXPECT_EQ(names::hex_id(0x10DE), "0x10DE");
    EXPECT_EQ(names::hex_id(0), "0x0000");
    EXPECT_EQ(names::hex_id(0x887A0002), "0x887A0002");
}

TEST(HwNames, MachineArchitecture) {
    EXPECT_EQ(names::machine_architecture(0x8664), "x64");
    EXPECT_EQ(names::machine_architecture(0xAA64), "ARM64");
    EXPECT_EQ(names::machine_architecture(0x014C), "x86");
    EXPECT_EQ(names::machine_architecture(0x1234), "Unknown (0x1234)");
}

TEST(HwNames, GpuVendor) {
    EXPECT_EQ(names::gpu_vendor(0x10DE), "NVIDIA");
    EXPECT_EQ(names::gpu_vendor(0x1002), "AMD");
    EXPECT_EQ(names::gpu_vendor(0x8086), "Intel");
    EXPECT_EQ(names::gpu_vendor(0xABCD), "Unknown (0xABCD)");
}

TEST(HwNames, InterfaceAndDriveTypes) {
    EXPECT_EQ(names::interface_type(6), "ethernet");
    EXPECT_EQ(names::interface_type(71), "wifi");
    EXPECT_EQ(names::interface_type(9999), "other");
    EXPECT_EQ(names::drive_type(3), "fixed");
    EXPECT_EQ(names::drive_type(2), "removable");
    EXPECT_EQ(names::drive_type(4), "network");
    EXPECT_EQ(names::drive_type(0), "unknown");
    EXPECT_EQ(names::drive_type(1), "unknown");
}

TEST(HwNames, Windows11ProductNameCorrection) {
    EXPECT_EQ(names::normalize_windows_product_name("Windows 10 Pro", 22631), "Windows 11 Pro");
    EXPECT_EQ(names::normalize_windows_product_name("Windows 10 Pro", 22000), "Windows 11 Pro");
    EXPECT_EQ(names::normalize_windows_product_name("Windows 10 Pro", 19045), "Windows 10 Pro");
    EXPECT_EQ(names::normalize_windows_product_name("Windows 10 Pro", std::nullopt),
              "Windows 10 Pro");
    EXPECT_EQ(names::normalize_windows_product_name("Windows Server 2022", 20348),
              "Windows Server 2022");
    EXPECT_EQ(names::normalize_windows_product_name("Windows 11 Home", 26100), "Windows 11 Home");
}

TEST(HwNames, HardwareAddressOnlyForRealLinks) {
    EXPECT_TRUE(names::has_hardware_address(6));     // ethernet
    EXPECT_TRUE(names::has_hardware_address(71));    // wifi
    EXPECT_FALSE(names::has_hardware_address(131));  // tunnel (e.g. Teredo)
    EXPECT_FALSE(names::has_hardware_address(23));   // ppp
    EXPECT_FALSE(names::has_hardware_address(24));   // loopback
}

TEST(HwNames, FormatMac) {
    const std::array<std::uint8_t, 6> mac = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0xFF};
    EXPECT_EQ(names::format_mac(mac).value_or(""), "00-1A-2B-3C-4D-FF");
    const std::array<std::uint8_t, 6> zero{};
    EXPECT_FALSE(names::format_mac(zero).has_value());
    EXPECT_FALSE(names::format_mac({}).has_value());
}

TEST(HwNames, FormatIpv4) {
    EXPECT_EQ(names::format_ipv4({192, 168, 1, 10}, 24), "192.168.1.10/24");
    EXPECT_EQ(names::format_ipv4({255, 255, 255, 255}, 32), "255.255.255.255/32");
    EXPECT_EQ(names::format_ipv4({10, 0, 0, 1}, 0), "10.0.0.1/0");
    EXPECT_EQ(names::format_ipv4({10, 0, 0, 1}, 255), "10.0.0.1");
}

TEST(HwNames, LinkSpeedSentinels) {
    EXPECT_FALSE(names::link_speed(0).has_value());
    EXPECT_FALSE(names::link_speed(std::numeric_limits<std::uint64_t>::max()).has_value());
    EXPECT_EQ(names::link_speed(1'000'000'000).value_or(0), 1'000'000'000U);
}
