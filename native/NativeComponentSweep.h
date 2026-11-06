#pragma once
#include "NativeVehicleSweep.h"
#include <array>
namespace TwinKartRace {
struct alignas(16) ComponentRotation {float x=0,y=0,z=0,w=1;};
struct ComponentHits {void* data=nullptr;int count=0,capacity=0;};
static_assert(sizeof(ComponentRotation)==16 && sizeof(ComponentHits)==16,"Original component sweep x64 values");
using ConstructCollisionParams=void*(__fastcall*)(void*,std::uint64_t,const void*,bool,void*);
using ConstructCollisionResponses=void(__fastcall*)(void*);
using SetCollisionResponses=void(__fastcall*)(void*,int);
using InitComponentSweep=void(__fastcall*)(void*,void*,void*);
using ComponentBodySweep=bool(__fastcall*)(void*,ComponentHits*,void*,const ArenaPoint*,const ArenaPoint*,
    const ComponentRotation*,const void*);
using ReleaseSweepAllocation=void(__fastcall*)(void*);
// Must revalidate world/physics scene, owning pawn/current root, component
// serial, nonzero original geometry, original body and physics actor handle.
using ComponentSweepValid=bool(*)(void*,void*,void*,void*);
struct ComponentSweepApi {
    ConstructCollisionParams construct=nullptr;ConstructCollisionResponses responses=nullptr;
    SetCollisionResponses setResponses=nullptr;InitComponentSweep initialize=nullptr;
    ComponentBodySweep sweep=nullptr;ReleaseSweepAllocation release=nullptr;
};
enum class ComponentContactDecision {Obstacle,Approach,Invalid};
using ComponentContactPolicy=ComponentContactDecision(*)(void*,void*,void*,const unsigned char*,void*);
inline bool SweepQuaternion(const ComponentRotation& rotation) {
    float norm=rotation.x*rotation.x+rotation.y*rotation.y+rotation.z*rotation.z+rotation.w*rotation.w;
    return std::isfinite(rotation.x) && std::isfinite(rotation.y) && std::isfinite(rotation.z) &&
        std::isfinite(rotation.w) && std::isfinite(norm) && std::fabs(norm-1.f)<=0.002f;
}
namespace ComponentSweepDetail {
constexpr std::uint64_t Guard=0x6347d28fa091be5cull;
struct alignas(16) Params {unsigned char bytes[0x70]{};std::uint64_t guard=Guard,padding=0;};
struct alignas(16) Responses {unsigned char bytes[32]{};std::uint64_t guard=Guard,padding=0;};
struct alignas(16) Hits {ComponentHits value{};std::uint64_t guard=Guard,padding=0;};
static_assert(offsetof(Params,guard)==0x70 && offsetof(Responses,guard)==32 && offsetof(Hits,guard)==16,
    "Native constructor/query scratch boundaries");
inline void* Pointer(const Params& params,unsigned offset) {void* pointer=nullptr;memcpy(&pointer,params.bytes+offset,8);return pointer;}
inline bool InlineArray(const Params& params,unsigned pointerOffset,unsigned inlineCapacity) {
    int count=0,capacity=0;memcpy(&count,params.bytes+pointerOffset+8,4);memcpy(&capacity,params.bytes+pointerOffset+12,4);
    return count>=0 && count<=4096 && capacity>=count && capacity<=65536 &&
        (Pointer(params,pointerOffset) || static_cast<unsigned>(capacity)<=inlineCapacity);
}
inline bool Inside(const void* pointer,const void* begin,std::size_t bytes) {
    auto value=reinterpret_cast<std::uintptr_t>(pointer),start=reinterpret_cast<std::uintptr_t>(begin);
    return value>=start && value-start<bytes;
}
struct OwnedScratch {
    Params params;Responses responses;Hits hits;ReleaseSweepAllocation release=nullptr;
    void* world=nullptr;void* component=nullptr;void* owner=nullptr;
    bool ScratchPointer(const void* pointer)const {
        return pointer && (Inside(pointer,this,sizeof(*this)) || pointer==world || pointer==component || pointer==owner);
    }
    bool ValidParams()const {
        auto components=Pointer(params,0x40),actors=Pointer(params,0x60);
        return params.guard==Guard && responses.guard==Guard && InlineArray(params,0x40,8) && InlineArray(params,0x60,4) &&
            !ScratchPointer(components) && !ScratchPointer(actors) && (!components || components!=actors);
    }
    void Release() {
        // These are allocations owned by the exact original constructors and
        // output array. Native cleanup uses Free without per-hit destructors.
        // Snapshot before any allocator callback, clear all fields first and
        // release unique original heap pointers once on every exit.
        std::array<void*,3> pointers{hits.value.data,Pointer(params,0x40),Pointer(params,0x60)};
        hits.value={};void* empty=nullptr;memcpy(params.bytes+0x40,&empty,8);memcpy(params.bytes+0x60,&empty,8);
        for(unsigned i=0;i<pointers.size();++i) {
            auto pointer=pointers[i];if(!pointer || ScratchPointer(pointer))continue;
            bool duplicate=false;for(unsigned j=0;j<i;++j)duplicate|=pointers[j]==pointer;
            if(!duplicate && release)release(pointer);
        }
    }
};
}
inline VolumeSweep RunComponentBodyQuery(ComponentSweepDetail::OwnedScratch& scratch,void* world,void* component,void* owner,
    const ComponentSweepApi& api,ComponentSweepValid valid,void* context,
    const ArenaPoint& first,const ArenaPoint& last,const ComponentRotation& rotation,
    ComponentContactPolicy policy,void* policyContext) {
    auto alive=[&]{return valid(world,component,owner,context);};
    // None tag and unused StatId are legal in this exact shipping constructor.
    // Original constructor owns the ignored-actor allocation and actor IDs.
    if(api.construct(scratch.params.bytes,0,nullptr,false,owner)!=scratch.params.bytes ||
       !scratch.ValidParams() || !alive())return {};
    api.responses(scratch.responses.bytes);if(!scratch.ValidParams() || !alive())return {};
    api.setResponses(scratch.responses.bytes,2);if(!scratch.ValidParams() || !alive())return {};
    // Same original per-component setup used by native MoveComponentImpl,
    // including its own ignored components/actors and response preparation.
    api.initialize(component,scratch.params.bytes,scratch.responses.bytes);
    if(!scratch.ValidParams() || !alive())return {};
    auto start=first,end=last;auto orientation=rotation;
    bool blocked=api.sweep(world,&scratch.hits.value,component,&start,&end,&orientation,scratch.params.bytes);
    const auto& hits=scratch.hits.value;
    if(!alive() || !scratch.ValidParams() || scratch.hits.guard!=ComponentSweepDetail::Guard ||
       hits.count<0 || hits.count>512 || hits.capacity<hits.count || hits.capacity>8192 ||
       (hits.capacity && !hits.data) || (!hits.capacity && hits.data) || scratch.ScratchPointer(hits.data) ||
       (hits.data && (hits.data==ComponentSweepDetail::Pointer(scratch.params,0x40) ||
                      hits.data==ComponentSweepDetail::Pointer(scratch.params,0x60))))return {};
    unsigned blockers=0;float earliest=1;
    for(int i=0;i<hits.count;++i) {
        auto hit=static_cast<const unsigned char*>(hits.data)+static_cast<std::size_t>(i)*0x88;
        float time=0;memcpy(&time,hit+4,4);unsigned flags=hit[0x5d]&3;
        if(!std::isfinite(time) || time<0 || time>1)return {};
        // Nonblocking overlaps/triggers retain native nonblocking semantics.
        // Initial penetrating BLOCKING contacts still obstruct the body.
        if(flags&1){++blockers;earliest=(std::min)(earliest,time);}
    }
    if(blocked!=(blockers!=0))return {};
    unsigned approach=0;
    if(policy)for(int i=0;i<hits.count;++i) {
        auto hit=static_cast<const unsigned char*>(hits.data)+static_cast<std::size_t>(i)*0x88;
        // Trigger contacts already retain their native overlap semantics.
        // Initial penetrating blockers cannot be waived by a contact policy.
        unsigned flags=hit[0x5d]&3;if(!(flags&1) || (flags&2))continue;
        if(!alive())return {};
        std::array<unsigned char,0x88> copy{};memcpy(copy.data(),hit,copy.size());
        auto decision=policy(world,component,owner,copy.data(),policyContext);
        if(!alive() || decision==ComponentContactDecision::Invalid)return {};
        if(decision==ComponentContactDecision::Approach)++approach;
        else if(decision!=ComponentContactDecision::Obstacle)return {};
    }
    // Keep the original blocking result/time. An intentional native contact
    // is permission to approach, not geometric clearance or an infection.
    return {blocked?VolumeSweepResult::Blocked:VolumeSweepResult::Clear,earliest,approach,blockers && approach==blockers};
}
// Original geometry/responses are queried in place. This function creates no
// replacement body/box and never moves the component or changes its filters.
// The native caller must supply actual native poses. Road/body attitude,
// support and game-specific approach permissions remain caller contracts.
inline VolumeSweep QueryComponentBody(void* world,void* component,void* owner,const ComponentSweepApi& api,
    ComponentSweepValid valid,void* context,const ArenaPoint& first,const ArenaPoint& last,const ComponentRotation& rotation,
    ComponentContactPolicy policy=nullptr,void* policyContext=nullptr) {
    if(!world || !component || !owner || !api.construct || !api.responses || !api.setResponses || !api.initialize ||
       !api.sweep || !api.release || !valid || !SweepPoint(first) || !SweepPoint(last) || !SweepQuaternion(rotation) ||
       ArenaDistance(first,last)<=0.01f || ArenaDistance(first,last)>1.e5f || !valid(world,component,owner,context))return {};
    ComponentSweepDetail::OwnedScratch scratch;scratch.release=api.release;scratch.world=world;scratch.component=component;scratch.owner=owner;
    // /EHsc alone does not unwind C++ destructors for a native access fault.
    // A finally boundary releases output/parameter allocations for both SEH
    // and C++ exceptions, then preserves the caller's original exception path.
    __try {return RunComponentBodyQuery(scratch,world,component,owner,api,valid,context,first,last,rotation,policy,policyContext);}
    __finally {scratch.Release();}
}
}
