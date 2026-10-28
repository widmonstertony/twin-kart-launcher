#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>

#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "user32.lib")

// This DLL is loaded only by TwinKartLauncher. It never installs a driver,
// injects into a game, creates force-feedback effects, or changes device data.
// All names are UTF-16 and all structs use the Windows x64 default packing.
extern "C" {
struct KD_DeviceInfo {
    GUID instanceGuid;
    GUID productGuid;
    DWORD vendorId;
    DWORD productId;
    DWORD deviceType;
    WCHAR instanceName[260];
    WCHAR productName[260];
};

struct KD_AxisInfo {
    DWORD offset;
    LONG minimum;
    LONG maximum;
    DWORD hasRange;
    WCHAR name[260];
};

struct KD_InputSample {
    LONG axes[8];          // DIJOYSTATE2: X, Y, Z, Rx, Ry, Rz, slider 0, slider 1
    BYTE buttons[128];     // high bit set means pressed
    DWORD pov[4];          // 0xFFFFFFFF means centered
};
}

static_assert(sizeof(KD_DeviceInfo) == 1084, "KD_DeviceInfo ABI changed");
static_assert(sizeof(KD_AxisInfo) == 536, "KD_AxisInfo ABI changed");
static_assert(sizeof(KD_InputSample) == 176, "KD_InputSample ABI changed");

namespace {
struct DeviceContext {
    GUID instanceGuid;
    HWND window;
    IDirectInputDevice8W* device;

    DeviceContext(const GUID& guid, HWND hwnd)
        : instanceGuid(guid), window(hwnd), device(nullptr) {}

    ~DeviceContext() {
        if (device != nullptr) {
            device->Unacquire();
            device->Release();
        }
    }
};

std::mutex g_mutex;
IDirectInput8W* g_directInput = nullptr;
std::unordered_map<int, std::unique_ptr<DeviceContext>> g_devices;
int g_nextHandle = 1;

HRESULT EnsureDirectInput() {
    if (g_directInput != nullptr) return S_OK;
    return DirectInput8Create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION,
        IID_IDirectInput8W, reinterpret_cast<void**>(&g_directInput), nullptr);
}

void Detach(DeviceContext& context) {
    if (context.device == nullptr) return;
    context.device->Unacquire();
    context.device->Release();
    context.device = nullptr;
}

HRESULT Attach(DeviceContext& context) {
    if (context.device != nullptr) return S_OK;
    HRESULT hr = g_directInput->CreateDevice(context.instanceGuid,
        &context.device, nullptr);
    if (FAILED(hr)) return hr;
    hr = context.device->SetDataFormat(&c_dfDIJoystick2);
    if (SUCCEEDED(hr)) {
        // The launcher can keep reading while an official client has focus.
        hr = context.device->SetCooperativeLevel(context.window,
            DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
    }
    if (FAILED(hr)) {
        Detach(context);
        return hr;
    }
    context.device->Acquire(); // A later Read retries if this is temporarily lost.
    return S_OK;
}

struct DeviceCollector {
    IDirectInput8W* directInput;
    std::vector<KD_DeviceInfo> entries;
};

BOOL CALLBACK CollectDevice(const DIDEVICEINSTANCEW* source, VOID* user) {
    DeviceCollector* collector = static_cast<DeviceCollector*>(user);
    KD_DeviceInfo info = {};
    info.instanceGuid = source->guidInstance;
    info.productGuid = source->guidProduct;
    info.deviceType = source->dwDevType;
    lstrcpynW(info.instanceName, source->tszInstanceName, 260);
    lstrcpynW(info.productName, source->tszProductName, 260);

    IDirectInputDevice8W* device = nullptr;
    if (SUCCEEDED(collector->directInput->CreateDevice(source->guidInstance,
            &device, nullptr))) {
        DIPROPDWORD property = {};
        property.diph.dwSize = sizeof(property);
        property.diph.dwHeaderSize = sizeof(property.diph);
        property.diph.dwObj = 0;
        property.diph.dwHow = DIPH_DEVICE;
        if (SUCCEEDED(device->GetProperty(DIPROP_VIDPID, &property.diph))) {
            info.vendorId = LOWORD(property.dwData);
            info.productId = HIWORD(property.dwData);
        }
        device->Release();
    }
    collector->entries.push_back(info);
    return DIENUM_CONTINUE;
}

struct AxisCollector {
    IDirectInputDevice8W* device;
    std::vector<KD_AxisInfo> entries;
};

BOOL CALLBACK CollectAxis(const DIDEVICEOBJECTINSTANCEW* source, VOID* user) {
    // DIJOYSTATE2's first eight LONGs are position axes. Later offsets are
    // POV, buttons, velocity, acceleration, and force data rather than pedals.
    if (source->dwOfs >= offsetof(DIJOYSTATE2, rgdwPOV) ||
        source->dwOfs % sizeof(LONG) != 0) return DIENUM_CONTINUE;

    AxisCollector* collector = static_cast<AxisCollector*>(user);
    KD_AxisInfo info = {};
    info.offset = source->dwOfs;
    lstrcpynW(info.name, source->tszName, 260);
    DIPROPRANGE range = {};
    range.diph.dwSize = sizeof(range);
    range.diph.dwHeaderSize = sizeof(range.diph);
    range.diph.dwObj = source->dwOfs;
    range.diph.dwHow = DIPH_BYOFFSET;
    if (SUCCEEDED(collector->device->GetProperty(DIPROP_RANGE,
            &range.diph))) {
        info.minimum = range.lMin;
        info.maximum = range.lMax;
        info.hasRange = 1;
    }
    collector->entries.push_back(info);
    return DIENUM_CONTINUE;
}

template <typename T>
int CopyEntries(const std::vector<T>& entries, T* output, int capacity) {
    if (capacity < 0 || (capacity > 0 && output == nullptr)) return E_INVALIDARG;
    const int count = static_cast<int>(entries.size());
    const int copied = std::min(count, capacity);
    if (copied > 0) std::memcpy(output, entries.data(), copied * sizeof(T));
    return count;
}
} // namespace

extern "C" __declspec(dllexport) int __stdcall KD_EnumerateDevices(
    KD_DeviceInfo* output, int capacity) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (capacity < 0 || (capacity > 0 && output == nullptr)) return E_INVALIDARG;
    const HRESULT init = EnsureDirectInput();
    if (FAILED(init)) return init;
    DeviceCollector collector = { g_directInput, {} };
    const HRESULT hr = g_directInput->EnumDevices(DI8DEVCLASS_GAMECTRL,
        CollectDevice, &collector, DIEDFL_ATTACHEDONLY);
    if (FAILED(hr)) return hr;
    return CopyEntries(collector.entries, output, capacity);
}

extern "C" __declspec(dllexport) int __stdcall KD_EnumerateAxes(
    const GUID* instanceGuid, KD_AxisInfo* output, int capacity) {
    if (instanceGuid == nullptr) return E_POINTER;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (capacity < 0 || (capacity > 0 && output == nullptr)) return E_INVALIDARG;
    const HRESULT init = EnsureDirectInput();
    if (FAILED(init)) return init;
    IDirectInputDevice8W* device = nullptr;
    HRESULT hr = g_directInput->CreateDevice(*instanceGuid, &device, nullptr);
    if (FAILED(hr)) return hr;
    hr = device->SetDataFormat(&c_dfDIJoystick2);
    if (SUCCEEDED(hr)) {
        AxisCollector collector = { device, {} };
        hr = device->EnumObjects(CollectAxis, &collector, DIDFT_AXIS);
        if (SUCCEEDED(hr)) {
            std::sort(collector.entries.begin(), collector.entries.end(),
                [](const KD_AxisInfo& a, const KD_AxisInfo& b) {
                    return a.offset < b.offset;
                });
            const int count = CopyEntries(collector.entries, output, capacity);
            device->Release();
            return count;
        }
    }
    device->Release();
    return hr;
}

extern "C" __declspec(dllexport) int __stdcall KD_Open(
    const GUID* instanceGuid, HWND hwnd) {
    if (instanceGuid == nullptr) return E_POINTER;
    std::lock_guard<std::mutex> lock(g_mutex);
    const HRESULT init = EnsureDirectInput();
    if (FAILED(init)) return init;
    const HWND window = IsWindow(hwnd) ? hwnd : GetDesktopWindow();
    int handle = g_nextHandle;
    while (g_devices.find(handle) != g_devices.end()) {
        handle = handle == INT_MAX ? 1 : handle + 1;
    }
    g_nextHandle = handle == INT_MAX ? 1 : handle + 1;
    std::unique_ptr<DeviceContext> context(
        new DeviceContext(*instanceGuid, window));
    // Keep the handle even when the device vanishes between enumeration and
    // Open; Read will retry attachment after reconnection.
    Attach(*context);
    g_devices.emplace(handle, std::move(context));
    return handle;
}

extern "C" __declspec(dllexport) int __stdcall KD_Read(
    int handle, KD_InputSample* output) {
    if (output == nullptr) return E_POINTER;
    std::lock_guard<std::mutex> lock(g_mutex);
    const auto found = g_devices.find(handle);
    if (found == g_devices.end()) return E_HANDLE;
    std::memset(output, 0, sizeof(*output));
    DeviceContext& context = *found->second;
    if (context.device == nullptr && FAILED(Attach(context))) return 1;

    HRESULT hr = context.device->Poll();
    if (FAILED(hr)) {
        const HRESULT acquired = context.device->Acquire();
        if (SUCCEEDED(acquired)) hr = context.device->Poll();
    }
    DIJOYSTATE2 state = {};
    if (SUCCEEDED(hr)) hr = context.device->GetDeviceState(sizeof(state), &state);
    if (FAILED(hr)) {
        Detach(context);
        return 1; // disconnected or temporarily unavailable; retry next Read
    }
    output->axes[0] = state.lX;
    output->axes[1] = state.lY;
    output->axes[2] = state.lZ;
    output->axes[3] = state.lRx;
    output->axes[4] = state.lRy;
    output->axes[5] = state.lRz;
    output->axes[6] = state.rglSlider[0];
    output->axes[7] = state.rglSlider[1];
    std::memcpy(output->buttons, state.rgbButtons, sizeof(output->buttons));
    std::memcpy(output->pov, state.rgdwPOV, sizeof(output->pov));
    return 0;
}

extern "C" __declspec(dllexport) void __stdcall KD_Close(int handle) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_devices.erase(handle);
}

extern "C" __declspec(dllexport) void __stdcall KD_Shutdown() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_devices.clear();
    if (g_directInput != nullptr) {
        g_directInput->Release();
        g_directInput = nullptr;
    }
}
