#pragma once
#include <dxgi.h>
struct ID3D12Device;
namespace TwinKartGraphics {
using PresentationObserver=HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*,UINT,UINT);
using PresentationLog=void(*)(const char*);
// Opt-in factory interception, installed before the game's swapchain exists.
// A late install cannot convert an already-created native chain.
bool ConfigureNativePresentation(PresentationObserver,PresentationLog);
bool IsNativePresentation(IDXGISwapChain*);
bool NativePresentationOwnsNgxDevice(ID3D12Device*);
bool InsideNativePresentation();
unsigned NativePresentationFactoryHookMask(IDXGISwapChain*);
UINT NativePresentationBufferIndex(IDXGISwapChain*,unsigned renderFrame);
HRESULT PresentNativePresentation(IDXGISwapChain*,UINT,UINT,unsigned renderFrame);
}
