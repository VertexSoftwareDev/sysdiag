// Single place that includes Windows SDK headers in the right order.
// (winsock2.h must precede windows.h; lean-and-mean/NOMINMAX come from CMake.)
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

// clang-format off
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <objbase.h>
#include <iphlpapi.h>
#include <dxgi.h>
// clang-format on
