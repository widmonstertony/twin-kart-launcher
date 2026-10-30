#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3dcompiler.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>
#include "NativeNeuralPass.h"
#include "NeuralD3D12.h"
#include "SharedTextureBridge.h"
#include "RenderPassTrace.h"
#include "ClientBuildGuard.h"
#include "NativeViewMath.h"
#include "NativeGuideShader.h"
#include "TemporalHistory.h"
#include "NativeTemporalPass.h"
#include "NativeFrameMarkers.h"
#include "NativeVelocity.h"
#include "NativeGuideSnapshot.h"
#include "NativeSceneRectangle.h"
using Microsoft::WRL::ComPtr;
namespace TwinKartGraphics {
namespace {
constexpr char PassHash[]="190895c92630ac60e123be317004c6c8fa033d514b006e470b69b80d994eb5fc";
constexpr char CompositeHash[]="e8d1965b9d9771dbff40fbb7273f3f9007d4f8a54e7a38e30a2e865d49aba14d";
std::wstring modulePath,providerPath,cachePath;
NativeNeuralLog logger=nullptr;
NativeNeuralCoreOwner externalCoreOwner=nullptr;
bool configured=false,disabled=false;
SRWLOCK runtimeLock=SRWLOCK_INIT;
struct RuntimeUse {
    bool entered=TryAcquireSRWLockExclusive(&runtimeLock)!=FALSE;
    ~RuntimeUse(){if(entered)ReleaseSRWLockExclusive(&runtimeLock);}
};
bool wantSuperResolution=false;
unsigned requestedNrPasses=1,requestedNrStyle=0;
unsigned guideAttemptFrame=0,guideAttempts=0,guideSuccesses=0;int guideRejectStage=0;
thread_local bool processing=false;
struct Processing { Processing(){processing=true;} ~Processing(){processing=false;} };
using Init=decltype(&TwinKartNeuralInitialize);
using Create=decltype(&TwinKartNeuralCreate);
using Configure=decltype(&TwinKartNeuralConfigure);
using ReadState=decltype(&TwinKartNeuralGetState);
using Evaluate=decltype(&TwinKartNeuralEvaluate);
using SuperCreate=decltype(&TwinKartSuperResolutionCreate);
using SuperEvaluate=decltype(&TwinKartSuperResolutionEvaluate);
using Shutdown=decltype(&TwinKartNeuralShutdown);
using ShutdownDrained=decltype(&TwinKartNeuralShutdownDrained);
struct ComputeState {
    ID3D11ComputeShader* shader=nullptr;
    ID3D11ClassInstance* instances[256]{}; UINT count=256;
    ID3D11ShaderResourceView* resources[128]{};
    ID3D11UnorderedAccessView* unordered[8]{};
    ID3D11Buffer* constants[14]{};
    ID3D11DeviceContext* context;
    explicit ComputeState(ID3D11DeviceContext* value):context(value) {
        context->CSGetShader(&shader,instances,&count);
        context->CSGetShaderResources(0,128,resources);
        context->CSGetUnorderedAccessViews(0,8,unordered);
        context->CSGetConstantBuffers(0,14,constants);
    }
    ~ComputeState() {
        ID3D11ShaderResourceView* empty[128]{}; ID3D11UnorderedAccessView* noWrites[8]{};
        context->CSSetShaderResources(0,128,empty); context->CSSetUnorderedAccessViews(0,8,noWrites,nullptr);
        context->CSSetShader(shader,instances,count); context->CSSetConstantBuffers(0,14,constants);
        context->CSSetShaderResources(0,128,resources); context->CSSetUnorderedAccessViews(0,8,unordered,nullptr);
        if(shader)shader->Release();
        for(UINT i=0;i<count;++i)if(instances[i])instances[i]->Release();
        for(auto p:resources)if(p)p->Release(); for(auto p:unordered)if(p)p->Release(); for(auto p:constants)if(p)p->Release();
    }
};
struct Runtime {
    ComPtr<ID3D11Device> device11;
    ComPtr<ID3D12Device> device12;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> commands;
    ComPtr<ID3D12Fence> fence;
    ComPtr<ID3D11ComputeShader> convert;
    ComPtr<ID3D11Buffer> rectangle;
    ComPtr<ID3D11Texture2D> depth,motion;
    ComPtr<ID3D11UnorderedAccessView> depthUav,motionUav;
    SharedTextureBridge colorBridge,depthBridge,motionBridge,outputBridge;
    SharedTextureBridge srColorBridge,srDepthBridge,srMotionBridge;
    ComPtr<ID3D11Texture2D> srColor,srDepth,srMotion;
    ComPtr<ID3D11UnorderedAccessView> srColorUav,srDepthUav,srMotionUav;
    Init init=nullptr; Create create=nullptr; Evaluate evaluate=nullptr;
    Configure configure=nullptr;ReadState readState=nullptr;
    SuperCreate superCreate=nullptr;SuperEvaluate superEvaluate=nullptr;
    Shutdown shutdown=nullptr;
    ShutdownDrained shutdownDrained=nullptr;
    unsigned inputWidth=0,inputHeight=0,lastSuperFrame=0;
    UINT64 superEvaluations=0;
    LARGE_INTEGER frequency{},lastTime{};
    CameraFrame camera{};bool cameraValid=false;unsigned cameraWidth=0,cameraHeight=0;
    TemporalHistory history;
    unsigned long long nativeFrame=0;bool nativeFrameObserved=false;
    unsigned width=0,height=0,nrWidth=0,nrHeight=0,lastFrame=0; UINT64 nextFence=0,evaluations=0;
    bool lowResolutionNeural=false;
    HMODULE module=nullptr; HANDLE completion=nullptr; DWORD owner=0;
    bool borrowedCore=false;
    // An address for same-frame identity comparison only, never dereferenced.
    // Keeping an AddRef here would prevent native ResizeBuffers from working.
    uintptr_t lastOutputIdentity=0;
    unsigned lastOutputFrame=0;
    bool Submit() {
        if(FAILED(commands->Close()))return false;
        ID3D12CommandList* lists[]{commands.Get()}; queue->ExecuteCommandLists(1,lists);
        if(FAILED(queue->Signal(fence.Get(),++nextFence)))return false;
        if(FAILED(fence->SetEventOnCompletion(nextFence,completion)))return false;
        return WaitForSingleObject(completion,10000)==WAIT_OBJECT_0;
    }
};
Runtime* runtime=nullptr; // Rebuilt only after owned GPU work is drained.
ComPtr<ID3D11ShaderResourceView> nativeDepth,nativeVelocity;
ComPtr<ID3D11Buffer> nativeView;
NativeTemporalContract nativeLayout{};
unsigned guideFrame=0;
void Event(const char* event,unsigned frame,unsigned value=0,void* target=nullptr) {
    if(!logger)return; char text[512];
    bool sr=runtime && frame && runtime->superEvaluations && runtime->lastSuperFrame==frame;
    sprintf_s(text,"{\"event\":\"%s\",\"frame\":%u,\"result\":%u,\"target_resource\":\"%p\",\"game_acceptance\":false,\"sr_evaluated\":%s,\"nr_generated_frames\":0}\r\n",event,frame,value,target,sr?"true":"false"); logger(text);
}
bool ValidateView(ID3D11DeviceContext* context,ID3D11Device* device,ID3D11Buffer* buffer,unsigned width,unsigned height,float* jitter=nullptr,CameraFrame* camera=nullptr,bool reset=false) {
    if(!buffer)return false; D3D11_BUFFER_DESC desc{}; buffer->GetDesc(&desc);
    if(desc.ByteWidth!=8192)return false;
    D3D11_BUFFER_DESC staging=desc; staging.Usage=D3D11_USAGE_STAGING; staging.BindFlags=0; staging.CPUAccessFlags=D3D11_CPU_ACCESS_READ; staging.MiscFlags=0;
    ComPtr<ID3D11Buffer> copy; if(FAILED(device->CreateBuffer(&staging,nullptr,&copy)))return false;
    context->CopyResource(copy.Get(),buffer); D3D11_MAPPED_SUBRESOURCE mapped{};
    if(FAILED(context->Map(copy.Get(),0,D3D11_MAP_READ,0,&mapped)))return false;
    float view[576]; memcpy(view,mapped.pData,sizeof(view)); context->Unmap(copy.Get(),0);
    for(float value:view)if(!std::isfinite(value))return false;
    // Exact native ViewSizeAndInvSize, inverted-Z perspective and temporal
    // jitter layout confirmed against two captured real race frames.
    bool valid=view[536]==static_cast<float>(width) && view[537]==static_cast<float>(height) &&
        fabsf(view[538]*width-1)<0.001f && fabsf(view[539]*height-1)<0.001f &&
        view[142]==10 && view[143]==0 && view[139]==1 && view[138]==0 &&
        fabsf(view[520])<0.01f && fabsf(view[521])<0.01f &&
        fabsf(view[519])>0.1f && fabsf(view[519])<10;
    // The supplied current/previous no-AA transforms must also agree with
    // ClipToPrevClip. Inversion is avoided: A*ClipToPrevClip must equal B.
    if(valid) {
        double current[4][4],previous[4][4];
        for(int r=0;r<4;++r)for(int c=0;c<4;++c) {
            current[r][c]=view[32+r*4+c]; previous[r][c]=view[316+r*4+c];
        }
        for(int r=0;r<4;++r) {
            current[r][0]-=current[r][3]*view[520]; current[r][1]-=current[r][3]*view[521];
            previous[r][0]-=previous[r][3]*view[522]; previous[r][1]-=previous[r][3]*view[523];
        }
        for(int r=0;r<4;++r)for(int c=0;c<4;++c) {
            double value=0; for(int k=0;k<4;++k)value+=current[r][k]*view[504+k*4+c];
            if(fabs(value-previous[r][c])>0.02*(1+fabs(previous[r][c])))valid=false;
        }
    }
    if(valid && jitter) {
        // Native NDC jitter converted to the actual low-resolution input's
        // pixel coordinates; not guessed from the display resolution.
        jitter[0]=-view[520]*width*0.5f;jitter[1]=view[521]*height*0.5f;
    }
    if(valid && camera)valid=DecodeNativeCamera(view,width,height,reset,*camera);
    return valid;
}
bool Initialize(Runtime& r,ID3D11DeviceContext* context,ID3D11Buffer* view,unsigned width,unsigned height,unsigned guideWidth,unsigned guideHeight,bool lowResolutionNeural) {
    context->GetDevice(&r.device11); r.owner=GetCurrentThreadId(); r.width=width; r.height=height;
    r.lowResolutionNeural=lowResolutionNeural;r.nrWidth=lowResolutionNeural?guideWidth:width;r.nrHeight=lowResolutionNeural?guideHeight:height;
    if(!ValidateView(context,r.device11.Get(),view,guideWidth,guideHeight)) { Event("nr_native_view_layout_rejected",0);return false; }
    ComPtr<IDXGIDevice> dxgi; ComPtr<IDXGIAdapter> adapter;
    if(FAILED(r.device11.As(&dxgi)) || FAILED(dxgi->GetAdapter(&adapter)) ||
       FAILED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&r.device12))))return false;
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    if(FAILED(r.device12->CreateCommandQueue(&queueDesc,IID_PPV_ARGS(&r.queue))) ||
       FAILED(r.device12->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&r.allocator))) ||
       FAILED(r.device12->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,r.allocator.Get(),nullptr,IID_PPV_ARGS(&r.commands))) ||
       FAILED(r.device12->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&r.fence))))return false;
    r.completion=CreateEventW(nullptr,FALSE,FALSE,nullptr); if(!r.completion)return false;
    r.module=LoadLibraryExW(modulePath.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS); if(!r.module)return false;
    r.init=reinterpret_cast<Init>(GetProcAddress(r.module,"TwinKartNeuralInitialize"));
    r.borrowedCore=externalCoreOwner && externalCoreOwner(r.device12.Get());
    if(r.borrowedCore)r.init=reinterpret_cast<Init>(GetProcAddress(r.module,"TwinKartNeuralInitializeBorrowed"));
    r.create=reinterpret_cast<Create>(GetProcAddress(r.module,"TwinKartNeuralCreate"));
    r.configure=reinterpret_cast<Configure>(GetProcAddress(r.module,"TwinKartNeuralConfigure"));
    r.readState=reinterpret_cast<ReadState>(GetProcAddress(r.module,"TwinKartNeuralGetState"));
    r.evaluate=reinterpret_cast<Evaluate>(GetProcAddress(r.module,"TwinKartNeuralEvaluate"));
    r.superCreate=reinterpret_cast<SuperCreate>(GetProcAddress(r.module,"TwinKartSuperResolutionCreate"));
    r.superEvaluate=reinterpret_cast<SuperEvaluate>(GetProcAddress(r.module,"TwinKartSuperResolutionEvaluate"));
    r.shutdown=reinterpret_cast<Shutdown>(GetProcAddress(r.module,"TwinKartNeuralShutdown"));
    r.shutdownDrained=reinterpret_cast<ShutdownDrained>(GetProcAddress(r.module,"TwinKartNeuralShutdownDrained"));
    if(!r.init || !r.create || !r.evaluate || !r.shutdown || !r.shutdownDrained || !r.configure || !r.readState)return false;
    unsigned initialized=r.init(r.device12.Get(),cachePath.c_str(),providerPath.c_str(),nullptr,true); Event("nr_real_game_initialize",0,initialized);
    if(initialized!=1)return false;
    if(!r.configure(requestedNrPasses,requestedNrStyle))return false;
    Event(r.borrowedCore?"nr_presentation_ngx_core_borrowed":"nr_standalone_ngx_core_owned",0,initialized);
    unsigned created=r.create(r.commands.Get(),r.nrWidth,r.nrHeight); Event("nr_real_game_create",0,created);
    if(!r.Submit() || created!=1)return false;
    ComPtr<ID3DBlob> code,errors;
    if(FAILED(D3DCompile(NativeGuideShader,sizeof(NativeGuideShader),nullptr,nullptr,nullptr,"main","cs_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&code,&errors)) ||
       FAILED(r.device11->CreateComputeShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&r.convert)))return false;
    D3D11_BUFFER_DESC rectangle{};rectangle.ByteWidth=32;rectangle.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    if(FAILED(r.device11->CreateBuffer(&rectangle,nullptr,&r.rectangle)))return false;
    D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=height;desc.MipLevels=1;desc.ArraySize=1;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_UNORDERED_ACCESS|D3D11_BIND_SHADER_RESOURCE;
    desc.Format=DXGI_FORMAT_R32_FLOAT;
    if(FAILED(r.device11->CreateTexture2D(&desc,nullptr,&r.depth)) || FAILED(r.device11->CreateUnorderedAccessView(r.depth.Get(),nullptr,&r.depthUav)))return false;
    desc.Format=DXGI_FORMAT_R16G16_FLOAT;
    if(FAILED(r.device11->CreateTexture2D(&desc,nullptr,&r.motion)) || FAILED(r.device11->CreateUnorderedAccessView(r.motion.Get(),nullptr,&r.motionUav)))return false;
    QueryPerformanceFrequency(&r.frequency);
    return SUCCEEDED(r.colorBridge.Initialize(r.device11.Get(),r.device12.Get(),r.queue.Get(),width,height,DXGI_FORMAT_R10G10B10A2_UNORM)) &&
        SUCCEEDED(r.depthBridge.Initialize(r.device11.Get(),r.device12.Get(),r.queue.Get(),width,height,DXGI_FORMAT_R32_FLOAT)) &&
        SUCCEEDED(r.motionBridge.Initialize(r.device11.Get(),r.device12.Get(),r.queue.Get(),width,height,DXGI_FORMAT_R16G16_FLOAT)) &&
        SUCCEEDED(r.outputBridge.Initialize(r.device11.Get(),r.device12.Get(),r.queue.Get(),r.nrWidth,r.nrHeight,DXGI_FORMAT_R10G10B10A2_UNORM));
}
bool PrepareSuperResolution(Runtime& r,unsigned guideWidth,unsigned guideHeight) {
    if(r.inputWidth)return r.inputWidth==guideWidth && r.inputHeight==guideHeight;
    if(!r.superCreate || !r.superEvaluate || FAILED(r.allocator->Reset()) || FAILED(r.commands->Reset(r.allocator.Get(),nullptr)))return false;
    unsigned created=r.superCreate(r.commands.Get(),guideWidth,guideHeight,r.width,r.height,2,false,true);
    bool submitted=r.Submit();Event("sr_native_feature_create",0,created);
    if(created!=1 || !submitted)return false;
    D3D11_TEXTURE2D_DESC desc{};desc.Width=guideWidth;desc.Height=guideHeight;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;
    desc.BindFlags=D3D11_BIND_UNORDERED_ACCESS|D3D11_BIND_SHADER_RESOURCE;
    desc.Format=DXGI_FORMAT_R32_FLOAT;
    if(FAILED(r.device11->CreateTexture2D(&desc,nullptr,&r.srDepth)) || FAILED(r.device11->CreateUnorderedAccessView(r.srDepth.Get(),nullptr,&r.srDepthUav)))return false;
    desc.Format=DXGI_FORMAT_R16G16_FLOAT;
    if(FAILED(r.device11->CreateTexture2D(&desc,nullptr,&r.srMotion)) || FAILED(r.device11->CreateUnorderedAccessView(r.srMotion.Get(),nullptr,&r.srMotionUav)))return false;
    desc.Format=DXGI_FORMAT_R10G10B10A2_UNORM;
    if(FAILED(r.device11->CreateTexture2D(&desc,nullptr,&r.srColor)) || FAILED(r.device11->CreateUnorderedAccessView(r.srColor.Get(),nullptr,&r.srColorUav)) ||
       FAILED(r.srColorBridge.Initialize(r.device11.Get(),r.device12.Get(),r.queue.Get(),guideWidth,guideHeight,DXGI_FORMAT_R10G10B10A2_UNORM)) ||
       FAILED(r.srDepthBridge.Initialize(r.device11.Get(),r.device12.Get(),r.queue.Get(),guideWidth,guideHeight,DXGI_FORMAT_R32_FLOAT)) ||
       FAILED(r.srMotionBridge.Initialize(r.device11.Get(),r.device12.Get(),r.queue.Get(),guideWidth,guideHeight,DXGI_FORMAT_R16G16_FLOAT)))return false;
    r.inputWidth=guideWidth;r.inputHeight=guideHeight;return true;
}
bool RetireRuntime(ID3D11DeviceContext* handoffContext=nullptr) {
    if(!runtime)return false;
    bool foreign=runtime->owner!=GetCurrentThreadId();
    if(foreign && !handoffContext){
        if(logger){char row[256];sprintf_s(row,"{\"event\":\"nr_runtime_retire_owner_rejected\",\"owner\":%lu,\"caller\":%lu}\r\n",runtime->owner,GetCurrentThreadId());logger(row);}return false;
    }
    auto& r=*runtime;
    if(!r.queue || !r.fence || !r.completion || !r.shutdownDrained)return false;
    if(FAILED(r.queue->Signal(r.fence.Get(),++r.nextFence)) || FAILED(r.fence->SetEventOnCompletion(r.nextFence,r.completion)) ||
       WaitForSingleObject(r.completion,10000)!=WAIT_OBJECT_0 || r.fence->GetCompletedValue()==~0ull)return false;
    SharedTextureBridge* bridges[]{&runtime->colorBridge,&runtime->depthBridge,&runtime->motionBridge,&runtime->outputBridge,
        &runtime->srColorBridge,&runtime->srDepthBridge,&runtime->srMotionBridge};
    for(unsigned i=0;i<ARRAYSIZE(bridges);++i)if(bridges[i]->Texture12()){
        HRESULT result=foreign?bridges[i]->DrainForRetirement(r.owner,handoffContext):bridges[i]->Drain();if(FAILED(result)){
            if(logger){char row[256];sprintf_s(row,"{\"event\":\"nr_runtime_retire_bridge_rejected\",\"bridge\":%u,\"hresult\":%ld}\r\n",i,result);logger(row);}return false;
        }
    }
    // Every NR/SR command has a completed fence; the presentation consumer
    // copies inputs into its own synchronized resources before frame end.
    if(!r.shutdownDrained(r.owner,r.fence.Get(),r.nextFence)){
        Event("nr_consumer_drained_shutdown_rejected",0);return false;
    }
    HMODULE module=runtime->module;HANDLE completion=runtime->completion;
    delete runtime;runtime=nullptr;
    if(completion)CloseHandle(completion);if(module)FreeLibrary(module);return true;
}
void Transition(ID3D12GraphicsCommandList* list,ID3D12Resource* resource,D3D12_RESOURCE_STATES from,D3D12_RESOURCE_STATES to) {
    D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition.pResource=resource;b.Transition.StateBefore=from;b.Transition.StateAfter=to;b.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;list->ResourceBarrier(1,&b);
}
bool Texture(ID3D11View* view,DXGI_FORMAT format,ComPtr<ID3D11Texture2D>& result,unsigned& width,unsigned& height,bool first=false) {
    if(!view)return false; ComPtr<ID3D11Resource> resource;view->GetResource(&resource);if(FAILED(resource.As(&result)))return false;
    D3D11_TEXTURE2D_DESC desc{};result->GetDesc(&desc);
    if(first){width=desc.Width;height=desc.Height;}
    return desc.Width==width && desc.Height==height && width>=128 && height>=128 && width<=4096 && height<=4096 && desc.SampleDesc.Count==1 && desc.ArraySize==1 && desc.MipLevels==1 && desc.Format==format;
}
bool ReadPatch(ID3D11DeviceContext* context,ID3D11Device* device,ID3D11Texture2D* texture,unsigned (&pixels)[1024]) {
    D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
    if(desc.Width<32 || desc.Height<32 || desc.Format!=DXGI_FORMAT_R10G10B10A2_UNORM)return false;
    unsigned x=desc.Width/2-16,y=desc.Height/2-16;
    desc.Width=desc.Height=32;desc.BindFlags=desc.MiscFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> copy;if(FAILED(device->CreateTexture2D(&desc,nullptr,&copy)))return false;
    D3D11_BOX box{x,y,0,x+32,y+32,1};context->CopySubresourceRegion(copy.Get(),0,0,0,0,texture,0,&box);
    D3D11_MAPPED_SUBRESOURCE mapped{};if(FAILED(context->Map(copy.Get(),0,D3D11_MAP_READ,0,&mapped)))return false;
    for(unsigned row=0;row<32;++row)memcpy(pixels+row*32,static_cast<const BYTE*>(mapped.pData)+row*mapped.RowPitch,128);
    context->Unmap(copy.Get(),0);return true;
}
}
bool ConfigureNativeNeuralPass(const wchar_t* module,const char* expectedModuleSha256,const wchar_t* provider,const wchar_t* cache,NativeNeuralLog log,NativeNeuralCoreOwner ownerCheck) {
    // Pin the independently built consumer to its reviewed private manifest,
    // rather than a single developer binary that changes on a clean rebuild.
    // The external model's separate exact-interface hash guard is unchanged.
    if(configured || !module || !provider || !cache || !*provider || !*cache || !TwinKartClient::FileHasHash(module,expectedModuleSha256))return false;
    wchar_t srFlag[8]{};GetEnvironmentVariableW(L"TWIN_KART_SUPER_RESOLUTION",srFlag,ARRAYSIZE(srFlag));
    wantSuperResolution=!wcscmp(srFlag,L"1");
    wchar_t passes[8]{},style[8]{};
    DWORD pn=GetEnvironmentVariableW(L"TWIN_KART_NR_PASSES",passes,ARRAYSIZE(passes));
    DWORD sn=GetEnvironmentVariableW(L"TWIN_KART_NR_STYLE",style,ARRAYSIZE(style));
    if(pn && (pn!=1 || (passes[0]!=L'1' && passes[0]!=L'2')))return false;
    if(sn && (sn!=1 || style[0]<L'0' || style[0]>L'2'))return false;
    requestedNrPasses=pn?passes[0]-L'0':1;requestedNrStyle=sn?style[0]-L'0':0;
    modulePath=module;providerPath=provider;cachePath=cache;logger=log;externalCoreOwner=ownerCheck;configured=true;return true;
}
bool ProcessNativeNeuralPass(ID3D11DeviceContext* context,unsigned frame) {
    if(processing || !context || !frame)return false;
    RuntimeUse use;if(!use.entered)return false;
    if(!configured || disabled)return false;
    ComPtr<ID3D11ComputeShader> shader;context->CSGetShader(&shader,nullptr,nullptr);char hash[65];
    if(!TwinKartRender::ShaderHash(shader.Get(),hash) || strcmp(hash,PassHash))return false;
    if(guideAttemptFrame!=frame){guideAttemptFrame=frame;guideAttempts=guideSuccesses=0;}++guideAttempts;guideRejectStage=1;
    // Reject a later invalid capture in this frame rather than reusing stale
    // guides from an earlier view (split views and pooled buffers can differ).
    nativeDepth.Reset();nativeVelocity.Reset();nativeView.Reset();guideFrame=0;
    ComPtr<ID3D11ShaderResourceView> depth,velocity;ComPtr<ID3D11Buffer> view,parameters;
    context->CSGetShaderResources(3,1,&depth);context->CSGetShaderResources(4,1,&velocity);
    context->CSGetConstantBuffers(0,1,&parameters);context->CSGetConstantBuffers(1,1,&view);
    ComPtr<ID3D11UnorderedAccessView> output;context->CSGetUnorderedAccessViews(0,1,&output);
    ComPtr<ID3D11Texture2D> depthTexture,outputTexture;unsigned iw=0,ih=0;
    if(!Texture(depth.Get(),DXGI_FORMAT_R32G8X24_TYPELESS,depthTexture,iw,ih,true) || !output)return false;
    guideRejectStage=2;
    D3D11_SHADER_RESOURCE_VIEW_DESC depthBinding{};depth->GetDesc(&depthBinding);
    if(depthBinding.ViewDimension!=D3D11_SRV_DIMENSION_TEXTURE2D || depthBinding.Format!=DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS ||
       depthBinding.Texture2D.MostDetailedMip || depthBinding.Texture2D.MipLevels!=1)return false;
    D3D11_UNORDERED_ACCESS_VIEW_DESC binding{};output->GetDesc(&binding);
    if(binding.ViewDimension!=D3D11_UAV_DIMENSION_TEXTURE2D || binding.Format!=DXGI_FORMAT_R16G16B16A16_FLOAT || binding.Texture2D.MipSlice)return false;
    guideRejectStage=3;
    ComPtr<ID3D11Resource> resource;output->GetResource(&resource);if(FAILED(resource.As(&outputTexture)))return false;
    D3D11_TEXTURE2D_DESC extent{};outputTexture->GetDesc(&extent);
    bool textureValid=extent.Format==DXGI_FORMAT_R16G16B16A16_FLOAT && extent.SampleDesc.Count==1 && extent.ArraySize==1 && extent.MipLevels==1;
    unsigned snapshotFailure=0;
    NativeGuideSnapshotFacts facts;
    if(!textureValid || !SnapshotNativeGuides(context,parameters.Get(),view.Get(),iw,ih,extent.Width,extent.Height,nativeLayout,nativeView,&snapshotFailure,&facts)) {
        if(logger && frame%120==0){char row[512];sprintf_s(row,"{\"event\":\"native_guide_snapshot_rejected\",\"frame\":%u,\"output\":[%u,%u],\"format\":%u,\"samples\":%u,\"array\":%u,\"mips\":%u,\"texture_valid\":%s,\"snapshot_failure\":%u}\r\n",
            frame,extent.Width,extent.Height,extent.Format,extent.SampleDesc.Count,extent.ArraySize,extent.MipLevels,textureValid?"true":"false",snapshotFailure);logger(row);
            if(snapshotFailure==3){NativeRect input{};memcpy(&input,facts.parameters+80,sizeof(input));
                char detail[1800];sprintf_s(detail,"{\"event\":\"native_guide_layout_rejected\",\"frame\":%u,\"input_allocation\":[%u,%u],\"inclusive_sample_bounds\":[%d,%d,%d,%d],\"output_bounds\":[%.9g,%.9g,%.9g,%.9g],\"view_extent\":[%.9g,%.9g],\"view_jitter\":[%.9g,%.9g],\"input_uv_mapping\":[%.9g,%.9g,%.9g,%.9g],\"input_buffer_size\":[%.9g,%.9g,%.9g,%.9g],\"output_view_size\":[%.9g,%.9g,%.9g,%.9g],\"exposure\":[%.9g,%.9g]}\r\n",
                    frame,iw,ih,input.left,input.top,input.right,input.bottom,facts.parameters[96],facts.parameters[97],facts.parameters[98],facts.parameters[99],
                    facts.viewExtent[0],facts.viewExtent[1],facts.viewJitter[0],facts.viewJitter[1],facts.parameters[60],facts.parameters[61],facts.parameters[62],facts.parameters[63],
                    facts.parameters[76],facts.parameters[77],facts.parameters[78],facts.parameters[79],facts.parameters[92],facts.parameters[93],facts.parameters[94],facts.parameters[95],facts.exposure[0],facts.exposure[1]);logger(detail);}
        }return false;
    }
    guideRejectStage=0;++guideSuccesses;
    nativeDepth=depth;nativeVelocity=velocity;guideFrame=frame;
    return false;
}
namespace {
// Qualified-resource processing is also exercised by the explicitly synthetic,
// offscreen GPU probe. The public game entry below retains its exact hash gate.
bool ProcessQualifiedNeuralPixelPassUnlocked(ID3D11DeviceContext* context,unsigned frame) {
    // A temporary loading/window-thread callback cannot retire the established
    // owner's features or touch its D3D11 state. Reject this callback without
    // permanently disabling reconstruction; the owner can resume/rebuild.
    if(runtime && runtime->owner!=GetCurrentThreadId()) {
        if(logger && frame%120==0){char row[320];sprintf_s(row,"{\"event\":\"native_reconstruction_foreign_owner_deferred\",\"frame\":%u,\"owner\":%lu,\"caller\":%lu,\"disabled\":false}\r\n",frame,runtime->owner,GetCurrentThreadId());logger(row);}
        return false;
    }
    Processing recursion; ComputeState saved(context);
    ComPtr<ID3D11RenderTargetView> outputView;context->OMGetRenderTargets(1,&outputView,nullptr);
    ComPtr<ID3D11Texture2D> depth,motion,target;unsigned width=0,height=0,allocationWidth=0,allocationHeight=0;
    bool cameraOnly=false;
    bool targetValid=Texture(outputView.Get(),DXGI_FORMAT_R10G10B10A2_UNORM,target,width,height,true);
    bool depthValid=Texture(nativeDepth.Get(),DXGI_FORMAT_R32G8X24_TYPELESS,depth,allocationWidth,allocationHeight,true);
    bool rectangleValid=depthValid && nativeLayout.input.Fits(allocationWidth,allocationHeight);
    bool velocityValid=rectangleValid && ReadNativeVelocity(context,nativeVelocity.Get(),static_cast<unsigned>(nativeLayout.input.right),static_cast<unsigned>(nativeLayout.input.bottom),motion,cameraOnly);
    if(!targetValid || !rectangleValid || !velocityValid) {
        if(logger && frame%120==0){char row[320];sprintf_s(row,"{\"event\":\"native_guide_resources_rejected\",\"frame\":%u,\"target\":%s,\"depth\":%s,\"rectangle\":%s,\"velocity\":%s}\r\n",frame,
            targetValid?"true":"false",depthValid?"true":"false",rectangleValid?"true":"false",velocityValid?"true":"false");logger(row);}return false;
    }
    const unsigned guideWidth=nativeLayout.input.Width(),guideHeight=nativeLayout.input.Height();
    ComPtr<ID3D11ShaderResourceView> scene;context->PSGetShaderResources(0,1,&scene);
    ComPtr<ID3D11Texture2D> sceneTexture;D3D11_TEXTURE2D_DESC sceneDesc{};D3D11_SHADER_RESOURCE_VIEW_DESC sceneBinding{};
    if(scene) {
        ComPtr<ID3D11Resource> source;scene->GetResource(&source);
        if(SUCCEEDED(source.As(&sceneTexture))) {sceneTexture->GetDesc(&sceneDesc);scene->GetDesc(&sceneBinding);}
    }
    D3D11_VIEWPORT viewport{};UINT viewportCount=1;context->RSGetViewports(&viewportCount,&viewport);
    NativeRect colorRectangle{};
    bool colorLayout=sceneTexture && NativeSceneRectangle(sceneTexture.Get(),context,frame,guideWidth,guideHeight,colorRectangle);
    const bool observedColorRectangle=colorLayout;
    if(!colorLayout && sceneDesc.Width==guideWidth && sceneDesc.Height==guideHeight) {
        colorRectangle={0,0,static_cast<int>(guideWidth),static_cast<int>(guideHeight)};colorLayout=true;
    }
    bool useSr=wantSuperResolution && !NativeTemporalSuperResolutionRan(frame) && sceneTexture && guideWidth<width && guideHeight<height &&
        nativeLayout.output.Width()==guideWidth && nativeLayout.output.Height()==guideHeight &&
        colorLayout && sceneDesc.SampleDesc.Count==1 && sceneDesc.ArraySize==1 && sceneDesc.MipLevels==1 &&
        sceneBinding.ViewDimension==D3D11_SRV_DIMENSION_TEXTURE2D && sceneBinding.Texture2D.MostDetailedMip==0 &&
        (sceneBinding.Format==DXGI_FORMAT_B8G8R8A8_UNORM || sceneBinding.Format==DXGI_FORMAT_R8G8B8A8_UNORM || sceneBinding.Format==DXGI_FORMAT_R10G10B10A2_UNORM) &&
        viewportCount==1 && viewport.TopLeftX==0 && viewport.TopLeftY==0 && viewport.Width==width && viewport.Height==height &&
        fabs(static_cast<double>(guideWidth)*height-static_cast<double>(guideHeight)*width)<=width;
    if(logger && frame%120==0) {
        char row[1024];sprintf_s(row,"{\"event\":\"native_reconstruction_layout\",\"frame\":%u,\"guide_allocation\":[%u,%u],\"native_input_rect\":[%d,%d,%d,%d],\"native_aa_output\":[%u,%u],\"color_allocation\":[%u,%u],\"color_rect\":[%d,%d,%d,%d],\"color_rect_observed_at_draw\":%s,\"output\":[%u,%u],\"resolved_sr_qualified\":%s,\"temporal_sr_ran\":%s,\"camera_only_velocity\":%s,\"source_pixels_resized\":false,\"game_acceptance\":false}\r\n",
            frame,allocationWidth,allocationHeight,nativeLayout.input.left,nativeLayout.input.top,nativeLayout.input.right,nativeLayout.input.bottom,
            nativeLayout.output.Width(),nativeLayout.output.Height(),sceneDesc.Width,sceneDesc.Height,
            colorRectangle.left,colorRectangle.top,colorRectangle.right,colorRectangle.bottom,observedColorRectangle?"true":"false",width,height,
            useSr?"true":"false",NativeTemporalSuperResolutionRan(frame)?"true":"false",cameraOnly?"true":"false");logger(row);
    }
    if(wantSuperResolution && !useSr && !NativeTemporalSuperResolutionRan(frame) && frame%120==0)Event("sr_native_lower_input_contract_missing",frame);
    const unsigned requiredNrWidth=useSr?guideWidth:width,requiredNrHeight=useSr?guideHeight:height;
    if(runtime && (runtime->width!=width || runtime->height!=height || runtime->nrWidth!=requiredNrWidth || runtime->nrHeight!=requiredNrHeight ||
       (useSr && runtime->inputWidth && (runtime->inputWidth!=guideWidth || runtime->inputHeight!=guideHeight)))) {
        if(!RetireRuntime()){disabled=true;Event("native_reconstruction_resize_drain_failed",frame);return false;}
        Event("native_reconstruction_resize_reset",frame);
    }
    if(!runtime) {
        runtime=new Runtime;
        if(!Initialize(*runtime,context,nativeView.Get(),width,height,guideWidth,guideHeight,useSr)) {disabled=true;Event("nr_native_pass_initialize_failed",frame);return false;}
        Event("nr_native_pass_initialized",frame);
    }
    auto& r=*runtime;
    if(r.owner!=GetCurrentThreadId() || r.width!=width || r.height!=height || r.lastFrame==frame)return false;
    // A later resource-size change cannot silently evaluate the wrong feature.
    if(useSr && !PrepareSuperResolution(r,guideWidth,guideHeight)) {
        disabled=true;Event("sr_native_prepare_failed",frame);return false;
    }
    float jitter[2]{};
    r.cameraValid=ValidateView(context,r.device11.Get(),nativeView.Get(),guideWidth,guideHeight,jitter,&r.camera,false);
    r.cameraWidth=guideWidth;r.cameraHeight=guideHeight;
    if(!r.cameraValid) {
        if(frame%120==0)Event("native_current_view_rejected",frame);return false;
    }
    using Revision=unsigned long long(WINAPI*)();
    HMODULE cockpit=GetModuleHandleW(L"TwinKartDriftCamera.dll");
    auto readRevision=cockpit?reinterpret_cast<Revision>(GetProcAddress(cockpit,"TwinKartCameraHistoryRevision")):nullptr;
    auto revision=readRevision?readRevision():0;
    bool reset=r.history.NeedsReset(frame,revision,r.camera);
    r.camera.reset=reset;
    r.lastFrame=frame;
    unsigned before[1024]{},after[1024]{};
    bool verify=r.evaluations<3 && ReadPatch(context,r.device11.Get(),target.Get(),before);
    ID3D11ShaderResourceView* empty[128]{}; ID3D11UnorderedAccessView* noWrites[8]{};
    context->CSSetShaderResources(0,128,empty);context->CSSetUnorderedAccessViews(0,8,noWrites,nullptr);
    ID3D11ShaderResourceView* guides[]{nativeDepth.Get(),nativeVelocity.Get()};ID3D11UnorderedAccessView* writes[]{r.depthUav.Get(),r.motionUav.Get()};
    ID3D11Buffer* guideBuffer=nativeView.Get();context->CSSetConstantBuffers(1,1,&guideBuffer);
    unsigned crop[]{static_cast<unsigned>(nativeLayout.input.left),static_cast<unsigned>(nativeLayout.input.top),guideWidth,guideHeight,
        static_cast<unsigned>(colorRectangle.left),static_cast<unsigned>(colorRectangle.top),guideWidth,guideHeight};
    context->UpdateSubresource(r.rectangle.Get(),0,nullptr,crop,0,0);
    ID3D11Buffer* rectangle=r.rectangle.Get();context->CSSetConstantBuffers(3,1,&rectangle);
    context->CSSetShader(r.convert.Get(),nullptr,0);context->CSSetShaderResources(0,2,guides);context->CSSetUnorderedAccessViews(0,2,writes,nullptr);
    context->Dispatch((width+7)/8,(height+7)/8,1);
    context->CSSetShaderResources(0,128,empty);context->CSSetUnorderedAccessViews(0,8,noWrites,nullptr);
    if(useSr) {
        ID3D11ShaderResourceView* srGuides[]{nativeDepth.Get(),nativeVelocity.Get(),scene.Get()};
        ID3D11UnorderedAccessView* srWrites[]{r.srDepthUav.Get(),r.srMotionUav.Get(),r.srColorUav.Get()};
        context->CSSetShaderResources(0,3,srGuides);context->CSSetUnorderedAccessViews(0,3,srWrites,nullptr);
        context->Dispatch((guideWidth+7)/8,(guideHeight+7)/8,1);
        context->CSSetShaderResources(0,128,empty);context->CSSetUnorderedAccessViews(0,8,noWrites,nullptr);
        if(FAILED(r.srColorBridge.CopyTo12(r.srColor.Get())) || FAILED(r.srDepthBridge.CopyTo12(r.srDepth.Get())) ||
           FAILED(r.srMotionBridge.CopyTo12(r.srMotion.Get()))) {disabled=true;Event("sr_native_handoff_failed",frame);return false;}
    }
    if(FAILED(r.colorBridge.CopyTo12(target.Get())) || FAILED(r.depthBridge.CopyTo12(r.depth.Get())) ||
       FAILED(r.motionBridge.CopyTo12(r.motion.Get())) || FAILED(r.outputBridge.CopyTo12(useSr?r.srColor.Get():target.Get())) ||
       FAILED(r.allocator->Reset()) || FAILED(r.commands->Reset(r.allocator.Get(),nullptr))) {disabled=true;Event("nr_native_resource_handoff_failed",frame);return false;}
    ID3D12Resource* fullInputs[]{r.colorBridge.Texture12(),r.depthBridge.Texture12(),r.motionBridge.Texture12()};
    ID3D12Resource* lowInputs[]{r.srColorBridge.Texture12(),r.srDepthBridge.Texture12(),r.srMotionBridge.Texture12()};
    ID3D12Resource** nrInputs=useSr?lowInputs:fullInputs;
    for(unsigned i=0;i<3;++i)Transition(r.commands.Get(),nrInputs[i],D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    LARGE_INTEGER now{};QueryPerformanceCounter(&now);
    float milliseconds=r.lastTime.QuadPart?static_cast<float>(1000.0*(now.QuadPart-r.lastTime.QuadPart)/r.frequency.QuadPart):16.666667f;
    r.lastTime=now;
    auto output=r.outputBridge.Texture12();Transition(r.commands.Get(),output,D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    TwinKartNeuralFrame request{sizeof(TwinKartNeuralFrame),r.commands.Get(),nrInputs[0],nrInputs[1],nrInputs[2],output,1,1,1,reset,true,true};
    unsigned nrResult=r.evaluate(&request),srResult=0;
    if(useSr && nrResult==1) {
        // Both NR features run on the genuine lower-resolution scene and
        // guides. Only their refined result is reconstructed to the display.
        // This keeps every requested pass while avoiding two full-4K NR runs.
        Transition(r.commands.Get(),output,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        Transition(r.commands.Get(),fullInputs[0],D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        // This exact native source has already been tone mapped. Exposure is
        // baked into its SDR samples; do not apply the HDR pre-exposure again.
        TwinKartSuperResolutionFrame srRequest{sizeof(TwinKartSuperResolutionFrame),r.commands.Get(),output,lowInputs[1],lowInputs[2],fullInputs[0],0,0,1,1,milliseconds,1,1,reset || frame!=r.lastSuperFrame+1,true,true};
        srResult=r.superEvaluate(&srRequest);
        Transition(r.commands.Get(),fullInputs[0],D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COMMON);
        Transition(r.commands.Get(),output,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COMMON);
    } else Transition(r.commands.Get(),output,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COMMON);
    unsigned result=useSr?(nrResult==1?srResult:nrResult):nrResult;
    TwinKartNeuralState nrState{sizeof(TwinKartNeuralState)};
    bool nrStateRead=r.readState(&nrState);
    for(unsigned i=0;i<3;++i)Transition(r.commands.Get(),nrInputs[i],D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COMMON);
    bool submitted=r.Submit();
    bool returned=SUCCEEDED(r.colorBridge.ReturnTo11(useSr && submitted && result==1?target.Get():nullptr)) &&
        SUCCEEDED(r.depthBridge.ReturnTo11(nullptr)) && SUCCEEDED(r.motionBridge.ReturnTo11(nullptr)) &&
        SUCCEEDED(r.outputBridge.ReturnTo11(!useSr && submitted && result==1?target.Get():nullptr));
    if(useSr) {
        bool srReturned=SUCCEEDED(r.srColorBridge.ReturnTo11(nullptr)) && SUCCEEDED(r.srDepthBridge.ReturnTo11(nullptr)) && SUCCEEDED(r.srMotionBridge.ReturnTo11(nullptr));
        returned=returned && srReturned;
    }
    if(!submitted || !returned || result!=1) {disabled=true;Event("nr_native_evaluate_failed",frame,result);return false;}
    if(logger && (r.evaluations<3 || !(r.evaluations%120))) {
        char line[768];sprintf_s(line,"{\"event\":\"nr_pass_chain_gpu_completed\",\"frame\":%u,\"state_read\":%s,\"requested_passes\":%u,\"style\":%u,\"pass_results\":[%u,%u],\"pass_evaluations\":[%llu,%llu],\"gpu_completed\":true,\"quality_verified\":false}\r\n",
            frame,nrStateRead?"true":"false",requestedNrPasses,requestedNrStyle,nrState.passResults[0],nrState.passResults[1],nrState.passEvaluations[0],nrState.passEvaluations[1]);logger(line);
    }
    ++r.evaluations;
    r.history.Commit(frame,revision,r.camera);
    r.nativeFrameObserved=CurrentNativeRenderFrame(r.nativeFrame);
    if(useSr && srResult==1) {
        ++r.superEvaluations;r.lastSuperFrame=frame;
        if(r.superEvaluations<=3 || r.superEvaluations%120==0)Event("nr_low_resolution_output_submitted_to_sr",frame,srResult,target.Get());
    }
    ComPtr<IUnknown> identity;target.As(&identity);
    r.lastOutputIdentity=reinterpret_cast<uintptr_t>(identity.Get());r.lastOutputFrame=frame;
    if(verify && ReadPatch(context,r.device11.Get(),target.Get(),after) && logger) {
        unsigned changed=0;unsigned long long inputHash=14695981039346656037ull,outputHash=inputHash;
        for(unsigned i=0;i<1024;++i) {if(before[i]!=after[i])++changed;inputHash=(inputHash^before[i])*1099511628211ull;outputHash=(outputHash^after[i])*1099511628211ull;}
        char text[512];sprintf_s(text,"{\"event\":\"nr_native_output_gpu_readback\",\"frame\":%u,\"pixels\":1024,\"changed_pixels\":%u,\"input_hash\":\"%016llx\",\"output_hash\":\"%016llx\",\"gpu_completed\":true,\"quality_verified\":false}\r\n",frame,changed,inputHash,outputHash);logger(text);
    }
    if(r.evaluations<=3 || !(r.evaluations%120))Event("nr_pre_hud_output_copy_submitted",frame,result,target.Get());
    return true;
}
bool ProcessQualifiedNeuralPixelPass(ID3D11DeviceContext* context,unsigned frame) {
    RuntimeUse use;return use.entered && configured && !disabled && context && frame && ProcessQualifiedNeuralPixelPassUnlocked(context,frame);
}
bool OutputMatchesUnlocked(ID3D11Resource* resource,unsigned frame) {
    if(!runtime || disabled || runtime->owner!=GetCurrentThreadId() || !resource || runtime->lastOutputFrame!=frame || !runtime->lastOutputIdentity)return false;
    ComPtr<IUnknown> identity;return SUCCEEDED(resource->QueryInterface(IID_PPV_ARGS(&identity))) && reinterpret_cast<uintptr_t>(identity.Get())==runtime->lastOutputIdentity;
}
}
bool ProcessNativeNeuralPixelPass(ID3D11DeviceContext* context,unsigned frame) {
    if(processing || !context || !frame)return false;
    RuntimeUse use;if(!use.entered)return false;
    if(!configured || disabled)return false;
    bool ready=guideFrame==frame && nativeView;
    if(!ready && frame%120)return false;
    ComPtr<ID3D11PixelShader> shader;context->PSGetShader(&shader,nullptr,nullptr);char hash[65];
    if(!TwinKartRender::ShaderHash(shader.Get(),hash) || strcmp(hash,CompositeHash))return false;
    if(!ready) {
        if(logger){char row[320];sprintf_s(row,"{\"event\":\"native_composite_guides_unavailable\",\"frame\":%u,\"guide_frame\":%u,\"attempt_frame\":%u,\"attempts\":%u,\"successful_snapshots\":%u,\"last_reject_stage\":%d}\r\n",
            frame,guideFrame,guideAttemptFrame,guideAttempts,guideSuccesses,guideRejectStage);logger(row);}return false;
    }
    return ProcessQualifiedNeuralPixelPassUnlocked(context,frame);
}
bool NativeNeuralOutputMatches(ID3D11Resource* resource,unsigned frame) {
    RuntimeUse use;return use.entered && OutputMatchesUnlocked(resource,frame);
}
bool NativeSuperResolutionOutputMatches(ID3D11Resource* resource,unsigned frame) {
    RuntimeUse use;return use.entered && runtime && runtime->lastSuperFrame==frame && OutputMatchesUnlocked(resource,frame);
}
bool GetNativePresentationInputs(unsigned frame,NativePresentationInputs& inputs) {
    RuntimeUse use;if(!use.entered)return false;
    if(!runtime || disabled || runtime->owner!=GetCurrentThreadId() || !runtime->cameraValid || runtime->lastOutputFrame!=frame)return false;
    inputs.depth=runtime->depth.Get();inputs.motion=runtime->motion.Get();
    inputs.hudless=runtime->lowResolutionNeural?runtime->colorBridge.Texture11():runtime->outputBridge.Texture11();
    inputs.camera=ResolvedPresentationCamera(runtime->camera,runtime->width,runtime->height);
    inputs.nativeFrame=runtime->nativeFrame;inputs.nativeFrameObserved=runtime->nativeFrameObserved;
    return inputs.depth && inputs.motion && inputs.hudless;
}
bool RetireNativeNeuralForRenderThread(ID3D11DeviceContext* context) {
    if(!context || context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return false;
    RuntimeUse use;if(!use.entered)return false;
    if(!runtime)return true;
    if(disabled)return false;
    if(runtime->owner==GetCurrentThreadId())return true;
    ComPtr<ID3D11DeviceContext> immediate;runtime->device11->GetImmediateContext(&immediate);
    if(immediate.Get()!=context)return false;
    DWORD oldOwner=runtime->owner;
    if(!RetireRuntime(context))return false;
    nativeDepth.Reset();nativeVelocity.Reset();nativeView.Reset();guideFrame=0;
    guideAttemptFrame=guideAttempts=guideSuccesses=0;guideRejectStage=0;
    if(logger){char row[320];sprintf_s(row,"{\"event\":\"native_reconstruction_owner_retired\",\"old_owner\":%lu,\"next_owner\":%lu,\"gpu_drained\":true,\"next_frame_rebuild_required\":true,\"game_acceptance\":false}\r\n",oldOwner,GetCurrentThreadId());logger(row);}
    return true;
}
bool RetireNativeNeuralForDeviceShutdown(ID3D11DeviceContext* context) {
    if(!context || context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return false;
    // The presentation owner has stopped calling us. Wait for the final
    // admitted draw instead of destroying its shared NGX instance underneath it.
    AcquireSRWLockExclusive(&runtimeLock);
    struct Release {~Release(){ReleaseSRWLockExclusive(&runtimeLock);}} release;
    if(!runtime)return true;
    ComPtr<ID3D11DeviceContext> immediate;runtime->device11->GetImmediateContext(&immediate);
    if(immediate.Get()!=context || !RetireRuntime(context))return false;
    nativeDepth.Reset();nativeVelocity.Reset();nativeView.Reset();guideFrame=0;
    guideAttemptFrame=guideAttempts=guideSuccesses=0;guideRejectStage=0;
    Event("nr_retired_before_presentation_core_shutdown",0);return true;
}
void FinishNativeNeuralFrame(unsigned frame) {
    RuntimeUse use;if(!use.entered)return;
    if(guideFrame==frame) {
        nativeDepth.Reset();nativeVelocity.Reset();nativeView.Reset();guideFrame=0;
    }
    if(runtime && runtime->lastOutputFrame==frame) {
        runtime->lastOutputIdentity=0;runtime->lastOutputFrame=0;
    }
}
}
