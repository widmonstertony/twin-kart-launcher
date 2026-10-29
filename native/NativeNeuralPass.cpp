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
using Microsoft::WRL::ComPtr;
namespace TwinKartGraphics {
namespace {
constexpr char PassHash[]="190895c92630ac60e123be317004c6c8fa033d514b006e470b69b80d994eb5fc";
constexpr char CompositeHash[]="e8d1965b9d9771dbff40fbb7273f3f9007d4f8a54e7a38e30a2e865d49aba14d";
constexpr char ModuleHash[]="8df8fb48f60cc5469d54f6c34f11e0402537de8c0e47c69132ce246794233201";
std::wstring modulePath,providerPath,cachePath;
NativeNeuralLog logger=nullptr;
bool configured=false,disabled=false;
thread_local bool processing=false;
struct Processing { Processing(){processing=true;} ~Processing(){processing=false;} };
using Init=decltype(&TwinKartNeuralInitialize);
using Create=decltype(&TwinKartNeuralCreate);
using Evaluate=decltype(&TwinKartNeuralEvaluate);
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
    Init init=nullptr; Create create=nullptr; Evaluate evaluate=nullptr;
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
Runtime* runtime=nullptr; // Resident until owned process exit; no in-flight unload.
ComPtr<ID3D11ShaderResourceView> nativeDepth,nativeVelocity;
ComPtr<ID3D11Buffer> nativeView;
unsigned guideFrame=0;
void Event(const char* event,unsigned frame,unsigned value=0,void* target=nullptr) {
    if(!logger)return; char text[512];
    sprintf_s(text,"{\"event\":\"%s\",\"frame\":%u,\"result\":%u,\"target_resource\":\"%p\",\"game_acceptance\":false,\"sr_evaluated\":false,\"generated_frames\":0}\r\n",event,frame,value,target); logger(text);
}
bool ValidateView(ID3D11DeviceContext* context,ID3D11Device* device,ID3D11Buffer* buffer,unsigned width,unsigned height) {
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
    if(!r.init || !r.create || !r.evaluate)return false;
    unsigned initialized=r.init(r.device12.Get(),cachePath.c_str(),providerPath.c_str(),nullptr,true); Event("nr_real_game_initialize",0,initialized);
    if(initialized!=1)return false;
    unsigned created=r.create(r.commands.Get(),width,height); Event("nr_real_game_create",0,created);
    if(!r.Submit() || created!=1)return false;
    const char source[]=R"(
Texture2D<float> NativeDepth:register(t0);
Texture2D<float4> NativeVelocity:register(t1);
RWTexture2D<float> OutDepth:register(u0);
RWTexture2D<float2> OutMotion:register(u1);
cbuffer NativeView:register(b1) { float4 v[512]; };
[numthreads(8,8,1)] void main(uint3 id:SV_DispatchThreadID) {
 uint w,h;OutDepth.GetDimensions(w,h);if(id.x>=w||id.y>=h)return;
 uint gw,gh;NativeDepth.GetDimensions(gw,gh);uint2 samplePoint=min(uint2((float2(id.xy)+0.5)*float2(gw,gh)/float2(w,h)),uint2(gw-1,gh-1));
 float z=NativeDepth.Load(int3(samplePoint,0));float4 packed=NativeVelocity.Load(int3(samplePoint,0));
 float2 ndc=(float2(id.xy)+0.5)/float2(w,h)*float2(2,-2)+float2(-1,1)-v[130].xy;
 float4 previous=mul(float4(ndc,z,1),float4x4(v[126],v[127],v[128],v[129]));
 float2 delta=previous.xy/previous.w-ndc;
 if(packed.x>0)delta=-(packed.xy-(32767.0/65535.0))/(0.499*0.5);
 OutDepth[id.xy]=z;OutMotion[id.xy]=delta*float2(w*0.5,-h*0.5);
})";
    ComPtr<ID3DBlob> code,errors;
    if(FAILED(D3DCompile(source,sizeof(source),nullptr,nullptr,nullptr,"main","cs_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&code,&errors)) ||
       FAILED(r.device11->CreateComputeShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&r.convert)))return false;
    D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=height;desc.MipLevels=1;desc.ArraySize=1;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_UNORDERED_ACCESS|D3D11_BIND_SHADER_RESOURCE;
    desc.Format=DXGI_FORMAT_R32_FLOAT;
    if(FAILED(r.device11->CreateTexture2D(&desc,nullptr,&r.depth)) || FAILED(r.device11->CreateUnorderedAccessView(r.depth.Get(),nullptr,&r.depthUav)))return false;
    desc.Format=DXGI_FORMAT_R16G16_FLOAT;
    if(FAILED(r.device11->CreateTexture2D(&desc,nullptr,&r.motion)) || FAILED(r.device11->CreateUnorderedAccessView(r.motion.Get(),nullptr,&r.motionUav)))return false;
    return SUCCEEDED(r.colorBridge.Initialize(r.device11.Get(),r.device12.Get(),r.queue.Get(),width,height,DXGI_FORMAT_R10G10B10A2_UNORM)) &&
        SUCCEEDED(r.depthBridge.Initialize(r.device11.Get(),r.device12.Get(),r.queue.Get(),width,height,DXGI_FORMAT_R32_FLOAT)) &&
        SUCCEEDED(r.motionBridge.Initialize(r.device11.Get(),r.device12.Get(),r.queue.Get(),width,height,DXGI_FORMAT_R16G16_FLOAT)) &&
        SUCCEEDED(r.outputBridge.Initialize(r.device11.Get(),r.device12.Get(),r.queue.Get(),width,height,DXGI_FORMAT_R10G10B10A2_UNORM));
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
    if(!runtime) {
        runtime=new Runtime;
        if(!Initialize(*runtime,context,nativeView.Get(),width,height,guideWidth,guideHeight)) {disabled=true;Event("nr_native_pass_initialize_failed",frame);return false;}
        Event("nr_native_pass_initialized",frame);
    }
    auto& r=*runtime;
    if(r.owner!=GetCurrentThreadId() || r.width!=width || r.height!=height || r.lastFrame==frame)return false;
    bool reset=!r.evaluations || frame>r.lastFrame+8;
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
    if(FAILED(r.colorBridge.CopyTo12(target.Get())) || FAILED(r.depthBridge.CopyTo12(r.depth.Get())) ||
       FAILED(r.motionBridge.CopyTo12(r.motion.Get())) || FAILED(r.outputBridge.CopyTo12(target.Get())) ||
       FAILED(r.allocator->Reset()) || FAILED(r.commands->Reset(r.allocator.Get(),nullptr))) {disabled=true;Event("nr_native_resource_handoff_failed",frame);return false;}
    ID3D12Resource* inputs[]{r.colorBridge.Texture12(),r.depthBridge.Texture12(),r.motionBridge.Texture12()};
    for(auto p:inputs)Transition(r.commands.Get(),p,D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    auto output=r.outputBridge.Texture12();Transition(r.commands.Get(),output,D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    TwinKartNeuralFrame request{sizeof(TwinKartNeuralFrame),r.commands.Get(),inputs[0],inputs[1],inputs[2],output,1,1,1,reset,true,true};
    unsigned result=r.evaluate(&request);
    for(auto p:inputs)Transition(r.commands.Get(),p,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COMMON);
    Transition(r.commands.Get(),output,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COMMON);
    bool submitted=r.Submit();
    bool returned=SUCCEEDED(r.colorBridge.ReturnTo11(nullptr)) && SUCCEEDED(r.depthBridge.ReturnTo11(nullptr)) && SUCCEEDED(r.motionBridge.ReturnTo11(nullptr)) &&
        SUCCEEDED(r.outputBridge.ReturnTo11(submitted && result==1?target.Get():nullptr));
    if(!submitted || !returned || result!=1) {disabled=true;Event("nr_native_evaluate_failed",frame,result);return false;}
    ++r.evaluations;
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
void FinishNativeNeuralFrame(unsigned frame) {
    if(guideFrame==frame) {
        nativeDepth.Reset();nativeVelocity.Reset();nativeView.Reset();guideFrame=0;
    }
    if(runtime && runtime->lastOutputFrame==frame) {
        runtime->lastOutputIdentity=0;runtime->lastOutputFrame=0;
    }
}
}
