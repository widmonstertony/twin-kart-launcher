#pragma once
#include "FlagArenaDriver.h"
#include <cstring>
#include <cstdint>
#include <cstddef>
namespace TwinKartRace {
struct QueryArray {const void* data=nullptr;int count=0,capacity=0;};
struct QueryColor {float r=0,g=0,b=0,a=1;};
using ObjectTrace=bool(__fastcall*)(void*,const ArenaPoint*,const ArenaPoint*,const QueryArray*,bool,
    const QueryArray*,int,void*,bool,const QueryColor*,const QueryColor*,float);
// The original reflected LineTraceSingleForObjects boundary owns construction
// and cleanup of its collision parameters, including ignored native actors.
// Both input arrays remain borrowed during this synchronous call.
inline bool QueryLineClear(void* world,ObjectTrace trace,const QueryArray& types,const QueryArray& ignored,
    const ArenaPoint& start,const ArenaPoint& end) {
    if(!world || !trace || !Finite(start) || !Finite(end) || !types.data || types.count<1 || types.count>32 ||
       types.capacity<types.count || types.capacity>32 || ignored.count<0 || ignored.count>8 ||
       ignored.capacity<ignored.count || ignored.capacity>8 || (ignored.count && !ignored.data))return false;
    alignas(16) unsigned char hit[0x90]{};QueryColor color{};
    return !trace(world,&start,&end,&types,false,&ignored,0,hit,false,&color,&color,0.f);
}
using RoadTrace=bool(__fastcall*)(void*,void*,const ArenaPoint*,const ArenaPoint*,const void*,const void*);
enum class RoadSurfaceResult {Invalid,Miss,Hit};
struct RoadSurface {ArenaPoint point{};float normalZ=0;bool penetrating=false;ArenaPoint normal{0,0,1};};
// Read the complete original contact even when it is an underside or an
// initial penetration. A layered sampler must distinguish that from a miss
// so it can continue below the obstruction without inventing a road there.
inline RoadSurfaceResult QueryRoadSurface(void* world,RoadTrace trace,const void* parameters,
    const ArenaPoint& start,const ArenaPoint& end,RoadSurface& output) {
    if(!world || !trace || !parameters || !Finite(start) || !Finite(end) || start.z<=end.z ||
       std::fabs(start.x-end.x)>0.01f || std::fabs(start.y-end.y)>0.01f)return RoadSurfaceResult::Invalid;
    struct ObjectMask {unsigned bits;unsigned char kind,padding[3];} mask{0x20000,0,{}};
    struct alignas(16) Storage {unsigned char bytes[0x88]{};std::uint64_t guard=0x28f591a7b6ce430dull;} storage;
    static_assert(sizeof(Storage)==0x90 && offsetof(Storage,guard)==0x88,"Original road contact output extent");
    auto hit=storage.bytes;float time=1;memcpy(hit+4,&time,4);
    trace(world,hit,&start,&end,&mask,parameters);
    if(storage.guard!=0x28f591a7b6ce430dull)return RoadSurfaceResult::Invalid;
    if(!(hit[0x5d]&1))return RoadSurfaceResult::Miss;
    RoadSurface value;memcpy(&value.point,hit+0xc,sizeof(value.point));
    // Exact original FHitResult reflection identifies ImpactNormal at+0x30
    // as a twelve-byte Vector_NetQuantizeNormal in native memory. Keep XY for
    // bank/slope body mapping; no serialized/network bytes are reinterpreted.
    memcpy(&value.normal,hit+0x30,sizeof(value.normal));value.normalZ=value.normal.z;
    // Exact PC FHitResult metadata: bStartPenetrating setter at0x440b990
    // ORs byte+0x5d with2; its reflected struct size is0x88.
    value.penetrating=(hit[0x5d]&2)!=0;
    float norm=value.normal.x*value.normal.x+value.normal.y*value.normal.y+value.normal.z*value.normal.z;
    if(!Finite(value.point) || !Finite(value.normal) || !std::isfinite(norm) || std::fabs(value.normalZ)>1.01f ||
       (!value.penetrating && std::fabs(norm-1.f)>0.03f) ||
       std::fabs(value.point.x-start.x)>2 || std::fabs(value.point.y-start.y)>2 ||
       value.point.z>start.z+2 || value.point.z<end.z-2)return RoadSurfaceResult::Invalid;
    output=value;return RoadSurfaceResult::Hit;
}
inline bool QueryRoad(void* world,RoadTrace trace,const void* parameters,const ArenaPoint& start,const ArenaPoint& end,ArenaPoint& point) {
    if(!world || !trace || !parameters || !Finite(start) || !Finite(end))return false;
    struct ObjectMask {unsigned bits;unsigned char kind,padding[3];} mask{0x20000,0,{}};
    // Exact native FHitResult includes the final eight-byte name field after
    // the first 128 bytes. Reserve its complete 0x88-byte ABI, rounded to16.
    alignas(16) unsigned char hit[0x90]{};float time=1;memcpy(hit+4,&time,4);
    trace(world,hit,&start,&end,&mask,parameters);
    ArenaPoint contact{};memcpy(&contact,hit+0xc,sizeof(contact));float normalZ=0;memcpy(&normalZ,hit+0x38,4);
    if(!(hit[0x5d]&1) || !Finite(contact) || !std::isfinite(normalZ) || normalZ<=0.70f)return false;
    point=contact;return true;
}
}
