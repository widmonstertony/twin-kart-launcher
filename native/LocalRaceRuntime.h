#pragma once
#include "OwnedCodePatch.h"
#include "DriftObjectIdentity.h"
#include "LocalRaceWorld.h"
#include "NativeRacePhase.h"
#include "NativeEquipmentPreset.h"
#include "NativeAiRoster.h"
#include "NativeAiAssets.h"
#include "NativeRaceModes.h"

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
inline AddUnknown addUnknown=nullptr;
inline AutoDrive autoDrive=nullptr;
inline ResultInit resultInit=nullptr;
inline Construct construct=nullptr;
inline BeginRule managerBegin=nullptr,activationBegin=nullptr,splineBegin=nullptr;
inline BYTE* registrationActor=nullptr;
inline LocalRaceWorld raceWorld;
inline TwinKartNative::OwnedCodePatch spawnPatch,statePatch,driverPatch,resultPatch;
inline TwinKartNative::OwnedCodePatch managerPatch,activationPatch,splinePatch;
inline TwinKartNative::OwnedCodePatch constructionPatches[5];
using DescriptorEqual=bool(__fastcall*)(const void*,const void*);
inline DescriptorEqual kartEqual=nullptr,characterEqual=nullptr;
inline TwinKartNative::OwnedCodePatch kartEquipmentPatch,characterEquipmentPatch;
inline BYTE* equipmentPawn=nullptr;
inline TwinKartEquipment::Preset equipmentPreset;
inline bool equipmentReady=false;
inline TwinKartRace::AiRoster aiRoster;
inline bool aiRosterEnabled=false;
inline int equipmentSlot=0;
using SetBody=void(__fastcall*)(void*,const void*,const void*);
inline SetBody setBody=nullptr;
inline TwinKartNative::OwnedCodePatch bodyEquipmentPatch;
inline TwinKartEquipment::Body equipmentBody;
inline bool equipmentBodyReady=false;
inline unsigned flagAiPreparedMask=0;
inline bool flagStartRequested=false;
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
    return armed && GetCurrentThreadId()==owner && Live(rule) &&
        *reinterpret_cast<void**>(rule)==image+0x51dd410 && World(rule);
}
inline bool InRaceWorld(BYTE* component) {
    if(!armed || GetCurrentThreadId()!=owner || !Live(component))return false;
    return raceWorld.Matches(World(component),*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),
        *reinterpret_cast<int*>(image+0x69c28c4));
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
    return Live(component) && *reinterpret_cast<void**>(component+0xa0)==equipmentPawn &&
        *reinterpret_cast<void**>(component+0x10)==type && InRaceWorld(component) && current==component+offset;
}
inline bool __fastcall OnKartDescriptorEqual(const void* current,const void* desired) {
    if(OwnEquipmentDescriptor(current,0x22fef20,0x10b0)) {
        TwinKartEquipment::ApplyKart(const_cast<void*>(desired),equipmentPreset,reinterpret_cast<TwinKartEquipment::CopyString>(image+0x923880));
        Emit(equipmentSlot?"native_local_ai_kart_preset_applied":"native_local_player_kart_preset_applied",equipmentSlot,1);
    }
    return kartEqual(current,desired);
}
inline bool __fastcall OnCharacterDescriptorEqual(const void* current,const void* desired) {
    if(OwnEquipmentDescriptor(current,0x2251540,0x1020)) {
        TwinKartEquipment::ApplyCharacter(const_cast<void*>(desired),equipmentPreset,reinterpret_cast<TwinKartEquipment::CopyString>(image+0x923880));
        Emit(equipmentSlot?"native_local_ai_character_preset_applied":"native_local_player_character_preset_applied",equipmentSlot,1);
    }
    return characterEqual(current,desired);
}
inline void __fastcall OnKartBody(void* component,const void* body,const void* channel) {
    if(equipmentBodyReady && OwnEquipmentDescriptor(component,0x22ff4a0,0)) {
        Emit("native_local_player_account_body_applied",0,1);
        setBody(component,&equipmentBody,channel);return;
    }
    setBody(component,body,channel);
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
inline bool __fastcall OnSpawn(void* raw,void* mode,int slot,void* start,void** pawn,bool* isAi) {
    auto rule=static_cast<BYTE*>(raw);
    if(BindWorld(rule) && isAi) {
        int players=*reinterpret_cast<int*>(rule+0x130),ais=*reinterpret_cast<int*>(rule+0x138);
        if(players==1 && ais>=1 && ais<=7 && slot>=0 && slot<players+ais) {
            *isAi=AiSlot(slot,players,ais);
            Emit("native_local_slot_classified",slot,*isAi?1:0);
        }
    }
    bool result=spawn(raw,mode,slot,start,pawn,isAi);
    if(Local(rule)) {
        Emit("native_local_pawn_spawned",slot,result && pawn && *pawn?1:0);
    }
    return result;
}
inline bool OwnedComponent(BYTE* component,BYTE* pawn,void* type) {
    return Live(component) && *reinterpret_cast<void**>(component+0xa0)==pawn && *reinterpret_cast<void**>(component+0x10)==type;
}
inline void EnsureAiDriver(BYTE* pawn,int slot) {
        using Type=void*(__fastcall*)(); using Find=BYTE*(__fastcall*)(void*,void*);
        auto aiType=reinterpret_cast<Type>(image+0x23bd880)();
        auto pathType=reinterpret_cast<Type>(image+0x22b1e10)();
        // Registration itself selects team components through the native
        // world's IsTeam predicate. Look up the matching mode-specific class.
        unsigned getter=TwinKartRace::BehaviorGetter(armedMode);if(!getter)return;
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
            }
        }
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
inline bool ForceFlagStartWhenReady(BYTE* rule,int slot,bool prepared) {
    __try {
        if(!prepared || !TwinKartRace::FlagMode(armedMode) || slot<1 || slot>7 || !Local(rule))return false;
        flagAiPreparedMask|=1u<<slot;
        if(flagAiPreparedMask!=0xfe || flagStartRequested)return flagStartRequested;
        auto world=World(rule);
        BYTE* state=world?*reinterpret_cast<BYTE**>(world+0x120):nullptr;
        using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
        auto waitType=reinterpret_cast<Type>(image+0x23b4a90)();
        if(!Live(state) || !waitType)return false;
        auto find=reinterpret_cast<Find>((*reinterpret_cast<void***>(state))[0x620/8]);
        BYTE* wait=find(state,waitType);
        if(!OwnedComponent(wait,state,waitType) || !InRaceWorld(wait))return false;
        if(*reinterpret_cast<BYTE*>(wait+0xc8)) {
            flagStartRequested=true;
            Emit("native_local_flag_player_wait_already_ready",-1,1);
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
        flagStartRequested=*reinterpret_cast<BYTE*>(wait+0xc8)!=0 || *reinterpret_cast<BYTE*>(wait+0xc9)!=0;
        Emit("native_local_flag_player_wait_forced",-1,flagStartRequested?1:0);
        return flagStartRequested;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        Emit("native_local_flag_player_wait_fault",slot,0);return false;
    }
}
inline void __fastcall OnUnknown(void* raw,void* rawPawn,int slot,bool flag) {
    auto rule=static_cast<BYTE*>(raw),pawn=static_cast<BYTE*>(rawPawn);
    bool local=Local(rule) && Live(pawn);
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
    equipmentPawn=local && slot==0?pawn:nullptr;
    BYTE* account=nullptr;equipmentReady=equipmentPawn && ReadEquipmentPreset(equipmentPreset,&account);
    equipmentBodyReady=equipmentReady && ReadCurrentEquipmentBody(account,equipmentBody);
    if(local && aiRosterEnabled && AiSlot(slot,*reinterpret_cast<int*>(rule+0x130),*reinterpret_cast<int*>(rule+0x138))) {
        equipmentPawn=pawn;equipmentReady=aiRoster.Get(slot,equipmentPreset);equipmentBodyReady=false;
        Emit("native_local_ai_roster_slot",slot,equipmentReady?1:0);
    }
    if(equipmentPawn && !equipmentReady)Emit("native_local_equipment_unavailable",slot,0);
    if(!slot && equipmentReady && !equipmentBodyReady)Emit("native_local_player_account_body_unavailable",0,0);
    __try {addUnknown(raw,rawPawn,slot,flag);}
    __finally {equipmentPawn=previousPawn;equipmentReady=previousReady;equipmentPreset=previousPreset;equipmentBodyReady=previousBodyReady;equipmentBody=previousBody;equipmentSlot=previousSlot;}
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
            bool prepared=PrepareFlagAi(pawn,slot);
            Emit("native_local_flag_ai_ready",slot,prepared?1:0);
            ForceFlagStartWhenReady(rule,slot,prepared);
        } else EnsureAiDriver(pawn,slot);
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
                if(TwinKartRace::FlagMode(armedMode))PrepareFlagAi(pawn,slot);else EnsureAiDriver(pawn,slot);++ais;
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
inline void __fastcall OnActivationBegin(void* rule) {PreparePhaseAi(rule,1);activationBegin(rule);}
inline void __fastcall OnAutoDrive(void* raw,bool enabled) {
    auto movement=static_cast<BYTE*>(raw);
    if(InRaceWorld(movement)) {
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
                Emit("native_local_followtg_preserved",-1,enabled?1:0);
                return;
            }
        }
    }
    autoDrive(raw,enabled);
}
inline void __fastcall OnResultInit(void* raw,const void* resultClass,const void* playerClass,const void* pointClass,void** provider,void** styler) {
    auto component=static_cast<BYTE*>(raw);
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
    // Prologue consists entirely of relocatable register/stack instructions.
    // Calls are patched only while this owned game's window thread is idle.
    if(memcmp(image+0x15acaf0,prefix,sizeof(prefix)) || memcmp(image+0x1973d10,registerPrefix,sizeof(registerPrefix)) ||
       memcmp(image+0x15f5d00,resultPrefix,sizeof(resultPrefix)) || memcmp(image+0x168da80,factoryPrefix,sizeof(factoryPrefix)) || memcmp(image+0x168db20,factoryPrefix,sizeof(factoryPrefix)) ||
       memcmp(image+0x168da30,factoryPrefix,sizeof(factoryPrefix)) || memcmp(image+0x168dad0,factoryPrefix,sizeof(factoryPrefix)) ||
       memcmp(image+0x15d1100,bindPrefix,sizeof(bindPrefix)) || memcmp(image+0x1952440,managerPrefix,sizeof(managerPrefix)) ||
       memcmp(image+0x193fe50,activationPrefix,sizeof(activationPrefix)) || memcmp(image+0x14d2660,splinePrefix,sizeof(splinePrefix)) ||
       memcmp(image+0x1638220,forceFlagPrefix,sizeof(forceFlagPrefix))) return false;
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
    installed=true; Emit("native_local_ai_hooks_installed",-1,1); return true;
}
inline void Arm(int mode) {
    raceWorld.Reset();armedMode=mode;armed=TwinKartRace::PlayableMode(mode);
    flagAiPreparedMask=0;flagStartRequested=false;
}
}
