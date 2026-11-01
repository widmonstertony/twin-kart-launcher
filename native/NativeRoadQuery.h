#pragma once
#include "FlagArenaDriver.h"
#include <cstring>
namespace TwinKartRace {
using RoadTrace=bool(__fastcall*)(void*,void*,const ArenaPoint*,const ArenaPoint*,const void*,const void*);
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
