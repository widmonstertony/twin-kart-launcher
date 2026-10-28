#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <bcrypt.h>
#include <cstdio>
#include <cstring>
#include "DlssD3D11.h"

// First-party resource observer. It does not upscale, interpolate frames, or
// claim that an observed depth/motion-format texture is a validated DLSS guide.
namespace {
HANDLE output = INVALID_HANDLE_VALUE;
SRWLOCK logLock = SRWLOCK_INIT;
using Present = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
using Targets = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, ID3D11RenderTargetView* const*, ID3D11DepthStencilView*);
using Resources = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT, ID3D11ShaderResourceView* const*);
using Unordered = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT, ID3D11UnorderedAccessView* const*, const UINT*);
Present present = nullptr;
struct ContextHooks { void** table; Targets targets; Resources pixel, compute; Unordered unordered; };
ContextHooks contexts[8]{};
unsigned contextCount = 0;
SRWLOCK contextLock = SRWLOCK_INIT;
volatile LONG realContextHooked = 0;
volatile LONG frames = 0;
LARGE_INTEGER frequency{}, lastPresent{};
struct Description { UINT width, height, format, bind, samples, role; };
Description observed[256]{};
unsigned observedCount = 0;
using DlssInitialize = unsigned(*)(ID3D11Device*, const wchar_t*, const wchar_t*);
using DlssState = bool(*)(TwinKartDlssState*);
DlssInitialize initializeDlss = nullptr;
DlssState getDlssState = nullptr;
volatile LONG dlssAttempted = 0;
wchar_t dlssCache[32768]{}, dlssProvider[32768]{};
bool Patch(void** table, unsigned slot, void* replacement, void** original);
void STDMETHODCALLTYPE SetTargets(ID3D11DeviceContext*, UINT, ID3D11RenderTargetView* const*, ID3D11DepthStencilView*);
void STDMETHODCALLTYPE SetResources(ID3D11DeviceContext*, UINT, UINT, ID3D11ShaderResourceView* const*);
void STDMETHODCALLTYPE SetComputeResources(ID3D11DeviceContext*, UINT, UINT, ID3D11ShaderResourceView* const*);
void STDMETHODCALLTYPE SetUnordered(ID3D11DeviceContext*, UINT, UINT, ID3D11UnorderedAccessView* const*, const UINT*);
bool HookContext(ID3D11DeviceContext*);
ContextHooks Hooks(ID3D11DeviceContext* context) {
    ContextHooks result{};
    void** table = *reinterpret_cast<void***>(context);
    AcquireSRWLockShared(&contextLock);
    for (unsigned i=0; i<contextCount; ++i) if (contexts[i].table == table) { result = contexts[i]; break; }
    ReleaseSRWLockShared(&contextLock);
    return result;
}
void Log(const char* line) {
    if (output == INVALID_HANDLE_VALUE) return;
    AcquireSRWLockExclusive(&logLock);
    DWORD written; WriteFile(output, line, static_cast<DWORD>(strlen(line)), &written, nullptr);
    ReleaseSRWLockExclusive(&logLock);
}
bool Allowed() {
    wchar_t expected[32768], actual[32768];
    DWORD n = GetEnvironmentVariableW(L"TWIN_KART_ALLOWED_GAME", expected, ARRAYSIZE(expected));
    if (!n || n >= ARRAYSIZE(expected) || !GetModuleFileNameW(nullptr, actual, ARRAYSIZE(actual)) || _wcsicmp(expected, actual)) return false;
    HANDLE file = CreateFileW(actual, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    BCRYPT_ALG_HANDLE algorithm = nullptr; BCRYPT_HASH_HANDLE hash = nullptr;
    bool ok = false;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0 &&
        BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) >= 0) {
        BYTE buffer[65536], digest[32]; DWORD bytes;
        bool valid = true;
        for (;;) {
            if (!ReadFile(file, buffer, sizeof(buffer), &bytes, nullptr)) { valid = false; break; }
            if (!bytes) break;
            if (BCryptHashData(hash, buffer, bytes, 0) < 0) { valid = false; break; }
        }
        if (valid && BCryptFinishHash(hash, digest, 32, 0) >= 0) {
            char text[65]; for (int i=0; i<32; ++i) sprintf_s(text+i*2, 65-i*2, "%02x", digest[i]);
            ok = !strcmp(text, "5041d9f5d45adb4b1c932d8056a89f70db7fa98f93614ac30685cd1adeb9d9c2");
        }
    }
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    CloseHandle(file); return ok;
}
void Observe(ID3D11Resource* resource, unsigned role) {
    if (!resource) return;
    ID3D11Texture2D* texture = nullptr;
    if (FAILED(resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&texture)))) return;
    D3D11_TEXTURE2D_DESC desc{}; texture->GetDesc(&desc); texture->Release();
    if (desc.Width < 128 || desc.Height < 128) return;
    Description key{desc.Width, desc.Height, static_cast<UINT>(desc.Format), desc.BindFlags, desc.SampleDesc.Count, role};
    AcquireSRWLockExclusive(&logLock);
    bool seen = false;
    for (unsigned i=0; i<observedCount; ++i) if (!memcmp(&observed[i], &key, sizeof(key))) { seen = true; break; }
    if (!seen && observedCount < ARRAYSIZE(observed)) {
        observed[observedCount++] = key;
        char line[384]; DWORD written;
        sprintf_s(line, "{\"event\":\"texture_format_observed\",\"frame\":%ld,\"role\":%u,\"width\":%u,\"height\":%u,\"dxgi_format\":%u,\"bind\":%u,\"samples\":%u,\"guide_validated\":false}\r\n",
            frames, role, key.width, key.height, key.format, key.bind, key.samples);
        WriteFile(output, line, static_cast<DWORD>(strlen(line)), &written, nullptr);
    }
    ReleaseSRWLockExclusive(&logLock);
}
template<typename T> void ObserveView(T* view, unsigned role) {
    if (!view) return;
    ID3D11Resource* resource = nullptr; view->GetResource(&resource);
    Observe(resource, role); if (resource) resource->Release();
}
void STDMETHODCALLTYPE SetTargets(ID3D11DeviceContext* context, UINT count, ID3D11RenderTargetView* const* views, ID3D11DepthStencilView* depth) {
    auto original = Hooks(context).targets;
    if (!original) return;
    original(context, count, views, depth);
    if ((frames % 120) != 0) return;
    for (UINT i=0; views && i<count && i<8; ++i) ObserveView(views[i], 1);
    ObserveView(depth, 2);
}
void STDMETHODCALLTYPE SetResources(ID3D11DeviceContext* context, UINT slot, UINT count, ID3D11ShaderResourceView* const* views) {
    auto original = Hooks(context).pixel;
    if (!original) return;
    original(context, slot, count, views);
    if ((frames % 120) != 0) return;
    for (UINT i=0; views && i<count && i<128; ++i) ObserveView(views[i], 3);
}
void STDMETHODCALLTYPE SetComputeResources(ID3D11DeviceContext* context, UINT slot, UINT count, ID3D11ShaderResourceView* const* views) {
    auto original = Hooks(context).compute;
    if (!original) return;
    original(context, slot, count, views);
    if ((frames % 120) != 0) return;
    for (UINT i=0; views && i<count && i<128; ++i) ObserveView(views[i], 5);
}
void STDMETHODCALLTYPE SetUnordered(ID3D11DeviceContext* context, UINT slot, UINT count, ID3D11UnorderedAccessView* const* views, const UINT* counts) {
    auto original = Hooks(context).unordered;
    if (!original) return;
    original(context, slot, count, views, counts);
    if ((frames % 120) != 0) return;
    for (UINT i=0; views && i<count && i<8; ++i) ObserveView(views[i], 6);
}
bool HookContext(ID3D11DeviceContext* context) {
    void** table = *reinterpret_cast<void***>(context);
    AcquireSRWLockExclusive(&contextLock);
    for (unsigned i=0; i<contextCount; ++i) if (contexts[i].table == table) { ReleaseSRWLockExclusive(&contextLock); return true; }
    if (contextCount == ARRAYSIZE(contexts)) { ReleaseSRWLockExclusive(&contextLock); return false; }
    auto& hooks = contexts[contextCount++]; hooks.table = table;
    // Keep each wrapper's original functions with its own vtable. The game's
    // context can differ from the temporary discovery device's context.
    bool a=Patch(table,33,reinterpret_cast<void*>(SetTargets),reinterpret_cast<void**>(&hooks.targets));
    bool b=Patch(table,8,reinterpret_cast<void*>(SetResources),reinterpret_cast<void**>(&hooks.pixel));
    bool c=Patch(table,67,reinterpret_cast<void*>(SetComputeResources),reinterpret_cast<void**>(&hooks.compute));
    bool d=Patch(table,68,reinterpret_cast<void*>(SetUnordered),reinterpret_cast<void**>(&hooks.unordered));
    ReleaseSRWLockExclusive(&contextLock);
    return a && b && c && d;
}
HRESULT STDMETHODCALLTYPE OnPresent(IDXGISwapChain* swap, UINT interval, UINT flags) {
    if (!(flags & DXGI_PRESENT_TEST)) {
        if (InterlockedCompareExchange(&realContextHooked, 1, 0) == 0) {
            ID3D11Device* actualDevice = nullptr;
            if (SUCCEEDED(swap->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&actualDevice)))) {
                ID3D11DeviceContext* actualContext = nullptr;
                actualDevice->GetImmediateContext(&actualContext);
                if (actualContext) {
                    Log(HookContext(actualContext) ? "{\"event\":\"real_immediate_context_observed\"}\r\n" : "{\"event\":\"real_context_hook_failed\"}\r\n");
                    actualContext->Release();
                }
                actualDevice->Release();
            }
        }
        if (initializeDlss && getDlssState && InterlockedCompareExchange(&dlssAttempted, 1, 0) == 0) {
            ID3D11Device* gameDevice = nullptr;
            if (SUCCEEDED(swap->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&gameDevice)))) {
                unsigned result = initializeDlss(gameDevice, dlssCache, dlssProvider);
                TwinKartDlssState state{sizeof(TwinKartDlssState)};
                bool read = getDlssState(&state);
                char line[384];
                sprintf_s(line, "{\"event\":\"sr_real_device_capability\",\"result\":%u,\"state_read\":%s,\"available\":%s,\"evaluations\":%llu,\"final_composition\":false}\r\n",
                    result, read ? "true" : "false", read && state.srAvailable ? "true" : "false", state.successfulEvaluations);
                Log(line); gameDevice->Release();
            } else Log("{\"event\":\"sr_game_device_unavailable\"}\r\n");
        }
        LONG frame = InterlockedIncrement(&frames);
        LARGE_INTEGER now; QueryPerformanceCounter(&now);
        if (frame == 1 || frame % 120 == 0) {
            DXGI_SWAP_CHAIN_DESC desc{}; swap->GetDesc(&desc);
            char line[512];
            double elapsed = lastPresent.QuadPart ? 1000.0*(now.QuadPart-lastPresent.QuadPart)/frequency.QuadPart : 0;
            sprintf_s(line, "{\"event\":\"real_dxgi_present\",\"frame\":%ld,\"width\":%u,\"height\":%u,\"dxgi_format\":%u,\"last_interval_ms\":%.4f,\"dlss_sr_evaluated\":false,\"generated_frames\":0,\"nr_final_composite\":false}\r\n",
                frame, desc.BufferDesc.Width, desc.BufferDesc.Height, desc.BufferDesc.Format, elapsed);
            Log(line);
            ID3D11Texture2D* back = nullptr;
            if (SUCCEEDED(swap->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back)))) {
                Observe(back, 4); back->Release();
            }
        }
        lastPresent = now;
    }
    return present(swap, interval, flags);
}
bool Patch(void** table, unsigned slot, void* replacement, void** original) {
    DWORD old;
    if (!VirtualProtect(table+slot, sizeof(void*), PAGE_READWRITE, &old)) return false;
    void* expected = table[slot];
    *original = expected;
    void* found = InterlockedCompareExchangePointer(reinterpret_cast<PVOID volatile*>(table+slot), replacement, expected);
    DWORD ignored; VirtualProtect(table+slot, sizeof(void*), old, &ignored); return found == expected;
}
DWORD WINAPI Worker(void*) {
    wchar_t path[32768]; DWORD n = GetEnvironmentVariableW(L"TWIN_KART_RENDER_LOG", path, ARRAYSIZE(path));
    if (!n || n >= ARRAYSIZE(path) || !Allowed()) return 1;
    output = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, CREATE_NEW, 0, nullptr);
    if (output == INVALID_HANDLE_VALUE) return 2;
    QueryPerformanceFrequency(&frequency);
    wchar_t modulePath[32768]{};
    GetEnvironmentVariableW(L"TWIN_KART_DLSS_MODULE", modulePath, ARRAYSIZE(modulePath));
    GetEnvironmentVariableW(L"TWIN_KART_DLSS_CACHE", dlssCache, ARRAYSIZE(dlssCache));
    GetEnvironmentVariableW(L"TWIN_KART_DLSS_PROVIDER", dlssProvider, ARRAYSIZE(dlssProvider));
    if (modulePath[0] && dlssCache[0] && dlssProvider[0]) {
        HMODULE module = LoadLibraryExW(modulePath, nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (module) {
            initializeDlss = reinterpret_cast<DlssInitialize>(GetProcAddress(module, "TwinKartDlssInitialize"));
            getDlssState = reinterpret_cast<DlssState>(GetProcAddress(module, "TwinKartDlssGetState"));
        }
        Log(initializeDlss && getDlssState ? "{\"event\":\"original_sr_consumer_loaded\",\"evaluated\":false}\r\n" : "{\"event\":\"original_sr_consumer_load_failed\"}\r\n");
    }
    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW wc{}; wc.lpfnWndProc = DefWindowProcW; wc.hInstance = instance; wc.lpszClassName = L"TwinKartRenderObserver";
    RegisterClassW(&wc);
    HWND window = CreateWindowExW(0, wc.lpszClassName, L"", WS_POPUP, 0, 0, 64, 64, nullptr, nullptr, instance, nullptr);
    DXGI_SWAP_CHAIN_DESC desc{}; desc.BufferDesc.Width=64; desc.BufferDesc.Height=64;
    desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count=1; desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount=1; desc.OutputWindow=window; desc.Windowed=TRUE; desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    ID3D11Device* device=nullptr; ID3D11DeviceContext* context=nullptr; IDXGISwapChain* swap=nullptr;
    HRESULT hr = window ? D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &desc, &swap, &device, nullptr, &context) : E_FAIL;
    if (SUCCEEDED(hr)) {
        void** scTable=*reinterpret_cast<void***>(swap);
        bool a=Patch(scTable,8,reinterpret_cast<void*>(OnPresent),reinterpret_cast<void**>(&present));
        bool b=HookContext(context);
        Log(a && b ? "{\"event\":\"observer_installed\",\"is_dlss_implementation\":false}\r\n" : "{\"event\":\"observer_partial_install\"}\r\n");
    } else Log("{\"event\":\"observer_device_failed\"}\r\n");
    if (context) context->Release(); if (device) device->Release(); if (swap) swap->Release();
    if (window) DestroyWindow(window);
    UnregisterClassW(wc.lpszClassName, instance); return 0;
}
}
BOOL WINAPI DllMain(HINSTANCE self, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(self);
        HANDLE worker = CreateThread(nullptr,0,Worker,nullptr,0,nullptr); if (worker) CloseHandle(worker);
    }
    return TRUE;
}
