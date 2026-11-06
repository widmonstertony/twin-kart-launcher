#pragma once
#include "NativeRacePreparation.h"
#include <cstddef>
#include <cstdint>
namespace TwinKartObjective {
struct AuthoredLobbyDefinition {const wchar_t* gameClass=nullptr;const wchar_t* travel=nullptr;int teamSize=0,teamCount=0;};
inline bool PrototypeLobbyDefinition(unsigned mode,AuthoredLobbyDefinition& value) {
    if(mode==1){value={L"/Script/KartDrift.SpeedSingleGameMode",L"SpeedSingle",1,0};return true;}
    if(mode==100){value={L"/Script/KartDrift.TobyRushGameMode",L"TobyRush",4,1};return true;}
    if(mode==109){value={L"/Script/KartDrift.HideNSeekGameMode",L"HideNSeek",16,1};return true;}
    return false;
}
// Native enums108/109 describe a sixteen-seat single group. Only the two
// original MENU selection calls need a solo-selection bucket for our local
// one-human sessions. Never change the native gameplay team/capacity table.
using ReadLobbyTeamSize=int(__fastcall*)(const TwinKartRace::PreparationMode*);
inline int PrototypeLobbyTeamSize(const TwinKartRace::PreparationMode* mode,bool owner,bool chaser,bool hide,ReadLobbyTeamSize native) {
    if(!native)return 0;
    int result=native(mode);
    if(!mode || !owner || result!=16 || !((chaser && mode->mode==108) || (hide && mode->mode==109)))return result;
    const auto& text=mode->profile;
    if(text.count<0 || text.capacity<text.count || text.capacity>97 || text.count>1 || (text.count==1 && (!text.data || text.data[0])))return result;
    return 1;
}
// Exact original FStaticConstructObjectParameters boundary, independently
// corroborated by GameplayStatics.SpawnObject. Native initialization supplies
// defaults; callers only author their OWN transient metadata object.
struct alignas(8) AuthoredObjectParameters {
    void* type=nullptr;void* outer=nullptr;std::uint64_t name=0;
    unsigned flags=0,internalFlags=0;unsigned char copyTransients=0,assumeTemplate=0,pad[6]{};
    void* objectTemplate=nullptr;void* instanceGraph=nullptr;void* externalPackage=nullptr;
};
static_assert(sizeof(AuthoredObjectParameters)==64 && offsetof(AuthoredObjectParameters,flags)==0x18 &&
    offsetof(AuthoredObjectParameters,objectTemplate)==0x28,"Pinned native construction ABI");
using InitializeAuthoredObject=void*(__fastcall*)(AuthoredObjectParameters*,void*);
using ConstructAuthoredObject=void*(__fastcall*)(AuthoredObjectParameters*);
inline void* CreateAuthoredLobbyObject(void* type,void* outer,InitializeAuthoredObject initialize,ConstructAuthoredObject construct) {
    if(!type || !outer || !initialize || !construct)return nullptr;
    AuthoredObjectParameters parameters;
    if(initialize(&parameters,type)!=&parameters || parameters.type!=type || parameters.name || parameters.internalFlags ||
       parameters.copyTransients || parameters.assumeTemplate || parameters.objectTemplate || parameters.instanceGraph || parameters.externalPackage)return nullptr;
    parameters.outer=outer;
    // RF_Transient|RF_MarkAsRootSet, processed by the ORIGINAL constructor.
    // This root belongs only to our new presentation descriptor, not a vendor
    // CDO or shared resource. Its lifetime is the owned game process.
    parameters.flags=0xc0;
    return construct(&parameters);
}
}
