#pragma once
#include <d3d11.h>
#include "FrameGenerationD3D12.h"
namespace TwinKartGraphics {
using NativeNeuralLog=void(*)(const char*);
using NativeNeuralCoreOwner=bool(*)(ID3D12Device*);
bool ConfigureNativeNeuralPass(const wchar_t* module,const char* expectedModuleSha256,const wchar_t* provider,const wchar_t* cache,NativeNeuralLog,NativeNeuralCoreOwner=nullptr);
// Exact-build experimental pre-HUD integration. The compute callback retains
// the current native guides; only the pixel callback submits NR output.
bool ProcessNativeNeuralPass(ID3D11DeviceContext*,unsigned frame);
bool ProcessNativeNeuralPixelPass(ID3D11DeviceContext*,unsigned frame);
bool NativeNeuralOutputMatches(ID3D11Resource*,unsigned frame);
bool NativeSuperResolutionOutputMatches(ID3D11Resource*,unsigned frame);
struct NativePresentationInputs {
    ID3D11Texture2D *depth=nullptr,*motion=nullptr,*hudless=nullptr;
    CameraFrame camera{};
    unsigned long long nativeFrame=0;
    bool nativeFrameObserved=false;
};
// Borrowed resources are current-frame only; copy/consume before FinishFrame.
bool GetNativePresentationInputs(unsigned frame,NativePresentationInputs&);
// Called only by the real native Present boundary after a serialized context
// handoff. Drains and retires the previous owner's features; the next qualified
// draw initializes a fresh runtime/history on its actual render thread.
bool RetireNativeNeuralForRenderThread(ID3D11DeviceContext*);
// Before the external NGX owner shuts down. Caller has stopped presentation
// and drained its inputs; this drains/retire our features before that owner.
bool RetireNativeNeuralForDeviceShutdown(ID3D11DeviceContext*);
// Release per-frame native references after presentation, including frames
// that did not reach the selected NR pass. Never retains a swapchain buffer.
void FinishNativeNeuralFrame(unsigned frame);
}
