#pragma once
#include "NativeRoadQuery.h"
#include <array>
#include <cstdint>
namespace TwinKartRace {
enum class RoadEdgeObservation {Invalid,Blocked,Clear,Approach};
using ReadRoadEdge=RoadEdgeObservation(*)(const ArenaPoint&,const ArenaPoint&,void*);
// Per-pawn, directed graph-edge observations. The original callback still
// decides clearance; this cache never turns an unqueried edge into clear.
// Exhaustion defers unknown edges, so successive bounded planner turns can
// discover a route without issuing an entire graph of native queries at once.
class RoadEdgeQueries {
    struct Entry {ArenaPoint a{},b{};std::uint64_t stamp=0,scope=0;bool clear=false;};
    std::array<Entry,2048> entries{};
    std::uint64_t generation=0,now=0,epoch=1;
    unsigned remaining=0,used=0;
    unsigned blockedLifetime=250;
    bool querying=false;
    std::array<unsigned char,256> qualification{};bool qualified=false;
    static unsigned Hash(const ArenaPoint& a,const ArenaPoint& b,std::uint64_t scope) {
        unsigned hash=2166136261u;
        for(float value:{a.x,a.y,a.z,b.x,b.y,b.z}) {
            // Treat -0 and +0 as the same coordinate. Finite() is required
            // before hashing, so no NaN canonicalization is needed.
            unsigned bits=0;if(value!=0)memcpy(&bits,&value,4);
            hash=(hash^bits)*16777619u;
        }
        hash=(hash^static_cast<unsigned>(scope))*16777619u;
        hash=(hash^static_cast<unsigned>(scope>>32))*16777619u;
        // Native grid coordinates often have identical low float bits.
        // Avalanche high bits before the power-of-two table index.
        hash^=hash>>16;hash*=0x7feb352du;hash^=hash>>15;hash*=0x846ca68bu;return hash^(hash>>16);
    }
    static bool Same(const ArenaPoint& a,const ArenaPoint& b){return a.x==b.x && a.y==b.y && a.z==b.z;}
    struct BoolReader {bool(*read)(const ArenaPoint&,const ArenaPoint&,void*);void* context;};
    static RoadEdgeObservation ReadBoolean(const ArenaPoint& a,const ArenaPoint& b,void* raw) {
        auto& call=*static_cast<BoolReader*>(raw);
        return call.read(a,b,call.context)?RoadEdgeObservation::Clear:RoadEdgeObservation::Blocked;
    }
public:
    void Invalidate(){for(auto& entry:entries)entry.stamp=0;++epoch;}
    void Reset(){Invalidate();generation=now=0;remaining=used=0;blockedLifetime=250;qualification={};qualified=false;}
    // Exact caller-serialized body/pose/geometry identity, with no hash-only
    // equivalence and no padding bytes. Changing it invalidates observations
    // without replenishing this pump's native query allowance.
    bool Qualify(const std::array<unsigned char,256>& value) {
        if(querying || !generation || !now)return false;
        if(!qualified || qualification!=value){Invalidate();qualification=value;qualified=true;}
        return true;
    }
    bool Begin(std::uint64_t owner,std::uint64_t time,unsigned budget,unsigned blockedTtl=250) {
        if(querying)return false;
        if(!owner || !time || budget>256 || !blockedTtl || blockedTtl>10000){Reset();return false;}
        if(generation!=owner || time<now || blockedLifetime!=blockedTtl)Invalidate();
        blockedLifetime=blockedTtl;
        // The same pump cannot replenish its native query allowance.
        if(generation==owner && now==time)return true;
        generation=owner;now=time;remaining=budget;used=0;return true;
    }
    unsigned QueriesUsed()const{return used;}
    RoadEdgeObservation Observe(const ArenaPoint& a,const ArenaPoint& b,ReadRoadEdge read,void* context,std::uint64_t scope=0) {
        using Result=RoadEdgeObservation;
        if(querying || !generation || !now || !read || !Finite(a) || !Finite(b))return Result::Invalid;
        unsigned start=Hash(a,b,scope)%static_cast<unsigned>(entries.size()),replace=start;
        for(unsigned probe=0;probe<8;++probe) {
            unsigned at=(start+probe)%static_cast<unsigned>(entries.size());const auto& entry=entries[at];
            if(!entry.stamp){replace=at;break;}
            if(entry.scope==scope && Same(entry.a,a) && Same(entry.b,b)) {
                if(now>=entry.stamp && now-entry.stamp<=(entry.clear?10000u:blockedLifetime))return entry.clear?Result::Clear:Result::Blocked;
                replace=at;break;
            }
            if(entries[replace].stamp>entry.stamp)replace=at;
        }
        if(!remaining)return Result::Invalid;
        --remaining;++used;Result result=Result::Invalid;auto current=epoch;querying=true;
        __try {result=read(a,b,context);}
        __finally {querying=false;}
        if(current!=epoch || !generation || !now)return Result::Invalid;
        // Approach retains the native blocking contact. It is permission for
        // this fresh, scoped request, never cached geometric clearance. An
        // expired/malformed observation is also not stored as an obstacle.
        if(result==Result::Approach)return result;
        if(result!=Result::Clear && result!=Result::Blocked)return Result::Invalid;
        entries[replace]={a,b,now,scope,result==Result::Clear};return result;
    }
    bool Check(const ArenaPoint& a,const ArenaPoint& b,
        bool(*read)(const ArenaPoint&,const ArenaPoint&,void*),void* context) {
        if(!read)return false;
        BoolReader call{read,context};return Observe(a,b,ReadBoolean,&call)==RoadEdgeObservation::Clear;
    }
};
}
