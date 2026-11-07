#pragma once
#include "OwnedCodePatch.h"
#include "DriftObjectIdentity.h"
#include "LocalRaceWorld.h"
#include "NativeRacePhase.h"
#include "NativeEquipmentPreset.h"
#include "NativeAiRoster.h"
#include "NativeAiAssets.h"
#include "NativeRaceModes.h"
#include "NativeCubeWorld.h"
#include "NativeReturnRoute.h"
#include "NativeFactoryPopulation.h"
#include "NativeChaserMode.h"
#include "FlagArenaDriver.h"
#include "FlagGroundNavigation.h"
#include "NativeRoadQuery.h"

namespace TwinKartLocalRace {
using Spawn=bool(__fastcall*)(void*,void*,int,void*,void**,bool*);
using AddUnknown=void(__fastcall*)(void*,void*,int,bool);
using AutoDrive=void(__fastcall*)(void*,bool);
using ResultInit=void(__fastcall*)(void*,const void*,const void*,const void*,void**,void**);
using Construct=void*(__fastcall*)(void*);
using BeginRule=void(__fastcall*)(void*);
using Event=void(*)(const char*,int,int);
inline BYTE* image=nullptr;
inline DWORD owner=0;
inline bool armed=false,installed=false;
inline int armedMode=0;
inline Event event=nullptr;
inline Spawn spawn=nullptr;
using ChaserSpawnClassifier=void(*)(void*,void*,int,bool*);
inline ChaserSpawnClassifier chaserSpawnClassifier=nullptr;
using ChaserStartSelector=bool(*)(void*,void*,int,void*&);
inline ChaserStartSelector chaserStartSelector=nullptr;
inline bool chaserItemContextEnabled=false;
inline AddUnknown addUnknown=nullptr;
inline AutoDrive autoDrive=nullptr;
inline ResultInit resultInit=nullptr;
inline Construct construct=nullptr;
inline BeginRule managerBegin=nullptr,activationBegin=nullptr,splineBegin=nullptr;
inline BYTE* registrationActor=nullptr;
inline LocalRaceWorld raceWorld;
// Original objective worlds use a separate serial lease from circuit races.
inline LocalRaceWorld objectiveWorld;
inline TwinKartNative::OwnedCodePatch spawnPatch,statePatch,driverPatch,resultPatch;
inline TwinKartNative::OwnedCodePatch managerPatch,activationPatch,splinePatch;
inline TwinKartNative::OwnedCodePatch constructionPatches[5];
using DescriptorEqual=bool(__fastcall*)(const void*,const void*);
inline DescriptorEqual kartEqual=nullptr,characterEqual=nullptr;
inline TwinKartNative::OwnedCodePatch kartEquipmentPatch,characterEquipmentPatch;
inline BYTE* equipmentPawn=nullptr;
inline TwinKartEquipment::Preset equipmentPreset;
inline bool equipmentReady=false;
// Temporary, game-thread-only appearance preparation for the bound original
// objective world. Never broadens normal race ownership or survives a call.
inline bool objectiveEquipmentScope=false;
inline TwinKartRace::AiRoster aiRoster;
// Event skill roles are independent of the visible, per-seat random outfit.
// Keep the prescribed team skills without replacing four racers' appearances
// with the same character (or changing the driver's selected character).
inline TwinKartRace::AiRoster eventSkillRoster;
inline bool aiRosterEnabled=false;
// Separate opt-in native Factory integration. It observes only a real native
// TobyRushGameMode with its original 4/36 population; it does not expose an
// unfinished menu entry or widen the ordinary eight-seat request contract.
inline bool factoryCompatEnabled=false;
inline int factorySpawnSlot=-1;
inline LocalRaceWorld factoryWorld;
inline TwinKartFactory::Population factoryPopulation;
inline TwinKartRace::AiRoster factoryRacerRoster;
inline bool factoryRosterReady=false;
inline wchar_t factoryAndroidKart[97]{};
inline bool factoryStartRequested=false;
inline unsigned factoryCooperativeMask=0;
inline unsigned long long factoryLastPhase=~0ull;
inline TwinKartCamera::ObjectIdentity factoryHumanWinWorld{};
using FactoryWinReport=void(__fastcall*)(void*,void*);
inline FactoryWinReport factoryWinReport=nullptr;
inline TwinKartNative::OwnedCodePatch factoryWinReportPatch;
inline void __fastcall OnFactoryWinReport(void*,void*);
inline TwinKartNative::OwnedCodePatch factoryConstructionPatches[3];
inline BeginRule factoryManagerBegin=nullptr;
inline TwinKartNative::OwnedCodePatch factoryManagerPatch;
inline void __fastcall OnFactoryManagerBegin(void* raw);
using CustomPodium=void(__fastcall*)(void*,void*,const void*,void*);
inline CustomPodium factoryCustomPodium=nullptr;
inline TwinKartNative::OwnedCodePatch factoryPodiumPatch;
inline void __fastcall OnFactoryPodium(void*,void*,const void*,void*);
using IntroSequence=void(__fastcall*)(void*,void*,void*);
inline IntroSequence factoryIntroSequence=nullptr;
inline BeginRule factoryIntroBegin=nullptr;
inline TwinKartNative::OwnedCodePatch factoryIntroPatch,factoryIntroSequencePatch;
inline BYTE* factoryIntroScope=nullptr;
inline bool factoryIntroPlaying=false;
inline void __fastcall OnFactoryIntroBegin(void*);
inline void __fastcall OnFactoryIntroSequence(void*,void*,void*);
using AiItemUse=void(__fastcall*)(void*);
inline AiItemUse factoryItemUse=nullptr;
inline TwinKartNative::OwnedCodePatch factoryItemUsePatch;
inline unsigned factoryItemUseObserved=0;
inline unsigned chaserItemUseObserved=0;
inline void __fastcall OnFactoryItemUse(void*);
// The native AI decision has one shared use call. Factory and Chaser must
// share its observer rather than install competing patches at that address.
inline bool InstallNativeAiItemObserver() {
    if(factoryItemUsePatch.Active())return true;
    factoryItemUse=reinterpret_cast<AiItemUse>(image+0x15188f0);
    return factoryItemUsePatch.Call(image+0x14e1947,reinterpret_cast<void*>(factoryItemUse),reinterpret_cast<void*>(OnFactoryItemUse));
}
using CanOverlapCube=bool(__fastcall*)(void*);
inline CanOverlapCube factoryCanOverlapCube=nullptr;
inline TwinKartNative::OwnedCodePatch factoryOverlapPatch;
inline bool __fastcall OnFactoryCanOverlapCube(void*);
inline bool nativeCharacterRules=false;
inline bool OwnedComponent(BYTE* component,BYTE* pawn,void* type);
using CharacterSkillQuery=bool(__fastcall*)(void*,const TwinKartEquipment::String*,unsigned char,void*);
using SkillSpawn=void*(__fastcall*)(void*,void*);
inline CharacterSkillQuery characterSkillQuery=nullptr;
inline SkillSpawn skillSpawn=nullptr;
inline TwinKartNative::OwnedCodePatch characterSkillQueryPatch,skillSpawnPatch;
using CubeAuthOverlap=void(__fastcall*)(void*,void*,void*);
inline CubeAuthOverlap cubeAuthOverlap=nullptr;
inline TwinKartNative::OwnedCodePatch cubeAuthOverlapPatch;
inline bool eventDiagnosticsInstalled=false;
inline wchar_t cubeBaseTrack[97]{};
using CurrentLevelName=TwinKartRace::ReturnString*(__fastcall*)(TwinKartRace::ReturnString*,void*,bool);
inline CurrentLevelName currentLevelName=nullptr;
inline TwinKartNative::OwnedCodePatch cubePathLevelPatch;
inline bool flagSkillsEnabled=false;
inline bool flagSkillTickObserved=false;
inline int flagSkillReadinessObserved[8]{-1,-1,-1,-1,-1,-1,-1,-1};
inline int equipmentSlot=0;
using SetBody=void(__fastcall*)(void*,const void*,const void*);
inline SetBody setBody=nullptr;
inline TwinKartNative::OwnedCodePatch bodyEquipmentPatch;
inline TwinKartEquipment::Body equipmentBody;
inline bool equipmentBodyReady=false;
inline unsigned localAiPreparedMask=0;
inline bool localStartRequested=false;
inline TwinKartCamera::ObjectIdentity humanStartMovement{};
inline TwinKartCamera::ObjectIdentity humanStartPhase{};
using KartTick=void(__fastcall*)(void*,float,int,void*);
using RawForward=void(__fastcall*)(void*,float);
using ArenaForwardAxis=float(*)(BYTE*,float);
inline ArenaForwardAxis arenaForwardAxis=nullptr;
using ArenaKartTick=void(*)(BYTE*,RawForward);
inline ArenaKartTick arenaKartTick=nullptr;
inline KartTick originalHumanKartTick=nullptr;
inline RawForward originalRawForward=nullptr;
inline TwinKartNative::OwnedCodePatch humanKartTickPatch,humanRawForwardPatch;
inline bool wasdForwardEnabled=false,humanForwardFallback=false,humanForwardLogged=false;
inline unsigned long long humanForwardKeyName=0;
inline ULONGLONG humanDriveSampleAfter=0;
inline unsigned humanDriveSamples=0;
inline ULONGLONG nextInfiniteBoosterCheck=0;
inline int infiniteBoosterFailures=0;
inline bool infiniteBoosterWasAvailable=false;
inline TwinKartCamera::ObjectIdentity pendingBoosterPress{};
inline ULONGLONG pendingBoosterUntil=0;
inline ULONGLONG humanBodyCheckUntil=0;
inline ULONGLONG humanBodyAwaitedSince=0;
inline bool humanBodyRepairTried=false;
inline bool humanSoundRepairTried=false;
inline TwinKartNative::OwnedCodePatch humanItemWidgetPatch;
inline TwinKartCamera::ObjectIdentity pendingHumanItemHud{},pendingHumanItemWidgetClass{};
inline unsigned long long pendingHumanItemWidgetName=0;
inline bool pendingHumanItemWidgetHasClass=false,humanItemWidgetRetried=false;
inline int lastHumanItemCount=-1;
inline bool humanHudStartHandled=false;
inline ULONGLONG localCountdownStartedAt=0;
inline bool localCountdownElapsedLogged=false;
inline BeginRule notifyCountdownBegin=nullptr;
inline TwinKartNative::OwnedCodePatch notifyCountdownPatch;
using NetMode=int(__fastcall*)(void*);
using FlagConfig=BYTE*(__fastcall*)(void*);
inline NetMode flagNetMode=nullptr;
inline NetMode aiSkillNetMode=nullptr;
inline TwinKartNative::OwnedCodePatch aiSkillModePatch;
inline TwinKartNative::OwnedCodePatch aiSkillUsePatch;
inline TwinKartNative::OwnedCodePatch aiSkillCanUsePatch;
inline TwinKartNative::OwnedCodePatch flagItemContextPatch;
using CanUseSkill=bool(__fastcall*)(void*,void*);
inline CanUseSkill aiSkillCanUse=nullptr,baseSkillCanUse=nullptr;
using UseSkillIfCan=void(__fastcall*)(void*);
inline UseSkillIfCan aiUseSkill=nullptr;
inline NetMode flagZoneNetMode=nullptr;
inline FlagConfig flagConfig=nullptr;
inline BeginRule flagMovementBegin=nullptr;
inline BYTE* flagPhysicsScope=nullptr;
inline TwinKartNative::OwnedCodePatch flagBeginPatch,flagPhysicsPatch,flagZonePatch,flagConfigPatches[2];
inline bool AiSlot(int slot,int players,int ais) {
    return players==1 && ais>=1 && ais<=7 && slot>=players && slot<players+ais;
}
inline bool Live(BYTE* object) {
    __try {
    if(!object || (*reinterpret_cast<unsigned*>(object+8)&0x18000)) return false;
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
    int count=*reinterpret_cast<int*>(image+0x69c28c4),index=*reinterpret_cast<int*>(object+0xc);
    return chunks && index>=0 && index<count && count<=2000000 && chunks[index/65536] && chunks[index/65536][index%65536].object==object && !(chunks[index/65536][index%65536].flags&0x30000000);
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
inline BYTE* World(BYTE* component) {
    using Get=BYTE*(__fastcall*)(void*);
    auto world=reinterpret_cast<Get>(image+0x3ddf4b0)(component);
    return Live(world) && !*reinterpret_cast<void**>(world+0x38)?world:nullptr;
}
inline bool Eligible(BYTE* rule) {
    // Chaser uses its separately leased role/body controller. An eight-seat
    // Chaser session must not accidentally enter circuit registration/results.
    return armed && armedMode!=108 && GetCurrentThreadId()==owner && Live(rule) &&
        *reinterpret_cast<void**>(rule)==image+0x51dd410 && World(rule);
}
inline bool InRaceWorld(BYTE* component) {
    if(!armed || armedMode==108 || GetCurrentThreadId()!=owner || !Live(component))return false;
    return raceWorld.Matches(World(component),*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),
        *reinterpret_cast<int*>(image+0x69c28c4));
}
inline bool InActorRaceWorld(BYTE* actor) {
    if(!armed || armedMode==108 || GetCurrentThreadId()!=owner || !Live(actor))return false;
    using GetWorld=BYTE*(__fastcall*)(void*);
    auto world=reinterpret_cast<GetWorld>((*reinterpret_cast<void***>(actor))[0x160/8])(actor);
    return Live(world) && !*reinterpret_cast<void**>(world+0x38) && raceWorld.Matches(world,
        *reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),*reinterpret_cast<int*>(image+0x69c28c4));
}
inline bool Local(BYTE* rule) {return Eligible(rule) && InRaceWorld(rule);}
inline bool BindWorld(BYTE* rule) {
    if(!Eligible(rule))return false;
    if(*reinterpret_cast<int*>(rule+0x130)!=1 || *reinterpret_cast<int*>(rule+0x138)<1 ||
       *reinterpret_cast<int*>(rule+0x138)>7)return false;
    auto world=World(rule);
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
    int count=*reinterpret_cast<int*>(image+0x69c28c4);
    TwinKartCamera::ObjectIdentity identity{};
    if(!TwinKartCamera::Capture(world,chunks,count,&identity)) {
        // Native FWeakObjectPtr assignment allocates a missing serial using
        // the original engine, as in our existing cockpit lifecycle adapter.
        struct Weak {int index,serial;} weak{-1,0};
        using Assign=void(__fastcall*)(Weak*,void*);
        reinterpret_cast<Assign>(image+0x2796c20)(&weak,world);
        if(!TwinKartCamera::Capture(world,chunks,count,&identity) ||
            identity.index!=weak.index || identity.serial!=weak.serial)return false;
    }
    return raceWorld.Bind(identity,chunks,count);
}
inline void Emit(const char* name,int slot,int value) { if(event) event(name,slot,value); }
inline bool FactoryActorWorld(BYTE* actor) {
    if(!factoryCompatEnabled || GetCurrentThreadId()!=owner || !Live(actor))return false;
    using GetWorld=BYTE*(__fastcall*)(void*);
    auto world=reinterpret_cast<GetWorld>((*reinterpret_cast<void***>(actor))[0x160/8])(actor);
    return Live(world) && !*reinterpret_cast<void**>(world+0x38) && factoryWorld.Matches(world,
        *reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),*reinterpret_cast<int*>(image+0x69c28c4));
}
inline bool CaptureOwnedWorld(BYTE* world,LocalRaceWorld& lease) {
    if(GetCurrentThreadId()!=owner || !Live(world) || *reinterpret_cast<void**>(world+0x38))return false;
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);int count=*reinterpret_cast<int*>(image+0x69c28c4);
    if(armed && raceWorld.Matches(world,chunks,count)){lease=raceWorld;return true;}
    if(factoryCompatEnabled && factoryWorld.Matches(world,chunks,count)){lease=factoryWorld;return true;}
    if(objectiveWorld.Matches(world,chunks,count)){lease=objectiveWorld;return true;}
    return false;
}
inline bool InFactoryComponentWorld(BYTE* component) {
    if(!factoryCompatEnabled || GetCurrentThreadId()!=owner || !Live(component))return false;
    return factoryWorld.Matches(World(component),*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),
        *reinterpret_cast<int*>(image+0x69c28c4));
}
inline bool InChaserItemWorld(BYTE* actor) {
    __try {
        if(!armed || armedMode!=108 || !chaserItemContextEnabled || GetCurrentThreadId()!=owner || !Live(actor))return false;
        using GetWorld=BYTE*(__fastcall*)(void*);using Type=void*(__fastcall*)();
        auto world=reinterpret_cast<GetWorld>((*reinterpret_cast<void***>(actor))[0x160/8])(actor);
        if(!Live(world) || *reinterpret_cast<void**>(world+0x38) || !objectiveWorld.Matches(world,
            *reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),*reinterpret_cast<int*>(image+0x69c28c4)))return false;
        auto mode=*reinterpret_cast<BYTE**>(world+0x118);
        return Live(mode) && *reinterpret_cast<void**>(mode+0x10)==reinterpret_cast<Type>(image+0x22507c0)();
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
inline bool BindFactorySpawn(BYTE* rule,BYTE* mode,int slot) {
    if(!factoryCompatEnabled || GetCurrentThreadId()!=owner || !Live(rule) || !Live(mode) ||
       *reinterpret_cast<void**>(rule)!=image+0x51dd410 ||
       TwinKartFactory::SlotRole(slot,*reinterpret_cast<int*>(rule+0x130),*reinterpret_cast<int*>(rule+0x138))==TwinKartFactory::Role::Invalid)return false;
    using Type=void*(__fastcall*)();
    if(*reinterpret_cast<void**>(mode+0x10)!=reinterpret_cast<Type>(image+0x24345d0)())return false;
    auto world=World(rule);
    if(!world || *reinterpret_cast<void**>(world+0x118)!=mode || !FactoryActorWorld(mode)) {
        // Bind once, using an engine-allocated serial, before retaining any
        // Factory actor. Subsequent foreign worlds cannot take this lease.
        if(!world || *reinterpret_cast<void**>(world+0x118)!=mode)return false;
        auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
        int count=*reinterpret_cast<int*>(image+0x69c28c4);
        TwinKartCamera::ObjectIdentity identity{};
        if(!TwinKartCamera::Capture(world,chunks,count,&identity)) {
            struct Weak {int index,serial;} weak{-1,0};
            reinterpret_cast<void(__fastcall*)(Weak*,void*)>(image+0x2796c20)(&weak,world);
            if(!TwinKartCamera::Capture(world,chunks,count,&identity) || identity.index!=weak.index || identity.serial!=weak.serial)return false;
        }
        if(!factoryWorld.Bind(identity,chunks,count))return false;
    }
    return FactoryActorWorld(mode);
}
inline void* __fastcall OnFactoryConstruct(void* raw) {
    auto values=static_cast<BYTE**>(raw);
    if(raw && factorySpawnSlot>=TwinKartFactory::RacerCount && factorySpawnSlot<TwinKartFactory::PopulationCount &&
       factoryCompatEnabled && GetCurrentThreadId()==owner) {
        auto pawn=values[1];using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
        if(FactoryActorWorld(pawn) && *reinterpret_cast<void**>(pawn+0x10)==reinterpret_cast<Type>(image+0x2433330)()) {
            const unsigned getters[]={0x22b1e10,0x22f9030,0x23077d0};bool expected=false;
            for(auto getter:getters)if(values[0]==reinterpret_cast<Type>(image+getter)())expected=true;
            if(expected) {
                auto existing=reinterpret_cast<Find>((*reinterpret_cast<void***>(pawn))[0x620/8])(pawn,values[0]);
                if(Live(existing) && *reinterpret_cast<void**>(existing+0xa0)==pawn && *reinterpret_cast<void**>(existing+0x10)==values[0]) {
                    Emit("native_factory_robot_component_reused",factorySpawnSlot,1);return existing;
                }
            }
        }
    }
    return construct(raw);
}
inline bool InstallFactoryCompatibility() {
    wchar_t enabled[4]{};
    if(GetEnvironmentVariableW(L"TWIN_KART_FACTORY_COMPAT",enabled,4)!=1 || enabled[0]!=L'1')return true;
    if(factoryCompatEnabled)return GetCurrentThreadId()==owner;
    if(!installed || GetCurrentThreadId()!=owner)return false;
    // Exact construction calls in the original TobyRushAiPawn dedicated
    // registration (interface index6). Never invoke ordinary RaceAiPawn's
    // registration, modify class defaults or append components to forty pawns
    // through the ordinary seven-AI loop.
    const unsigned sites[]={0x19ea8dc,0x19ea914,0x19ea957};
    const BYTE enablePrefix[]={0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0xc6,0x81,0xd8,0x0e,0x00,0x00,0x01};
    const BYTE aiFlagPrefix[]={0xc6,0x81,0x21,0x03,0x00,0x00,0x01,0xc3};
    if(memcmp(image+0x15c3790,enablePrefix,sizeof(enablePrefix)) || memcmp(image+0x23b02c0,aiFlagPrefix,sizeof(aiFlagPrefix)))return false;
    for(unsigned i=0;i<3;++i)if(!factoryConstructionPatches[i].Call(image+sites[i],
        reinterpret_cast<void*>(construct),reinterpret_cast<void*>(OnFactoryConstruct))) {
        for(auto& patch:factoryConstructionPatches)patch.Restore();return false;
    }
    const BYTE factoryManagerPrefix[]={0x40,0x55,0x53,0x41,0x56,0x48,0x8d,0x6c,0x24,0xb9,0x48,0x81,0xec,0xb0,0x00,0x00,0x00};
    if(!factoryManagerPatch.Entry(image+0x1954300,factoryManagerPrefix,sizeof(factoryManagerPrefix),
        reinterpret_cast<void*>(OnFactoryManagerBegin),reinterpret_cast<void**>(&factoryManagerBegin))) {
        factoryManagerPatch.Restore();for(auto& patch:factoryConstructionPatches)patch.Restore();return false;
    }
    // Only the Factory rule's custom losing-podium call. The native caller
    // accepts a failed soft load, while PlayCustomPodium dereferences the null
    // sequence actor returned for that input. Do not patch the shared engine
    // sequence factory or substitute an unrelated movie/result.
    factoryCustomPodium=reinterpret_cast<CustomPodium>(image+0x16b1400);
    if(!factoryPodiumPatch.Call(image+0x195b49c,reinterpret_cast<void*>(factoryCustomPodium),reinterpret_cast<void*>(OnFactoryPodium))) {
        factoryPodiumPatch.Restore();factoryManagerPatch.Restore();for(auto& patch:factoryConstructionPatches)patch.Restore();return false;
    }
    const BYTE factoryIntroPrefix[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x48,0x89,0x7c,0x24,0x20};
    factoryIntroSequence=reinterpret_cast<IntroSequence>(image+0x191f790);
    if(!factoryIntroSequencePatch.Call(image+0x19661be,reinterpret_cast<void*>(factoryIntroSequence),reinterpret_cast<void*>(OnFactoryIntroSequence)) ||
       !factoryIntroPatch.Entry(image+0x1965e40,factoryIntroPrefix,sizeof(factoryIntroPrefix),
           reinterpret_cast<void*>(OnFactoryIntroBegin),reinterpret_cast<void**>(&factoryIntroBegin))) {
        factoryIntroPatch.Restore();factoryIntroSequencePatch.Restore();factoryPodiumPatch.Restore();factoryManagerPatch.Restore();
        for(auto& patch:factoryConstructionPatches)patch.Restore();return false;
    }
    if(!InstallNativeAiItemObserver()) {
        factoryItemUsePatch.Restore();factoryIntroPatch.Restore();factoryIntroSequencePatch.Restore();factoryPodiumPatch.Restore();factoryManagerPatch.Restore();
        for(auto& patch:factoryConstructionPatches)patch.Restore();return false;
    }
    const BYTE factoryOverlapPrefix[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0x99,0xa0,0x00,0x00,0x00};
    if(!factoryOverlapPatch.Entry(image+0x1525350,factoryOverlapPrefix,sizeof(factoryOverlapPrefix),
        reinterpret_cast<void*>(OnFactoryCanOverlapCube),reinterpret_cast<void**>(&factoryCanOverlapCube))) {
        factoryOverlapPatch.Restore();factoryItemUsePatch.Restore();factoryIntroPatch.Restore();factoryIntroSequencePatch.Restore();factoryPodiumPatch.Restore();factoryManagerPatch.Restore();
        for(auto& patch:factoryConstructionPatches)patch.Restore();return false;
    }
    // The native winner rule calls this exact UI handler with the original
    // winning pawn. Observe the local recipient; leave all native scoring,
    // report, podium and progression calls in charge.
    const BYTE winReportPrefix[]={0x40,0x53,0x55,0x56,0x48,0x83,0xec,0x40,0x48,0x8b,0x81,0x88,0x00,0x00,0x00};
    if(!factoryWinReportPatch.Entry(image+0x196cef0,winReportPrefix,sizeof(winReportPrefix),
        reinterpret_cast<void*>(OnFactoryWinReport),reinterpret_cast<void**>(&factoryWinReport))) {
        factoryOverlapPatch.Restore();factoryItemUsePatch.Restore();factoryIntroPatch.Restore();factoryIntroSequencePatch.Restore();
        factoryPodiumPatch.Restore();factoryManagerPatch.Restore();
        for(auto& patch:factoryConstructionPatches)patch.Restore();return false;
    }
    factoryCompatEnabled=true;Emit("native_factory_registration_adapter_installed",-1,1);return true;
}
inline bool __fastcall OnCharacterSkillQuery(void* component,const TwinKartEquipment::String* character,unsigned char grade,void* output) {
    bool local=InRaceWorld(static_cast<BYTE*>(component));
    TwinKartEquipment::Preset role;
    if(local && nativeCharacterRules) {
        auto pawn=*reinterpret_cast<BYTE**>(static_cast<BYTE*>(component)+0xa0);
        using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
        auto type=reinterpret_cast<Type>(image+0x23afeb0)();
        auto state=Live(pawn)?reinterpret_cast<Find>((*reinterpret_cast<void***>(pawn))[0x620/8])(pawn,type):nullptr;
        if(OwnedComponent(state,pawn,type) && InRaceWorld(state) &&
           character && TwinKartEquipment::Valid(*character,97) &&
           eventSkillRoster.GetCharacter(*reinterpret_cast<int*>(state+0x168),role)) {
            // This PC ABI passes an OWNED by-value FString through RDX.
            // The original query releases its data on both return paths.
            // Replace the caller's native-owned temporary via native deep
            // assignment; never hand a borrowed std::wstring to its destructor.
            reinterpret_cast<TwinKartEquipment::CopyString>(image+0x923880)(
                const_cast<TwinKartEquipment::String*>(character),&role.character[0]);
        }
    }
    if(eventDiagnosticsInstalled && local && character && TwinKartEquipment::Valid(*character,97) && TwinKartEquipment::Nonempty(*character)) {
        const wchar_t prefix[]=L"Character:";
        if(!wcsncmp(character->data,prefix,ARRAYSIZE(prefix)-1))Emit("native_event_skill_character_id",-1,_wtoi(character->data+ARRAYSIZE(prefix)-1));
        Emit("native_event_skill_costume_grade",-1,grade);
    }
    bool result=characterSkillQuery(component,character,grade,output);
    if(eventDiagnosticsInstalled && local && output)Emit("native_event_skill_definition_count",-1,result?*reinterpret_cast<int*>(static_cast<BYTE*>(output)+16):-1);
    return result;
}
inline void* __fastcall OnSkillSpawn(void* component,void* parameter) {
    void* result=skillSpawn(component,parameter);
    if(InRaceWorld(static_cast<BYTE*>(component))) {
        Emit("native_event_skill_spawned",-1,result?1:0);
        if(result && Live(static_cast<BYTE*>(result)))Emit("native_event_skill_actor_vtable",-1,static_cast<int>(*static_cast<BYTE**>(result)-image));
    }
    return result;
}
inline bool InChaserComponentWorld(BYTE* component) {
    return Live(component) && GetCurrentThreadId()==owner && armed && armedMode==108 && chaserItemContextEnabled &&
        objectiveWorld.Matches(World(component),*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),
            *reinterpret_cast<int*>(image+0x69c28c4));
}
inline bool CubeItemCount(BYTE* pawn,int& items,bool factory=false,bool chaser=false) {
    __try {
    items=0;using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
    if(!Live(pawn) || (factory && !FactoryActorWorld(pawn)) || (chaser && !InChaserItemWorld(pawn)))return false;
    auto type=reinterpret_cast<Type>(image+0x22ef370)();
    auto slot=reinterpret_cast<Find>((*reinterpret_cast<void***>(pawn))[0x620/8])(pawn,type);
    if(!Live(slot) || *reinterpret_cast<void**>(slot+0x10)!=type || *reinterpret_cast<void**>(slot+0xa0)!=pawn ||
       !(chaser?InChaserComponentWorld(slot):factory?InFactoryComponentWorld(slot):InRaceWorld(slot)))return false;
    struct Array {BYTE** data;int count,capacity;};auto stacks=*reinterpret_cast<Array*>(slot+0x170);
    // Exact reflected ItemStacks (+0x170) and ChunkItems (+0xc0). These are
    // borrowed original arrays; observe live chunk counts without allocating.
    if(stacks.count<0 || stacks.count>8 || stacks.capacity<stacks.count || stacks.capacity>64 || (stacks.count && !stacks.data))return false;
    auto stackType=reinterpret_cast<Type>(image+0x22ef0d0)();
    for(int i=0;i<stacks.count;++i) {
        auto stack=stacks.data[i];if(!Live(stack) || *reinterpret_cast<void**>(stack+0x10)!=stackType ||
           !(chaser?InChaserComponentWorld(stack):factory?InFactoryComponentWorld(stack):InRaceWorld(stack)))return false;
        auto chunks=*reinterpret_cast<Array*>(stack+0xc0);
        if(chunks.count<0 || chunks.count>128 || chunks.capacity<chunks.count || chunks.capacity>1024 || (chunks.count && !chunks.data))return false;
        items+=chunks.count;
    }
    return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {items=0;return false;}
}
inline bool ReadNativeChaserItemUse(void* raw,BYTE*& pawn,int& slot,int& count) {
    __try {
        auto item=static_cast<BYTE*>(raw);using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
        if(!InChaserComponentWorld(item) || *reinterpret_cast<void**>(item+0x10)!=reinterpret_cast<Type>(image+0x22d7240)())return false;
        pawn=*reinterpret_cast<BYTE**>(item+0xa0);
        if(!InChaserItemWorld(pawn))return false;
        auto type=reinterpret_cast<Type>(image+0x23afeb0)();
        auto state=reinterpret_cast<Find>((*reinterpret_cast<void***>(pawn))[0x620/8])(pawn,type);
        if(!Live(state) || !InChaserComponentWorld(state) || *reinterpret_cast<void**>(state+0xa0)!=pawn ||
           *reinterpret_cast<void**>(state+0x10)!=type || !state[0x321] || state[0x338]!=1)return false;
        slot=*reinterpret_cast<int*>(state+0x168);
        return slot>=1 && slot<=7 && CubeItemCount(pawn,count,false,true);
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
inline void __fastcall OnFactoryItemUse(void* raw) {
    BYTE* pawn=nullptr;int slot=-1,before=0,after=0;bool observed=false;
    BYTE* chaserPawn=nullptr;int chaserSlot=-1,chaserBefore=0;
    bool chaserObserved=ReadNativeChaserItemUse(raw,chaserPawn,chaserSlot,chaserBefore);
    __try {
        auto item=static_cast<BYTE*>(raw);using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
        if(InFactoryComponentWorld(item) && *reinterpret_cast<void**>(item+0x10)==reinterpret_cast<Type>(image+0x22d7240)()) {
            pawn=*reinterpret_cast<BYTE**>(item+0xa0);
            if(FactoryActorWorld(pawn) && *reinterpret_cast<void**>(pawn+0x10)==reinterpret_cast<Type>(image+0x2431240)()) {
                auto type=reinterpret_cast<Type>(image+0x23afeb0)();
                auto state=reinterpret_cast<Find>((*reinterpret_cast<void***>(pawn))[0x620/8])(pawn,type);
                if(Live(state) && *reinterpret_cast<void**>(state+0xa0)==pawn && *reinterpret_cast<void**>(state+0x10)==type) {
                    slot=*reinterpret_cast<int*>(state+0x168);
                    observed=slot>=1 && slot<TwinKartFactory::RacerCount && CubeItemCount(pawn,before,true);
                }
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER){observed=false;}
    factoryItemUse(raw); // Original native decision's use call, exactly once.
    if(chaserObserved) {
        BYTE* currentPawn=nullptr;int currentSlot=-1,currentCount=0;
        if(ReadNativeChaserItemUse(raw,currentPawn,currentSlot,currentCount) && currentPawn==chaserPawn && currentSlot==chaserSlot) {
            if(!(chaserItemUseObserved&(1u<<chaserSlot))) {
                chaserItemUseObserved|=1u<<chaserSlot;Emit("native_chaser_ai_item_use_observed",chaserSlot,1);
            }
            if(chaserBefore>currentCount)Emit("native_chaser_ai_item_consumed",chaserSlot,chaserBefore-currentCount);
        }
    }
    if(observed && CubeItemCount(pawn,after,true)) {
        if(!(factoryItemUseObserved&(1u<<slot))) {
            factoryItemUseObserved|=1u<<slot;Emit("native_factory_cooperative_item_use_observed",slot,1);
        }
        if(before>after)Emit("native_factory_cooperative_item_consumed",slot,before-after);
    }
}
inline bool __fastcall OnFactoryCanOverlapCube(void* raw) {
    bool original=factoryCanOverlapCube(raw);if(original)return true;
    __try {
        auto item=static_cast<BYTE*>(raw);using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
        if(!InFactoryComponentWorld(item) || *reinterpret_cast<void**>(item+0x10)!=reinterpret_cast<Type>(image+0x22d7240)() || item[0x120])return original;
        auto pawn=*reinterpret_cast<BYTE**>(item+0xa0);
        if(!FactoryActorWorld(pawn) || *reinterpret_cast<void**>(pawn+0x10)!=reinterpret_cast<Type>(image+0x2431240)())return original;
        auto type=reinterpret_cast<Type>(image+0x23afeb0)();
        auto state=reinterpret_cast<Find>((*reinterpret_cast<void***>(pawn))[0x620/8])(pawn,type);
        if(!Live(state) || *reinterpret_cast<void**>(state+0xa0)!=pawn || *reinterpret_cast<void**>(state+0x10)!=type)return original;
        int slot=*reinterpret_cast<int*>(state+0x168);
        // The native predicate accepts a locally controlled pawn or a native
        // AI-pawn flag. Our local cooperative controller keeps TobyRushPawn,
        // which satisfies neither remote-human default. Add only that local
        // ownership case; keep the original explicit ignore-cube flag, human,
        // Android, foreign-world and original-success behavior unchanged.
        return slot>=1 && slot<TwinKartFactory::RacerCount;
    } __except(EXCEPTION_EXECUTE_HANDLER){return original;}
}
inline void __fastcall OnCubeAuthOverlap(void* rawCube,void* rawPawn,void* rawItem) {
    auto cube=static_cast<BYTE*>(rawCube),pawn=static_cast<BYTE*>(rawPawn),item=static_cast<BYTE*>(rawItem);
    int before=0,slot=-1;unsigned first=0,second=0;bool observe=false;
    if(TwinKartRace::CubeMode(armedMode) && InRaceWorld(item) && Live(pawn) &&
       *reinterpret_cast<void**>(item+0xa0)==pawn && InActorRaceWorld(cube)) {
        using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
        auto state=reinterpret_cast<Find>((*reinterpret_cast<void***>(pawn))[0x620/8])(pawn,reinterpret_cast<Type>(image+0x23afeb0)());
        if(Live(state))slot=*reinterpret_cast<int*>(state+0x168);
        if(slot>=0 && slot<=7 && CubeItemCount(pawn,before)) {
            first=cube[0x228];second=cube[0x2c0];observe=true;
        }
    }
    cubeAuthOverlap(rawCube,rawPawn,rawItem);
    int after=0;
    if(observe && InRaceWorld(item) && CubeItemCount(pawn,after)) {
        Emit("native_local_cube_pickup_designated_first",slot,static_cast<int>(first));
        Emit("native_local_cube_pickup_designated_second",slot,static_cast<int>(second));
        Emit("native_local_cube_pickup_items_before",slot,before);
        Emit("native_local_cube_pickup_items_after",slot,after);
    }
}
inline bool InstallEventDiagnostics() {
    if(eventDiagnosticsInstalled)return true;
    // This native query boundary also supplies event role skills, so it must
    // be installed in normal play, independently of diagnostic logging.
    characterSkillQuery=reinterpret_cast<CharacterSkillQuery>(image+0x1697f40);
    if(!characterSkillQueryPatch.Active() && !characterSkillQueryPatch.Call(image+0x16c0f60,
        reinterpret_cast<void*>(characterSkillQuery),reinterpret_cast<void*>(OnCharacterSkillQuery)))return false;
    wchar_t enabled[4]{};
    if(GetEnvironmentVariableW(L"TWIN_KART_EVENT_DIAGNOSTICS",enabled,4)!=1 || enabled[0]!=L'1')return true;
    skillSpawn=reinterpret_cast<SkillSpawn>(image+0x16c1b50);
    const BYTE cubePrefix[]={0x48,0x89,0x5c,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x57,0x48,0x83,0xec,0x30};
    if(!skillSpawnPatch.Call(image+0x16c0fca,reinterpret_cast<void*>(skillSpawn),reinterpret_cast<void*>(OnSkillSpawn)) ||
       !cubeAuthOverlapPatch.Entry(image+0x17a9850,cubePrefix,sizeof(cubePrefix),reinterpret_cast<void*>(OnCubeAuthOverlap),reinterpret_cast<void**>(&cubeAuthOverlap))) {
        cubeAuthOverlapPatch.Restore();skillSpawnPatch.Restore();characterSkillQueryPatch.Restore();return false;
    }
    eventDiagnosticsInstalled=true;return true;
}
inline bool ReadEquipmentPreset(TwinKartEquipment::Preset& preset,BYTE** accountOut=nullptr) {
    __try {
        using Type=void*(__fastcall*)();auto type=reinterpret_cast<Type>(image+0x220fc90)();
        auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
        int count=*reinterpret_cast<int*>(image+0x69c28c4);BYTE* found=nullptr;
        if(!type || !chunks || count<1 || count>2000000)return false;
        for(int index=0;index<count;++index) {
            auto chunk=chunks[index/65536];if(!chunk)continue;auto object=static_cast<BYTE*>(chunk[index%65536].object);
            if(!object || (chunk[index%65536].flags&0x30000000) || !Live(object) ||
               (*reinterpret_cast<unsigned*>(object+8)&0x18030) || *reinterpret_cast<void**>(object+0x10)!=type)continue;
            if(found)return false;found=object;
        }
        // The owning UKdAccount is UObject::Outer (+0x20). Live PC data
        // shows +0x28 points at the account's embedded interface (+0x28),
        // not the UObject header; treating it as an object rejects valid gear.
        BYTE* account=found?*reinterpret_cast<BYTE**>(found+0x20):nullptr;
        auto accountType=reinterpret_cast<Type>(image+0x22ff320)();
        if(!Live(account) || !accountType || *reinterpret_cast<void**>(account+0x10)!=accountType || !TwinKartEquipment::Snapshot(found+0x98,preset))return false;
        if(accountOut)*accountOut=account;return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
inline bool ReadCurrentEquipmentBody(BYTE* account,TwinKartEquipment::Body& body) {
    __try {
        // OnUPcKartBodyParamUpdated stores the selected/tuned payload here.
        // Reuse the original account getter for decoding and native field
        // construction, rather than transporting a newer packed ABI into PC.
        if(!Live(account))return false;
        const auto& encoded=*reinterpret_cast<const TwinKartEquipment::String*>(account+0x988);
        if(!TwinKartEquipment::Valid(encoded,8192) || !TwinKartEquipment::Nonempty(encoded))return false;
        using GetBody=void*(__fastcall*)(void*,void*);TwinKartEquipment::Body next;
        if(reinterpret_cast<GetBody>(image+0x1b4cfd0)(account,&next)!=&next || !TwinKartEquipment::ValidBody(next))return false;
        body=next;return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
inline bool OwnEquipmentDescriptor(const void* current,unsigned getter,unsigned offset) {
    if(!equipmentReady || !equipmentPawn || GetCurrentThreadId()!=owner || !Live(equipmentPawn))return false;
    using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
    auto type=reinterpret_cast<Type>(image+getter)();
    auto find=reinterpret_cast<Find>((*reinterpret_cast<void***>(equipmentPawn))[0x620/8]);
    BYTE* component=find(equipmentPawn,type);
    if(!Live(component))return false;
    bool local=InRaceWorld(component) || (FactoryActorWorld(equipmentPawn) && factoryWorld.Matches(World(component),
        *reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),*reinterpret_cast<int*>(image+0x69c28c4)));
    if(objectiveEquipmentScope) {
        auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
        int count=*reinterpret_cast<int*>(image+0x69c28c4);
        using GetActorWorld=BYTE*(__fastcall*)(void*);
        auto actorWorld=reinterpret_cast<GetActorWorld>((*reinterpret_cast<void***>(equipmentPawn))[0x160/8])(equipmentPawn);
        local=objectiveWorld.Matches(actorWorld,chunks,count) &&
            objectiveWorld.Matches(World(component),chunks,count);
    }
    return Live(component) && *reinterpret_cast<void**>(component+0xa0)==equipmentPawn &&
        *reinterpret_cast<void**>(component+0x10)==type && local && current==component+offset;
}
inline bool __fastcall OnKartDescriptorEqual(const void* current,const void* desired) {
    bool owned=OwnEquipmentDescriptor(current,0x22fef20,0x10b0);
    if(owned) {
        TwinKartEquipment::ApplyKart(const_cast<void*>(desired),equipmentPreset,reinterpret_cast<TwinKartEquipment::CopyString>(image+0x923880));
        Emit(equipmentSlot?"native_local_ai_kart_preset_applied":"native_local_player_kart_preset_applied",equipmentSlot,1);
    }
    bool equal=kartEqual(current,desired);
    // The retained objective initializer may already have the desired ID but
    // no skeletal mesh. Equality of descriptors must not skip the original
    // model preparation after its separately loaded soft dependencies arrive.
    // Restrict recovery to this synchronous, owned objective preparation call.
    if(equal && owned && objectiveEquipmentScope &&
       !Live(*reinterpret_cast<BYTE* const*>(static_cast<const BYTE*>(current)-0x10b0+0x4c0))) {
        Emit("native_objective_missing_kart_mesh_rebuild",equipmentSlot,1);return false;
    }
    return equal;
}
inline bool __fastcall OnCharacterDescriptorEqual(const void* current,const void* desired) {
    bool owned=OwnEquipmentDescriptor(current,0x2251540,0x1020);
    if(owned) {
        TwinKartEquipment::ApplyCharacter(const_cast<void*>(desired),equipmentPreset,reinterpret_cast<TwinKartEquipment::CopyString>(image+0x923880));
        Emit(equipmentSlot?"native_local_ai_character_preset_applied":"native_local_player_character_preset_applied",equipmentSlot,1);
    }
    bool equal=characterEqual(current,desired);
    if(equal && owned && objectiveEquipmentScope &&
       !Live(*reinterpret_cast<BYTE* const*>(static_cast<const BYTE*>(current)-0x1020+0x4c0))) {
        Emit("native_objective_missing_character_mesh_rebuild",equipmentSlot,1);return false;
    }
    return equal;
}
inline void __fastcall OnKartBody(void* component,const void* body,const void* channel) {
    if(equipmentBodyReady && OwnEquipmentDescriptor(component,0x22ff4a0,0)) {
        Emit("native_local_player_account_body_applied",0,1);
        setBody(component,&equipmentBody,channel);return;
    }
    setBody(component,body,channel);
}
inline bool OwnedComponent(BYTE* component,BYTE* pawn,void* type);
inline bool RegisteredObjectiveKart(const TwinKartEquipment::Preset& preset) {
    TwinKartRace::AiAssetLookup lookup;
    return TwinKartRace::BeginAiAssetLookup(image,lookup) &&
        TwinKartEquipment::Valid(preset.kart[0],97) && TwinKartEquipment::Nonempty(preset.kart[0]) &&
        TwinKartRace::RegisteredAiAsset(std::wstring(preset.kart[0].data),&lookup)==1 &&
        TwinKartEquipment::Valid(preset.character[0],97) && TwinKartEquipment::Nonempty(preset.character[0]) &&
        TwinKartEquipment::Valid(preset.character[1],97) && TwinKartEquipment::Nonempty(preset.character[1]) &&
        TwinKartRace::RegisteredAiAsset(std::wstring(preset.character[0].data),&lookup)==1 &&
        TwinKartRace::RegisteredAiAsset(std::wstring(preset.character[1].data),&lookup)==1;
}
inline int PrepareObjectiveAppearance(BYTE* pawn,int slot,const TwinKartEquipment::Preset* selected=nullptr,
                                      TwinKartRace::ObjectiveModelFailure* failure=nullptr) {
    if(!installed || GetCurrentThreadId()!=owner || !Live(pawn) || (pawn[0x5c]&0x10))return -1;
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
    int count=*reinterpret_cast<int*>(image+0x69c28c4);
    using GetActorWorld=BYTE*(__fastcall*)(void*);
    auto actorWorld=reinterpret_cast<GetActorWorld>((*reinterpret_cast<void***>(pawn))[0x160/8])(pawn);
    if(!objectiveWorld.Matches(actorWorld,chunks,count))return -2;
    using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
    auto find=reinterpret_cast<Find>((*reinterpret_cast<void***>(pawn))[0x620/8]);
    auto kartType=reinterpret_cast<Type>(image+0x22fef20)();
    auto characterType=reinterpret_cast<Type>(image+0x2251540)();
    auto kart=find(pawn,kartType);auto character=find(pawn,characterType);
    if(!OwnedComponent(kart,pawn,kartType) || !OwnedComponent(character,pawn,characterType))return -3;
    TwinKartEquipment::Preset next;
    if(selected)next=*selected;
    else if(!ReadEquipmentPreset(next))return -4;
    if(!RegisteredObjectiveKart(next))return -5;
    TwinKartRace::AiAssetLookup lookup;
    if(!TwinKartRace::BeginAiAssetLookup(image,lookup))return -6;
    // The kart body ID does not include every model-bearing part. In an
    // ordinary race the original player-resource phase prepares the complete
    // outfit first; the retained objective prototype has no such phase.
    // Prepare each actual selected dependency before the native model update.
    TwinKartRace::ObjectiveModelFailure modelFailure;
    if(!TwinKartRace::LoadObjectivePresetAssets(lookup,next,&modelFailure)) {
        if(failure)*failure=modelFailure;
        Emit(modelFailure.group?"native_objective_character_dependency_rejected":"native_objective_kart_dependency_rejected",
            slot,static_cast<int>(modelFailure.field));return -6;
    }
    if(!TwinKartRace::LoadObjectivePresetModels(lookup,next,&modelFailure)) {
        if(failure)*failure=modelFailure;
        Emit("native_objective_model_dependency_group",slot,static_cast<int>(modelFailure.group));
        Emit("native_objective_model_dependency_field",slot,static_cast<int>(modelFailure.field));
        Emit("native_objective_model_dependency_offset",slot,static_cast<int>(modelFailure.offset));
        Emit("native_objective_model_dependency_required",slot,modelFailure.required?1:0);
        const auto& identifier=modelFailure.group?next.character[modelFailure.field]:next.kart[modelFailure.field];
        Emit("native_objective_model_dependency_identifier",slot,TwinKartRace::ObjectiveResourceNumber(identifier));
        return -7;
    }
    BYTE* previousPawn=equipmentPawn;bool previousReady=equipmentReady;
    bool previousScope=objectiveEquipmentScope,previousBodyReady=equipmentBodyReady;
    auto previousPreset=equipmentPreset;int previousSlot=equipmentSlot;
    equipmentPawn=pawn;equipmentPreset=next;equipmentReady=true;
    equipmentSlot=slot;objectiveEquipmentScope=true;equipmentBodyReady=false;
    __try {
        // These original SessionBased initializer operations create their own
        // desired descriptors. Our existing comparison hooks apply the preset
        // before the original deep copy and asynchronous resource preparation.
        reinterpret_cast<void(__fastcall*)(void*)>(image+0x15be7e0)(kart);
        reinterpret_cast<void(__fastcall*)(void*,int)>(image+0x1534080)(character,1);
    } __finally {
        equipmentPawn=previousPawn;equipmentPreset=previousPreset;equipmentReady=previousReady;
        equipmentSlot=previousSlot;objectiveEquipmentScope=previousScope;equipmentBodyReady=previousBodyReady;
    }
    return 1; // preparation request, not model/physics readiness
}
// Loaded model assets, not visibility, physics or final rendered pixels.
// The original model components are skeletal-mesh components themselves;
// their +0x4c0 member is USkeletalMesh*, not another mesh component.
inline int ObjectiveAppearanceMeshMask(BYTE* pawn) {
    if(!installed || GetCurrentThreadId()!=owner || !Live(pawn) || (pawn[0x5c]&0x10))return -1;
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
    int count=*reinterpret_cast<int*>(image+0x69c28c4);
    using GetActorWorld=BYTE*(__fastcall*)(void*);
    auto actorWorld=reinterpret_cast<GetActorWorld>((*reinterpret_cast<void***>(pawn))[0x160/8])(pawn);
    if(!objectiveWorld.Matches(actorWorld,chunks,count))return -1;
    using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
    auto find=reinterpret_cast<Find>((*reinterpret_cast<void***>(pawn))[0x620/8]);
    int mask=0;const unsigned getters[]{0x22fef20,0x2251540};
    for(unsigned index=0;index<2;++index) {
        auto type=reinterpret_cast<Type>(image+getters[index])();auto component=find(pawn,type);
        if(!OwnedComponent(component,pawn,type) || !objectiveWorld.Matches(World(component),chunks,count))return -1;
        if(Live(*reinterpret_cast<BYTE**>(component+0x4c0)))mask|=1<<index;
    }
    return mask;
}
inline void* __fastcall OnConstruct(void* parameters) {
    // Only the five original construction calls inside this AI registration
    // function are redirected. FStaticConstructObjectParameters begins with
    // Class, Outer; both were verified in the exact PC constructor/consumer.
    auto values=static_cast<BYTE**>(parameters);
    if(armed && GetCurrentThreadId()==owner && registrationActor && parameters &&
       values[1]==registrationActor && Live(registrationActor) && Live(values[0])) {
        using Find=BYTE*(__fastcall*)(void*,void*);
        auto find=reinterpret_cast<Find>((*reinterpret_cast<void***>(registrationActor))[0x620/8]);
        BYTE* existing=find(registrationActor,values[0]);
        if(Live(existing) && *reinterpret_cast<void**>(existing+0xa0)==registrationActor &&
           *reinterpret_cast<void**>(existing+0x10)==values[0]) {
            Emit("native_local_ai_component_reused",-1,1);
            return existing;
        }
    }
    return construct(parameters);
}
inline void RegisterMissing(BYTE* actor) {
    BYTE* previous=registrationActor; registrationActor=actor;
    __try {
        // Native AI registration selects AISpeed[Team]Component when this
        // mode flag is true and AIItem[Team]Component otherwise. It is not an
        // authority flag. The normal constructor defaults it to item mode.
        *reinterpret_cast<BYTE*>(actor+0x328)=TwinKartRace::SpeedMode(armedMode)?1:0;
        using Register=void(__fastcall*)(void*);
        reinterpret_cast<Register>(image+0x1973d10)(actor+0x320);
    } __finally { registrationActor=previous; }
}
inline void RestoreConstruction() { for(auto& patch:constructionPatches) patch.Restore(); }
inline void* __fastcall OnItemContextConstruct(void* raw) {
    // Only ItemComponent's original item-context construction call is hooked.
    // Choose the original mode subclass before NewObject and registration;
    // native code then owns the cache, lifecycle and all targeting/SP rules.
    // Do not replace vtables, class defaults or an already registered context.
    auto parameters=static_cast<BYTE**>(raw);
    bool chaser=armedMode==108 && chaserItemContextEnabled;
    if(raw && armed && (TwinKartRace::FlagMode(armedMode) || chaser) && GetCurrentThreadId()==owner) {
        auto pawn=parameters[1];using Type=void*(__fastcall*)();
        // Both original mode constructors select SpeedSingleAiPawn for bots;
        // ChaserPawn already configures its dedicated context for the human.
        if(Live(pawn) && *reinterpret_cast<void**>(pawn+0x10)==reinterpret_cast<Type>(image+0x2413310)() &&
           parameters[0]==reinterpret_cast<Type>(image+0x22d8460)()) {
            if(!(chaser?InChaserItemWorld(pawn):InActorRaceWorld(pawn)))return construct(raw);
            auto flagType=reinterpret_cast<Type>(image+(chaser?0x2250d20:0x22acef0))();
            if(Live(static_cast<BYTE*>(flagType))) {
                alignas(8) BYTE local[0x40];memcpy(local,raw,sizeof(local));
                *reinterpret_cast<void**>(local)=flagType;
                auto result=construct(local);
                Emit(chaser?"native_chaser_original_ai_item_context_selected":"native_local_flag_ai_item_context_selected",-1,result?1:0);
                return result;
            }
        }
    }
    return construct(raw);
}
inline bool __fastcall OnSpawn(void* raw,void* mode,int slot,void* start,void** pawn,bool* isAi) {
    auto rule=static_cast<BYTE*>(raw);
    if(BindWorld(rule) && isAi) {
        int players=*reinterpret_cast<int*>(rule+0x130),ais=*reinterpret_cast<int*>(rule+0x138);
        if(players==1 && ais>=1 && ais<=7 && slot>=0 && slot<players+ais) {
            *isAi=AiSlot(slot,players,ais);
            Emit("native_local_slot_classified",slot,*isAi?1:0);
        }
    }
    bool factory=BindFactorySpawn(rule,static_cast<BYTE*>(mode),slot);
    if(factory && isAi) {
        // Standalone MajorConnect defaults every slot to the racer path.
        // Keep the first four original racer pawns; only the remaining
        // thirty-six slots select its original TobyRushAiPawn class.
        *isAi=TwinKartFactory::SlotRole(slot,4,36)==TwinKartFactory::Role::Android;
        Emit("native_factory_slot_classified",slot,*isAi?1:0);
    }
    // A separately validated original Chaser mode may classify its own local
    // slots before the unchanged native spawn. Ordinary/Factory semantics and
    // the eight-seat circuit request contract are not broadened by this hook.
    if(armedMode==108 && chaserSpawnClassifier)chaserSpawnClassifier(raw,mode,slot,isAi);
    if(armedMode==108 && chaserStartSelector && !chaserStartSelector(raw,mode,slot,start))return false;
    int previousFactorySlot=factorySpawnSlot;factorySpawnSlot=factory?slot:-1;
    bool result=false;
    __try {result=spawn(raw,mode,slot,start,pawn,isAi);}
    __finally {factorySpawnSlot=previousFactorySlot;}
    if(factory)Emit("native_factory_original_spawn",slot,result?1:0);
    if(Local(rule)) {
        Emit("native_local_pawn_spawned",slot,result && pawn && *pawn?1:0);
    }
    return result;
}
inline bool OwnedComponent(BYTE* component,BYTE* pawn,void* type) {
    return Live(component) && *reinterpret_cast<void**>(component+0xa0)==pawn && *reinterpret_cast<void**>(component+0x10)==type;
}
inline bool PrepareFactoryControlledPawn(BYTE* pawn,int slot) {
    if(slot<1 || slot>=TwinKartFactory::PopulationCount || !FactoryActorWorld(pawn))return false;
    using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
    auto pawnType=reinterpret_cast<Type>(image+(slot<TwinKartFactory::RacerCount?0x2431240:0x2433330))();
    if(*reinterpret_cast<void**>(pawn+0x10)!=pawnType)return false;
    auto type=reinterpret_cast<Type>(image+0x23afeb0)();
    auto state=reinterpret_cast<Find>((*reinterpret_cast<void***>(pawn))[0x620/8])(pawn,type);
    if(!OwnedComponent(state,pawn,type) || *reinterpret_cast<int*>(state+0x168)!=slot)return false;
    // Exact reflected UPawnStateComponent::bIsAiPlayer setter. Local bots
    // have no remote human supplying input. Keep their original pawn class
    // and team: TobyRushPawn remains a racer, not an Android. The original
    // Factory retirement/winner predicates distinguish those native classes.
    if(!state[0x321])reinterpret_cast<void(__fastcall*)(void*)>(image+0x23b02c0)(state);
    Emit("native_factory_ai_control_state_ready",slot,state[0x321]?1:0);return state[0x321]!=0;
}
inline void ObserveFactoryPawn(BYTE* rule,BYTE* pawn,int slot) {
    if(!factoryCompatEnabled || GetCurrentThreadId()!=owner || !Live(rule) ||
       *reinterpret_cast<void**>(rule)!=image+0x51dd410 || !FactoryActorWorld(pawn))return;
    if(!factoryWorld.Matches(World(rule),*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),
       *reinterpret_cast<int*>(image+0x69c28c4)))return;
    auto role=TwinKartFactory::SlotRole(slot,*reinterpret_cast<int*>(rule+0x130),*reinterpret_cast<int*>(rule+0x138));
    if(role==TwinKartFactory::Role::Invalid)return;
    using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
    auto type=reinterpret_cast<Type>(image+(role==TwinKartFactory::Role::Android?0x2433330:0x2431240))();
    if(*reinterpret_cast<void**>(pawn+0x10)!=type){Emit("native_factory_wrong_role_class",slot,0);return;}
    unsigned ready=0;
    if(role==TwinKartFactory::Role::Android) {
        const unsigned getters[]={0x22b1e10,0x22f9030,0x23077d0};
        for(unsigned i=0;i<3;++i) {
            auto componentType=reinterpret_cast<Type>(image+getters[i])();
            auto component=reinterpret_cast<Find>((*reinterpret_cast<void***>(pawn))[0x620/8])(pawn,componentType);
            if(OwnedComponent(component,pawn,componentType))ready|=1u<<i;
        }
        Emit("native_factory_robot_component_mask",slot,ready);
    }
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
    int count=*reinterpret_cast<int*>(image+0x69c28c4);TwinKartCamera::ObjectIdentity identity{};
    if(!TwinKartCamera::Capture(pawn,chunks,count,&identity)) {
        struct Weak {int index,serial;} weak{-1,0};
        reinterpret_cast<void(__fastcall*)(Weak*,void*)>(image+0x2796c20)(&weak,pawn);
        if(!TwinKartCamera::Capture(pawn,chunks,count,&identity) || identity.index!=weak.index || identity.serial!=weak.serial)return;
    }
    if(factoryPopulation.Observe(slot,role,identity,chunks,count,ready==7)) {
        Emit("native_factory_original_pawn_observed",slot,static_cast<int>(role));
        Emit("native_factory_population_present",-1,factoryPopulation.NativePopulationPresent(chunks,count)?1:0);
    } else Emit("native_factory_population_rejected",slot,0);
}
inline TwinKartRace::ReturnString* __fastcall CubePathLevelName(TwinKartRace::ReturnString* output,void* raw,bool removePrefix) {
    auto result=currentLevelName(output,raw,removePrefix);
    auto world=static_cast<BYTE*>(raw);
    auto cube=TwinKartRace::CubeWorldForTrack(armedMode,cubeBaseTrack);
    if(result!=output || !output || !cube || GetCurrentThreadId()!=owner || !armed || !Live(world) ||
       !raceWorld.Matches(world,*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),*reinterpret_cast<int*>(image+0x69c28c4)) ||
       !TwinKartRace::HasReturnRoute(*output))return result;
    auto leaf=wcsrchr(cube->package,L'/');
    if(!leaf || wcscmp(output->data,leaf+1))return result;
    // Only AIManage::InitSplineData's current-level-name call is redirected.
    // The real Cube world and all other level-name consumers stay unchanged.
    // Assign through the original allocator into the original returned FString.
    TwinKartRace::ReturnString base{cubeBaseTrack,static_cast<int>(wcslen(cubeBaseTrack)+1),static_cast<int>(wcslen(cubeBaseTrack)+1)};
    reinterpret_cast<TwinKartRace::AssignReturnString>(image+0x9118f0)(output,&base);
    Emit("native_local_cube_base_ai_path_selected",-1,armedMode);return result;
}
inline int __fastcall LocalAiSkillNetMode(void* raw) {
    int mode=aiSkillNetMode(raw);
    // Only AISkillComponent::TickComponent's exact native call is redirected.
    // Its randomized release timer is restricted to NM_DedicatedServer; an
    // owned standalone race is also authoritative. Preserve all native skill
    // readiness, cooldown, target selection and timer/UseSkillIfCan behavior.
    auto world=static_cast<BYTE*>(raw);
    if(mode==0 && armed && TwinKartRace::PlayableMode(armedMode) && GetCurrentThreadId()==owner && Live(world) &&
       !*reinterpret_cast<void**>(world+0x38) && raceWorld.Matches(world,
           *reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),*reinterpret_cast<int*>(image+0x69c28c4))) {
        if(flagSkillsEnabled && eventDiagnosticsInstalled && !flagSkillTickObserved) {
            flagSkillTickObserved=true;Emit("native_local_flag_ai_skill_tick_observed",-1,1);
        }
        return 1;
    }
    return mode;
}
inline void __fastcall OnAiUseSkill(void* raw) {
    BYTE* component=static_cast<BYTE*>(raw),*skill=nullptr,*holder=nullptr;float before=0;int slot=-1;bool heldFlag=false;
    if(InRaceWorld(component)) {
        using Current=BYTE*(__fastcall*)(void*);using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
        auto collection=*reinterpret_cast<BYTE**>(component+0x130);
        auto pawn=*reinterpret_cast<BYTE**>(component+0xa0);
        if(Live(collection) && Live(pawn) && *reinterpret_cast<void**>(component+0x10)==reinterpret_cast<Type>(image+0x2217c80)()) {
            skill=reinterpret_cast<Current>(image+0x1a51c70)(collection);
            if(Live(skill))before=*reinterpret_cast<float*>(skill+0x248);else skill=nullptr;
            auto state=reinterpret_cast<Find>((*reinterpret_cast<void***>(pawn))[0x620/8])(pawn,reinterpret_cast<Type>(image+0x23afeb0)());
            if(Live(state))slot=*reinterpret_cast<int*>(state+0x168);
            if(flagSkillsEnabled && eventDiagnosticsInstalled) {
                holder=reinterpret_cast<Find>((*reinterpret_cast<void***>(pawn))[0x620/8])(pawn,reinterpret_cast<Type>(image+0x22ac760)());
                if(Live(holder) && *reinterpret_cast<void**>(holder+0xa0)==pawn)heldFlag=holder[0x110]!=0;else holder=nullptr;
            }
        }
    }
    // Native timer/CanUseSkill/ServerUseSkill owns the action and the cost.
    // Observe a real SP decrease; UseCount is not a cumulative action counter.
    aiUseSkill(raw);
    if(skill && Live(skill) && InRaceWorld(component)) {
        float after=*reinterpret_cast<float*>(skill+0x248);
        if(before>after && before-after<1000000) {
            Emit("native_local_ai_skill_sp_spent",slot,static_cast<int>((before-after)*1000));
            if(holder && Live(holder)) {
                Emit("native_local_flag_ai_skill_held_before",slot,heldFlag?1:0);
                Emit("native_local_flag_ai_skill_held_after",slot,holder[0x110]?1:0);
            }
        }
    }
}
inline bool StandaloneFlagWorld(BYTE* world) {
    if(!armed || !TwinKartRace::FlagMode(armedMode) || GetCurrentThreadId()!=owner || !Live(world) ||
       *reinterpret_cast<void**>(world+0x38))return false;
    BYTE* mode=*reinterpret_cast<BYTE**>(world+0x118);
    using Type=void*(__fastcall*)();
    return Live(mode) && *reinterpret_cast<void**>(mode+0x10)==reinterpret_cast<Type>(image+0x22a77b0)();
}
inline int __fastcall LocalFlagPhysicsNetMode(void* raw) {
    int mode=flagNetMode(raw);
    // This call site belongs only to FlagMovement::BeginPlay. Standalone
    // authority needs the original client projectile/config initialization,
    // alongside the unchanged authoritative native game rules. No global
    // net-mode override or duplicate BeginPlay invocation is involved.
    if(mode==0 && flagPhysicsScope && World(flagPhysicsScope)==raw && StandaloneFlagWorld(static_cast<BYTE*>(raw))) {
        Emit("native_local_flag_projectile_initialized",-1,1);return 3;
    }
    return mode;
}
inline BYTE* __fastcall LocalFlagConfig(void* raw) {
    BYTE* notification=flagConfig(raw);
    if(StandaloneFlagWorld(World(static_cast<BYTE*>(raw))) && notification) {
        BYTE* config=*reinterpret_cast<BYTE**>(notification+0x20);
        using Type=void*(__fastcall*)();
        if(Live(config) && !(*reinterpret_cast<unsigned*>(config+8)&0x30) &&
           *reinterpret_cast<void**>(config+0x10)==reinterpret_cast<Type>(image+0x22a6580)() && !config[0x30]) {
            // Exact reflected bDoOverlapCheckInServer byte. The independent
            // per-world instance is native owned; never change its class CDO.
            config[0x30]=1;Emit("native_local_flag_authority_overlaps_enabled",-1,1);
        }
    }
    return notification;
}
inline int __fastcall LocalFlagZoneInitialization(void* raw) {
    int mode=flagZoneNetMode(raw);auto actor=static_cast<BYTE*>(raw);
    if(mode!=0 || !Live(actor) || GetCurrentThreadId()!=owner)return mode;
    using Type=void*(__fastcall*)();using GetWorld=BYTE*(__fastcall*)(void*);
    if(*reinterpret_cast<void**>(actor+0x10)!=reinterpret_cast<Type>(image+0x22add30)())return mode;
    BYTE* world=reinterpret_cast<GetWorld>((*reinterpret_cast<void***>(actor))[0x160/8])(actor);
    if(!StandaloneFlagWorld(world))return mode;
    // Only the first net-mode query in original FlagZone::BeginPlay gates
    // authoritative PawnList/goal overlap registration. Its second query and
    // all other actor/world queries keep standalone mode and native visuals.
    Emit("native_local_flag_goal_registered",*reinterpret_cast<int*>(actor+0x220),1);return 1;
}
inline bool EnsureAiDriver(BYTE* pawn,int slot) {
        using Type=void*(__fastcall*)(); using Find=BYTE*(__fastcall*)(void*,void*);
        auto aiType=reinterpret_cast<Type>(image+0x23bd880)();
        auto pathType=reinterpret_cast<Type>(image+0x22b1e10)();
        // Registration itself selects team components through the native
        // world's IsTeam predicate. Look up the matching mode-specific class.
        unsigned getter=TwinKartRace::BehaviorGetter(armedMode);if(!getter)return false;
        auto behaviorType=reinterpret_cast<Type>(image+getter)();
        bool exactAi=*reinterpret_cast<void**>(pawn+0x10)==aiType;
        auto find=reinterpret_cast<Find>((*reinterpret_cast<void***>(pawn))[0x620/8]);
        BYTE* path=find(pawn,pathType);
        BYTE* behavior=find(pawn,behaviorType);
        Emit("native_local_ai_driver_setup_state",slot,(exactAi?1:0)|(OwnedComponent(path,pawn,pathType)?2:0)|(OwnedComponent(behavior,pawn,behaviorType)?4:0));
        // A pointer alone is not a usable component. A pending-destruction
        // entry from a respawn/cache transition must follow native recovery,
        // just as a missing entry does; never report it as an initialized AI.
        if(exactAi && (!OwnedComponent(path,pawn,pathType) || !OwnedComponent(behavior,pawn,behaviorType))) {
            RegisterMissing(pawn);
            path=find(pawn,pathType);
            behavior=find(pawn,behaviorType);
            Emit("native_local_ai_driver_registered",slot,Live(path)?1:0);
        }
        Emit("native_local_ai_mode_behavior_ready",slot,OwnedComponent(behavior,pawn,behaviorType)?armedMode:0);
        if(exactAi && OwnedComponent(path,pawn,pathType)) {
            auto movementType=reinterpret_cast<Type>(image+0x22f9030)();
            BYTE* movement=find(pawn,movementType);
            if(OwnedComponent(movement,pawn,movementType)) {
                BYTE* current=*reinterpret_cast<BYTE**>(movement+0xf08);
                if(!Live(current) || current==path) {
                    // Exact native SetAIBaseComponent, independently verified
                    // as assigning this movement's AutoMoveComponent. Reusing
                    // a previously registered movement must not leave it null.
                    using Bind=void(__fastcall*)(void*,void*);
                    reinterpret_cast<Bind>(image+0x15d1100)(movement,path);
                    Emit("native_local_ai_movement_driver_bound",slot,*reinterpret_cast<void**>(movement+0xf08)==path?1:0);
                } else Emit("native_local_ai_driver_binding_conflict",slot,1);
                return OwnedComponent(behavior,pawn,behaviorType) && *reinterpret_cast<void**>(movement+0xf08)==path;
            }
        }
        return false;
    }
inline bool PrepareFlagAi(BYTE* pawn,int slot) {
    __try {
        using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
        auto aiType=reinterpret_cast<Type>(image+0x2413310)();
        auto stateType=reinterpret_cast<Type>(image+0x23afeb0)();
        if(!aiType || *reinterpret_cast<void**>(pawn+0x10)!=aiType)return false;
        auto find=reinterpret_cast<Find>((*reinterpret_cast<void***>(pawn))[0x620/8]);
        auto state=find(pawn,stateType);
        if(!OwnedComponent(state,pawn,stateType))return false;
        *reinterpret_cast<BYTE*>(state+0x321)=1;
        Emit("native_local_flag_ai_original_pawn_ready",slot,1);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
inline BYTE* EnsureFlagComponent(BYTE* pawn,unsigned getter,bool pawnHolder=false) {
    using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
    auto type=reinterpret_cast<Type>(image+getter)();
    if(!Live(pawn) || !Live(static_cast<BYTE*>(type)))return nullptr;
    auto find=reinterpret_cast<Find>((*reinterpret_cast<void***>(pawn))[0x620/8]);
    BYTE* component=find(pawn,type);
    if(OwnedComponent(component,pawn,type))return component;
    // Exact original NewObject/RegisterComponent sequence used by FlagPawn.
    // RegisterComponent owns initialization and BeginPlay. Never call either
    // lifecycle method ourselves or replace the AI pawn/interface class.
    alignas(8) BYTE parameters[0x40]{};
    using Parameters=void(__fastcall*)(void*,void*);
    reinterpret_cast<Parameters>(image+0x2771dd0)(parameters,type);
    *reinterpret_cast<BYTE**>(parameters+8)=pawn;
    component=static_cast<BYTE*>(construct(parameters));
    if(!Live(component) || *reinterpret_cast<void**>(component+0x10)!=type ||
       *reinterpret_cast<void**>(component+0x20)!=pawn)return nullptr;
    // AFlagPawn's default FlagHolder identifies its holder as a carrier.
    // Set before registration so the original receiver/holder callbacks see it.
    if(pawnHolder)*reinterpret_cast<BYTE*>(component+0x130)=1;
    using Register=void(__fastcall*)(void*);
    reinterpret_cast<Register>(image+0x3de4900)(component);
    return OwnedComponent(component,pawn,type)?component:nullptr;
}
inline bool CaptureFlagObject(BYTE* object,TwinKartCamera::ObjectIdentity& identity);
inline BYTE* FlagFind(BYTE* actor,unsigned getter);
struct ChaserCoreLease {
    TwinKartCamera::ObjectIdentity world{},pawn{},state{},replication{};
    int slot=-1;
};
inline bool CurrentChaserCore(const ChaserCoreLease& lease) {
    if(lease.slot<1 || lease.slot>=16 || !InChaserItemWorld(static_cast<BYTE*>(lease.pawn.object)))return false;
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
    int count=*reinterpret_cast<int*>(image+0x69c28c4);
    if(!TwinKartCamera::Live(lease.world,chunks,count) || !TwinKartCamera::Live(lease.pawn,chunks,count) ||
       !TwinKartCamera::Live(lease.state,chunks,count) || !TwinKartCamera::Live(lease.replication,chunks,count))return false;
    auto pawn=static_cast<BYTE*>(lease.pawn.object),state=static_cast<BYTE*>(lease.state.object);
    auto replication=static_cast<BYTE*>(lease.replication.object);
    using Type=void*(__fastcall*)();using GetWorld=BYTE*(__fastcall*)(void*);
    return *reinterpret_cast<void**>(pawn+0x10)==reinterpret_cast<Type>(image+0x2413310)() &&
        reinterpret_cast<GetWorld>((*reinterpret_cast<void***>(pawn))[0x160/8])(pawn)==lease.world.object &&
        OwnedComponent(state,pawn,reinterpret_cast<Type>(image+0x23afeb0)()) &&
        OwnedComponent(replication,pawn,reinterpret_cast<Type>(image+0x22ff4a0)()) &&
        FlagFind(pawn,0x22ff4a0)==replication && World(replication)==lease.world.object &&
        FlagFind(pawn,0x23afeb0)==state && World(state)==lease.world.object &&
        *reinterpret_cast<int*>(state+0x168)==lease.slot && TwinKartObjective::ChaserCoreConnection(state[0x338]);
}
inline bool ChaserCoreNotReady(int slot,int stage) {
    static ULONGLONG next[16]{};auto now=GetTickCount64();
    if(slot>0 && slot<16 && now>=next[slot]){next[slot]=now+3000;Emit("native_chaser_core_not_ready",slot,stage);}
    return false;
}
inline bool PrepareChaserAiCore(BYTE* pawn,int slot) {
    __try {
        if(slot<1 || slot>=16 || !InChaserItemWorld(pawn))return false;
        using Type=void*(__fastcall*)();using GetWorld=BYTE*(__fastcall*)(void*);
        if(*reinterpret_cast<void**>(pawn+0x10)!=reinterpret_cast<Type>(image+0x2413310)())return false;
        ChaserCoreLease lease;lease.slot=slot;
        auto world=reinterpret_cast<GetWorld>((*reinterpret_cast<void***>(pawn))[0x160/8])(pawn);
        auto state=FlagFind(pawn,0x23afeb0);
        // Original KartMovement.InitializeComponent immediately consumes the
        // constructor-provided KartMovementReplication; never construct a
        // movement against a missing/pending/wrong-owner replication object.
        auto replication=FlagFind(pawn,0x22ff4a0);
        if(!OwnedComponent(state,pawn,reinterpret_cast<Type>(image+0x23afeb0)()))return ChaserCoreNotReady(slot,1);
        if(!OwnedComponent(replication,pawn,reinterpret_cast<Type>(image+0x22ff4a0)()))return ChaserCoreNotReady(slot,2);
        if(!CaptureFlagObject(world,lease.world) || !CaptureFlagObject(pawn,lease.pawn) ||
           !CaptureFlagObject(state,lease.state) || !CaptureFlagObject(replication,lease.replication))return ChaserCoreNotReady(slot,3);
        if(!CurrentChaserCore(lease)) {
            Emit("native_chaser_core_observed_slot",slot,*reinterpret_cast<int*>(state+0x168));
            Emit("native_chaser_core_observed_connection",slot,state[0x338]);
            return ChaserCoreNotReady(slot,4);
        }
        // Exact reflected native AI-state setter; never write an actor class,
        // interface-mode byte, team, connection or role. The actual original
        // SpeedSingleAiPawn has no RaceAiPawn's extra +0x328 mode storage.
        if(!state[0x321])reinterpret_cast<void(__fastcall*)(void*)>(image+0x23b02c0)(state);
        if(!CurrentChaserCore(lease) || !state[0x321])return ChaserCoreNotReady(slot,5);
        // Original SpeedSingleAiPawn's dedicated registration supplies
        // KartMovement and KartEvent; its client registration supplies
        // KdAnimation. Use only the same generic NewObject/RegisterComponent
        // operations for a missing component, after original client setup.
        // FollowTG is a circuit controller, and SimulationMovement is remote
        // smoothing: neither is needed to supply this arena driver's inputs.
        const unsigned getters[]{0x22f9030,0x22f6900,0x23077d0};
        TwinKartCamera::ObjectIdentity identities[3]{};
        for(unsigned i=0;i<3;++i) {
            if(!CurrentChaserCore(lease))return ChaserCoreNotReady(slot,6+static_cast<int>(i));
            auto component=EnsureFlagComponent(pawn,getters[i]);
            if(!CurrentChaserCore(lease) || !OwnedComponent(component,pawn,reinterpret_cast<Type>(image+getters[i])()) ||
               World(component)!=world || !CaptureFlagObject(component,identities[i]))return ChaserCoreNotReady(slot,10+static_cast<int>(i));
        }
        // Registration may execute native callbacks. Revalidate the entire
        // set, not just the last constructed object, before publishing ready.
        auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
        int count=*reinterpret_cast<int*>(image+0x69c28c4);
        for(unsigned i=0;i<3;++i) {
            auto component=static_cast<BYTE*>(identities[i].object);
            if(!TwinKartCamera::Live(identities[i],chunks,count) || FlagFind(pawn,getters[i])!=component ||
               !OwnedComponent(component,pawn,reinterpret_cast<Type>(image+getters[i])()) || World(component)!=world)return false;
        }
        // This is component readiness only. Native param/enable/body/role
        // readiness remains required by the driver; no readiness byte is set.
        return CurrentChaserCore(lease) && state[0x321]!=0;
    } __except(EXCEPTION_EXECUTE_HANDLER){Emit("native_chaser_core_preparation_rejected",slot,0);return false;}
}
inline void PrepareChaserClientPawn(BYTE* rule,BYTE* pawn,int slot) {
    __try {
        if(slot<1 || slot>=16 || !InChaserItemWorld(pawn) || !Live(rule) ||
           *reinterpret_cast<BYTE**>(rule)!=image+0x51dd410)return;
        using GetWorld=BYTE*(__fastcall*)(void*);
        auto world=reinterpret_cast<GetWorld>((*reinterpret_cast<void***>(pawn))[0x160/8])(pawn);
        if(World(rule)!=world || *reinterpret_cast<void**>(rule+0xa0)!=*reinterpret_cast<void**>(world+0x118) ||
           *reinterpret_cast<int*>(rule+0x130)!=1 || *reinterpret_cast<int*>(rule+0x138)<1 ||
           *reinterpret_cast<int*>(rule+0x138)>15 || slot>*reinterpret_cast<int*>(rule+0x138))return;
        Emit("native_chaser_post_client_core_ready",slot,PrepareChaserAiCore(pawn,slot)?1:0);
    } __except(EXCEPTION_EXECUTE_HANDLER){Emit("native_chaser_post_client_core_rejected",slot,0);}
}
inline bool PrepareChaserMovementMode(BYTE* pawn,BYTE* movement,int slot) {
    __try {
        if(slot<1 || slot>=16 || !InChaserItemWorld(pawn))return false;
        using Type=void*(__fastcall*)();using ActorWorld=BYTE*(__fastcall*)(void*);
        if(*reinterpret_cast<void**>(pawn+0x10)!=reinterpret_cast<Type>(image+0x2413310)())return false;
        auto state=FlagFind(pawn,0x23afeb0),replication=FlagFind(pawn,0x22ff4a0);
        auto world=reinterpret_cast<ActorWorld>((*reinterpret_cast<void***>(pawn))[0x160/8])(pawn);
        if(!OwnedComponent(state,pawn,reinterpret_cast<Type>(image+0x23afeb0)()) || !state[0x321] || state[0x338]!=1 ||
           !OwnedComponent(replication,pawn,reinterpret_cast<Type>(image+0x22ff4a0)()) ||
           !OwnedComponent(movement,pawn,reinterpret_cast<Type>(image+0x22f9030)()) || World(movement)!=world)return false;
        ChaserCoreLease lease;lease.slot=slot;TwinKartCamera::ObjectIdentity movementId{};
        if(!CaptureFlagObject(world,lease.world) || !CaptureFlagObject(pawn,lease.pawn) || !CaptureFlagObject(state,lease.state) ||
           !CaptureFlagObject(replication,lease.replication) || !CaptureFlagObject(movement,movementId) || !CurrentChaserCore(lease))return false;
        // +0xed8 is the original ITEM-MODE flag, not parameter/body readiness.
        // ChaserPawn's actual local registration calls ItemModeSettingParam
        // after registering its KartMovement (0x18be104). The original speed
        // AI's registration omits it. Supply that same native mode operation
        // for this owned Chaser bot, never write a readiness byte or invoke
        // another pawn class's registration interface.
        if(!movement[0xed8])reinterpret_cast<void(__fastcall*)(void*)>(image+0x15c3790)(movement);
        return CurrentChaserCore(lease) && state[0x321] && state[0x338]==1 &&
            TwinKartCamera::Live(movementId,*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),
                *reinterpret_cast<int*>(image+0x69c28c4)) && FlagFind(pawn,0x22f9030)==movement &&
            OwnedComponent(movement,pawn,reinterpret_cast<Type>(image+0x22f9030)()) && World(movement)==world && movement[0xed8];
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
inline bool PauseFactoryRemoteTick(BYTE* pawn,int slot,bool requireComponent) {
    if(slot<1 || slot>=TwinKartFactory::PopulationCount || !FactoryActorWorld(pawn))return false;
    using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
    auto pawnType=reinterpret_cast<Type>(image+(slot<TwinKartFactory::RacerCount?0x2431240:0x2433330))();
    if(*reinterpret_cast<void**>(pawn+0x10)!=pawnType)return false;
    auto type=reinterpret_cast<Type>(image+0x2402870)();
    auto simulation=requireComponent?EnsureFlagComponent(pawn,0x2402870):
        reinterpret_cast<Find>((*reinterpret_cast<void***>(pawn))[0x620/8])(pawn,type);
    if(!simulation)return !requireComponent;
    if(!OwnedComponent(simulation,pawn,type))return false;
    auto table=*reinterpret_cast<BYTE***>(simulation);
    if(table[0x358/8]!=image+0x3de6fb0 || table[0x368/8]!=image+0x3de02a0)return false;
    using Enabled=bool(__fastcall*)(void*);using SetTick=void(__fastcall*)(void*,bool);
    auto enabled=reinterpret_cast<Enabled>(image+0x3de02a0);
    if(enabled(simulation))reinterpret_cast<SetTick>(image+0x3de6fb0)(simulation,false);
    if(enabled(simulation))return false;
    Emit(slot<TwinKartFactory::RacerCount?"native_factory_cooperative_remote_tick_paused":"native_factory_android_remote_tick_paused",slot,1);
    return true;
}
inline bool PrepareFactoryCooperativeDriver(BYTE* pawn,int slot) {
    __try {
        if(slot<1 || slot>=TwinKartFactory::RacerCount || !FactoryActorWorld(pawn))return false;
        using Type=void*(__fastcall*)();
        if(*reinterpret_cast<void**>(pawn+0x10)!=reinterpret_cast<Type>(image+0x2431240)())return false;
        // Use the original generic component construction/registration APIs,
        // retaining TobyRushPawn and its racer team/retirement semantics.
        // Never call a different pawn class's registration interface on it.
        auto path=EnsureFlagComponent(pawn,0x22b1e10);
        auto movement=EnsureFlagComponent(pawn,0x22f9030);
        auto animation=EnsureFlagComponent(pawn,0x23077d0);
        // Local cooperative racers also need native item decisions. The
        // remote-human pawn never creates UAIItemTeamComponent itself. Its
        // original BeginPlay discovers UItemComponent through the lazy
        // registry and binds native gain/hit delegates. Do not call another
        // pawn's server interface or synthesize item gains/uses.
        auto items=EnsureFlagComponent(pawn,0x22174e0);
        if(!path || !movement || !animation || !items)return false;
        Emit("native_factory_cooperative_item_ai_present",slot,1);
        auto current=*reinterpret_cast<BYTE**>(movement+0xf08);
        if(Live(current) && current!=path)return false;
        // Original Factory item-mode parameters, followed by its FollowTG
        // binding. Native 0x15c3790 is ItemModeSettingParam, not an AI/physics
        // enable method. No actor-class, player flag or team is rewritten.
        using ConfigureItem=void(__fastcall*)(void*);using Bind=void(__fastcall*)(void*,void*);
        if(!movement[0xed8])reinterpret_cast<ConfigureItem>(image+0x15c3790)(movement);
        reinterpret_cast<Bind>(image+0x15d1100)(movement,path);
        // This pawn originally represents a remote human. Its native network
        // smoothing component must not also write the local AI's rigid body.
        // Keep the component and collision/physics settings; pause only its
        // tick through the original synchronous ActorComponent API. The
        // reflected SetComponentTickEnabled/IsComponentTickEnabled wrappers
        // independently establish these two slots/targets for this build.
        if(!PauseFactoryRemoteTick(pawn,slot,false))return false;
        bool ready=movement[0xed8] && *reinterpret_cast<void**>(movement+0xf08)==path;
        Emit("native_factory_cooperative_driver_bound",slot,ready?1:0);return ready;
    } __except(EXCEPTION_EXECUTE_HANDLER){Emit("native_factory_cooperative_driver_fault",slot,0);return false;}
}
inline bool PrepareFactoryRobotDriver(BYTE* pawn,int slot) {
    __try {
        if(slot<TwinKartFactory::RacerCount || slot>=TwinKartFactory::PopulationCount || !FactoryActorWorld(pawn))return false;
        using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
        if(*reinterpret_cast<void**>(pawn+0x10)!=reinterpret_cast<Type>(image+0x2433330)())return false;
        auto find=reinterpret_cast<Find>((*reinterpret_cast<void***>(pawn))[0x620/8]);
        const unsigned getters[]={0x22b1e10,0x22f9030,0x23077d0};BYTE* components[3]{};void* types[3]{};bool missing=false;
        for(unsigned i=0;i<3;++i) {
            types[i]=reinterpret_cast<Type>(image+getters[i])();components[i]=find(pawn,types[i]);
            if(!OwnedComponent(components[i],pawn,types[i]))missing=true;
        }
        if(missing) {
            int previous=factorySpawnSlot;factorySpawnSlot=slot;
            // Exact TobyRushAiPawn dedicated registration, after native client
            // preparation has finished. Its own three construction calls reuse
            // any surviving components through OnFactoryConstruct.
            __try {reinterpret_cast<void(__fastcall*)(void*)>(image+0x19ea8a0)(pawn+0x320);}
            __finally {factorySpawnSlot=previous;}
            for(unsigned i=0;i<3;++i)components[i]=find(pawn,types[i]);
        }
        for(unsigned i=0;i<3;++i)if(!OwnedComponent(components[i],pawn,types[i]))return false;
        // Native TobyRushAiPawn::Others supplies these on a network client.
        // Standalone authority previously had only its dedicated components,
        // so original retirement could not find the simulation component for
        // SetKartScale. Keep that original scale/event path, while preventing
        // its network smoothing tick from competing with authoritative AI.
        if(!EnsureFlagComponent(pawn,0x22f6900) || !PauseFactoryRemoteTick(pawn,slot,true))return false;
        auto movement=components[1],path=components[0];auto current=*reinterpret_cast<BYTE**>(movement+0xf08);
        if(Live(current) && current!=path)return false;
        if(!movement[0xed8])reinterpret_cast<void(__fastcall*)(void*)>(image+0x15c3790)(movement);
        reinterpret_cast<void(__fastcall*)(void*,void*)>(image+0x15d1100)(movement,path);
        bool ready=movement[0xed8] && *reinterpret_cast<void**>(movement+0xf08)==path;
        Emit("native_factory_android_driver_bound",slot,ready?1:0);return ready;
    } __except(EXCEPTION_EXECUTE_HANDLER){Emit("native_factory_android_driver_fault",slot,0);return false;}
}
inline void PrepareFactoryPopulation(BYTE* phase,int stage) {
    struct Pawns {BYTE** data;int count,capacity;} pawns{};
    __try {
        if(!factoryCompatEnabled || GetCurrentThreadId()!=owner || !Live(phase))return;
        using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
        auto world=World(phase);
        if(*reinterpret_cast<void**>(phase+0x10)!=reinterpret_cast<Type>(image+0x2431330)() ||
           !factoryWorld.Matches(world,*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),*reinterpret_cast<int*>(image+0x69c28c4)))return;
        __try {
            reinterpret_cast<void(__fastcall*)(void*,Pawns*)>(image+0x1971550)(phase,&pawns);
            if(!pawns.data || pawns.count!=TwinKartFactory::PopulationCount || pawns.capacity<pawns.count || pawns.capacity>4096) {
                Emit("native_factory_pre_phase_population_rejected",stage,pawns.count);return;
            }
            auto stateType=reinterpret_cast<Type>(image+0x23afeb0)();std::uint64_t observed=0,ready=0;
            for(int i=0;i<pawns.count;++i) {
                auto pawn=pawns.data[i];if(!FactoryActorWorld(pawn))continue;
                auto state=reinterpret_cast<Find>((*reinterpret_cast<void***>(pawn))[0x620/8])(pawn,stateType);
                if(!OwnedComponent(state,pawn,stateType))continue;
                int slot=*reinterpret_cast<int*>(state+0x168);
                if(slot<0 || slot>=TwinKartFactory::PopulationCount || (observed&(std::uint64_t{1}<<slot)))continue;
                observed|=std::uint64_t{1}<<slot;
                if(slot && !PrepareFactoryControlledPawn(pawn,slot))continue;
                bool prepared=slot==0?*reinterpret_cast<void**>(pawn+0x10)==reinterpret_cast<Type>(image+0x2431240)():
                    slot<TwinKartFactory::RacerCount?PrepareFactoryCooperativeDriver(pawn,slot):PrepareFactoryRobotDriver(pawn,slot);
                if(prepared)ready|=std::uint64_t{1}<<slot;
            }
            Emit("native_factory_pre_phase_drivers_present",stage,observed==TwinKartFactory::AllSlots && ready==TwinKartFactory::AllSlots?1:0);
        } __finally {if(pawns.data)reinterpret_cast<void(__fastcall*)(void*)>(image+0x248b950)(pawns.data);}
    } __except(EXCEPTION_EXECUTE_HANDLER){Emit("native_factory_pre_phase_driver_fault",stage,0);}
}
inline void __fastcall OnFactoryManagerBegin(void* raw) {
    __try {
        if(factoryCompatEnabled && GetCurrentThreadId()==owner && Live(static_cast<BYTE*>(raw))) {
            auto phase=*reinterpret_cast<BYTE**>(static_cast<BYTE*>(raw)+0x28);
            using Type=void*(__fastcall*)();
            if(Live(phase) && *reinterpret_cast<void**>(phase+0x10)==reinterpret_cast<Type>(image+0x2431330)()) {
                auto state=*reinterpret_cast<BYTE**>(phase+0xa0);
                auto world=World(phase);
                if(world && *reinterpret_cast<void**>(world+0x120)==state && FactoryActorWorld(state) && *reinterpret_cast<void**>(state+0x10)==reinterpret_cast<Type>(image+0x2432080)() &&
                   factoryWorld.Matches(World(phase),*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),*reinterpret_cast<int*>(image+0x69c28c4))) {
                    // Native InitTobyRushAIManager returns without any path or
                    // scenario initialization when standalone omitted this
                    // dedicated component. Supply the exact original class
                    // before its original rule executes, and let that rule
                    // select the real scenario table and current-track path.
                    PrepareFactoryPopulation(phase,1);
                    auto manager=EnsureFlagComponent(state,0x2216dd0);
                    Emit("native_factory_original_ai_manager_present",-1,manager?1:0);
                }
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER){Emit("native_factory_ai_manager_preparation_fault",-1,0);}
    factoryManagerBegin(raw);
}
inline void __fastcall OnFactoryPodium(void* raw,void* sequence,const void* environment,void* widget) {
    bool owned=false;
    __try {
        auto component=static_cast<BYTE*>(raw);
        if(factoryCompatEnabled && GetCurrentThreadId()==owner && Live(component)) {
            auto world=World(component);using Type=void*(__fastcall*)();
            auto mode=world?*reinterpret_cast<BYTE**>(world+0x118):nullptr;
            auto state=world?*reinterpret_cast<BYTE**>(world+0x120):nullptr;
            owned=factoryWorld.Matches(world,*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),
                *reinterpret_cast<int*>(image+0x69c28c4)) && FactoryActorWorld(mode) &&
                *reinterpret_cast<void**>(mode+0x10)==reinterpret_cast<Type>(image+0x24345d0)() && FactoryActorWorld(state) &&
                *reinterpret_cast<void**>(state+0x10)==reinterpret_cast<Type>(image+0x2432080)() &&
                OwnedComponent(component,state,reinterpret_cast<Type>(image+0x2294250)());
        }
    } __except(EXCEPTION_EXECUTE_HANDLER){owned=false;}
    if(owned) {
        Emit("native_factory_lose_sequence_loaded",-1,sequence?1:0);
        if(!sequence) {
            // Leave native result/rule progression untouched. This suppresses
            // only an unavailable optional cinematic, not a loss or its UI.
            Emit("native_factory_missing_lose_cinematic_skipped",-1,1);return;
        }
    }
    factoryCustomPodium(raw,sequence,environment,widget);
}
inline BYTE* FactoryIntroPhase(BYTE* rule) {
    if(!factoryCompatEnabled || GetCurrentThreadId()!=owner || !Live(rule))return nullptr;
    using Type=void*(__fastcall*)();
    if(*reinterpret_cast<void**>(rule+0x10)!=reinterpret_cast<Type>(image+0x2432c70)())return nullptr;
    auto phase=*reinterpret_cast<BYTE**>(rule+0x28);
    if(!Live(phase) || *reinterpret_cast<void**>(phase+0x10)!=reinterpret_cast<Type>(image+0x2431330)())return nullptr;
    auto world=World(phase),state=*reinterpret_cast<BYTE**>(phase+0xa0);
    if(!world || *reinterpret_cast<void**>(world+0x120)!=state || !FactoryActorWorld(state) ||
       *reinterpret_cast<void**>(state+0x10)!=reinterpret_cast<Type>(image+0x2432080)() ||
       !factoryWorld.Matches(world,*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),*reinterpret_cast<int*>(image+0x69c28c4)))return nullptr;
    return phase;
}
inline bool RestoreFactoryIntroRacers(BYTE* phase) {
    struct Pawns {BYTE** data;int count,capacity;} pawns{};
    bool restored=false;
    __try {
        __try {
            reinterpret_cast<void(__fastcall*)(void*,Pawns*)>(image+0x1971550)(phase,&pawns);
            if(!pawns.data || pawns.count!=TwinKartFactory::PopulationCount || pawns.capacity<pawns.count || pawns.capacity>4096)return false;
            using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
            auto type=reinterpret_cast<Type>(image+0x2431240)(),stateType=reinterpret_cast<Type>(image+0x23afeb0)();
            BYTE* racers[TwinKartFactory::RacerCount]{};unsigned seen=0;
            for(int i=0;i<pawns.count;++i) {
                auto pawn=pawns.data[i];if(!FactoryActorWorld(pawn))return false;
                if(*reinterpret_cast<void**>(pawn+0x10)!=type)continue;
                auto state=reinterpret_cast<Find>((*reinterpret_cast<void***>(pawn))[0x620/8])(pawn,stateType);
                if(!OwnedComponent(state,pawn,stateType))return false;
                int slot=*reinterpret_cast<int*>(state+0x168);
                if(slot<0 || slot>=TwinKartFactory::RacerCount || (seen&(1u<<slot)) ||
                   (*reinterpret_cast<BYTE***>(pawn))[0x308/8]!=image+0x3cc28b0)return false;
                racers[slot]=pawn;seen|=1u<<slot;
            }
            if(seen!=TwinKartFactory::RacerSlots)return false;
            // Intro's own original loop hid exactly these four racer actors.
            // A missing movie cannot run its show tracks. Restore once at
            // this same rule boundary, through the original visibility API;
            // never poll/unhide during retirement, podium or ordinary races.
            for(int slot=0;slot<TwinKartFactory::RacerCount;++slot) {
                reinterpret_cast<void(__fastcall*)(void*,bool)>(image+0x3cc28b0)(racers[slot],false);
                Emit("native_factory_missing_intro_racer_restored",slot,(racers[slot][0x58]&0x20)?0:1);
            }
            restored=true;
        } __finally {if(pawns.data)reinterpret_cast<void(__fastcall*)(void*)>(image+0x248b950)(pawns.data);}
    } __except(EXCEPTION_EXECUTE_HANDLER){Emit("native_factory_intro_visibility_fault",-1,0);}
    return restored;
}
inline void __fastcall OnFactoryIntroSequence(void* raw,void* sequence,void* pawn) {
    if(factoryIntroScope && FactoryIntroPhase(factoryIntroScope)) {
        auto phase=*reinterpret_cast<BYTE**>(factoryIntroScope+0x28),state=*reinterpret_cast<BYTE**>(phase+0xa0);
        using Type=void*(__fastcall*)();
        if(OwnedComponent(static_cast<BYTE*>(raw),state,reinterpret_cast<Type>(image+0x2385f60)()) && FactoryActorWorld(static_cast<BYTE*>(pawn))) {
            factoryIntroPlaying=sequence!=nullptr;Emit("native_factory_intro_sequence_loaded",-1,sequence?1:0);
            if(!sequence)return;
        }
    }
    factoryIntroSequence(raw,sequence,pawn);
}
inline void __fastcall OnFactoryIntroBegin(void* raw) {
    BYTE* phase=nullptr;
    __try {phase=FactoryIntroPhase(static_cast<BYTE*>(raw));} __except(EXCEPTION_EXECUTE_HANDLER){phase=nullptr;}
    if(!phase){factoryIntroBegin(raw);return;}
    auto previous=factoryIntroScope;bool previousPlaying=factoryIntroPlaying;
    factoryIntroScope=static_cast<BYTE*>(raw);factoryIntroPlaying=false;
    __try {
        factoryIntroBegin(raw);
        if(!factoryIntroPlaying)RestoreFactoryIntroRacers(phase);
    } __finally {factoryIntroScope=previous;factoryIntroPlaying=previousPlaying;}
}
inline bool ForceFactoryStartWhenReady(BYTE* rule) {
    __try {
        if(!factoryCompatEnabled || !factoryRosterReady || GetCurrentThreadId()!=owner || !Live(rule) ||
           *reinterpret_cast<void**>(rule)!=image+0x51dd410 || *reinterpret_cast<int*>(rule+0x130)!=4 ||
           *reinterpret_cast<int*>(rule+0x138)!=36 || factoryCooperativeMask!=14)return false;
        auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);int count=*reinterpret_cast<int*>(image+0x69c28c4);
        auto world=World(rule);
        if(!factoryWorld.Matches(world,chunks,count) || !factoryPopulation.NativePopulationPresent(chunks,count))return false;
        auto mode=*reinterpret_cast<BYTE**>(world+0x118);using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
        if(!FactoryActorWorld(mode) || *reinterpret_cast<void**>(mode+0x10)!=reinterpret_cast<Type>(image+0x24345d0)())return false;
        auto type=reinterpret_cast<Type>(image+0x23b4a90)();
        auto wait=reinterpret_cast<Find>((*reinterpret_cast<void***>(mode))[0x620/8])(mode,type);
        if(!OwnedComponent(wait,mode,type))return false;
        if(factoryStartRequested)return true;
        if(!wait[0xc8] && !wait[0xc9])reinterpret_cast<void(__fastcall*)(void*)>(image+0x1638220)(wait);
        factoryStartRequested=wait[0xc8] || wait[0xc9];
        Emit("native_factory_original_player_wait_requested",-1,factoryStartRequested?1:0);return factoryStartRequested;
    } __except(EXCEPTION_EXECUTE_HANDLER){Emit("native_factory_player_wait_fault",-1,0);return false;}
}
inline volatile LONG factoryHudPacket=0;
inline TwinKartCamera::ObjectIdentity factoryHumanPawn{};
inline ULONGLONG factoryHumanLookupAfter=0;
inline float factoryRaceClockStart=-1.f;
inline void ObserveFactoryHuman(BYTE* world,BYTE* phase,BYTE* current);
inline void ObserveFactoryPhase(BYTE* world) {
    __try {
        if(GetCurrentThreadId()!=owner)return;
        if(!factoryCompatEnabled || !Live(world) ||
           !factoryWorld.Matches(world,*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),*reinterpret_cast<int*>(image+0x69c28c4))){
            factoryHumanPawn={};factoryHumanLookupAfter=0;factoryRaceClockStart=-1.f;
            InterlockedExchange(&factoryHudPacket,0);return;
        }
        using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);using Name=void(__fastcall*)(void*,const wchar_t*,int);
        auto state=*reinterpret_cast<BYTE**>(world+0x120),mode=*reinterpret_cast<BYTE**>(world+0x118);
        if(FactoryActorWorld(mode) && !factoryStartRequested) {
            auto ruleType=reinterpret_cast<Type>(image+0x2381ed0)();
            auto rule=reinterpret_cast<Find>((*reinterpret_cast<void***>(mode))[0x620/8])(mode,ruleType);
            if(OwnedComponent(rule,mode,ruleType))ForceFactoryStartWhenReady(rule);
        }
        if(!FactoryActorWorld(state))return;
        auto type=reinterpret_cast<Type>(image+0x2431330)();
        auto phase=reinterpret_cast<Find>((*reinterpret_cast<void***>(state))[0x620/8])(state,type);
        if(!OwnedComponent(phase,state,type))return;auto current=*reinterpret_cast<BYTE**>(phase+0x110);if(!Live(current))return;
        ObserveFactoryHuman(world,phase,current);
        auto actual=*reinterpret_cast<unsigned long long*>(current+0x28);if(actual==factoryLastPhase)return;
        struct PhaseLabel {const wchar_t* name;const char* event;};
        const PhaseLabel labels[]={{L"InitGame","native_factory_phase_InitGame"},{L"StandByGame","native_factory_phase_StandByGame"},
            {L"StartCountDown","native_factory_phase_StartCountDown"},{L"BeginRace","native_factory_phase_BeginRace"},
            {L"RetireCountdown","native_factory_phase_RetireCountdown"},{L"Podium","native_factory_phase_Podium"},
            {L"Result","native_factory_phase_Result"},{L"EndGame","native_factory_phase_EndGame"}};
        factoryLastPhase=actual;
        for(const auto& label:labels){unsigned long long name=0;reinterpret_cast<Name>(image+0x2589130)(&name,label.name,0);
            if(name && name==actual){Emit(label.event,-1,1);return;}}
        Emit("native_factory_phase_unmapped",-1,0);
    } __except(EXCEPTION_EXECUTE_HANDLER){Emit("native_factory_phase_observer_fault",-1,0);}
}
inline bool PrepareFlagInteractions(BYTE* pawn,int slot) {
    __try {
        if(!PrepareFlagAi(pawn,slot))return false;
        // The stock SpeedSingleAiPawn has movement but lacks FlagPawn's
        // interaction components. Use original native components for overlaps,
        // pickup, steal, hit/drop, carried-flag visuals and scoring delegates.
        // The original client-default registration creates FlagDropper later
        // in the pawn lifecycle. Do not eagerly create a duplicate here.
        const unsigned getters[]={0x23a1840,0x22ac760,0x22ac830,0x22a6400};
        unsigned ready=0;
        for(unsigned i=0;i<4;++i)
            if(EnsureFlagComponent(pawn,getters[i],i==1))ready|=1u<<i;
        Emit("native_local_flag_ai_interaction_components",slot,ready);
        // Flag uses SpeedSingleAiPawn, whose original local registration
        // does not create AISkillComponent. Install the original component
        // before SetupSkill visits the pawn; its own registration, native
        // skill definitions and randomized timer remain authoritative.
        bool skillReady=!flagSkillsEnabled || EnsureFlagComponent(pawn,0x2217c80)!=nullptr;
        if(flagSkillsEnabled)Emit("native_local_flag_ai_skill_component_ready",slot,skillReady?1:0);
        return ready==15 && skillReady;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        Emit("native_local_flag_ai_interaction_fault",slot,0);return false;
    }
}
inline void __fastcall OnFlagMovementBegin(void* raw) {
    auto movement=static_cast<BYTE*>(raw);BYTE* previous=flagPhysicsScope;
    __try {
        if(Live(movement) && StandaloneFlagWorld(World(movement))) {
            BYTE* actor=*reinterpret_cast<BYTE**>(movement+0xa0);
            // The native authority overlap registry requires MagneticMovement
            // (+0x168), while the native local visual/drop physics requires
            // ProjectileMovement (+0x170). Preserve both dependencies before
            // original config callbacks publish the newly spawned flag.
            auto magnetic=EnsureFlagComponent(actor,0x237cd80);
            if(magnetic){*reinterpret_cast<BYTE**>(movement+0x168)=magnetic;flagPhysicsScope=movement;}
        }
        flagMovementBegin(raw);
    } __finally {flagPhysicsScope=previous;}
}
inline bool ForceLocalStartWhenReady(BYTE* rule,int slot,bool prepared) {
    __try {
        if(!prepared || !TwinKartRace::PlayableMode(armedMode) || slot<1 || slot>7 || !Local(rule))return false;
        int ais=*reinterpret_cast<int*>(rule+0x138),players=*reinterpret_cast<int*>(rule+0x130);
        if(players!=1 || !TwinKartRace::PopulationValid(armedMode,ais) || slot>ais)return false;
        localAiPreparedMask|=1u<<slot;
        unsigned expected=(1u<<(ais+1))-2;
        if((localAiPreparedMask&expected)!=expected || localStartRequested)return localStartRequested;
        auto world=World(rule);
        // PlayerWait and MajorConnect belong to AuthorityGameMode (+0x118),
        // while FlagModeState belongs to GameState (+0x120).
        BYTE* state=world?*reinterpret_cast<BYTE**>(world+0x118):nullptr;
        using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
        auto waitType=reinterpret_cast<Type>(image+0x23b4a90)();
        if(!Live(state) || !waitType)return false;
        auto find=reinterpret_cast<Find>((*reinterpret_cast<void***>(state))[0x620/8]);
        BYTE* wait=find(state,waitType);
        if(!OwnedComponent(wait,state,waitType) || !InRaceWorld(wait))return false;
        if(*reinterpret_cast<BYTE*>(wait+0xc8)) {
            localStartRequested=true;
            Emit("native_local_player_wait_already_ready",-1,1);
            return true;
        }
        if(!*reinterpret_cast<BYTE*>(wait+0xc9)) {
            // Exact original UPlayerWaitComponent::ForceBeginGame. The method
            // only sets its native force flag and emits the game's own notice;
            // the regular Tick/CheckLoadedPlayerCount path remains solely
            // responsible for the readiness broadcast and phase transition.
            using Force=void(__fastcall*)(void*);
            reinterpret_cast<Force>(image+0x1638220)(wait);
        }
        localStartRequested=*reinterpret_cast<BYTE*>(wait+0xc8)!=0 || *reinterpret_cast<BYTE*>(wait+0xc9)!=0;
        Emit("native_local_player_wait_forced",-1,localStartRequested?1:0);
        return localStartRequested;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        Emit("native_local_player_wait_fault",slot,0);return false;
    }
}
inline bool RetryLocalStartWhenReady(BYTE* world) {
    __try {
        if(!armed || !TwinKartRace::PlayableMode(armedMode) || GetCurrentThreadId()!=owner || localStartRequested)return false;
        auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
        int count=*reinterpret_cast<int*>(image+0x69c28c4);
        if(!raceWorld.Matches(world,chunks,count))return false;
        BYTE* state=*reinterpret_cast<BYTE**>(world+0x118);
        if(!Live(state))return false;
        using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
        auto type=reinterpret_cast<Type>(image+0x2381ed0)();
        auto find=reinterpret_cast<Find>((*reinterpret_cast<void***>(state))[0x620/8]);
        BYTE* rule=find(state,type);
        if(!OwnedComponent(rule,state,type) || !Local(rule))return false;
        int ais=*reinterpret_cast<int*>(rule+0x138);
        if(!TwinKartRace::PopulationValid(armedMode,ais))return false;
        unsigned expected=(1u<<(ais+1))-2;
        return (localAiPreparedMask&expected)==expected && ForceLocalStartWhenReady(rule,ais,true);
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
struct FlagCommandMirror {
    TwinKartCamera::ObjectIdentity identity{};
    alignas(16) BYTE command[0x50]{};
};
inline FlagCommandMirror flagMirrors[32]{};
inline TwinKartRace::ArenaDriver flagDrivers[8]{};
inline TwinKartCamera::ObjectIdentity flagDriverPawns[8]{};
inline ULONGLONG flagDriverTime=0;
inline unsigned flagDriverSamples=0;
inline ULONGLONG flagDiagnosticNext=0;
inline bool flagDiagnostics=false;
inline unsigned long long flagLoggedPhase=~0ull;
inline int flagLoggedWinner=-1,flagLoggedPoints[3]{-1,-1,-1};
inline TwinKartCamera::ObjectIdentity flagHumanDriveOwner{};
inline ULONGLONG flagHumanDriveUntil=0,flagHumanDriveAwaited=0;
inline bool flagHumanDriveAttempted=false;
alignas(8) inline volatile LONG64 flagHudPacket=0;
inline TwinKartCamera::ObjectIdentity factoryHumanDriveOwner{};
inline ULONGLONG factoryHumanDriveUntil=0,factoryHumanDriveAwaited=0;
inline bool factoryHumanDriveAttempted=false;
inline void RecoverDriveBody(BYTE* movement,BYTE* pawn,BYTE* motionState,ULONGLONG now,
                            ULONGLONG deadline,ULONGLONG& awaited,bool& attempted,bool factory=false);
inline TwinKartRace::FlagGroundNavigation flagGround;
inline TwinKartRace::FlagGroundRoute flagRoutes[8];
inline bool flagGroundDecided=false;
inline float flagGroundTop=0,flagGroundBottom=0;
inline void BuildFlagGround(BYTE* world,const TwinKartRace::ArenaPoint* points,int count) {
    using Point=TwinKartRace::ArenaPoint;
    if(!flagGroundDecided && count>=4) {
        Point low=points[0],high=points[0];
        for(int i=1;i<count;++i){low.x=(std::min)(low.x,points[i].x);low.y=(std::min)(low.y,points[i].y);low.z=(std::min)(low.z,points[i].z);
            high.x=(std::max)(high.x,points[i].x);high.y=(std::max)(high.y,points[i].y);high.z=(std::max)(high.z,points[i].z);}
        flagGroundDecided=true;
        if(high.z-low.z>500) {
            flagGroundTop=high.z+3000;flagGroundBottom=low.z-3000;
            low.x-=6000;low.y-=6000;high.x+=6000;high.y+=6000;
            if(flagGround.Initialize(low,high))Emit("native_local_flag_road_sampling_started",static_cast<int>(flagGround.Count()),400);
        }
    }
    if(!flagGround.Started() || flagGround.Ready())return;
    // Same exact native road object-mask and immutable collision query used by
    // FlagMovement::FetchContactLocationOfNearestRoad. Query only; no scene or
    // vehicle transform changes. Spread the work across window-thread pumps.
    for(int i=0;i<128 && !flagGround.Ready();++i) {
        Point at{};if(!flagGround.Next(at))break;Point start{at.x,at.y,flagGroundTop},end{at.x,at.y,flagGroundBottom};
        Point contact{};bool road=TwinKartRace::QueryRoad(world,reinterpret_cast<TwinKartRace::RoadTrace>(image+0x3de0910),image+0x6b01810,start,end,contact);
        flagGround.Sample(contact.z,road);
    }
    if(flagGround.Ready()) {
        Emit("native_local_flag_road_sampling_ready",static_cast<int>(flagGround.ValidCount()),static_cast<int>(flagGround.Count()));
        if(flagDiagnostics)for(size_t i=0;i<flagGround.Count();++i) {
            Point point{};if(!flagGround.SampleAt(i,point))continue;
            Emit("native_local_flag_road_xy",static_cast<int>(point.x),static_cast<int>(point.y));
            Emit("native_local_flag_road_z",static_cast<int>(i),static_cast<int>(point.z));
        }
    }
}
inline void LogFlagPhase(BYTE* phase) {
    if(!Live(phase))return;
    auto value=*reinterpret_cast<unsigned long long*>(phase+0x28);if(value==flagLoggedPhase)return;
    flagLoggedPhase=value;unsigned index=static_cast<unsigned>(value),block=index>>16,offset=index&65535;
    if(block>=8192)return;
    auto entry=*reinterpret_cast<BYTE**>(image+0x6986150+block*8)+offset*2;
    unsigned header=*reinterpret_cast<unsigned short*>(entry),length=header>>6;
    if((header&1) || !length || length>80)return;
    char eventName[128]="native_local_flag_phase_";size_t prefix=strlen(eventName);
    for(unsigned i=0;i<length;++i){char ch=static_cast<char>(entry[2+i]);if(!((ch>='A'&&ch<='Z') || (ch>='a'&&ch<='z') || (ch>='0'&&ch<='9') || ch=='_'))return;eventName[prefix+i]=ch;}
    Emit(eventName,-1,1);
}
inline bool CaptureFlagObject(BYTE* object,TwinKartCamera::ObjectIdentity& identity) {
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
    int count=*reinterpret_cast<int*>(image+0x69c28c4);
    if(TwinKartCamera::Capture(object,chunks,count,&identity))return true;
    struct Weak {int index,serial;} weak{-1,0};using Assign=void(__fastcall*)(Weak*,void*);
    reinterpret_cast<Assign>(image+0x2796c20)(&weak,object);
    return TwinKartCamera::Capture(object,chunks,count,&identity) && identity.index==weak.index && identity.serial==weak.serial;
}
inline BYTE* FlagFind(BYTE* actor,unsigned getter) {
    if(!Live(actor))return nullptr;
    using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
    auto type=reinterpret_cast<Type>(image+getter)();
    auto find=reinterpret_cast<Find>((*reinterpret_cast<void***>(actor))[0x620/8]);
    auto value=find(actor,type);return OwnedComponent(value,actor,type)?value:nullptr;
}
inline bool __fastcall OnAiCanUseSkill(void* raw,void* optionalIndex) {
    auto component=static_cast<BYTE*>(raw);
    if(!flagSkillsEnabled || !TwinKartRace::FlagMode(armedMode) || !InRaceWorld(component))
        return aiSkillCanUse(raw,optionalIndex);
    // AISkillComponent's circuit override requires RaceProgressComponent and
    // RaceTimeComponent; Flag has neither. Use the original shared skill
    // readiness/cooldown contract during real Flag gameplay phases instead.
    // UseSkillIfCan still checks CanUseSkillPrerequisite and the skill's own
    // native targeting/cost rules. Other modes retain their original override.
    bool active=false;auto world=World(component);
    auto state=world?*reinterpret_cast<BYTE**>(world+0x120):nullptr;
    auto phase=FlagFind(state,0x22ac6a0);
    auto current=phase?*reinterpret_cast<BYTE**>(phase+0x110):nullptr;
    if(Live(current)) {
        using Name=void(__fastcall*)(void*,const wchar_t*,int);
        unsigned long long game=0,overtime=0,lastSpurt=0;
        auto name=reinterpret_cast<Name>(image+0x2589130);
        name(&game,L"Game",0);name(&overtime,L"Overtime",0);name(&lastSpurt,L"LastSpurt",0);
        auto actual=*reinterpret_cast<unsigned long long*>(current+0x28);
        active=(game && actual==game) || (overtime && actual==overtime) || (lastSpurt && actual==lastSpurt);
    }
    bool ready=false;
    if(active)ready=baseSkillCanUse(raw,optionalIndex);
    else if(optionalIndex)static_cast<BYTE*>(optionalIndex)[4]=0;
    if(eventDiagnosticsInstalled) {
        auto pawn=*reinterpret_cast<BYTE**>(component+0xa0);
        auto pawnState=FlagFind(pawn,0x23afeb0);
        int slot=pawnState?*reinterpret_cast<int*>(pawnState+0x168):-1;
        int flags=(active?1:0)|(ready?2:0);
        if(slot>=1 && slot<=7 && flagSkillReadinessObserved[slot]!=flags) {
            flagSkillReadinessObserved[slot]=flags;Emit("native_local_flag_ai_skill_readiness",slot,flags);
        }
    }
    return ready;
}
inline bool FlagPosition(BYTE* actor,TwinKartRace::ArenaPoint& point,float* heading=nullptr) {
    if(!Live(actor))return false;
    BYTE* root=*reinterpret_cast<BYTE**>(actor+0x130);if(!Live(root))return false;
    memcpy(&point,root+0x1e0,sizeof(point));if(!TwinKartRace::Finite(point))return false;
    if(heading) {
        const float* q=reinterpret_cast<const float*>(root+0x1d0);
        for(int i=0;i<4;++i)if(!std::isfinite(q[i]))return false;
        float norm=q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3];if(norm<0.9f || norm>1.1f)return false;
        *heading=std::atan2(2*(q[3]*q[2]+q[0]*q[1]),1-2*(q[1]*q[1]+q[2]*q[2]));
    }
    return true;
}
inline void MirrorFlagCommand(BYTE* movement) {
    if(!InRaceWorld(movement))return;
    int kind=*reinterpret_cast<int*>(movement+0x184);
    if(*reinterpret_cast<int*>(movement+0x180)<0 || kind<0 || kind>2)return;
    TwinKartCamera::ObjectIdentity identity{};if(!CaptureFlagObject(movement,identity))return;
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
    int count=*reinterpret_cast<int*>(image+0x69c28c4);
    FlagCommandMirror* slot=nullptr;
    for(auto& item:flagMirrors) {
        if(item.identity.object==movement && item.identity.index==identity.index && item.identity.serial==identity.serial){slot=&item;break;}
        if(!slot && !TwinKartCamera::Live(item.identity,chunks,count))slot=&item;
    }
    if(!slot)return;
    bool same=slot->identity.object==movement && slot->identity.serial==identity.serial;
    if(same && !memcmp(slot->command,movement+0x180,sizeof(slot->command)))return;
    alignas(16) BYTE previous[0x50]{};
    if(same)memcpy(previous,slot->command,sizeof(previous));
    else *reinterpret_cast<int*>(previous+4)=kind==2?0:2;
    // Standalone authority mutates FFlagCommandParam, but replication never
    // delivers OnRep to its local renderer. Invoke the exact original consumer
    // with the previous value, once per actual native command change.
    using Replicate=void(__fastcall*)(void*,const void*);
    reinterpret_cast<Replicate>(image+0x191dc50)(movement,previous);
    slot->identity=identity;memcpy(slot->command,movement+0x180,sizeof(slot->command));
    Emit("native_local_flag_command_mirrored",*reinterpret_cast<int*>(movement+0x180),kind);
}
inline bool FlagDrivingPhase(BYTE* current) {
    if(!Live(current))return false;
    using Name=void(__fastcall*)(void*,const wchar_t*,int);
    static unsigned long long names[3]{};
    if(!names[0]) {
        constexpr const wchar_t* labels[]{L"Game",L"Overtime",L"LastSpurt"};
        for(unsigned i=0;i<ARRAYSIZE(labels);++i)reinterpret_cast<Name>(image+0x2589130)(&names[i],labels[i],0);
    }
    const auto phase=*reinterpret_cast<unsigned long long*>(current+0x28);
    return phase && (phase==names[0] || phase==names[1] || phase==names[2]);
}
inline void UpdateFlagArena(BYTE* world,bool sceneActive) {
    LONG64 hudPacket=0;
    __try {
      __try {
        if(!armed || !TwinKartRace::FlagMode(armedMode) || GetCurrentThreadId()!=owner)return;
        auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
        int count=*reinterpret_cast<int*>(image+0x69c28c4);
        if(!raceWorld.Matches(world,chunks,count))return;
        BYTE* gameState=*reinterpret_cast<BYTE**>(world+0x120);
        BYTE* phase=FlagFind(gameState,0x22ac6a0);
        BYTE* current=phase?*reinterpret_cast<BYTE**>(phase+0x110):nullptr;
        LogFlagPhase(current);
        BYTE* flagState=FlagFind(gameState,0x22ad790);
        if(flagState && flagState[0x121]) {
            int winner=flagState[0x120];
            if(winner!=flagLoggedWinner){flagLoggedWinner=winner;Emit("native_local_flag_winner",-1,winner);}
        }
        bool driving=sceneActive && FlagDrivingPhase(current);
        ULONGLONG now=GetTickCount64();float dt=flagDriverTime?static_cast<float>(now-flagDriverTime)/1000.f:0.25f;
        flagDriverTime=now;if(dt<=0 || dt>1)dt=0.25f;
        bool logControls=now>=flagDiagnosticNext;if(logControls)flagDiagnosticNext=now+5000;
        using Type=void*(__fastcall*)();
        auto movementType=reinterpret_cast<Type>(image+0x22adc60)();
        auto zoneType=reinterpret_cast<Type>(image+0x22add30)();
        auto aiType=reinterpret_cast<Type>(image+0x2413310)();
        auto playerType=reinterpret_cast<Type>(image+0x22ae2e0)();
        BYTE* pawns[8]{};BYTE* flags[32]{};unsigned flagsCount=0;
        TwinKartRace::ArenaPoint zones[3]{};bool zoneReady[3]{};
        for(int index=0;index<count;++index) {
            auto chunk=chunks[index/65536];if(!chunk)continue;
            auto object=static_cast<BYTE*>(chunk[index%65536].object);
            if(!object || (chunk[index%65536].flags&0x30000000))continue;
            auto type=*reinterpret_cast<void**>(object+0x10);
            if(type!=movementType && type!=zoneType && type!=aiType && type!=playerType)continue;
            if(!Live(object) || (*reinterpret_cast<unsigned*>(object+8)&0x18030))continue;
            if(type==movementType && InRaceWorld(object)) {
                MirrorFlagCommand(object);
                BYTE* actor=*reinterpret_cast<BYTE**>(object+0xa0);
                BYTE* holder=FlagFind(actor,0x22ac760);
                if(holder && holder[0x110] && !holder[0x130] && flagsCount<32)flags[flagsCount++]=actor;
            } else if(type==zoneType) {
                using GetWorld=BYTE*(__fastcall*)(void*);
                if(reinterpret_cast<GetWorld>((*reinterpret_cast<void***>(object))[0x160/8])(object)!=world)continue;
                int team=*reinterpret_cast<int*>(object+0x220);
                if(team>=1 && team<=2)zoneReady[team]=FlagPosition(object,zones[team]);
            } else if(type==aiType || type==playerType) {
                BYTE* state=FlagFind(object,0x23afeb0);if(!state || !InRaceWorld(state))continue;
                int slot=*reinterpret_cast<int*>(state+0x168);
                if(slot>=0 && slot<=7 && (slot==0 || state[0x321]))pawns[slot]=object;
            }
        }
        using Input=void(__fastcall*)(void*,float);
        if(zoneReady[1] && zoneReady[2]) {
            TwinKartRace::ArenaPoint bounds[42]{};int used=0;bounds[used++]=zones[1];bounds[used++]=zones[2];
            for(unsigned i=0;i<flagsCount && used<42;++i){TwinKartRace::ArenaPoint at{};if(FlagPosition(flags[i],at))bounds[used++]=at;}
            for(int slot=0;slot<=7 && used<42;++slot){TwinKartRace::ArenaPoint at{};if(pawns[slot] && FlagPosition(pawns[slot],at))bounds[used++]=at;}
            BuildFlagGround(world,bounds,used);
        }
        unsigned claims[32]{};
        int totals[3]{};
        for(int slot=0;slot<=7;++slot)if(pawns[slot]) {
            BYTE* score=FlagFind(pawns[slot],0x23b3790);if(!score)continue;
            struct Contribution {int team,points;};
            struct Points {Contribution* data;int count,capacity;};auto points=*reinterpret_cast<Points*>(score+0x148);
            if(points.count<0 || points.count>8 || points.capacity<points.count || points.capacity>1024 || (points.count && !points.data))continue;
            for(int i=0;i<points.count;++i)if(points.data[i].team>=1 && points.data[i].team<=2 && points.data[i].points>=-10000 && points.data[i].points<=10000)
                totals[points.data[i].team]+=points.data[i].points;
        }
        for(int team=1;team<=2;++team)if(totals[team]!=flagLoggedPoints[team]){flagLoggedPoints[team]=totals[team];Emit("native_local_flag_team_points",team,totals[team]);}
        if(sceneActive && pawns[0] && totals[1]>=0 && totals[1]<=65535 && totals[2]>=0 && totals[2]<=65535) {
            auto player=pawns[0],state=FlagFind(player,0x23afeb0),hud=FlagFind(player,0x22c2670);
            auto root=Live(hud) && *reinterpret_cast<BYTE**>(hud)==image+0x51092c0?*reinterpret_cast<BYTE**>(hud+0x110):nullptr;
            const bool native=Live(root) && reinterpret_cast<bool(__fastcall*)(void*)>(image+0x3a73d20)(root);
            const int team=state && !state[0x321]?*reinterpret_cast<int*>(state+0x16c):0;
            const int winner=flagState && flagState[0x121]?flagState[0x120]:0;
            if(!native && (team==1 || team==2) && (driving || winner==1 || winner==2)) {
                auto holder=FlagFind(player,0x22ac760);
                const int hasFlag=holder && holder[0x110]?1:0;
                int itemCount=0;const int items=CubeItemCount(player,itemCount) && itemCount>=0 && itemCount<=2?itemCount:3;
                hudPacket=(winner==1 || winner==2?2LL:1LL)|(static_cast<LONG64>(team)<<2)|
                    (static_cast<LONG64>(hasFlag)<<4)|(static_cast<LONG64>(items)<<5)|
                    (static_cast<LONG64>(winner&3)<<7)|(static_cast<LONG64>(totals[1])<<9)|
                    (static_cast<LONG64>(totals[2])<<25);
            }
        }
        if(driving && pawns[0] && *reinterpret_cast<void**>(pawns[0]+0x10)==playerType) {
            auto pawn=pawns[0],state=FlagFind(pawn,0x23afeb0),movement=FlagFind(pawn,0x22f9030);
            auto controller=*reinterpret_cast<BYTE**>(pawn+0x258);
            if(Live(movement) && InRaceWorld(movement) && Live(controller) &&
               *reinterpret_cast<BYTE**>(controller)==image+0x50fa2c0 &&
               Live(*reinterpret_cast<BYTE**>(controller+0x298)) && *reinterpret_cast<void**>(controller+0x250)==pawn &&
               state && !state[0x321] && state[0x338]==1) {
                if(flagHumanDriveOwner.object!=movement || !TwinKartCamera::Live(flagHumanDriveOwner,chunks,count)) {
                    flagHumanDriveUntil=0;flagHumanDriveAttempted=false;flagHumanDriveAwaited=0;
                    if(CaptureFlagObject(movement,flagHumanDriveOwner))flagHumanDriveUntil=now+3500;
                }
                if(flagHumanDriveUntil)RecoverDriveBody(movement,pawn,*reinterpret_cast<BYTE**>(movement+0xf38),now,
                    flagHumanDriveUntil,flagHumanDriveAwaited,flagHumanDriveAttempted);
            }
        }
        for(int slot=1;slot<=7;++slot) {
            BYTE* pawn=pawns[slot];if(!pawn)continue;
            BYTE* movement=FlagFind(pawn,0x22f9030);if(!movement)continue;
            auto state=FlagFind(pawn,0x23afeb0);auto holder=FlagFind(pawn,0x22ac760);
            int team=state?*reinterpret_cast<int*>(state+0x16c):0;
            TwinKartRace::ArenaPoint position{},target{};float heading=0;
            bool ready=driving && holder && team>=1 && team<=2 && FlagPosition(pawn,position,&heading);
            bool found=false;
            if(ready && holder[0x110] && zoneReady[team]){target=zones[team];found=true;}
            if(ready && !found) {
                float best=1.e30f;int selected=-1;
                for(unsigned i=0;i<flagsCount;++i) {
                    auto flagHolder=FlagFind(flags[i],0x22ac760);if(!flagHolder)continue;
                    int color=*reinterpret_cast<int*>(flagHolder+0x104);
                    TwinKartRace::ArenaPoint candidate{};if(!FlagPosition(flags[i],candidate))continue;
                    // Native Flag scores either color at the driver's own zone;
                    // matching colors earn more. Do not strand a usable flag by
                    // imposing an unrelated capture-the-enemy-only rule.
                    float distance=TwinKartRace::ArenaDistance(position,candidate)+5000.f*claims[i]+(color==team?0.f:2500.f);
                    if(distance<best){target=candidate;best=distance;found=true;selected=static_cast<int>(i);}
                }
                if(selected>=0)++claims[selected];
            }
            if(ready && !found) {
                float best=1.e30f;
                for(int other=0;other<=7;++other) {
                    if(other==slot || !pawns[other])continue;
                    auto otherState=FlagFind(pawns[other],0x23afeb0),otherHolder=FlagFind(pawns[other],0x22ac760);
                    if(!otherState || !otherHolder || !otherHolder[0x110] || *reinterpret_cast<int*>(otherState+0x16c)==team)continue;
                    TwinKartRace::ArenaPoint candidate{};if(!FlagPosition(pawns[other],candidate))continue;
                    float distance=TwinKartRace::ArenaDistance(position,candidate);
                    if(distance<best){target=candidate;best=distance;found=true;}
                }
            }
            TwinKartCamera::ObjectIdentity identity{};
            if(!CaptureFlagObject(pawn,identity))continue;
            if(flagDriverPawns[slot].object!=pawn || flagDriverPawns[slot].serial!=identity.serial){flagDrivers[slot].Reset();flagRoutes[slot].Reset();}
            flagDriverPawns[slot]=identity;
            auto steeringTarget=ready && found && flagGround.Ready()?flagRoutes[slot].Target(flagGround,position,target,now):target;
            if(flagRoutes[slot].planned==now)Emit("native_local_flag_ground_route",slot,static_cast<int>(flagRoutes[slot].points.size()));
            auto control=ready && found?flagDrivers[slot].Step(position,heading,steeringTarget,dt,slot):TwinKartRace::ArenaControl{};
            if(!ready)flagDrivers[slot].Reset();
            reinterpret_cast<Input>(image+0x15d4320)(movement,TwinKartRace::NativeKartThrottle(control));
            reinterpret_cast<Input>(image+0x15d3fd0)(movement,control.steer);
            if(logControls)Emit("native_local_flag_arena_control",slot,(driving?1:0)|(found?2:0)|(holder && holder[0x110]?4:0));
            if(flagDiagnostics && ready && found && logControls) {
                Emit("native_local_flag_aim_x",slot,static_cast<int>(steeringTarget.x));
                Emit("native_local_flag_aim_y",slot,static_cast<int>(steeringTarget.y));
                Emit("native_local_flag_aim_z",slot,static_cast<int>(steeringTarget.z));
            }
        }
        ++flagDriverSamples;
      } __finally {InterlockedExchange64(&flagHudPacket,hudPacket);}
    } __except(EXCEPTION_EXECUTE_HANDLER){Emit("native_local_flag_arena_fault",-1,0);}
}
inline void __fastcall OnUnknown(void* raw,void* rawPawn,int slot,bool flag) {
    auto rule=static_cast<BYTE*>(raw),pawn=static_cast<BYTE*>(rawPawn);
    bool local=Local(rule) && Live(pawn);
    bool chaser=false;
    if(InChaserItemWorld(pawn) && Live(rule) && *reinterpret_cast<BYTE**>(rule)==image+0x51dd410 &&
       *reinterpret_cast<int*>(rule+0x130)==1 && *reinterpret_cast<int*>(rule+0x138)>=1 &&
       *reinterpret_cast<int*>(rule+0x138)<=7 && slot>=0 && slot<=*reinterpret_cast<int*>(rule+0x138)) {
        using GetWorld=BYTE*(__fastcall*)(void*);
        auto world=reinterpret_cast<GetWorld>((*reinterpret_cast<void***>(pawn))[0x160/8])(pawn);
        chaser=World(rule)==world && *reinterpret_cast<void**>(rule+0xa0)==*reinterpret_cast<void**>(world+0x118);
    }
    if(local) {
        using Type=void*(__fastcall*)(); using Find=BYTE*(__fastcall*)(void*,void*);
        auto type=reinterpret_cast<Type>(image+0x23afeb0)();
        auto find=reinterpret_cast<Find>((*reinterpret_cast<void***>(pawn))[0x620/8]);
        BYTE* state=find(pawn,type);
        if(Live(state) && *reinterpret_cast<void**>(state+0xa0)==pawn) {
            bool ai=AiSlot(slot,*reinterpret_cast<int*>(rule+0x130),*reinterpret_cast<int*>(rule+0x138));
            // Exact reflected PC bIsAiPlayer setter at RVA 0x23b02c0.
            *reinterpret_cast<BYTE*>(state+0x321)=ai?1:0;
            Emit("native_local_pawn_state_ai",slot,ai?1:0);
        }
    }
    // Default descriptors are original stack-owned objects. Rewrite them at
    // their native comparison boundary, before both model loading and the
    // original PawnState deep copies. Never retain preset pointers across
    // registration. AI roster strings remain owned by our per-race storage;
    // native assignment deep-copies them before asynchronous resource loading.
    BYTE* previousPawn=equipmentPawn;bool previousReady=equipmentReady;
    TwinKartEquipment::Preset previousPreset=equipmentPreset;
    bool previousBodyReady=equipmentBodyReady;TwinKartEquipment::Body previousBody=equipmentBody;
    int previousSlot=equipmentSlot;equipmentSlot=slot;
    bool previousObjectiveScope=objectiveEquipmentScope;
    equipmentPawn=(local || chaser) && slot==0?pawn:nullptr;
    BYTE* account=nullptr;equipmentReady=equipmentPawn && ReadEquipmentPreset(equipmentPreset,&account);
    equipmentBodyReady=equipmentReady && ReadCurrentEquipmentBody(account,equipmentBody);
    if((local || chaser) && aiRosterEnabled && AiSlot(slot,*reinterpret_cast<int*>(rule+0x130),*reinterpret_cast<int*>(rule+0x138))) {
        equipmentPawn=pawn;equipmentReady=aiRoster.Get(slot,equipmentPreset);equipmentBodyReady=false;
        Emit("native_local_ai_roster_slot",slot,equipmentReady?1:0);
    }
    using FactoryType=void*(__fastcall*)();
    if(factoryRosterReady && slot>=0 && slot<TwinKartFactory::RacerCount && FactoryActorWorld(pawn) &&
       *reinterpret_cast<void**>(pawn+0x10)==reinterpret_cast<FactoryType>(image+0x2431240)()) {
        equipmentPawn=pawn;equipmentReady=factoryRacerRoster.GetFixedRacer(slot,equipmentPreset);equipmentBodyReady=false;
        Emit("native_factory_fixed_racer_preset",slot,equipmentReady?1:0);
    }
    if(factoryAndroidKart[0] && slot>=TwinKartFactory::RacerCount && slot<TwinKartFactory::PopulationCount && FactoryActorWorld(pawn) &&
       *reinterpret_cast<void**>(pawn+0x10)==reinterpret_cast<FactoryType>(image+0x2433330)()) {
        equipmentPawn=pawn;equipmentPreset={};int n=static_cast<int>(wcslen(factoryAndroidKart)+1);
        equipmentPreset.kart[0]={factoryAndroidKart,n,n};equipmentReady=true;equipmentBodyReady=false;
        Emit("native_factory_android_original_body_selected",slot,1);
    }
    if(equipmentPawn && !equipmentReady)Emit("native_local_equipment_unavailable",slot,0);
    if(!slot && equipmentReady && !equipmentBodyReady)Emit("native_local_player_account_body_unavailable",0,0);
    if(chaser)objectiveEquipmentScope=true;
    __try {addUnknown(raw,rawPawn,slot,flag);}
    __finally {equipmentPawn=previousPawn;equipmentReady=previousReady;equipmentPreset=previousPreset;equipmentBodyReady=previousBodyReady;equipmentBody=previousBody;equipmentSlot=previousSlot;objectiveEquipmentScope=previousObjectiveScope;}
    PrepareChaserClientPawn(rule,pawn,slot);
    PrepareFactoryControlledPawn(pawn,slot);
    if(PrepareFactoryCooperativeDriver(pawn,slot))factoryCooperativeMask|=1u<<slot;
    ObserveFactoryPawn(rule,pawn,slot);
    ForceFactoryStartWhenReady(rule);
    if(local && Local(rule) && Live(pawn) && TwinKartRace::TeamMode(armedMode)) {
        // Native AddUnknownPlayer puts every AI on team2. Replace that local
        // fallback before phase initialization using its exact PawnState team
        // field (+0x16c). Keep the original human/AI classification unchanged.
        int racers=*reinterpret_cast<int*>(rule+0x130)+*reinterpret_cast<int*>(rule+0x138);
        int team=TwinKartRace::SlotTeam(armedMode,slot,racers);
        int count=TwinKartRace::TeamCount(armedMode,racers);
        using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
        auto type=reinterpret_cast<Type>(image+0x23afeb0)();
        auto find=reinterpret_cast<Find>((*reinterpret_cast<void***>(pawn))[0x620/8]);
        auto state=find(pawn,type);
        if(team>0 && count==*reinterpret_cast<int*>(rule+0x13c) && OwnedComponent(state,pawn,type)) {
            *reinterpret_cast<int*>(state+0x16c)=team;
            Emit("native_local_team_assigned",slot,team);
        } else Emit("native_local_team_assignment_rejected",slot,team);
    }
    // Native client setup owns the initial component population. Complete it
    // before asking the native server interface to supply a missing AI driver;
    // existing movement/animation components are reused by OnConstruct.
    if(local && Local(rule) && Live(pawn) && AiSlot(slot,*reinterpret_cast<int*>(rule+0x130),*reinterpret_cast<int*>(rule+0x138))) {
        if(TwinKartRace::FlagMode(armedMode)) {
            // FlagSquad deliberately uses the original SpeedSingleAiPawn and
            // its arena/flag components. Flag maps have no circuit AIPath;
            // never fabricate or bind FollowTG to them.
            bool prepared=PrepareFlagInteractions(pawn,slot);
            Emit("native_local_flag_ai_ready",slot,prepared?1:0);
            ForceLocalStartWhenReady(rule,slot,prepared);
        } else ForceLocalStartWhenReady(rule,slot,EnsureAiDriver(pawn,slot));
    }
}
inline void PrepareAiPopulation(BYTE* phase,int phaseStage) {
    struct Pawns {BYTE** data;int count,capacity;} pawns{};
    __try {
        if(!InRaceWorld(phase))return;
        __try {
            // Both original native rules use this same world-pawn interface.
            // Prepare all available AI before manager path initialization and
            // again before the native OnRaceStart broadcast. Never duplicate
            // InitializeComponent/BeginPlay or synthesize a race-start event.
            using GetPawns=void(__fastcall*)(void*,Pawns*);
            reinterpret_cast<GetPawns>(image+0x1971550)(phase,&pawns);
            if(!pawns.data || pawns.count<1 || pawns.count>8 || pawns.capacity<pawns.count || pawns.capacity>1024) {
                Emit("native_local_ai_pre_phase_population_rejected",phaseStage,pawns.count);return;
            }
            using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
            auto aiType=reinterpret_cast<Type>(image+(TwinKartRace::FlagMode(armedMode)?0x2413310:0x23bd880))();
            auto stateType=reinterpret_cast<Type>(image+0x23afeb0)();
            int ais=0;
            for(int index=0;index<pawns.count;++index) {
                BYTE* pawn=pawns.data[index];
                if(!Live(pawn) || *reinterpret_cast<void**>(pawn+0x10)!=aiType)continue;
                auto find=reinterpret_cast<Find>((*reinterpret_cast<void***>(pawn))[0x620/8]);
                BYTE* state=find(pawn,stateType);
                if(!Live(state) || *reinterpret_cast<void**>(state+0xa0)!=pawn)continue;
                int slot=*reinterpret_cast<int*>(state+0x168);
                if(slot<1 || slot>7)continue;
                *reinterpret_cast<BYTE*>(state+0x321)=1;
                if(TwinKartRace::FlagMode(armedMode))PrepareFlagAi(pawn,slot);
                else if(EnsureAiDriver(pawn,slot))localAiPreparedMask|=1u<<slot;
                ++ais;
            }
            Emit("native_local_ai_pre_phase_prepared",phaseStage,ais);
        } __finally {
            if(pawns.data) {
                using Free=void(__fastcall*)(void*);
                reinterpret_cast<Free>(image+0x248b950)(pawns.data);
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {Emit("native_local_ai_pre_phase_fault",phaseStage,0);}
}
inline void PreparePhaseAi(void* raw,int stage) {
    __try {
        auto rule=static_cast<BYTE*>(raw);
        if(armed && GetCurrentThreadId()==owner && Live(rule))PrepareAiPopulation(*reinterpret_cast<BYTE**>(rule+0x28),stage);
    } __except(EXCEPTION_EXECUTE_HANDLER) {Emit("native_local_ai_pre_phase_fault",stage,0);}
}
inline void __fastcall OnSplineBegin(void* raw) {
    __try {
        auto manager=static_cast<BYTE*>(raw);
        if(InRaceWorld(manager)) {
            BYTE* state=*reinterpret_cast<BYTE**>(manager+0xa0);
            using GetWorld=BYTE*(__fastcall*)(void*);
            // The manager owner is an actor, not an actor component. Dispatch
            // its actual GetWorld virtual instead of the component-only helper.
            BYTE* stateWorld=Live(state)?reinterpret_cast<GetWorld>((*reinterpret_cast<void***>(state))[0x160/8])(state):nullptr;
            if(Live(stateWorld) && !*reinterpret_cast<void**>(stateWorld+0x38) &&
               raceWorld.Matches(stateWorld,*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),*reinterpret_cast<int*>(image+0x69c28c4))) {
                using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
                auto type=reinterpret_cast<Type>(image+TwinKartGraphics::RacePhaseComponentGetter)();
                auto find=reinterpret_cast<Find>((*reinterpret_cast<void***>(state))[0x620/8]);
                BYTE* phase=find(state,type);
                if(Live(phase) && *reinterpret_cast<void**>(phase+0xa0)==state && *reinterpret_cast<void**>(phase+0x10)==type)
                    PrepareAiPopulation(phase,2);
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {Emit("native_local_ai_pre_spline_fault",2,0);}
    // Native async path-load completion owns initial nearest-spline selection,
    // FollowTG manager binding and item-manager binding. Prepare before this
    // actual consumer too; do not rerun Init or invent a spline index afterward.
    splineBegin(raw);
}
inline void __fastcall OnManagerBegin(void* rule) {PreparePhaseAi(rule,0);managerBegin(rule);}
inline void PrepareHumanStartMovement(BYTE* phase) {
    struct Pawns {BYTE** data;int count,capacity;} pawns{};
    __try {
        if(!TwinKartRace::CircuitMode(armedMode) || !InRaceWorld(phase))return;
        auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
        int count=*reinterpret_cast<int*>(image+0x69c28c4);
        if(TwinKartCamera::Live(humanStartMovement,chunks,count))return;
        using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
        auto current=*reinterpret_cast<BYTE**>(phase+TwinKartGraphics::CurrentPhaseOffset);
        auto phaseClass=reinterpret_cast<Type>(image+TwinKartGraphics::PhaseObjectGetter)();
        auto begin=*reinterpret_cast<unsigned long long*>(image+TwinKartGraphics::BeginRaceNameRva);
        if(!Live(current) || !TwinKartGraphics::ClassifyNativeDrivingPhase(*reinterpret_cast<void**>(current+0x10),phaseClass,
            *reinterpret_cast<unsigned long long*>(current+TwinKartGraphics::PhaseNameOffset),begin,false))return;
        // The ordinary intro normally enables movement before the countdown.
        // An offline event may reach BeginRace without that client operation.
        // Recover only the single real human's missing enable delegate here;
        // never clear frozen/item states, synthesize throttle or drive an AI.
        __try {
            reinterpret_cast<void(__fastcall*)(void*,Pawns*)>(image+0x1971550)(phase,&pawns);
            if(!pawns.data || pawns.count<2 || pawns.count>8 || pawns.capacity<pawns.count || pawns.capacity>1024)return;
            BYTE* human=nullptr;auto stateType=reinterpret_cast<Type>(image+0x23afeb0)();
            auto movementType=reinterpret_cast<Type>(image+0x22f9030)();
            for(int i=0;i<pawns.count;++i) {
                auto pawn=pawns.data[i];if(!Live(pawn))continue;
                auto state=reinterpret_cast<Find>((*reinterpret_cast<void***>(pawn))[0x620/8])(pawn,stateType);
                if(!OwnedComponent(state,pawn,stateType) || !InRaceWorld(state) || *reinterpret_cast<int*>(state+0x168)!=0 || state[0x321])continue;
                if(human)return;human=pawn;
            }
            if(!human)return;
            auto movement=reinterpret_cast<Find>((*reinterpret_cast<void***>(human))[0x620/8])(human,movementType);
            if(!OwnedComponent(movement,human,movementType) || !InRaceWorld(movement))return;
            using Query=BYTE*(__fastcall*)(void*,void*);
            auto interfaceType=reinterpret_cast<Type>(image+0x239be10)();
            auto api=reinterpret_cast<Query>(image+0x277d780)(movement,interfaceType);
            if(api!=movement+0x140 || *reinterpret_cast<BYTE**>(api)!=image+0x5118b80 ||
               (*reinterpret_cast<BYTE***>(api))[0x1f8/8]!=image+0x15be550)return;
            int delegateStorage=*reinterpret_cast<int*>(api+0xc0);
            if(delegateStorage<0 || delegateStorage>64)return;
            TwinKartCamera::ObjectIdentity identity{};if(!CaptureFlagObject(movement,identity))return;
            Emit("native_local_human_start_enable_storage",0,delegateStorage);
            if(!delegateStorage) {
                reinterpret_cast<void(__fastcall*)(void*,bool)>(image+0x15be550)(api,true);
                if(!TwinKartCamera::Live(identity,chunks,count) || !InRaceWorld(movement))return;
                delegateStorage=*reinterpret_cast<int*>(api+0xc0);
                Emit("native_local_human_start_movement_recovered",0,delegateStorage);
            }
            if(delegateStorage>0 && delegateStorage<=64 && CaptureFlagObject(phase,humanStartPhase)) {
                humanStartMovement=identity;
                if(!humanBodyCheckUntil)humanBodyCheckUntil=GetTickCount64()+6000;
            }
        } __finally {if(pawns.data)reinterpret_cast<void(__fastcall*)(void*)>(image+0x248b950)(pawns.data);}
    } __except(EXCEPTION_EXECUTE_HANDLER){Emit("native_local_human_start_movement_rejected",0,0);}
}
enum class ForwardRecovery {None,Press,Release};
inline ForwardRecovery RecoverForwardDecision(bool inputAllowed,bool wHeld,bool usedFallback,float raw) {
    if(!std::isfinite(raw))return ForwardRecovery::None;
    if(inputAllowed && wHeld && std::fabs(raw)<0.001f)return ForwardRecovery::Press;
    if((!inputAllowed || !wHeld) && usedFallback && std::fabs(raw-1.f)<0.001f)return ForwardRecovery::Release;
    return ForwardRecovery::None;
}
inline void __fastcall OnHumanRawForward(void* raw,float value) {
    // A real native axis callback has authority over any previously supplied
    // fallback value (including a real release or a different input device).
    if(GetCurrentThreadId()==owner && raw==humanStartMovement.object)humanForwardFallback=false;
    originalRawForward(raw,arenaForwardAxis?arenaForwardAxis(static_cast<BYTE*>(raw),value):value);
}
inline bool ForwardControlChordAllowed(int mode,bool leftCtrl,bool rightCtrl) {
    return !rightCtrl && (!leftCtrl || TwinKartRace::CircuitMode(mode));
}
inline void RecoverHumanForwardInput(BYTE* movement) {
    if(!wasdForwardEnabled || GetCurrentThreadId()!=owner || movement!=humanStartMovement.object || !armed)return;
    const bool wPhysical=(GetAsyncKeyState('W')&0x8000)!=0;
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
    const int count=*reinterpret_cast<int*>(image+0x69c28c4);
    if(!TwinKartCamera::Live(humanStartMovement,chunks,count) || !InRaceWorld(movement) ||
       !TwinKartCamera::Live(humanStartPhase,chunks,count))return;
    auto phase=static_cast<BYTE*>(humanStartPhase.object);
    auto pawn=*reinterpret_cast<BYTE**>(movement+0xa0);
    if(!InRaceWorld(phase) || !InActorRaceWorld(pawn))return;
    auto controller=*reinterpret_cast<BYTE**>(pawn+0x258);
    auto state=FlagFind(pawn,0x23afeb0);
    if(!InActorRaceWorld(controller) || *reinterpret_cast<BYTE**>(controller)!=image+0x50fa2c0 ||
       *reinterpret_cast<BYTE**>(controller+0x250)!=pawn || !Live(*reinterpret_cast<BYTE**>(controller+0x298)) ||
       !InRaceWorld(state) || state[0x321] || *reinterpret_cast<int*>(state+0x168)!=0)return;
    auto current=*reinterpret_cast<BYTE**>(phase+TwinKartGraphics::CurrentPhaseOffset);
    auto world=World(movement);
    auto body=*reinterpret_cast<BYTE**>(movement+0xf38);
    using Query=bool(__fastcall*)(void*);
    DWORD foreground=0;GetWindowThreadProcessId(GetForegroundWindow(),&foreground);
    bool allowed=Live(current) && world && Live(body) && *reinterpret_cast<BYTE**>(body+0xa0)==pawn && !body[0x359] &&
        *reinterpret_cast<unsigned long long*>(current+TwinKartGraphics::PhaseNameOffset)==
            *reinterpret_cast<unsigned long long*>(image+TwinKartGraphics::BeginRaceNameRva) &&
        foreground==GetCurrentProcessId() && !reinterpret_cast<Query>(image+0x4011c70)(world) &&
        ForwardControlChordAllowed(armedMode,(GetAsyncKeyState(VK_LCONTROL)&0x8000)!=0,(GetAsyncKeyState(VK_RCONTROL)&0x8000)!=0) &&
        !(GetAsyncKeyState(VK_MENU)&0x8000) &&
        !(GetAsyncKeyState('S')&0x8000) &&
        !reinterpret_cast<Query>((*reinterpret_cast<void***>(controller))[0x740/8])(controller);
    bool held=false;
    if(allowed) {
        if(!humanForwardKeyName)
            reinterpret_cast<void(__fastcall*)(void*,const wchar_t*,int)>(image+0x2589130)(&humanForwardKeyName,L"W",0);
        // APlayerController::IsInputKeyDown takes a 24-byte FKey by value.
        // This key contains an engine FName and no cached detail references.
        struct Key {unsigned long long name;void* detail;void* reference;};
        Key key{humanForwardKeyName,nullptr,nullptr};
        using KeyDown=bool(__fastcall*)(void*,Key*);
        held=key.name && ((wPhysical && reinterpret_cast<KeyDown>(image+0x41d8d60)(controller,&key)) ||
            // A real engine W press counts for its input frame even if both
            // Windows key messages arrived before this movement tick.
            reinterpret_cast<KeyDown>(image+0x41e4930)(controller,&key));
    }
    float raw=*reinterpret_cast<float*>(movement+0x18c);
    switch(RecoverForwardDecision(allowed,held,humanForwardFallback,raw)) {
    case ForwardRecovery::Press:
        // Reuse the actual callback bound to MoveForward. Engine start,
        // kart/contact state and throttle processing still decide movement.
        originalRawForward(movement,1.f);
        humanForwardFallback=true;
        if(!humanForwardLogged){humanForwardLogged=true;Emit("native_local_w_forward_restored",armedMode,1);}
        break;
    case ForwardRecovery::Release:
        originalRawForward(movement,0.f);humanForwardFallback=false;break;
    default:break;
    }
}
inline void RetryBufferedBoosterPress(BYTE* movement);
inline void __fastcall OnHumanKartTick(void* raw,float delta,int type,void* function) {
    __try {RecoverHumanForwardInput(static_cast<BYTE*>(raw));}
    __except(EXCEPTION_EXECUTE_HANDLER) {wasdForwardEnabled=false;Emit("native_local_w_forward_fault",armedMode,0);}
    if(arenaKartTick)arenaKartTick(static_cast<BYTE*>(raw),originalRawForward);
    originalHumanKartTick(raw,delta,type,function);
    if(pendingBoosterPress.object && raw==humanStartMovement.object && GetCurrentThreadId()==owner) {
        __try {RetryBufferedBoosterPress(static_cast<BYTE*>(raw));}
        __except(EXCEPTION_EXECUTE_HANDLER){pendingBoosterPress={};}
    }
}
inline void __fastcall OnActivationBegin(void* rule) {
    PreparePhaseAi(rule,1);
    __try {if(Live(static_cast<BYTE*>(rule)))PrepareHumanStartMovement(*reinterpret_cast<BYTE**>(static_cast<BYTE*>(rule)+0x28));}
    __except(EXCEPTION_EXECUTE_HANDLER){Emit("native_local_human_start_movement_rejected",0,0);}
    __try {if(Live(static_cast<BYTE*>(rule)))PrepareFactoryPopulation(*reinterpret_cast<BYTE**>(static_cast<BYTE*>(rule)+0x28),2);}
    __except(EXCEPTION_EXECUTE_HANDLER){Emit("native_factory_pre_phase_driver_fault",2,0);}
    activationBegin(rule);
}
inline void RecoverDriveBody(BYTE* movement,BYTE* pawn,BYTE* motionState,ULONGLONG now,
                            ULONGLONG deadline,ULONGLONG& awaited,bool& attempted,bool factory) {
    if(attempted || now>deadline || !Live(motionState) ||
       *reinterpret_cast<BYTE**>(motionState+0xa0)!=pawn || motionState[0x359] || movement[0x4b8])return;
    auto root=*reinterpret_cast<BYTE**>(pawn+0x130);
    if(!Live(root) || root!=*reinterpret_cast<BYTE**>(movement+0xb8) ||
       *reinterpret_cast<BYTE**>(root+0xa0)!=pawn || !(factory?InFactoryComponentWorld(root):InRaceWorld(root)) ||
       *reinterpret_cast<BYTE**>(root)!=image+0x5116878 || !Live(*reinterpret_cast<BYTE**>(root+0x4c0)))return;
    auto table=*reinterpret_cast<BYTE***>(root);
    if(table[0x408/8]!=image+0x41d8f70 || table[0x5a8/8]!=image+0x42994f0 || table[0x6d8/8]!=image+0x428fcd0)return;
    using Simulating=bool(__fastcall*)(void*,unsigned long long);
    using GetBody=void*(__fastcall*)(void*,unsigned long long,bool);
    auto simulating=reinterpret_cast<Simulating>(table[0x408/8]);
    // An allocated delegate does not mean the replacement kart mesh is still
    // simulating. Re-arm via the original movement interface only when the
    // engine reports an existing, non-simulating body in the opening window.
    // The original tick will then submit its own custom physics callback.
    if(!reinterpret_cast<GetBody>(table[0x6d8/8])(root,0,true))return;
    auto movementTable=*reinterpret_cast<BYTE***>(movement);
    if(movementTable[0x358/8]!=image+0x3de6fb0 || movementTable[0x368/8]!=image+0x3de02a0)return;
    using IsTickEnabled=bool(__fastcall*)(void*);
    auto tickEnabled=reinterpret_cast<IsTickEnabled>(movementTable[0x368/8]);
    bool sim=simulating(root,0),tick=tickEnabled(movement);
    if(sim && tick) {
        // A simulating root may still have no live substep delegate after
        // native equipment replaced the startup body. Require real positive
        // input and a sustained zero counter before reinstalling it once.
        if(*reinterpret_cast<unsigned*>(motionState+0x3bc) || !(*reinterpret_cast<float*>(movement+0x18c)>0)) {
            awaited=0;return;
        }
        if(!awaited){awaited=now;return;}
        if(now-awaited<450)return;
        Emit("native_local_human_start_substep_rebind",0,1);
    }
    auto api=movement+0x140;
    if(*reinterpret_cast<BYTE**>(api)!=image+0x5118b80 ||
       (*reinterpret_cast<BYTE***>(api))[0x1f8/8]!=image+0x15be550)return;
    attempted=true;
    if(!tick) {
        reinterpret_cast<void(__fastcall*)(void*,bool)>(movementTable[0x358/8])(movement,true);
        Emit("native_local_human_start_movement_tick",0,tickEnabled(movement)?1:0);
    }
    if(!sim || tick)reinterpret_cast<void(__fastcall*)(void*,bool)>(image+0x15be550)(api,true);
    Emit("native_local_human_start_body_simulation",0,
        (factory?InFactoryComponentWorld(root):InRaceWorld(root)) && *reinterpret_cast<BYTE**>(pawn+0x130)==root && simulating(root,0)?1:0);
}
inline void RecoverHumanDriveBody(BYTE* movement,BYTE* pawn,BYTE* motionState,ULONGLONG now) {
    RecoverDriveBody(movement,pawn,motionState,now,humanBodyCheckUntil,humanBodyAwaitedSince,humanBodyRepairTried);
}
inline bool IsFactoryHumanPawn(BYTE* pawn) {
    using Type=void*(__fastcall*)();
    if(!FactoryActorWorld(pawn) || *reinterpret_cast<void**>(pawn+0x10)!=reinterpret_cast<Type>(image+0x2431240)())return false;
    auto state=FlagFind(pawn,0x23afeb0),controller=*reinterpret_cast<BYTE**>(pawn+0x258);
    return state && !state[0x321] && *reinterpret_cast<int*>(state+0x168)==0 && state[0x338]==1 &&
        Live(controller) && *reinterpret_cast<BYTE**>(controller)==image+0x50fa2c0 &&
        Live(*reinterpret_cast<BYTE**>(controller+0x298)) && *reinterpret_cast<void**>(controller+0x250)==pawn;
}
inline bool FactoryHumanWon(BYTE* world) {
    if(!factoryCompatEnabled || GetCurrentThreadId()!=owner || !world || factoryHumanWinWorld.object!=world)return false;
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
    const int count=*reinterpret_cast<int*>(image+0x69c28c4);
    return factoryWorld.Matches(world,chunks,count) && TwinKartCamera::Live(factoryHumanWinWorld,chunks,count);
}
inline BYTE* FactoryHumanWinReportWorld(BYTE* rule,BYTE* pawn) {
    if(!factoryCompatEnabled || GetCurrentThreadId()!=owner || !Live(rule) || !IsFactoryHumanPawn(pawn))return nullptr;
    using Type=void*(__fastcall*)();
    if(*reinterpret_cast<void**>(rule+0x10)!=reinterpret_cast<Type>(image+0x24334b0)())return nullptr;
    auto phase=*reinterpret_cast<BYTE**>(rule+0x28);
    if(!InFactoryComponentWorld(phase) ||
       *reinterpret_cast<void**>(phase+0x10)!=reinterpret_cast<Type>(image+0x2431330)())return nullptr;
    auto world=World(phase),state=*reinterpret_cast<BYTE**>(phase+0xa0);
    if(!world || *reinterpret_cast<BYTE**>(world+0x120)!=state || !FactoryActorWorld(state) ||
       *reinterpret_cast<void**>(state+0x10)!=reinterpret_cast<Type>(image+0x2432080)() ||
       reinterpret_cast<BYTE*(__fastcall*)(void*)>((*reinterpret_cast<void***>(pawn))[0x160/8])(pawn)!=world)return nullptr;
    auto pawnState=FlagFind(pawn,0x23afeb0);
    return pawnState && *reinterpret_cast<int*>(pawnState+0x16c)==1?world:nullptr;
}
inline void __fastcall OnFactoryWinReport(void* raw,void* actor) {
    auto rule=static_cast<BYTE*>(raw),pawn=static_cast<BYTE*>(actor);
    BYTE* world=nullptr;
    __try {
        world=FactoryHumanWinReportWorld(rule,pawn);
        TwinKartCamera::ObjectIdentity identity{};
        if(world && !FactoryHumanWon(world) && CaptureFlagObject(world,identity)) {
            factoryHumanWinWorld=identity;
            Emit("native_factory_player_win_decided",0,1);
            auto progress=FlagFind(pawn,0x23cc170);
            if(progress && InFactoryComponentWorld(progress)) {
                const auto rank=*reinterpret_cast<int*>(progress+0x204);
                if(rank>=1 && rank<=TwinKartFactory::PopulationCount)Emit("native_factory_player_win_finish_rank",0,rank);
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {world=nullptr;}
    factoryWinReport(raw,actor);
    __try {
        if(world && FactoryHumanWon(world) && Live(rule) && Live(*reinterpret_cast<BYTE**>(rule+0x88)))
            Emit("native_factory_player_win_popup_created",0,1);
    } __except(EXCEPTION_EXECUTE_HANDLER) {}
}
inline BYTE* FindFactoryHumanPawn(BYTE* phase,ULONGLONG now) {
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
    const int count=*reinterpret_cast<int*>(image+0x69c28c4);
    auto cached=static_cast<BYTE*>(factoryHumanPawn.object);
    if(TwinKartCamera::Live(factoryHumanPawn,chunks,count) && IsFactoryHumanPawn(cached))return cached;
    factoryHumanPawn={};
    if(now<factoryHumanLookupAfter)return nullptr;
    factoryHumanLookupAfter=now+400;
    struct Pawns {BYTE** data;int count,capacity;} pawns{};
    __try {
        // The circuit helper can return no local pawn on TobyRush. Factory's
        // native overlay and population rules use this phase-owned list.
        reinterpret_cast<void(__fastcall*)(void*,Pawns*)>(image+0x1971550)(phase,&pawns);
        if(!pawns.data || pawns.count!=TwinKartFactory::PopulationCount || pawns.capacity<pawns.count || pawns.capacity>4096)return nullptr;
        BYTE* human=nullptr;
        for(int i=0;i<pawns.count;++i)if(IsFactoryHumanPawn(pawns.data[i])) {
            if(human)return nullptr;
            human=pawns.data[i];
        }
        if(!human || !CaptureFlagObject(human,factoryHumanPawn))return nullptr;
        Emit("native_factory_human_slot_bound",0,1);
        return human;
    } __finally {if(pawns.data)reinterpret_cast<void(__fastcall*)(void*)>(image+0x248b950)(pawns.data);}
}
inline void ObserveFactoryHuman(BYTE* world,BYTE* phase,BYTE* current) {
    LONG packet=0;
    __try {
        auto name=*reinterpret_cast<unsigned long long*>(current+TwinKartGraphics::PhaseNameOffset);
        auto racing=*reinterpret_cast<unsigned long long*>(image+TwinKartGraphics::BeginRaceNameRva);
        auto countdown=*reinterpret_cast<unsigned long long*>(image+0x68ba7e8);
        if(!name || (name!=racing && name!=countdown) || reinterpret_cast<bool(__fastcall*)(void*)>(image+0x4011c70)(world))return;
        const float worldTime=*reinterpret_cast<float*>(world+0x5a0);
        int seconds=4095,round=0;
        if(name==countdown) {
            factoryRaceClockStart=-1.f;
            // TobyRushOverrideStartTime uses this original RaceTimeComponent.
            // Its native elapsed getter is negative before the start. Only
            // publish the final three seconds of that synchronized clock.
            auto timer=FlagFind(*reinterpret_cast<BYTE**>(world+0x120),0x23d2930);
            if(InFactoryComponentWorld(timer)) {
                const float elapsed=reinterpret_cast<float(__fastcall*)(void*)>(image+0x1993c90)(timer);
                if(std::isfinite(elapsed) && elapsed>=-3.05f && elapsed<0.f)
                    seconds=(std::min)(3,static_cast<int>(std::ceil(-elapsed)));
            }
        }
        if(name==racing && std::isfinite(worldTime) && worldTime>=0.f) {
            if(factoryRaceClockStart<0.f)factoryRaceClockStart=worldTime;
            const float elapsed=worldTime-factoryRaceClockStart;
            if(elapsed>=0.f && elapsed<4095.f)seconds=static_cast<int>(elapsed);
        }
        const ULONGLONG now=GetTickCount64();
        auto pawn=FindFactoryHumanPawn(phase,now);if(!pawn)return;
        auto state=FlagFind(pawn,0x23afeb0),movement=FlagFind(pawn,0x22f9030),hud=FlagFind(pawn,0x22c2670);
        if(!state)return;
        if(name==racing && InFactoryComponentWorld(movement)) {
            auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
            const int count=*reinterpret_cast<int*>(image+0x69c28c4);
            if(factoryHumanDriveOwner.object!=movement || !TwinKartCamera::Live(factoryHumanDriveOwner,chunks,count)) {
                factoryHumanDriveUntil=0;factoryHumanDriveAttempted=false;factoryHumanDriveAwaited=0;
                if(CaptureFlagObject(movement,factoryHumanDriveOwner))factoryHumanDriveUntil=now+20000;
            }
            if(factoryHumanDriveUntil)RecoverDriveBody(movement,pawn,*reinterpret_cast<BYTE**>(movement+0xf38),now,
                factoryHumanDriveUntil,factoryHumanDriveAwaited,factoryHumanDriveAttempted,true);
        }
        if(hud && *reinterpret_cast<BYTE**>(hud)==image+0x51092c0) {
            auto root=*reinterpret_cast<BYTE**>(hud+0x110);
            unsigned long long slot=0;
            reinterpret_cast<void(__fastcall*)(void*,const wchar_t*,int)>(image+0x2589130)(&slot,L"AddtionalOverlay_Slot",1);
            if(Live(root) && slot && reinterpret_cast<bool(__fastcall*)(void*)>(image+0x3a73d20)(root) &&
               Live(reinterpret_cast<BYTE*(__fastcall*)(void*,unsigned long long)>(image+0x192aa50)(hud,slot)))return;
        }
        int measured=0;const int items=CubeItemCount(pawn,measured,true) && measured>=0 && measured<=2?measured:3;
        const int team=*reinterpret_cast<int*>(state+0x16c);
        auto manager=FlagFind(*reinterpret_cast<BYTE**>(world+0x120),0x2434450);
        if(!manager)manager=FlagFind(*reinterpret_cast<BYTE**>(world+0x118),0x2434450);
        if(InFactoryComponentWorld(manager) && *reinterpret_cast<BYTE**>(manager)==image+0x5213250 &&
           *reinterpret_cast<int*>(manager+0x114)==TwinKartFactory::Laps) {
            const int completed=*reinterpret_cast<int*>(manager+0x110);
            if(completed>=0 && completed<=TwinKartFactory::Laps)round=(std::min)(completed+1,TwinKartFactory::Laps);
        }
        if(team==1 || team==2)packet=(name==racing?2:1)|(items<<2)|(team==2?16:0)|(round<<5)|(seconds<<8);
    } __finally {InterlockedExchange(&factoryHudPacket,packet);}
}
using AddHumanHudWidget=void*(__fastcall*)(void*,unsigned long long,void*);
inline void* __fastcall OnOriginalHumanItemWidget(void* raw,unsigned long long name,void* widgetClass) {
    auto hud=static_cast<BYTE*>(raw);
    auto add=reinterpret_cast<AddHumanHudWidget>(image+0x1518ff0);
    // This is the original Changer_Slot call inside AddItemSlotChanger. Only
    // remember a call that cannot attach because neither native host exists.
    // Once a host appears, replay the original request at most once.
    __try {
        if(TwinKartRace::CircuitMode(armedMode) && InRaceWorld(hud) && !humanItemWidgetRetried && !pendingHumanItemHud.object &&
           !*reinterpret_cast<void**>(hud+0x110) && !*reinterpret_cast<void**>(hud+0x168)) {
            TwinKartCamera::ObjectIdentity cls{},target{};
            if(CaptureFlagObject(hud,target) && (!widgetClass || CaptureFlagObject(static_cast<BYTE*>(widgetClass),cls))) {
                pendingHumanItemHud=target;pendingHumanItemWidgetClass=cls;
                pendingHumanItemWidgetName=name;pendingHumanItemWidgetHasClass=widgetClass!=nullptr;
                Emit("native_local_human_item_hud_waiting_for_host",0,1);
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {pendingHumanItemHud={};}
    return add(raw,name,widgetClass);
}
inline void RecoverHumanClientEffects(BYTE* pawn,ULONGLONG now) {
    int items=0;
    if(CubeItemCount(pawn,items) && lastHumanItemCount!=items) {
        lastHumanItemCount=items;Emit("native_local_human_item_inventory_count",0,items);
    }
    if(now>humanBodyCheckUntil)return;
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
    int count=*reinterpret_cast<int*>(image+0x69c28c4);
    if(TwinKartCamera::Live(pendingHumanItemHud,chunks,count)) {
        auto hud=static_cast<BYTE*>(pendingHumanItemHud.object);
        auto host=*reinterpret_cast<BYTE**>(hud+0x168),fallback=*reinterpret_cast<BYTE**>(hud+0x110);
        if(InRaceWorld(hud) && (Live(host) || Live(fallback))) {
            pendingHumanItemHud={};humanItemWidgetRetried=true;
            if(!pendingHumanItemWidgetHasClass || TwinKartCamera::Live(pendingHumanItemWidgetClass,chunks,count)) {
                auto widget=reinterpret_cast<AddHumanHudWidget>(image+0x1518ff0)(hud,pendingHumanItemWidgetName,pendingHumanItemWidgetClass.object);
                Emit("native_local_human_item_hud_bound_after_host",0,widget?1:0);
            }
        }
    }
    if(humanSoundRepairTried || !reinterpret_cast<bool(__fastcall*)(void*)>(image+0x16a91d0)(pawn))return;
    auto sound=FlagFind(pawn,0x2411750);
    if(!InRaceWorld(sound) || *reinterpret_cast<BYTE**>(sound+0xa0)!=pawn)return;
    using WeakGet=BYTE*(__fastcall*)(void*);
    auto resolve=reinterpret_cast<WeakGet>(image+0x2799770);
    auto source=resolve(sound+0xb0),emitter=resolve(sound+0xb8);
    if(!Live(source) || !Live(emitter) || emitter[0xe0] || !*reinterpret_cast<void**>(emitter+0xe8))return;
    // Use the same start operation as EnableSounds for this exact pawn. Keep
    // the native sound bank, device, mute settings, and FMOD event ownership.
    humanSoundRepairTried=true;
    reinterpret_cast<void(__fastcall*)(void*)>(image+0x16c25d0)(sound);
    Emit("native_local_human_race_sound_enabled",0,Live(emitter) && emitter[0xe0]?1:0);
}
inline void RestoreHumanRaceHud(BYTE* phase,BYTE* pawn,ULONGLONG now,bool countdown=false) {
    if(humanHudStartHandled || (!countdown && now>humanBodyCheckUntil) || !InRaceWorld(phase) ||
       !reinterpret_cast<bool(__fastcall*)(void*)>(image+0x16a91d0)(pawn))return;
    // The race HUD is owned by the local RacePawn, not the controller.
    // CreateHUDWidget's native attach path uses this pawn's PlayerController.
    auto controller=*reinterpret_cast<BYTE**>(pawn+0x258);
    auto hud=FlagFind(pawn,0x22c2670);
    if(!InActorRaceWorld(controller) || !InRaceWorld(hud) || *reinterpret_cast<BYTE**>(controller)!=image+0x50fa2c0 ||
       *reinterpret_cast<BYTE**>(hud)!=image+0x51092c0)return;
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
    int count=*reinterpret_cast<int*>(image+0x69c28c4);
    if(!chunks || count<1 || count>2000000)return;
    BYTE* create=nullptr;BYTE* show=nullptr;BYTE* item=nullptr;bool multipleCreate=false,multipleShow=false,multipleItem=false;
    struct ProgressSlot {const wchar_t* name;unsigned table;unsigned begin;};
    // LapWidget owns the game's lap and timing display. The rank and minimap
    // are separate widgets; restore the native rule for each slot, not the UI
    // class or any gameplay value that the rule binds to.
    constexpr ProgressSlot progress[]{{L"LapWidget_Slot",0x51e9288,0x1944e80},
        {L"RaceRanking_Slot",0x51e9528,0x1946570},{L"Minimap_Slot",0x51e97c8,0x1945640}};
    BYTE* progressRules[ARRAYSIZE(progress)]{};unsigned ambiguousProgress=0;
    for(int index=0;index<count;++index) {
        auto chunk=chunks[index/65536];if(!chunk)continue;
        auto& entry=chunk[index%65536];auto rule=static_cast<BYTE*>(entry.object);
        if(!rule || (entry.flags&0x30000000))continue;
        auto table=*reinterpret_cast<BYTE**>(rule);
        unsigned slot=0;while(slot<ARRAYSIZE(progress) && table!=image+progress[slot].table)++slot;
        if(slot==ARRAYSIZE(progress) && table!=image+0x51e8fe8 && table!=image+0x55f0860 && table!=image+0x51ec1c8)continue;
        if(!Live(rule) || (*reinterpret_cast<unsigned*>(rule+8)&0x18030) || *reinterpret_cast<BYTE**>(rule+0x28)!=phase)continue;
        if(table==image+0x51e8fe8){multipleCreate|=create!=nullptr;create=rule;}
        else if(table==image+0x55f0860){multipleShow|=show!=nullptr;show=rule;}
        else if(table==image+0x51ec1c8){multipleItem|=item!=nullptr;item=rule;}
        else if((*reinterpret_cast<BYTE***>(rule))[0x270/8]==image+progress[slot].begin) {
            if(progressRules[slot])ambiguousProgress|=1u<<slot;
            progressRules[slot]=rule;
        }
    }
    auto root=*reinterpret_cast<BYTE**>(hud+0x110);
    bool recreated=!Live(root);
    if(recreated && create && !multipleCreate) {
        reinterpret_cast<BeginRule>(image+0x1949930)(create);
        root=*reinterpret_cast<BYTE**>(hud+0x110);
    }
    if(!Live(root)){Emit("native_local_race_hud_root_ready",0,0);return;}
    using IsViewport=bool(__fastcall*)(void*);
    auto inViewport=reinterpret_cast<IsViewport>(image+0x3a73d20);
    if(!inViewport(root) && create && !multipleCreate) {
        auto z=*reinterpret_cast<int*>(create+0xb0);
        if(z>=-1000 && z<=1000)reinterpret_cast<void(__fastcall*)(void*,int)>(image+0x3a54eb0)(root,z);
    }
    if(inViewport(root)) {
        using NameFor=void(__fastcall*)(void*,const wchar_t*,int);
        using WidgetFor=BYTE*(__fastcall*)(void*,unsigned long long);
        const auto nameFor=reinterpret_cast<NameFor>(image+0x2589130);
        const auto widgetFor=reinterpret_cast<WidgetFor>(image+0x192aa50);
        unsigned available=0,missing=0,restored=0;
        for(unsigned slot=0;slot<ARRAYSIZE(progress);++slot) {
            if(!InRaceWorld(hud) || *reinterpret_cast<BYTE**>(hud+0x110)!=root)break;
            const auto bit=1u<<slot;
            unsigned long long name=0;nameFor(&name,progress[slot].name,1);
            if(!name)continue;
            if(Live(widgetFor(hud,name))){available|=bit;continue;}
            missing|=bit;
            auto rule=progressRules[slot];
            if((ambiguousProgress&bit) || !Live(rule) || *reinterpret_cast<BYTE**>(rule+0x28)!=phase ||
               (*reinterpret_cast<BYTE***>(rule))[0x270/8]!=image+progress[slot].begin)continue;
            reinterpret_cast<BeginRule>(image+progress[slot].begin)(rule);
            if(Live(widgetFor(hud,name)))restored|=bit;
        }
        Emit("native_local_race_progress_hud_missing_mask",0,missing);
        Emit("native_local_race_progress_hud_restored_mask",0,restored);
        Emit("native_local_race_progress_hud_present_mask",0,available|restored);
    }
    if(show && !multipleShow)reinterpret_cast<BeginRule>(image+0x19637a0)(show);
    else reinterpret_cast<void(__fastcall*)(void*,bool)>(image+0x153c2f0)(hud,false);
    if(recreated && item && !multipleItem && !pendingHumanItemHud.object)
        reinterpret_cast<BeginRule>(image+0x19446f0)(item);
    // The normal tachometer rule also has an exact UI-only path for an
    // already-created widget. Re-enable that original gauge without creating
    // a second movement/tachometer component.
    unsigned long long name=0;
    reinterpret_cast<void(__fastcall*)(void*,const wchar_t*,int)>(image+0x2589130)(&name,L"Tachometer_Slot",1);
    auto gauge=reinterpret_cast<BYTE*(__fastcall*)(void*,unsigned long long)>(image+0x192aa50)(hud,name);
    if(Live(gauge))reinterpret_cast<void(__fastcall*)(void*,bool)>(image+0x1d60420)(gauge,true);
    humanHudStartHandled=inViewport(root);
    Emit(countdown?"native_local_countdown_hud_ready":"native_local_race_hud_root_ready",0,humanHudStartHandled?1:0);
}
inline void PrepareCountdownHud(BYTE* rule) {
    struct Pawns {BYTE** data;int count,capacity;} pawns{};
    __try {
        if(!TwinKartRace::CircuitMode(armedMode) || GetCurrentThreadId()!=owner || !Live(rule) ||
           (*reinterpret_cast<unsigned*>(rule+8)&0x18030))return;
        auto phase=*reinterpret_cast<BYTE**>(rule+0x28);
        if(!InRaceWorld(phase))return;
        auto current=*reinterpret_cast<BYTE**>(phase+TwinKartGraphics::CurrentPhaseOffset);
        if(!Live(current) || *reinterpret_cast<unsigned long long*>(current+TwinKartGraphics::PhaseNameOffset)!=
            *reinterpret_cast<unsigned long long*>(image+0x68ba7e8))return;
        if(!localCountdownStartedAt) {
            localCountdownStartedAt=GetTickCount64();
            Emit("native_local_countdown_rule_entered",-1,armedMode);
        }
        __try {
            reinterpret_cast<void(__fastcall*)(void*,Pawns*)>(image+0x1971550)(phase,&pawns);
            if(!pawns.data || pawns.count<2 || pawns.count>8 || pawns.capacity<pawns.count || pawns.capacity>1024)return;
            BYTE* human=nullptr;
            for(int i=0;i<pawns.count;++i) {
                auto pawn=pawns.data[i],state=FlagFind(pawn,0x23afeb0);
                if(!InRaceWorld(state) || state[0x321] || *reinterpret_cast<int*>(state+0x168)!=0)continue;
                if(human)return;
                human=pawn;
            }
            if(human)RestoreHumanRaceHud(phase,human,GetTickCount64(),true);
        } __finally {if(pawns.data)reinterpret_cast<void(__fastcall*)(void*)>(image+0x248b950)(pawns.data);}
    } __except(EXCEPTION_EXECUTE_HANDLER) {Emit("native_local_countdown_hud_fault",-1,0);}
}
inline void __fastcall OnNotifyCountdownBegin(void* raw) {
    // The stock notification publishes the synchronized race start time.
    // Make the local HUD available before its countdown subscribers run.
    PrepareCountdownHud(static_cast<BYTE*>(raw));
    notifyCountdownBegin(raw);
}
// Apply only to the three original InfinityBooster modes. Inventory is
// authoritative; never overwrite an item or stack a second backup booster.
inline bool NeedsInfiniteBooster(int mode,bool racing,bool localHuman,int itemCount,int failures) {
    return TwinKartRace::InfiniteBoostMode(mode) && racing && localHuman && itemCount==0 && failures<4;
}
inline void MaintainInfiniteBooster(BYTE* phase,bool racing) {
    if(!racing || !TwinKartRace::InfiniteBoostMode(armedMode) || GetCurrentThreadId()!=owner || !armed)return;
    auto now=GetTickCount64();if(now<nextInfiniteBoosterCheck)return;
    nextInfiniteBoosterCheck=now+250;
    __try {
        if(!InRaceWorld(phase))return;
        auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
        int count=*reinterpret_cast<int*>(image+0x69c28c4);
        if(!TwinKartCamera::Live(humanStartMovement,chunks,count) ||
           !TwinKartCamera::Live(humanStartPhase,chunks,count) || humanStartPhase.object!=phase)return;
        auto movement=static_cast<BYTE*>(humanStartMovement.object);
        if(!InRaceWorld(movement) || *reinterpret_cast<BYTE**>(movement)!=image+0x5118590)return;
        auto pawn=*reinterpret_cast<BYTE**>(movement+0xa0);
        if(!InActorRaceWorld(pawn))return;
        auto state=FlagFind(pawn,0x23afeb0),progress=FlagFind(pawn,0x23cc170);
        if(!InRaceWorld(state) || !InRaceWorld(progress) || state[0x321] ||
           *reinterpret_cast<int*>(state+0x168)!=0 || *reinterpret_cast<float*>(progress+0x20c)>0.f)return;
        auto slots=FlagFind(pawn,0x22ef370),items=FlagFind(pawn,0x22d7240);
        if(!InRaceWorld(slots) || !InRaceWorld(items) || *reinterpret_cast<BYTE**>(items+0x548)!=slots)return;
        int before=-1;
        if(!CubeItemCount(pawn,before))return;
        if(before>0){infiniteBoosterFailures=0;return;}
        if(!NeedsInfiniteBooster(armedMode,racing,true,before,infiniteBoosterFailures))return;
        // Original EItemType::Booster (4). Use the same one-item, normal
        // spawn parameters and grant method as the client's fixed item cube.
        // The native slot, button, activation, sound and kart effects remain
        // in charge of what the item does.
        struct alignas(16) ItemSpawn {int source,quantity;float overrideValue,extra;};
        const ItemSpawn spawnArgs{0,1,0.f,0.f};
        using Grant=void(__fastcall*)(void*,unsigned char,const ItemSpawn*);
        reinterpret_cast<Grant>(image+0x151efe0)(items,4,&spawnArgs);
        int after=-1;
        if(CubeItemCount(pawn,after) && after==1) {
            Emit(infiniteBoosterWasAvailable?"native_infinite_booster_refilled":"native_infinite_booster_stocked",armedMode,after);
            infiniteBoosterWasAvailable=true;infiniteBoosterFailures=0;
        } else {
            nextInfiniteBoosterCheck=now+1000;
            if(++infiniteBoosterFailures==4)Emit("native_infinite_booster_stock_rejected",armedMode,after);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        infiniteBoosterFailures=4;Emit("native_infinite_booster_stock_fault",armedMode,0);
    }
}
inline bool InfiniteBoosterPlayerItem(void* raw) {
    auto items=static_cast<BYTE*>(raw);
    if(!TwinKartRace::InfiniteBoostMode(armedMode) || GetCurrentThreadId()!=owner || !InRaceWorld(items))return false;
    auto pawn=*reinterpret_cast<BYTE**>(items+0xa0);
    if(!InActorRaceWorld(pawn) || FlagFind(pawn,0x22d7240)!=items)return false;
    auto state=FlagFind(pawn,0x23afeb0),progress=FlagFind(pawn,0x23cc170);
    if(!InRaceWorld(state) || !InRaceWorld(progress) || state[0x321] ||
       *reinterpret_cast<int*>(state+0x168)!=0 || *reinterpret_cast<float*>(progress+0x20c)>0.f)return false;
    auto phase=static_cast<BYTE*>(humanStartPhase.object);
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
    int count=*reinterpret_cast<int*>(image+0x69c28c4);
    if(!TwinKartCamera::Live(humanStartPhase,chunks,count) || !InRaceWorld(phase))return false;
    auto world=World(items);auto current=*reinterpret_cast<BYTE**>(phase+TwinKartGraphics::CurrentPhaseOffset);
    return Live(world) && !*reinterpret_cast<void**>(world+0x38) &&
        !reinterpret_cast<bool(__fastcall*)(void*)>(image+0x4011c70)(world) && Live(current) &&
        *reinterpret_cast<unsigned long long*>(current+TwinKartGraphics::PhaseNameOffset)==
        *reinterpret_cast<unsigned long long*>(image+TwinKartGraphics::BeginRaceNameRva);
}
inline bool InfiniteBoosterActor(BYTE* actor,unsigned char type) {
    if(!InActorRaceWorld(actor) || (type!=4 && type!=9) || actor[0x240]!=type)return false;
    using Type=void*(__fastcall*)();
    auto klass=reinterpret_cast<Type>(image+(type==4?0x22d26a0:0x22f4120))();
    return *reinterpret_cast<void**>(actor+0x10)==klass;
}
inline void RetryBufferedBoosterPress(BYTE* movement) {
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
    int count=*reinterpret_cast<int*>(image+0x69c28c4);
    DWORD foreground=0;GetWindowThreadProcessId(GetForegroundWindow(),&foreground);
    if(GetTickCount64()>pendingBoosterUntil || foreground!=GetCurrentProcessId() ||
       !TwinKartCamera::Live(pendingBoosterPress,chunks,count) || !TwinKartCamera::Live(humanStartMovement,chunks,count)) {
        pendingBoosterPress={};return;
    }
    auto item=static_cast<BYTE*>(pendingBoosterPress.object);
    auto pawn=*reinterpret_cast<BYTE**>(movement+0xa0);
    if(!InfiniteBoosterPlayerItem(item) || *reinterpret_cast<BYTE**>(item+0xa0)!=pawn) {
        pendingBoosterPress={};return;
    }
    if(!(*reinterpret_cast<float*>(movement+0x18c)>0.f) ||
       !(reinterpret_cast<float(__fastcall*)(void*)>(image+0x15f5260)(movement+0x1d8)>0.f))return;
    // The native slot handler checks availability, collision state and the
    // real accelerator again. Do not generate input or an automatic boost.
    pendingBoosterPress={};
    reinterpret_cast<void(__fastcall*)(void*,int)>(image+0x1555690)(item,0);
    Emit("native_infinite_booster_buffered_press_delivered",armedMode,1);
}
inline void ObserveHumanDriving(BYTE* phase) {
    if(!TwinKartRace::CircuitMode(armedMode) || GetCurrentThreadId()!=owner || humanDriveSamples>=60)return;
    auto now=GetTickCount64();if(now<humanDriveSampleAfter)return;
    humanDriveSampleAfter=now+250;
    __try {
        if(!InRaceWorld(phase))return;
        if(!localCountdownElapsedLogged) {
            localCountdownElapsedLogged=true;
            Emit("native_local_countdown_to_begin_race_ms",armedMode,
                 localCountdownStartedAt && now-localCountdownStartedAt<120000?static_cast<int>(now-localCountdownStartedAt):-1);
        }
        if(!humanStartMovement.object)PrepareHumanStartMovement(phase);
        auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
        int count=*reinterpret_cast<int*>(image+0x69c28c4);
        if(!TwinKartCamera::Live(humanStartMovement,chunks,count)) {
            humanStartMovement={};humanStartPhase={};humanForwardFallback=false;
            PrepareHumanStartMovement(phase);
            if(!TwinKartCamera::Live(humanStartMovement,chunks,count))return;
        }
        auto movement=static_cast<BYTE*>(humanStartMovement.object);
        if(!InRaceWorld(movement) || *reinterpret_cast<BYTE**>(movement)!=image+0x5118590)return;
        auto pawn=*reinterpret_cast<BYTE**>(movement+0xa0);auto state=FlagFind(pawn,0x23afeb0);
        if(!InRaceWorld(state) || state[0x321] || *reinterpret_cast<int*>(state+0x168)!=0)return;
        DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);bool focused=pid==GetCurrentProcessId();
        int flags=(focused?1:0)|(focused && ((GetAsyncKeyState(VK_UP)|GetAsyncKeyState('W'))&0x8000)?2:0)|
            (movement[0x4b8]?4:0);
        auto body=*reinterpret_cast<BYTE**>(movement+0xf38);
        bool liveBody=Live(body) && InRaceWorld(body) && *reinterpret_cast<BYTE**>(body+0xa0)==pawn;
        if(liveBody && body[0x359])flags|=8;
        if(liveBody)RecoverHumanDriveBody(movement,pawn,body,now);
        RestoreHumanRaceHud(phase,pawn,now);
        RecoverHumanClientEffects(pawn,now);
        auto raw=*reinterpret_cast<float*>(movement+0x18c),drive=*reinterpret_cast<float*>(movement+0x1c4);
        const auto milli=[](float n){return std::isfinite(n) && std::fabs(n)<=100?static_cast<int>(n*1000):-100001;};
        ++humanDriveSamples;
        Emit("native_local_human_drive_flags",0,flags);
        Emit("native_local_human_raw_throttle_milli",0,milli(raw));
        Emit("native_local_human_drive_throttle_milli",0,milli(drive));
        Emit("native_local_human_substep_counter",0,liveBody?*reinterpret_cast<int*>(body+0x3bc):-1);
        Emit("native_local_human_wheel_contacts",0,*reinterpret_cast<int*>(movement+0x4f0));
    } __except(EXCEPTION_EXECUTE_HANDLER){humanDriveSamples=60;Emit("native_local_human_drive_sample_rejected",0,0);}
}
inline void __fastcall OnAutoDrive(void* raw,bool enabled) {
    auto movement=static_cast<BYTE*>(raw);
    bool factory=false;int factorySlot=-1;
    if(factoryCompatEnabled && GetCurrentThreadId()==owner && Live(movement)) {
        auto pawn=*reinterpret_cast<BYTE**>(movement+0xa0);
        if(FactoryActorWorld(pawn)) {
            using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
            auto stateType=reinterpret_cast<Type>(image+0x23afeb0)();
            auto state=reinterpret_cast<Find>((*reinterpret_cast<void***>(pawn))[0x620/8])(pawn,stateType);
            if(OwnedComponent(state,pawn,stateType)) {
                factorySlot=*reinterpret_cast<int*>(state+0x168);
                if(factorySlot>=1 && factorySlot<TwinKartFactory::PopulationCount) {
                    auto pawnType=reinterpret_cast<Type>(image+(factorySlot<TwinKartFactory::RacerCount?0x2431240:0x2433330))();
                    factory=*reinterpret_cast<void**>(pawn+0x10)==pawnType &&
                        *reinterpret_cast<void**>(movement+0x10)==reinterpret_cast<Type>(image+0x22f9030)();
                }
            }
        }
    }
    if(InRaceWorld(movement) || factory) {
        BYTE* path=*reinterpret_cast<BYTE**>(movement+0xf08);
        if(Live(path)) {
            using Type=void*(__fastcall*)();
            void* follow=reinterpret_cast<Type>(image+0x22b1e10)();
            if(*reinterpret_cast<void**>(path+0x10)==follow) {
                // Exact native enabled branch updates its control flag and
                // returns immediately when a driver is already bound. Only
                // the disabled branch destroys the FollowTG component; do
                // not swallow a legitimate enable request with that branch.
                if(enabled)autoDrive(raw,true);
                else *reinterpret_cast<BYTE*>(movement+0xedd)=0;
                if(factory)Emit("native_factory_followtg_preserved",factorySlot,enabled?1:0);
                Emit("native_local_followtg_preserved",-1,enabled?1:0);
                return;
            }
        }
    }
    autoDrive(raw,enabled);
}
inline void RecordNativeFinishers(BYTE* component) {
    if(!eventDiagnosticsInstalled || !TwinKartRace::CircuitMode(armedMode) || !InRaceWorld(component))return;
    __try {
        // 0x1971550 is a specific pawn-list component method, not a general
        // world enumeration API: RaceResultComponent cannot be passed to it.
        // Read exact live PawnState objects from the original registry instead.
        // This is diagnostics-only, once per result; no native array is made.
        using Type=void*(__fastcall*)();auto type=reinterpret_cast<Type>(image+0x23afeb0)();
        auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
        int count=*reinterpret_cast<int*>(image+0x69c28c4),observed=0;unsigned seen=0;
        if(!type || !chunks || count<1 || count>2000000)return;
        for(int index=0;index<count;++index) {
                auto chunk=chunks[index/65536];if(!chunk || (chunk[index%65536].flags&0x30000000))continue;
                auto state=static_cast<BYTE*>(chunk[index%65536].object);
                if(!Live(state) || (*reinterpret_cast<unsigned*>(state+8)&0x18030) ||
                   *reinterpret_cast<void**>(state+0x10)!=type || !InRaceWorld(state))continue;
                auto progress=FlagFind(*reinterpret_cast<BYTE**>(state+0xa0),0x23cc170);
                if(!progress || !InRaceWorld(progress))continue;
                int slot=*reinterpret_cast<int*>(state+0x168),rank=*reinterpret_cast<int*>(progress+0x204);
                float time=*reinterpret_cast<float*>(progress+0x20c);
                if(slot<0 || slot>7 || (seen&(1u<<slot)) || rank<0 || rank>8 || !std::isfinite(time) || time<0 || time>86400)continue;
                seen|=1u<<slot;++observed;
                Emit("native_local_race_finish_rank",slot,rank);
                Emit("native_local_race_finish_time_ms",slot,static_cast<int>(time*1000));
        }
        Emit("native_local_race_result_snapshot",-1,observed);
    } __except(EXCEPTION_EXECUTE_HANDLER) {Emit("native_local_race_result_snapshot_failed",-1,0);}
}
inline void __fastcall OnResultInit(void* raw,const void* resultClass,const void* playerClass,const void* pointClass,void** provider,void** styler) {
    auto component=static_cast<BYTE*>(raw);
    RecordNativeFinishers(component);
    if(TwinKartRace::CircuitMode(armedMode) && InRaceWorld(component) && styler && !*styler) {
        // Original factories return native-owned TUniquePtr storage in
        // RDX; RCX is unused by these exact implementations. The original
        // Init consumes the pointer and owns all ranking/time data and UI.
        using Factory=void*(__fastcall*)(const void*,void**);
        reinterpret_cast<Factory>(image+TwinKartRace::ResultFactory(armedMode))(nullptr,styler);
        Emit("native_local_result_styler_fallback",-1,*styler?armedMode:0);
    }
    resultInit(raw,resultClass,playerClass,pointClass,provider,styler);
}
inline bool Install(BYTE* base,Event callback) {
    if(installed) return owner==GetCurrentThreadId();
    image=base; event=callback; owner=GetCurrentThreadId();
    const BYTE prefix[]={0x40,0x57,0x48,0x83,0xec,0x60,0x88,0x91,0xdd,0x0e,0x00,0x00,0x48,0x8b,0xf9};
    const BYTE registerPrefix[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74};
    const BYTE resultPrefix[]={0x48,0x83,0xec,0x58,0x48,0x89,0x5c,0x24,0x60,0x48,0x89,0x6c,0x24,0x68,0x49,0x8b,0xe9};
    const BYTE factoryPrefix[]={0x40,0x53,0x48,0x83,0xec,0x20};
    const BYTE bindPrefix[]={0x48,0x89,0x91,0x08,0x0f,0x00,0x00,0xc3};
    const BYTE managerPrefix[]={0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0x41,0x28,0x48,0x8b,0x98,0xa0,0x00,0x00,0x00};
    const BYTE activationPrefix[]={0x41,0x56,0x48,0x83,0xec,0x30,0x48,0x8b,0x49,0x28,0x48,0x8d,0x54,0x24,0x20};
    const BYTE splinePrefix[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18};
    const BYTE forceFlagPrefix[]={0x40,0x53,0x48,0x83,0xec,0x30,0x33,0xc0,0xc6,0x81,0xc9,0x00,0x00,0x00,0x01};
    const BYTE parametersPrefix[]={0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x48,0x89,0x11};
    const BYTE componentPrefix[]={0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x48,0x8b,0x89,0xa0,0x00,0x00,0x00};
    const BYTE flagCommandPrefix[]={0x48,0x89,0x5c,0x24,0x10,0x55,0x48,0x8b,0xec,0x48,0x83,0xec,0x60};
    const BYTE throttlePrefix[]={0x4c,0x8b,0xdc,0x49,0x89,0x5b,0x10,0x49,0x89,0x73,0x18};
    const BYTE steerPrefix[]={0x40,0x53,0x48,0x83,0xec,0x30,0x80,0xb9,0xfd,0x0e,0x00,0x00,0x00};
    const BYTE tracePrefix[]={0x48,0x8b,0xc4,0x48,0x89,0x58,0x08,0x48,0x89,0x68,0x10,0x48,0x89,0x70,0x20,0x57};
    // Prologue consists entirely of relocatable register/stack instructions.
    // Calls are patched only while this owned game's window thread is idle.
    if(memcmp(image+0x15acaf0,prefix,sizeof(prefix)) || memcmp(image+0x1973d10,registerPrefix,sizeof(registerPrefix)) ||
       memcmp(image+0x15f5d00,resultPrefix,sizeof(resultPrefix)) || memcmp(image+0x168da80,factoryPrefix,sizeof(factoryPrefix)) || memcmp(image+0x168db20,factoryPrefix,sizeof(factoryPrefix)) ||
       memcmp(image+0x168da30,factoryPrefix,sizeof(factoryPrefix)) || memcmp(image+0x168dad0,factoryPrefix,sizeof(factoryPrefix)) ||
       memcmp(image+0x15d1100,bindPrefix,sizeof(bindPrefix)) || memcmp(image+0x1952440,managerPrefix,sizeof(managerPrefix)) ||
       memcmp(image+0x193fe50,activationPrefix,sizeof(activationPrefix)) || memcmp(image+0x14d2660,splinePrefix,sizeof(splinePrefix)) ||
       memcmp(image+0x1638220,forceFlagPrefix,sizeof(forceFlagPrefix)) ||
       memcmp(image+0x2771dd0,parametersPrefix,sizeof(parametersPrefix)) ||
       memcmp(image+0x3de4900,componentPrefix,sizeof(componentPrefix)) ||
       memcmp(image+0x191dc50,flagCommandPrefix,sizeof(flagCommandPrefix)) ||
       memcmp(image+0x15d4320,throttlePrefix,sizeof(throttlePrefix)) ||
       memcmp(image+0x15d3fd0,steerPrefix,sizeof(steerPrefix)) ||
       memcmp(image+0x3de0910,tracePrefix,sizeof(tracePrefix))) return false;
    spawn=reinterpret_cast<Spawn>(image+0x1926150);
    addUnknown=reinterpret_cast<AddUnknown>(image+0x18f7370);
    construct=reinterpret_cast<Construct>(image+0x278aa90);
    const unsigned sites[]={0x1973d59,0x1973da1,0x1973de7,0x1973e7a,0x1973eaf};
    for(unsigned i=0;i<5;++i) if(!constructionPatches[i].Call(image+sites[i],reinterpret_cast<void*>(construct),reinterpret_cast<void*>(OnConstruct))) { RestoreConstruction(); return false; }
    if(!spawnPatch.Call(image+0x1913d1a,reinterpret_cast<void*>(spawn),reinterpret_cast<void*>(OnSpawn))) { RestoreConstruction(); return false; }
    if(!statePatch.Call(image+0x1913d72,reinterpret_cast<void*>(addUnknown),reinterpret_cast<void*>(OnUnknown))) { spawnPatch.Restore(); RestoreConstruction(); return false; }
    if(!driverPatch.Entry(image+0x15acaf0,prefix,sizeof(prefix),reinterpret_cast<void*>(OnAutoDrive),reinterpret_cast<void**>(&autoDrive))) {
        statePatch.Restore(); spawnPatch.Restore(); RestoreConstruction(); return false;
    }
    if(!resultPatch.Entry(image+0x15f5d00,resultPrefix,sizeof(resultPrefix),reinterpret_cast<void*>(OnResultInit),reinterpret_cast<void**>(&resultInit))) {
        driverPatch.Restore(); statePatch.Restore(); spawnPatch.Restore(); RestoreConstruction(); return false;
    }
    if(!managerPatch.Entry(image+0x1952440,managerPrefix,sizeof(managerPrefix),reinterpret_cast<void*>(OnManagerBegin),reinterpret_cast<void**>(&managerBegin))) {
        resultPatch.Restore();driverPatch.Restore();statePatch.Restore();spawnPatch.Restore();RestoreConstruction();return false;
    }
    if(!activationPatch.Entry(image+0x193fe50,activationPrefix,sizeof(activationPrefix),reinterpret_cast<void*>(OnActivationBegin),reinterpret_cast<void**>(&activationBegin))) {
        managerPatch.Restore();resultPatch.Restore();driverPatch.Restore();statePatch.Restore();spawnPatch.Restore();RestoreConstruction();return false;
    }
    if(!splinePatch.Entry(image+0x14d2660,splinePrefix,sizeof(splinePrefix),reinterpret_cast<void*>(OnSplineBegin),reinterpret_cast<void**>(&splineBegin))) {
        activationPatch.Restore();managerPatch.Restore();resultPatch.Restore();driverPatch.Restore();statePatch.Restore();spawnPatch.Restore();RestoreConstruction();return false;
    }
    kartEqual=reinterpret_cast<DescriptorEqual>(image+0x15a51a0);
    characterEqual=reinterpret_cast<DescriptorEqual>(image+0x15089b0);
    setBody=reinterpret_cast<SetBody>(image+0x1609160);
    if(!kartEquipmentPatch.Call(image+0x15bec36,reinterpret_cast<void*>(kartEqual),reinterpret_cast<void*>(OnKartDescriptorEqual)) ||
       !characterEquipmentPatch.Call(image+0x15341a6,reinterpret_cast<void*>(characterEqual),reinterpret_cast<void*>(OnCharacterDescriptorEqual)) ||
       !bodyEquipmentPatch.Call(image+0x18f788c,reinterpret_cast<void*>(setBody),reinterpret_cast<void*>(OnKartBody))) {
        bodyEquipmentPatch.Restore();characterEquipmentPatch.Restore();kartEquipmentPatch.Restore();splinePatch.Restore();activationPatch.Restore();managerPatch.Restore();
        resultPatch.Restore();driverPatch.Restore();statePatch.Restore();spawnPatch.Restore();RestoreConstruction();return false;
    }
    flagNetMode=reinterpret_cast<NetMode>(image+0x43beaf0);
    aiSkillNetMode=reinterpret_cast<NetMode>(image+0x43beaf0);
    aiUseSkill=reinterpret_cast<UseSkillIfCan>(image+0x16b45e0);
    baseSkillCanUse=reinterpret_cast<CanUseSkill>(image+0x16889a0);
    currentLevelName=reinterpret_cast<CurrentLevelName>(image+0x3f43180);
    flagZoneNetMode=reinterpret_cast<NetMode>(image+0x3cb9a40);
    flagConfig=reinterpret_cast<FlagConfig>(image+0x186ee70);
    const BYTE flagBeginPrefix[]={0x40,0x55,0x53,0x57,0x48,0x8d,0x6c,0x24,0xb9,0x48,0x81,0xec,0x90,0x00,0x00,0x00};
    const BYTE aiSkillCanUsePrefix[]={0x40,0x55,0x57,0x41,0x56,0x48,0x83,0xec,0x20,0x48,0x8b,0x81,0xa8,0x00,0x00,0x00};
    if(!flagBeginPatch.Entry(image+0x18f9aa0,flagBeginPrefix,sizeof(flagBeginPrefix),reinterpret_cast<void*>(OnFlagMovementBegin),reinterpret_cast<void**>(&flagMovementBegin)) ||
       !flagPhysicsPatch.Call(image+0x18f9ae3,reinterpret_cast<void*>(flagNetMode),reinterpret_cast<void*>(LocalFlagPhysicsNetMode)) ||
       !flagZonePatch.Call(image+0x18f86c6,reinterpret_cast<void*>(flagZoneNetMode),reinterpret_cast<void*>(LocalFlagZoneInitialization)) ||
       !flagConfigPatches[0].Call(image+0x18f94af,reinterpret_cast<void*>(flagConfig),reinterpret_cast<void*>(LocalFlagConfig)) ||
       !flagConfigPatches[1].Call(image+0x18f9c78,reinterpret_cast<void*>(flagConfig),reinterpret_cast<void*>(LocalFlagConfig)) ||
       !aiSkillModePatch.Call(image+0x14e1a0c,reinterpret_cast<void*>(aiSkillNetMode),reinterpret_cast<void*>(LocalAiSkillNetMode)) ||
       !aiSkillUsePatch.Call(image+0x14e6389,reinterpret_cast<void*>(aiUseSkill),reinterpret_cast<void*>(OnAiUseSkill)) ||
       !aiSkillCanUsePatch.Entry(image+0x14c0a70,aiSkillCanUsePrefix,sizeof(aiSkillCanUsePrefix),reinterpret_cast<void*>(OnAiCanUseSkill),reinterpret_cast<void**>(&aiSkillCanUse)) ||
       !cubePathLevelPatch.Call(image+0x14d2384,reinterpret_cast<void*>(currentLevelName),reinterpret_cast<void*>(CubePathLevelName)) ||
       !flagItemContextPatch.Call(image+0x153e6d4,reinterpret_cast<void*>(construct),reinterpret_cast<void*>(OnItemContextConstruct)) ||
       !humanItemWidgetPatch.Call(image+0x194489f,image+0x1518ff0,reinterpret_cast<void*>(OnOriginalHumanItemWidget))) {
        humanItemWidgetPatch.Restore();
        flagItemContextPatch.Restore();
        cubePathLevelPatch.Restore();
        aiSkillCanUsePatch.Restore();
        aiSkillUsePatch.Restore();
        aiSkillModePatch.Restore();
        for(auto& patch:flagConfigPatches)patch.Restore();flagZonePatch.Restore();flagPhysicsPatch.Restore();flagBeginPatch.Restore();
        bodyEquipmentPatch.Restore();characterEquipmentPatch.Restore();kartEquipmentPatch.Restore();splinePatch.Restore();activationPatch.Restore();managerPatch.Restore();
        resultPatch.Restore();driverPatch.Restore();statePatch.Restore();spawnPatch.Restore();RestoreConstruction();return false;
    }
    const BYTE notifyPrefix[]={0x48,0x89,0x5c,0x24,0x10,0x57,0x48,0x83,0xec,0x30,0x48,0x8b,0x01,0x48,0x8b,0xf9};
    const bool notify=notifyCountdownPatch.Entry(image+0x19567b0,notifyPrefix,sizeof(notifyPrefix),
        reinterpret_cast<void*>(OnNotifyCountdownBegin),reinterpret_cast<void**>(&notifyCountdownBegin));
    Emit("native_local_countdown_notifier_installed",-1,notify?1:0);
    wchar_t keyboard[4]{};
    const BYTE tickPrefix[]{0x48,0x8b,0xc4,0x48,0x89,0x58,0x18,0x55,0x56,0x57,0x41,0x54,0x41,0x55};
    const bool kartTick=humanKartTickPatch.Entry(image+0x15d9230,tickPrefix,sizeof(tickPrefix),
        reinterpret_cast<void*>(OnHumanKartTick),reinterpret_cast<void**>(&originalHumanKartTick));
    Emit("native_timeattack_practice_observer_installed",-1,kartTick?1:0);
    if(kartTick && GetEnvironmentVariableW(L"TWIN_KART_ARROW_LOOK",keyboard,ARRAYSIZE(keyboard))==1 && keyboard[0]==L'1') {
        const BYTE rawPrefix[]{0x40,0x53,0x48,0x83,0xec,0x30,0x0f,0x29,0x74,0x24,0x20,0x48,0x8b,0xd9,0x0f,0x28,0xf1};
        wasdForwardEnabled=humanRawForwardPatch.Entry(image+0x15ccd60,rawPrefix,sizeof(rawPrefix),
            reinterpret_cast<void*>(OnHumanRawForward),reinterpret_cast<void**>(&originalRawForward));
        if(!wasdForwardEnabled)humanRawForwardPatch.Restore();
        Emit("native_local_w_forward_adapter_installed",-1,wasdForwardEnabled?1:0);
    }
    installed=true; Emit("native_local_ai_hooks_installed",-1,1); return true;
}
inline void Arm(int mode) {
    raceWorld.Reset();armedMode=mode;armed=TwinKartRace::PlayableMode(mode);
    objectiveWorld.Reset();
    factoryWorld.Reset();factoryHumanWinWorld={};factoryPopulation.Reset();factorySpawnSlot=-1;
    factoryRosterReady=false;factoryAndroidKart[0]=0;
    factoryStartRequested=false;factoryCooperativeMask=0;factoryLastPhase=~0ull;factoryItemUseObserved=0;chaserItemUseObserved=0;
    nativeCharacterRules=false;flagSkillsEnabled=false;flagSkillTickObserved=false;
    cubeBaseTrack[0]=0;
    for(auto& value:flagSkillReadinessObserved)value=-1;
    localAiPreparedMask=0;localStartRequested=false;
    humanStartMovement={};humanStartPhase={};humanForwardFallback=humanForwardLogged=false;
    humanDriveSampleAfter=0;humanDriveSamples=0;
    nextInfiniteBoosterCheck=0;infiniteBoosterFailures=0;infiniteBoosterWasAvailable=false;
    pendingBoosterPress={};pendingBoosterUntil=0;
    humanBodyCheckUntil=humanBodyAwaitedSince=0;humanBodyRepairTried=false;
    humanSoundRepairTried=false;lastHumanItemCount=-1;
    humanHudStartHandled=false;
    localCountdownStartedAt=0;localCountdownElapsedLogged=false;
    pendingHumanItemHud={};pendingHumanItemWidgetClass={};pendingHumanItemWidgetName=0;
    pendingHumanItemWidgetHasClass=false;humanItemWidgetRetried=false;
    for(auto& mirror:flagMirrors)mirror={};
    for(int i=0;i<8;++i){flagDrivers[i].Reset();flagDriverPawns[i]={};}
      flagDriverTime=0;flagDriverSamples=0;flagDiagnosticNext=0;
    flagLoggedPhase=~0ull;flagLoggedWinner=-1;for(auto& points:flagLoggedPoints)points=-1;
    flagHumanDriveOwner={};flagHumanDriveUntil=flagHumanDriveAwaited=0;flagHumanDriveAttempted=false;
    InterlockedExchange64(&flagHudPacket,0);
    factoryHumanPawn={};factoryHumanLookupAfter=0;factoryRaceClockStart=-1.f;
    factoryHumanDriveOwner={};factoryHumanDriveUntil=factoryHumanDriveAwaited=0;factoryHumanDriveAttempted=false;
    InterlockedExchange(&factoryHudPacket,0);
    flagGround.Reset();flagGroundDecided=false;for(auto& route:flagRoutes)route.Reset();
}
}
