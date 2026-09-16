#include "platform/windows/console.hpp"

#include <algorithm>

#include "platform/windows/win_util.hpp"

namespace sysdiag::win {
namespace {

constexpr std::size_t kChunk = 16 * 1024;  // keeps every length well inside DWORD

bool write_console(HANDLE handle, std::string_view utf8) {
    const std::wstring wide = to_wide(utf8);
    std::size_t offset = 0;
    while (offset < wide.size()) {
        const DWORD request = static_cast<DWORD>(std::min(kChunk, wide.size() - offset));
        DWORD written = 0;
        if (::WriteConsoleW(handle, wide.data() + offset, request, &written, nullptr) == FALSE ||
            written == 0) {
            return false;
        }
        offset += written;
    }
    return true;
}

bool write_file(HANDLE handle, std::string_view bytes) {
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const DWORD request = static_cast<DWORD>(std::min(kChunk, bytes.size() - offset));
        DWORD written = 0;
        if (::WriteFile(handle, bytes.data() + offset, request, &written, nullptr) == FALSE ||
            written == 0) {
            return false;
        }
        offset += written;
    }
    return true;
}

}  // namespace

bool write(Stream stream, std::string_view utf8) {
    const HANDLE handle =
        ::GetStdHandle(stream == Stream::Out ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE);
    if (handle == INVALID_HANDLE_VALUE || handle == nullptr) {
        return false;
    }
    DWORD mode = 0;
    if (::GetConsoleMode(handle, &mode) != FALSE) {
        return write_console(handle, utf8);
    }
    return write_file(handle, utf8);
}

}  // namespace sysdiag::win
