// RAII wrappers and helpers around raw Win32 APIs.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "core/model.hpp"
#include "platform/windows/win_headers.hpp"

namespace sysdiag::win {

// UTF-16 <-> UTF-8. Unpaired surrogates become U+FFFD instead of failing.
[[nodiscard]] std::string to_utf8(std::wstring_view text);
[[nodiscard]] std::wstring to_wide(std::string_view text);

// For fixed-size WCHAR arrays inside Win32 structs: stops at the first NUL
// but never reads past `capacity`, even if the OS did not terminate it.
[[nodiscard]] std::string to_utf8_bounded(const wchar_t* buffer, std::size_t capacity);

// System message text for a Win32 error / HRESULT (trailing newline removed).
[[nodiscard]] std::string error_message(DWORD code);

[[nodiscard]] CollectionError win32_error(std::string operation, DWORD code);
[[nodiscard]] CollectionError hresult_error(std::string operation, HRESULT hr);

template <typename T>
struct WinResult {
    std::optional<T> value;
    DWORD error = ERROR_SUCCESS;
};

// Reads from the native (64-bit) registry view regardless of process bitness.
[[nodiscard]] WinResult<std::wstring> read_registry_string(HKEY root, const wchar_t* subkey,
                                                           const wchar_t* value_name);
[[nodiscard]] WinResult<DWORD> read_registry_dword(HKEY root, const wchar_t* subkey,
                                                   const wchar_t* value_name);

// Suppresses "There is no disk in the drive" style dialogs for this thread.
class ThreadErrorModeGuard {
  public:
    ThreadErrorModeGuard() noexcept;
    ~ThreadErrorModeGuard();
    ThreadErrorModeGuard(const ThreadErrorModeGuard&) = delete;
    ThreadErrorModeGuard& operator=(const ThreadErrorModeGuard&) = delete;

  private:
    DWORD previous_ = 0;
    bool active_ = false;
};

// Minimal owning COM pointer (avoids a dependency on WRL/ATL).
template <typename T>
class ComPtr {
  public:
    ComPtr() noexcept = default;
    ~ComPtr() { reset(); }
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;
    ComPtr(ComPtr&& other) noexcept : ptr_(std::exchange(other.ptr_, nullptr)) {}
    ComPtr& operator=(ComPtr&& other) noexcept {
        if (this != &other) {
            reset();
            ptr_ = std::exchange(other.ptr_, nullptr);
        }
        return *this;
    }

    [[nodiscard]] T* get() const noexcept { return ptr_; }
    T* operator->() const noexcept { return ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    // Releases the current object and returns the slot for an out-parameter.
    [[nodiscard]] T** put() noexcept {
        reset();
        return &ptr_;
    }

    void reset() noexcept {
        if (ptr_ != nullptr) {
            ptr_->Release();
            ptr_ = nullptr;
        }
    }

  private:
    T* ptr_ = nullptr;
};

}  // namespace sysdiag::win
