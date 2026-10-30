#pragma once
#include <d3d12.h>
#ifdef TWIN_KART_NEURAL_BUILD
#define TK_NEURAL extern "C" __declspec(dllexport)
#else
#define TK_NEURAL extern "C" __declspec(dllimport)
#endif
struct TwinKartNeuralFrame {
    unsigned size;
    ID3D12GraphicsCommandList* commands;
    ID3D12Resource *color,*depth,*motion,*output;
    float motionScaleX,motionScaleY,intensity;
    bool reset,invertedDepth,guidesValidated;
};
struct TwinKartNeuralState {
    unsigned size,initializeResult,createResult,evaluateResult,width,height;
    unsigned long long evaluations;
    bool initialized,featureCreated,privateProvider;
    unsigned privateInitializeResult,privatePopulateResult;
    bool externalCoreLifetime;
    unsigned requestedPasses,style,passResults[2];
    unsigned long long passEvaluations[2];
};
struct TwinKartSuperResolutionFrame {
    unsigned size;
    ID3D12GraphicsCommandList* commands;
    ID3D12Resource *color,*depth,*motion,*output;
    float jitterX,jitterY,motionScaleX,motionScaleY,frameMilliseconds,preExposure,exposureScale;
    bool reset,guidesValidated,hudlessValidated;
};
struct TwinKartSuperResolutionState {
    unsigned size,createResult,evaluateResult,inputWidth,inputHeight,outputWidth,outputHeight;
    unsigned long long evaluations;
    bool featureCreated;
};
// The caller owns barriers, command submission, fences and presentation. A
// successful evaluate records GPU work; it never proves final visible output.
using TwinKartNeuralLogCallback=void(*)(const char*,unsigned,unsigned);
TK_NEURAL unsigned TwinKartNeuralInitialize(ID3D12Device*,const wchar_t* cache,const wchar_t* provider,TwinKartNeuralLogCallback log=nullptr,bool allowPrivateAbi=false);
// The caller has an already-initialized NGX instance on this exact native
// device. Retiring our own features must not shut down its other features.
// The external owner must outlive us and drain/retire us before its shutdown.
TK_NEURAL unsigned TwinKartNeuralInitializeBorrowed(ID3D12Device*,const wchar_t* cache,const wchar_t* provider,TwinKartNeuralLogCallback log=nullptr,bool allowPrivateAbi=false);
TK_NEURAL unsigned TwinKartNeuralCreate(ID3D12GraphicsCommandList*,unsigned width,unsigned height);
// Configure before Create. Each pass owns a distinct temporal feature. Style
// follows the pinned provider interface: 0 default, 1 natural, 2 cinematic.
TK_NEURAL bool TwinKartNeuralConfigure(unsigned passes,unsigned style);
TK_NEURAL unsigned TwinKartNeuralEvaluate(const TwinKartNeuralFrame*);
TK_NEURAL bool TwinKartNeuralGetState(TwinKartNeuralState*);
// Uses the same initialized NGX D3D12 device as the neural consumer. Dimensions
// must describe a real smaller render buffer and larger target. The caller
// supplies viewport-correct motion, depth, exposure and jitter, before the HUD.
TK_NEURAL unsigned TwinKartSuperResolutionCreate(ID3D12GraphicsCommandList*,unsigned inputWidth,unsigned inputHeight,unsigned outputWidth,unsigned outputHeight,int quality,bool hdr,bool invertedDepth);
TK_NEURAL unsigned TwinKartSuperResolutionEvaluate(const TwinKartSuperResolutionFrame*);
TK_NEURAL bool TwinKartSuperResolutionGetState(TwinKartSuperResolutionState*);
// Call only after the caller's fence proves all submitted work has completed.
TK_NEURAL void TwinKartNeuralShutdown();
// Explicit renderer handoff only: caller has excluded all old CPU use and
// drained its queues/bridges. The fence must belong to this exact device and
// its nonzero value must actually be complete. Features are retired, never
// transferred to a new thread with old temporal history.
TK_NEURAL bool TwinKartNeuralShutdownDrained(unsigned expectedOwnerThread,ID3D12Fence*,unsigned long long completedValue);
