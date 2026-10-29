#define WIN32_LEAN_AND_MEAN
#include "FrameGenerationD3D12.h"
#include <sl.h>
#include <sl_dlss_g.h>
#include <sl_reflex.h>
#include <sl_pcl.h>
#include <wintrust.h>
#include <softpub.h>
#include <cmath>
#include <cstring>
#include <string>
#include <atomic>
#include "NativeFrameMarkers.h"
#include "NativeFrameTimeline.h"

namespace TwinKartGraphics {
namespace {
constexpr unsigned Invalid=0xf2000001,WrongThread=0xf2000002,MissingGuides=0xf2000003,UntrustedRuntime=0xf2000004;
unsigned Code(sl::Result value) { return static_cast<unsigned>(value); }
bool SignedLibrary(const wchar_t* path) {
    WINTRUST_FILE_INFO file{}; file.cbStruct=sizeof(file); file.pcwszFilePath=path;
    WINTRUST_DATA data{}; data.cbStruct=sizeof(data); data.dwUIChoice=WTD_UI_NONE; data.fdwRevocationChecks=WTD_REVOKE_NONE;
    data.dwUnionChoice=WTD_CHOICE_FILE; data.pFile=&file; data.dwStateAction=WTD_STATEACTION_VERIFY;
    data.dwProvFlags=WTD_CACHE_ONLY_URL_RETRIEVAL; GUID action=WINTRUST_ACTION_GENERIC_VERIFY_V2;
    LONG result=WinVerifyTrust(nullptr,&action,&data); data.dwStateAction=WTD_STATEACTION_CLOSE; WinVerifyTrust(nullptr,&action,&data); return result==ERROR_SUCCESS;
}
bool Finite(const float* data,unsigned count) { for(unsigned i=0;i<count;++i) if(!std::isfinite(data[i])) return false; return true; }
}
struct FrameGeneration::Impl {
    HMODULE module=nullptr; DWORD thread=0; bool initialized=false,bound=false,configured=false;
    ID3D12Device* device=nullptr; sl::FrameToken* token=nullptr; sl::ViewportHandle viewport{0}; sl::DLSSGOptions options{};
    PFun_slInit* initialize=nullptr; PFun_slShutdown* shutdown=nullptr; PFun_slGetFeatureFunction* function=nullptr;
    PFun_slSetD3DDevice* setDevice=nullptr; PFun_slGetNewFrameToken* frameToken=nullptr;
    PFun_slSetConstants* constants=nullptr; PFun_slSetTagForFrame* tag=nullptr; PFun_slFreeResources* freeResources=nullptr;
    PFun_slDLSSGGetState* state=nullptr; PFun_slDLSSGSetOptions* setOptions=nullptr;
    PFun_slReflexSetOptions* reflex=nullptr; PFun_slReflexSleep* sleep=nullptr; PFun_slPCLSetMarker* marker=nullptr;
    GenerationState result{};
    SRWLOCK nativeLock=SRWLOCK_INIT;
    NativeFrameTimeline nativeFrames;
    std::atomic<bool> acceptingNative{false};
    std::atomic<unsigned> nativeCallbacks{0};
    DWORD simulationThread=0;
    unsigned long long lastNativeUsed=0;
    bool haveNativeUsed=false;
    std::shared_ptr<NativeFrameMarkerSink> nativeSink;
    void NativeMarker(FrameStage,unsigned long long);
    unsigned Camera(const CameraFrame&);
    struct Sink final:NativeFrameMarkerSink {
        std::weak_ptr<Impl> owner;
        explicit Sink(const std::shared_ptr<Impl>& value):owner(value){}
        void Observe(FrameStage stage,unsigned long long frame) override {
            auto selected=owner.lock();if(!selected)return;
            AcquireSRWLockExclusive(&selected->nativeLock);
            bool admitted=selected->acceptingNative.load(std::memory_order_acquire);
            if(admitted)selected->nativeCallbacks.fetch_add(1);
            ReleaseSRWLockExclusive(&selected->nativeLock);if(!admitted)return;
            struct Callback {
                Impl* target;explicit Callback(Impl* value):target(value){}
                ~Callback(){target->nativeCallbacks.fetch_sub(1);}
            } callback(selected.get());
            selected->NativeMarker(stage,frame);
        }
    };
    bool Owner() const { return thread==GetCurrentThreadId(); }
    template<class T> bool Import(T*& target,const char* name) { target=reinterpret_cast<T*>(GetProcAddress(module,name)); return target!=nullptr; }
    template<class T> bool Feature(T*& target,unsigned id,const char* name) {
        void* found=nullptr; if(function(id,name,found)!=sl::Result::eOk || !found) return false; target=reinterpret_cast<T*>(found); return true;
    }
    bool Resource(ID3D12DeviceChild* child) {
        if(!child) return false; ID3D12Device* parent=nullptr; HRESULT hr=child->GetDevice(IID_PPV_ARGS(&parent));
        bool same=SUCCEEDED(hr) && parent->GetAdapterLuid().HighPart==device->GetAdapterLuid().HighPart && parent->GetAdapterLuid().LowPart==device->GetAdapterLuid().LowPart;
        if(parent) parent->Release(); return same;
    }
};
FrameGeneration::FrameGeneration():impl(std::make_shared<Impl>()) {}
FrameGeneration::~FrameGeneration() { Shutdown(); }
unsigned FrameGeneration::Load(const wchar_t* directory,const wchar_t* logDirectory) {
    if(impl->module || !directory || !directory[0]) return Invalid;
    std::wstring path=std::wstring(directory)+L"\\sl.interposer.dll";
    if(!SignedLibrary(path.c_str())) return UntrustedRuntime;
    impl->module=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if(!impl->module) return GetLastError(); impl->thread=GetCurrentThreadId();
    if(!impl->Import(impl->initialize,"slInit") || !impl->Import(impl->shutdown,"slShutdown") || !impl->Import(impl->function,"slGetFeatureFunction") ||
       !impl->Import(impl->setDevice,"slSetD3DDevice") || !impl->Import(impl->frameToken,"slGetNewFrameToken") || !impl->Import(impl->constants,"slSetConstants") ||
       !impl->Import(impl->tag,"slSetTagForFrame") || !impl->Import(impl->freeResources,"slFreeResources")) return Invalid;
    const wchar_t* paths[]={directory}; const sl::Feature features[]={sl::kFeatureDLSS_G,sl::kFeatureReflex,sl::kFeaturePCL}; sl::Preferences preferences{};
    preferences.pathsToPlugins=paths; preferences.numPathsToPlugins=1; preferences.pathToLogsAndData=logDirectory;
    preferences.featuresToLoad=features; preferences.numFeaturesToLoad=3;
    preferences.flags=sl::PreferenceFlags::eDisableCLStateTracking|sl::PreferenceFlags::eUseFrameBasedResourceTagging;
    preferences.projectId="45d35067-277b-4e3f-b2f6-e7dd197c609e"; preferences.engine=sl::EngineType::eCustom; preferences.engineVersion="TwinKart-Original-D3D12";
    preferences.renderAPI=sl::RenderAPI::eD3D12; preferences.showConsole=false; preferences.logLevel=sl::LogLevel::eVerbose;
    auto result=impl->initialize(preferences,sl::kSDKVersion); impl->initialized=result==sl::Result::eOk; return Code(result);
}
FARPROC FrameGeneration::GraphicsEntry(const char* name) const {
    if(!impl->initialized || !name || (strcmp(name,"CreateDXGIFactory1") && strcmp(name,"CreateDXGIFactory2") && strcmp(name,"D3D12CreateDevice"))) return nullptr;
    return GetProcAddress(impl->module,name);
}
unsigned FrameGeneration::BindDevice(ID3D12Device* device) {
    if(!impl->Owner()) return WrongThread;
    if(!impl->initialized || impl->bound || !device) return Invalid;
    auto result=impl->setDevice(device); if(result!=sl::Result::eOk) return Code(result);
    if(!impl->Feature(impl->state,sl::kFeatureDLSS_G,"slDLSSGGetState") || !impl->Feature(impl->setOptions,sl::kFeatureDLSS_G,"slDLSSGSetOptions") ||
       !impl->Feature(impl->reflex,sl::kFeatureReflex,"slReflexSetOptions") || !impl->Feature(impl->sleep,sl::kFeatureReflex,"slReflexSleep") ||
       !impl->Feature(impl->marker,sl::kFeaturePCL,"slPCLSetMarker")) return Invalid;
    sl::ReflexOptions reflex{}; reflex.mode=sl::ReflexMode::eLowLatency;
    result=impl->reflex(reflex); if(result!=sl::Result::eOk) return Code(result);
    impl->device=device; device->AddRef(); impl->bound=true; return 0;
}
unsigned FrameGeneration::Configure(unsigned multiplier,unsigned width,unsigned height,unsigned format,unsigned buffers) {
    if(!impl->Owner()) return WrongThread;
    if(!impl->bound || multiplier<2 || multiplier>6 || width<128 || height<128 || width>8192 || height>8192 || buffers<2 || buffers>4) return Invalid;
    sl::DLSSGState state{}; auto result=impl->state(impl->viewport,state,nullptr);
    if(result!=sl::Result::eOk) return Code(result);
    impl->result.maximumGeneratedFrames=state.numFramesToGenerateMax;
    if(multiplier-1>state.numFramesToGenerateMax) return Invalid;
    auto& options=impl->options; options.mode=sl::DLSSGMode::eOn; options.numFramesToGenerate=multiplier-1;
    options.colorWidth=width; options.colorHeight=height; options.colorBufferFormat=format; options.numBackBuffers=buffers;
    options.enableUserInterfaceRecomposition=sl::Boolean::eTrue;
    result=impl->setOptions(impl->viewport,options); impl->configured=result==sl::Result::eOk; return Code(result);
}
unsigned FrameGeneration::Impl::Camera(const CameraFrame& camera) {
    if(!Owner()) return WrongThread;
    if(!configured || !token || !Finite(camera.viewToClip,16) || !Finite(camera.clipToView,16) || !Finite(camera.clipToPrevious,16) || !Finite(camera.previousToClip,16) ||
       !Finite(camera.position,3) || !Finite(camera.up,3) || !Finite(camera.right,3) || !Finite(camera.forward,3) ||
       !std::isfinite(camera.jitterX) || !std::isfinite(camera.jitterY) || !std::isfinite(camera.motionScaleX) || !std::isfinite(camera.motionScaleY) ||
       !camera.motionScaleX || !camera.motionScaleY || !std::isfinite(camera.nearPlane) || !std::isfinite(camera.farPlane) || camera.nearPlane<=0 || camera.farPlane<=camera.nearPlane ||
       !std::isfinite(camera.fieldOfView) || camera.fieldOfView<=0 || camera.fieldOfView>=3.141593f || !std::isfinite(camera.aspect) || camera.aspect<=0) return Invalid;
    sl::Constants values{};
    const float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    memcpy(&values.clipToLensClip,identity,sizeof(identity)); values.cameraPinholeOffset={0,0};
    memcpy(&values.cameraViewToClip,camera.viewToClip,sizeof(camera.viewToClip)); memcpy(&values.clipToCameraView,camera.clipToView,sizeof(camera.clipToView));
    memcpy(&values.clipToPrevClip,camera.clipToPrevious,sizeof(camera.clipToPrevious)); memcpy(&values.prevClipToClip,camera.previousToClip,sizeof(camera.previousToClip));
    memcpy(&values.cameraPos,camera.position,sizeof(camera.position)); memcpy(&values.cameraUp,camera.up,sizeof(camera.up));
    memcpy(&values.cameraRight,camera.right,sizeof(camera.right)); memcpy(&values.cameraFwd,camera.forward,sizeof(camera.forward));
    values.jitterOffset={camera.jitterX,camera.jitterY}; values.mvecScale={camera.motionScaleX,camera.motionScaleY};
    values.cameraNear=camera.nearPlane; values.cameraFar=camera.farPlane; values.cameraFOV=camera.fieldOfView; values.cameraAspectRatio=camera.aspect;
    values.depthInverted=camera.invertedDepth?sl::Boolean::eTrue:sl::Boolean::eFalse; values.reset=camera.reset?sl::Boolean::eTrue:sl::Boolean::eFalse;
    values.cameraMotionIncluded=sl::Boolean::eTrue; values.motionVectors3D=sl::Boolean::eFalse;
    values.motionVectorsJittered=sl::Boolean::eFalse; values.orthographicProjection=sl::Boolean::eFalse; values.motionVectorsDilated=sl::Boolean::eFalse;
    return Code(constants(values,*token,viewport));
}
unsigned FrameGeneration::BeginFrame(unsigned index,const CameraFrame& camera) {
    if(!impl->Owner())return WrongThread;if(!impl->configured || impl->acceptingNative.load())return Invalid;
    auto result=impl->frameToken(impl->token,&index);if(result!=sl::Result::eOk)return Code(result);if(!impl->token)return Invalid;
    result=impl->sleep(*impl->token);if(result!=sl::Result::eOk)return Code(result);
    return impl->Camera(camera);
}
void FrameGeneration::Impl::NativeMarker(FrameStage stage,unsigned long long id) {
    sl::FrameToken* selected=nullptr;bool valid=false;
    AcquireSRWLockExclusive(&nativeLock);
    if(stage==FrameStage::SimulationStart && !simulationThread)simulationThread=GetCurrentThreadId();
    bool sim=stage==FrameStage::SimulationStart || stage==FrameStage::SimulationEnd;
    auto ticket=(!sim || simulationThread==GetCurrentThreadId())?nativeFrames.Advance(stage,id,GetTickCount64()):nullptr;
    if(ticket) {
        if(stage==FrameStage::SimulationStart) {
            auto tokenResult=frameToken(selected,&ticket->index);
            if(tokenResult==sl::Result::eOk && selected)ticket->token=selected;else ticket->failed=true;
        } else selected=static_cast<sl::FrameToken*>(ticket->token);
        valid=selected && !ticket->failed;
    }
    ReleaseSRWLockExclusive(&nativeLock);
    if(!ticket)return;
    sl::Result markerResult=valid?sl::Result::eOk:static_cast<sl::Result>(Invalid);
    // Pacing occurs on the actual native simulation thread before its start
    // marker. Never sleep while holding the ledger lock needed by rendering.
    if(valid && stage==FrameStage::SimulationStart)markerResult=sleep(*selected);
    if(markerResult==sl::Result::eOk) {
        const sl::PCLMarker stages[]{sl::PCLMarker::eSimulationStart,sl::PCLMarker::eSimulationEnd,
            sl::PCLMarker::eRenderSubmitStart,sl::PCLMarker::eRenderSubmitEnd,sl::PCLMarker::ePresentStart,sl::PCLMarker::ePresentEnd};
        markerResult=marker(stages[static_cast<unsigned>(stage)],*selected);
    }
    AcquireSRWLockExclusive(&nativeLock);nativeFrames.Complete(id,stage,markerResult==sl::Result::eOk);ReleaseSRWLockExclusive(&nativeLock);
}
bool FrameGeneration::AttachNativeMarkers() {
    if(!impl->Owner() || !impl->configured || impl->nativeSink)return false;
    auto selected=std::make_shared<Impl::Sink>(impl);
    impl->acceptingNative.store(true,std::memory_order_release);
    if(!BindNativeFrameMarkerSink(selected)){impl->acceptingNative.store(false);return false;}
    impl->nativeSink=selected;return true;
}
unsigned FrameGeneration::BeginNativeFrame(unsigned long long nativeFrame,const CameraFrame& input) {
    if(!impl->Owner())return WrongThread;if(!impl->configured || !impl->acceptingNative.load())return Invalid;
    AcquireSRWLockExclusive(&impl->nativeLock);
    auto ticket=impl->nativeFrames.ForPresent(nativeFrame,GetTickCount64());
    impl->token=ticket?static_cast<sl::FrameToken*>(ticket->token):nullptr;
    ReleaseSRWLockExclusive(&impl->nativeLock);if(!impl->token)return MissingGuides;
    CameraFrame camera=input;camera.reset=camera.reset || !impl->haveNativeUsed || nativeFrame!=impl->lastNativeUsed+1;
    unsigned result=impl->Camera(camera);
    if(!result){impl->lastNativeUsed=nativeFrame;impl->haveNativeUsed=true;}
    return result;
}
bool FrameGeneration::AdoptInitialRenderThread() {
    if(!impl->bound || impl->token)return impl->Owner();
    impl->thread=GetCurrentThreadId();return true;
}
unsigned FrameGeneration::SetActive(bool enabled) {
    if(!impl->Owner() || !impl->configured)return Invalid;
    auto desired=enabled?sl::DLSSGMode::eOn:sl::DLSSGMode::eOff;
    if(impl->options.mode==desired)return 0;
    auto changed=impl->options;changed.mode=desired;
    auto result=impl->setOptions(impl->viewport,changed);
    if(result==sl::Result::eOk)impl->options=changed;
    return Code(result);
}
unsigned FrameGeneration::Marker(FrameStage stage) {
    if(!impl->Owner()) return WrongThread;
    if(!impl->token || !impl->marker) return Invalid;
    sl::PCLMarker marker;
    switch(stage) {
        case FrameStage::SimulationStart:marker=sl::PCLMarker::eSimulationStart;break;
        case FrameStage::SimulationEnd:marker=sl::PCLMarker::eSimulationEnd;break;
        case FrameStage::RenderSubmitStart:marker=sl::PCLMarker::eRenderSubmitStart;break;
        case FrameStage::RenderSubmitEnd:marker=sl::PCLMarker::eRenderSubmitEnd;break;
        case FrameStage::PresentStart:marker=sl::PCLMarker::ePresentStart;break;
        case FrameStage::PresentEnd:marker=sl::PCLMarker::ePresentEnd;break;
        default:return Invalid;
    }
    return Code(impl->marker(marker,*impl->token));
}
unsigned FrameGeneration::Tag(const GenerationResources& input) {
    if(!impl->Owner()) return WrongThread;
    if(!impl->token || !impl->configured || !impl->Resource(input.commands)) return Invalid;
    if(!input.guidesValidated || !input.hudAndUiValidated) return MissingGuides;
    ID3D12Resource* resources[]={input.depth,input.motion,input.hudless,input.uiAlpha};
    D3D12_RESOURCE_STATES states[]={input.depthState,input.motionState,input.hudlessState,input.uiState};
    sl::Resource views[4];
    for(unsigned i=0;i<4;++i) {
        if(!impl->Resource(resources[i])) return Invalid;
        auto desc=resources[i]->GetDesc(); if(desc.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D || desc.DepthOrArraySize!=1 || desc.MipLevels!=1 || desc.SampleDesc.Count!=1) return Invalid;
        if(i==3 && desc.Format!=DXGI_FORMAT_R8_UNORM && desc.Format!=DXGI_FORMAT_R16_FLOAT && desc.Format!=DXGI_FORMAT_R32_FLOAT) return Invalid;
        if(i>=2 && (desc.Width!=impl->options.colorWidth || desc.Height!=impl->options.colorHeight)) return Invalid;
        views[i]=sl::Resource{sl::ResourceType::eTex2d,resources[i],static_cast<unsigned>(states[i])};
        views[i].width=static_cast<unsigned>(desc.Width); views[i].height=desc.Height; views[i].nativeFormat=desc.Format;
    }
    if(views[0].width!=views[1].width || views[0].height!=views[1].height) return Invalid;
    sl::ResourceTag tags[]={
        {&views[0],sl::kBufferTypeDepth,sl::ResourceLifecycle::eValidUntilPresent},
        {&views[1],sl::kBufferTypeMotionVectors,sl::ResourceLifecycle::eValidUntilPresent},
        {&views[2],sl::kBufferTypeHUDLessColor,sl::ResourceLifecycle::eValidUntilPresent},
        {&views[3],sl::kBufferTypeUIAlpha,sl::ResourceLifecycle::eValidUntilPresent}};
    return Code(impl->tag(*impl->token,impl->viewport,tags,4,input.commands));
}
GenerationState FrameGeneration::Presented() {
    if(!impl->Owner() || !impl->bound) { GenerationState bad{}; bad.result=Invalid; return bad; }
    sl::DLSSGState state{}; auto result=impl->state(impl->viewport,state,nullptr);
    impl->result.result=Code(result); impl->result.status=static_cast<unsigned>(state.status);
    impl->result.lastPresented=state.numFramesActuallyPresented; impl->result.totalPresented+=state.numFramesActuallyPresented;
    impl->result.maximumGeneratedFrames=state.numFramesToGenerateMax;
    impl->result.inputCompletionFence=static_cast<ID3D12Fence*>(state.inputsProcessingCompletionFence); impl->result.inputCompletionValue=state.lastPresentInputsProcessingCompletionFenceValue;
    return impl->result;
}
void FrameGeneration::Shutdown() {
    if(!impl)return;
    AcquireSRWLockExclusive(&impl->nativeLock);impl->acceptingNative.store(false,std::memory_order_release);ReleaseSRWLockExclusive(&impl->nativeLock);
    if(impl->nativeSink)RemoveNativeFrameMarkerSink(impl->nativeSink);
    ULONGLONG start=GetTickCount64();
    while(impl->nativeCallbacks.load(std::memory_order_acquire) && GetTickCount64()-start<10000)Sleep(1);
    // If a provider callback hangs, retain its raw device/runtime resources
    // until process exit rather than free resources while it is executing.
    if(impl->nativeCallbacks.load(std::memory_order_acquire))return;
    impl->nativeSink.reset();
    if(impl->configured) { impl->options.mode=sl::DLSSGMode::eOff; impl->setOptions(impl->viewport,impl->options); impl->configured=false; }
    if(impl->initialized) { if(impl->bound) impl->freeResources(sl::kFeatureDLSS_G,impl->viewport); impl->shutdown(); impl->initialized=false; }
    if(impl->device) { impl->device->Release(); impl->device=nullptr; } impl->bound=false; impl->token=nullptr;
}
}
