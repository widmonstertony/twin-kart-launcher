#pragma once
#include "NativeComponentSweep.h"
namespace TwinKartRace {
using AllKartTiresContact=bool(__fastcall*)(void*,bool);
using KartWheelLocation=bool(__fastcall*)(void*,ArenaPoint*,unsigned);
using KartWheelComponent=bool(__fastcall*)(void*,void**,unsigned);
using KartContactsValid=bool(*)(void*,void*);
using KartSupportComponentValid=bool(*)(void*,void*);
struct KartContactsApi {
    AllKartTiresContact grounded=nullptr;KartWheelLocation location=nullptr;KartWheelComponent component=nullptr;
};
struct KartWheelContact {ArenaPoint point{};void* component=nullptr;};
struct KartContactSnapshot {KartWheelContact wheels[4]{};};
namespace KartContactDetail {
struct alignas(16) PointOutput {ArenaPoint point{};unsigned guard=0x9d4ac761u;};
struct ComponentOutput {void* component=nullptr;std::uint64_t guard=0xc3198a74e26db5f0ull;};
static_assert(sizeof(PointOutput)==16 && offsetof(PointOutput,guard)==12 && offsetof(ComponentOutput,guard)==8,
    "Original wheel-contact output bounds");
inline bool ReadPass(void* movementInterface,const KartContactsApi& api,KartContactsValid valid,
    KartSupportComponentValid support,void* context,KartContactSnapshot& output) {
    if(!valid(movementInterface,context) || !api.grounded(movementInterface,true) || !valid(movementInterface,context))return false;
    for(unsigned wheel=0;wheel<4;++wheel) {
        PointOutput point;float nan=(std::numeric_limits<float>::quiet_NaN)();point.point={nan,nan,nan};
        ComponentOutput component;
        if(!valid(movementInterface,context) || !api.location(movementInterface,&point.point,wheel) ||
           point.guard!=0x9d4ac761u || !SweepPoint(point.point) || !valid(movementInterface,context))return false;
        if(!api.component(movementInterface,&component.component,wheel) || component.guard!=0xc3198a74e26db5f0ull ||
           !component.component || !valid(movementInterface,context) || !support(component.component,context) ||
           !valid(movementInterface,context))return false;
        output.wheels[wheel]={point.point,component.component};
    }
    return api.grounded(movementInterface,true) && valid(movementInterface,context);
}
}
// The caller must first verify original interface targets and four-element
// contact/point/weak-component array headers, and retain their storage lease.
// Values/components remain borrowed. A double read rejects an observed cache
// transition; it is not a claim that the UE cache has a documented lock.
inline bool ReadKartContacts(void* movementInterface,const KartContactsApi& api,KartContactsValid valid,
    KartSupportComponentValid support,void* context,KartContactSnapshot& output) {
    if(!movementInterface || !api.grounded || !api.location || !api.component || !valid || !support)return false;
    KartContactSnapshot first{},second{};
    if(!KartContactDetail::ReadPass(movementInterface,api,valid,support,context,first) ||
       !KartContactDetail::ReadPass(movementInterface,api,valid,support,context,second))return false;
    for(unsigned wheel=0;wheel<4;++wheel) {
        if(first.wheels[wheel].component!=second.wheels[wheel].component ||
           memcmp(&first.wheels[wheel].point,&second.wheels[wheel].point,sizeof(ArenaPoint)) ||
           !support(second.wheels[wheel].component,context) || !valid(movementInterface,context))return false;
    }
    output=second;return true;
}
}
