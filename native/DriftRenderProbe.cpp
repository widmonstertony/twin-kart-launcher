#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <d3d11_4.h>
#include <dxgi.h>
#include <bcrypt.h>
#include <cstdio>
#include <cstring>
#include <intrin.h>
#include <atomic>
#include "DlssD3D11.h"
#include "RenderPassTrace.h"
#include "TemporalCapture.h"
#include "NativeNeuralPass.h"
#include "NativeTemporalPass.h"
#include "NativePresentation.h"
#include "NativeSceneRectangle.h"

// First-party resource observer with an opt-in, exact-build NR adapter.
// SR and generated-frame presentation remain separate unfinished integrations.
// A resource format alone never establishes a valid depth/motion guide.
namespace {
HANDLE output = INVALID_HANDLE_VALUE;
SRWLOCK logLock = SRWLOCK_INIT;
using Present = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
using Targets = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, ID3D11RenderTargetView* const*, ID3D11DepthStencilView*);
using Resources = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT, ID3D11ShaderResourceView* const*);
using Unordered = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT, ID3D11UnorderedAccessView* const*, const UINT*);
using Draw = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT);
using DrawIndexed = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT, INT);
using DrawInstances = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT, UINT, UINT);
using DrawIndexedInstances = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT, UINT, INT, UINT);
using Dispatch = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT, UINT);
using Indirect = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11Buffer*, UINT);
using DrawAuto=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*);
using CopyRegion=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Resource*,UINT,UINT,UINT,UINT,ID3D11Resource*,UINT,const D3D11_BOX*);
using Copy=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Resource*,ID3D11Resource*);
using Update=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Resource*,UINT,const D3D11_BOX*,const void*,UINT,UINT);
using Resolve=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Resource*,UINT,ID3D11Resource*,UINT,DXGI_FORMAT);
using Execute=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11CommandList*,BOOL);
using ClearTarget=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11RenderTargetView*,const FLOAT*);
using ClearUint=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11UnorderedAccessView*,const UINT*);
using ClearFloat=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11UnorderedAccessView*,const FLOAT*);
using CopyRegion1=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Resource*,UINT,UINT,UINT,UINT,ID3D11Resource*,UINT,const D3D11_BOX*,UINT);
using Update1=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Resource*,UINT,const D3D11_BOX*,const void*,UINT,UINT,UINT);
using DiscardResource=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Resource*);
using DiscardView=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11View*);
using ClearView=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11View*,const FLOAT*,const D3D11_RECT*,UINT);
using DiscardView1=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11View*,const D3D11_RECT*,UINT);
using CreatePixel = HRESULT(STDMETHODCALLTYPE*)(ID3D11Device*, const void*, SIZE_T, ID3D11ClassLinkage*, ID3D11PixelShader**);
using CreateCompute = HRESULT(STDMETHODCALLTYPE*)(ID3D11Device*, const void*, SIZE_T, ID3D11ClassLinkage*, ID3D11ComputeShader**);
Present present = nullptr;
struct ContextHooks {
    void** table; ID3D11DeviceContext* owner; void* cloned[149];
    Targets targets; Resources pixel, compute; Unordered unordered;
    Draw draw; DrawIndexed indexed; DrawInstances instances; DrawIndexedInstances indexedInstances;
    Dispatch dispatch; Indirect indexedIndirect, indirect, dispatchIndirect;
    DrawAuto automatic;CopyRegion copyRegion;Copy copy;Update update;Resolve resolve;Execute execute;
    ClearTarget clearTarget;ClearUint clearUint;ClearFloat clearFloat;
    CopyRegion1 copyRegion1;Update1 update1;DiscardResource discardResource;DiscardView discardView;ClearView clearView;DiscardView1 discardView1;
};
struct DeviceHooks { void** table; CreatePixel pixel; CreateCompute compute; };
DeviceHooks devices[8]{};
unsigned deviceCount = 0;
SRWLOCK deviceLock = SRWLOCK_INIT;
std::atomic<ID3D11DeviceContext*> gameContext{nullptr}; // retained for this owned game process
bool tracePasses = false;
volatile LONG passRows = 0, sampledRows = 0;
unsigned traceInterval=120,traceLimit=1024,tracePerFrame=64;
ContextHooks contexts[8]{};
unsigned contextCount = 0;
SRWLOCK contextLock = SRWLOCK_INIT;
volatile LONG realContextHooked = 0;
volatile LONG frames = 0;
volatile LONG nrLastSubmitted=-1;
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
bool HookDevice(ID3D11Device*);
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
void TracePass(ID3D11DeviceContext* context, bool compute, void* caller) {
    if (!tracePasses || context != gameContext.load(std::memory_order_acquire) || frames < 1 || frames % traceInterval != 0 || passRows >= static_cast<LONG>(traceLimit) || sampledRows >= static_cast<LONG>(tracePerFrame)) return;
    if (InterlockedIncrement(&sampledRows) > static_cast<LONG>(tracePerFrame)) return;
    uintptr_t address = reinterpret_cast<uintptr_t>(caller), base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    uintptr_t rva = address >= base && address - base < 0x8657000 ? address - base : 0;
    char line[32768];
    if (TwinKartRender::DescribePass(context, compute, static_cast<unsigned>(frames), rva, line, sizeof(line))) {
        if (InterlockedIncrement(&passRows) <= static_cast<LONG>(traceLimit)) Log(line);
    }
}
void RecordSceneDraw(ID3D11DeviceContext* context) {
    if(context==gameContext.load(std::memory_order_acquire))TwinKartGraphics::RecordNativeSceneDraw(context,static_cast<unsigned>(frames));
}
// Every resource-write API that can alter the supported default, non-tiled,
// one-mip color textures invalidates producer geometry. Command-list writes
// have no observable per-resource boundaries here, so invalidate the epoch.
#define TK_NATIVE_WRITE(name,field,signature,arguments,invalidate) \
void STDMETHODCALLTYPE name signature {invalidate;auto original=Hooks(context).field;if(original)original arguments;}
TK_NATIVE_WRITE(OnCopyRegion,copyRegion,(ID3D11DeviceContext* context,ID3D11Resource* dst,UINT ds,UINT x,UINT y,UINT z,ID3D11Resource* src,UINT ss,const D3D11_BOX* box),
    (context,dst,ds,x,y,z,src,ss,box),TwinKartGraphics::InvalidateNativeScene(dst))
TK_NATIVE_WRITE(OnCopy,copy,(ID3D11DeviceContext* context,ID3D11Resource* dst,ID3D11Resource* src),(context,dst,src),TwinKartGraphics::InvalidateNativeScene(dst))
TK_NATIVE_WRITE(OnUpdate,update,(ID3D11DeviceContext* context,ID3D11Resource* dst,UINT sub,const D3D11_BOX* box,const void* data,UINT row,UINT depth),
    (context,dst,sub,box,data,row,depth),TwinKartGraphics::InvalidateNativeScene(dst))
TK_NATIVE_WRITE(OnResolve,resolve,(ID3D11DeviceContext* context,ID3D11Resource* dst,UINT ds,ID3D11Resource* src,UINT ss,DXGI_FORMAT format),
    (context,dst,ds,src,ss,format),TwinKartGraphics::InvalidateNativeScene(dst))
TK_NATIVE_WRITE(OnExecute,execute,(ID3D11DeviceContext* context,ID3D11CommandList* list,BOOL restore),(context,list,restore),TwinKartGraphics::InvalidateNativeSceneCommandList())
TK_NATIVE_WRITE(OnClearTarget,clearTarget,(ID3D11DeviceContext* context,ID3D11RenderTargetView* view,const FLOAT* color),(context,view,color),TwinKartGraphics::InvalidateNativeSceneView(view))
TK_NATIVE_WRITE(OnClearUint,clearUint,(ID3D11DeviceContext* context,ID3D11UnorderedAccessView* view,const UINT* color),(context,view,color),TwinKartGraphics::InvalidateNativeSceneView(view))
TK_NATIVE_WRITE(OnClearFloat,clearFloat,(ID3D11DeviceContext* context,ID3D11UnorderedAccessView* view,const FLOAT* color),(context,view,color),TwinKartGraphics::InvalidateNativeSceneView(view))
TK_NATIVE_WRITE(OnCopyRegion1,copyRegion1,(ID3D11DeviceContext* context,ID3D11Resource* dst,UINT ds,UINT x,UINT y,UINT z,ID3D11Resource* src,UINT ss,const D3D11_BOX* box,UINT flags),
    (context,dst,ds,x,y,z,src,ss,box,flags),TwinKartGraphics::InvalidateNativeScene(dst))
TK_NATIVE_WRITE(OnUpdate1,update1,(ID3D11DeviceContext* context,ID3D11Resource* dst,UINT sub,const D3D11_BOX* box,const void* data,UINT row,UINT depth,UINT flags),
    (context,dst,sub,box,data,row,depth,flags),TwinKartGraphics::InvalidateNativeScene(dst))
TK_NATIVE_WRITE(OnDiscardResource,discardResource,(ID3D11DeviceContext* context,ID3D11Resource* resource),(context,resource),TwinKartGraphics::InvalidateNativeScene(resource))
TK_NATIVE_WRITE(OnDiscardView,discardView,(ID3D11DeviceContext* context,ID3D11View* view),(context,view),TwinKartGraphics::InvalidateNativeSceneView(view))
TK_NATIVE_WRITE(OnClearView,clearView,(ID3D11DeviceContext* context,ID3D11View* view,const FLOAT* color,const D3D11_RECT* rects,UINT count),
    (context,view,color,rects,count),TwinKartGraphics::InvalidateNativeSceneView(view))
TK_NATIVE_WRITE(OnDiscardView1,discardView1,(ID3D11DeviceContext* context,ID3D11View* view,const D3D11_RECT* rects,UINT count),
    (context,view,rects,count),TwinKartGraphics::InvalidateNativeSceneView(view))
#undef TK_NATIVE_WRITE
void STDMETHODCALLTYPE OnAutomatic(ID3D11DeviceContext* context) {
    auto original=Hooks(context).automatic;if(original)original(context);RecordSceneDraw(context);
}
void STDMETHODCALLTYPE OnDraw(ID3D11DeviceContext* context, UINT a, UINT b) {
    if (a <= 6) TracePass(context, false, _ReturnAddress()); auto original = Hooks(context).draw; if (original) original(context,a,b);
    RecordSceneDraw(context);
    if(a<=6 && context==gameContext.load(std::memory_order_acquire) && TwinKartGraphics::ProcessNativeNeuralPixelPass(context,static_cast<unsigned>(frames))) InterlockedExchange(&nrLastSubmitted,frames);
}
void STDMETHODCALLTYPE OnIndexed(ID3D11DeviceContext* context, UINT a, UINT b, INT c) {
    if (a <= 6) TracePass(context, false, _ReturnAddress()); auto original = Hooks(context).indexed; if (original) original(context,a,b,c);
    RecordSceneDraw(context);
    if(a<=6 && context==gameContext.load(std::memory_order_acquire) && TwinKartGraphics::ProcessNativeNeuralPixelPass(context,static_cast<unsigned>(frames))) InterlockedExchange(&nrLastSubmitted,frames);
}
void STDMETHODCALLTYPE OnInstances(ID3D11DeviceContext* context, UINT a, UINT b, UINT c, UINT d) {
    if (a <= 6) TracePass(context, false, _ReturnAddress()); auto original = Hooks(context).instances; if (original) original(context,a,b,c,d);
    RecordSceneDraw(context);
    if(a<=6 && context==gameContext.load(std::memory_order_acquire) && TwinKartGraphics::ProcessNativeNeuralPixelPass(context,static_cast<unsigned>(frames))) InterlockedExchange(&nrLastSubmitted,frames);
}
void STDMETHODCALLTYPE OnIndexedInstances(ID3D11DeviceContext* context, UINT a, UINT b, UINT c, INT d, UINT e) {
    if (a <= 6) TracePass(context, false, _ReturnAddress()); auto original = Hooks(context).indexedInstances; if (original) original(context,a,b,c,d,e);
    RecordSceneDraw(context);
    if(a<=6 && context==gameContext.load(std::memory_order_acquire) && TwinKartGraphics::ProcessNativeNeuralPixelPass(context,static_cast<unsigned>(frames))) InterlockedExchange(&nrLastSubmitted,frames);
}
void STDMETHODCALLTYPE OnDispatch(ID3D11DeviceContext* context, UINT a, UINT b, UINT c) {
    if(context==gameContext.load(std::memory_order_acquire)) TwinKartGraphics::CaptureTemporalInputs(context,static_cast<unsigned>(frames));
    TracePass(context, true, _ReturnAddress());
    // Freeze the exact native dispatch inputs before another consumer or the
    // original dispatch can alter its bindings. Private capture uses this same
    // boundary; the pre-HUD pass must consume that view's immutable constants.
    if(context==gameContext.load(std::memory_order_acquire)) TwinKartGraphics::ProcessNativeNeuralPass(context,static_cast<unsigned>(frames));
    bool replaced=context==gameContext.load(std::memory_order_acquire) && TwinKartGraphics::ProcessNativeTemporalPass(context,static_cast<unsigned>(frames),a,b,c);
    TwinKartGraphics::InvalidateNativeComputeTargets(context);
    auto original = Hooks(context).dispatch; if (original && !replaced) original(context,a,b,c);
}
void STDMETHODCALLTYPE OnIndexedIndirect(ID3D11DeviceContext* context, ID3D11Buffer* args, UINT offset) {
    TracePass(context, false, _ReturnAddress()); auto original = Hooks(context).indexedIndirect; if (original) original(context,args,offset);
    RecordSceneDraw(context);
}
void STDMETHODCALLTYPE OnIndirect(ID3D11DeviceContext* context, ID3D11Buffer* args, UINT offset) {
    TracePass(context, false, _ReturnAddress()); auto original = Hooks(context).indirect; if (original) original(context,args,offset);
    RecordSceneDraw(context);
}
void STDMETHODCALLTYPE OnDispatchIndirect(ID3D11DeviceContext* context, ID3D11Buffer* args, UINT offset) {
    if(context==gameContext.load(std::memory_order_acquire)) TwinKartGraphics::CaptureTemporalInputs(context,static_cast<unsigned>(frames));
    if(context==gameContext.load(std::memory_order_acquire)) TwinKartGraphics::ProcessNativeNeuralPass(context,static_cast<unsigned>(frames));
    TwinKartGraphics::InvalidateNativeComputeTargets(context);
    TracePass(context, true, _ReturnAddress()); auto original = Hooks(context).dispatchIndirect; if (original) original(context,args,offset);
}
DeviceHooks DeviceFunctions(ID3D11Device* device) {
    DeviceHooks result{}; void** table = *reinterpret_cast<void***>(device);
    AcquireSRWLockShared(&deviceLock);
    for (unsigned i=0; i<deviceCount; ++i) if (devices[i].table == table) { result = devices[i]; break; }
    ReleaseSRWLockShared(&deviceLock); return result;
}
HRESULT STDMETHODCALLTYPE OnCreatePixel(ID3D11Device* device, const void* bytes, SIZE_T size, ID3D11ClassLinkage* linkage, ID3D11PixelShader** shader) {
    auto original = DeviceFunctions(device).pixel;
    if (!original) return E_FAIL;
    HRESULT result = original(device,bytes,size,linkage,shader);
    if (SUCCEEDED(result) && shader && *shader) TwinKartRender::TagShader(*shader,bytes,size);
    return result;
}
HRESULT STDMETHODCALLTYPE OnCreateCompute(ID3D11Device* device, const void* bytes, SIZE_T size, ID3D11ClassLinkage* linkage, ID3D11ComputeShader** shader) {
    auto original = DeviceFunctions(device).compute;
    if (!original) return E_FAIL;
    HRESULT result = original(device,bytes,size,linkage,shader);
    if (SUCCEEDED(result) && shader && *shader) TwinKartRender::TagShader(*shader,bytes,size);
    return result;
}
bool HookDevice(ID3D11Device* device) {
    void** table = *reinterpret_cast<void***>(device);
    AcquireSRWLockExclusive(&deviceLock);
    for (unsigned i=0; i<deviceCount; ++i) if (devices[i].table == table) { ReleaseSRWLockExclusive(&deviceLock); return true; }
    if (deviceCount == ARRAYSIZE(devices)) { ReleaseSRWLockExclusive(&deviceLock); return false; }
    auto& hooks = devices[deviceCount++]; hooks.table = table;
    bool a=Patch(table,15,reinterpret_cast<void*>(OnCreatePixel),reinterpret_cast<void**>(&hooks.pixel));
    bool b=Patch(table,18,reinterpret_cast<void*>(OnCreateCompute),reinterpret_cast<void**>(&hooks.compute));
    ReleaseSRWLockExclusive(&deviceLock); return a && b;
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
    void** source = *reinterpret_cast<void***>(context);
    // The runtime can refresh its shared dispatch table after lazy device
    // setup. Use a complete instance table, preserving all supported methods.
    unsigned entries = 115;
    const struct { const IID* iid; unsigned entries; } versions[] = {
        {&__uuidof(ID3D11DeviceContext1),134}, {&__uuidof(ID3D11DeviceContext2),144},
        {&__uuidof(ID3D11DeviceContext3),147}, {&__uuidof(ID3D11DeviceContext4),149}
    };
    for (const auto& version : versions) {
        IUnknown* extra = nullptr;
        if (SUCCEEDED(context->QueryInterface(*version.iid,reinterpret_cast<void**>(&extra)))) {
            if (extra == static_cast<IUnknown*>(context)) entries = version.entries;
            extra->Release();
        }
    }
    AcquireSRWLockExclusive(&contextLock);
    for (unsigned i=0; i<contextCount; ++i) if (contexts[i].owner == context) {
        bool installed = contexts[i].table == source;
        ReleaseSRWLockExclusive(&contextLock); return installed;
    }
    if (contextCount == ARRAYSIZE(contexts)) { ReleaseSRWLockExclusive(&contextLock); return false; }
    auto& hooks = contexts[contextCount++]; hooks.owner = context; hooks.table = hooks.cloned;
    memcpy(hooks.cloned,source,entries*sizeof(void*));
    void** table = hooks.table;
    // Keep each wrapper's original functions with its own vtable. The game's
    // context can differ from the temporary discovery device's context.
    bool a=Patch(table,33,reinterpret_cast<void*>(SetTargets),reinterpret_cast<void**>(&hooks.targets));
    bool b=Patch(table,8,reinterpret_cast<void*>(SetResources),reinterpret_cast<void**>(&hooks.pixel));
    bool c=Patch(table,67,reinterpret_cast<void*>(SetComputeResources),reinterpret_cast<void**>(&hooks.compute));
    bool d=Patch(table,68,reinterpret_cast<void*>(SetUnordered),reinterpret_cast<void**>(&hooks.unordered));
    bool e=Patch(table,12,reinterpret_cast<void*>(OnIndexed),reinterpret_cast<void**>(&hooks.indexed));
    bool f=Patch(table,13,reinterpret_cast<void*>(OnDraw),reinterpret_cast<void**>(&hooks.draw));
    bool g=Patch(table,20,reinterpret_cast<void*>(OnIndexedInstances),reinterpret_cast<void**>(&hooks.indexedInstances));
    bool h=Patch(table,21,reinterpret_cast<void*>(OnInstances),reinterpret_cast<void**>(&hooks.instances));
    bool i=Patch(table,39,reinterpret_cast<void*>(OnIndexedIndirect),reinterpret_cast<void**>(&hooks.indexedIndirect));
    bool j=Patch(table,40,reinterpret_cast<void*>(OnIndirect),reinterpret_cast<void**>(&hooks.indirect));
    bool k=Patch(table,41,reinterpret_cast<void*>(OnDispatch),reinterpret_cast<void**>(&hooks.dispatch));
    bool l=Patch(table,42,reinterpret_cast<void*>(OnDispatchIndirect),reinterpret_cast<void**>(&hooks.dispatchIndirect));
    bool writes=true;
#define TK_NATIVE_WRITE_SLOT(slot,name,field) writes=Patch(table,slot,reinterpret_cast<void*>(name),reinterpret_cast<void**>(&hooks.field)) && writes
    TK_NATIVE_WRITE_SLOT(38,OnAutomatic,automatic);
    TK_NATIVE_WRITE_SLOT(46,OnCopyRegion,copyRegion);TK_NATIVE_WRITE_SLOT(47,OnCopy,copy);TK_NATIVE_WRITE_SLOT(48,OnUpdate,update);
    TK_NATIVE_WRITE_SLOT(50,OnClearTarget,clearTarget);TK_NATIVE_WRITE_SLOT(51,OnClearUint,clearUint);TK_NATIVE_WRITE_SLOT(52,OnClearFloat,clearFloat);
    TK_NATIVE_WRITE_SLOT(57,OnResolve,resolve);TK_NATIVE_WRITE_SLOT(58,OnExecute,execute);
    if(entries>=134) {
        TK_NATIVE_WRITE_SLOT(115,OnCopyRegion1,copyRegion1);TK_NATIVE_WRITE_SLOT(116,OnUpdate1,update1);
        TK_NATIVE_WRITE_SLOT(117,OnDiscardResource,discardResource);TK_NATIVE_WRITE_SLOT(118,OnDiscardView,discardView);
        TK_NATIVE_WRITE_SLOT(132,OnClearView,clearView);TK_NATIVE_WRITE_SLOT(133,OnDiscardView1,discardView1);
    }
#undef TK_NATIVE_WRITE_SLOT
    bool installed = false;
    if (a && b && c && d && e && f && g && h && i && j && k && l && writes) {
        DWORD previous;
        if (VirtualProtect(context,sizeof(void*),PAGE_READWRITE,&previous)) {
            installed = InterlockedCompareExchangePointer(reinterpret_cast<PVOID volatile*>(context),table,source) == source;
            DWORD ignored; VirtualProtect(context,sizeof(void*),previous,&ignored);
        }
    }
    if (installed) context->AddRef(); // adapter remains resident until owned process exit
    else { --contextCount; hooks = ContextHooks{}; }
    ReleaseSRWLockExclusive(&contextLock);
    return installed;
}
HRESULT STDMETHODCALLTYPE OnPresent(IDXGISwapChain* swap, UINT interval, UINT flags) {
    // Streamline's private D3D12 chain can reach the shared native DXGI vtable.
    // Its nested Present must not increment the D3D11 frame or run hooks again.
    if(TwinKartGraphics::InsideNativePresentation())return present(swap,interval,flags);
    // NVIDIA may present generated frames on its own thread, where our
    // caller's thread-local recursion flag is absent. The shared DXGI vtable
    // also sees that private D3D12 chain. Only the actual D3D11 renderer may
    // advance native frame/history state or retire its current-frame guides.
    ID3D11Device* rendererDevice=nullptr;
    if(FAILED(swap->GetDevice(__uuidof(ID3D11Device),reinterpret_cast<void**>(&rendererDevice))))
        return present(swap,interval,flags);
    rendererDevice->Release();
    bool nrBackbuffer=false,srBackbuffer=false;LONG nrRenderFrame=frames;
    if(!(flags&DXGI_PRESENT_TEST) && nrLastSubmitted==nrRenderFrame) {
        ID3D11Texture2D* texture=nullptr;
        if(SUCCEEDED(swap->GetBuffer(TwinKartGraphics::NativePresentationBufferIndex(swap,static_cast<unsigned>(nrRenderFrame)),__uuidof(ID3D11Texture2D),reinterpret_cast<void**>(&texture)))) {
            nrBackbuffer=TwinKartGraphics::NativeNeuralOutputMatches(texture,static_cast<unsigned>(nrRenderFrame));
            srBackbuffer=TwinKartGraphics::NativeSuperResolutionOutputMatches(texture,static_cast<unsigned>(nrRenderFrame));texture->Release();
        }
    }
    if (!(flags & DXGI_PRESENT_TEST)) {
        if (InterlockedCompareExchange(&realContextHooked, 1, 0) == 0) {
            DXGI_SWAP_CHAIN_DESC actual{};
            if(SUCCEEDED(swap->GetDesc(&actual))) {
                char row[512];sprintf_s(row,"{\"event\":\"real_swapchain_contract\",\"width\":%u,\"height\":%u,\"format\":%u,\"buffers\":%u,\"effect\":%u,\"flags\":%u,\"samples\":%u,\"quality\":%u,\"windowed\":%s,\"factory_hook_mask\":%u,\"game_acceptance\":false}\r\n",
                    actual.BufferDesc.Width,actual.BufferDesc.Height,actual.BufferDesc.Format,actual.BufferCount,actual.SwapEffect,actual.Flags,actual.SampleDesc.Count,actual.SampleDesc.Quality,actual.Windowed?"true":"false",TwinKartGraphics::NativePresentationFactoryHookMask(swap));Log(row);
            }
            ID3D11Device* actualDevice = nullptr;
            if (SUCCEEDED(swap->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&actualDevice)))) {
                ID3D11DeviceContext* actualContext = nullptr;
                actualDevice->GetImmediateContext(&actualContext);
                if (actualContext) {
                    actualContext->AddRef(); gameContext.store(actualContext,std::memory_order_release);
                    HookDevice(actualDevice);
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
        InterlockedExchange(&sampledRows, 0);
        LARGE_INTEGER now; QueryPerformanceCounter(&now);
        if (frame == 1 || frame % 120 == 0) {
            DXGI_SWAP_CHAIN_DESC desc{}; swap->GetDesc(&desc);
            char line[512];
            double elapsed = lastPresent.QuadPart ? 1000.0*(now.QuadPart-lastPresent.QuadPart)/frequency.QuadPart : 0;
            sprintf_s(line, "{\"event\":\"real_dxgi_present\",\"frame\":%ld,\"width\":%u,\"height\":%u,\"dxgi_format\":%u,\"last_interval_ms\":%.4f,\"dlss_sr_evaluated\":%s,\"generated_frames\":null,\"nr_pre_hud_copy_render_frame\":%ld,\"nr_final_composite\":false}\r\n",
                frame, desc.BufferDesc.Width, desc.BufferDesc.Height, desc.BufferDesc.Format, elapsed,srBackbuffer?"true":"false",nrLastSubmitted);
            Log(line);
            ID3D11Texture2D* back = nullptr;
            if (SUCCEEDED(swap->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back)))) {
                Observe(back, 4); back->Release();
            }
        }
        lastPresent = now;
    }
    bool bridgedPresentation=TwinKartGraphics::IsNativePresentation(swap);
    if(!bridgedPresentation && !(flags&DXGI_PRESENT_TEST)) {
        // NR/SR without FG still has a real native Present/context boundary.
        // Generated D3D12 worker chains were excluded above; the NR adapter
        // verifies this exact immediate context before retiring any owner.
        ID3D11Device* device=nullptr;
        if(SUCCEEDED(swap->GetDevice(__uuidof(ID3D11Device),reinterpret_cast<void**>(&device)))) {
            ID3D11DeviceContext* context=nullptr;device->GetImmediateContext(&context);device->Release();
            if(context){TwinKartGraphics::RetireNativeTemporalForRenderThread(context);TwinKartGraphics::RetireNativeNeuralForRenderThread(context);context->Release();}
        }
    }
    HRESULT presented=bridgedPresentation?TwinKartGraphics::PresentNativePresentation(swap,interval,flags,static_cast<unsigned>(nrRenderFrame)):present(swap,interval,flags);
    if(FAILED(presented)) {
        char row[192];sprintf_s(row,"{\"event\":\"native_present_failed\",\"frame\":%ld,\"thread\":%lu,\"hresult\":%ld}\r\n",nrRenderFrame,GetCurrentThreadId(),presented);Log(row);
    }
    if(srBackbuffer && (nrRenderFrame%120==0 || FAILED(presented))) {
        char line[320];sprintf_s(line,"{\"event\":\"sr_native_backbuffer_present\",\"render_frame\":%ld,\"present_hresult\":%ld,\"same_output_resource\":true,\"quality_verified\":false}\r\n",nrRenderFrame,presented);Log(line);
    }
    if(!(flags&DXGI_PRESENT_TEST)) TwinKartGraphics::FinishNativeNeuralFrame(static_cast<unsigned>(nrRenderFrame));
    if(nrBackbuffer && (nrRenderFrame%120==0 || FAILED(presented))) {
        char line[320];sprintf_s(line,"{\"event\":\"nr_native_backbuffer_present\",\"render_frame\":%ld,\"present_hresult\":%ld,\"same_output_resource\":true,\"quality_verified\":false}\r\n",nrRenderFrame,presented);Log(line);
    }
    return presented;
}
bool Patch(void** table, unsigned slot, void* replacement, void** original) {
    DWORD old;
    if (!VirtualProtect(table+slot, sizeof(void*), PAGE_READWRITE, &old)) return false;
    void* expected = table[slot];
    *original = expected;
    void* found = InterlockedCompareExchangePointer(reinterpret_cast<PVOID volatile*>(table+slot), replacement, expected);
    DWORD ignored; VirtualProtect(table+slot, sizeof(void*), old, &ignored); return found == expected;
}
unsigned EnvironmentNumber(const wchar_t* name,unsigned fallback,unsigned minimum,unsigned maximum) {
    wchar_t text[16]{}; DWORD length=GetEnvironmentVariableW(name,text,ARRAYSIZE(text));
    if(!length || length>=ARRAYSIZE(text)) return fallback;
    wchar_t* end=nullptr; unsigned long value=wcstoul(text,&end,10);
    return end && !*end && value>=minimum && value<=maximum?static_cast<unsigned>(value):fallback;
}
DWORD WINAPI Worker(void*) {
    wchar_t path[32768]; DWORD n = GetEnvironmentVariableW(L"TWIN_KART_RENDER_LOG", path, ARRAYSIZE(path));
    if (!n || n >= ARRAYSIZE(path) || !Allowed()) return 1;
    output = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, CREATE_NEW, 0, nullptr);
    if (output == INVALID_HANDLE_VALUE) return 2;
    QueryPerformanceFrequency(&frequency);
    bool presentationReady=TwinKartGraphics::ConfigureNativePresentation(OnPresent,Log);
    wchar_t fgRequested[8]{};GetEnvironmentVariableW(L"TWIN_KART_FRAME_GENERATION",fgRequested,ARRAYSIZE(fgRequested));
    bool neuralReady=false,temporalReady=false;
    wchar_t passFlag[8]{};
    GetEnvironmentVariableW(L"TWIN_KART_RENDER_PASSES", passFlag, ARRAYSIZE(passFlag));
    tracePasses = passFlag[0] == L'1';
    traceInterval=EnvironmentNumber(L"TWIN_KART_RENDER_TRACE_INTERVAL",120,30,3600);
    traceLimit=EnvironmentNumber(L"TWIN_KART_RENDER_TRACE_LIMIT",1024,64,16384);
    tracePerFrame=EnvironmentNumber(L"TWIN_KART_RENDER_TRACE_PER_FRAME",64,8,256);
    wchar_t capturePath[32768]{},captureHash[65]{}; char selectedHash[65]{};
    DWORD captureLength=GetEnvironmentVariableW(L"TWIN_KART_TEMPORAL_CAPTURE",capturePath,ARRAYSIZE(capturePath));
    DWORD hashLength=GetEnvironmentVariableW(L"TWIN_KART_TEMPORAL_SHADER",captureHash,ARRAYSIZE(captureHash));
    if(captureLength && captureLength<ARRAYSIZE(capturePath) && hashLength==64) {
        for(unsigned i=0;i<64;++i) selectedHash[i]=captureHash[i]<128?static_cast<char>(captureHash[i]):'!';
        Log(TwinKartGraphics::ConfigureTemporalCapture(capturePath,selectedHash,2,true)?"{\"event\":\"temporal_capture_waiting_for_request\",\"guides_validated\":false}\r\n":"{\"event\":\"temporal_capture_rejected\"}\r\n");
    }
    wchar_t neuralModule[32768]{},neuralProvider[32768]{},neuralCache[32768]{};
    DWORD neuralLength=GetEnvironmentVariableW(L"TWIN_KART_NEURAL_MODULE",neuralModule,ARRAYSIZE(neuralModule));
    DWORD providerLength=GetEnvironmentVariableW(L"TWIN_KART_NEURAL_PROVIDER",neuralProvider,ARRAYSIZE(neuralProvider));
    DWORD cacheLength=GetEnvironmentVariableW(L"TWIN_KART_DLSS_CACHE",neuralCache,ARRAYSIZE(neuralCache));
    if(neuralLength && neuralLength<ARRAYSIZE(neuralModule) && providerLength && providerLength<ARRAYSIZE(neuralProvider) && cacheLength && cacheLength<ARRAYSIZE(neuralCache)) {
        wchar_t expected[65]{};char sha[65]{};
        DWORD neuralHashLength=GetEnvironmentVariableW(L"TWIN_KART_NEURAL_MODULE_SHA256",expected,ARRAYSIZE(expected));
        if(neuralHashLength==64)for(unsigned i=0;i<64;++i)sha[i]=expected[i]<128?static_cast<char>(expected[i]):'!';
        neuralReady=TwinKartGraphics::ConfigureNativeNeuralPass(neuralModule,sha,neuralProvider,neuralCache,Log,TwinKartGraphics::NativePresentationOwnsNgxDevice);
        Log(neuralReady?"{\"event\":\"nr_native_pass_configured\",\"game_acceptance\":false}\r\n":"{\"event\":\"nr_native_pass_configuration_rejected\"}\r\n");
    }
    wchar_t modulePath[32768]{};
    GetEnvironmentVariableW(L"TWIN_KART_DLSS_MODULE", modulePath, ARRAYSIZE(modulePath));
    GetEnvironmentVariableW(L"TWIN_KART_DLSS_CACHE", dlssCache, ARRAYSIZE(dlssCache));
    GetEnvironmentVariableW(L"TWIN_KART_DLSS_PROVIDER", dlssProvider, ARRAYSIZE(dlssProvider));
    wchar_t srFlag[8]{};GetEnvironmentVariableW(L"TWIN_KART_SUPER_RESOLUTION",srFlag,ARRAYSIZE(srFlag));
    if(!wcscmp(srFlag,L"1")) {
        wchar_t moduleSha[65]{};char sha[65]{};
        DWORD length=GetEnvironmentVariableW(L"TWIN_KART_DLSS_MODULE_SHA256",moduleSha,ARRAYSIZE(moduleSha));
        if(length==64)for(unsigned i=0;i<64;++i)sha[i]=moduleSha[i]<128?static_cast<char>(moduleSha[i]):'!';
        temporalReady=TwinKartGraphics::ConfigureNativeTemporalPass(modulePath,sha,dlssProvider,dlssCache,Log);
        Log(temporalReady?
            "{\"event\":\"sr_native_temporal_pass_configured\",\"game_acceptance\":false}\r\n":
            "{\"event\":\"sr_native_temporal_pass_rejected\"}\r\n");
    } else if (modulePath[0] && dlssCache[0] && dlssProvider[0]) {
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
        bool c=HookDevice(device);
        Log(a && b && c ? "{\"event\":\"observer_installed\",\"is_dlss_implementation\":false}\r\n" : "{\"event\":\"observer_partial_install\"}\r\n");
        bool requestedReady=(wcscmp(fgRequested,L"1") || presentationReady) &&
            (!neuralLength || neuralReady) && (wcscmp(srFlag,L"1") || temporalReady);
        if(a && b && c && requestedReady) {
            wchar_t readyName[128]{};
            DWORD readyLength=GetEnvironmentVariableW(L"TWIN_KART_RENDER_READY_EVENT",readyName,ARRAYSIZE(readyName));
            constexpr wchar_t prefix[]=L"Local\\TwinKartRenderReady-";
            if(readyLength && readyLength<ARRAYSIZE(readyName) && !wcsncmp(readyName,prefix,ARRAYSIZE(prefix)-1)) {
                HANDLE ready=OpenEventW(EVENT_MODIFY_STATE,FALSE,readyName);
                if(ready){SetEvent(ready);CloseHandle(ready);}
            }
        }
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
