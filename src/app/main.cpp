// Entry point: parse arguments -> collect -> format -> write.
// All real work lives in sysdiag_core and sysdiag_windows.

#include <exception>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "core/cli.hpp"
#include "core/formatters.hpp"
#include "core/report_builder.hpp"
#include "platform/windows/collectors.hpp"
#include "platform/windows/console.hpp"
#include "platform/windows/win_util.hpp"

namespace {

enum ExitCode : int { kSuccess = 0, kFailure = 1, kUsage = 2 };

using sysdiag::win::Stream;

int run(std::span<const std::string_view> args) {
    namespace cli = sysdiag::cli;

    const cli::ParseResult parsed = cli::parse_arguments(args);
    if (const auto* error = std::get_if<cli::Error>(&parsed)) {
        sysdiag::win::write(Stream::Err, "sysdiag: error: " + error->message +
                                             "\nRun 'sysdiag --help' for usage.\n");
        return kUsage;
    }

    const auto& request = std::get<cli::Request>(parsed);
    switch (request.command) {
        case cli::Command::Help:
            return sysdiag::win::write(Stream::Out, cli::help_text()) ? kSuccess : kFailure;
        case cli::Command::Version:
            return sysdiag::win::write(Stream::Out, cli::version_text()) ? kSuccess : kFailure;
        case cli::Command::Run: break;
    }

    const sysdiag::SystemReport report =
        sysdiag::build_report(request.options, sysdiag::win::make_probe());
    const std::string output = request.options.format == cli::OutputFormat::Json
                                   ? sysdiag::format_json(report)
                                   : sysdiag::format_text(report);
    if (!sysdiag::win::write(Stream::Out, output)) {
        return kFailure;  // e.g. stdout closed; nothing useful left to report
    }
    return kSuccess;
}

}  // namespace

// wmain receives arguments as UTF-16, so non-ASCII input is never mangled
// by the ANSI code page before we see it.
int wmain(int argc, wchar_t* argv[]) {
    try {
        std::vector<std::string> storage;
        for (int i = 1; i < argc; ++i) {
            storage.push_back(argv[i] != nullptr ? sysdiag::win::to_utf8(argv[i]) : std::string{});
        }
        const std::vector<std::string_view> args(storage.begin(), storage.end());
        return run(args);
    } catch (const std::exception& e) {
        sysdiag::win::write(Stream::Err, std::string("sysdiag: fatal error: ") + e.what() + "\n");
    } catch (...) {
        sysdiag::win::write(Stream::Err, "sysdiag: fatal error: unknown exception\n");
    }
    return kFailure;
}
