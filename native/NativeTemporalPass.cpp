#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <string>
#include <cstdio>
#include "NativeTemporalPass.h"
#include "NativeTemporalContract.h"
#include "NativeGuideShader.h"
#include "NativeViewMath.h"
#include "TemporalHistory.h"
#include "ClientBuildGuard.h"
#include "RenderPassTrace.h"
#include "DlssD3D11.h"
using Microsoft::WRL::ComPtr;
namespace TwinKartGraphics {
namespace {
constexpr char PassHash[]="190895c92630ac60e123be317004c6c8fa033d514b006e470b69b80d994eb5fc";
using Init=decltype(&TwinKartDlssInitialize);
using Evaluate=decltype(&TwinKartDlssEvaluate);
using Shutdown=decltype(&TwinKartDlssShutdown);
struct TemporalRuntime {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext1> context;
    ComPtr<ID3DDeviceContextState> isolated;
    ComPtr<ID3D11ComputeShader> convert;
    ComPtr<ID3D11Buffer> rectangle;
    ComPtr<ID3D11Texture2D> color,depth,motion,output;
    ComPtr<ID3D11UnorderedAccessView> colorWrite,depthWrite,motionWrite;
    HMODULE module=nullptr;Init init=nullptr;Evaluate evaluate=nullptr;Shutdown shutdown=nullptr;
    unsigned w=0,h=0,ow=0,oh=0,lastOutput=0;
    DWORD thread=0;
    TemporalHistory history;
    LARGE_INTEGER frequency{},lastTime{};
};
TemporalRuntime runtime;
std::wstring modulePath,providerPath,cachePath;
TemporalPassLog logger=nullptr;
bool configured=false,failed=false;
thread_local bool processing=false;
void Event(const char* name,unsigned frame,unsigned result=0) {
    if(!logger)return;char row[320];
    sprintf_s(row,"{\"event\":\"%s\",\"frame\":%u,\"result\":%u,\"temporal_input\":true,\"game_acceptance\":false}\r\n",name,frame,result);logger(row);
}
bool ReadBuffer(ID3D11DeviceContext* context,ID3D11Buffer* source,void* data,unsigned count,unsigned expected) {
    if(!source)return false;D3D11_BUFFER_DESC desc{};source->GetDesc(&desc);
    if(desc.ByteWidth!=expected || count>desc.ByteWidth)return false;
    ComPtr<ID3D11Device> device;context->GetDevice(&device);
    desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=desc.MiscFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Buffer> copy;if(FAILED(device->CreateBuffer(&desc,nullptr,&copy)))return false;
    context->CopyResource(copy.Get(),source);D3D11_MAPPED_SUBRESOURCE mapped{};
    if(FAILED(context->Map(copy.Get(),0,D3D11_MAP_READ,0,&mapped)))return false;
    memcpy(data,mapped.pData,count);context->Unmap(copy.Get(),0);return true;
}
bool ReadTexture(ID3D11ShaderResourceView* view,DXGI_FORMAT resourceFormat,DXGI_FORMAT viewFormat,
    ComPtr<ID3D11Texture2D>& texture,D3D11_TEXTURE2D_DESC& desc) {
    if(!view)return false;D3D11_SHADER_RESOURCE_VIEW_DESC binding{};view->GetDesc(&binding);
    if(binding.ViewDimension!=D3D11_SRV_DIMENSION_TEXTURE2D || binding.Format!=viewFormat ||
       binding.Texture2D.MostDetailedMip!=0 || binding.Texture2D.MipLevels!=1)return false;
    ComPtr<ID3D11Resource> resource;view->GetResource(&resource);if(FAILED(resource.As(&texture)))return false;
    texture->GetDesc(&desc);
    return desc.Format==resourceFormat && desc.ArraySize==1 && desc.MipLevels==1 && desc.SampleDesc.Count==1 &&
        desc.Width>=128 && desc.Height>=128 && desc.Width<=8192 && desc.Height<=8192;
}
bool InitializeContext(ID3D11DeviceContext* context) {
    auto& r=runtime;context->GetDevice(&r.device);r.thread=GetCurrentThreadId();
    ComPtr<ID3D11Device1> device;if(FAILED(r.device.As(&device)) || FAILED(context->QueryInterface(IID_PPV_ARGS(&r.context))))return false;
    D3D_FEATURE_LEVEL level=r.device->GetFeatureLevel(),selected;
    if(FAILED(device->CreateDeviceContextState(0,&level,1,D3D11_SDK_VERSION,__uuidof(ID3D11Device),&selected,&r.isolated)))return false;
    ComPtr<ID3DBlob> code,errors;D3D_SHADER_MACRO definitions[]={{"TK_TEMPORAL_INPUT","1"},{nullptr,nullptr}};
    if(FAILED(D3DCompile(NativeGuideShader,sizeof(NativeGuideShader),nullptr,definitions,nullptr,"main","cs_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&code,&errors)) ||
       FAILED(r.device->CreateComputeShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&r.convert)))return false;
    D3D11_BUFFER_DESC desc{};desc.ByteWidth=16;desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    if(FAILED(r.device->CreateBuffer(&desc,nullptr,&r.rectangle)))return false;
    QueryPerformanceFrequency(&r.frequency);return true;
}
bool Initialize(ID3D11DeviceContext* context) {
    if(!InitializeContext(context))return false;auto& r=runtime;
    r.module=LoadLibraryExW(modulePath.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);if(!r.module)return false;
    r.init=reinterpret_cast<Init>(GetProcAddress(r.module,"TwinKartDlssInitialize"));
    r.evaluate=reinterpret_cast<Evaluate>(GetProcAddress(r.module,"TwinKartDlssEvaluate"));
    r.shutdown=reinterpret_cast<Shutdown>(GetProcAddress(r.module,"TwinKartDlssShutdown"));
    if(!r.init || !r.evaluate || !r.shutdown)return false;
    unsigned result=r.init(r.device.Get(),cachePath.c_str(),providerPath.c_str());Event("sr_temporal_initialize",0,result);
    return result==1;
}
bool Resources(const NativeTemporalContract& c) {
    auto& r=runtime;
    unsigned w=c.input.Width(),h=c.input.Height(),ow=c.output.Width(),oh=c.output.Height();
    if(r.w==w && r.h==h && r.ow==ow && r.oh==oh)return true;
    ComPtr<ID3D11Texture2D> colors,depths,motions,output;
    ComPtr<ID3D11UnorderedAccessView> cw,dw,mw;
    D3D11_TEXTURE2D_DESC desc{};desc.Width=w;desc.Height=h;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;
    desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS;
    auto create=[&](DXGI_FORMAT format,ComPtr<ID3D11Texture2D>& texture,ComPtr<ID3D11UnorderedAccessView>& write){
        desc.Format=format;return SUCCEEDED(r.device->CreateTexture2D(&desc,nullptr,&texture)) && SUCCEEDED(r.device->CreateUnorderedAccessView(texture.Get(),nullptr,&write));
    };
    if(!create(DXGI_FORMAT_R16G16B16A16_FLOAT,colors,cw) || !create(DXGI_FORMAT_R32_FLOAT,depths,dw) || !create(DXGI_FORMAT_R16G16_FLOAT,motions,mw))return false;
    desc.Width=ow;desc.Height=oh;desc.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;
    if(FAILED(r.device->CreateTexture2D(&desc,nullptr,&output)))return false;
    r.color=colors;r.depth=depths;r.motion=motions;r.output=output;r.colorWrite=cw;r.depthWrite=dw;r.motionWrite=mw;
    r.w=w;r.h=h;r.ow=ow;r.oh=oh;r.history=TemporalHistory{};return true;
}
struct Isolated {
    ID3D11DeviceContext1* context;ComPtr<ID3DDeviceContextState> saved;
    explicit Isolated(TemporalRuntime& r):context(r.context.Get()){context->SwapDeviceContextState(r.isolated.Get(),&saved);}
    ~Isolated(){context->SwapDeviceContextState(saved.Get(),nullptr);}
};
}
bool ConfigureNativeTemporalPass(const wchar_t* module,const char* sha,const wchar_t* provider,const wchar_t* cache,TemporalPassLog log) {
    if(configured || !provider || !*provider || !cache || !*cache || !TwinKartClient::FileHasHash(module,sha))return false;
    modulePath=module;providerPath=provider;cachePath=cache;logger=log;configured=true;return true;
}
namespace {
bool ProcessQualifiedTemporalPass(ID3D11DeviceContext* context,ID3D11ComputeShader* shader,unsigned frame,UINT x,UINT y,UINT z) {
    struct Guard {Guard(){processing=true;}~Guard(){processing=false;}} guard;
    ComPtr<ID3D11ShaderResourceView> color,depth,motion;
    context->CSGetShaderResources(1,1,&color);context->CSGetShaderResources(3,1,&depth);context->CSGetShaderResources(4,1,&motion);
    ComPtr<ID3D11Texture2D> ct,dt,mt,target;D3D11_TEXTURE2D_DESC cd{},dd{},md{},od{};
    if(!ReadTexture(color.Get(),DXGI_FORMAT_R11G11B10_FLOAT,DXGI_FORMAT_R11G11B10_FLOAT,ct,cd) ||
       !ReadTexture(depth.Get(),DXGI_FORMAT_R32G8X24_TYPELESS,DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS,dt,dd) ||
       !ReadTexture(motion.Get(),DXGI_FORMAT_R16G16B16A16_UNORM,DXGI_FORMAT_R16G16B16A16_UNORM,mt,md))return false;
    ComPtr<ID3D11UnorderedAccessView> output;context->CSGetUnorderedAccessViews(0,1,&output);
    if(!output)return false;D3D11_UNORDERED_ACCESS_VIEW_DESC ov{};output->GetDesc(&ov);
    if(ov.ViewDimension!=D3D11_UAV_DIMENSION_TEXTURE2D || ov.Format!=DXGI_FORMAT_R16G16B16A16_FLOAT || ov.Texture2D.MipSlice)return false;
    ComPtr<ID3D11Resource> resource;output->GetResource(&resource);if(FAILED(resource.As(&target)))return false;target->GetDesc(&od);
    if(od.Format!=DXGI_FORMAT_R16G16B16A16_FLOAT || od.MipLevels!=1 || od.ArraySize!=1 || od.SampleDesc.Count!=1 ||
       target.Get()==ct.Get() || target.Get()==dt.Get() || target.Get()==mt.Get())return false;
    ID3D11UnorderedAccessView* extra[7]{};context->CSGetUnorderedAccessViews(1,7,extra);bool additional=false;
    for(auto p:extra)if(p){additional=true;p->Release();}if(additional)return false;
    ComPtr<ID3D11Buffer> params,view;context->CSGetConstantBuffers(0,1,&params);context->CSGetConstantBuffers(1,1,&view);
    float p[24]{},v[576]{};
    if(!ReadBuffer(context,params.Get(),p,sizeof(p),4096) || !ReadBuffer(context,view.Get(),v,sizeof(v),8192))return false;
    NativeTemporalContract contract;
    if(!DecodeNativeTemporalContract(p,sizeof(p),v,cd.Width,cd.Height,od.Width,od.Height,contract) ||
       !contract.input.Fits(dd.Width,dd.Height) || !contract.input.Fits(md.Width,md.Height) ||
       !x || !y || z!=1)return false;
    // Read work-group dimensions from reflection at native shader creation.
    // Do not guess a group size or replace a partial native dispatch.
    unsigned gx=0,gy=0,gz=0;
    if(!TwinKartRender::ShaderThreadGroup(shader,gx,gy,gz) || gz!=1 ||
       x!=(contract.output.Width()+gx-1)/gx || y!=(contract.output.Height()+gy-1)/gy)return false;
    CameraFrame camera{};
    if(!DecodeNativeCamera(v,contract.input.Width(),contract.input.Height(),false,camera))return false;
    auto& r=runtime;
    if(!r.device && !Initialize(context)){failed=true;Event("sr_temporal_initialization_failed",frame);return false;}
    if(r.thread!=GetCurrentThreadId() || r.context.Get()!=context || r.lastOutput==frame || !Resources(contract))return false;
    using Revision=unsigned long long(WINAPI*)();HMODULE cockpit=GetModuleHandleW(L"TwinKartDriftCamera.dll");
    auto read=cockpit?reinterpret_cast<Revision>(GetProcAddress(cockpit,"TwinKartCameraHistoryRevision")):nullptr;
    auto revision=read?read():0;bool reset=r.history.NeedsReset(frame,revision,camera);
    LARGE_INTEGER now{};QueryPerformanceCounter(&now);
    float milliseconds=r.lastTime.QuadPart?static_cast<float>(1000.0*(now.QuadPart-r.lastTime.QuadPart)/r.frequency.QuadPart):16.666667f;
    r.lastTime=now;if(milliseconds<=0 || milliseconds>1000){milliseconds=16.666667f;reset=true;}
    unsigned result;
    {
        Isolated saved(r);
        unsigned rect[]{static_cast<unsigned>(contract.input.left),static_cast<unsigned>(contract.input.top),r.w,r.h};
        context->UpdateSubresource(r.rectangle.Get(),0,nullptr,rect,0,0);
        ID3D11Buffer* native=view.Get();context->CSSetConstantBuffers(1,1,&native);ID3D11Buffer* rectangle=r.rectangle.Get();context->CSSetConstantBuffers(3,1,&rectangle);
        ID3D11ShaderResourceView* reads[]{depth.Get(),motion.Get(),color.Get()};
        ID3D11UnorderedAccessView* writes[]{r.depthWrite.Get(),r.motionWrite.Get(),r.colorWrite.Get()};
        context->CSSetShader(r.convert.Get(),nullptr,0);context->CSSetShaderResources(0,3,reads);context->CSSetUnorderedAccessViews(0,3,writes,nullptr);
        context->Dispatch((r.w+7)/8,(r.h+7)/8,1);
        context->ClearState();
        TwinKartTemporalFrame request{sizeof(TwinKartTemporalFrame),context,r.color.Get(),r.depth.Get(),r.motion.Get(),r.output.Get(),
            contract.jitterX,contract.jitterY,1,1,milliseconds,contract.preExposure,1,reset,true,true,true,true};
        result=r.evaluate(&request);
        if(result==1)context->CopySubresourceRegion(target.Get(),0,contract.output.left,contract.output.top,0,r.output.Get(),0,nullptr);
    }
    if(result!=1){failed=true;Event("sr_temporal_evaluate_failed_native_fallback",frame,result);return false;}
    r.lastOutput=frame;r.history.Commit(frame,revision,camera);
    if(reset || frame%120==0)Event("sr_native_temporal_output_submitted",frame,result);
    return true;
}
}
bool ProcessNativeTemporalPass(ID3D11DeviceContext* context,unsigned frame,UINT x,UINT y,UINT z) {
    if(!configured || failed || processing || !frame || !context || context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return false;
    ComPtr<ID3D11ComputeShader> shader;context->CSGetShader(&shader,nullptr,nullptr);char hash[65];
    if(!TwinKartRender::ShaderHash(shader.Get(),hash) || strcmp(hash,PassHash))return false;
    return ProcessQualifiedTemporalPass(context,shader.Get(),frame,x,y,z);
}
bool NativeTemporalSuperResolutionRan(unsigned frame){return frame && runtime.lastOutput==frame;}
}
