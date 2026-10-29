#pragma once
#include "OwnedCodePatch.h"
#include "DriftObjectIdentity.h"

namespace TwinKartLocalRace {
using Spawn=bool(__fastcall*)(void*,void*,int,void*,void**,bool*);
using AddUnknown=void(__fastcall*)(void*,void*,int,bool);
using AutoDrive=void(__fastcall*)(void*,bool);
using ResultInit=void(__fastcall*)(void*,const void*,const void*,const void*,void**,void**);
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
inline TwinKartNative::OwnedCodePatch spawnPatch,statePatch,driverPatch,resultPatch;
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
inline bool Local(BYTE* rule) {
    return armed && GetCurrentThreadId()==owner && Live(rule) &&
        *reinterpret_cast<void**>(rule)==image+0x51dd410 && World(rule);
}
inline void Emit(const char* name,int slot,int value) { if(event) event(name,slot,value); }
inline bool __fastcall OnSpawn(void* raw,void* mode,int slot,void* start,void** pawn,bool* isAi) {
    auto rule=static_cast<BYTE*>(raw);
    if(Local(rule) && isAi) {
        int players=*reinterpret_cast<int*>(rule+0x130),ais=*reinterpret_cast<int*>(rule+0x138);
        if(players==1 && ais>=1 && ais<=7 && slot>=0 && slot<players+ais) {
            *isAi=AiSlot(slot,players,ais);
            Emit("native_local_slot_classified",slot,*isAi?1:0);
        }
    }
    bool result=spawn(raw,mode,slot,start,pawn,isAi);
    if(Local(rule)) {
        Emit("native_local_pawn_spawned",slot,result && pawn && *pawn?1:0);
        if(result && pawn && *pawn && isAi && *isAi) {
            auto actor=static_cast<BYTE*>(*pawn);
            using Type=void*(__fastcall*)(); using Find=BYTE*(__fastcall*)(void*,void*);
            auto aiType=reinterpret_cast<Type>(image+0x23bd880)();
            auto pathType=reinterpret_cast<Type>(image+0x22b1e10)();
            auto find=reinterpret_cast<Find>((*reinterpret_cast<void***>(actor))[0x620/8]);
            // The PC client-only component route creates a display pawn with
            // no authoritative driver. The retained native server registration
            // supplies its own FollowTG, movement and speed/item components.
            // Its this pointer is the original interface subobject at +0x320.
            if(Live(actor) && *reinterpret_cast<void**>(actor+0x10)==aiType && !find(actor,pathType)) {
                using Register=void(__fastcall*)(void*);
                reinterpret_cast<Register>(image+0x1973d10)(actor+0x320);
                Emit("native_local_ai_driver_registered",slot,find(actor,pathType)?1:0);
            }
        }
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
}
inline void __fastcall OnAutoDrive(void* raw,bool enabled) {
    auto movement=static_cast<BYTE*>(raw);
    if(armed && GetCurrentThreadId()==owner && Live(movement) && World(movement)) {
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
    if(armed && (armedMode==1 || armedMode==4) && GetCurrentThreadId()==owner && Live(component) && World(component) && styler && !*styler) {
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
    // Prologue consists entirely of relocatable register/stack instructions.
    // Calls are patched only while this owned game's window thread is idle.
    if(memcmp(image+0x15acaf0,prefix,sizeof(prefix)) || memcmp(image+0x1973d10,registerPrefix,sizeof(registerPrefix)) ||
       memcmp(image+0x15f5d00,resultPrefix,sizeof(resultPrefix)) || memcmp(image+0x168da80,factoryPrefix,sizeof(factoryPrefix)) || memcmp(image+0x168db20,factoryPrefix,sizeof(factoryPrefix))) return false;
    spawn=reinterpret_cast<Spawn>(image+0x1926150);
    addUnknown=reinterpret_cast<AddUnknown>(image+0x18f7370);
    if(!spawnPatch.Call(image+0x1913d1a,reinterpret_cast<void*>(spawn),reinterpret_cast<void*>(OnSpawn))) return false;
    if(!statePatch.Call(image+0x1913d72,reinterpret_cast<void*>(addUnknown),reinterpret_cast<void*>(OnUnknown))) { spawnPatch.Restore(); return false; }
    if(!driverPatch.Entry(image+0x15acaf0,prefix,sizeof(prefix),reinterpret_cast<void*>(OnAutoDrive),reinterpret_cast<void**>(&autoDrive))) {
        statePatch.Restore(); spawnPatch.Restore(); return false;
    }
    if(!resultPatch.Entry(image+0x15f5d00,resultPrefix,sizeof(resultPrefix),reinterpret_cast<void*>(OnResultInit),reinterpret_cast<void**>(&resultInit))) {
        driverPatch.Restore(); statePatch.Restore(); spawnPatch.Restore(); return false;
    }
    installed=true; Emit("native_local_ai_hooks_installed",-1,1); return true;
}
inline void Arm(int mode) { armedMode=mode; armed=true; }
}
