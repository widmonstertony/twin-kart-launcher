#pragma once
#include <d3d11.h>
namespace TwinKartGraphics {
// Opt-in, bounded capture of a selected real temporal pass. Contains texture
// pixels and camera constants only; never saves shader instructions.
bool ConfigureTemporalCapture(const wchar_t* directory,const char* shaderHash,unsigned maximumFrames=2,bool waitForRequest=false);
bool CaptureTemporalInputs(ID3D11DeviceContext*,unsigned frame);
}
