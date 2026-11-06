#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <stdexcept>
#include "../native/LocalRaceRuntime.h"
#include "../native/RenderActivity.h"
#include "../native/NativeRacePhase.h"
#include "../native/NativeRenderScale.h"
float scaleFraction=2.5f;unsigned scaleWrites=0;bool refuseScale=false;int scaleVariable=0;
void __fastcall SetScale(void* variable,float value,unsigned priority) {
    if(variable!=&scaleVariable || priority!=0x08000000)throw std::runtime_error("Native cvar interface/priority changed");
    ++scaleWrites;if(!refuseScale)scaleFraction=value/100;
}
float __fastcall ReadScale(){return scaleFraction;}
using Function=int(__fastcall*)(int);
Function originalEntry=nullptr;
int __fastcall Replacement(int value) { return value+10; }
int __fastcall EntryReplacement(int value) { return originalEntry(value)+10; }
int __fastcall VirtualOriginal(void*) {return 3;}
int __fastcall VirtualReplacement(void*) {return 42;}
unsigned widgetCalls=0;void* shownWidget=nullptr;unsigned char shownVisibility=0;
int __fastcall WidgetOriginal(void* widget,unsigned char visibility){++widgetCalls;shownWidget=widget;shownVisibility=visibility;return 42;}
int __fastcall WidgetGuard(void* widget,unsigned char visibility) {
    if(!widget)return -1;
    using Show=int(__fastcall*)(void*,unsigned char);
    return reinterpret_cast<Show>((*static_cast<void***>(widget))[0x280/8])(widget,visibility);
}
using CounterPredicate=bool(__fastcall*)(void*);
CounterPredicate originalCounterPredicate=nullptr;bool counterReady=false;
bool __fastcall CounterReplacement(void* rule){return originalCounterPredicate(rule) && counterReady;}
void Need(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
int main() {
    try {
        TwinKartGraphics::NativeRenderScale scale;float observed=0;
        using ScaleResult=TwinKartGraphics::NativeRenderScale::Result;
        Need(scale.Apply(&scaleVariable,SetScale,ReadScale,observed)==ScaleResult::Disabled,"Disabled SR changed native scale");
        Need(!scale.Configure(L"100") && !scale.Configure(L"24.99") && !scale.Configure(L"nan") &&
            !scale.Configure(L"66.6 trailing") && !scale.Configure(L"66.6.6") && !scale.Configure(L"66,6"),"Invalid native scale accepted");
        Need(scale.Configure(L"66.666667"),"Valid native scale rejected");
        Need(scale.Apply(nullptr,SetScale,ReadScale,observed)==ScaleResult::NotReady && !scaleWrites,"Missing cvar was written");
        Need(scale.Apply(&scaleVariable,SetScale,ReadScale,observed)==ScaleResult::Applied && scaleWrites==1 && observed<1,"Native request failed to replace supersampled scene fraction");
        Need(scale.Apply(&scaleVariable,SetScale,ReadScale,observed)==ScaleResult::AlreadyApplied && scaleWrites==1,"Native cvar was rewritten unnecessarily");
        scaleFraction=2.5f;refuseScale=true;
        Need(scale.Apply(&scaleVariable,SetScale,ReadScale,observed)==ScaleResult::Overridden && scaleWrites==2 && observed==2.5f,"Higher-priority native override reported as applied");
        BYTE worldA[32]{},worldB[32]{};
        *reinterpret_cast<int*>(worldA+0xc)=0;*reinterpret_cast<int*>(worldB+0xc)=1;
        TwinKartCamera::ObjectSlot worldSlots[2]{};TwinKartCamera::ObjectSlot* worldChunks[]{worldSlots};
        worldSlots[0].object=worldA;worldSlots[0].serial=11;worldSlots[1].object=worldB;worldSlots[1].serial=12;
        TwinKartLocalRace::LocalRaceWorld scope;
        Need(scope.Bind({worldA,0,11},worldChunks,2) && scope.Matches(worldA,worldChunks,2),"Local race world binding");
        Need(!scope.Bind({worldB,1,12},worldChunks,2) && !scope.Matches(worldB,worldChunks,2),"Local overrides escaped to a different world");
        worldSlots[0].serial=13;
        Need(!scope.Matches(worldA,worldChunks,2) && !scope.Bind({worldA,0,13},worldChunks,2),"Recycled world address inherited local overrides");
        scope.Reset();Need(scope.Bind({worldB,1,12},worldChunks,2),"New local request could not bind its new world");
        worldSlots[1].flags=0x20000000;Need(!scope.Matches(worldB,worldChunks,2),"Destroyed world accepted");
        auto code=static_cast<BYTE*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
        Need(code!=nullptr,"Fixture allocation");
        // Original Windows x64 call frame with mandatory home space/alignment.
        const BYTE caller[]={0x48,0x83,0xec,0x28,0xe8,0,0,0,0,0x48,0x83,0xc4,0x28,0xc3};
        memcpy(code,caller,sizeof(caller)); const BYTE callee[]={0x8d,0x41,0x01,0xc3};
        memcpy(code+64,callee,sizeof(callee)); int relative=64-9; memcpy(code+5,&relative,4);
        BYTE prefix[15]{0x8b,0xc1,0x83,0xc0,0x01}; memset(prefix+5,0x90,10);
        memcpy(code+128,prefix,sizeof(prefix)); code[143]=0xc3;
        const BYTE indirectCaller[]{0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0xf9,0xff,0x97,0,0,0,0,0x48,0x83,0xc4,0x20,0x5f,0xc3};
        memcpy(code+256,indirectCaller,sizeof(indirectCaller));
        const BYTE counterPrefix[]{0x8b,0x41,0x60,0x39,0x41,0x64,0x0f,0x95,0xc0,0xc3,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc};
        memcpy(code+512,counterPrefix,sizeof(counterPrefix));
        const BYTE adjacentPredicate[]{0xb0,0x01,0xc3};memcpy(code+528,adjacentPredicate,sizeof(adjacentPredicate));
        const BYTE widgetCaller[]{0x48,0x83,0xec,0x28,0x48,0x8b,0xc1,0x4c,0x8b,0x00,0x41,0xff,0x90,0x80,0x02,0x00,0x00,0x48,0x83,0xc4,0x28,0xc3};
        memcpy(code+640,widgetCaller,sizeof(widgetCaller));
        // Push RBX and build native home space. With a live animation, continue
        // through its duration vcall; with none, skip to gameplay cleanup.
        const BYTE boostFixture[]{0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,
            0x48,0x8b,0x03,0x48,0x8b,0xcb,0xff,0x90,0x90,0x02,0x00,0x00,
            0x48,0x83,0xc4,0x20,0x5b,0xc3};
        memcpy(code+768,boostFixture,sizeof(boostFixture));
        const BYTE boostResume[]{0x49,0x8b,0xce,0xb8,0x07,0x00,0x00,0x00,0x48,0x83,0xc4,0x20,0x5b,0xc3};
        memcpy(code+776+0x162,boostResume,sizeof(boostResume));
        DWORD ignored=0; Need(VirtualProtect(code,4096,PAGE_EXECUTE_READ,&ignored)!=FALSE,"Fixture protection");
        FlushInstructionCache(GetCurrentProcess(),code,144);
        auto call=reinterpret_cast<Function>(code),entry=reinterpret_cast<Function>(code+128);
        Need(call(2)==3 && entry(2)==3,"Original generated machine code");
        TwinKartNative::OwnedCodePatch redirect;
        Need(!redirect.Call(code+4,code+63,reinterpret_cast<void*>(Replacement)),"Unknown call target accepted");
        Need(redirect.Call(code+4,code+64,reinterpret_cast<void*>(Replacement)) && call(2)==12,"Real patched call did not execute replacement");
        Need(redirect.Restore() && call(2)==3,"Call rollback did not restore execution");
        TwinKartNative::OwnedCodePatch detour;
        BYTE wrong[15]{};
        Need(!detour.Entry(code+128,wrong,15,reinterpret_cast<void*>(EntryReplacement),reinterpret_cast<void**>(&originalEntry)),"Unknown prologue accepted");
        Need(detour.Entry(code+128,prefix,15,reinterpret_cast<void*>(EntryReplacement),reinterpret_cast<void**>(&originalEntry)),"Entry install");
        Need(originalEntry(2)==3 && entry(2)==13,"Trampoline did not preserve displaced instructions");
        Need(detour.Restore() && entry(2)==3,"Entry rollback");
        auto indirect=reinterpret_cast<int(__fastcall*)(void*)>(code+256);
        void* virtualTable[]{reinterpret_cast<void*>(VirtualOriginal)};
        Need(indirect(virtualTable)==3,"Original indirect-call fixture");
        TwinKartNative::OwnedCodePatch indirectPatch;
        const BYTE virtualCall[]{0xff,0x97,0,0,0,0};
        Need(!indirectPatch.IndirectCall(code+264,wrong,6,reinterpret_cast<void*>(VirtualReplacement)),"Unknown indirect call accepted");
        Need(indirectPatch.IndirectCall(code+264,virtualCall,6,reinterpret_cast<void*>(VirtualReplacement)) && indirect(virtualTable)==42,
            "Six-byte indirect-call relay failed to preserve the real native call frame");
        Need(indirectPatch.Restore() && indirect(virtualTable)==3,"Indirect-call rollback did not restore all six bytes");
        alignas(8) BYTE counterState[0x68]{};auto counter=reinterpret_cast<CounterPredicate>(code+512);
        auto adjacent=reinterpret_cast<CounterPredicate>(code+528);
        Need(!counter(counterState) && adjacent(counterState),"Synthetic loading-counter ABI invalid");
        *reinterpret_cast<unsigned*>(counterState+0x64)=1;
        Need(counter(counterState),"Native-style counter difference did not signal loading completion");
        TwinKartNative::OwnedCodePatch counterPatch;
        Need(counterPatch.Entry(code+512,counterPrefix,sizeof(counterPrefix),reinterpret_cast<void*>(CounterReplacement),
            reinterpret_cast<void**>(&originalCounterPredicate)) && !counter(counterState) && adjacent(counterState),
            "Short returned predicate plus padding could not be gated without modifying adjacent code");
        counterReady=true;Need(counter(counterState),"Prepared local navigation did not preserve native-style counter completion");
        *reinterpret_cast<unsigned*>(counterState+0x60)=1;
        Need(!counter(counterState),"Readiness bypassed an incomplete native-style loading predicate");
        Need(counterPatch.Restore() && !memcmp(code+512,counterPrefix,sizeof(counterPrefix)) && !counter(counterState) && adjacent(counterState),
            "Short predicate restoration changed its counter semantics or adjacent code");
        originalCounterPredicate=nullptr;
        auto widgetCall=reinterpret_cast<int(__fastcall*)(void*,unsigned char)>(code+640);
        void* widgetTable[81]{};widgetTable[80]=reinterpret_cast<void*>(WidgetOriginal);void** widget=widgetTable;
        Need(widgetCall(&widget,2)==42 && widgetCalls==1 && shownWidget==&widget && shownVisibility==2,"Original widget display ABI");
        TwinKartNative::OwnedCodePatch widgetPatch;
        Need(!widgetPatch.WidgetVisibilityCall(code+648,reinterpret_cast<void*>(WidgetGuard)),"Misaligned widget display accepted");
        Need(widgetPatch.WidgetVisibilityCall(code+647,reinterpret_cast<void*>(WidgetGuard)) &&
             widgetCall(&widget,3)==42 && widgetCalls==2 && shownVisibility==3 && widgetCall(nullptr,2)==-1 && widgetCalls==2,
             "Widget guard did not preserve live display or dereferenced the absent factory result");
        Need(widgetPatch.Restore() && !memcmp(code+647,widgetCaller+7,10) && widgetCall(&widget,4)==42 && widgetCalls==3,
             "Widget guard restoration did not restore all ten original bytes");
        TwinKartNative::OwnedCodePatch boostAnimationPatch;
        void* animationTable[83]{};animationTable[82]=reinterpret_cast<void*>(VirtualOriginal);void** animation=animationTable;
        auto boostAnimation=reinterpret_cast<int(__fastcall*)(void*)>(code+768);
        Need(!boostAnimationPatch.ChaserBoosterAnimationGuard(code+777) && boostAnimation(&animation)==3,
            "Chaser animation guard accepted a nonmatching instruction boundary");
        Need(boostAnimationPatch.ChaserBoosterAnimationGuard(code+776) && boostAnimation(&animation)==3 && boostAnimation(nullptr)==7,
            "Chaser animation guard failed to preserve the live duration call or resume after the missing animation");
        Need(boostAnimationPatch.Restore() && !memcmp(code+776,boostFixture+8,12) && boostAnimation(&animation)==3,
            "Chaser animation guard restoration did not restore the native duration call");
        for(int ais=1;ais<=7;++ais) for(int slot=-1;slot<10;++slot)
            Need(TwinKartLocalRace::AiSlot(slot,1,ais)==(slot>=1 && slot<=ais),"AI slot boundary");
        Need(!TwinKartLocalRace::AiSlot(1,2,3) && !TwinKartLocalRace::AiSlot(1,1,8),"Unsupported player count accepted");
        TwinKartGraphics::RenderActivity activity;
        Need(!activity.Active(2000),"Unpublished scene treated as racing");activity.Publish(true,2000);
        Need(activity.Active(2000) && activity.Active(3000) && !activity.Active(3001) && !activity.Active(1999),"Stale/loading/invalid-clock scene activity accepted");
        activity.Publish(false,2500);Need(!activity.Active(2500),"Paused/menu scene retained racing state");
        int phaseClass=0,transitionClass=0;
        auto phase=[&](const void* type,unsigned long long name,bool paused=false){return TwinKartGraphics::ClassifyNativeRacePhase(type,&phaseClass,name,100,200,paused);};
        Need(phase(&phaseClass,100) && phase(&phaseClass,200),"Native BeginRace/retirement phase rejected");
        Need(!phase(&transitionClass,100),"A transition rule object was accepted as CurrentPhase");
        Need(!phase(&phaseClass,300) && !phase(&phaseClass,0) && !phase(&phaseClass,100,true) && !phase(nullptr,100),"Menu/countdown/result/invalid/paused scene accepted");
        Need(!TwinKartGraphics::ClassifyNativeRacePhase(&phaseClass,&phaseClass,100,100,100,false) &&
             !TwinKartGraphics::ClassifyNativeRacePhase(&phaseClass,&phaseClass,100,100,0,false),"Uninitialized/aliased native FName globals accepted");
        Need(TwinKartGraphics::ClassifyNativeDrivingPhase(&phaseClass,&phaseClass,100,100,false),"BeginRace rejected as cockpit driving phase");
        Need(TwinKartGraphics::ScenePhaseComponentGetter(100,true)==0x2431330 &&
             TwinKartGraphics::ScenePhaseComponentGetter(100,false)==TwinKartGraphics::RacePhaseComponentGetter &&
             TwinKartGraphics::ScenePhaseComponentGetter(104,false)==0x22ac6a0 &&
             TwinKartGraphics::ScenePhaseComponentGetter(4,true)==TwinKartGraphics::RacePhaseComponentGetter,
             "Factory render/cockpit gate selected the ordinary phase or escaped its mode/world lease");
        Need(TwinKartGraphics::ScenePhaseComponentGetter(108,false,true)==0x2250f20 &&
             TwinKartGraphics::ScenePhaseComponentGetter(108,true,false)==TwinKartGraphics::RacePhaseComponentGetter &&
             TwinKartGraphics::ScenePhaseComponentGetter(4,false,true)==TwinKartGraphics::RacePhaseComponentGetter &&
             TwinKartGraphics::ChaserGameNameRva==0x68b9e50,
             "Chaser cockpit phase getter escaped its explicit mode/world lease");
        Need(!TwinKartGraphics::ClassifyNativeDrivingPhase(&phaseClass,&phaseClass,200,100,false) &&
             !TwinKartGraphics::ClassifyNativeDrivingPhase(&phaseClass,&phaseClass,100,100,true) &&
             !TwinKartGraphics::ClassifyNativeDrivingPhase(&transitionClass,&phaseClass,100,100,false),"Retirement/paused/transition phase accepted for cockpit");
        VirtualFree(code,0,MEM_RELEASE);
        puts("PASS real x64 call redirection/trampoline, local-world serial lifetime, native scale setter/readback contract and override rejection, AI slots, native phase gating and expiry. No game started.");
        return 0;
    } catch(const std::exception& error) { fprintf(stderr,"%s\n",error.what()); return 1; }
}
