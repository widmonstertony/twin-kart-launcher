#pragma once
#include <cstddef>
#include <cstring>
#include <cwchar>
#include "NativeRacePreparation.h"
#include "NativeEquipmentPreset.h"

namespace TwinKartObjective {
// Original Chaser class defaults declare a sixteen-seat lobby group, with
// roles subsequently assigned by the native tagger/runaway rules. This is not
// an ordinary sixteen-member race team or evidence of sixteen working bots.
inline constexpr int ChaserCapacity=16;
// Private, opt-in branch fixture. Normal manifests do not request it. The
// original selector still chooses participants, infects and decides results.
inline int ChaserCaptureTaggers(const wchar_t* text) {
    return text && text[0]>=L'1' && text[0]<=L'7' && !text[1]?text[0]-L'0':0;
}
inline bool ConfigureAuthoredChaserCapture(void* object,void* type,int taggers) {
    if(!object || !type || taggers<1 || taggers>7)return false;
    auto data=static_cast<unsigned char*>(object);
    unsigned flags=*reinterpret_cast<unsigned*>(data+8);
    // Only our newly constructed transient instance, never the native
    // class default, an archetype, another class or a destroying object.
    // Native construction consumes RF_MarkAsRootSet (0x80); it is not a
    // persistent UObject flag. The caller owns the freshly created identity.
    if(!(flags&0x40) || (flags&0x18030) || *reinterpret_cast<void**>(data+0x10)!=type ||
       *reinterpret_cast<void**>(static_cast<unsigned char*>(type)+0x118)==object || data[0x44])return false;
    *reinterpret_cast<int*>(data+0x40)=taggers;return true;
}
struct ChaserModeDefinition {
    void* asset=nullptr;
    int kind=-1,teamSize=0,teamCount=0;
    wchar_t gameClass[192]{},travelName[64]{};
};
inline bool ValidChaserDefinition(const ChaserModeDefinition& value) {
    return value.asset && value.kind==2 && value.teamSize==ChaserCapacity && value.teamCount==1 &&
        wcsnlen_s(value.gameClass,192)<192 && !wcscmp(value.gameClass,L"/Script/KartDrift.ChaserGameMode") &&
        wcsnlen_s(value.travelName,64)<64 && !wcscmp(value.travelName,L"Chaser");
}
using ReadClassName=bool(*)(std::uint64_t,wchar_t*,std::size_t);
// The caller first establishes live native class/object identities. Copy only
// the original mode descriptor fields; no cooked asset or class default is
// changed. A failed read must not publish a partly constructed definition.
inline bool ReadOriginalChaserDefault(void* type,void* asset,ReadClassName name,ChaserModeDefinition& output) {
    if(!type || !asset || !name)return false;
    __try {
        const auto object=static_cast<const unsigned char*>(asset);
        unsigned flags=*reinterpret_cast<const unsigned*>(object+8);
        if(!(flags&0x10) || (flags&0x18000) || *reinterpret_cast<void* const*>(object+0x10)!=type ||
           *reinterpret_cast<void* const*>(static_cast<const unsigned char*>(type)+0x118)!=asset)return false;
        auto travel=*reinterpret_cast<const TwinKartEquipment::String*>(object+0x238);
        if(!TwinKartEquipment::Valid(travel,64) || !TwinKartEquipment::Nonempty(travel))return false;
        ChaserModeDefinition definition;definition.asset=asset;
        definition.kind=*reinterpret_cast<const int*>(object+0x230);
        definition.teamSize=*reinterpret_cast<const int*>(object+0x248);
        definition.teamCount=*reinterpret_cast<const int*>(object+0x24c);
        memcpy(definition.travelName,travel.data,static_cast<std::size_t>(travel.count)*sizeof(wchar_t));
        if(!name(*reinterpret_cast<const std::uint64_t*>(object+0x40),definition.gameClass,192) ||
           !ValidChaserDefinition(definition))return false;
        output=definition;return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
inline bool DefaultChaserRequest(const TwinKartRace::PreparationMode* mode) {
    if(!mode || mode->mode!=108)return false;
    const auto& profile=mode->profile;
    TwinKartEquipment::String text{profile.data,profile.count,profile.capacity};
    return TwinKartEquipment::Valid(text,97) && !TwinKartEquipment::Nonempty(text);
}
using FindModeDefinition=void*(__fastcall*)(const TwinKartRace::PreparationMode*);
using RetainedChaserDefinition=void*(*)(void*);
// Call the native lookup once. Never replace an existing native definition,
// an explicit profile, ordinary mode or foreign-thread lookup with defaults.
inline void* ResolveRetainedChaser(const TwinKartRace::PreparationMode* mode,bool enabled,bool ownerThread,
                                  FindModeDefinition original,RetainedChaserDefinition retained,void* context) {
    if(!original)return nullptr;
    void* result=original(mode);
    if(result || !enabled || !ownerThread || !retained)return result;
    __try {
        if(!DefaultChaserRequest(mode))return result;
        return retained(context);
    } __except(EXCEPTION_EXECUTE_HANDLER) {return result;}
}
// Independently confirmed PC UClass storage and lazy-create slot. The caller
// validates the exact native class, original virtual entry and owning thread.
// Read the engine's stored result, not an arbitrary callback return value.
using CreateClassDefault=void*(__fastcall*)(void*);
inline void* OriginalClassDefault(void* type,CreateClassDefault create) {
    if(!type)return nullptr;
    auto& stored=*reinterpret_cast<void**>(static_cast<unsigned char*>(type)+0x118);
    if(!stored && create)create(type);
    return stored;
}
// Native SelectTaggerAndRunaway admits connected pawns through the original
// GetConnectionStatus (1=connected). It does not filter bIsAiPlayer. Preserve
// native random selection and role-component construction; only newly created
// standalone bot seats may need their original SetConnect notification.
struct ChaserConnectionSeat {
    void* pawn=nullptr;
    void* state=nullptr;
    int slot=-1;
    bool ai=false;
    unsigned connection=0;
};
// PawnState's original AIController initialization emits status 3; status 2
// is the native disconnected state. Accept AI initialization only during the
// separately owned pregame roster preparation, never as gameplay readiness.
inline bool ChaserCoreConnection(unsigned status) {return status==0 || status==1 || status==3;}
inline bool ChaserSpawnSlot(int players,int ais,int slot,bool& ai) {
    if(players!=1 || ais<1 || ais>=ChaserCapacity || slot<0 || slot>=players+ais)return false;
    ai=slot!=0;return true;
}
inline bool ChaserConnectionPlan(const ChaserConnectionSeat* seats,int count,unsigned& pending) {
    if(!seats || count<2 || count>ChaserCapacity)return false;
    unsigned slots=0,plan=0;
    for(int i=0;i<count;++i) {
        const auto& seat=seats[i];
        if(!seat.pawn || !seat.state || seat.slot<0 || seat.slot>=count || seat.connection>3 ||
           seat.ai!=(seat.slot!=0) || (!seat.ai && seat.connection!=1) || (slots&(1u<<seat.slot)))return false;
        for(int j=0;j<i;++j)if(seats[j].pawn==seat.pawn || seats[j].state==seat.state)return false;
        slots|=1u<<seat.slot;
        // Disconnected native seats remain disconnected, including bots.
        if(seat.ai && (seat.connection==0 || seat.connection==3))plan|=1u<<i;
    }
    if(slots!=((1u<<count)-1))return false;
    pending=plan;return true;
}
using ConnectChaserSeat=bool(*)(const ChaserConnectionSeat&,void*);
inline bool PrepareChaserConnections(const ChaserConnectionSeat* seats,int count,ConnectChaserSeat connect,
                                     void* context,unsigned& completed) {
    completed=0;unsigned pending=0;
    // Validate the complete roster before emitting any native notification.
    if(!ChaserConnectionPlan(seats,count,pending) || (pending && !connect))return false;
    for(int i=0;i<count;++i)if(pending&(1u<<i)) {
        // The native adapter rechecks serial/world/state here. Its notification
        // can invoke engine callbacks; never retain this roster after the call.
        if(!connect(seats[i],context))return false;
        ++completed;
    }
    return true;
}
using ChaserNativeNetMode=int(__fastcall*)(void*);
using ChaserOwnedWorld=bool(*)(void*,void*);
using ChaserNativeCanTransit=bool(__fastcall*)(void*);
using ChaserLoadingReady=bool(*)(void*,bool&,void*);
// Qualifier sets owned only after identifying the exact native Loading rule,
// its current phase and owned world. An owned readiness failure holds loading;
// other rules keep the unchanged native result. No timer/result is rewritten.
inline bool ResolveChaserLoadingReady(void* rule,bool enabled,bool ownerThread,ChaserNativeCanTransit original,
                                     ChaserLoadingReady qualify,void* context) {
    if(!original)return false;
    bool native=original(rule);
    if(!native || !enabled || !ownerThread || !rule || !qualify)return native;
    bool owned=false;
    __try {bool ready=qualify(rule,owned,context);return owned?ready:native;}
    __except(EXCEPTION_EXECUTE_HANDLER){return owned?false:native;}
}
// These two adapters belong to distinct, exact Tagger BeginPlay call sites.
// Standalone also needs its original movement-collision event subscription,
// which the first native query restricts to network clients. No global world
// net mode, role, RPC routing or score is changed by this single-query result.
inline int ResolveChaserRoleEvents(void* world,bool enabled,bool ownerThread,ChaserNativeNetMode original,
                                  ChaserOwnedWorld owns,void* context) {
    if(!original)return -1;
    int result=original(world);
    if(result!=0 || !enabled || !ownerThread || !world || !owns)return result;
    __try {return owns(world,context)?3:result;}
    __except(EXCEPTION_EXECUTE_HANDLER){return result;}
}
// The separate native authority query still activates original rule/item
// bindings. Other client/server queries and every other world remain native.
inline int ResolveChaserRoleAuthority(void* world,bool enabled,bool ownerThread,ChaserNativeNetMode original,
                                      ChaserOwnedWorld owns,void* context) {
    if(!original)return -1;
    int result=original(world);
    if(result!=0 || !enabled || !ownerThread || !world || !owns)return result;
    __try {return owns(world,context)?1:result;}
    __except(EXCEPTION_EXECUTE_HANDLER){return result;}
}
}
