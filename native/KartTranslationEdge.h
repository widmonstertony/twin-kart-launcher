#pragma once
#include "KartRoadPose.h"
#include "RoadSegmentSupport.h"
#include <array>
namespace TwinKartRace {
using TranslationBodyQuery=VolumeSweep(*)(const ArenaPoint&,const ArenaPoint&,const ComponentRotation&,void*);
using TranslationLeaseValid=bool(*)(void*);
// Qualify a graph edge with the current native body, not its centerline alone.
// Only fixed-attitude, straight body translations are supported here. Every
// sampled actual road normal must retain that attitude and its mapped body
// position must lie on the queried translation. Turns/airborne trajectories
// require a separate angular/dynamics qualification; they are not inferred.
inline VolumeSweep QueryKartTranslationEdge(const KartRoadCalibration& calibration,
    const ComponentRotation& originalRotation,const ArenaPoint& first,const ArenaPoint& last,
    SurfaceReader read,TranslationBodyQuery body,TranslationLeaseValid alive,void* context) {
    if(!calibration.valid || !SweepQuaternion(originalRotation) || !SweepPoint(first) || !SweepPoint(last) ||
       !read || !body || !alive || !alive(context))return {};
    float distance=ArenaDistance(first,last);
    if(!std::isfinite(distance) || distance<1 || distance>1500 || std::fabs(first.z-last.z)>distance*0.60f+15)return {};
    unsigned steps=static_cast<unsigned>(std::ceil(distance/50.f));
    std::array<ArenaPoint,31> poses{};ArenaPoint previous{};
    for(unsigned sample=0;sample<=steps;++sample) {
        float t=static_cast<float>(sample)/steps;
        ArenaPoint expected{first.x+(last.x-first.x)*t,first.y+(last.y-first.y)*t,first.z+(last.z-first.z)*t};
        auto start=expected,end=expected;start.z+=72;end.z-=72;
        RoadSurface surface{};
        if(!alive(context) || read(start,end,surface,context)!=RoadSurfaceResult::Hit || !alive(context) ||
           surface.penetrating || !SweepPoint(surface.point) || !Finite(surface.normal) ||
           !std::isfinite(surface.normalZ) || surface.normalZ<=0.70f || surface.normalZ>1.01f ||
           std::fabs(surface.normalZ-surface.normal.z)>0.001f ||
           std::fabs(surface.point.x-expected.x)>2 || std::fabs(surface.point.y-expected.y)>2 ||
           std::fabs(surface.point.z-expected.z)>45)return {};
        float norm=KartRoadPoseDetail::Dot(surface.normal,surface.normal);
        if(!std::isfinite(norm) || std::fabs(norm-1.f)>0.03f || (sample &&
           std::fabs(surface.point.z-previous.z)>ArenaDistance(surface.point,previous)*0.65f+8))return {};
        ComponentRotation rotation{};
        if(!MapKartRoadPose(calibration,surface.point,surface.normal,calibration.forward,poses[sample],rotation) ||
           !KartSameAttitude(originalRotation,rotation))return {};
        previous=surface.point;
    }
    // A single original component sweep has a straight path. A sampled bend
    // cannot be substituted for that path merely because its ends are clear.
    for(unsigned sample=1;sample<steps;++sample) {
        float t=static_cast<float>(sample)/steps;
        auto expected=KartRoadPoseDetail::Add(poses[0],KartRoadPoseDetail::Scale(
            KartRoadPoseDetail::Sub(poses[steps],poses[0]),t));
        auto delta=KartRoadPoseDetail::Sub(expected,poses[sample]);
        if(KartRoadPoseDetail::Dot(delta,delta)>0.0001f)return {};
    }
    if(ArenaDistance(poses[0],poses[steps])<1 || !alive(context))return {};
    auto result=body(poses[0],poses[steps],originalRotation,context);
    return alive(context)?result:VolumeSweep{};
}
}
