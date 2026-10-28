#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _WIN32_WINNT 0x0602
#include <windows.h>
#include <Xinput.h>
#include <cstring>
#include <cwchar>

// Original, process-local input adapter. Only an explicitly named session can
// supply the virtual controller. No driver installation or physical FFB output.
// The game-specific deployment is intentionally separate from this DLL.
namespace {
INIT_ONCE init = INIT_ONCE_STATIC_INIT;
HMODULE systemInput = nullptr;
HANDLE mapping = nullptr;
const BYTE* shared = nullptr;
bool configured = false;
volatile LONG inputEnabled = 1;
SRWLOCK mappingLock = SRWLOCK_INIT;
wchar_t mapName[128] = {};

BOOL CALLBACK Initialize(PINIT_ONCE, PVOID, PVOID*) {
    DWORD count = GetEnvironmentVariableW(L"TWIN_KART_INPUT_MAP", mapName, 128);
    const wchar_t prefix[] = L"Local\\KartLocalWheel-";
    const size_t prefixLength = sizeof(prefix) / sizeof(wchar_t) - 1;
    configured = count > prefixLength && count < 128 &&
        wcsncmp(mapName, prefix, prefixLength) == 0;
    wchar_t directory[MAX_PATH] = {};
    UINT length = GetSystemDirectoryW(directory, MAX_PATH);
    if (length && length < MAX_PATH - 20) {
        const wchar_t* names[] = { L"\\xinput1_3.dll", L"\\xinput1_4.dll", L"\\xinput9_1_0.dll" };
        for (const auto* name : names) {
            wchar_t path[MAX_PATH] = {};
            wcscpy_s(path, directory); wcscat_s(path, name);
            systemInput = LoadLibraryExW(path, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (systemInput) break;
        }
    }
    return TRUE;
}
void EnsureInitialized() { InitOnceExecuteOnce(&init, Initialize, nullptr, nullptr); }
template<typename T> T System(const char* name) {
    EnsureInitialized();
    return systemInput ? reinterpret_cast<T>(GetProcAddress(systemInput, name)) : nullptr;
}
bool VirtualSlot(DWORD index) { EnsureInitialized(); return configured && index == 0; }

bool Snapshot(XINPUT_STATE* result) {
    EnsureInitialized();
    AcquireSRWLockExclusive(&mappingLock);
    if (!shared) {
        mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, mapName);
        if (mapping) {
            shared = static_cast<const BYTE*>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 64));
            if (!shared) { CloseHandle(mapping); mapping = nullptr; }
        }
    }
    bool valid = false;
    if (shared) {
        for (int attempt = 0; attempt < 4; ++attempt) {
            DWORD before = *reinterpret_cast<const volatile DWORD*>(shared + 8);
            if (before & 1) continue;
            MemoryBarrier();
            BYTE copy[64]; memcpy(copy, shared, sizeof(copy));
            MemoryBarrier();
            DWORD after = *reinterpret_cast<const volatile DWORD*>(shared + 8);
            if (before != after || (after & 1)) continue;
            DWORD magic, version, enabled, heartbeat, packet;
            memcpy(&magic, copy, 4); memcpy(&version, copy + 4, 4);
            memcpy(&enabled, copy + 12, 4); memcpy(&heartbeat, copy + 16, 4);
            memcpy(&packet, copy + 32, 4);
            if (magic != 0x4B415254 || version != 1 || enabled != 1 ||
                static_cast<DWORD>(GetTickCount() - heartbeat) > 250) break;
            if (result) {
                ZeroMemory(result, sizeof(*result));
                result->dwPacketNumber = packet;
                if (InterlockedCompareExchange(&inputEnabled, 1, 1)) {
                    memcpy(&result->Gamepad.wButtons, copy + 20, 2);
                    memcpy(&result->Gamepad.sThumbLX, copy + 22, 2);
                    memcpy(&result->Gamepad.sThumbLY, copy + 24, 2);
                    memcpy(&result->Gamepad.sThumbRX, copy + 26, 2);
                    memcpy(&result->Gamepad.sThumbRY, copy + 28, 2);
                    result->Gamepad.bLeftTrigger = copy[30];
                    result->Gamepad.bRightTrigger = copy[31];
                }
            }
            valid = true;
            break;
        }
    }
    ReleaseSRWLockExclusive(&mappingLock);
    return valid;
}
}

extern "C" DWORD WINAPI XInputGetState(DWORD index, XINPUT_STATE* state) noexcept {
    if (!state || index >= XUSER_MAX_COUNT) return ERROR_BAD_ARGUMENTS;
    if (VirtualSlot(index)) {
        ZeroMemory(state, sizeof(*state));
        return Snapshot(state) ? ERROR_SUCCESS : ERROR_DEVICE_NOT_CONNECTED;
    }
    auto call = System<decltype(&XInputGetState)>("XInputGetState");
    return call ? call(index, state) : ERROR_DEVICE_NOT_CONNECTED;
}
extern "C" DWORD WINAPI XInputGetStateEx(DWORD index, XINPUT_STATE* state) {
    if (!state || index >= XUSER_MAX_COUNT) return ERROR_BAD_ARGUMENTS;
    if (VirtualSlot(index)) return XInputGetState(index, state);
    auto call = System<decltype(&XInputGetStateEx)>(MAKEINTRESOURCEA(100));
    return call ? call(index, state) : XInputGetState(index, state);
}
extern "C" DWORD WINAPI XInputSetState(DWORD index, XINPUT_VIBRATION* vibration) noexcept {
    if (!vibration || index >= XUSER_MAX_COUNT) return ERROR_BAD_ARGUMENTS;
    if (VirtualSlot(index)) return Snapshot(nullptr) ? ERROR_SUCCESS : ERROR_DEVICE_NOT_CONNECTED;
    auto call = System<decltype(&XInputSetState)>("XInputSetState");
    return call ? call(index, vibration) : ERROR_DEVICE_NOT_CONNECTED;
}
extern "C" DWORD WINAPI XInputGetCapabilities(DWORD index, DWORD flags, XINPUT_CAPABILITIES* caps) noexcept {
    if (!caps || index >= XUSER_MAX_COUNT) return ERROR_BAD_ARGUMENTS;
    if (VirtualSlot(index)) {
        ZeroMemory(caps, sizeof(*caps));
        if (!Snapshot(nullptr)) return ERROR_DEVICE_NOT_CONNECTED;
        caps->Type = XINPUT_DEVTYPE_GAMEPAD; caps->SubType = XINPUT_DEVSUBTYPE_GAMEPAD;
        caps->Gamepad.wButtons = 0xF3FF;
        caps->Gamepad.bLeftTrigger = caps->Gamepad.bRightTrigger = 255;
        caps->Gamepad.sThumbLX = 32767;
        return ERROR_SUCCESS;
    }
    auto call = System<decltype(&XInputGetCapabilities)>("XInputGetCapabilities");
    return call ? call(index, flags, caps) : ERROR_DEVICE_NOT_CONNECTED;
}
extern "C" void WINAPI XInputEnable(BOOL enable) noexcept {
    InterlockedExchange(&inputEnabled, enable ? 1 : 0);
    auto call = System<decltype(&XInputEnable)>("XInputEnable");
    if (call) call(enable);
}
extern "C" DWORD WINAPI XInputGetBatteryInformation(DWORD index, BYTE type, XINPUT_BATTERY_INFORMATION* battery) noexcept {
    if (!battery || index >= XUSER_MAX_COUNT) return ERROR_BAD_ARGUMENTS;
    if (VirtualSlot(index)) {
        ZeroMemory(battery, sizeof(*battery));
        if (!Snapshot(nullptr)) return ERROR_DEVICE_NOT_CONNECTED;
        battery->BatteryType = BATTERY_TYPE_WIRED; battery->BatteryLevel = BATTERY_LEVEL_FULL;
        return ERROR_SUCCESS;
    }
    auto call = System<decltype(&XInputGetBatteryInformation)>("XInputGetBatteryInformation");
    return call ? call(index, type, battery) : ERROR_DEVICE_NOT_CONNECTED;
}
extern "C" DWORD WINAPI XInputGetKeystroke(DWORD index, DWORD reserved, PXINPUT_KEYSTROKE stroke) noexcept {
    if (!stroke) return ERROR_BAD_ARGUMENTS;
    if (VirtualSlot(index)) {
        ZeroMemory(stroke, sizeof(*stroke));
        return Snapshot(nullptr) ? ERROR_EMPTY : ERROR_DEVICE_NOT_CONNECTED;
    }
    auto call = System<decltype(&XInputGetKeystroke)>("XInputGetKeystroke");
    return call ? call(index, reserved, stroke) : ERROR_DEVICE_NOT_CONNECTED;
}
extern "C" DWORD WINAPI XInputGetAudioDeviceIds(DWORD index, LPWSTR render, UINT* renderSize, LPWSTR capture, UINT* captureSize) noexcept {
    if (VirtualSlot(index)) {
        if (render && renderSize && *renderSize) *render = 0;
        if (capture && captureSize && *captureSize) *capture = 0;
        if (renderSize) *renderSize = 0;
        if (captureSize) *captureSize = 0;
        return Snapshot(nullptr) ? ERROR_SUCCESS : ERROR_DEVICE_NOT_CONNECTED;
    }
    auto call = System<decltype(&XInputGetAudioDeviceIds)>("XInputGetAudioDeviceIds");
    return call ? call(index, render, renderSize, capture, captureSize) : ERROR_DEVICE_NOT_CONNECTED;
}
extern "C" DWORD WINAPI XInputGetDSoundAudioDeviceGuids(DWORD index, GUID* render, GUID* capture) {
    if (VirtualSlot(index)) {
        if (render) ZeroMemory(render, sizeof(*render));
        if (capture) ZeroMemory(capture, sizeof(*capture));
        return Snapshot(nullptr) ? ERROR_SUCCESS : ERROR_DEVICE_NOT_CONNECTED;
    }
    auto call = System<decltype(&XInputGetDSoundAudioDeviceGuids)>("XInputGetDSoundAudioDeviceGuids");
    return call ? call(index, render, capture) : ERROR_DEVICE_NOT_CONNECTED;
}
