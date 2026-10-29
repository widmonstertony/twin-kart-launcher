#pragma once
#include <d3d11.h>
namespace TwinKartGraphics {
using NativeNeuralLog=void(*)(const char*);
bool ConfigureNativeNeuralPass(const wchar_t* module,const wchar_t* provider,const wchar_t* cache,NativeNeuralLog);
// Exact-build experimental pre-HUD integration. The compute callback retains
// the current native guides; only the pixel callback submits NR output.
bool ProcessNativeNeuralPass(ID3D11DeviceContext*,unsigned frame);
bool ProcessNativeNeuralPixelPass(ID3D11DeviceContext*,unsigned frame);
bool NativeNeuralOutputMatches(ID3D11Resource*,unsigned frame);
// Release per-frame native references after presentation, including frames
// that did not reach the selected NR pass. Never retains a swapchain buffer.
void FinishNativeNeuralFrame(unsigned frame);
}
