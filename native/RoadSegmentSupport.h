#pragma once
#include "NativeRoadQuery.h"
namespace TwinKartRace {
enum class SupportState {Idle,Reading,Supported,Rejected};
using SurfaceReader=RoadSurfaceResult(*)(const ArenaPoint&,const ArenaPoint&,RoadSurface&,void*);
// Independently authored bounded centerline support check. A clear horizontal
// ray alone cannot distinguish a road from empty space between two ledges.
// This is sampled ground support, not a vehicle-volume sweep or jump planner.
class RoadSegmentSupport {
    SupportState state=SupportState::Idle;
    ArenaPoint first{},last{},previous{};
    unsigned steps=0,next=0;
    float distance=0;
public:
    void Reset(){state=SupportState::Idle;first=last=previous={};steps=next=0;distance=0;}
    SupportState State()const{return state;}
    unsigned SamplesRead()const{return next;}
    bool Begin(const ArenaPoint& a,const ArenaPoint& b,float spacing=50.f) {
        Reset();
        if(!Finite(a) || !Finite(b) || !std::isfinite(spacing) || spacing<50.f || spacing>100.f) {
            state=SupportState::Rejected;return false;
        }
        distance=ArenaDistance(a,b);
        if(distance<1 || distance>1500 || std::fabs(a.z-b.z)>distance*0.60f+15) {
            state=SupportState::Rejected;return false;
        }
        first=a;last=b;steps=static_cast<unsigned>(std::ceil(distance/spacing));
        state=SupportState::Reading;return true;
    }
    // Returns actual callback count, so a caller can share one strict native
    // ray budget across a partially completed edge and subsequent edges.
    unsigned Step(unsigned budget,SurfaceReader read,void* context) {
        if(state!=SupportState::Reading || !budget)return 0;
        if(!read || budget>256){state=SupportState::Rejected;return 0;}
        unsigned used=0;
        while(used<budget && state==SupportState::Reading) {
            float t=static_cast<float>(next)/steps;
            ArenaPoint expected{first.x+(last.x-first.x)*t,first.y+(last.y-first.y)*t,
                                first.z+(last.z-first.z)*t};
            auto start=expected,end=expected;start.z+=72;end.z-=72;
            RoadSurface contact{};auto result=read(start,end,contact,context);++used;
            bool valid=result==RoadSurfaceResult::Hit && Finite(contact.point) &&
                std::isfinite(contact.normalZ) && contact.normalZ>0.70f && contact.normalZ<=1.01f &&
                !contact.penetrating && std::fabs(contact.point.x-expected.x)<=2 &&
                std::fabs(contact.point.y-expected.y)<=2 && std::fabs(contact.point.z-expected.z)<=45;
            if(valid && next)valid=std::fabs(contact.point.z-previous.z)<=
                ArenaDistance(contact.point,previous)*0.65f+8;
            if(!valid){state=SupportState::Rejected;break;}
            previous=contact.point;++next;
            if(next>steps)state=SupportState::Supported;
        }
        return used;
    }
};
}
