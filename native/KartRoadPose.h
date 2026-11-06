#pragma once
#include "NativeKartContacts.h"
namespace TwinKartRace {
namespace KartRoadPoseDetail {
inline ArenaPoint Add(const ArenaPoint& a,const ArenaPoint& b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline ArenaPoint Sub(const ArenaPoint& a,const ArenaPoint& b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline ArenaPoint Scale(const ArenaPoint& a,float s){return {a.x*s,a.y*s,a.z*s};}
inline float Dot(const ArenaPoint& a,const ArenaPoint& b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline ArenaPoint Cross(const ArenaPoint& a,const ArenaPoint& b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline bool Unit(const ArenaPoint& value,ArenaPoint& result) {
    float length=Dot(value,value);if(!Finite(value) || !std::isfinite(length) || length<1.e-8f)return false;
    result=Scale(value,1.f/std::sqrt(length));return true;
}
inline ComponentRotation Inverse(const ComponentRotation& q){return {-q.x,-q.y,-q.z,q.w};}
inline ComponentRotation Multiply(const ComponentRotation& a,const ComponentRotation& b) {
    return {a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
        a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};
}
inline ArenaPoint Rotate(const ComponentRotation& q,const ArenaPoint& p) {
    ArenaPoint vector{q.x,q.y,q.z};auto twice=Scale(Cross(vector,p),2);
    return Add(p,Add(Scale(twice,q.w),Cross(vector,twice)));
}
inline bool FrameRotation(const ArenaPoint& surfaceNormal,const ArenaPoint& forward,ComponentRotation& output) {
    ArenaPoint z{},x{},y{};if(!Unit(surfaceNormal,z) || z.z<=0.70f ||
        !Unit(Sub(forward,Scale(z,Dot(forward,z))),x) || !Unit(Cross(z,x),y))return false;
    ComponentRotation q;float trace=x.x+y.y+z.z;
    if(trace>0) {float s=2*std::sqrt(trace+1);q={ (y.z-z.y)/s,(z.x-x.z)/s,(x.y-y.x)/s,0.25f*s};}
    else if(x.x>y.y && x.x>z.z) {float s=2*std::sqrt(1+x.x-y.y-z.z);q={0.25f*s,(y.x+x.y)/s,(z.x+x.z)/s,(y.z-z.y)/s};}
    else if(y.y>z.z) {float s=2*std::sqrt(1+y.y-x.x-z.z);q={(y.x+x.y)/s,0.25f*s,(z.y+y.z)/s,(z.x-x.z)/s};}
    else {float s=2*std::sqrt(1+z.z-x.x-y.y);q={(z.x+x.z)/s,(z.y+y.z)/s,0.25f*s,(x.y-y.x)/s};}
    if(!SweepQuaternion(q))return false;output=q;return true;
}
}
struct KartRoadCalibration {
    ArenaPoint roadOrigin{},roadNormal{},forward{},bodyOffset{};
    ComponentRotation bodyRelativeRotation{};
    bool valid=false;
};
inline bool KartSameAttitude(const ComponentRotation& first,const ComponentRotation& last) {
    if(!SweepQuaternion(first) || !SweepQuaternion(last))return false;
    float dot=std::fabs(first.x*last.x+first.y*last.y+first.z*last.z+first.w*last.w);
    float a=first.x*first.x+first.y*first.y+first.z*first.z+first.w*first.w;
    float b=last.x*last.x+last.y*last.y+last.z*last.z+last.w*last.w;
    return dot/std::sqrt(a*b)>=0.9999995f;
}
// Derive the body/road relationship from actual stable original wheel contacts
// and the actual root pose. No visual bounds, assumed wheelbase/body size or
// fixed ground lift is used. Uneven/nonplanar support is left to native physics.
inline bool CalibrateKartRoad(const KartContactSnapshot& contacts,const ArenaPoint& bodyPosition,
    const ComponentRotation& bodyRotation,KartRoadCalibration& output) {
    using namespace KartRoadPoseDetail;
    if(!SweepPoint(bodyPosition) || !SweepQuaternion(bodyRotation))return false;
    ArenaPoint center{};for(const auto& wheel:contacts.wheels) {
        if(!wheel.component || !SweepPoint(wheel.point))return false;center=Add(center,Scale(wheel.point,0.25f));
    }
    ArenaPoint best{};float area=0;
    for(unsigned a=1;a<4;++a)for(unsigned b=a+1;b<4;++b) {
        auto candidate=Cross(Sub(contacts.wheels[a].point,contacts.wheels[0].point),Sub(contacts.wheels[b].point,contacts.wheels[0].point));
        float score=Dot(candidate,candidate);if(std::isfinite(score) && score>area){area=score;best=candidate;}
    }
    ArenaPoint normal{};if(area<1 || !Unit(best,normal))return false;
    if(normal.z<0)normal=Scale(normal,-1);
    if(normal.z<=0.70f || Dot(Rotate(bodyRotation,{0,0,1}),normal)<=0.1f)return false;
    // Five native world units are a planarity tolerance, not a body height or
    // collision margin. Reject mixed ledges/uneven contacts rather than infer
    // a fictitious flat support surface for them.
    for(const auto& wheel:contacts.wheels)if(std::fabs(Dot(Sub(wheel.point,center),normal))>5)return false;
    ArenaPoint origin{bodyPosition.x,bodyPosition.y,
        center.z-(normal.x*(bodyPosition.x-center.x)+normal.y*(bodyPosition.y-center.y))/normal.z};
    auto forward=Rotate(bodyRotation,{1,0,0});ComponentRotation frame{};
    if(!SweepPoint(origin) || !FrameRotation(normal,forward,frame))return false;
    KartRoadCalibration calibration;calibration.roadOrigin=origin;calibration.roadNormal=normal;calibration.forward=forward;
    calibration.bodyOffset=Rotate(Inverse(frame),Sub(bodyPosition,origin));
    calibration.bodyRelativeRotation=Multiply(Inverse(frame),bodyRotation);calibration.valid=true;
    if(!Finite(calibration.bodyOffset) || !SweepQuaternion(calibration.bodyRelativeRotation))return false;
    output=calibration;return true;
}
// A geometric pose mapping, not a future suspension/steering simulation or
// proof of continuous angular clearance. Native sweep/ground/lease validation
// must separately qualify every use of the mapped original body pose.
inline bool MapKartRoadPose(const KartRoadCalibration& calibration,const ArenaPoint& point,const ArenaPoint& normal,
    const ArenaPoint& forward,ArenaPoint& bodyPosition,ComponentRotation& bodyRotation) {
    using namespace KartRoadPoseDetail;
    if(!calibration.valid || !SweepPoint(point) || !Finite(calibration.bodyOffset) || !SweepQuaternion(calibration.bodyRelativeRotation))return false;
    ComponentRotation frame{};if(!FrameRotation(normal,forward,frame))return false;
    auto position=Add(point,Rotate(frame,calibration.bodyOffset));auto rotation=Multiply(frame,calibration.bodyRelativeRotation);
    if(!SweepPoint(position) || !SweepQuaternion(rotation))return false;
    bodyPosition=position;bodyRotation=rotation;return true;
}
}
