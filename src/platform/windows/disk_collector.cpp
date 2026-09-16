#include <array>
#include <string>
#include <vector>

#include "core/hw_names.hpp"
#include "core/units.hpp"
#include "platform/windows/collectors.hpp"
#include "platform/windows/win_util.hpp"

namespace sysdiag::win {
namespace {

// Errors that simply mean "no media inserted" and are not worth reporting.
bool is_not_ready(DWORD error) noexcept {
    return error == ERROR_NOT_READY || error == ERROR_NO_MEDIA_IN_DRIVE ||
           error == ERROR_UNRECOGNIZED_MEDIA;
}

std::vector<std::wstring> logical_drive_roots(DiskSection& section) {
    std::vector<wchar_t> buffer(512);
    for (int attempt = 0; attempt < 3; ++attempt) {
        const DWORD result =
            ::GetLogicalDriveStringsW(static_cast<DWORD>(buffer.size()), buffer.data());
        if (result == 0) {
            section.errors.push_back(win32_error("GetLogicalDriveStringsW", ::GetLastError()));
            return {};
        }
        if (result >= buffer.size()) {
            // Buffer too small: `result` is the required size including the final NUL.
            buffer.resize(static_cast<std::size_t>(result) + 1);
            continue;
        }
        // Success: buffer holds "C:\<NUL>D:\<NUL>...<NUL>", `result` excludes the last NUL.
        std::vector<std::wstring> roots;
        std::size_t start = 0;
        for (std::size_t i = 0; i < result; ++i) {
            if (buffer[i] == L'\0') {
                if (i > start) {
                    roots.emplace_back(buffer.data() + start, i - start);
                }
                start = i + 1;
            }
        }
        return roots;
    }
    section.errors.push_back({"GetLogicalDriveStringsW", std::nullopt,
                              "drive list kept changing while it was being read"});
    return {};
}

DiskVolume inspect_volume(const std::wstring& root, UINT drive_type, DiskSection& section) {
    DiskVolume volume;
    volume.root = to_utf8(root);
    volume.drive_type = names::drive_type(drive_type);

    ULARGE_INTEGER available_to_caller{};
    ULARGE_INTEGER total{};
    ULARGE_INTEGER total_free{};
    if (::GetDiskFreeSpaceExW(root.c_str(), &available_to_caller, &total, &total_free) == FALSE) {
        const DWORD error = ::GetLastError();
        if (!is_not_ready(error)) {
            section.errors.push_back(
                win32_error("GetDiskFreeSpaceExW(" + volume.root + ")", error));
        }
        return volume;
    }
    volume.ready = true;
    const auto usage = units::usage_from_free(total.QuadPart, total_free.QuadPart);
    volume.total_bytes = total.QuadPart;
    volume.free_bytes = total.QuadPart - usage.used;  // == min(free, total)
    volume.used_bytes = usage.used;
    volume.usage_percent = usage.percent;

    std::array<wchar_t, MAX_PATH + 1> label{};
    std::array<wchar_t, MAX_PATH + 1> file_system{};
    if (::GetVolumeInformationW(root.c_str(), label.data(), static_cast<DWORD>(label.size()),
                                nullptr, nullptr, nullptr, file_system.data(),
                                static_cast<DWORD>(file_system.size())) != FALSE) {
        volume.label = to_utf8_bounded(label.data(), label.size());
        volume.file_system = to_utf8_bounded(file_system.data(), file_system.size());
    } else {
        section.errors.push_back(
            win32_error("GetVolumeInformationW(" + volume.root + ")", ::GetLastError()));
    }
    return volume;
}

}  // namespace

DiskSection collect_disks() {
    DiskSection section;
    const ThreadErrorModeGuard no_error_dialogs;

    for (const auto& root : logical_drive_roots(section)) {
        const UINT type = ::GetDriveTypeW(root.c_str());
        if (type == DRIVE_NO_ROOT_DIR) {
            continue;  // drive letter vanished between enumeration and inspection
        }
        section.volumes.push_back(inspect_volume(root, type, section));
    }
    return section;
}

}  // namespace sysdiag::win
