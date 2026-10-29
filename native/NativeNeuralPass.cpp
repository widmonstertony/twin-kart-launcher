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
using Microsoft::WRL::ComPtr;
namespace TwinKartGraphics {
namespace {
constexpr char PassHash[]="190895c92630ac60e123be317004c6c8fa033d514b006e470b69b80d994eb5fc";
constexpr char CompositeHash[]="e8d1965b9d9771dbff40fbb7273f3f9007d4f8a54e7a38e30a2e865d49aba14d";
constexpr char ModuleHash[]="8df8fb48f60cc5469d54f6c34f11e0402537de8c0e47c69132ce246794233201";
std::wstring modulePath,providerPath,cachePath;
NativeNeuralLog logger=nullptr;
bool configured=false,disabled=false;
bool wantSuperResolution=false;
thread_local bool processing=false;
struct Processing { Processing(){processing=true;} ~Processing(){processing=false;} };
using Init=decltype(&TwinKartNeuralInitialize);
using Create=decltype(&TwinKartNeuralCreate);
using Evaluate=decltype(&TwinKartNeuralEvaluate);
using SuperCreate=decltype(&TwinKartSuperResolutionCreate);
using SuperEvaluate=decltype(&TwinKartSuperResolutionEvaluate);
using Shutdown=decltype(&TwinKartNeuralShutdown);
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
    ComPtr<ID3D11Texture2D> depth,motion;
    ComPtr<ID3D11UnorderedAccessView> depthUav,motionUav;
    SharedTextureBridge colorBridge,depthBridge,motionBridge,outputBridge;
    SharedTextureBridge srColorBridge,srDepthBridge,srMotionBridge;
    ComPtr<ID3D11Texture2D> srColor,srDepth,srMotion;
    ComPtr<ID3D11UnorderedAccessView> srColorUav,srDepthUav,srMotionUav;
    Init init=nullptr; Create create=nullptr; Evaluate evaluate=nullptr;
    SuperCreate superCreate=nullptr;SuperEvaluate superEvaluate=nullptr;
    Shutdown shutdown=nullptr;
    unsigned inputWidth=0,inputHeight=0,lastSuperFrame=0;
    UINT64 superEvaluations=0;
    LARGE_INTEGER frequency{},lastTime{};
    CameraFrame camera{};bool cameraValid=false;unsigned cameraWidth=0,cameraHeight=0;
    TemporalHistory history;
    unsigned long long nativeFrame=0;bool nativeFrameObserved=false;
    unsigned width=0,height=0,lastFrame=0; UINT64 nextFence=0,evaluations=0;
    HMODULE module=nullptr; HANDLE completion=nullptr; DWORD owner=0;
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
bool Initialize(Runtime& r,ID3D11DeviceContext* context,ID3D11Buffer* view,unsigned width,unsigned height,unsigned guideWidth,unsigned guideHeight) {
    context->GetDevice(&r.device11); r.owner=GetCurrentThreadId(); r.width=width; r.height=height;
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
    r.create=reinterpret_cast<Create>(GetProcAddress(r.module,"TwinKartNeuralCreate"));
    r.evaluate=reinterpret_cast<Evaluate>(GetProcAddress(r.module,"TwinKartNeuralEvaluate"));
    r.superCreate=reinterpret_cast<SuperCreate>(GetProcAddress(r.module,"TwinKartSuperResolutionCreate"));
    r.superEvaluate=reinterpret_cast<SuperEvaluate>(GetProcAddress(r.module,"TwinKartSuperResolutionEvaluate"));
    r.shutdown=reinterpret_cast<Shutdown>(GetProcAddress(r.module,"TwinKartNeuralShutdown"));
    if(!r.init || !r.create || !r.evaluate || !r.shutdown)return false;
    unsigned initialized=r.init(r.device12.Get(),cachePath.c_str(),providerPath.c_str(),nullptr,true); Event("nr_real_game_initialize",0,initialized);
    if(initialized!=1)return false;
    unsigned created=r.create(r.commands.Get(),width,height); Event("nr_real_game_create",0,created);
    if(!r.Submit() || created!=1)return false;
    ComPtr<ID3DBlob> code,errors;
    if(FAILED(D3DCompile(NativeGuideShader,sizeof(NativeGuideShader),nullptr,nullptr,nullptr,"main","cs_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&code,&errors)) ||
       FAILED(r.device11->CreateComputeShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&r.convert)))return false;
    D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=height;desc.MipLevels=1;desc.ArraySize=1;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_UNORDERED_ACCESS|D3D11_BIND_SHADER_RESOURCE;
    desc.Format=DXGI_FORMAT_R32_FLOAT;
    if(FAILED(r.device11->CreateTexture2D(&desc,nullptr,&r.depth)) || FAILED(r.device11->CreateUnorderedAccessView(r.depth.Get(),nullptr,&r.depthUav)))return false;
    desc.Format=DXGI_FORMAT_R16G16_FLOAT;
    if(FAILED(r.device11->CreateTexture2D(&desc,nullptr,&r.motion)) || FAILED(r.device11->CreateUnorderedAccessView(r.motion.Get(),nullptr,&r.motionUav)))return false;
    QueryPerformanceFrequency(&r.frequency);
    return SUCCEEDED(r.colorBridge.Initialize(r.device11.Get(),r.device12.Get(),r.queue.Get(),width,height,DXGI_FORMAT_R10G10B10A2_UNORM)) &&
        SUCCEEDED(r.depthBridge.Initialize(r.device11.Get(),r.device12.Get(),r.queue.Get(),width,height,DXGI_FORMAT_R32_FLOAT)) &&
        SUCCEEDED(r.motionBridge.Initialize(r.device11.Get(),r.device12.Get(),r.queue.Get(),width,height,DXGI_FORMAT_R16G16_FLOAT)) &&
        SUCCEEDED(r.outputBridge.Initialize(r.device11.Get(),r.device12.Get(),r.queue.Get(),width,height,DXGI_FORMAT_R10G10B10A2_UNORM));
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
bool RetireRuntime() {
    if(!runtime || runtime->owner!=GetCurrentThreadId())return false;
    SharedTextureBridge* bridges[]{&runtime->colorBridge,&runtime->depthBridge,&runtime->motionBridge,&runtime->outputBridge,
        &runtime->srColorBridge,&runtime->srDepthBridge,&runtime->srMotionBridge};
    for(auto p:bridges)if(p->Texture12() && FAILED(p->Drain()))return false;
    // Every NR/SR command has a completed fence; the presentation consumer
    // copies inputs into its own synchronized resources before frame end.
    runtime->shutdown();HMODULE module=runtime->module;HANDLE completion=runtime->completion;
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
bool ConfigureNativeNeuralPass(const wchar_t* module,const wchar_t* provider,const wchar_t* cache,NativeNeuralLog log) {
    if(configured || !module || !provider || !cache || !*provider || !*cache || !TwinKartClient::FileHasHash(module,ModuleHash))return false;
    wchar_t srFlag[8]{};GetEnvironmentVariableW(L"TWIN_KART_SUPER_RESOLUTION",srFlag,ARRAYSIZE(srFlag));
    wantSuperResolution=!wcscmp(srFlag,L"1");
    modulePath=module;providerPath=provider;cachePath=cache;logger=log;configured=true;return true;
}
bool ProcessNativeNeuralPass(ID3D11DeviceContext* context,unsigned frame) {
    if(!configured || disabled || processing || !context || !frame)return false;
    ComPtr<ID3D11ComputeShader> shader;context->CSGetShader(&shader,nullptr,nullptr);char hash[65];
    if(!TwinKartRender::ShaderHash(shader.Get(),hash) || strcmp(hash,PassHash))return false;
    ID3D11ShaderResourceView* depth=nullptr; ID3D11ShaderResourceView* velocity=nullptr; ID3D11Buffer* view=nullptr;
    context->CSGetShaderResources(3,1,&depth);context->CSGetShaderResources(4,1,&velocity);context->CSGetConstantBuffers(1,1,&view);
    nativeDepth.Attach(depth);nativeVelocity.Attach(velocity);nativeView.Attach(view);guideFrame=frame;
    return false;
}
bool ProcessNativeNeuralPixelPass(ID3D11DeviceContext* context,unsigned frame) {
    if(!configured || disabled || processing || !context || !frame || guideFrame!=frame || !nativeView)return false;
    ComPtr<ID3D11PixelShader> shader;context->PSGetShader(&shader,nullptr,nullptr);char hash[65];
    if(!TwinKartRender::ShaderHash(shader.Get(),hash) || strcmp(hash,CompositeHash))return false;
    Processing recursion; ComputeState saved(context);
    ComPtr<ID3D11RenderTargetView> outputView;context->OMGetRenderTargets(1,&outputView,nullptr);
    ComPtr<ID3D11Texture2D> depth,motion,target;unsigned width=0,height=0,guideWidth=0,guideHeight=0;
    if(!Texture(outputView.Get(),DXGI_FORMAT_R10G10B10A2_UNORM,target,width,height,true) ||
       !Texture(nativeDepth.Get(),DXGI_FORMAT_R32G8X24_TYPELESS,depth,guideWidth,guideHeight,true) ||
       !Texture(nativeVelocity.Get(),DXGI_FORMAT_R16G16B16A16_UNORM,motion,guideWidth,guideHeight))return false;
    ComPtr<ID3D11ShaderResourceView> scene;context->PSGetShaderResources(0,1,&scene);
    ComPtr<ID3D11Texture2D> sceneTexture;D3D11_TEXTURE2D_DESC sceneDesc{};D3D11_SHADER_RESOURCE_VIEW_DESC sceneBinding{};
    if(scene) {
        ComPtr<ID3D11Resource> source;scene->GetResource(&source);
        if(SUCCEEDED(source.As(&sceneTexture))) {sceneTexture->GetDesc(&sceneDesc);scene->GetDesc(&sceneBinding);}
    }
    D3D11_VIEWPORT viewport{};UINT viewportCount=1;context->RSGetViewports(&viewportCount,&viewport);
    bool useSr=wantSuperResolution && !NativeTemporalSuperResolutionRan(frame) && sceneTexture && guideWidth<width && guideHeight<height &&
        sceneDesc.Width==guideWidth && sceneDesc.Height==guideHeight && sceneDesc.SampleDesc.Count==1 && sceneDesc.ArraySize==1 && sceneDesc.MipLevels==1 &&
        sceneBinding.ViewDimension==D3D11_SRV_DIMENSION_TEXTURE2D && sceneBinding.Texture2D.MostDetailedMip==0 &&
        (sceneBinding.Format==DXGI_FORMAT_B8G8R8A8_UNORM || sceneBinding.Format==DXGI_FORMAT_R8G8B8A8_UNORM || sceneBinding.Format==DXGI_FORMAT_R10G10B10A2_UNORM) &&
        viewportCount==1 && viewport.TopLeftX==0 && viewport.TopLeftY==0 && viewport.Width==width && viewport.Height==height &&
        fabs(static_cast<double>(guideWidth)*height-static_cast<double>(guideHeight)*width)<=width;
    if(wantSuperResolution && !useSr && !NativeTemporalSuperResolutionRan(frame) && frame%120==0)Event("sr_native_lower_input_contract_missing",frame);
    if(runtime && (runtime->width!=width || runtime->height!=height ||
       (useSr && runtime->inputWidth && (runtime->inputWidth!=guideWidth || runtime->inputHeight!=guideHeight)))) {
        if(!RetireRuntime()){disabled=true;Event("native_reconstruction_resize_drain_failed",frame);return false;}
        Event("native_reconstruction_resize_reset",frame);
    }
    if(!runtime) {
        runtime=new Runtime;
        if(!Initialize(*runtime,context,nativeView.Get(),width,height,guideWidth,guideHeight)) {disabled=true;Event("nr_native_pass_initialize_failed",frame);return false;}
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
       FAILED(r.motionBridge.CopyTo12(r.motion.Get())) || FAILED(r.outputBridge.CopyTo12(target.Get())) ||
       FAILED(r.allocator->Reset()) || FAILED(r.commands->Reset(r.allocator.Get(),nullptr))) {disabled=true;Event("nr_native_resource_handoff_failed",frame);return false;}
    ID3D12Resource* inputs[]{r.colorBridge.Texture12(),r.depthBridge.Texture12(),r.motionBridge.Texture12()};
    for(unsigned i=0;i<3;++i)Transition(r.commands.Get(),inputs[i],D3D12_RESOURCE_STATE_COMMON,i==0 && useSr?D3D12_RESOURCE_STATE_UNORDERED_ACCESS:D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    LARGE_INTEGER now{};QueryPerformanceCounter(&now);
    float milliseconds=r.lastTime.QuadPart?static_cast<float>(1000.0*(now.QuadPart-r.lastTime.QuadPart)/r.frequency.QuadPart):16.666667f;
    r.lastTime=now;
    unsigned srResult=0;
    if(useSr) {
        ID3D12Resource* srInputs[]{r.srColorBridge.Texture12(),r.srDepthBridge.Texture12(),r.srMotionBridge.Texture12()};
        for(auto p:srInputs)Transition(r.commands.Get(),p,D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        // This exact native source has already been tone mapped. Exposure is
        // baked into its SDR samples; do not apply the HDR pre-exposure again.
        // Native AA has resolved projection jitter in this specific color
        // source. The converted guides are aligned to its unjittered grid.
        TwinKartSuperResolutionFrame srRequest{sizeof(TwinKartSuperResolutionFrame),r.commands.Get(),srInputs[0],srInputs[1],srInputs[2],inputs[0],0,0,1,1,milliseconds,1,1,reset || frame!=r.lastSuperFrame+1,true,true};
        srResult=r.superEvaluate(&srRequest);
        for(auto p:srInputs)Transition(r.commands.Get(),p,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COMMON);
        Transition(r.commands.Get(),inputs[0],D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    }
    auto output=r.outputBridge.Texture12();Transition(r.commands.Get(),output,D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    TwinKartNeuralFrame request{sizeof(TwinKartNeuralFrame),r.commands.Get(),inputs[0],inputs[1],inputs[2],output,1,1,1,reset,true,true};
    unsigned result=(!useSr || srResult==1)?r.evaluate(&request):srResult;
    for(auto p:inputs)Transition(r.commands.Get(),p,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COMMON);
    Transition(r.commands.Get(),output,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COMMON);
    bool submitted=r.Submit();
    bool returned=SUCCEEDED(r.colorBridge.ReturnTo11(nullptr)) && SUCCEEDED(r.depthBridge.ReturnTo11(nullptr)) && SUCCEEDED(r.motionBridge.ReturnTo11(nullptr)) &&
        SUCCEEDED(r.outputBridge.ReturnTo11(submitted && result==1?target.Get():nullptr));
    if(useSr) {
        bool srReturned=SUCCEEDED(r.srColorBridge.ReturnTo11(nullptr)) && SUCCEEDED(r.srDepthBridge.ReturnTo11(nullptr)) && SUCCEEDED(r.srMotionBridge.ReturnTo11(nullptr));
        returned=returned && srReturned;
    }
    if(!submitted || !returned || result!=1) {disabled=true;Event("nr_native_evaluate_failed",frame,result);return false;}
    ++r.evaluations;
    r.history.Commit(frame,revision,r.camera);
    r.nativeFrameObserved=CurrentNativeRenderFrame(r.nativeFrame);
    if(useSr && srResult==1) {
        ++r.superEvaluations;r.lastSuperFrame=frame;
        if(r.superEvaluations<=3 || r.superEvaluations%120==0)Event("sr_native_output_submitted_to_nr",frame,srResult,target.Get());
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
bool NativeNeuralOutputMatches(ID3D11Resource* resource,unsigned frame) {
    if(!runtime || disabled || !resource || runtime->lastOutputFrame!=frame || !runtime->lastOutputIdentity)return false;
    ComPtr<IUnknown> identity;return SUCCEEDED(resource->QueryInterface(IID_PPV_ARGS(&identity))) && reinterpret_cast<uintptr_t>(identity.Get())==runtime->lastOutputIdentity;
}
bool NativeSuperResolutionOutputMatches(ID3D11Resource* resource,unsigned frame) {
    return runtime && runtime->lastSuperFrame==frame && NativeNeuralOutputMatches(resource,frame);
}
bool GetNativePresentationInputs(unsigned frame,NativePresentationInputs& inputs) {
    if(!runtime || disabled || runtime->owner!=GetCurrentThreadId() || !runtime->cameraValid || runtime->lastOutputFrame!=frame)return false;
    inputs.depth=runtime->depth.Get();inputs.motion=runtime->motion.Get();inputs.hudless=runtime->outputBridge.Texture11();
    inputs.camera=ResolvedPresentationCamera(runtime->camera,runtime->width,runtime->height);
    inputs.nativeFrame=runtime->nativeFrame;inputs.nativeFrameObserved=runtime->nativeFrameObserved;
    return inputs.depth && inputs.motion && inputs.hudless;
}
void FinishNativeNeuralFrame(unsigned frame) {
    if(guideFrame==frame) {
        nativeDepth.Reset();nativeVelocity.Reset();nativeView.Reset();guideFrame=0;
    }
    if(runtime && runtime->lastOutputFrame==frame) {
        runtime->lastOutputIdentity=0;runtime->lastOutputFrame=0;
    }
}
}
