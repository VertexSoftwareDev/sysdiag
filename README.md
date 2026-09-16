# sysdiag — Windows System Diagnostics CLI

[![CI](https://github.com/VertexSoftwareDev/sysdiag/actions/workflows/ci.yml/badge.svg)](https://github.com/VertexSoftwareDev/sysdiag/actions/workflows/ci.yml)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue)
![Platform](https://img.shields.io/badge/platform-Windows%2010%20%7C%2011-lightgrey)
![License](https://img.shields.io/badge/license-MIT-green)

**English** | [Türkçe](README.tr.md)

`sysdiag` is a small, dependency-free command-line tool that shows the state of a
Windows machine: operating system, CPU, memory, disks, GPUs and network adapters.
It prints a readable report for people and stable JSON for scripts.

All values come from the operating system at run time; nothing is hard-coded. If a
value can't be read, the report says so and shows why, and every other value is still
reported.

```text
> sysdiag
sysdiag 1.0.0 - report generated 2026-01-02T03:04:05Z

Operating System
  Product               Windows 11 Pro
  Version               23H2 (build 22631.3880)
  Edition               Professional
  Kernel version        10.0
  Architecture          x64
  Computer name         DEV-PC
  Uptime                1d 2h 3m

CPU
  Name                  AMD Ryzen 7 5800X 8-Core Processor
  Vendor                AuthenticAMD
  Physical cores        8
  Logical processors    16
  Base frequency        3800 MHz
  Usage                 12.3% (sampled over 500 ms)

Memory
  Total                 32.0 GiB
  Used                  12.0 GiB (37.5%)
  Available             20.0 GiB

Disks
  C:\ [fixed, NTFS] "Windows"
    Capacity            500.0 GiB
    Used                375.0 GiB (75.0%)
    Free                125.0 GiB
  D:\ [optical] - not ready (no media or unavailable)

GPU
  [0] NVIDIA GeForce RTX 3070
    Vendor              NVIDIA
    PCI id              0x10DE:0x2484
    Dedicated VRAM      8.0 GiB
    Shared sys memory   16.0 GiB

Network
  Ethernet (Intel(R) Ethernet Connection)
    Type                ethernet
    MAC address         00-1A-2B-3C-4D-5E
    IPv4                192.168.1.10/24
    IPv6                fe80::1/64
    Gateway             192.168.1.1
    Link speed          1 Gbps
```
<sub>Example layout only — the values shown come from the formatter's test data, not
from a real machine.</sub>

## Contents

- [Features](#features)
- [Requirements](#requirements)
- [Building](#building)
- [Usage](#usage)
- [Quick test](#quick-test)
- [JSON output](#json-output)
- [Architecture](#architecture)
- [Error handling](#error-handling)
- [Testing](#testing)
- [Dependencies](#dependencies)
- [Known limitations](#known-limitations)
- [Possible future work](#possible-future-work)
- [License](#license)

## Features

| Section   | Information | Source |
|-----------|-------------|--------|
| `os`      | Product name, display version (e.g. 23H2), edition, build + UBR, kernel version, OS and process architecture, computer name, uptime | `RtlGetVersion`, registry, `IsWow64Process2`, `GetComputerNameExW`, `GetTickCount64` |
| `cpu`     | Model name, vendor, sockets, physical cores, logical processors, base frequency, utilisation | registry / `CPUID`, `GetLogicalProcessorInformationEx`, `GetActiveProcessorCount`, `GetSystemTimes` |
| `memory`  | Total, used, available physical memory and usage % | `GlobalMemoryStatusEx` |
| `disk`    | Each drive-letter volume: type, label, file system, capacity, used, free, usage % | `GetLogicalDriveStringsW`, `GetDiskFreeSpaceExW`, `GetVolumeInformationW` |
| `gpu`     | Every hardware adapter: name, vendor, PCI ids, dedicated VRAM, dedicated/shared system memory | DXGI (`IDXGIFactory1::EnumAdapters1`) |
| `network` | Adapters that are up: name, description, type, MAC, IPv4/IPv6 (CIDR), gateways, link speed | `GetAdaptersAddresses` |

Other behaviour:

- **Two output formats.** Human-readable text by default, and JSON with `--json` for scripts.
- **Select what you need.** Choose any set of sections, e.g. `sysdiag cpu memory`.
- **Partial results.** Each section records its own failures, and a failing section never stops the others.
- **Correct Unicode handling.** Arguments are read as UTF-16 through `wmain`. Console output goes through `WriteConsoleW`, and redirected output is written as UTF-8.
- **Terminal safety.** Text from the OS or from the user is sanitised before it reaches the terminal, so control or escape sequences in a device name can't affect your terminal.

## Requirements

- Windows 10 or Windows 11 (x64; ARM64 should work but is not tested in CI)
- CMake ≥ 3.21
- A C++20 compiler:
  - Visual Studio 2022 or newer (MSVC) with the *Desktop development with C++* workload, **or**
  - MinGW-w64 GCC ≥ 13 / LLVM-MinGW (best-effort)
- Internet access at configure time **only if** you build the tests and GoogleTest is not
  already installed (see [Dependencies](#dependencies))

## Building

### Visual Studio 2022 or newer (recommended)

```powershell
git clone https://github.com/VertexSoftwareDev/sysdiag.git
cd sysdiag

cmake --preset msvc                       # configure (newest Visual Studio generator, x64)
cmake --build --preset msvc-release       # or msvc-debug
ctest --preset msvc-release               # run the unit tests

.\build\msvc\Release\sysdiag.exe
```

If `cmake` is not recognised, run these commands in **Developer PowerShell for VS**
(Visual Studio adds its bundled CMake to `PATH` there). You can also open the folder in
Visual Studio with **File → Open → Folder**. Visual
Studio picks up `CMakePresets.json` automatically.

### Ninja (Developer PowerShell / Developer Command Prompt)

```powershell
cmake --preset ninja-release
cmake --build --preset ninja-release
ctest --preset ninja-release
.\build\ninja-release\sysdiag.exe
```

### CMake options

| Option | Default | Description |
|--------|---------|-------------|
| `SYSDIAG_BUILD_TESTS` | `ON` (top-level) | Build the unit tests |
| `SYSDIAG_WARNINGS_AS_ERRORS` | `OFF` | Treat warnings as errors (CI turns this on) |
| `SYSDIAG_ENABLE_SANITIZERS` | `OFF` | ASan + UBSan for GCC/Clang builds |

To build only the executable, without tests or network access, pass
`-DSYSDIAG_BUILD_TESTS=OFF`. To install it, use
`cmake --install build/msvc --config Release --prefix <dir>`.

## Usage

```text
sysdiag [SECTION...] [--json] [--sample-ms <ms>]
sysdiag --help | --version
```

| Argument | Meaning |
|----------|---------|
| *(none)* / `all` | Show every section |
| `os` | Operating system |
| `cpu` | Processor |
| `memory`, `ram` | Physical memory |
| `disk` | Volumes |
| `gpu` | Graphics adapters |
| `network`, `net` | Network adapters |
| `--json` | Emit JSON instead of text |
| `--sample-ms <ms>` / `--sample-ms=<ms>` | CPU sampling window, 100–5000 ms (default 500) |
| `-h`, `--help`, `/?` | Show help |
| `-V`, `--version` | Show version |

Section names are case-insensitive and may be combined. Sections are always printed
in the fixed order shown above, whatever order you give them in. If `--help` or
`--version` appears anywhere on the command line, it takes precedence over the other
arguments.

```powershell
sysdiag                          # full report
sysdiag cpu ram                  # only CPU and memory
sysdiag disk --json              # disks as JSON
sysdiag cpu --sample-ms 2000     # smoother CPU reading over 2 seconds
sysdiag --json > report.json     # save a snapshot

# PowerShell: list volumes that are more than 90% full
(sysdiag disk --json | ConvertFrom-Json).disk.volumes |
    Where-Object { $_.usage_percent -gt 90 } |
    Select-Object root, usage_percent
```

### Exit codes

| Code | Meaning |
|------|---------|
| `0` | Report produced. Individual values may still be unavailable; check the per-section `errors`. |
| `1` | Unexpected internal failure, or the output could not be written (e.g. closed pipe) |
| `2` | Invalid command line; a message is printed to stderr |

```text
> sysdiag cpux
sysdiag: error: unknown section 'cpux' (valid: all, os, cpu, memory|ram, disk, gpu, network|net)
Run 'sysdiag --help' for usage.
```

## Quick test

After building, the fastest way to check everything end to end is the smoke-test
script. It runs the real executable more than 20 ways and prints one `PASS`/`FAIL` line
per check. The checks cover:

- Exit codes
- Text sections
- JSON validity and value sanity
- Section filtering
- Redirected output
- Rejection of invalid input

```powershell
# From the repository root. -ExecutionPolicy Bypass is only needed because
# Windows blocks scripts by default (it applies to this one process only).
powershell -ExecutionPolicy Bypass -File .\scripts\smoke-test.ps1

# Test another build
powershell -ExecutionPolicy Bypass -File .\scripts\smoke-test.ps1 -Exe .\build\msvc\Debug\sysdiag.exe
```

Expected result: the last line reads `All 21 checks passed.` and the script exits with code 0.

To try things by hand (PowerShell, from the repository root):

```powershell
Set-Alias sysdiag "$PWD\build\msvc\Release\sysdiag.exe"   # shortcut for this session

# Normal use - expect exit code 0
sysdiag                               # full report
sysdiag cpu ram                       # selected sections (aliases work)
sysdiag GPU Net                       # names are case-insensitive
sysdiag cpu --sample-ms 2000          # longer CPU sampling window
sysdiag --version; $LASTEXITCODE

# JSON
$r = sysdiag --json | ConvertFrom-Json
$r.cpu.usage_percent
$r.memory | Format-List
$r.gpu.adapters | Select-Object name, vendor, dedicated_video_memory_bytes
$r.disk.volumes | Where-Object { $_.usage_percent -gt 90 } | Select-Object root, usage_percent
sysdiag memory disk --json > report.json

# Invalid usage - each prints "sysdiag: error: ..." and exit code 2
sysdiag cpux;            $LASTEXITCODE
sysdiag --verbose;       $LASTEXITCODE
sysdiag --sample-ms 1;   $LASTEXITCODE
sysdiag --sample-ms abc; $LASTEXITCODE
sysdiag --json --json;   $LASTEXITCODE

# --help always wins - exit code 0
sysdiag nonsense --help; $LASTEXITCODE
```

To see the CPU reading react to load, run this in one window:

```powershell
1..4 | ForEach-Object { Start-Job { while ($true) {} } }   # start busy loops
```

Then run `sysdiag cpu` in another window. Afterwards, stop the loops with
`Get-Job | Stop-Job; Get-Job | Remove-Job`.

Unit tests:

```powershell
ctest --preset msvc-release                    # whole suite
ctest --preset msvc-release -R Cli             # only tests whose name matches "Cli"
.\build\msvc\tests\Release\sysdiag_tests.exe --gtest_filter=Units.*
```

## JSON output

The JSON document follows these rules, so scripts can rely on its shape:

- **Stable key order.** Keys always appear in the same order, and the output is valid RFC 8259 JSON (UTF-8, pretty-printed).
- **Omitted sections.** Sections you didn't ask for are left out entirely.
- **Missing values are `null`.** A value that couldn't be collected is `null`; it is never `0` or `""`.
- **Raw units.** Sizes are integers in **bytes**, speeds are in **bits per second**, and uptime is in **seconds**.
- **Percentages** are numbers from 0 to 100, rounded to one decimal place.
- **Error lists.** Every section has an `errors` array of `{operation, code, message}` objects. `code` is the Win32 error or HRESULT, or `null` when there is none.
- **Schema version.** `schema_version` is incremented whenever the format changes in an incompatible way.

<details>
<summary>Example (<code>sysdiag --json</code>)</summary>

```json
{
  "schema_version": 1,
  "tool": {
    "name": "sysdiag",
    "version": "1.0.0"
  },
  "generated_at": "2026-01-02T03:04:05Z",
  "os": {
    "product_name": "Windows 11 Pro",
    "display_version": "23H2",
    "edition": "Professional",
    "major_version": 10,
    "minor_version": 0,
    "build_number": 22631,
    "update_revision": 3880,
    "os_architecture": "x64",
    "process_architecture": "x64",
    "computer_name": "DEV-PC",
    "uptime_seconds": 93784,
    "errors": []
  },
  "cpu": {
    "name": "AMD Ryzen 7 5800X 8-Core Processor",
    "vendor": "AuthenticAMD",
    "packages": 1,
    "physical_cores": 8,
    "logical_processors": 16,
    "base_frequency_mhz": 3800,
    "usage_percent": 12.3,
    "sample_interval_ms": 500,
    "errors": []
  },
  "memory": {
    "total_bytes": 34359738368,
    "used_bytes": 12884901888,
    "available_bytes": 21474836480,
    "usage_percent": 37.5,
    "errors": []
  },
  "disk": {
    "volumes": [
      {
        "root": "C:\\",
        "drive_type": "fixed",
        "ready": true,
        "label": "Windows",
        "file_system": "NTFS",
        "total_bytes": 536870912000,
        "used_bytes": 402653184000,
        "free_bytes": 134217728000,
        "usage_percent": 75.0
      },
      {
        "root": "D:\\",
        "drive_type": "optical",
        "ready": false,
        "label": null,
        "file_system": null,
        "total_bytes": null,
        "used_bytes": null,
        "free_bytes": null,
        "usage_percent": null
      }
    ],
    "errors": []
  },
  "gpu": {
    "adapters": [
      {
        "name": "NVIDIA GeForce RTX 3070",
        "vendor": "NVIDIA",
        "vendor_id": 4318,
        "device_id": 9348,
        "dedicated_video_memory_bytes": 8589934592,
        "dedicated_system_memory_bytes": 0,
        "shared_system_memory_bytes": 17179869184
      }
    ],
    "errors": []
  },
  "network": {
    "adapters": [
      {
        "name": "Ethernet",
        "description": "Intel(R) Ethernet Connection",
        "type": "ethernet",
        "mac_address": "00-1A-2B-3C-4D-5E",
        "transmit_bps": 1000000000,
        "receive_bps": 1000000000,
        "ipv4_addresses": [
          "192.168.1.10/24"
        ],
        "ipv6_addresses": [
          "fe80::1/64"
        ],
        "gateways": [
          "192.168.1.1"
        ]
      }
    ],
    "errors": []
  }
}
```
</details>

A section with a problem looks like this:

```json
"memory": {
  "total_bytes": null,
  "used_bytes": null,
  "available_bytes": null,
  "usage_percent": null,
  "errors": [
    { "operation": "GlobalMemoryStatusEx", "code": 5, "message": "Access is denied" }
  ]
}
```

## Architecture

```text
src/
├── core/                  platform-independent, unit-tested (library: sysdiag_core)
│   ├── model.hpp          plain data structs; std::optional = "unknown"
│   ├── cli.*              argument parsing -> Request | Error (no I/O)
│   ├── report_builder.*   runs requested collectors, isolates failures
│   ├── cpu_usage.*        utilisation maths from two time samples
│   ├── units.*            overflow-safe arithmetic, byte/percent/speed formatting
│   ├── hw_names.*         vendor ids, machine types, MAC/IPv4 formatting, ...
│   ├── json_writer.*      small validating streaming JSON writer
│   ├── json_formatter.cpp / text_formatter.cpp
│   └── text.*             UTF-8 validation, terminal-safe sanitising
├── platform/windows/      Win32 code only (library: sysdiag_windows)
│   ├── *_collector.cpp    one file per section
│   ├── win_util.*         UTF-16<->UTF-8, error messages, registry, RAII helpers
│   └── console.*          WriteConsoleW / WriteFile output
└── app/main.cpp           wmain: parse -> collect -> format -> write
tests/                     GoogleTest suite for sysdiag_core
```

**Design decisions**

- **Core and platform are separate.** Everything that can be written without Windows headers lives in
  `sysdiag_core`: parsing, maths, formatting and orchestration. That code builds and is
  tested on any OS, including with sanitizers on Linux in CI. The Windows layer only
  translates API results into the data model.
- **The data model is the seam for testing.** Collectors fill plain structs, and formatters read
  them. `build_report` takes its collectors as `std::function`s, so tests pass in fake
  collectors (including ones that throw) without mocking any Win32 API. There is no
  class hierarchy just for testability.
- **"Unknown" is explicit.** Values are `std::optional`, and failures are data
  (`CollectionError`) attached to their section. Collectors don't throw for OS errors;
  `build_report` still catches exceptions per section as a last line of defence.
- **Only the Win32 APIs that are needed:**
  - **CPU usage:** two `GetSystemTimes` samples, taken `--sample-ms` apart. PDH
    counter paths are localised and WMI is slow and needs COM. The first sample
    never yields a value by itself, and a sample where no time elapsed produces
    "unknown" rather than 0%.
  - **Physical core count:** `GetLogicalProcessorInformationEx` instead of
    `GetSystemInfo`, which is wrong on systems with more than 64 logical processors.
  - **GPU memory:** DXGI instead of WMI, because WMI's `AdapterRAM` is 32-bit and
    tops out at 4 GiB.
  - **Windows version:** `RtlGetVersion` instead of `GetVersionEx`, which reports a
    false version to applications without a manifest. The registry still says
    "Windows 10" on Windows 11, so the build number (≥ 22000) is used to correct it.
  - **API availability:** `IsWow64Process2` is looked up at run time so the binary
    still starts on early Windows 10 builds.
- **No JSON library.** The tool only *writes* JSON. A writer of under 300 lines that checks its
  own nesting and escapes and validates UTF-8 costs less than a dependency.
- **RAII everywhere.** Registry buffers, `LocalFree` memory, COM pointers and the
  thread error mode are all owned by objects. There are no manual cleanup paths and
  no global state.

## Error handling

- **Failures are reported with context.** Every OS failure is reported with the operation that failed, the error code, and the
  system message (from `FormatMessageW`, so it appears in the OS language):

  ```text
  Memory
    Total                 n/a
    ...
    ! GlobalMemoryStatusEx failed: Access is denied (error 5)
  ```
- **Expected conditions aren't errors.** An empty card reader or DVD drive is shown as
  *not ready*, and "no network adapters" is just an empty list.
- **Buffers that can change size are retried.** When a size-query API returns a buffer
  size that is outdated by the time of the second call, the call is retried a bounded
  number of times.
- **Input is untrusted.** Command-line values are parsed with `std::from_chars` using
  explicit range checks, and any user text echoed in an error message is sanitised and
  truncated.
- **Arithmetic can't overflow or divide by zero.** Subtractions saturate at zero,
  percentages of a zero total are `null`, results are clamped to 0–100, and all sizes
  are 64-bit.

## Testing

The unit tests cover:

- CLI parsing, including edge cases and hostile input
- Byte, percent and speed conversions and their limits
- CPU utilisation maths
- JSON escaping and writer misuse
- JSON validity, checked by an independent RFC 8259 validator written for the tests
- Formatter output
- Partial-failure handling in `build_report`

```powershell
ctest --preset msvc-release          # Windows
```

```bash
cmake --preset linux-sanitize        # core library only, with ASan + UBSan
cmake --build --preset linux-sanitize
ctest --preset linux-sanitize
```

**CI** (`.github/workflows/ci.yml`) runs two jobs:

1. **Windows, MSVC (Debug and Release, warnings as errors):**
   - Builds and runs the unit tests.
   - Smoke-tests the real executable with `scripts/smoke-test.ps1`.
   - Uploads the Release `sysdiag.exe` as a build artifact.
2. **Linux, GCC:** builds the core library with ASan and UBSan and runs the unit tests.

The Win32 collectors are not unit-tested, because their results depend on the machine.
They are exercised by [`scripts/smoke-test.ps1`](scripts/smoke-test.ps1), which runs
locally (see [Quick test](#quick-test)) and in CI under both PowerShell 7 and Windows
PowerShell 5.1.

## Dependencies

| Dependency | Used for | Why |
|------------|----------|-----|
| Windows SDK (`dxgi`, `iphlpapi`, `ws2_32`) | Collectors | System APIs; ship with Windows |
| [GoogleTest](https://github.com/google/googletest) 1.15.2 | **Tests only** | Widely known, integrates with CTest and Visual Studio. An installed copy is used when found (`find_package`); otherwise it is downloaded with `FetchContent`. Not linked into `sysdiag.exe`. |

The executable itself has no third-party dependencies.

## Known limitations

- **Windows only.** Only the core library builds on other platforms; there are no Linux
  or macOS collectors yet.
- **CPU:**
  - Utilisation is a single system-wide average over the sampling window. Per-core
    load isn't shown.
  - On machines with more than 64 logical processors, older Windows versions may
    report `GetSystemTimes` values only for the caller's processor group.
  - Performance and efficiency cores on hybrid CPUs aren't distinguished.
  - *Base frequency* is the nominal value stored by the firmware, not the current
    clock speed.
- **GPU:**
  - Reported memory is what DXGI reports. Integrated GPUs typically show a small
    dedicated amount and a large shared amount.
  - Temperatures, utilisation and driver versions aren't reported.
- **Disk:**
  - Only volumes with a drive letter are listed; volumes mounted into folders aren't.
  - A disconnected mapped network drive can make the disk section slow to respond.
  - SMART and health data aren't collected.
- **Network:**
  - Only adapters that are *up* are shown, and loopback is excluded. Virtual adapters
    (Hyper-V, VPN, WSL) are included.
  - Tunnel adapters (e.g. Teredo) have no meaningful MAC address, so none is shown.
  - IPv6 link-local addresses are shown without their zone index.
- **Uptime** comes from `GetTickCount64`. With Windows *Fast Startup* enabled, it counts
  from the last full boot, not the last "shut down".
- **No sensor data.** Temperatures, fan speeds and battery status aren't read, because
  Windows has no dependable vendor-neutral API for them.
- **Large numbers in JSON.** Values are exact 64-bit integers. Parsers that store every
  number as a double (such as JavaScript) lose precision above 2^53 bytes (about 8 PiB).
- **MinGW builds** are best-effort; CI covers MSVC.

## Possible future work

- Per-core CPU utilisation and a `--watch <seconds>` refresh mode
- Battery and power information (`GetSystemPowerStatus`)
- Driver version and GPU utilisation via vendor-neutral APIs where available
- Volumes without drive letters (`FindFirstVolumeW`)
- Linux collectors (`/proc`, `sysfs`) behind the existing data model
- Signed release binaries attached to GitHub Releases

## License

[MIT](LICENSE)
