#pragma once
#include "NativeRoadQuery.h"
#include <cstddef>
#include <cstdint>
#include <limits>
namespace TwinKartRace {
struct QueryRotation {float pitch=0,yaw=0,roll=0;};
static_assert(sizeof(QueryRotation)==12 && sizeof(ArenaPoint)==12 && sizeof(QueryArray)==16,
    "Original x64 vector/rotator/borrowed-array boundaries");
// Verified UKismetSystemLibrary::BoxTraceSingleForObjects boundary. The
// original wrapper constructs/releases its own query/object parameters. The
// arrays, points, rotation and colors are borrowed for this synchronous call.
using BoxObjectTrace=bool(__fastcall*)(void*,const ArenaPoint*,const ArenaPoint*,const ArenaPoint*,
    const QueryRotation*,const QueryArray*,bool,const QueryArray*,int,void*,bool,
    const QueryColor*,const QueryColor*,float);
// The caller's lease must cover the original world AND its physics scene.
// A false native trace without a physics scene must never mean a clear road.
using SweepWorldValid=bool(*)(void*,void*);
enum class VolumeSweepResult {Invalid,Blocked,Clear};
struct VolumeSweep {
    VolumeSweepResult result=VolumeSweepResult::Invalid;float time=0;
    unsigned approachContacts=0;bool onlyApproachContacts=false;
};
inline bool SweepPoint(const ArenaPoint& p) {
    return Finite(p) && std::fabs(p.x)<=1.e7f && std::fabs(p.y)<=1.e7f && std::fabs(p.z)<=1.e7f;
}
inline bool SweepArray(const QueryArray& values,int maximum,bool nonempty) {
    return values.count>=(nonempty?1:0) && values.count<=maximum && values.capacity>=values.count &&
        values.capacity<=maximum && (!values.count || values.data);
}
// This is a query boundary, not a vehicle-size estimator. The caller must
// supply actual original geometry/pose and qualify its use for a route.
// No guessed dimensions, axis-aligned stand-in, floor offset or query-channel
// substitution is supplied here; it is not enabled as a Chaser path gate yet.
inline VolumeSweep QueryVehicleBox(void* world,BoxObjectTrace trace,SweepWorldValid valid,void* context,
    const QueryArray& types,const QueryArray& ignored,const ArenaPoint& first,const ArenaPoint& last,
    const ArenaPoint& halfSize,const QueryRotation& rotation) {
    if(!world || !trace || !valid || !SweepArray(types,32,true) || !SweepArray(ignored,8,false) ||
       !SweepPoint(first) || !SweepPoint(last) || !Finite(halfSize) ||
       halfSize.x<=0 || halfSize.y<=0 || halfSize.z<=0 ||
       halfSize.x>1.e5f || halfSize.y>1.e5f || halfSize.z>1.e5f ||
       !std::isfinite(rotation.pitch) || !std::isfinite(rotation.yaw) || !std::isfinite(rotation.roll) ||
       std::fabs(rotation.pitch)>360000 || std::fabs(rotation.yaw)>360000 || std::fabs(rotation.roll)>360000 ||
       ArenaDistance(first,last)<=0.01f || ArenaDistance(first,last)>1.e5f || !valid(world,context))return {};
    struct alignas(16) HitStorage {unsigned char data[0x88]{};std::uint64_t guard=0;};
    static_assert(offsetof(HitStorage,guard)==0x88 && sizeof(HitStorage)==0x90,
        "Complete original FHitResult and adjacent overwrite guard");
    HitStorage hit;constexpr std::uint64_t marker=0x57c2a08bd34f69e1ull;hit.guard=marker;
    float sentinel=(std::numeric_limits<float>::quiet_NaN)();memcpy(hit.data+4,&sentinel,4);
    // Own copies of value-type arguments: no caller-owned path/box is passed
    // as writable native storage. The original arrays remain read-only views.
    auto start=first,end=last,extent=halfSize;auto orientation=rotation;
    auto objectTypes=types,actors=ignored;QueryColor color{};
    bool blocked=trace(world,&start,&end,&extent,&orientation,&objectTypes,false,&actors,0,
        hit.data,false,&color,&color,0.f);
    if(!valid(world,context) || hit.guard!=marker)return {};
    float time=0;memcpy(&time,hit.data+4,4);unsigned flags=hit.data[0x5d]&3;
    if(!std::isfinite(time) || time<0 || time>1 || (!blocked && (flags || time!=1.f)) || (blocked && !(flags&1)))return {};
    return {blocked?VolumeSweepResult::Blocked:VolumeSweepResult::Clear,time};
}
}
