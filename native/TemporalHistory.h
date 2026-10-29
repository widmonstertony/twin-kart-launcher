#pragma once
#include "FrameGenerationD3D12.h"
#include <cmath>
#include <cstdint>
namespace TwinKartGraphics {
// Only a successfully evaluated frame advances history. Menu gaps, rejected
// guides, camera controls and respawn cuts must never reuse stale features.
class TemporalHistory {
    CameraFrame previous{};
    unsigned lastFrame=0;
    std::uint64_t lastRevision=0;
    bool valid=false;
public:
    bool NeedsReset(unsigned frame,std::uint64_t revision,const CameraFrame& camera) const {
        if(!valid || frame!=lastFrame+1 || revision!=lastRevision || camera.reset)return true;
        if(std::fabs(camera.fieldOfView-previous.fieldOfView)>0.001f ||
           std::fabs(camera.aspect-previous.aspect)>0.001f ||
           camera.nearPlane!=previous.nearPlane || camera.invertedDepth!=previous.invertedDepth)return true;
        float distance=0,forward=0,up=0;
        for(unsigned i=0;i<3;++i) {
            float delta=camera.position[i]-previous.position[i];distance+=delta*delta;
            forward+=camera.forward[i]*previous.forward[i];up+=camera.up[i]*previous.up[i];
        }
        return !std::isfinite(distance) || !std::isfinite(forward) || !std::isfinite(up) ||
            distance>1000.0f*1000.0f || forward<0.5f || up<0.5f;
    }
    void Commit(unsigned frame,std::uint64_t revision,const CameraFrame& camera) {
        previous=camera;lastFrame=frame;lastRevision=revision;valid=true;
    }
};
// These guides and the post-resolve color are explicitly unjittered. Preserve
// the raw native camera in the caller; do not apply its projection jitter twice.
inline CameraFrame ResolvedPresentationCamera(const CameraFrame& native,unsigned width,unsigned height) {
    CameraFrame result=native;
    result.jitterX=result.jitterY=0;
    result.motionScaleX=width?1.0f/width:0;
    result.motionScaleY=height?1.0f/height:0;
    return result;
}
}
