#pragma once
#include <windows.h>
#include <d3d12.h>
#include <memory>
namespace TwinKartGraphics {
struct CameraFrame {
    float viewToClip[16],clipToView[16],clipToPrevious[16],previousToClip[16];
    float position[3],up[3],right[3],forward[3];
    float jitterX,jitterY,motionScaleX,motionScaleY,nearPlane,farPlane,fieldOfView,aspect;
    bool reset,invertedDepth;
};
struct GenerationResources {
    ID3D12GraphicsCommandList* commands;
    ID3D12Resource *depth,*motion,*hudless,*uiAlpha;
    D3D12_RESOURCE_STATES depthState,motionState,hudlessState,uiState;
    bool guidesValidated,hudAndUiValidated;
};
struct GenerationState {
    // lastPresented counts presentations since the previous vendor GetState
    // query; it is not a per-render count of generated frames. totalPresented
    // sums successful observations, including ordinary native presentations.
    unsigned result,status,maximumGeneratedFrames,lastPresented;
    unsigned long long totalPresented;
    ID3D12Fence* inputCompletionFence;
    unsigned long long inputCompletionValue;
};
enum class FrameStage { SimulationStart,SimulationEnd,RenderSubmitStart,RenderSubmitEnd,PresentStart,PresentEnd };
struct NativeFrameDiagnostic {
    unsigned seen=0,completed=0,tokenResult=0;
    bool live=false,failed=false,hasToken=false;
    unsigned long long nativeId=0,time=0,stageIds[6]{};
    DWORD simulationThread=0,stageThreads[6]{};
    unsigned rejected[6]{},markerResults[6]{};
};
// Original Streamline integration. Load BEFORE any graphics API/device or
// factory creation. It neither identifies game passes nor invents camera data.
class FrameGeneration {
#ifdef TWIN_KART_FRAME_GENERATION_FIXTURE
    friend struct FrameGenerationFixture;
#endif
    struct Impl;
    std::shared_ptr<Impl> impl;
public:
    FrameGeneration();
    ~FrameGeneration();
    FrameGeneration(const FrameGeneration&)=delete;
    FrameGeneration& operator=(const FrameGeneration&)=delete;
    unsigned Load(const wchar_t* externalRuntimeDirectory,const wchar_t* privateLogDirectory);
    FARPROC GraphicsEntry(const char* name) const;
    unsigned BindDevice(ID3D12Device* device);
    unsigned Configure(unsigned multiplier,unsigned displayWidth,unsigned displayHeight,unsigned colorFormat,unsigned backBuffers);
    // Serialized startup handoff only, before the first frame token exists.
    bool AdoptInitialRenderThread();
    // Caller must serialize Present and drain its queue/input-completion fence
    // before a native movie/loading/RHI thread handoff. Clears temporal use.
    bool AdoptDrainedRenderThread();
    unsigned SetActive(bool);
    unsigned BeginFrame(unsigned index,const CameraFrame& camera);
    bool AttachNativeMarkers();
    unsigned BeginNativeFrame(unsigned long long nativeFrame,const CameraFrame& camera);
    NativeFrameDiagnostic InspectNativeFrame(unsigned long long nativeFrame);
    unsigned Marker(FrameStage stage);
    unsigned Tag(const GenerationResources& resources);
    GenerationState Presented();
    // Caller must first finish GPU work and stop presenting. Interposer module
    // remains loaded while any graphics proxy could still refer to its vtable.
    void Shutdown();
};
}
