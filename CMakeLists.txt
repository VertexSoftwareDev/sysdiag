cmake_minimum_required(VERSION 3.21)

project(sysdiag
    VERSION 1.0.0
    DESCRIPTION "Windows system diagnostics command-line tool"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SYSDIAG_BUILD_TESTS "Build unit tests" ${PROJECT_IS_TOP_LEVEL})
option(SYSDIAG_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(SYSDIAG_ENABLE_SANITIZERS "Enable ASan/UBSan (GCC/Clang only)" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(cmake/CompilerOptions.cmake)

# Version header generated from the project() version (single source of truth).
set(SYSDIAG_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(cmake/version.hpp.in "${SYSDIAG_GENERATED_DIR}/sysdiag/version.hpp" @ONLY)

# ---------------------------------------------------------------------------
# Core: platform-independent logic (data model, CLI, formatting, math).
# Builds on any platform so it can be unit-tested everywhere.
# ---------------------------------------------------------------------------
add_library(sysdiag_core STATIC
    src/core/cli.cpp
    src/core/cpu_usage.cpp
    src/core/hw_names.cpp
    src/core/json_formatter.cpp
    src/core/json_writer.cpp
    src/core/report_builder.cpp
    src/core/text.cpp
    src/core/text_formatter.cpp
    src/core/units.cpp)
target_include_directories(sysdiag_core PUBLIC src "${SYSDIAG_GENERATED_DIR}")
sysdiag_apply_compiler_options(sysdiag_core)

# ---------------------------------------------------------------------------
# Windows collectors and the executable.
# ---------------------------------------------------------------------------
if(WIN32)
    add_library(sysdiag_windows STATIC
        src/platform/windows/console.cpp
        src/platform/windows/cpu_collector.cpp
        src/platform/windows/disk_collector.cpp
        src/platform/windows/gpu_collector.cpp
        src/platform/windows/memory_collector.cpp
        src/platform/windows/network_collector.cpp
        src/platform/windows/os_collector.cpp
        src/platform/windows/probe.cpp
        src/platform/windows/win_util.cpp)
    target_link_libraries(sysdiag_windows PUBLIC sysdiag_core PRIVATE dxgi iphlpapi ws2_32)
    target_compile_definitions(sysdiag_windows PRIVATE
        WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE _WIN32_WINNT=0x0A00 WINVER=0x0A00)
    sysdiag_apply_compiler_options(sysdiag_windows)

    add_executable(sysdiag src/app/main.cpp)
    target_link_libraries(sysdiag PRIVATE sysdiag_windows)
    target_compile_definitions(sysdiag PRIVATE
        WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE _WIN32_WINNT=0x0A00 WINVER=0x0A00)
    sysdiag_apply_compiler_options(sysdiag)

    if(MINGW)
        # wmain entry point; static runtime so the .exe runs without MinGW DLLs.
        target_link_options(sysdiag PRIVATE -municode -static)
    endif()

    install(TARGETS sysdiag RUNTIME DESTINATION bin)
else()
    message(STATUS "sysdiag: non-Windows host - building core library and tests only")
endif()

if(SYSDIAG_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
