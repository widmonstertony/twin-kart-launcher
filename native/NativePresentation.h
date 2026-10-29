#pragma once
#include <dxgi.h>
namespace TwinKartGraphics {
using PresentationObserver=HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*,UINT,UINT);
using PresentationLog=void(*)(const char*);
// Opt-in factory interception, installed before the game's swapchain exists.
// A late install cannot convert an already-created native chain.
bool ConfigureNativePresentation(PresentationObserver,PresentationLog);
bool IsNativePresentation(IDXGISwapChain*);
bool InsideNativePresentation();
HRESULT PresentNativePresentation(IDXGISwapChain*,UINT,UINT,unsigned renderFrame);
}
