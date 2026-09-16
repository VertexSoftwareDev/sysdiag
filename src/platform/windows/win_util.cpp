#include "platform/windows/win_util.hpp"

#include <algorithm>
#include <climits>
#include <cwchar>
#include <memory>
#include <stdexcept>

// A 64-bit process always sees the 64-bit registry view. A 32-bit build must
// ask for it explicitly, otherwise WOW64 redirection may return other values.
#if defined(_WIN64)
constexpr DWORD kRegistryViewFlag = 0;
#else
#ifndef RRF_SUBKEY_WOW6464KEY
#define RRF_SUBKEY_WOW6464KEY 0x00010000
#endif
constexpr DWORD kRegistryViewFlag = RRF_SUBKEY_WOW6464KEY;
#endif

namespace sysdiag::win {
namespace {

// Registry strings larger than this are not plausible for the values we read.
constexpr DWORD kMaxRegistryBytes = 64 * 1024;

struct LocalFreeDeleter {
    void operator()(wchar_t* p) const noexcept { ::LocalFree(p); }
};

int checked_int(std::size_t size) {
    if (size > static_cast<std::size_t>(INT_MAX)) {
        throw std::length_error("string too large for Win32 conversion");
    }
    return static_cast<int>(size);
}

}  // namespace

std::string to_utf8(std::wstring_view text) {
    if (text.empty()) {
        return {};
    }
    const int length = checked_int(text.size());
    const int needed =
        ::WideCharToMultiByte(CP_UTF8, 0, text.data(), length, nullptr, 0, nullptr, nullptr);
    if (needed <= 0) {
        return "?";
    }
    std::string out(static_cast<std::size_t>(needed), '\0');
    const int written = ::WideCharToMultiByte(CP_UTF8, 0, text.data(), length, out.data(), needed,
                                              nullptr, nullptr);
    if (written <= 0) {
        return "?";
    }
    out.resize(static_cast<std::size_t>(written));
    return out;
}

std::wstring to_wide(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    const int length = checked_int(text.size());
    const int needed = ::MultiByteToWideChar(CP_UTF8, 0, text.data(), length, nullptr, 0);
    if (needed <= 0) {
        return L"?";
    }
    std::wstring out(static_cast<std::size_t>(needed), L'\0');
    const int written = ::MultiByteToWideChar(CP_UTF8, 0, text.data(), length, out.data(), needed);
    if (written <= 0) {
        return L"?";
    }
    out.resize(static_cast<std::size_t>(written));
    return out;
}

std::string to_utf8_bounded(const wchar_t* buffer, std::size_t capacity) {
    if (buffer == nullptr || capacity == 0) {
        return {};
    }
    return to_utf8(std::wstring_view(buffer, ::wcsnlen(buffer, capacity)));
}

std::string error_message(DWORD code) {
    wchar_t* raw = nullptr;
    const DWORD length = ::FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, 0,
        // With ALLOCATE_BUFFER the API expects the address of the pointer.
        reinterpret_cast<LPWSTR>(&raw), 0, nullptr);
    const std::unique_ptr<wchar_t, LocalFreeDeleter> owner(raw);
    if (length == 0 || raw == nullptr) {
        return "unknown error";
    }
    std::string message = to_utf8(std::wstring_view(raw, length));
    while (!message.empty() && (message.back() == '\n' || message.back() == '\r' ||
                                message.back() == ' ' || message.back() == '.')) {
        message.pop_back();
    }
    return message.empty() ? "unknown error" : message;
}

CollectionError win32_error(std::string operation, DWORD code) {
    return CollectionError{std::move(operation), static_cast<std::uint32_t>(code),
                           error_message(code)};
}

CollectionError hresult_error(std::string operation, HRESULT hr) {
    // Signed -> unsigned conversion is well defined (modulo 2^32).
    const auto code = static_cast<std::uint32_t>(hr);
    return CollectionError{std::move(operation), code, error_message(static_cast<DWORD>(code))};
}

WinResult<std::wstring> read_registry_string(HKEY root, const wchar_t* subkey,
                                             const wchar_t* value_name) {
    const DWORD kFlags = RRF_RT_REG_SZ | kRegistryViewFlag;
    DWORD size = 0;
    LSTATUS status = ::RegGetValueW(root, subkey, value_name, kFlags, nullptr, nullptr, &size);

    // The value can change between the size query and the read; retry a few times.
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (status != ERROR_SUCCESS) {
            return {std::nullopt, static_cast<DWORD>(status)};
        }
        if (size > kMaxRegistryBytes) {
            return {std::nullopt, ERROR_INVALID_DATA};
        }
        // +1 guarantees room for the terminator RegGetValueW appends.
        std::wstring buffer(size / sizeof(wchar_t) + 1, L'\0');
        DWORD capacity = static_cast<DWORD>(buffer.size() * sizeof(wchar_t));
        status =
            ::RegGetValueW(root, subkey, value_name, kFlags, nullptr, buffer.data(), &capacity);
        if (status == ERROR_MORE_DATA) {
            size = capacity;
            status = ERROR_SUCCESS;
            continue;
        }
        if (status != ERROR_SUCCESS) {
            return {std::nullopt, static_cast<DWORD>(status)};
        }
        buffer.resize(std::min<std::size_t>(buffer.size(), capacity / sizeof(wchar_t)));
        while (!buffer.empty() && buffer.back() == L'\0') {
            buffer.pop_back();
        }
        return {std::move(buffer), ERROR_SUCCESS};
    }
    return {std::nullopt, ERROR_MORE_DATA};
}

WinResult<DWORD> read_registry_dword(HKEY root, const wchar_t* subkey, const wchar_t* value_name) {
    DWORD data = 0;
    DWORD size = sizeof(data);
    const LSTATUS status = ::RegGetValueW(
        root, subkey, value_name, RRF_RT_REG_DWORD | kRegistryViewFlag, nullptr, &data, &size);
    if (status != ERROR_SUCCESS) {
        return {std::nullopt, static_cast<DWORD>(status)};
    }
    return {data, ERROR_SUCCESS};
}

ThreadErrorModeGuard::ThreadErrorModeGuard() noexcept {
    active_ =
        ::SetThreadErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX, &previous_) != 0;
}

ThreadErrorModeGuard::~ThreadErrorModeGuard() {
    if (active_) {
        ::SetThreadErrorMode(previous_, nullptr);
    }
}

}  // namespace sysdiag::win
