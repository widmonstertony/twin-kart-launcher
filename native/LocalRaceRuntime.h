#pragma once
#include "OwnedCodePatch.h"
#include "DriftObjectIdentity.h"
#include "LocalRaceWorld.h"

namespace TwinKartLocalRace {
using Spawn=bool(__fastcall*)(void*,void*,int,void*,void**,bool*);
using AddUnknown=void(__fastcall*)(void*,void*,int,bool);
using AutoDrive=void(__fastcall*)(void*,bool);
using ResultInit=void(__fastcall*)(void*,const void*,const void*,const void*,void**,void**);
using Construct=void*(__fastcall*)(void*);
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
inline BYTE* registrationActor=nullptr;
inline LocalRaceWorld raceWorld;
inline TwinKartNative::OwnedCodePatch spawnPatch,statePatch,driverPatch,resultPatch;
inline TwinKartNative::OwnedCodePatch constructionPatches[5];
inline bool AiSlot(int slot,int players,int ais) {
    return players==1 && ais>=1 && ais<=7 && slot>=players && slot<players+ais;
}
inline bool Live(BYTE* object) {
    if(!object || (*reinterpret_cast<unsigned*>(object+8)&0x18000)) return false;
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
    int count=*reinterpret_cast<int*>(image+0x69c28c4),index=*reinterpret_cast<int*>(object+0xc);
    return chunks && index>=0 && index<count && count<=2000000 && chunks[index/65536] && chunks[index/65536][index%65536].object==object && !(chunks[index/65536][index%65536].flags&0x30000000);
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
        *reinterpret_cast<BYTE*>(actor+0x328)=armedMode==1?1:0;
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
    addUnknown(raw,rawPawn,slot,flag);
    // Native client setup owns the initial component population. Complete it
    // before asking the native server interface to supply a missing AI driver;
    // existing movement/animation components are reused by OnConstruct.
    if(local && Local(rule) && Live(pawn) && AiSlot(slot,*reinterpret_cast<int*>(rule+0x130),*reinterpret_cast<int*>(rule+0x138))) {
        using Type=void*(__fastcall*)(); using Find=BYTE*(__fastcall*)(void*,void*);
        auto aiType=reinterpret_cast<Type>(image+0x23bd880)();
        auto pathType=reinterpret_cast<Type>(image+0x22b1e10)();
        // Both supported launcher modes are solo races. A pre-existing path
        // alone does not establish that native speed/item behavior was added.
        auto behaviorType=reinterpret_cast<Type>(image+(armedMode==1?0x2218050:0x220fed0))();
        bool exactAi=*reinterpret_cast<void**>(pawn+0x10)==aiType;
        auto find=reinterpret_cast<Find>((*reinterpret_cast<void***>(pawn))[0x620/8]);
        BYTE* path=find(pawn,pathType);
        BYTE* behavior=find(pawn,behaviorType);
        Emit("native_local_ai_driver_setup_state",slot,(exactAi?1:0)|(Live(path)?2:0)|(Live(behavior)?4:0));
        if(exactAi && (!path || !behavior)) {
            RegisterMissing(pawn);
            path=find(pawn,pathType);
            behavior=find(pawn,behaviorType);
            Emit("native_local_ai_driver_registered",slot,Live(path)?1:0);
        }
        Emit("native_local_ai_mode_behavior_ready",slot,Live(behavior) && *reinterpret_cast<void**>(behavior+0xa0)==pawn &&
            *reinterpret_cast<void**>(behavior+0x10)==behaviorType?armedMode:0);
        if(exactAi && Live(path) && *reinterpret_cast<void**>(path+0xa0)==pawn &&
           *reinterpret_cast<void**>(path+0x10)==pathType) {
            auto movementType=reinterpret_cast<Type>(image+0x22f9030)();
            BYTE* movement=find(pawn,movementType);
            if(Live(movement) && *reinterpret_cast<void**>(movement+0xa0)==pawn &&
               *reinterpret_cast<void**>(movement+0x10)==movementType) {
                BYTE* current=*reinterpret_cast<BYTE**>(movement+0xf08);
                if(!current || current==path) {
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
}
inline void __fastcall OnAutoDrive(void* raw,bool enabled) {
    auto movement=static_cast<BYTE*>(raw);
    if(InRaceWorld(movement)) {
        BYTE* path=*reinterpret_cast<BYTE**>(movement+0xf08);
        if(Live(path)) {
            using Type=void*(__fastcall*)();
            void* follow=reinterpret_cast<Type>(image+0x22b1e10)();
            if(*reinterpret_cast<void**>(path+0x10)==follow) {
                Emit("native_local_followtg_preserved",-1,enabled?1:0);
                return;
            }
        }
    }
    autoDrive(raw,enabled);
}
inline void __fastcall OnResultInit(void* raw,const void* resultClass,const void* playerClass,const void* pointClass,void** provider,void** styler) {
    auto component=static_cast<BYTE*>(raw);
    if((armedMode==1 || armedMode==4) && InRaceWorld(component) && styler && !*styler) {
        // Original solo factories return native-owned TUniquePtr storage in
        // RDX; RCX is unused by these exact two implementations. The original
        // Init consumes the pointer and owns all ranking/time data and UI.
        using Factory=void*(__fastcall*)(const void*,void**);
        reinterpret_cast<Factory>(image+(armedMode==1?0x168da80:0x168db20))(nullptr,styler);
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
    // Prologue consists entirely of relocatable register/stack instructions.
    // Calls are patched only while this owned game's window thread is idle.
    if(memcmp(image+0x15acaf0,prefix,sizeof(prefix)) || memcmp(image+0x1973d10,registerPrefix,sizeof(registerPrefix)) ||
       memcmp(image+0x15f5d00,resultPrefix,sizeof(resultPrefix)) || memcmp(image+0x168da80,factoryPrefix,sizeof(factoryPrefix)) || memcmp(image+0x168db20,factoryPrefix,sizeof(factoryPrefix)) ||
       memcmp(image+0x15d1100,bindPrefix,sizeof(bindPrefix))) return false;
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
    installed=true; Emit("native_local_ai_hooks_installed",-1,1); return true;
}
inline void Arm(int mode) {raceWorld.Reset();armedMode=mode;armed=mode==1 || mode==4;}
}
