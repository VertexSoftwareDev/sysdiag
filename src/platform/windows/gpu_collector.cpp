#include <iterator>

#include "core/hw_names.hpp"
#include "platform/windows/collectors.hpp"
#include "platform/windows/win_util.hpp"

// DXGI is used instead of WMI (Win32_VideoController): WMI's AdapterRAM is a
// 32-bit value that caps at 4 GiB, and WMI needs COM initialisation plus a
// much slower query. CreateDXGIFactory1 does not require CoInitialize.

namespace sysdiag::win {
namespace {

constexpr UINT kMaxAdapters = 64;  // guards against a misbehaving enumerator

}  // namespace

GpuSection collect_gpus() {
    GpuSection section;

    ComPtr<IDXGIFactory1> factory;
    HRESULT hr = ::CreateDXGIFactory1(IID_PPV_ARGS(factory.put()));
    if (FAILED(hr)) {
        section.errors.push_back(hresult_error("CreateDXGIFactory1", hr));
        return section;
    }

    for (UINT index = 0; index < kMaxAdapters; ++index) {
        ComPtr<IDXGIAdapter1> adapter;
        hr = factory->EnumAdapters1(index, adapter.put());
        if (hr == DXGI_ERROR_NOT_FOUND) {
            break;
        }
        if (FAILED(hr)) {
            section.errors.push_back(hresult_error("IDXGIFactory1::EnumAdapters1", hr));
            break;
        }

        DXGI_ADAPTER_DESC1 desc{};
        hr = adapter->GetDesc1(&desc);
        if (FAILED(hr)) {
            section.errors.push_back(hresult_error("IDXGIAdapter1::GetDesc1", hr));
            continue;
        }
        // Skip "Microsoft Basic Render Driver" and other software rasterizers.
        if ((desc.Flags & static_cast<UINT>(DXGI_ADAPTER_FLAG_SOFTWARE)) != 0) {
            continue;
        }

        GpuAdapter gpu;
        gpu.name = to_utf8_bounded(desc.Description, std::size(desc.Description));
        gpu.vendor_id = desc.VendorId;
        gpu.device_id = desc.DeviceId;
        gpu.vendor = names::gpu_vendor(desc.VendorId);
        gpu.dedicated_video_memory_bytes = desc.DedicatedVideoMemory;
        gpu.dedicated_system_memory_bytes = desc.DedicatedSystemMemory;
        gpu.shared_system_memory_bytes = desc.SharedSystemMemory;
        section.adapters.push_back(std::move(gpu));
    }
    return section;
}

}  // namespace sysdiag::win
