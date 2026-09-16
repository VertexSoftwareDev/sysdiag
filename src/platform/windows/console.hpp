// Output that is correct for both an interactive console and redirection.
#pragma once

#include <string_view>

namespace sysdiag::win {

enum class Stream { Out, Err };

// Writes UTF-8 text. On a console the text is converted to UTF-16 and written
// with WriteConsoleW, so non-ASCII characters display correctly without
// changing the console code page. When redirected to a file or pipe the
// UTF-8 bytes are written unchanged. Returns false if the write failed
// (e.g. the pipe was closed).
bool write(Stream stream, std::string_view utf8);

}  // namespace sysdiag::win
