#pragma once
#include <d3d11.h>
namespace TwinKartGraphics {
using TemporalPassLog=void(*)(const char*);
bool ConfigureNativeTemporalPass(const wchar_t* module,const char* sha256,const wchar_t* provider,const wchar_t* cache,TemporalPassLog);
// Before Dispatch. True means an exact, qualified native output was replaced;
// false requires the caller to execute the original native dispatch unchanged.
bool ProcessNativeTemporalPass(ID3D11DeviceContext*,unsigned frame,UINT x,UINT y,UINT z);
bool NativeTemporalSuperResolutionRan(unsigned frame);
}
