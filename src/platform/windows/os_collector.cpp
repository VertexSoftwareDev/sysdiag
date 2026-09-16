#include <algorithm>
#include <exception>
#include <string>

#include "core/hw_names.hpp"
#include "platform/windows/collectors.hpp"
#include "platform/windows/win_util.hpp"

namespace sysdiag::win {
namespace {

constexpr const wchar_t* kVersionKey = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";

// Architecture this binary was compiled for.
constexpr const char* kCompiledArchitecture =
#if defined(_M_ARM64) || defined(__aarch64__)
    "ARM64";
#elif defined(_M_X64) || defined(__x86_64__)
    "x64";
#elif defined(_M_IX86) || defined(__i386__)
    "x86";
#else
    "unknown";
#endif

// Looks up an export at run time. Used for APIs that are not present on every
// Windows 10 build (and for RtlGetVersion, which has no SDK import library).
template <typename Fn>
Fn find_export(const wchar_t* module_name, const char* export_name) noexcept {
    // Both modules are always mapped into a Win32 process, so GetModuleHandle
    // is sufficient and no FreeLibrary is required.
    HMODULE module = ::GetModuleHandleW(module_name);
    if (module == nullptr) {
        return nullptr;
    }
    FARPROC proc = ::GetProcAddress(module, export_name);
    if (proc == nullptr) {
        return nullptr;
    }
    // Casting via void(*)() is the documented way to silence
    // -Wcast-function-type; the target type matches the real signature.
    return reinterpret_cast<Fn>(reinterpret_cast<void (*)()>(proc));
}

// GetVersionEx lies to un-manifested applications; RtlGetVersion does not.
void read_kernel_version(OsInfo& info) {
    using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
    if (const auto rtl_get_version = find_export<RtlGetVersionFn>(L"ntdll.dll", "RtlGetVersion")) {
        RTL_OSVERSIONINFOW version{};
        version.dwOSVersionInfoSize = sizeof(version);
        if (rtl_get_version(&version) == 0 /* STATUS_SUCCESS */) {
            info.major_version = static_cast<std::uint32_t>(version.dwMajorVersion);
            info.minor_version = static_cast<std::uint32_t>(version.dwMinorVersion);
            info.build_number = static_cast<std::uint32_t>(version.dwBuildNumber);
            return;
        }
    }

    // Fallback: registry values present on Windows 10 and later.
    const auto major =
        read_registry_dword(HKEY_LOCAL_MACHINE, kVersionKey, L"CurrentMajorVersionNumber");
    const auto minor =
        read_registry_dword(HKEY_LOCAL_MACHINE, kVersionKey, L"CurrentMinorVersionNumber");
    const auto build = read_registry_string(HKEY_LOCAL_MACHINE, kVersionKey, L"CurrentBuildNumber");
    if (major.value && minor.value) {
        info.major_version = static_cast<std::uint32_t>(*major.value);
        info.minor_version = static_cast<std::uint32_t>(*minor.value);
    } else {
        info.errors.push_back(
            win32_error("read Windows version (RtlGetVersion and registry)",
                        major.error != ERROR_SUCCESS ? major.error : minor.error));
    }
    if (build.value) {
        try {
            // unsigned long is 32-bit on Windows; stoul throws if the value does not fit.
            info.build_number = static_cast<std::uint32_t>(std::stoul(*build.value));
        } catch (const std::exception&) {
            info.errors.push_back({"parse CurrentBuildNumber", std::nullopt,
                                   "unexpected value '" + to_utf8(*build.value) + "'"});
        }
    }
}

void read_product_details(OsInfo& info) {
    if (const auto name = read_registry_string(HKEY_LOCAL_MACHINE, kVersionKey, L"ProductName");
        name.value) {
        info.product_name =
            names::normalize_windows_product_name(to_utf8(*name.value), info.build_number);
    } else {
        info.errors.push_back(win32_error("read ProductName (registry)", name.error));
    }

    // DisplayVersion ("23H2") exists since 20H2; ReleaseId ("2004") before that.
    auto display = read_registry_string(HKEY_LOCAL_MACHINE, kVersionKey, L"DisplayVersion");
    if (!display.value) {
        display = read_registry_string(HKEY_LOCAL_MACHINE, kVersionKey, L"ReleaseId");
    }
    if (display.value && !display.value->empty()) {
        info.display_version = to_utf8(*display.value);
    }

    if (const auto edition = read_registry_string(HKEY_LOCAL_MACHINE, kVersionKey, L"EditionID");
        edition.value && !edition.value->empty()) {
        info.edition = to_utf8(*edition.value);
    }

    if (const auto ubr = read_registry_dword(HKEY_LOCAL_MACHINE, kVersionKey, L"UBR"); ubr.value) {
        info.update_revision = static_cast<std::uint32_t>(*ubr.value);
    }
}

std::string architecture_from_system_info(WORD processor_architecture) {
    switch (processor_architecture) {
        case PROCESSOR_ARCHITECTURE_AMD64: return "x64";
        case PROCESSOR_ARCHITECTURE_ARM64: return "ARM64";
        case PROCESSOR_ARCHITECTURE_INTEL: return "x86";
        case PROCESSOR_ARCHITECTURE_ARM: return "ARM";
        default: return "unknown";
    }
}

void read_architecture(OsInfo& info) {
    info.process_architecture = kCompiledArchitecture;

    // IsWow64Process2 (Windows 10 1511+) correctly reports ARM64 hosts even for
    // emulated processes; fall back to GetNativeSystemInfo when unavailable.
    using IsWow64Process2Fn = BOOL(WINAPI*)(HANDLE, USHORT*, USHORT*);
    if (const auto is_wow64_process2 =
            find_export<IsWow64Process2Fn>(L"kernel32.dll", "IsWow64Process2")) {
        USHORT process_machine = 0;
        USHORT native_machine = 0;
        if (is_wow64_process2(::GetCurrentProcess(), &process_machine, &native_machine) != FALSE) {
            info.os_architecture = names::machine_architecture(native_machine);
            if (process_machine != IMAGE_FILE_MACHINE_UNKNOWN) {  // running under WOW64
                info.process_architecture = names::machine_architecture(process_machine);
            }
            return;
        }
    }
    SYSTEM_INFO system_info{};
    ::GetNativeSystemInfo(&system_info);  // cannot fail
    info.os_architecture = architecture_from_system_info(system_info.wProcessorArchitecture);
}

void read_computer_name(OsInfo& info) {
    constexpr COMPUTER_NAME_FORMAT kFormat = ComputerNameDnsHostname;
    DWORD size = 0;
    if (::GetComputerNameExW(kFormat, nullptr, &size) != FALSE ||
        ::GetLastError() != ERROR_MORE_DATA) {
        info.errors.push_back(win32_error("GetComputerNameExW", ::GetLastError()));
        return;
    }
    std::wstring buffer(size, L'\0');  // size includes the terminator here
    if (::GetComputerNameExW(kFormat, buffer.data(), &size) == FALSE) {
        info.errors.push_back(win32_error("GetComputerNameExW", ::GetLastError()));
        return;
    }
    buffer.resize(std::min<std::size_t>(size, buffer.size()));  // on success size excludes it
    info.computer_name = to_utf8(buffer);
}

}  // namespace

OsInfo collect_os() {
    OsInfo info;
    read_kernel_version(info);
    read_product_details(info);
    read_architecture(info);
    read_computer_name(info);
    info.uptime_seconds = ::GetTickCount64() / 1000ULL;
    return info;
}

}  // namespace sysdiag::win
