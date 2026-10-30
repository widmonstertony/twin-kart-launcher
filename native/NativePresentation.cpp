#define WIN32_LEAN_AND_MEAN
#include "NativePresentation.h"
#ifndef TWIN_KART_FRAME_PRESENTATION
namespace TwinKartGraphics {
bool ConfigureNativePresentation(PresentationObserver,PresentationLog){return false;}
bool IsNativePresentation(IDXGISwapChain*){return false;}
bool NativePresentationOwnsNgxDevice(ID3D12Device*){return false;}
bool InsideNativePresentation(){return false;}
unsigned NativePresentationFactoryHookMask(IDXGISwapChain*){return 0;}
UINT NativePresentationBufferIndex(IDXGISwapChain*,unsigned){return 0;}
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
#include "NativeTemporalPass.h"
#include "NativeDeviceIdentity.h"
#include "NativeFrameMarkers.h"
#include "NativeSceneRectangle.h"
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
void Ledger(FrameGeneration& generation,unsigned frame,unsigned long long nativeFrame) {
    if(!logger)return;auto d=generation.InspectNativeFrame(nativeFrame);char row[1200];
    sprintf_s(row,"{\"event\":\"fg_native_frame_ledger\",\"frame\":%u,\"native_frame\":%llu,\"live\":%s,\"failed\":%s,\"has_token\":%s,\"seen\":%u,\"completed\":%u,\"token_result\":%u,\"simulation_thread\":%lu,\"stage_ids\":[%llu,%llu,%llu,%llu,%llu,%llu],\"stage_threads\":[%lu,%lu,%lu,%lu,%lu,%lu],\"rejected\":[%u,%u,%u,%u,%u,%u],\"marker_results\":[%u,%u,%u,%u,%u,%u]}\r\n",
        frame,nativeFrame,d.live?"true":"false",d.failed?"true":"false",d.hasToken?"true":"false",d.seen,d.completed,d.tokenResult,d.simulationThread,
        d.stageIds[0],d.stageIds[1],d.stageIds[2],d.stageIds[3],d.stageIds[4],d.stageIds[5],d.stageThreads[0],d.stageThreads[1],d.stageThreads[2],d.stageThreads[3],d.stageThreads[4],d.stageThreads[5],
        d.rejected[0],d.rejected[1],d.rejected[2],d.rejected[3],d.rejected[4],d.rejected[5],d.markerResults[0],d.markerResults[1],d.markerResults[2],d.markerResults[3],d.markerResults[4],d.markerResults[5]);logger(row);
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
    ComPtr<ID3D11Texture2D> shadows[2];
    UINT nativeIndex=0;
    bool indexedConsumer=false;
    ComPtr<ID3D12Device> device12;ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> commands;ComPtr<ID3D12Fence> fence;
    ComPtr<IDXGISwapChain4> display;
    FrameGeneration generation;std::unique_ptr<RenderBuffers> buffers;
    DXGI_SWAP_CHAIN_DESC nativeDesc{};
    HANDLE completed=nullptr;UINT64 fenceValue=0;DWORD renderThread=0;unsigned frame=0;
    bool failed=false;
    bool pendingResize=false,presentationReset=true;
    // GetState reports an asynchronous count since its previous query. A
    // single value of two must not be reported as a sustained 2x multiplier.
    unsigned intervalCalls=0,intervalQualified=0,intervalQueryErrors=0,intervalOccluded=0;
    unsigned intervalNativeChanges=0,intervalRepeatedNative=0;
    unsigned long long intervalPresented=0,intervalNative=0;
    LARGE_INTEGER intervalStart{};
    void RecordPresentation(const GenerationState& state,HRESULT result,bool qualified,bool haveNative,unsigned long long id) {
        LARGE_INTEGER now{},frequency{};QueryPerformanceCounter(&now);QueryPerformanceFrequency(&frequency);
        if(!intervalCalls)intervalStart=now;
        ++intervalCalls;if(qualified)++intervalQualified;
        if(state.result)++intervalQueryErrors;else intervalPresented+=state.lastPresented;
        if(result==DXGI_STATUS_OCCLUDED)++intervalOccluded;
        if(haveNative) {
            if(intervalNativeChanges && intervalNative==id)++intervalRepeatedNative;
            else ++intervalNativeChanges;
            intervalNative=id;
        }
        if(intervalCalls<120)return;
        if(logger){char row[1024];sprintf_s(row,"{\"event\":\"fg_presentation_interval\",\"render_frame\":%u,\"present_calls\":%u,\"qualified_calls\":%u,\"provider_presented_since_queries\":%llu,\"query_errors\":%u,\"occluded_calls\":%u,\"native_id_changes\":%u,\"repeated_native_ids\":%u,\"elapsed_ms\":%.6f,\"counter_scope\":\"since_last_provider_query\",\"generated_frames\":null,\"quality_verified\":false}\r\n",
            frame,intervalCalls,intervalQualified,intervalPresented,intervalQueryErrors,intervalOccluded,intervalNativeChanges,intervalRepeatedNative,
            frequency.QuadPart?1000.0*(now.QuadPart-intervalStart.QuadPart)/frequency.QuadPart:0.0);logger(row);}
        intervalCalls=intervalQualified=intervalQueryErrors=intervalOccluded=0;
        intervalNativeChanges=intervalRepeatedNative=0;intervalPresented=intervalNative=0;
    }
    bool Activity(const char* exportName) {
        using Active=BOOL(WINAPI*)();
        HMODULE race=GetModuleHandleW(L"TwinKartDriftRaceBridge.dll");
        auto active=race?reinterpret_cast<Active>(GetProcAddress(race,exportName)):nullptr;
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
    HRESULT MakeShadows(unsigned w,unsigned h,DXGI_FORMAT format,UINT count,ComPtr<ID3D11Texture2D> (&out)[2]) {
        for(UINT i=0;i<count;++i){HRESULT hr=MakeShadow(w,h,format,out[i]);if(FAILED(hr))return hr;}
        return S_OK;
    }
public:
    UINT ProducedBuffer(unsigned renderFrame) {
        // D3D11 engines may retain GetBuffer(0), whereas explicit flip clients
        // acquire the current index. Use the current frame's actual producer,
        // never the independent D3D12 display chain's buffer index.
        for(UINT i=0;i<nativeDesc.BufferCount;++i)
            if(NativeNeuralOutputMatches(shadows[i].Get(),renderFrame))return i;
        NativeRect rectangle{};
        if(NativeSceneRectangle(shadows[nativeIndex].Get(),context.Get(),renderFrame,
            nativeDesc.BufferDesc.Width,nativeDesc.BufferDesc.Height,rectangle))return nativeIndex;
        for(UINT i=0;i<nativeDesc.BufferCount;++i)
            if(NativeSceneRectangle(shadows[i].Get(),context.Get(),renderFrame,
                nativeDesc.BufferDesc.Width,nativeDesc.BufferDesc.Height,rectangle))return i;
        return nativeIndex;
    }
#ifdef TWIN_KART_PRESENTATION_TEST
    HRESULT InitializeFixture(ID3D11Device* device,const DXGI_SWAP_CHAIN_DESC& desc,IDXGISwapChain4* chain) {
        device11=device;device11->GetImmediateContext(&context);nativeDesc=desc;display=chain;
        return MakeShadows(desc.BufferDesc.Width,desc.BufferDesc.Height,desc.BufferDesc.Format,desc.BufferCount,shadows);
    }
#endif
    ~PresentationChain(){
        Internal nested;
        if(renderThread)Wait();
        if(context && !RetireNativeNeuralForDeviceShutdown(context.Get()))Event("nr_shared_core_shutdown_retirement_failed",E_FAIL);
        generation.Shutdown();buffers.reset();display.Reset();
        if(completed)CloseHandle(completed);
        AcquireSRWLockExclusive(&chainLock);if(activeChain==this)activeChain=nullptr;ReleaseSRWLockExclusive(&chainLock);
    }
    bool OwnsNgxDevice(ID3D12Device* device)const{return SameNativeGraphicsDevice(device12.Get(),device);}
    HRESULT Initialize(ID3D11Device* native,const DXGI_SWAP_CHAIN_DESC& desc) {
        nativeDesc=desc;device11=native;device11->GetImmediateContext(&context);
        HRESULT hr=MakeShadows(desc.BufferDesc.Width,desc.BufferDesc.Height,desc.BufferDesc.Format,desc.BufferCount,shadows);if(FAILED(hr))return hr;
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
        target.Flags=desc.Flags&(DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH|DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING);
        DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreen{};fullscreen.RefreshRate=desc.BufferDesc.RefreshRate;
        fullscreen.ScanlineOrdering=desc.BufferDesc.ScanlineOrdering;fullscreen.Scaling=desc.BufferDesc.Scaling;fullscreen.Windowed=desc.Windowed;
        ComPtr<IDXGISwapChain1> chain;
        hr=factory->CreateSwapChainForHwnd(queue.Get(),desc.OutputWindow,&target,&fullscreen,nullptr,&chain);
        if(FAILED(hr) || FAILED(hr=chain.As(&display)))return hr;
        factory->MakeWindowAssociation(desc.OutputWindow,DXGI_MWA_NO_ALT_ENTER);
        if(!generation.AttachNativeMarkers())return E_FAIL;
        return S_OK;
    }
    HRESULT DoPresent(UINT interval,UINT flags,unsigned renderFrame) {
        Operation operation(operationLock);if(!operation.entered)return DXGI_ERROR_WAS_STILL_DRAWING;
        Internal nested;
        frame=renderFrame;
        if(flags&DXGI_PRESENT_TEST)return display->Present(interval,flags);
        UINT producedIndex=ProducedBuffer(renderFrame);
        ID3D11Texture2D* shadow=shadows[producedIndex].Get();
        if(!renderThread) {
            if(!generation.AdoptInitialRenderThread())return E_FAIL;
            renderThread=GetCurrentThreadId();
        }
        if(failed)return E_FAIL;
        if(renderThread!=GetCurrentThreadId()) {
            // UE presents startup/loading movies on a temporary thread, then
            // hands its immediate context to the RHI renderer. The operation
            // lock excludes the old Present; Wait drains prior GPU/input use.
            HRESULT drained=Wait();if(FAILED(drained)){Event("fg_thread_handoff_drain_failed",drained);return drained;}
            if(!generation.AdoptDrainedRenderThread()){Event("fg_thread_handoff_rejected",E_FAIL);return E_FAIL;}
            buffers.reset();renderThread=GetCurrentThreadId();presentationReset=true;
            Event("fg_native_render_thread_handoff",S_OK);
        }
        // An ordinary foreign draw is not permission to take NR ownership.
        // Only this serialized, real native Present/context boundary may
        // retire the previous runtime after queue and bridge completion.
        // A deferred retirement keeps native presentation available and is
        // retried next frame; it does not silently disable reconstruction.
        if(!RetireNativeTemporalForRenderThread(context.Get()) && frame%120==0)
            Event("sr_temporal_thread_retirement_pending",E_PENDING);
        if(!RetireNativeNeuralForRenderThread(context.Get()) && frame%120==0)
            Event("native_reconstruction_handoff_deferred",S_FALSE);
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
        unsigned long long nativeFrame=0;
        NativePresentationInputs guides{};
        bool racing=Activity("TwinKartRaceRenderingActive");
        bool sceneActive=Activity("TwinKartSceneRenderingActive");
        bool haveInputs=sceneActive && GetNativePresentationInputs(frame,guides);
        bool haveMarker=CurrentNativeRenderFrame(nativeFrame);
        bool haveGuides=haveInputs && guides.nativeFrameObserved && haveMarker && nativeFrame==guides.nativeFrame;
        if(frame%120==0 && logger) {
            char row[640];sprintf_s(row,"{\"event\":\"fg_native_producer\",\"frame\":%u,\"buffer\":%u,\"logical_index\":%u,\"indexed_consumer\":%s,\"racing\":%s,\"guides\":%s,\"reconstruction_inputs\":%s,\"guide_marker_observed\":%s,\"present_marker_observed\":%s,\"native_frame\":%llu,\"guide_native_frame\":%llu}\r\n",
                frame,producedIndex,nativeIndex,indexedConsumer?"true":"false",racing?"true":"false",haveGuides?"true":"false",haveInputs?"true":"false",guides.nativeFrameObserved?"true":"false",haveMarker?"true":"false",nativeFrame,guides.nativeFrame);logger(row);
            if(sceneActive && !haveInputs && haveMarker)Ledger(generation,frame,nativeFrame);
            char activity[160];sprintf_s(activity,"{\"event\":\"fg_scene_activity\",\"frame\":%u,\"scene_active\":%s,\"racing\":%s}\r\n",frame,sceneActive?"true":"false",racing?"true":"false");logger(activity);
        }
        if(haveGuides && presentationReset)guides.camera.reset=true;
        // Missing/current-frame guide failures disable interpolation. A native
        // rendered frame is still copied and displayed through the same chain.
        if(haveGuides) {
            SavedCompute saved(context.Get());ComPtr<ID3D11ShaderResourceView> finalView,sceneView;
            if(FAILED(device11->CreateShaderResourceView(shadow,nullptr,&finalView)) || FAILED(device11->CreateShaderResourceView(guides.hudless,nullptr,&sceneView)))haveGuides=false;
            else {
                ID3D11ShaderResourceView* views[]{finalView.Get(),sceneView.Get()};ID3D11UnorderedAccessView* write=buffers->uiUav.Get();
                context->CSSetShader(buffers->uiShader.Get(),nullptr,0);context->CSSetShaderResources(0,2,views);context->CSSetUnorderedAccessViews(0,1,&write,nullptr);
                context->Dispatch((w+7)/8,(h+7)/8,1);
            }
        }
        HRESULT hr=buffers->finalColor.CopyTo12(shadow);if(FAILED(hr))return hr;
        if(haveGuides && (FAILED(buffers->depth.CopyTo12(guides.depth)) || FAILED(buffers->motion.CopyTo12(guides.motion)) ||
           FAILED(buffers->hudless.CopyTo12(guides.hudless)) || FAILED(buffers->alpha.CopyTo12(buffers->ui.Get())))) {failed=true;return E_FAIL;}
        if(FAILED(hr=allocator->Reset()) || FAILED(hr=commands->Reset(allocator.Get(),nullptr)))return hr;
        bool guideStates=false;
        if(haveGuides) {
            unsigned beginResult=generation.BeginNativeFrame(nativeFrame,guides.camera);
            if(beginResult!=0){haveGuides=false;if(frame%120==0){
                Event("fg_native_frame_contract_rejected",static_cast<HRESULT>(beginResult));
                Ledger(generation,frame,nativeFrame);
            }}
            else {
                ID3D12Resource* tagged[]{buffers->depth.Texture12(),buffers->motion.Texture12(),buffers->hudless.Texture12(),buffers->alpha.Texture12()};
                for(auto p:tagged)Transition(commands.Get(),p,D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                GenerationResources inputs{commands.Get(),tagged[0],tagged[1],tagged[2],tagged[3],D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,true,true};
                guideStates=true;
                unsigned tagResult=generation.Tag(inputs);
                if(tagResult!=0){haveGuides=false;if(frame%120==0)Event("fg_native_resource_tag_rejected",static_cast<HRESULT>(tagResult));}
            }
        }
        // Commit the mode once, after current-frame token/camera/tag checks.
        // Enabling on provisional guides and then disabling on a rejected
        // ticket issued opposite options for the same provider frame, causing
        // the observed repeated-options warnings and needless resource churn.
        // Tags/constants are frame-scoped common inputs, independent of mode;
        // the final mode is committed before command execution and Present.
        unsigned modeResult=generation.SetActive(haveGuides);
        if(modeResult!=0){failed=true;Event("fg_native_mode_commit_failed",static_cast<HRESULT>(modeResult));return E_FAIL;}
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
        hr=display->Present(interval,flags);
        HRESULT displayResult=hr;
        if(indexedConsumer && SUCCEEDED(displayResult) && displayResult!=DXGI_STATUS_OCCLUDED)nativeIndex=(nativeIndex+1)%nativeDesc.BufferCount;
        auto state=generation.Presented();
        RecordPresentation(state,displayResult,haveGuides,haveMarker,nativeFrame);
        if(frame%120==0 && logger){char row[384];sprintf_s(row,"{\"event\":\"fg_native_runtime_state\",\"frame\":%u,\"result\":%u,\"status\":%u,\"maximum_generated_frames\":%u,\"last_presented\":%u,\"total_presented\":%llu,\"quality_verified\":false}\r\n",frame,state.result,state.status,state.maximumGeneratedFrames,state.lastPresented,state.totalPresented);logger(row);}
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
    HRESULT STDMETHODCALLTYPE GetBuffer(UINT index,REFIID id,void** out) override{if(!out)return E_POINTER;*out=nullptr;return index<nativeDesc.BufferCount?shadows[index]->QueryInterface(id,out):DXGI_ERROR_INVALID_CALL;}
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
    UINT STDMETHODCALLTYPE GetCurrentBackBufferIndex() override{indexedConsumer=true;return nativeIndex;}
    HRESULT STDMETHODCALLTYPE ResizeBuffers(UINT count,UINT w,UINT h,DXGI_FORMAT format,UINT flags) override {
        Operation operation(operationLock);if(!operation.entered)return DXGI_ERROR_WAS_STILL_DRAWING;
        auto rejected=[&](const char* reason,ULONG held=0)->HRESULT {
            if(logger){char row[512];sprintf_s(row,"{\"event\":\"fg_native_resize_rejected\",\"reason\":\"%s\",\"count\":%u,\"width\":%u,\"height\":%u,\"format\":%u,\"flags\":%u,\"native_flags\":%u,\"held_references\":%lu}\r\n",reason,count,w,h,format,flags,nativeDesc.Flags,held);logger(row);}return DXGI_ERROR_INVALID_CALL;
        };
        UINT desired=count?count:nativeDesc.BufferCount;
        bool flip=nativeDesc.SwapEffect==DXGI_SWAP_EFFECT_FLIP_DISCARD;
        if(desired!=(flip?2u:1u) || (flags&~(DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH|(flip?DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING:0))) ||
           ((flags^nativeDesc.Flags)&DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING))return rejected("layout_or_flags");
        for(UINT i=0;i<nativeDesc.BufferCount;++i){shadows[i]->AddRef();ULONG held=shadows[i]->Release();if(held!=1)return rejected("held_buffer",held);}
        RECT bounds{};if((!w || !h) && !GetClientRect(nativeDesc.OutputWindow,&bounds))return E_FAIL;
        if(!w)w=bounds.right-bounds.left;if(!h)h=bounds.bottom-bounds.top;if(format==DXGI_FORMAT_UNKNOWN)format=nativeDesc.BufferDesc.Format;
        if(w<128 || h<128 || w>4096 || h>4096 || format!=nativeDesc.BufferDesc.Format)return rejected("dimensions_or_format");
        ComPtr<ID3D11Texture2D> replacement[2];HRESULT hr=MakeShadows(w,h,format,desired,replacement);if(FAILED(hr))return hr;
        Internal nested;if(renderThread && FAILED(hr=Wait()))return hr;
        if(FAILED(hr=display->ResizeBuffers(2,w,h,format,flags))){Event("fg_private_display_resize_failed",hr);return hr;}
        for(UINT i=0;i<2;++i)shadows[i]=replacement[i];nativeIndex=0;
        nativeDesc.BufferCount=desired;nativeDesc.BufferDesc.Width=w;nativeDesc.BufferDesc.Height=h;nativeDesc.Flags=flags;
        // DXGI may resize on the window thread. Keep bridge retirement and
        // Streamline configuration on their established render owner; GPU
        // work is already complete and no Present can overlap this operation.
        pendingResize=true;presentationReset=true;Event("fg_native_resize_complete",S_OK);return S_OK;
    }
    HRESULT STDMETHODCALLTYPE ResizeBuffers1(UINT count,UINT w,UINT h,DXGI_FORMAT format,UINT flags,const UINT* masks,IUnknown* const* queues) override {
        if(masks || queues)return DXGI_ERROR_UNSUPPORTED;return ResizeBuffers(count,w,h,format,flags);
    }
    // Native descriptors describe the D3D11 shadow interface. The D3D12 flip
    // chain remains private, including its distinct backbuffer count.
    HRESULT STDMETHODCALLTYPE GetDesc1(DXGI_SWAP_CHAIN_DESC1* out) override {
        if(!out)return E_POINTER;HRESULT hr=display->GetDesc1(out);if(FAILED(hr))return hr;
        out->Width=nativeDesc.BufferDesc.Width;out->Height=nativeDesc.BufferDesc.Height;out->Format=nativeDesc.BufferDesc.Format;
        out->BufferCount=nativeDesc.BufferCount;out->SwapEffect=nativeDesc.SwapEffect;out->BufferUsage=nativeDesc.BufferUsage;out->Flags=nativeDesc.Flags;return S_OK;
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
    bool legacy=d.BufferCount==1 && d.SwapEffect==DXGI_SWAP_EFFECT_DISCARD;
    bool flip=d.BufferCount==2 && d.SwapEffect==DXGI_SWAP_EFFECT_FLIP_DISCARD;
    return d.OutputWindow && pid==GetCurrentProcessId() && (legacy || flip) &&
        d.SampleDesc.Count==1 && !d.SampleDesc.Quality && d.BufferDesc.Width>=128 && d.BufferDesc.Height>=128 &&
        d.BufferDesc.Width<=4096 && d.BufferDesc.Height<=4096 && d.BufferDesc.Format==DXGI_FORMAT_R10G10B10A2_UNORM &&
        !(d.Flags&~(DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH|(flip?DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING:0)));
}
HRESULT Wrap(IUnknown* device,const DXGI_SWAP_CHAIN_DESC& desc,IDXGISwapChain** out) {
    if(internal || !out || !Supported(desc))return DXGI_ERROR_UNSUPPORTED;
    ComPtr<ID3D11Device> native;if(FAILED(device->QueryInterface(IID_PPV_ARGS(&native))))return DXGI_ERROR_UNSUPPORTED;
    // The owned executable is packed: its native .rdata does not exist before
    // primary entry. Install native marker slots at actual chain creation,
    // after unpacking and before the engine's first simulation/render loop.
    if(!ConfigureNativeFrameMarkers(logger))return DXGI_ERROR_UNSUPPORTED;
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
bool NativePresentationOwnsNgxDevice(ID3D12Device* device) {
    AcquireSRWLockShared(&chainLock);bool owns=activeChain && activeChain->OwnsNgxDevice(device);ReleaseSRWLockShared(&chainLock);return owns;
}
bool InsideNativePresentation(){return internal;}
UINT NativePresentationBufferIndex(IDXGISwapChain* chain,unsigned frame) {
    return IsNativePresentation(chain)?static_cast<PresentationChain*>(chain)->ProducedBuffer(frame):0;
}
unsigned NativePresentationFactoryHookMask(IDXGISwapChain* chain) {
    if(!chain)return 0;ComPtr<IDXGIFactory> factory;
    if(FAILED(chain->GetParent(IID_PPV_ARGS(&factory))))return 0;
    auto table=*reinterpret_cast<void***>(factory.Get());unsigned result=4;
    if(table[10]==reinterpret_cast<void*>(OnCreate))result|=1;
    ComPtr<IDXGIFactory2> newer;if(SUCCEEDED(factory.As(&newer))) {
        auto extended=*reinterpret_cast<void***>(newer.Get());
        if(extended[15]==reinterpret_cast<void*>(OnCreateHwnd))result|=2;
    }
    return result;
}
HRESULT PresentNativePresentation(IDXGISwapChain* chain,UINT interval,UINT flags,unsigned frame) {
    return IsNativePresentation(chain)?static_cast<PresentationChain*>(chain)->DoPresent(interval,flags,frame):E_NOINTERFACE;
}
}
#endif
