#pragma once
#include "FlagArenaDriver.h"
#include <cstddef>
namespace TwinKartNavigation {
using Point=TwinKartRace::ArenaPoint;
struct ActorArray {void** data=nullptr;int count=0,capacity=0;};
static_assert(sizeof(ActorArray)==16,"Native x64 actor-array ABI");
using CollectActors=void(__fastcall*)(void*,void*,ActorArray*);
using ReleaseActors=void(__fastcall*)(void*);
using ReadActorBounds=bool(*)(void*,void*,Point&,Point&,void*);
enum class BoundsState {Idle,Reading,Ready,Rejected};
// Bounded read-only aggregation over the original engine's actor list. The
// caller must Reset on its owning game thread before native runtime shutdown;
// no destructor invokes engine code during DLL/process destruction.
class WorldBounds {
    ActorArray actors{};ReleaseActors release=nullptr;ReadActorBounds read=nullptr;
    void* world=nullptr;void* context=nullptr;
    int next=0,accepted=0;Point low{},high{};BoundsState state=BoundsState::Idle;
    void Release(){if(actors.data && release)release(actors.data);actors={};}
public:
    WorldBounds()=default;
    WorldBounds(const WorldBounds&)=delete;
    WorldBounds& operator=(const WorldBounds&)=delete;
    void Reset(){Release();release=nullptr;read=nullptr;world=context=nullptr;next=accepted=0;low=high={};state=BoundsState::Idle;}
    BoundsState State()const{return state;}
    int Processed()const{return next;}
    int Accepted()const{return accepted;}
    bool Begin(void* owner,void* actorClass,CollectActors collect,ReadActorBounds bounds,ReleaseActors free,void* data=nullptr) {
        Reset();
        if(!owner || !actorClass || !collect || !bounds || !free){state=BoundsState::Rejected;return false;}
        world=owner;read=bounds;release=free;context=data;
        collect(owner,actorClass,&actors);
        if(!actors.data || actors.count<1 || actors.count>262144 || actors.capacity<actors.count || actors.capacity>1048576) {
            Release();state=BoundsState::Rejected;return false;
        }
        state=BoundsState::Reading;return true;
    }
    BoundsState Step(void* owner,unsigned budget) {
        if(state!=BoundsState::Reading)return state;
        if(owner!=world || !budget || budget>256){Release();state=BoundsState::Rejected;return state;}
        unsigned work=0;
        while(next<actors.count && work++<budget) {
            void* actor=actors.data[next++];Point center{},extent{};
            if(!actor || !read(actor,world,center,extent,context))continue;
            if(!TwinKartRace::Finite(center) || !TwinKartRace::Finite(extent) ||
               std::fabs(center.x)>1.e7f || std::fabs(center.y)>1.e7f || std::fabs(center.z)>1.e7f || extent.x<0 || extent.y<0 || extent.z<0 ||
               extent.x>1.e7f || extent.y>1.e7f || extent.z>1.e7f || (extent.x==0 && extent.y==0 && extent.z==0))continue;
            Point a{center.x-extent.x,center.y-extent.y,center.z-extent.z};
            Point b{center.x+extent.x,center.y+extent.y,center.z+extent.z};
            if(!TwinKartRace::Finite(a) || !TwinKartRace::Finite(b))continue;
            if(!accepted){low=a;high=b;}
            else {
                low.x=(std::min)(low.x,a.x);low.y=(std::min)(low.y,a.y);low.z=(std::min)(low.z,a.z);
                high.x=(std::max)(high.x,b.x);high.y=(std::max)(high.y,b.y);high.z=(std::max)(high.z,b.z);
            }
            ++accepted;
        }
        if(next==actors.count){Release();state=accepted?BoundsState::Ready:BoundsState::Rejected;}
        return state;
    }
    bool Result(Point& a,Point& b)const {
        if(state!=BoundsState::Ready)return false;
        a=low;b=high;return true;
    }
};
}
