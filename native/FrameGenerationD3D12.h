#pragma once
#include <windows.h>
#include <d3d12.h>
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
    unsigned result,status,maximumGeneratedFrames,lastPresented;
    unsigned long long totalPresented;
    ID3D12Fence* inputCompletionFence;
    unsigned long long inputCompletionValue;
};
enum class FrameStage { SimulationStart,SimulationEnd,RenderSubmitStart,RenderSubmitEnd,PresentStart,PresentEnd };
// Original Streamline integration. Load BEFORE any graphics API/device or
// factory creation. It neither identifies game passes nor invents camera data.
class FrameGeneration {
    struct Impl;
    Impl* impl;
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
    unsigned SetActive(bool);
    unsigned BeginFrame(unsigned index,const CameraFrame& camera);
    unsigned Marker(FrameStage stage);
    unsigned Tag(const GenerationResources& resources);
    GenerationState Presented();
    // Caller must first finish GPU work and stop presenting. Interposer module
    // remains loaded while any graphics proxy could still refer to its vtable.
    void Shutdown();
};
}
