#pragma once
#include "ChaserArenaDriver.h"
#include <array>
#include <cstdint>
#include <limits>
namespace TwinKartObjective {
enum class ChaserItemControllerKind {Unknown,Generic,Speed};
using ChaserLegacyCondition=bool(__fastcall*)(void*);
using ChaserLegacyGain=void(__fastcall*)(void*);
inline bool ResolveChaserLegacyCondition(void* controller,bool owned,ChaserLegacyCondition original) {
    return original && !owned?original(controller):false;
}
inline void ResolveChaserLegacyGain(void* controller,bool owned,ChaserLegacyGain original) {
    if(original && !owned)original(controller);
}
inline bool ChaserKnownItemControllerAbi(ChaserItemControllerKind kind,std::uintptr_t base,std::uintptr_t tick,
    std::uintptr_t condition,std::uintptr_t timeCondition) {
    return base && (kind==ChaserItemControllerKind::Generic || kind==ChaserItemControllerKind::Speed) &&
        tick==base+0x14e18d0 && timeCondition==base+0x14c0ec0 &&
        condition==base+(kind==ChaserItemControllerKind::Speed?0x14c1520:0x14c1320);
}
// Component presence is not proof of queued native intent. Only the exact
// audited generic controller may yield an empty queue to the local cadence.
// Unknown subclasses, malformed queues and pending gain work retain control.
inline bool ChaserNativeControllerOwnsIntent(bool knownController,int count,int capacity,unsigned gainPending,bool cacheReady=true) {
    if(!knownController || count<0 || capacity<count || capacity>1024 || gainPending>1)return true;
    if(!cacheReady)return false;
    return count!=0 || gainPending!=0;
}
// Serial identities, not pointers to borrowed original arrays. The native
// adapter must read again at submission: inventory/roles can change in a call.
struct ChaserItemObservation {
    std::array<std::uint64_t,5> lease{}; // pawn, state, item, context, inventory
    ChaserRole role=ChaserRole::Unknown;
    bool ready=false,usable=false;
    unsigned inventorySlot=0;
};
inline bool SameChaserItemLease(const ChaserItemObservation& a,const ChaserItemObservation& b) {
    return a.lease==b.lease && a.role==b.role && a.inventorySlot==b.inventorySlot;
}
inline int ChooseChaserItemSlot(bool first,bool second,unsigned preferred) {
    if(preferred>1)return -1;
    return preferred==1?(second?1:first?0:-1):(first?0:second?1:-1);
}
struct ChaserEffectDelivery {
    void* actor=nullptr;bool delivered=false;
    void Observe(void* value){if(actor && actor==value)delivered=true;}
    bool ReserveLocal(bool owned,bool standalone,bool fresh) {
        if(!actor || delivered || !owned || !standalone || !fresh)return false;
        delivered=true;return true;
    }
};
struct ChaserRpcDelivery {
    void* receiver=nullptr;int sequence=-1;unsigned type=0;bool entered=false;
    void Observe(void* value,int itemSequence,unsigned itemType) {
        if(receiver && receiver==value && sequence==itemSequence && type==itemType)entered=true;
    }
    bool ReserveLocal(bool owned,bool standalone,bool fresh) {
        if(!receiver || sequence<0 || entered || !owned || !standalone || !fresh)return false;
        entered=true;return true;
    }
};
inline bool ValidChaserItemSeat(const ChaserItemObservation& seat) {
    if(!seat.ready || seat.inventorySlot>1 || (seat.role!=ChaserRole::Runaway && seat.role!=ChaserRole::Tagger))return false;
    for(auto id:seat.lease)if(!id)return false;
    return true;
}
using ReadChaserItem=bool(*)(int,ChaserItemObservation&,void*);
using SubmitChaserItem=bool(*)(int,const ChaserItemObservation&,void*);
struct ChaserItemPoll {int slot=-1;bool submitted=false;};
// Independent intent cadence only. Does not grant, remove, select an item or
// set aim/casting/cooldown state. Those operations remain native slot input.
class ChaserItemInputs {
    struct Pending {ChaserItemObservation seat{};std::uint64_t due=0;};
    std::array<Pending,16> pending{};
    std::uint64_t last=0,generation=0;
    unsigned cursor=1;bool pumping=false;
    static std::uint64_t After(std::uint64_t now,std::uint64_t delay) {
        return now>(std::numeric_limits<std::uint64_t>::max)()-delay?
            (std::numeric_limits<std::uint64_t>::max)():now+delay;
    }
    static unsigned Stagger(const ChaserItemObservation& seat,int slot) {
        std::uint64_t mix=seat.lease[0]^(seat.lease[2]>>17)^(static_cast<std::uint64_t>(slot)*0x9e3779b9u);
        mix^=mix>>29;return static_cast<unsigned>(mix%401);
    }
public:
    void Reset(){pending={};last=0;cursor=1;++generation;}
    ChaserItemPoll Step(std::uint64_t now,bool active,int count,ReadChaserItem read,SubmitChaserItem submit,void* context) {
        if(pumping)return {};
        if(!active || count<2 || count>16 || !read || !submit){Reset();return {};}
        if(last && now<last)Reset();last=now;
        pumping=true;
        struct PumpGuard {bool& value;~PumpGuard(){value=false;}} guard{pumping};
        const auto epoch=generation;ChaserItemPoll result;
        // One native use request per pump, round-robin across all bot slots.
        // Other slots are still read so expired leases cannot keep intent.
        unsigned first=cursor;
        if(first>=static_cast<unsigned>(count))first=1;
        for(int i=0;i<count-1;++i) {
            int slot=1+static_cast<int>((first-1+static_cast<unsigned>(i))%static_cast<unsigned>(count-1));
            ChaserItemObservation seat;
            bool readOk=read(slot,seat,context);
            if(epoch!=generation)break;
            auto& entry=pending[slot];
            if(!readOk || !ValidChaserItemSeat(seat) || !seat.usable){entry={};continue;}
            if(!SameChaserItemLease(entry.seat,seat) || !entry.seat.usable) {
                entry.seat=seat;entry.due=After(now,150+Stagger(seat,slot));continue;
            }
            if(result.slot>=0 || now<entry.due)continue;
            // Reserve before the callback, which can reenter or reset scope.
            entry.due=After(now,250+Stagger(seat,slot));
            cursor=1+static_cast<unsigned>(slot)%(static_cast<unsigned>(count)-1);
            result.slot=slot;result.submitted=submit(slot,seat,context);
            if(epoch!=generation)break;
        }
        if(epoch==generation)for(int slot=count;slot<16;++slot)pending[slot]={};
        return result;
    }
};
}
