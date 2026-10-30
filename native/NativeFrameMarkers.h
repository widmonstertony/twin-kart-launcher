#pragma once
#include <memory>
#include "FrameGenerationD3D12.h"
namespace TwinKartGraphics {
struct NativeFrameMarkerSink {
    virtual void Observe(FrameStage,unsigned long long nativeFrame)=0;
    virtual ~NativeFrameMarkerSink()=default;
};
using NativeMarkerLog=void(*)(const char*);
// Exact-build native interface interception, after unpacking at chain creation.
// Native simulation/render/present callbacks keep their actual thread/ID.
bool ConfigureNativeFrameMarkers(NativeMarkerLog);
bool BindNativeFrameMarkerSink(const std::shared_ptr<NativeFrameMarkerSink>&);
void RemoveNativeFrameMarkerSink(const std::shared_ptr<NativeFrameMarkerSink>&);
// Only the calling native render/present thread's observed frame is returned.
bool CurrentNativeRenderFrame(unsigned long long& nativeFrame);
#ifdef TWIN_KART_SYNTHETIC_MARKER_INPUT
// Standalone first-party fixture only. The production game module does not
// compile this input; its frame IDs still come exclusively from native hooks.
void ObserveSyntheticNativeMarker(FrameStage,unsigned long long nativeFrame);
#endif
}
