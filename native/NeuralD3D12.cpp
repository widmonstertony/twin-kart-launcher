#define WIN32_LEAN_AND_MEAN
#define TWIN_KART_NEURAL_BUILD
#include <windows.h>
#include <cmath>
#include <cstdio>
#include <string>
#include "NeuralD3D12.h"
#include "ClientBuildGuard.h"
// Public NGX declarations are externally supplied SDK dependencies. Resource
// parameter names are interface facts from the supplied NVIDIA NR runtime.
#include <nvsdk_ngx.h>
#include <nvsdk_ngx_helpers_d3d.h>

namespace {
ID3D12Device* device=nullptr;
NVSDK_NGX_Parameter* parameters=nullptr;
NVSDK_NGX_Handle* feature=nullptr;
NVSDK_NGX_Handle* secondFeature=nullptr;
NVSDK_NGX_Parameter* secondParameters=nullptr;
ID3D12Resource* intermediate=nullptr;
unsigned requestedPasses=1,requestedStyle=0;
bool resetNeuralHistory=true;
NVSDK_NGX_Handle* superResolution=nullptr;
NVSDK_NGX_Parameter* superParameters=nullptr;
TwinKartSuperResolutionState superState{sizeof(TwinKartSuperResolutionState)};
TwinKartNeuralState state{sizeof(TwinKartNeuralState)};
DWORD owner=0;
SRWLOCK consumerLock=SRWLOCK_INIT;
struct ConsumerCall {
    bool entered=TryAcquireSRWLockExclusive(&consumerLock)!=FALSE;
    ~ConsumerCall(){if(entered)ReleaseSRWLockExclusive(&consumerLock);}
};
TwinKartNeuralLogCallback logger=nullptr;
bool allowPrivate=false;
std::wstring providerDirectory,cacheDirectory;
HMODULE privateModule=nullptr;
using SnippetInit=NVSDK_NGX_Result(NVSDK_CONV*)(unsigned long long,const wchar_t*,ID3D12Device*,NVSDK_NGX_Version,const NVSDK_NGX_FeatureCommonInfo*);
using SnippetPopulate=NVSDK_NGX_Result(NVSDK_CONV*)(NVSDK_NGX_Parameter*);
using SnippetCreate=decltype(&NVSDK_NGX_D3D12_CreateFeature);
using SnippetEvaluate=decltype(&NVSDK_NGX_D3D12_EvaluateFeature);
using SnippetRelease=decltype(&NVSDK_NGX_D3D12_ReleaseFeature);
using SnippetShutdown=decltype(&NVSDK_NGX_D3D12_Shutdown1);
SnippetCreate privateCreate=nullptr; SnippetEvaluate privateEvaluate=nullptr;
SnippetRelease privateRelease=nullptr; SnippetShutdown privateShutdown=nullptr;
void NVSDK_CONV RuntimeLog(const char* message,NVSDK_NGX_Logging_Level level,NVSDK_NGX_Feature source) {
    if(logger) logger(message,static_cast<unsigned>(level),static_cast<unsigned>(source));
}
constexpr unsigned Invalid=0xf1000001,WrongThread=0xf1000002,MissingGuides=0xf1000003;
// This SDK reserves ID 18. The installed NVIDIA runtime's neural feature uses
// that ID. A runtime can reject it; reserved does not imply public SDK support.
constexpr NVSDK_NGX_Feature NeuralFeature=static_cast<NVSDK_NGX_Feature>(18);
bool InitializePrivateProvider() {
    if(!allowPrivate || privateModule) return false;
    std::wstring path=providerDirectory+L"\\nvngx_dlssnr.dll";
    // Exact interface revision observed in this externally provided model. Do
    // not apply an undocumented snippet ABI to an unknown future DLL.
    if(!TwinKartClient::FileHasHash(path.c_str(),"cdfa6c1ec784867182164c7836f324f087b5c9f374d0c9fe46f54999feabf3ed")) return false;
    privateModule=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if(!privateModule) return false;
    auto init=reinterpret_cast<SnippetInit>(GetProcAddress(privateModule,"NVSDK_NGX_D3D12_Init_Ext"));
    auto populate=reinterpret_cast<SnippetPopulate>(GetProcAddress(privateModule,"NVSDK_NGX_D3D12_PopulateParameters_Impl"));
    privateCreate=reinterpret_cast<SnippetCreate>(GetProcAddress(privateModule,"NVSDK_NGX_D3D12_CreateFeature"));
    privateEvaluate=reinterpret_cast<SnippetEvaluate>(GetProcAddress(privateModule,"NVSDK_NGX_D3D12_EvaluateFeature"));
    privateRelease=reinterpret_cast<SnippetRelease>(GetProcAddress(privateModule,"NVSDK_NGX_D3D12_ReleaseFeature"));
    privateShutdown=reinterpret_cast<SnippetShutdown>(GetProcAddress(privateModule,"NVSDK_NGX_D3D12_Shutdown1"));
    if(!init || !populate || !privateCreate || !privateEvaluate || !privateRelease || !privateShutdown) return false;
    NVSDK_NGX_FeatureCommonInfo common{};
    if(logger) { common.LoggingInfo.LoggingCallback=RuntimeLog; common.LoggingInfo.MinimumLoggingLevel=NVSDK_NGX_LOGGING_LEVEL_VERBOSE; common.LoggingInfo.DisableOtherLoggingSinks=true; }
    // This model's exported Init_Ext takes device, SDK version, common info in
    // that order. It differs from the public core Init_with_ProjectID API.
    auto result=init(0,cacheDirectory.c_str(),device,NVSDK_NGX_Version_API,&common);
    state.privateInitializeResult=static_cast<unsigned>(result);
    if(NVSDK_NGX_FAILED(result)) return false;
    state.privateProvider=true;
    result=populate(parameters); state.privatePopulateResult=static_cast<unsigned>(result);
    return NVSDK_NGX_SUCCEED(result);
}
bool SameDevice(ID3D12DeviceChild* resource) {
    if(!resource) return false;
    ID3D12Device* found=nullptr;
    HRESULT result=resource->GetDevice(IID_PPV_ARGS(&found));
    bool matches=SUCCEEDED(result) && found==device;
    if(found) found->Release(); return matches;
}
bool Texture(ID3D12Resource* resource,DXGI_FORMAT first,DXGI_FORMAT second=DXGI_FORMAT_UNKNOWN) {
    if(!SameDevice(resource)) return false;
    auto desc=resource->GetDesc();
    return desc.Dimension==D3D12_RESOURCE_DIMENSION_TEXTURE2D && desc.Width==state.width && desc.Height==state.height &&
        desc.DepthOrArraySize==1 && desc.MipLevels==1 && desc.SampleDesc.Count==1 && (desc.Format==first || desc.Format==second);
}
}
namespace {
unsigned InitializeConsumer(ID3D12Device* selected,const wchar_t* cache,const wchar_t* provider,TwinKartNeuralLogCallback log,bool privateAbi,bool borrowed) {
    ConsumerCall call;if(!call.entered)return Invalid;
    if(device || !selected || !cache || !cache[0] || !provider || !provider[0]) return Invalid;
    device=selected; device->AddRef(); owner=GetCurrentThreadId();
    allowPrivate=privateAbi; providerDirectory=provider; cacheDirectory=cache;
    const wchar_t* paths[]={provider}; NVSDK_NGX_FeatureCommonInfo common{};
    common.PathListInfo.Path=paths; common.PathListInfo.Length=1;
    logger=log;
    if(logger) { common.LoggingInfo.LoggingCallback=RuntimeLog; common.LoggingInfo.MinimumLoggingLevel=NVSDK_NGX_LOGGING_LEVEL_VERBOSE; common.LoggingInfo.DisableOtherLoggingSinks=true; }
    // Our SDK dispatcher still needs initialization even when Streamline owns
    // the same device's core lifetime. Only device-wide shutdown is borrowed;
    // omitting dispatcher initialization returns NGX NotInitialized.
    auto result=NVSDK_NGX_D3D12_Init_with_ProjectID("45d35067-277b-4e3f-b2f6-e7dd197c609e",NVSDK_NGX_ENGINE_TYPE_CUSTOM,"TwinKart-Original-Neural",cache,device,&common);
    state.initializeResult=static_cast<unsigned>(result);
    if(NVSDK_NGX_FAILED(result)) return state.initializeResult;
    state.initialized=true;state.externalCoreLifetime=borrowed;
    result=NVSDK_NGX_D3D12_AllocateParameters(&parameters);
    state.initializeResult=static_cast<unsigned>(result); return state.initializeResult;
}
}
unsigned TwinKartNeuralInitialize(ID3D12Device* selected,const wchar_t* cache,const wchar_t* provider,TwinKartNeuralLogCallback log,bool privateAbi) {
    return InitializeConsumer(selected,cache,provider,log,privateAbi,false);
}
unsigned TwinKartNeuralInitializeBorrowed(ID3D12Device* selected,const wchar_t* cache,const wchar_t* provider,TwinKartNeuralLogCallback log,bool privateAbi) {
    return InitializeConsumer(selected,cache,provider,log,privateAbi,true);
}
bool TwinKartNeuralConfigure(unsigned passes,unsigned style) {
    ConsumerCall call;
    if(!call.entered || GetCurrentThreadId()!=owner || !state.initialized || feature || secondFeature ||
       passes<1 || passes>2 || style>2)return false;
    requestedPasses=passes;requestedStyle=style;
    state.requestedPasses=passes;state.style=style;return true;
}
unsigned TwinKartNeuralCreate(ID3D12GraphicsCommandList* commands,unsigned width,unsigned height) {
    ConsumerCall call;if(!call.entered)return Invalid;
    if(GetCurrentThreadId()!=owner) return WrongThread;
    if(!state.initialized || !parameters || feature || !SameDevice(commands) || width<32 || height<32 || width>8192 || height>8192) return Invalid;
    parameters->Set(NVSDK_NGX_Parameter_Width,width); parameters->Set(NVSDK_NGX_Parameter_Height,height);
    parameters->Set(NVSDK_NGX_Parameter_OutWidth,width); parameters->Set(NVSDK_NGX_Parameter_OutHeight,height);
    parameters->Set("CreationNodeMask",1u); parameters->Set("VisibilityNodeMask",1u);
    parameters->Set("DLSSNR.Width",width); parameters->Set("DLSSNR.Height",height);
    parameters->Set("DLSSNR.Enabled",1); parameters->Set("DLSSNR.ScalingRatio",1.0f);
    parameters->Set("DLSSNR.Style",requestedStyle);
    auto result=NVSDK_NGX_D3D12_CreateFeature(commands,NeuralFeature,parameters,&feature);
    if(NVSDK_NGX_FAILED(result) && !feature && InitializePrivateProvider()) result=privateCreate(commands,NeuralFeature,parameters,&feature);
    state.createResult=static_cast<unsigned>(result); state.featureCreated=NVSDK_NGX_SUCCEED(result) && feature;
    if(state.featureCreated) { state.width=width; state.height=height; }
    state.requestedPasses=requestedPasses;state.style=requestedStyle;
    if(state.featureCreated && requestedPasses==2) {
        result=NVSDK_NGX_D3D12_AllocateParameters(&secondParameters);
        if(NVSDK_NGX_SUCCEED(result) && state.privateProvider) {
            auto populate=reinterpret_cast<SnippetPopulate>(GetProcAddress(privateModule,"NVSDK_NGX_D3D12_PopulateParameters_Impl"));
            result=populate?populate(secondParameters):static_cast<NVSDK_NGX_Result>(Invalid);
        }
        if(NVSDK_NGX_SUCCEED(result)) {
            secondParameters->Set(NVSDK_NGX_Parameter_Width,width);secondParameters->Set(NVSDK_NGX_Parameter_Height,height);
            secondParameters->Set(NVSDK_NGX_Parameter_OutWidth,width);secondParameters->Set(NVSDK_NGX_Parameter_OutHeight,height);
            secondParameters->Set("CreationNodeMask",1u);secondParameters->Set("VisibilityNodeMask",1u);
            secondParameters->Set("DLSSNR.Width",width);secondParameters->Set("DLSSNR.Height",height);
            secondParameters->Set("DLSSNR.Enabled",1);secondParameters->Set("DLSSNR.ScalingRatio",1.0f);
            secondParameters->Set("DLSSNR.Style",requestedStyle);
            result=state.privateProvider?privateCreate(commands,NeuralFeature,secondParameters,&secondFeature):
                NVSDK_NGX_D3D12_CreateFeature(commands,NeuralFeature,secondParameters,&secondFeature);
        }
        state.createResult=static_cast<unsigned>(result);
        if(secondFeature==feature)secondFeature=nullptr;
        state.featureCreated=NVSDK_NGX_SUCCEED(result) && secondFeature;
        if(!state.featureCreated && NVSDK_NGX_SUCCEED(result))state.createResult=Invalid;
    }
    return state.createResult;
}
unsigned TwinKartNeuralEvaluate(const TwinKartNeuralFrame* frame) {
    ConsumerCall call;if(!call.entered)return Invalid;
    if(GetCurrentThreadId()!=owner) return WrongThread;
    if(!frame || frame->size!=sizeof(*frame) || !state.featureCreated || !feature || !parameters || !SameDevice(frame->commands)) return Invalid;
    if(!frame->guidesValidated) return MissingGuides;
    if(!Texture(frame->color,DXGI_FORMAT_R16G16B16A16_FLOAT,DXGI_FORMAT_R10G10B10A2_UNORM) ||
       !Texture(frame->output,DXGI_FORMAT_R16G16B16A16_FLOAT,DXGI_FORMAT_R10G10B10A2_UNORM) ||
       !Texture(frame->depth,DXGI_FORMAT_R32_FLOAT) || !Texture(frame->motion,DXGI_FORMAT_R16G16_FLOAT,DXGI_FORMAT_R32G32_FLOAT) ||
       frame->color==frame->output || frame->depth==frame->output || frame->motion==frame->output ||
       !(frame->output->GetDesc().Flags&D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) ||
       !std::isfinite(frame->motionScaleX) || !std::isfinite(frame->motionScaleY) || !frame->motionScaleX || !frame->motionScaleY ||
       !std::isfinite(frame->intensity) || frame->intensity<0 || frame->intensity>1) return Invalid;
    if(requestedPasses==2) {
        if(!secondFeature || !secondParameters)return Invalid;
        // Allocation is owned by this feature lifetime and retired only after
        // the caller drains GPU work. A format change requires recreation.
        auto desc=frame->output->GetDesc();
        if(intermediate && intermediate->GetDesc().Format!=desc.Format)return Invalid;
        if(!intermediate) {
            D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
            if(FAILED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&intermediate))))return Invalid;
        }
    }
    state.passResults[0]=state.passResults[1]=0;
    NVSDK_NGX_Result result=static_cast<NVSDK_NGX_Result>(Invalid);
    for(unsigned pass=0;pass<requestedPasses;++pass) {
    auto parameters=pass?secondParameters: ::parameters;
    auto activeFeature=pass?secondFeature:feature;
    parameters->Set("DLSSNR.Color",pass?intermediate:frame->color); parameters->Set("DLSSNR.Depth",frame->depth);
    parameters->Set("DLSSNR.MVec",frame->motion); parameters->Set("DLSSNR.Output",requestedPasses==2 && !pass?intermediate:frame->output);
    parameters->Set("DLSSNR.Style",requestedStyle);
    parameters->Set("DLSSNR.MVecScaleX",frame->motionScaleX); parameters->Set("DLSSNR.MVecScaleY",frame->motionScaleY);
    parameters->Set("DLSSNR.Intensity",frame->intensity); parameters->Set("DLSSNR.Reset",frame->reset || resetNeuralHistory?1:0);
    parameters->Set("DLSSNR.DepthInverted",frame->invertedDepth?1:0);
    const char* prefixes[]={"DLSSNR.ColorSubrect","DLSSNR.DepthSubrect","DLSSNR.MVecSubrect","DLSSNR.OutputSubrect"};
    for(const char* prefix:prefixes) {
        char key[96]; sprintf_s(key,"%sBaseX",prefix); parameters->Set(key,0u);
        sprintf_s(key,"%sBaseY",prefix); parameters->Set(key,0u);
        sprintf_s(key,"%sWidth",prefix); parameters->Set(key,state.width);
        sprintf_s(key,"%sHeight",prefix); parameters->Set(key,state.height);
    }
    result=state.privateProvider?privateEvaluate(frame->commands,activeFeature,parameters,nullptr):NVSDK_NGX_D3D12_EvaluateFeature(frame->commands,activeFeature,parameters,nullptr);
    state.passResults[pass]=static_cast<unsigned>(result);
    if(NVSDK_NGX_SUCCEED(result))++state.passEvaluations[pass];
    if(requestedPasses==2 && !pass) {
        if(NVSDK_NGX_FAILED(result))break;
        D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition={intermediate,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE};
        frame->commands->ResourceBarrier(1,&barrier);
    } else if(pass) {
        D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition={intermediate,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS};
        frame->commands->ResourceBarrier(1,&barrier);
    }
    }
    state.evaluateResult=static_cast<unsigned>(result); if(NVSDK_NGX_SUCCEED(result)) ++state.evaluations;
    // A partially completed chain cannot keep one pass one frame ahead of the
    // other. Reset both independent temporal histories on the next attempt.
    resetNeuralHistory=NVSDK_NGX_FAILED(result);
    return state.evaluateResult;
}
bool TwinKartNeuralGetState(TwinKartNeuralState* target) {
    ConsumerCall call;if(!call.entered)return false;
    if(!target || target->size!=sizeof(*target) || GetCurrentThreadId()!=owner) return false;
    *target=state; return true;
}
unsigned TwinKartSuperResolutionCreate(ID3D12GraphicsCommandList* commands,unsigned iw,unsigned ih,unsigned ow,unsigned oh,int quality,bool hdr,bool invertedDepth) {
    ConsumerCall call;if(!call.entered)return Invalid;
    if(GetCurrentThreadId()!=owner) return WrongThread;
    if(!state.initialized || superResolution || !SameDevice(commands) || iw<32 || ih<32 || ow<=iw || oh<=ih || ow>8192 || oh>8192 || quality<0 || quality>3) return Invalid;
    if(!superParameters) {
        auto allocated=NVSDK_NGX_D3D12_GetCapabilityParameters(&superParameters);
        if(NVSDK_NGX_FAILED(allocated)) return static_cast<unsigned>(allocated);
    }
    NVSDK_NGX_DLSS_Create_Params create{};
    create.Feature.InWidth=iw; create.Feature.InHeight=ih; create.Feature.InTargetWidth=ow; create.Feature.InTargetHeight=oh;
    create.Feature.InPerfQualityValue=static_cast<NVSDK_NGX_PerfQuality_Value>(quality);
    create.InFeatureCreateFlags=NVSDK_NGX_DLSS_Feature_Flags_MVLowRes;
    if(hdr) create.InFeatureCreateFlags|=NVSDK_NGX_DLSS_Feature_Flags_IsHDR;
    if(invertedDepth) create.InFeatureCreateFlags|=NVSDK_NGX_DLSS_Feature_Flags_DepthInverted;
    auto result=NGX_D3D12_CREATE_DLSS_EXT(commands,1,1,&superResolution,superParameters,&create);
    superState.createResult=static_cast<unsigned>(result); superState.featureCreated=NVSDK_NGX_SUCCEED(result) && superResolution;
    if(superState.featureCreated) { superState.inputWidth=iw; superState.inputHeight=ih; superState.outputWidth=ow; superState.outputHeight=oh; }
    return superState.createResult;
}
unsigned TwinKartSuperResolutionEvaluate(const TwinKartSuperResolutionFrame* frame) {
    ConsumerCall call;if(!call.entered)return Invalid;
    if(GetCurrentThreadId()!=owner) return WrongThread;
    if(!frame || frame->size!=sizeof(*frame) || !superResolution || !superParameters || !SameDevice(frame->commands)) return Invalid;
    if(!frame->guidesValidated || !frame->hudlessValidated) return MissingGuides;
    if(!SameDevice(frame->color) || !SameDevice(frame->depth) || !SameDevice(frame->motion) || !SameDevice(frame->output) ||
        frame->color==frame->output || frame->depth==frame->output || frame->motion==frame->output) return Invalid;
    ID3D12Resource* resources[]={frame->color,frame->depth,frame->motion,frame->output};
    for(unsigned index=0;index<4;++index) {
        auto desc=resources[index]->GetDesc();
        if(desc.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D || desc.SampleDesc.Count!=1 || desc.DepthOrArraySize!=1 || desc.MipLevels!=1 ||
            desc.Width!=(index==3?superState.outputWidth:superState.inputWidth) || desc.Height!=(index==3?superState.outputHeight:superState.inputHeight)) return Invalid;
    }
    if(!(frame->output->GetDesc().Flags&D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) ||
       !std::isfinite(frame->jitterX) || !std::isfinite(frame->jitterY) || !std::isfinite(frame->motionScaleX) || !std::isfinite(frame->motionScaleY) ||
       !frame->motionScaleX || !frame->motionScaleY || !std::isfinite(frame->frameMilliseconds) || frame->frameMilliseconds<=0 ||
       !std::isfinite(frame->preExposure) || frame->preExposure<=0 || !std::isfinite(frame->exposureScale) || frame->exposureScale<=0) return Invalid;
    NVSDK_NGX_D3D12_DLSS_Eval_Params evaluate{};
    evaluate.Feature.pInColor=frame->color; evaluate.Feature.pInOutput=frame->output;
    evaluate.pInDepth=frame->depth; evaluate.pInMotionVectors=frame->motion;
    evaluate.InJitterOffsetX=frame->jitterX; evaluate.InJitterOffsetY=frame->jitterY;
    evaluate.InMVScaleX=frame->motionScaleX; evaluate.InMVScaleY=frame->motionScaleY;
    evaluate.InRenderSubrectDimensions.Width=superState.inputWidth; evaluate.InRenderSubrectDimensions.Height=superState.inputHeight;
    evaluate.InFrameTimeDeltaInMsec=frame->frameMilliseconds; evaluate.InPreExposure=frame->preExposure; evaluate.InExposureScale=frame->exposureScale;
    evaluate.InReset=frame->reset || !superState.evaluations;
    auto result=NGX_D3D12_EVALUATE_DLSS_EXT(frame->commands,superResolution,superParameters,&evaluate);
    superState.evaluateResult=static_cast<unsigned>(result); if(NVSDK_NGX_SUCCEED(result)) ++superState.evaluations;
    return superState.evaluateResult;
}
bool TwinKartSuperResolutionGetState(TwinKartSuperResolutionState* target) {
    ConsumerCall call;if(!call.entered)return false;
    if(!target || target->size!=sizeof(*target) || GetCurrentThreadId()!=owner) return false;
    *target=superState; return true;
}
namespace {
bool ShutdownUnchecked() {
    if(!device)return true;
    if(secondFeature) {
        auto result=state.privateProvider?privateRelease(secondFeature):NVSDK_NGX_D3D12_ReleaseFeature(secondFeature);
        if(NVSDK_NGX_FAILED(result))return false;secondFeature=nullptr;
    }
    if(intermediate){intermediate->Release();intermediate=nullptr;}
    if(secondParameters){if(NVSDK_NGX_FAILED(NVSDK_NGX_D3D12_DestroyParameters(secondParameters)))return false;secondParameters=nullptr;}
    if(feature) {
        auto result=state.privateProvider?privateRelease(feature):NVSDK_NGX_D3D12_ReleaseFeature(feature);
        if(NVSDK_NGX_FAILED(result))return false;feature=nullptr;state.featureCreated=false;
    }
    if(superResolution) {if(NVSDK_NGX_FAILED(NVSDK_NGX_D3D12_ReleaseFeature(superResolution)))return false;superResolution=nullptr;superState.featureCreated=false;}
    if(superParameters) {if(NVSDK_NGX_FAILED(NVSDK_NGX_D3D12_DestroyParameters(superParameters)))return false;superParameters=nullptr;}
    if(state.privateProvider && privateShutdown) {
        if(NVSDK_NGX_FAILED(privateShutdown(device)))return false;state.privateProvider=false;
    }
    superState=TwinKartSuperResolutionState{sizeof(TwinKartSuperResolutionState)};
    if(parameters) {if(NVSDK_NGX_FAILED(NVSDK_NGX_D3D12_DestroyParameters(parameters)))return false;parameters=nullptr;}
    if(state.initialized && !state.externalCoreLifetime) {if(NVSDK_NGX_FAILED(NVSDK_NGX_D3D12_Shutdown1(device)))return false;}
    state.initialized=false;
    if(privateModule) FreeLibrary(privateModule);
    privateModule=nullptr; privateCreate=nullptr; privateEvaluate=nullptr; privateRelease=nullptr; privateShutdown=nullptr;
    allowPrivate=false;requestedPasses=1;requestedStyle=0;resetNeuralHistory=true; providerDirectory.clear(); cacheDirectory.clear();
    device->Release(); device=nullptr; owner=0; state=TwinKartNeuralState{sizeof(TwinKartNeuralState)}; logger=nullptr;return true;
}
}
void TwinKartNeuralShutdown() {
    ConsumerCall call;if(!call.entered || !device || GetCurrentThreadId()!=owner)return;
    ShutdownUnchecked();
}
bool TwinKartNeuralShutdownDrained(unsigned expectedOwner,ID3D12Fence* fence,unsigned long long value) {
    ConsumerCall call;
    if(!call.entered || !device || !expectedOwner || owner!=expectedOwner || !value || !SameDevice(fence))return false;
    auto completed=fence->GetCompletedValue();
    if(completed==~0ull || completed<value)return false;
    return ShutdownUnchecked();
}
