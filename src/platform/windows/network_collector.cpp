#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <span>
#include <vector>

#include "core/hw_names.hpp"
#include "platform/windows/collectors.hpp"
#include "platform/windows/win_util.hpp"

namespace sysdiag::win {
namespace {

constexpr ULONG kInitialBufferSize = 15 * 1024;  // Microsoft's recommended starting size
constexpr std::size_t kMaxListItems = 1024;      // defensive cap for linked lists

struct FormattedAddress {
    std::string text;
    bool is_ipv4 = false;
};

// SOCKADDR data is copied into properly aligned locals before use.
std::optional<FormattedAddress> format_address(const SOCKET_ADDRESS& address,
                                               std::optional<std::uint8_t> prefix) {
    if (address.lpSockaddr == nullptr || address.iSockaddrLength <= 0) {
        return std::nullopt;
    }
    const auto length = static_cast<std::size_t>(address.iSockaddrLength);
    const ADDRESS_FAMILY family = address.lpSockaddr->sa_family;

    if (family == AF_INET && length >= sizeof(sockaddr_in)) {
        sockaddr_in ipv4{};
        std::memcpy(&ipv4, address.lpSockaddr, sizeof(ipv4));
        std::array<std::uint8_t, 4> octets{};
        std::memcpy(octets.data(), &ipv4.sin_addr, octets.size());
        return FormattedAddress{names::format_ipv4(octets, prefix.value_or(0xFF)), true};
    }

    if (family == AF_INET6 && length >= sizeof(sockaddr_in6)) {
        sockaddr_in6 ipv6{};
        std::memcpy(&ipv6, address.lpSockaddr, sizeof(ipv6));
        std::array<wchar_t, INET6_ADDRSTRLEN> buffer{};
        if (::InetNtopW(AF_INET6, &ipv6.sin6_addr, buffer.data(), buffer.size()) == nullptr) {
            return std::nullopt;
        }
        std::string text = to_utf8_bounded(buffer.data(), buffer.size());
        if (prefix && *prefix <= 128) {
            text += "/" + std::to_string(*prefix);
        }
        return FormattedAddress{std::move(text), false};
    }
    return std::nullopt;
}

NetworkAdapter describe_adapter(const IP_ADAPTER_ADDRESSES& raw) {
    NetworkAdapter adapter;
    adapter.name = raw.FriendlyName != nullptr ? to_utf8(raw.FriendlyName) : std::string{};
    if (adapter.name.empty() && raw.AdapterName != nullptr) {
        adapter.name = raw.AdapterName;  // GUID string, ASCII
    }
    adapter.description = raw.Description != nullptr ? to_utf8(raw.Description) : std::string{};
    adapter.type = names::interface_type(static_cast<std::uint32_t>(raw.IfType));

    if (names::has_hardware_address(static_cast<std::uint32_t>(raw.IfType))) {
        const std::size_t mac_length =
            std::min<std::size_t>(raw.PhysicalAddressLength, std::size(raw.PhysicalAddress));
        adapter.mac_address =
            names::format_mac(std::span<const std::uint8_t>(raw.PhysicalAddress, mac_length));
    }

    adapter.transmit_bps = names::link_speed(raw.TransmitLinkSpeed);
    adapter.receive_bps = names::link_speed(raw.ReceiveLinkSpeed);

    std::size_t count = 0;
    for (auto* unicast = raw.FirstUnicastAddress; unicast != nullptr && count < kMaxListItems;
         unicast = unicast->Next, ++count) {
        if (auto formatted = format_address(unicast->Address, unicast->OnLinkPrefixLength)) {
            (formatted->is_ipv4 ? adapter.ipv4_addresses : adapter.ipv6_addresses)
                .push_back(std::move(formatted->text));
        }
    }

    count = 0;
    for (auto* gateway = raw.FirstGatewayAddress; gateway != nullptr && count < kMaxListItems;
         gateway = gateway->Next, ++count) {
        if (auto formatted = format_address(gateway->Address, std::nullopt)) {
            adapter.gateways.push_back(std::move(formatted->text));
        }
    }
    return adapter;
}

}  // namespace

NetworkSection collect_network() {
    NetworkSection section;
    constexpr ULONG kFlags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
                             GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_INCLUDE_GATEWAYS;

    // Aligned storage: see the note in cpu_collector.cpp.
    std::vector<std::byte> buffer;
    ULONG size = kInitialBufferSize;
    ULONG result = ERROR_BUFFER_OVERFLOW;
    for (int attempt = 0; attempt < 3 && result == ERROR_BUFFER_OVERFLOW; ++attempt) {
        buffer.resize(size);
        result =
            ::GetAdaptersAddresses(AF_UNSPEC, kFlags, nullptr,
                                   reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data()), &size);
    }
    if (result == ERROR_NO_DATA) {
        return section;  // no adapters at all: not an error
    }
    if (result != NO_ERROR) {
        section.errors.push_back(win32_error("GetAdaptersAddresses", result));
        return section;
    }

    std::size_t count = 0;
    for (auto* raw = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
         raw != nullptr && count < kMaxListItems; raw = raw->Next, ++count) {
        if (raw->OperStatus != IfOperStatusUp || raw->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
            continue;
        }
        section.adapters.push_back(describe_adapter(*raw));
    }
    return section;
}

}  // namespace sysdiag::win
