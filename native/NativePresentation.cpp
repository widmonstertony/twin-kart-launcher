#define WIN32_LEAN_AND_MEAN
#include "NativePresentation.h"
#ifndef TWIN_KART_FRAME_PRESENTATION
namespace TwinKartGraphics {
bool ConfigureNativePresentation(PresentationObserver,PresentationLog){return false;}
bool IsNativePresentation(IDXGISwapChain*){return false;}
bool InsideNativePresentation(){return false;}
HRESULT PresentNativePresentation(IDXGISwapChain*,UINT,UINT,unsigned){return E_NOINTERFACE;}
}
#else
#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <atomic>
#include <memory>
#include <string>
#include <cstdio>
#include "FrameGenerationD3D12.h"
#include "SharedTextureBridge.h"
#include "NativeNeuralPass.h"
using Microsoft::WRL::ComPtr;
namespace TwinKartGraphics {
namespace {
using CreateLegacy=HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory*,IUnknown*,DXGI_SWAP_CHAIN_DESC*,IDXGISwapChain**);
using CreateHwnd=HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory2*,IUnknown*,HWND,const DXGI_SWAP_CHAIN_DESC1*,const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*,IDXGIOutput*,IDXGISwapChain1**);
CreateLegacy nativeCreate=nullptr;CreateHwnd nativeCreateHwnd=nullptr;
PresentationObserver observer=nullptr;PresentationLog logger=nullptr;
thread_local bool internal=false;
struct Internal { bool previous;Internal():previous(internal){internal=true;}~Internal(){internal=previous;} };
std::wstring runtimeDirectory,logDirectory;
SRWLOCK chainLock=SRWLOCK_INIT;
class PresentationChain;
PresentationChain* activeChain=nullptr;
bool creatingChain=false;
void Event(const char* name,HRESULT result,unsigned frames=0) {
    if(!logger)return;char row[320];
    sprintf_s(row,"{\"event\":\"%s\",\"hresult\":%ld,\"presented_frames\":%u,\"quality_verified\":false}\r\n",name,result,frames);logger(row);
}
struct SavedCompute {
    ID3D11DeviceContext* context;ComPtr<ID3D11ComputeShader> shader;
    ID3D11ClassInstance* instances[256]{};UINT count=256;
    ID3D11ShaderResourceView* resources[128]{};ID3D11UnorderedAccessView* unordered[8]{};
    ID3D11RenderTargetView* targets[8]{};ID3D11DepthStencilView* depth=nullptr;ID3D11UnorderedAccessView* pixelWrites[8]{};
    explicit SavedCompute(ID3D11DeviceContext* value):context(value) {
        context->CSGetShader(&shader,instances,&count);context->CSGetShaderResources(0,128,resources);context->CSGetUnorderedAccessViews(0,8,unordered);
        context->OMGetRenderTargets(8,targets,&depth);context->OMGetRenderTargetsAndUnorderedAccessViews(0,nullptr,nullptr,0,8,pixelWrites);
        ID3D11ShaderResourceView* empty[128]{};ID3D11UnorderedAccessView* noWrites[8]{};
        context->CSSetShaderResources(0,128,empty);context->CSSetUnorderedAccessViews(0,8,noWrites,nullptr);
        context->OMSetRenderTargetsAndUnorderedAccessViews(0,nullptr,nullptr,0,8,noWrites,nullptr);
    }
    ~SavedCompute(){
        ID3D11ShaderResourceView* empty[128]{};ID3D11UnorderedAccessView* noWrites[8]{};
        context->CSSetShaderResources(0,128,empty);context->CSSetUnorderedAccessViews(0,8,noWrites,nullptr);
        context->CSSetShader(shader.Get(),instances,count);context->CSSetShaderResources(0,128,resources);context->CSSetUnorderedAccessViews(0,8,unordered,nullptr);
        UINT targetCount=0;for(UINT i=0;i<8;++i)if(targets[i])targetCount=i+1;
        context->OMSetRenderTargetsAndUnorderedAccessViews(targetCount,targets,depth,targetCount,8-targetCount,pixelWrites+targetCount,nullptr);
        for(auto p:targets)if(p)p->Release();for(auto p:pixelWrites)if(p)p->Release();if(depth)depth->Release();
        for(auto p:resources)if(p)p->Release();for(auto p:unordered)if(p)p->Release();for(UINT i=0;i<count;++i)if(instances[i])instances[i]->Release();
    }
};
void Transition(ID3D12GraphicsCommandList* list,ID3D12Resource* resource,D3D12_RESOURCE_STATES from,D3D12_RESOURCE_STATES to) {
    D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition={resource,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,from,to};list->ResourceBarrier(1,&b);
}
struct RenderBuffers {
    SharedTextureBridge finalColor,depth,motion,hudless,alpha;
    ComPtr<ID3D11Texture2D> ui;ComPtr<ID3D11UnorderedAccessView> uiUav;ComPtr<ID3D11ComputeShader> uiShader;
    bool Initialize(ID3D11Device* d11,ID3D12Device* d12,ID3D12CommandQueue* queue,unsigned w,unsigned h,DXGI_FORMAT format) {
        if(FAILED(finalColor.Initialize(d11,d12,queue,w,h,format)) || FAILED(depth.Initialize(d11,d12,queue,w,h,DXGI_FORMAT_R32_FLOAT)) ||
           FAILED(motion.Initialize(d11,d12,queue,w,h,DXGI_FORMAT_R16G16_FLOAT)) || FAILED(hudless.Initialize(d11,d12,queue,w,h,format)) ||
           FAILED(alpha.Initialize(d11,d12,queue,w,h,DXGI_FORMAT_R8_UNORM)))return false;
        D3D11_TEXTURE2D_DESC desc{};desc.Width=w;desc.Height=h;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;
        desc.Format=DXGI_FORMAT_R8_UNORM;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS;
        if(FAILED(d11->CreateTexture2D(&desc,nullptr,&ui)) || FAILED(d11->CreateUnorderedAccessView(ui.Get(),nullptr,&uiUav)))return false;
        const char shader[]=R"(
Texture2D<float4> FinalColor:register(t0);Texture2D<float4> SceneColor:register(t1);RWTexture2D<float> Alpha:register(u0);
[numthreads(8,8,1)]void main(uint3 id:SV_DispatchThreadID){uint w,h;Alpha.GetDimensions(w,h);if(id.x>=w||id.y>=h)return;
float3 difference=abs(FinalColor.Load(int3(id.xy,0)).rgb-SceneColor.Load(int3(id.xy,0)).rgb);
Alpha[id.xy]=max(difference.r,max(difference.g,difference.b))>0.0005?1:0;})";
        ComPtr<ID3DBlob> code;
        return SUCCEEDED(D3DCompile(shader,sizeof(shader),nullptr,nullptr,nullptr,"main","cs_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&code,nullptr)) &&
            SUCCEEDED(d11->CreateComputeShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&uiShader));
    }
};
class PresentationChain final:public IDXGISwapChain4 {
    std::atomic<ULONG> references{1};
    SRWLOCK operationLock=SRWLOCK_INIT;
    struct Operation {
        SRWLOCK* lock;bool entered;
        explicit Operation(SRWLOCK& value):lock(&value),entered(TryAcquireSRWLockExclusive(lock)!=FALSE){}
        ~Operation(){if(entered)ReleaseSRWLockExclusive(lock);}
    };
    ComPtr<ID3D11Device> device11;ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D11Texture2D> shadow;
    ComPtr<ID3D12Device> device12;ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> commands;ComPtr<ID3D12Fence> fence;
    ComPtr<IDXGISwapChain4> display;
    FrameGeneration generation;std::unique_ptr<RenderBuffers> buffers;
    DXGI_SWAP_CHAIN_DESC nativeDesc{};
    HANDLE completed=nullptr;UINT64 fenceValue=0;DWORD renderThread=0;unsigned frame=0;
    bool failed=false;
    bool pendingResize=false,presentationReset=true;
    bool Racing() {
        using Active=BOOL(WINAPI*)();
        HMODULE race=GetModuleHandleW(L"TwinKartDriftRaceBridge.dll");
        auto active=race?reinterpret_cast<Active>(GetProcAddress(race,"TwinKartRaceRenderingActive")):nullptr;
        return active && active()!=FALSE;
    }
    HRESULT Wait() {
        if(!queue || !fence || !completed)return E_FAIL;
        HRESULT hr=queue->Signal(fence.Get(),++fenceValue);if(FAILED(hr))return hr;
        if(FAILED(hr=fence->SetEventOnCompletion(fenceValue,completed)))return hr;
        return WaitForSingleObject(completed,10000)==WAIT_OBJECT_0?S_OK:HRESULT_FROM_WIN32(WAIT_TIMEOUT);
    }
    HRESULT MakeShadow(unsigned w,unsigned h,DXGI_FORMAT format,ComPtr<ID3D11Texture2D>& out) {
        D3D11_TEXTURE2D_DESC desc{};desc.Width=w;desc.Height=h;desc.Format=format;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;
        desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        if(nativeDesc.BufferUsage&DXGI_USAGE_UNORDERED_ACCESS)desc.BindFlags|=D3D11_BIND_UNORDERED_ACCESS;
        return device11->CreateTexture2D(&desc,nullptr,&out);
    }
public:
#ifdef TWIN_KART_PRESENTATION_TEST
    HRESULT InitializeFixture(ID3D11Device* device,const DXGI_SWAP_CHAIN_DESC& desc,IDXGISwapChain4* chain) {
        device11=device;device11->GetImmediateContext(&context);nativeDesc=desc;display=chain;
        return MakeShadow(desc.BufferDesc.Width,desc.BufferDesc.Height,desc.BufferDesc.Format,shadow);
    }
#endif
    ~PresentationChain(){
        Internal nested;
        if(renderThread==GetCurrentThreadId())Wait();
        generation.Shutdown();buffers.reset();display.Reset();
        if(completed)CloseHandle(completed);
        AcquireSRWLockExclusive(&chainLock);if(activeChain==this)activeChain=nullptr;ReleaseSRWLockExclusive(&chainLock);
    }
    HRESULT Initialize(ID3D11Device* native,const DXGI_SWAP_CHAIN_DESC& desc) {
        nativeDesc=desc;device11=native;device11->GetImmediateContext(&context);
        HRESULT hr=MakeShadow(desc.BufferDesc.Width,desc.BufferDesc.Height,desc.BufferDesc.Format,shadow);if(FAILED(hr))return hr;
        unsigned result=generation.Load(runtimeDirectory.c_str(),logDirectory.c_str());if(result)return E_FAIL;
        using Factory=HRESULT(WINAPI*)(REFIID,void**);using Device=HRESULT(WINAPI*)(IUnknown*,D3D_FEATURE_LEVEL,REFIID,void**);
        auto createFactory=reinterpret_cast<Factory>(generation.GraphicsEntry("CreateDXGIFactory1"));auto createDevice=reinterpret_cast<Device>(generation.GraphicsEntry("D3D12CreateDevice"));
        ComPtr<IDXGIDevice> dxgi;ComPtr<IDXGIAdapter> adapter;ComPtr<IDXGIFactory2> factory;
        if(!createFactory || !createDevice || FAILED(hr=device11.As(&dxgi)) || FAILED(hr=dxgi->GetAdapter(&adapter)) ||
           FAILED(hr=createFactory(IID_PPV_ARGS(&factory))) || FAILED(hr=createDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device12))))return E_FAIL;
        if(generation.BindDevice(device12.Get()) || generation.Configure(2,desc.BufferDesc.Width,desc.BufferDesc.Height,desc.BufferDesc.Format,2))return E_FAIL;
        D3D12_COMMAND_QUEUE_DESC q{};
        if(FAILED(hr=device12->CreateCommandQueue(&q,IID_PPV_ARGS(&queue))) || FAILED(hr=device12->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator))) ||
           FAILED(hr=device12->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&commands))) ||
           FAILED(hr=commands->Close()) || FAILED(hr=device12->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence))))return hr;
        completed=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!completed)return HRESULT_FROM_WIN32(GetLastError());
        DXGI_SWAP_CHAIN_DESC1 target{};target.Width=desc.BufferDesc.Width;target.Height=desc.BufferDesc.Height;target.Format=desc.BufferDesc.Format;
        target.SampleDesc.Count=1;target.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;target.BufferCount=2;target.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
        target.Scaling=DXGI_SCALING_STRETCH;target.AlphaMode=DXGI_ALPHA_MODE_IGNORE;
        target.Flags=desc.Flags&DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreen{};fullscreen.RefreshRate=desc.BufferDesc.RefreshRate;
        fullscreen.ScanlineOrdering=desc.BufferDesc.ScanlineOrdering;fullscreen.Scaling=desc.BufferDesc.Scaling;fullscreen.Windowed=desc.Windowed;
        ComPtr<IDXGISwapChain1> chain;
        hr=factory->CreateSwapChainForHwnd(queue.Get(),desc.OutputWindow,&target,&fullscreen,nullptr,&chain);
        if(FAILED(hr) || FAILED(hr=chain.As(&display)))return hr;
        factory->MakeWindowAssociation(desc.OutputWindow,DXGI_MWA_NO_ALT_ENTER);
        return S_OK;
    }
    HRESULT DoPresent(UINT interval,UINT flags,unsigned renderFrame) {
        Operation operation(operationLock);if(!operation.entered)return DXGI_ERROR_WAS_STILL_DRAWING;
        Internal nested;
        frame=renderFrame;
        if(flags&DXGI_PRESENT_TEST)return display->Present(interval,flags);
        if(!renderThread) {
            if(!generation.AdoptInitialRenderThread())return E_FAIL;
            renderThread=GetCurrentThreadId();
        }
        if(failed || renderThread!=GetCurrentThreadId())return E_FAIL;
        if(pendingResize) {
            if(generation.Configure(2,nativeDesc.BufferDesc.Width,nativeDesc.BufferDesc.Height,nativeDesc.BufferDesc.Format,2)!=0){failed=true;return E_FAIL;}
            buffers.reset();pendingResize=false;
        }
        if(!buffers) {
            buffers.reset(new RenderBuffers);
            if(!buffers->Initialize(device11.Get(),device12.Get(),queue.Get(),nativeDesc.BufferDesc.Width,nativeDesc.BufferDesc.Height,nativeDesc.BufferDesc.Format)) {failed=true;Event("fg_bridge_initialize_failed",E_FAIL);}
        }
        if(failed)return E_FAIL;
        unsigned w=nativeDesc.BufferDesc.Width,h=nativeDesc.BufferDesc.Height;
        NativePresentationInputs guides;bool haveGuides=Racing() && GetNativePresentationInputs(frame,guides);
        if(haveGuides && presentationReset)guides.camera.reset=true;
        // Missing/current-frame guide failures disable interpolation. A native
        // rendered frame is still copied and displayed through the same chain.
        if(generation.SetActive(haveGuides)!=0){failed=true;return E_FAIL;}
        if(haveGuides) {
            SavedCompute saved(context.Get());ComPtr<ID3D11ShaderResourceView> finalView,sceneView;
            if(FAILED(device11->CreateShaderResourceView(shadow.Get(),nullptr,&finalView)) || FAILED(device11->CreateShaderResourceView(guides.hudless,nullptr,&sceneView)))haveGuides=false;
            else {
                ID3D11ShaderResourceView* views[]{finalView.Get(),sceneView.Get()};ID3D11UnorderedAccessView* write=buffers->uiUav.Get();
                context->CSSetShader(buffers->uiShader.Get(),nullptr,0);context->CSSetShaderResources(0,2,views);context->CSSetUnorderedAccessViews(0,1,&write,nullptr);
                context->Dispatch((w+7)/8,(h+7)/8,1);
            }
        }
        HRESULT hr=buffers->finalColor.CopyTo12(shadow.Get());if(FAILED(hr))return hr;
        if(haveGuides && (FAILED(buffers->depth.CopyTo12(guides.depth)) || FAILED(buffers->motion.CopyTo12(guides.motion)) ||
           FAILED(buffers->hudless.CopyTo12(guides.hudless)) || FAILED(buffers->alpha.CopyTo12(buffers->ui.Get())))) {failed=true;return E_FAIL;}
        if(FAILED(hr=allocator->Reset()) || FAILED(hr=commands->Reset(allocator.Get(),nullptr)))return hr;
        bool guideStates=false;
        if(haveGuides) {
            if(generation.BeginFrame(frame,guides.camera)!=0)haveGuides=false;
            else {
                generation.Marker(FrameStage::RenderSubmitStart);
                ID3D12Resource* tagged[]{buffers->depth.Texture12(),buffers->motion.Texture12(),buffers->hudless.Texture12(),buffers->alpha.Texture12()};
                for(auto p:tagged)Transition(commands.Get(),p,D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                GenerationResources inputs{commands.Get(),tagged[0],tagged[1],tagged[2],tagged[3],D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,true,true};
                guideStates=true;
                if(generation.Tag(inputs)!=0)haveGuides=false;
            }
        }
        if(!haveGuides && generation.SetActive(false)!=0){failed=true;return E_FAIL;}
        presentationReset=!haveGuides;
        ComPtr<ID3D12Resource> backbuffer;
        if(FAILED(hr=display->GetBuffer(display->GetCurrentBackBufferIndex(),IID_PPV_ARGS(&backbuffer))))return hr;
        Transition(commands.Get(),buffers->finalColor.Texture12(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_SOURCE);
        Transition(commands.Get(),backbuffer.Get(),D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_COPY_DEST);
        commands->CopyResource(backbuffer.Get(),buffers->finalColor.Texture12());
        Transition(commands.Get(),backbuffer.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PRESENT);
        Transition(commands.Get(),buffers->finalColor.Texture12(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_COMMON);
        if(FAILED(hr=commands->Close()))return hr;
        ID3D12CommandList* lists[]{commands.Get()};queue->ExecuteCommandLists(1,lists);
        if(haveGuides){generation.Marker(FrameStage::RenderSubmitEnd);generation.Marker(FrameStage::PresentStart);}
        hr=display->Present(interval,flags);
        HRESULT displayResult=hr;
        if(haveGuides)generation.Marker(FrameStage::PresentEnd);
        auto state=generation.Presented();
        if(state.inputCompletionFence && state.inputCompletionValue)queue->Wait(state.inputCompletionFence,state.inputCompletionValue);
        HRESULT waited=Wait();if(FAILED(waited)){failed=true;return waited;}
        if(guideStates) {
            if(FAILED(hr=allocator->Reset()) || FAILED(hr=commands->Reset(allocator.Get(),nullptr)))return hr;
            ID3D12Resource* tagged[]{buffers->depth.Texture12(),buffers->motion.Texture12(),buffers->hudless.Texture12(),buffers->alpha.Texture12()};
            for(auto p:tagged)Transition(commands.Get(),p,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COMMON);
            if(FAILED(hr=commands->Close()))return hr;
            ID3D12CommandList* returns[]{commands.Get()};queue->ExecuteCommandLists(1,returns);
            if(FAILED(waited=Wait())){failed=true;return waited;}
        }
        HRESULT returned=buffers->finalColor.ReturnTo11(nullptr);
        // Return any acquired guides even if the runtime refused their tags.
        HRESULT depthReturned=buffers->depth.ReturnTo11(nullptr),motionReturned=buffers->motion.ReturnTo11(nullptr),
            colorReturned=buffers->hudless.ReturnTo11(nullptr),alphaReturned=buffers->alpha.ReturnTo11(nullptr);
        if(FAILED(returned) || (guideStates && (FAILED(depthReturned) || FAILED(motionReturned) || FAILED(colorReturned) || FAILED(alphaReturned)))){failed=true;return E_FAIL;}
        if(frame%120==0 || FAILED(displayResult))Event(haveGuides?"fg_game_chain_present":"fg_game_chain_native_frame_only",displayResult,state.lastPresented);
        return displayResult;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override {
        if(!out)return E_POINTER;*out=nullptr;
        if(id==__uuidof(IUnknown) || id==__uuidof(IDXGIObject) || id==__uuidof(IDXGIDeviceSubObject) || id==__uuidof(IDXGISwapChain) ||
           id==__uuidof(IDXGISwapChain1) || id==__uuidof(IDXGISwapChain2) || id==__uuidof(IDXGISwapChain3) || id==__uuidof(IDXGISwapChain4)) {*out=static_cast<IDXGISwapChain4*>(this);AddRef();return S_OK;}
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override{return ++references;}
    ULONG STDMETHODCALLTYPE Release() override{ULONG left=--references;if(!left)delete this;return left;}
    HRESULT STDMETHODCALLTYPE GetDevice(REFIID id,void** out) override{return device11->QueryInterface(id,out);}
    HRESULT STDMETHODCALLTYPE GetBuffer(UINT index,REFIID id,void** out) override{if(!out)return E_POINTER;*out=nullptr;return index==0?shadow->QueryInterface(id,out):DXGI_ERROR_INVALID_CALL;}
    HRESULT STDMETHODCALLTYPE GetDesc(DXGI_SWAP_CHAIN_DESC* out) override{if(!out)return E_POINTER;*out=nativeDesc;return S_OK;}
    HRESULT STDMETHODCALLTYPE Present(UINT interval,UINT flags) override{return observer?observer(this,interval,flags):E_UNEXPECTED;}
    HRESULT STDMETHODCALLTYPE Present1(UINT interval,UINT flags,const DXGI_PRESENT_PARAMETERS* p) override {
        if(p && (p->DirtyRectsCount || p->pScrollRect || p->pScrollOffset))return DXGI_ERROR_UNSUPPORTED;return Present(interval,flags);
    }
    HRESULT STDMETHODCALLTYPE SetFullscreenState(BOOL full,IDXGIOutput* output) override {
        Operation operation(operationLock);if(!operation.entered)return DXGI_ERROR_WAS_STILL_DRAWING;
        Internal nested;
        if(renderThread && FAILED(Wait()))return E_FAIL;
        HRESULT hr=display->SetFullscreenState(full,output);if(SUCCEEDED(hr)){nativeDesc.Windowed=!full;presentationReset=true;}return hr;
    }
    UINT STDMETHODCALLTYPE GetCurrentBackBufferIndex() override{return 0;}
    HRESULT STDMETHODCALLTYPE ResizeBuffers(UINT count,UINT w,UINT h,DXGI_FORMAT format,UINT flags) override {
        Operation operation(operationLock);if(!operation.entered)return DXGI_ERROR_WAS_STILL_DRAWING;
        if(count>1 || (flags&~DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH))return DXGI_ERROR_INVALID_CALL;
        shadow->AddRef();ULONG held=shadow->Release();if(held!=1)return DXGI_ERROR_INVALID_CALL;
        RECT bounds{};if((!w || !h) && !GetClientRect(nativeDesc.OutputWindow,&bounds))return E_FAIL;
        if(!w)w=bounds.right-bounds.left;if(!h)h=bounds.bottom-bounds.top;if(format==DXGI_FORMAT_UNKNOWN)format=nativeDesc.BufferDesc.Format;
        if(w<128 || h<128 || w>4096 || h>4096 || format!=nativeDesc.BufferDesc.Format)return DXGI_ERROR_INVALID_CALL;
        ComPtr<ID3D11Texture2D> replacement;HRESULT hr=MakeShadow(w,h,format,replacement);if(FAILED(hr))return hr;
        Internal nested;if(renderThread && FAILED(hr=Wait()))return hr;
        if(FAILED(hr=display->ResizeBuffers(2,w,h,format,flags)))return hr;
        shadow=replacement;nativeDesc.BufferDesc.Width=w;nativeDesc.BufferDesc.Height=h;nativeDesc.Flags=flags;
        // DXGI may resize on the window thread. Keep bridge retirement and
        // Streamline configuration on their established render owner; GPU
        // work is already complete and no Present can overlap this operation.
        pendingResize=true;presentationReset=true;return S_OK;
    }
    HRESULT STDMETHODCALLTYPE ResizeBuffers1(UINT count,UINT w,UINT h,DXGI_FORMAT format,UINT flags,const UINT* masks,IUnknown* const* queues) override {
        if(masks || queues)return DXGI_ERROR_UNSUPPORTED;return ResizeBuffers(count,w,h,format,flags);
    }
    // Native descriptors describe the D3D11 shadow interface. The D3D12 flip
    // chain remains private, including its distinct backbuffer count.
    HRESULT STDMETHODCALLTYPE GetDesc1(DXGI_SWAP_CHAIN_DESC1* out) override {
        if(!out)return E_POINTER;HRESULT hr=display->GetDesc1(out);if(FAILED(hr))return hr;
        out->Width=nativeDesc.BufferDesc.Width;out->Height=nativeDesc.BufferDesc.Height;out->Format=nativeDesc.BufferDesc.Format;
        out->BufferCount=1;out->SwapEffect=DXGI_SWAP_EFFECT_DISCARD;out->BufferUsage=nativeDesc.BufferUsage;out->Flags=nativeDesc.Flags;return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID name,UINT size,const void* data) override{return display->SetPrivateData(name,size,data);}
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID name,const IUnknown* value) override{return display->SetPrivateDataInterface(name,value);}
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID name,UINT* size,void* data) override{return display->GetPrivateData(name,size,data);}
    HRESULT STDMETHODCALLTYPE GetParent(REFIID id,void** out) override{return display->GetParent(id,out);}
    HRESULT STDMETHODCALLTYPE GetFullscreenState(BOOL* full,IDXGIOutput** output) override{return display->GetFullscreenState(full,output);}
    HRESULT STDMETHODCALLTYPE ResizeTarget(const DXGI_MODE_DESC* target) override{return display->ResizeTarget(target);}
    HRESULT STDMETHODCALLTYPE GetContainingOutput(IDXGIOutput** output) override{return display->GetContainingOutput(output);}
    HRESULT STDMETHODCALLTYPE GetFrameStatistics(DXGI_FRAME_STATISTICS* stats) override{return display->GetFrameStatistics(stats);}
    HRESULT STDMETHODCALLTYPE GetLastPresentCount(UINT* count) override{return display->GetLastPresentCount(count);}
    HRESULT STDMETHODCALLTYPE GetFullscreenDesc(DXGI_SWAP_CHAIN_FULLSCREEN_DESC* desc) override{return display->GetFullscreenDesc(desc);}
    HRESULT STDMETHODCALLTYPE GetHwnd(HWND* window) override{return display->GetHwnd(window);}
    HRESULT STDMETHODCALLTYPE GetCoreWindow(REFIID id,void** out) override{return display->GetCoreWindow(id,out);}
    BOOL STDMETHODCALLTYPE IsTemporaryMonoSupported() override{return display->IsTemporaryMonoSupported();}
    HRESULT STDMETHODCALLTYPE GetRestrictToOutput(IDXGIOutput** output) override{return display->GetRestrictToOutput(output);}
    HRESULT STDMETHODCALLTYPE SetBackgroundColor(const DXGI_RGBA* color) override{return display->SetBackgroundColor(color);}
    HRESULT STDMETHODCALLTYPE GetBackgroundColor(DXGI_RGBA* color) override{return display->GetBackgroundColor(color);}
    HRESULT STDMETHODCALLTYPE SetRotation(DXGI_MODE_ROTATION rotation) override{return display->SetRotation(rotation);}
    HRESULT STDMETHODCALLTYPE GetRotation(DXGI_MODE_ROTATION* rotation) override{return display->GetRotation(rotation);}
    HRESULT STDMETHODCALLTYPE SetSourceSize(UINT width,UINT height) override{return display->SetSourceSize(width,height);}
    HRESULT STDMETHODCALLTYPE GetSourceSize(UINT* width,UINT* height) override{return display->GetSourceSize(width,height);}
    HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(UINT latency) override{return display->SetMaximumFrameLatency(latency);}
    HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(UINT* latency) override{return display->GetMaximumFrameLatency(latency);}
    HANDLE STDMETHODCALLTYPE GetFrameLatencyWaitableObject() override{return display->GetFrameLatencyWaitableObject();}
    HRESULT STDMETHODCALLTYPE SetMatrixTransform(const DXGI_MATRIX_3X2_F* matrix) override{return display->SetMatrixTransform(matrix);}
    HRESULT STDMETHODCALLTYPE GetMatrixTransform(DXGI_MATRIX_3X2_F* matrix) override{return display->GetMatrixTransform(matrix);}
    HRESULT STDMETHODCALLTYPE CheckColorSpaceSupport(DXGI_COLOR_SPACE_TYPE type,UINT* support) override{return display->CheckColorSpaceSupport(type,support);}
    HRESULT STDMETHODCALLTYPE SetColorSpace1(DXGI_COLOR_SPACE_TYPE type) override{return display->SetColorSpace1(type);}
    HRESULT STDMETHODCALLTYPE SetHDRMetaData(DXGI_HDR_METADATA_TYPE type,UINT size,void* data) override{return display->SetHDRMetaData(type,size,data);}
};
bool Supported(const DXGI_SWAP_CHAIN_DESC& d) {
    DWORD pid=0;GetWindowThreadProcessId(d.OutputWindow,&pid);
    return d.OutputWindow && pid==GetCurrentProcessId() && d.BufferCount==1 && d.SwapEffect==DXGI_SWAP_EFFECT_DISCARD &&
        d.SampleDesc.Count==1 && !d.SampleDesc.Quality && d.BufferDesc.Width>=128 && d.BufferDesc.Height>=128 &&
        d.BufferDesc.Width<=4096 && d.BufferDesc.Height<=4096 && d.BufferDesc.Format==DXGI_FORMAT_R10G10B10A2_UNORM && !(d.Flags&~DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
}
HRESULT Wrap(IUnknown* device,const DXGI_SWAP_CHAIN_DESC& desc,IDXGISwapChain** out) {
    if(internal || !out || !Supported(desc))return DXGI_ERROR_UNSUPPORTED;
    ComPtr<ID3D11Device> native;if(FAILED(device->QueryInterface(IID_PPV_ARGS(&native))))return DXGI_ERROR_UNSUPPORTED;
    AcquireSRWLockExclusive(&chainLock);bool busy=activeChain!=nullptr || creatingChain;if(!busy)creatingChain=true;ReleaseSRWLockExclusive(&chainLock);if(busy)return DXGI_ERROR_UNSUPPORTED;
    Internal nested;auto candidate=new PresentationChain;HRESULT hr=candidate->Initialize(native.Get(),desc);
    AcquireSRWLockExclusive(&chainLock);creatingChain=false;if(SUCCEEDED(hr))activeChain=candidate;ReleaseSRWLockExclusive(&chainLock);
    if(FAILED(hr)){candidate->Release();Event("fg_native_chain_create_failed",hr);return hr;}
    *out=candidate;Event("fg_native_shadow_chain_created",S_OK);return S_OK;
}
HRESULT STDMETHODCALLTYPE OnCreate(IDXGIFactory* factory,IUnknown* device,DXGI_SWAP_CHAIN_DESC* desc,IDXGISwapChain** out) {
    if(!internal && device && desc && out && SUCCEEDED(Wrap(device,*desc,out)))return S_OK;
    return nativeCreate(factory,device,desc,out);
}
HRESULT STDMETHODCALLTYPE OnCreateHwnd(IDXGIFactory2* factory,IUnknown* device,HWND window,const DXGI_SWAP_CHAIN_DESC1* desc,const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* full,IDXGIOutput* restrict,IDXGISwapChain1** out) {
    if(!internal && device && desc && out && !restrict) {
        DXGI_SWAP_CHAIN_DESC legacy{};legacy.BufferDesc.Width=desc->Width;legacy.BufferDesc.Height=desc->Height;legacy.BufferDesc.Format=desc->Format;
        legacy.SampleDesc=desc->SampleDesc;legacy.BufferUsage=desc->BufferUsage;legacy.BufferCount=desc->BufferCount;legacy.OutputWindow=window;
        legacy.Windowed=full?full->Windowed:TRUE;legacy.SwapEffect=desc->SwapEffect;legacy.Flags=desc->Flags;
        if(full){legacy.BufferDesc.RefreshRate=full->RefreshRate;legacy.BufferDesc.ScanlineOrdering=full->ScanlineOrdering;legacy.BufferDesc.Scaling=full->Scaling;}
        IDXGISwapChain* result=nullptr;if(SUCCEEDED(Wrap(device,legacy,&result))){*out=static_cast<IDXGISwapChain1*>(result);return S_OK;}
    }
    return nativeCreateHwnd(factory,device,window,desc,full,restrict,out);
}
bool Patch(void** slot,void* target,void** previous) {
    DWORD old=0;if(!VirtualProtect(slot,sizeof(void*),PAGE_READWRITE,&old))return false;
    *previous=InterlockedExchangePointer(slot,target);DWORD ignored=0;VirtualProtect(slot,sizeof(void*),old,&ignored);return true;
}
}
bool ConfigureNativePresentation(PresentationObserver callback,PresentationLog log) {
    if(observer || !callback || !log)return false;
    wchar_t enabled[8]{},directory[32768]{},logs[32768]{};
    GetEnvironmentVariableW(L"TWIN_KART_FRAME_GENERATION",enabled,ARRAYSIZE(enabled));
    DWORD n=GetEnvironmentVariableW(L"TWIN_KART_FG_PROVIDER",directory,ARRAYSIZE(directory)),m=GetEnvironmentVariableW(L"TWIN_KART_FG_LOG_DIRECTORY",logs,ARRAYSIZE(logs));
    if(wcscmp(enabled,L"1") || !n || n>=ARRAYSIZE(directory) || !m || m>=ARRAYSIZE(logs))return false;
    runtimeDirectory=directory;logDirectory=logs;observer=callback;logger=log;
    ComPtr<IDXGIFactory2> factory;if(FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))return false;
    auto table=*reinterpret_cast<void***>(factory.Get());
    bool a=Patch(table+10,reinterpret_cast<void*>(OnCreate),reinterpret_cast<void**>(&nativeCreate));
    bool b=Patch(table+15,reinterpret_cast<void*>(OnCreateHwnd),reinterpret_cast<void**>(&nativeCreateHwnd));
    Event(a && b?"fg_native_factory_hooks_installed":"fg_native_factory_hooks_partial",a && b?S_OK:E_FAIL);return a && b;
}
bool IsNativePresentation(IDXGISwapChain* chain) {
    AcquireSRWLockShared(&chainLock);bool same=chain && activeChain==chain;ReleaseSRWLockShared(&chainLock);return same;
}
bool InsideNativePresentation(){return internal;}
HRESULT PresentNativePresentation(IDXGISwapChain* chain,UINT interval,UINT flags,unsigned frame) {
    return IsNativePresentation(chain)?static_cast<PresentationChain*>(chain)->DoPresent(interval,flags,frame):E_NOINTERFACE;
}
}
#endif
