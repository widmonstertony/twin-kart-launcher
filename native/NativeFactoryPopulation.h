#pragma once
#include "DriftObjectIdentity.h"
#include "RaceControl.h"
#include <array>
#include <cstdint>
namespace TwinKartFactory {
// Factory's native population is not an ordinary 1+7 race. Keep its native
// four racer slots distinct from the thirty-six Android slots. A cooperative
// AI must remain a racer; an Android model is never a substitute for it.
constexpr int RacerCount=4,AndroidCount=36,PopulationCount=40,Laps=5;
constexpr std::uint64_t AllSlots=(std::uint64_t{1}<<PopulationCount)-1;
constexpr std::uint64_t RacerSlots=(std::uint64_t{1}<<RacerCount)-1;
constexpr std::uint64_t AndroidSlots=AllSlots^RacerSlots;
inline constexpr const wchar_t* RacerCharacters[]={L"Character:100000001",L"Character:100000002",L"Character:100000007",L"Character:100000010"};
inline constexpr const wchar_t* RacerKarts[]={L"Kart:401000177",L"Kart:401000079",L"Kart:401000163",L"Kart:401000168"};
inline constexpr const wchar_t* AndroidBodyObjectPath=L"/Game/ART/Kart/Kart_Speed/AIKart/TobyRush/Kartbody_TobyRush.Kartbody_TobyRush";
enum class Role {Invalid,LocalRacer,CooperativeRacer,Android};
inline Role SlotRole(int slot,int racers,int androids) {
    if(racers!=RacerCount || androids!=AndroidCount || slot<0 || slot>=PopulationCount)return Role::Invalid;
    return slot==0?Role::LocalRacer:slot<RacerCount?Role::CooperativeRacer:Role::Android;
}
// Missing-data capture only. This separate request cannot accept custom engine
// options, arbitrary population sizes, profiles or substitute game classes.
// The ordinary TKRACE contract continues to reject mode100 and 36/39 AI.
inline bool ParseCapture(const char* text,TwinKartRace::Request* output) {
    if(!text || !output || strnlen_s(text,1025)>1024 || strncmp(text,"TKFACTORY_CAPTURE1 ",19))return false;
    TwinKartRace::Request candidate;char extra=0;
    if(sscanf_s(text,"TKFACTORY_CAPTURE1 %32s %96s %c",candidate.identity,33u,candidate.track,97u,&extra,1u)!=2 ||
       !TwinKartRace::Valid(candidate) || strcmp(candidate.track,"Factory_I01"))return false;
    candidate.mode=100;candidate.ai=AndroidCount;candidate.laps=Laps;*output=candidate;return true;
}
// Player-requested Factory starts share the audited population/options, while
// retaining a distinct contract from the ordinary eight-seat circuit host.
inline bool ParseStart(const char* text,TwinKartRace::Request* output) {
    if(!text || !output || strnlen_s(text,257)>256 || strncmp(text,"TKFACTORY1 ",11))return false;
    TwinKartRace::Request candidate;char extra=0;
    if(sscanf_s(text,"TKFACTORY1 %32s %96s %c",candidate.identity,33u,candidate.track,97u,&extra,1u)!=2 ||
       strcmp(candidate.track,"Factory_I01") || !TwinKartRace::Valid(candidate))return false;
    candidate.mode=100;candidate.ai=AndroidCount;candidate.laps=Laps;*output=candidate;return true;
}
inline bool CaptureOptions(const TwinKartRace::Request& request,wchar_t* output,size_t capacity) {
    if(request.mode!=100 || request.ai!=AndroidCount || request.laps!=Laps || request.profile[0] || request.custom[0] ||
       strcmp(request.track,"Factory_I01"))return false;
    auto syntax=request;syntax.mode=1;syntax.ai=7;
    if(!TwinKartRace::Valid(syntax) || !output)return false;
    const wchar_t options[]=L"game=/Script/KartDrift.TobyRushGameMode?gamemode=TobyRush?racemode=100?team=1?teamsize=4?players=4?observers=0?ais=36?lap=5";
    if(capacity<ARRAYSIZE(options))return false;
    return wcscpy_s(output,capacity,options)==0;
}
// Read-only native census. Serial identity and exact role class are supplied
// by the owned-world adapter, not guessed from a kart/character appearance.
// Recording a pawn does not certify its driving, elimination or final result.
class Population {
    std::array<TwinKartCamera::ObjectIdentity,PopulationCount> pawns{};
    std::uint64_t seen=0,robotComponents=0;
public:
    void Reset(){pawns={};seen=robotComponents=0;}
    bool Observe(int slot,Role nativeRole,const TwinKartCamera::ObjectIdentity& identity,
                 TwinKartCamera::ObjectSlot** chunks,int count,bool nativeRobotComponents) {
        auto expected=SlotRole(slot,RacerCount,AndroidCount);
        if(expected==Role::Invalid || nativeRole!=expected || !TwinKartCamera::Live(identity,chunks,count))return false;
        const auto& old=pawns[slot];
        // Do not let an address reuse, duplicate slot or one pawn appearing in
        // two slots silently turn an incomplete population into forty members.
        if(old.object && (old.object!=identity.object || old.index!=identity.index || old.serial!=identity.serial))return false;
        for(int i=0;i<PopulationCount;++i)if(i!=slot && pawns[i].object==identity.object)return false;
        pawns[slot]=identity;seen|=std::uint64_t{1}<<slot;
        if(expected==Role::Android) {
            if(nativeRobotComponents)robotComponents|=std::uint64_t{1}<<slot;
            else robotComponents&=~(std::uint64_t{1}<<slot);
        }
        return true;
    }
    std::uint64_t Seen() const{return seen;}
    std::uint64_t RobotComponents() const{return robotComponents;}
    bool NativePopulationPresent(TwinKartCamera::ObjectSlot** chunks,int count) const {
        if(seen!=AllSlots || robotComponents!=AndroidSlots)return false;
        for(const auto& pawn:pawns)if(!TwinKartCamera::Live(pawn,chunks,count))return false;
        return true;
    }
};
}
