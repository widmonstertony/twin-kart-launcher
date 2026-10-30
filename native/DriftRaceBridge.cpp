#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_set>
#include <stdexcept>
#include "ClientBuildGuard.h"
#include "GameThreadQueue.h"
#include "DriftObjectIdentity.h"
#include "RaceControl.h"
#include "LocalRaceRuntime.h"
#include "RenderActivity.h"
#include "NativeRacePhase.h"
#include "NativeRenderScale.h"
#include "NativeTrackModes.h"
#include "NativeTrackCatalog.h"
#include "NativeReturnRoute.h"

// Original game-thread bridge for exact-build native race preparation work.
// Track lookup and an opt-in native local-host candidate. A travel request is
// not evidence of AI driving, a completed race or native settlement.
namespace {
constexpr char ClientSha[]="5041d9f5d45adb4b1c932d8056a89f70db7fa98f93614ac30685cd1adeb9d9c2";
BYTE* image=nullptr; HINSTANCE module=nullptr; HANDLE output=INVALID_HANDLE_VALUE;
TwinKartNative::GameThreadQueue queue;
TwinKartGraphics::RenderActivity renderActivity;
TwinKartGraphics::RenderActivity sceneActivity;
TwinKartGraphics::RenderActivity cockpitDrivingActivity;
volatile LONG renderActivityPending=0;
TwinKartGraphics::NativeRenderScale renderScale;
int lastScaleResult=-1;
struct NativeString { const wchar_t* data; int count,capacity; };
struct Query { std::wstring alias; wchar_t level[512]{}; unsigned modeMask=0;int laps=0,status=-1; DWORD executedThread=0; HANDLE complete=nullptr; };
struct StartQuery { TwinKartRace::Request request; wchar_t track[97]{},options[512]{};unsigned modeMask=0;int status=-1,stage=0; DWORD fault=0; uintptr_t faultRva=0; DWORD executedThread=0; HANDLE complete=nullptr; };
struct CatalogQuery {TwinKartRace::NativeTrackEntry* entries=nullptr;int capacity=0,status=-1;TwinKartRace::NativeTrackEnumeration summary;DWORD executedThread=0;HANDLE complete=nullptr;};
HWND gameWindow=nullptr;
volatile LONG exitPending=0;
TwinKartLocalRace::LocalRaceWorld exitWorld;
// All object access remains inside this game's window thread. No object pointer
// survives a queued task or a level transition.
BYTE* Traveler() {
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
    int count=*reinterpret_cast<int*>(image+0x69c28c4);
    if(!chunks || count<1 || count>2000000) return nullptr;
    BYTE* found=nullptr;
    for(int index=0;index<count;++index) {
        auto chunk=chunks[index/65536]; if(!chunk) continue;
        auto& slot=chunk[index%65536]; auto object=static_cast<BYTE*>(slot.object);
        if(!object || (slot.flags&0x30000000) || *reinterpret_cast<void**>(object)!=image+0x52618c0 ||
           (*reinterpret_cast<unsigned*>(object+8)&0x18030) || *reinterpret_cast<int*>(object+0xc)!=index) continue;
        if(found) return nullptr; // Never pick an ambiguous subsystem instance.
        found=object;
    }
    return found;
}
void UpdateRenderActivity(void*) {
    bool active=false,sceneActive=false,cockpitDriving=false;
    __try {
        if(GetCurrentThreadId()==queue.ThreadId()) {
            if(renderScale.Enabled()) {
                // Exact PC registration of r.ScreenPercentage stores its
                // IConsoleVariable at RVA692c2d8. The native game settings call
                // this same float setter; 400fa80 reads the effective fraction.
                // This requests a genuine lower native scene, never a resized
                // copy of a full-resolution color image.
                float fraction=0;
                auto result=renderScale.Apply(*reinterpret_cast<void**>(image+0x692c2d8),
                    reinterpret_cast<TwinKartGraphics::NativeRenderScale::SetFloat>(image+0x1047740),
                    reinterpret_cast<TwinKartGraphics::NativeRenderScale::GetFraction>(image+0x400fa80),fraction);
                int code=static_cast<int>(result);
                if(code!=lastScaleResult) {
                    char row[320];int bytes=sprintf_s(row,"{\"event\":\"native_render_scale_request\",\"result\":%d,\"requested_percent\":%.6f,\"effective_fraction\":%.6f,\"game_thread\":%lu,\"scene_dimensions_verified\":false}\r\n",
                        code,renderScale.Requested(),std::isfinite(fraction)?fraction:0.0f,GetCurrentThreadId());
                    DWORD written=0;if(bytes>0)WriteFile(output,row,static_cast<DWORD>(bytes),&written,nullptr);
                    lastScaleResult=code;
                }
            }
            BYTE* traveler=Traveler();using GetWorld=BYTE*(__fastcall*)(void*);
            BYTE* world=traveler?reinterpret_cast<GetWorld>((*reinterpret_cast<void***>(traveler))[0x160/8])(traveler):nullptr;
            if(TwinKartLocalRace::Live(world) && !*reinterpret_cast<void**>(world+0x38)) {
                // Lobby, garage and results have live 3D worlds without a
                // BeginRace phase. Rendering eligibility is separate from
                // race eligibility; the renderer still requires current-frame
                // depth/motion/camera and real simulation/present markers.
                using Paused=bool(__fastcall*)(void*);
                sceneActive=!reinterpret_cast<Paused>(image+0x4011c70)(world);
                BYTE* state=*reinterpret_cast<BYTE**>(world+0x120);
                if(TwinKartLocalRace::Live(state)) {
                    using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
                    auto phaseType=reinterpret_cast<Type>(image+TwinKartGraphics::RacePhaseComponentGetter)();
                    auto find=reinterpret_cast<Find>((*reinterpret_cast<void***>(state))[0x620/8]);
                    BYTE* component=find(state,phaseType);
                    if(TwinKartLocalRace::Live(component) && *reinterpret_cast<void**>(component+0xa0)==state) {
                        // CurrentPhase is a UPhase. BeginRace is its native
                        // FName, while UTransition_BeginRace is a rule object
                        // inside it. Comparing those two classes never matches.
                        BYTE* phase=*reinterpret_cast<BYTE**>(component+TwinKartGraphics::CurrentPhaseOffset);
                        auto phaseClass=reinterpret_cast<Type>(image+TwinKartGraphics::PhaseObjectGetter)();
                        using Paused=bool(__fastcall*)(void*);
                        if(TwinKartLocalRace::Live(phase)) {
                            auto actualClass=*reinterpret_cast<void**>(phase+0x10);
                            auto currentName=*reinterpret_cast<unsigned long long*>(phase+TwinKartGraphics::PhaseNameOffset);
                            auto beginName=*reinterpret_cast<unsigned long long*>(image+TwinKartGraphics::BeginRaceNameRva);
                            bool paused=reinterpret_cast<Paused>(image+0x4011c70)(world);
                            active=TwinKartGraphics::ClassifyNativeRacePhase(actualClass,phaseClass,currentName,beginName,
                                *reinterpret_cast<unsigned long long*>(image+TwinKartGraphics::RetireCountdownNameRva),paused);
                            cockpitDriving=TwinKartGraphics::ClassifyNativeDrivingPhase(actualClass,phaseClass,currentName,beginName,paused);
                        }
                    }
                }
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {active=false;sceneActive=false;cockpitDriving=false;}
    sceneActivity.Publish(sceneActive,GetTickCount64());
    cockpitDrivingActivity.Publish(cockpitDriving,GetTickCount64());
    renderActivity.Publish(active,GetTickCount64());InterlockedExchange(&renderActivityPending,0);
}
void CancelRenderActivity(void*){sceneActivity.Publish(false,0);cockpitDrivingActivity.Publish(false,0);renderActivity.Publish(false,0);InterlockedExchange(&renderActivityPending,0);}
// 0=model not ready; 1=native travel submitted; 2=definition absent;
// -1=native fault; no value means race acceptance.
void RuntimeEvent(const char* name,int slot,int value) {
    char line[256]; int bytes=sprintf_s(line,"{\"event\":\"%s\",\"slot\":%d,\"value\":%d,\"race_verified\":false}\r\n",name,slot,value);
    DWORD written=0; if(bytes>0) WriteFile(output,line,static_cast<DWORD>(bytes),&written,nullptr);
}
BYTE* OwnedRaceWorld() {
    if(GetCurrentThreadId()!=queue.ThreadId() || !TwinKartLocalRace::armed)return nullptr;
    BYTE* traveler=Traveler();if(!traveler)return nullptr;
    using GetWorld=BYTE*(__fastcall*)(void*);
    BYTE* world=reinterpret_cast<GetWorld>((*reinterpret_cast<void***>(traveler))[0x160/8])(traveler);
    return TwinKartLocalRace::raceWorld.Matches(world,
        *reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),*reinterpret_cast<int*>(image+0x69c28c4))?world:nullptr;
}
bool RaceInput(const MSG& message) {
    if(message.wParam!=VK_ESCAPE || (message.message!=WM_KEYDOWN && message.message!=WM_KEYUP))return false;
    DWORD foreground=0;GetWindowThreadProcessId(GetForegroundWindow(),&foreground);
    if(foreground!=GetCurrentProcessId())return false;
    __try {
        if(!OwnedRaceWorld())return false;
        if(message.message==WM_KEYDOWN && !(message.lParam&(1LL<<30)) &&
            InterlockedCompareExchange(&exitPending,0,0)==0) {
            exitWorld=TwinKartLocalRace::raceWorld;
            InterlockedExchange(&exitPending,1);
        }
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
void CancelExit(void*){InterlockedExchange(&exitPending,0);}
void ExitRace(void*) {
    int result=-1;
    __try {
        BYTE* world=OwnedRaceWorld();BYTE* traveler=Traveler();
        const BYTE signature[]{0x48,0x89,0x5c,0x24,0x18,0x48,0x89,0x54,0x24,0x10};
        if(world && traveler && exitWorld.Matches(world,
            *reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),*reinterpret_cast<int*>(image+0x69c28c4)) &&
            TwinKartRace::HasReturnRoute(*reinterpret_cast<TwinKartRace::ReturnString*>(traveler+0xd8)) &&
            !memcmp(image+0x1bbc280,signature,sizeof(signature))) {
            // Original result exit at 1d06820 passes an empty FString as its
            // second argument. Reuse that native cleanup/travel path. Never
            // terminate the process or submit a fabricated completed result.
            TwinKartRace::ReturnString options{};
            using Return=void(__fastcall*)(void*,TwinKartRace::ReturnString*);
            reinterpret_cast<Return>(image+0x1bbc280)(traveler,&options);
            if(options.data)reinterpret_cast<void(__fastcall*)(void*)>(image+0x248b950)(options.data);
            TwinKartLocalRace::armed=false;TwinKartLocalRace::raceWorld.Reset();
            result=1;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {result=-2;}
    RuntimeEvent("native_mid_race_return_submitted",-1,result);
    InterlockedExchange(&exitPending,0);
}
void CheckExitPrompt() {
    if(InterlockedCompareExchange(&exitPending,2,1)!=1)return;
    // The prompt runs on this worker, never inside a hook or a UObject call.
    // Native race simulation continues; this is an exit confirmation, not a
    // claim that the original multiplayer pause rules have been changed.
    int answer=MessageBoxW(gameWindow,
        L"\u9000\u51fa\u5f53\u524d\u6bd4\u8d5b\u5e76\u8fd4\u56de\u5927\u5385\uff1f\u672c\u573a\u4e0d\u4f1a\u8bb0\u5f55\u4e3a\u5b8c\u8d5b\u3002\n\n"
        L"\u786e\u8ba4\u671f\u95f4\u6bd4\u8d5b\u4ecd\u4f1a\u7ee7\u7eed\u3002\u5207\u6362\u89c6\u89d2\u53ef\u6309 F6\u3002",
        L"\u9000\u51fa\u6bd4\u8d5b",MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2);
    if(answer!=IDYES || !queue.Submit({ExitRace,nullptr,CancelExit}))CancelExit(nullptr);
}
int AuditedBelonging(const std::wstring& identifier,void* context) {
    int result=TwinKartRace::RegisteredAiAsset(identifier,context);
    // Resource identifiers only, once per process. This produces a private
    // build-specific availability audit for garage compatibility, without
    // conflating an unlocked catalog row with a loadable native model.
    static std::unordered_set<std::wstring> recorded;
    if(recorded.size()<10000 && recorded.insert(identifier).second) {
        char id[128]{};bool safe=identifier.size()<sizeof(id);
        for(size_t i=0;safe && i<identifier.size();++i) {
            wchar_t c=identifier[i];
            safe=(c>=L'A' && c<=L'Z') || (c>=L'a' && c<=L'z') || (c>=L'0' && c<=L'9') || c==L':' || c==L'_';
            if(safe)id[i]=static_cast<char>(c);
        }
        if(safe) {
            char row[256];int size=sprintf_s(row,"{\"event\":\"native_belonging_availability\",\"identifier\":\"%s\",\"registered\":%d,\"dependent_mesh_loaded\":false}\r\n",id,result);
            DWORD written=0;if(size>0)WriteFile(output,row,size,&written,nullptr);
        }
    }
    return result;
}
BYTE* NativeTrackSelection(BYTE* account) {
    if(!TwinKartLocalRace::Live(account) || GetCurrentThreadId()!=queue.ThreadId())return nullptr;
    using Type=void*(__fastcall*)();auto type=reinterpret_cast<Type>(image+0x2210ea0)();
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
    int count=*reinterpret_cast<int*>(image+0x69c28c4);BYTE* selected=nullptr;
    if(!type || !chunks || count<1 || count>2000000)return nullptr;
    for(int index=0;index<count;++index) {
        auto chunk=chunks[index/65536];if(!chunk || (chunk[index%65536].flags&0x30000000))continue;
        auto object=static_cast<BYTE*>(chunk[index%65536].object);
        if(!TwinKartLocalRace::Live(object) || (*reinterpret_cast<unsigned*>(object+8)&0x18030) ||
           *reinterpret_cast<void**>(object+0x10)!=type || *reinterpret_cast<BYTE**>(object+0x20)!=account)continue;
        if(selected)return nullptr;selected=object;
    }
    return selected;
}
void InitializeRuntime(void* raw) {
    auto& value=*static_cast<Query*>(raw); value.executedThread=GetCurrentThreadId();
    bool ready=TwinKartLocalRace::Install(image,RuntimeEvent);
    // Keep the original lobby RegisterMatch path intact. The local gateway
    // converts that request into the original room/setup surface, so map and
    // team selection plus the loading screen remain inside the game UI.
    value.status=ready?1:-1; SetEvent(value.complete);
}
int NativeFault(EXCEPTION_POINTERS* error,StartQuery& query) {
    query.fault=error->ExceptionRecord->ExceptionCode;
    auto address=reinterpret_cast<uintptr_t>(error->ExceptionRecord->ExceptionAddress),base=reinterpret_cast<uintptr_t>(image);
    query.faultRva=address>=base && address-base<0x8657000?address-base:0;
    return EXCEPTION_EXECUTE_HANDLER;
}
int NativeStart(StartQuery& query) {
    const wchar_t* track=query.track; const wchar_t* options=query.options; int mode=query.request.mode;
    __try {
        query.stage=1;
        BYTE* traveler=Traveler();
        using Model=void*(__fastcall*)(void*);
        if(!traveler || !reinterpret_cast<Model>(image+0x1ba3cf0)(traveler)) return 0;
        // A delayed room packet must never replace an active local race.
        if(OwnedRaceWorld())return -9;
        // Resolve the exact requested alias. A suffix alone is not evidence
        // that this native build lacks the variant; never strip or remap it.
        NativeString alias{track,static_cast<int>(wcslen(track)+1),static_cast<int>(wcslen(track)+1)};
        alignas(16) BYTE definition[0x160]{};
        using Get=void*(__fastcall*)(void*,const NativeString*);
        using Destroy=void(__fastcall*)(void*);
        query.stage=2;
        if(reinterpret_cast<Get>(image+0x1472ce0)(definition,&alias)!=definition) return -1;
        unsigned long long levelName=0;
        __try {
            const auto level=*reinterpret_cast<NativeString*>(definition);
            if(!level.data || level.count<=1 || level.count>512 || level.capacity<level.count || level.data[level.count-1])return 2;
            bool validModes=TwinKartRace::ReadNativeTrackModeMask(definition,sizeof(definition),query.modeMask);
            if(!validModes || !TwinKartRace::NativeTrackSupports(query.modeMask,mode))return validModes?3:-5;
            using Name=void*(__fastcall*)(void*,const wchar_t*,int);
            reinterpret_cast<Name>(image+0x2589130)(&levelName,level.data,1);
        } __finally {reinterpret_cast<Destroy>(image+0x1620210)(definition);}
        query.stage=3;
        if(!TwinKartLocalRace::Install(image,RuntimeEvent)) return -3;
        // Native Major-connect preparation (1bb8150) saves World.URL.Map
        // into Traveler.ReturnURL before travel. Direct local loading bypasses
        // that caller; RaceReturnToLobby (1bbc280) rejects an empty ReturnURL.
        using GetWorld=BYTE*(__fastcall*)(void*);
        BYTE* lobbyWorld=reinterpret_cast<GetWorld>((*reinterpret_cast<void***>(traveler))[0x160/8])(traveler);
        if(!TwinKartLocalRace::Live(lobbyWorld) || *reinterpret_cast<void**>(lobbyWorld+0x38))return -6;
        auto& returnRoute=*reinterpret_cast<TwinKartRace::ReturnString*>(traveler+0xd8);
        const auto& lobbyMap=*reinterpret_cast<const TwinKartRace::ReturnString*>(lobbyWorld+0x4a8);
        bool inRace=TwinKartLocalRace::raceWorld.Matches(lobbyWorld,
            *reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),*reinterpret_cast<int*>(image+0x69c28c4));
        int routeResult=inRace?(TwinKartRace::HasReturnRoute(returnRoute)?1:-1):
            TwinKartRace::PrepareReturnRoute(returnRoute,lobbyMap,true,
                reinterpret_cast<TwinKartRace::AssignReturnString>(image+0x9118f0));
        RuntimeEvent("native_local_lobby_return_route",-1,routeResult);
        if(routeResult<0)return -6; // Never enter another race with no return route.
        // Launcher selection also updates the native picker using owned FString
        // storage, so the next original solo button uses the same selected map.
        TwinKartEquipment::Preset player;BYTE* playerAccount=nullptr;
        if(TwinKartLocalRace::ReadEquipmentPreset(player,&playerAccount)) {
            auto selection=NativeTrackSelection(playerAccount);
            if(selection) {
                auto& destination=*reinterpret_cast<TwinKartRace::ReturnString*>(selection+0x98);
                if(!TwinKartRace::ReturnStringShape(destination))return -8;
                TwinKartRace::ReturnString chosen{const_cast<wchar_t*>(track),static_cast<int>(wcslen(track)+1),static_cast<int>(wcslen(track)+1)};
                reinterpret_cast<TwinKartRace::AssignReturnString>(image+0x9118f0)(&destination,&chosen);
            }
        }
        if(TwinKartLocalRace::aiRosterEnabled) {
            TwinKartEquipment::Preset human;
            TwinKartRace::AiAssetLookup assets;unsigned characters=0,karts=0;
            if(!TwinKartLocalRace::ReadEquipmentPreset(human) || !TwinKartRace::BeginAiAssetLookup(image,assets))return -7;
            TwinKartRace::AiRoster::AvailabilityStats stats;
            bool prepared=TwinKartLocalRace::aiRoster.Prepare(query.request.ai,human,AuditedBelonging,&assets,&characters,&karts,&stats);
            for(int field=0;field<6;++field)RuntimeEvent("native_local_ai_character_field_available",field,static_cast<int>(stats.characterFields[field]));
            RuntimeEvent("native_local_ai_optional_motions_cleared",-1,static_cast<int>(stats.clearedMotions));
            RuntimeEvent("native_local_ai_registered_characters",-1,static_cast<int>(characters));
            RuntimeEvent("native_local_ai_registered_karts",-1,static_cast<int>(karts));
            if(!prepared)return -7;
            RuntimeEvent("native_local_ai_roster_randomized",-1,query.request.ai);
        }
        TwinKartLocalRace::Arm(mode);
        // The single-player preparation path fabricates time-attack details
        // and is inappropriate for native major-connect speed/item hosts.
        // Retain the original loading/transition path, as established by the
        // owner's working iOS client and this PC function's native callers.
        const wchar_t* modeText=TwinKartRace::ModeName(mode);
        NativeString modeName{modeText,static_cast<int>(wcslen(modeText)+1),static_cast<int>(wcslen(modeText)+1)};
        NativeString optionText{options,static_cast<int>(wcslen(options)+1),static_cast<int>(wcslen(options)+1)};
        using Load=bool(__fastcall*)(void*,const NativeString*,const NativeString*,unsigned long long,bool,const NativeString*,bool);
        query.stage=4;
        return reinterpret_cast<Load>(image+0x1bc6120)(traveler,&modeName,&alias,levelName,true,&optionText,true)?1:-4;
    } __except(NativeFault(GetExceptionInformation(),query)) { return -1; }
}
void ExecuteStart(void* raw) {
    auto& value=*static_cast<StartQuery*>(raw); value.executedThread=GetCurrentThreadId();
    value.status=NativeStart(value); SetEvent(value.complete);
}
void CancelStart(void* raw) { auto& value=*static_cast<StartQuery*>(raw); value.status=-1; SetEvent(value.complete); }
// 0=not ready, 1=valid native name, 2=empty native definition, -1=fault/cancel.
int NativeTrack(const wchar_t* name,wchar_t* level,unsigned& modeMask,int& laps) {
    modeMask=0;laps=0;
    __try {
        using Root=void*(__fastcall*)();
        if(!reinterpret_cast<Root>(image+0x1449110)()) return 0;
        size_t length=wcslen(name); NativeString alias{name,static_cast<int>(length+1),static_cast<int>(length+1)};
        alignas(16) BYTE record[0x160]{};
        using Get=void*(__fastcall*)(void*,const NativeString*);
        using Destroy=void(__fastcall*)(void*);
        void* returned=reinterpret_cast<Get>(image+0x1472ce0)(record,&alias);
        int result=-1;
        __try { if(returned==record) {
            auto value=reinterpret_cast<const NativeString*>(record);
            if(value->count==0) result=2;
            else if(value->data && value->count>0 && value->count<=512 && value->capacity>=value->count && value->data[value->count-1]==0) {
                memcpy(level,value->data,static_cast<size_t>(value->count)*sizeof(wchar_t)); result=value->count>1?1:2;
                if(result==1) {
                    if(!TwinKartRace::ReadNativeTrackModeMask(record,sizeof(record),modeMask))result=-1;
                    memcpy(&laps,record+0x10,sizeof(laps));if(!TwinKartRace::ValidNativeTrackLaps(laps))result=-1;
                }
            }
        }} __finally {reinterpret_cast<Destroy>(image+0x1620210)(record);}
        // Native copy constructor/default initialization and destructor use the
        // client's allocator/reference-count rules. No shallow struct clone.
        return result;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return -1; }
}
void Execute(void* raw) {
    auto& value=*static_cast<Query*>(raw); value.executedThread=GetCurrentThreadId();
    value.status=NativeTrack(value.alias.c_str(),value.level,value.modeMask,value.laps); SetEvent(value.complete);
}
void Cancel(void* raw) { auto& value=*static_cast<Query*>(raw); value.status=-1; SetEvent(value.complete); }
void ExecuteCatalog(void* raw) {
    auto& value=*static_cast<CatalogQuery*>(raw);value.executedThread=GetCurrentThreadId();
    __try {
        using Root=void*(__fastcall*)();auto root=static_cast<BYTE*>(reinterpret_cast<Root>(image+0x1449110)());
        value.status=!root?0:!TwinKartLocalRace::Live(root)?-1:TwinKartRace::EnumerateNativeTracks(root,
            reinterpret_cast<TwinKartRace::GetNativeTrackRows>(image+0x145a4a0),
            reinterpret_cast<TwinKartRace::FreeNativeTrackRows>(image+0x248b950),value.entries,value.capacity,value.summary);
    } __except(EXCEPTION_EXECUTE_HANDLER){value.status=-1;value.summary={};}
    SetEvent(value.complete);
}
void CancelCatalog(void* raw){auto& value=*static_cast<CatalogQuery*>(raw);value.status=-1;SetEvent(value.complete);}
bool SignaturesReady() {
    __try {
        const BYTE get[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74};
        const BYTE destroy[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74};
        const BYTE initialize[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c};
        const BYTE prepare[]={0x48,0x89,0x5c,0x24,0x18,0x55,0x56,0x57};
        const BYTE name[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83};
        const BYTE open[]={0x48,0x89,0x54,0x24,0x10,0x55,0x53,0x56};
        const BYTE loading[]={0x4c,0x89,0x4c,0x24,0x20,0x4c,0x89,0x44};
        const BYTE scaleSet[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x30};
        const BYTE scaleGet[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20};
        const BYTE rows[]={0x40,0x55,0x53,0x56,0x57,0x41,0x56,0x48,0x8b,0xec,0x48,0x83,0xec,0x50};
        const BYTE assign[]={0x40,0x55,0x56,0x41,0x55,0x48,0x83,0xec,0x30,0x8b,0x42,0x08};
        const BYTE trackClass[]={0x4c,0x8b,0xdc,0x48,0x81,0xec,0x88,0x00,0x00,0x00};
        return !memcmp(image+0x1472ce0,get,8) && !memcmp(image+0x1620210,destroy,8) && !memcmp(image+0x1bba140,initialize,8) &&
            !memcmp(image+0x1bba570,prepare,8) && !memcmp(image+0x2589130,name,8) && !memcmp(image+0x3f50670,open,8) && !memcmp(image+0x1bc6120,loading,8) &&
            !memcmp(image+0x145a4a0,rows,sizeof(rows)) &&
            !memcmp(image+0x9118f0,assign,sizeof(assign)) &&
            !memcmp(image+0x2210ea0,trackClass,sizeof(trackClass)) &&
            (!renderScale.Enabled() || (!memcmp(image+0x1047740,scaleSet,sizeof(scaleSet)) && !memcmp(image+0x400fa80,scaleGet,sizeof(scaleGet))));
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
BOOL CALLBACK FindWindow(HWND window,LPARAM raw) {
    DWORD process=0; GetWindowThreadProcessId(window,&process);
    if(process!=GetCurrentProcessId()) return TRUE;
    wchar_t name[64]{}; GetClassNameW(window,name,ARRAYSIZE(name));
    if(wcscmp(name,L"UnrealWindow")) return TRUE;
    *reinterpret_cast<HWND*>(raw)=window; return FALSE;
}
std::string Utf8(const wchar_t* value) {
    int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value,-1,nullptr,0,nullptr,nullptr);
    if(size<=0) throw std::runtime_error("Invalid native text");
    std::string result(static_cast<size_t>(size),'\0');
    if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value,-1,&result[0],size,nullptr,nullptr)) throw std::runtime_error("Native text conversion failed");
    result.pop_back(); return result;
}
std::string Json(const std::string& value) {
    std::string result="\"";
    for(unsigned char ch:value) {
        if(ch=='"' || ch=='\\') { result+='\\'; result+=static_cast<char>(ch); }
        else if(ch<32) { char code[7]; sprintf_s(code,"\\u%04x",ch); result+=code; }
        else result+=static_cast<char>(ch);
    }
    return result+'"';
}
void Log(const std::string& line) {
    std::string bytes=line+"\r\n"; DWORD written=0;
    if(!WriteFile(output,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr) || written!=bytes.size()) throw std::runtime_error("Bridge log write failed");
}
std::vector<std::string> Queries(const wchar_t* path) {
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if(file==INVALID_HANDLE_VALUE) throw std::runtime_error("Track query file unavailable");
    LARGE_INTEGER size{}; bool valid=GetFileSizeEx(file,&size) && size.QuadPart>0 && size.QuadPart<=1024*1024;
    std::string bytes(valid?static_cast<size_t>(size.QuadPart):0,'\0'); DWORD read=0;
    bool ok=valid && ReadFile(file,&bytes[0],static_cast<DWORD>(bytes.size()),&read,nullptr) && read==bytes.size(); CloseHandle(file);
    if(!ok) throw std::runtime_error("Invalid track query input");
    std::vector<std::string> values; std::unordered_set<std::string> seen;
    size_t start=0;
    while(start<bytes.size()) {
        size_t end=bytes.find('\n',start); if(end==std::string::npos) end=bytes.size();
        std::string value=bytes.substr(start,end-start); if(!value.empty() && value.back()=='\r') value.pop_back(); start=end+1;
        if(value.empty()) continue;
        if(value.size()>96 || !((value[0]>='A' && value[0]<='Z') || (value[0]>='a' && value[0]<='z'))) throw std::runtime_error("Invalid track alias");
        for(char ch:value) if(!((ch>='a' && ch<='z') || (ch>='A' && ch<='Z') || (ch>='0' && ch<='9') || ch=='_')) throw std::runtime_error("Invalid track alias");
        if(seen.insert(value).second) values.push_back(value);
        if(values.size()>10000) throw std::runtime_error("Track query limit");
    }
    if(values.empty()) throw std::runtime_error("Empty track query input");
    return values;
}
void WatchControl(const wchar_t* path) {
    char previous[33]{};
    for(;;) {
        Sleep(250);
        CheckExitPrompt();
        if(InterlockedCompareExchange(&renderActivityPending,1,0)==0 && !queue.Submit({UpdateRenderActivity,nullptr,CancelRenderActivity}))CancelRenderActivity(nullptr);
        if(!path || !*path)continue;
        HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);
        if(file==INVALID_HANDLE_VALUE) continue;
        LARGE_INTEGER size{}; char bytes[257]{}; DWORD read=0;
        bool valid=GetFileSizeEx(file,&size) && size.QuadPart>0 && size.QuadPart<=256 &&
            ReadFile(file,bytes,static_cast<DWORD>(size.QuadPart),&read,nullptr) && read==size.QuadPart;
        CloseHandle(file);
        StartQuery value;
        if(!valid || memchr(bytes,0,read) || !TwinKartRace::Parse(bytes,&value.request) || !strcmp(previous,value.request.identity)) continue;
        strcpy_s(previous,value.request.identity);
        size_t length=strlen(value.request.track);
        for(size_t i=0;i<length;++i) value.track[i]=value.request.track[i];
        if(!TwinKartRace::Options(value.request,value.options,ARRAYSIZE(value.options))) continue;
        value.complete=CreateEventW(nullptr,TRUE,FALSE,nullptr); if(!value.complete) throw std::runtime_error("Race event unavailable");
        for(int attempt=0;attempt<30;++attempt) {
            value.status=-1; ResetEvent(value.complete);
            if(!queue.Submit({ExecuteStart,&value,CancelStart}) || WaitForSingleObject(value.complete,30000)!=WAIT_OBJECT_0) { queue.Stop(); break; }
            if(value.status!=0) break;
            Sleep(1000);
        }
        if(value.executedThread!=queue.ThreadId()) { queue.Stop(); CloseHandle(value.complete); throw std::runtime_error("Race task not dispatched"); }
        CloseHandle(value.complete);
        Log(std::string("{\"event\":\"native_local_host_request\",\"request_id\":")+Json(value.request.identity)+",\"track_id\":"+Json(value.request.track)+
            ",\"mode\":"+std::to_string(value.request.mode)+",\"native_mode_mask\":"+std::to_string(value.modeMask)+",\"ai\":"+std::to_string(value.request.ai)+",\"status\":"+std::to_string(value.status)+
            ",\"stage\":"+std::to_string(value.stage)+",\"fault_code\":"+std::to_string(value.fault)+",\"fault_rva\":"+std::to_string(value.faultRva)+
            ",\"travel_requested\":"+(value.status==1?"true":"false")+",\"race_verified\":false,\"game_thread\":"+std::to_string(value.executedThread)+"}");
        // Loading-call receipt lets the room reject overlapping start clicks;
        // it is not a race completion or a fabricated network host response.
        wchar_t receipt[32768]{};
        if(path && swprintf_s(receipt,L"%s.%hs.result",path,value.request.identity)>0) {
            HANDLE receiptFile=CreateFileW(receipt,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(receiptFile!=INVALID_HANDLE_VALUE) {
                char acknowledgement[96];int receiptLength=sprintf_s(acknowledgement,"TKRACEACK1 %s %d\n",value.request.identity,value.status);
                DWORD written=0;if(receiptLength>0)WriteFile(receiptFile,acknowledgement,static_cast<DWORD>(receiptLength),&written,nullptr);CloseHandle(receiptFile);
            }
        }
    }
}
DWORD WINAPI Worker(void*) {
    wchar_t path[32768]{},input[32768]{},control[32768]{};
    DWORD a=GetEnvironmentVariableW(L"TWIN_KART_RACE_BRIDGE_LOG",path,ARRAYSIZE(path));
    DWORD b=GetEnvironmentVariableW(L"TWIN_KART_TRACK_QUERIES",input,ARRAYSIZE(input));
    DWORD c=GetEnvironmentVariableW(L"TWIN_KART_RACE_CONTROL",control,ARRAYSIZE(control));
    wchar_t discovery[8]{};GetEnvironmentVariableW(L"TWIN_KART_DISCOVER_NATIVE_TRACKS",discovery,ARRAYSIZE(discovery));
    bool discover=discovery[0]==L'1' && !discovery[1];
    wchar_t fg[8]{};GetEnvironmentVariableW(L"TWIN_KART_FRAME_GENERATION",fg,ARRAYSIZE(fg));bool graphics=fg[0]==L'1' && !fg[1];
    wchar_t sr[8]{};GetEnvironmentVariableW(L"TWIN_KART_SUPER_RESOLUTION",sr,ARRAYSIZE(sr));
    if(sr[0]==L'1' && !sr[1]) {
        wchar_t percent[32]{};DWORD length=GetEnvironmentVariableW(L"TWIN_KART_RENDER_PERCENTAGE",percent,ARRAYSIZE(percent));
        if(!length || length>=ARRAYSIZE(percent) || !renderScale.Configure(percent))return 1;
        graphics=true;
    }
    if(!a || a>=ARRAYSIZE(path) || b>=ARRAYSIZE(input) || c>=ARRAYSIZE(control) || (!b && !c && !graphics && !discover) || !TwinKartClient::Allowed(ClientSha)) return 1;
    output=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,0,nullptr);
    if(output==INVALID_HANDLE_VALUE) return 2;
    image=reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr));
    TwinKartLocalRace::image=image;
    try {
        wchar_t aiCatalog[32768]{};DWORD aiLength=GetEnvironmentVariableW(L"TWIN_KART_AI_CATALOG",aiCatalog,ARRAYSIZE(aiCatalog));
        if(aiLength) {
            if(aiLength>=ARRAYSIZE(aiCatalog) || !TwinKartLocalRace::aiRoster.LoadEnvironment())throw std::runtime_error("Invalid private AI outfit catalog");
            TwinKartLocalRace::aiRosterEnabled=true;
        }
        auto aliases=b?Queries(input):std::vector<std::string>{}; HWND window=nullptr;
        for(int attempt=0;attempt<480 && !window;++attempt) { if(SignaturesReady()) EnumWindows(FindWindow,reinterpret_cast<LPARAM>(&window)); if(!window) Sleep(250); }
        if(!window) throw std::runtime_error(SignaturesReady()?"Owned Unreal window unavailable":"Exact-build startup signatures unavailable");
        gameWindow=window;
        if(!queue.Install(window,module,RaceInput)) {
            Log("{\"event\":\"native_game_thread_hook_failed\",\"win32_error\":"+std::to_string(queue.InstallError())+"}");
            throw std::runtime_error("Owned game-thread message hook unavailable");
        }
        if(c) {
            Query runtime; runtime.complete=CreateEventW(nullptr,TRUE,FALSE,nullptr);
            if(!runtime.complete) throw std::runtime_error("Runtime setup event unavailable");
            bool submitted=queue.Submit({InitializeRuntime,&runtime,Cancel});
            if(!submitted || WaitForSingleObject(runtime.complete,30000)!=WAIT_OBJECT_0) { queue.Stop(); CloseHandle(runtime.complete); throw std::runtime_error("Runtime setup not dispatched"); }
            CloseHandle(runtime.complete);
            if(runtime.status!=1 || runtime.executedThread!=queue.ThreadId()) throw std::runtime_error("Exact-build local match adapters unavailable");
            Log("{\"event\":\"native_lobby_setup_bridge_ready\",\"native_match_path_preserved\":true,\"default_ai\":7,\"race_verified\":false}");
        }
        Log(std::string("{\"event\":\"native_race_bridge_ready\",\"client_sha256\":\"")+ClientSha+"\",\"can_start_race\":false,\"experimental_local_host\":"+(c?"true":"false")+",\"game_thread\":"+std::to_string(queue.ThreadId())+"}");
        try {
        if(discover) {
            std::vector<TwinKartRace::NativeTrackEntry> entries(10000);CatalogQuery catalog;catalog.entries=entries.data();catalog.capacity=static_cast<int>(entries.size());
            catalog.complete=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!catalog.complete)throw std::runtime_error("Catalog event failed");
            for(int attempt=0;attempt<90;++attempt) {
                catalog.status=-1;catalog.summary={};ResetEvent(catalog.complete);
                if(!queue.Submit({ExecuteCatalog,&catalog,CancelCatalog}) || WaitForSingleObject(catalog.complete,30000)!=WAIT_OBJECT_0){queue.Stop();break;}
                if(catalog.status==1)break;
                if(!TwinKartRace::RetryNativeTrackEnumeration(catalog.status,catalog.summary))break;
                if(attempt==0 || (attempt+1)%10==0)Log("{\"event\":\"native_track_catalog_waiting\",\"attempt\":"+std::to_string(attempt+1)+
                    ",\"status\":"+std::to_string(catalog.status)+",\"failure_stage\":"+std::to_string(catalog.summary.failureStage)+"}");
                Sleep(1000);
            }
            CloseHandle(catalog.complete);
            if(catalog.status!=1)Log("{\"event\":\"native_track_catalog_diagnostic\",\"status\":"+std::to_string(catalog.status)+
                ",\"failure_stage\":"+std::to_string(catalog.summary.failureStage)+",\"row_index\":"+std::to_string(catalog.summary.rowIndex)+
                ",\"rows\":"+std::to_string(catalog.summary.rows)+",\"failure_value\":"+std::to_string(catalog.summary.failureValue)+"}");
            if(catalog.status!=1 || catalog.executedThread!=queue.ThreadId())throw std::runtime_error("Native track enumeration failed");
            const auto& summary=catalog.summary;
            Log("{\"event\":\"native_track_catalog_begin\",\"rows\":"+std::to_string(summary.rows)+",\"count\":"+std::to_string(summary.count)+
                ",\"non_racing\":"+std::to_string(summary.nonRacing)+",\"game_thread\":"+std::to_string(catalog.executedThread)+"}");
            std::unordered_set<std::string> known(aliases.begin(),aliases.end());
            for(int i=0;i<summary.count;++i) {
                const auto& entry=entries[i];auto alias=Utf8(entry.alias);
                Log("{\"event\":\"native_track_catalog_entry\",\"track_id\":"+Json(alias)+",\"native_mode_mask\":"+std::to_string(entry.modeMask)+
                    ",\"native_laps\":"+std::to_string(entry.laps)+",\"game_thread\":"+std::to_string(catalog.executedThread)+"}");
                if(known.insert(alias).second)aliases.push_back(alias);
            }
            Log("{\"event\":\"native_track_catalog_complete\",\"count\":"+std::to_string(summary.count)+",\"game_thread\":"+std::to_string(catalog.executedThread)+"}");
        }
        unsigned found=0,empty=0; bool initialized=discover;
        for(const auto& alias:aliases) {
            Query value; value.alias.assign(alias.begin(),alias.end()); value.complete=CreateEventW(nullptr,TRUE,FALSE,nullptr);
            if(!value.complete) throw std::runtime_error("Query event failed");
            bool success=false;
            for(int attempt=0;attempt<(initialized?1:90);++attempt) {
                value.status=-1; ResetEvent(value.complete);
                if(!queue.Submit({Execute,&value,Cancel}) || WaitForSingleObject(value.complete,30000)!=WAIT_OBJECT_0) { queue.Stop(); break; }
                if(value.status==1 || initialized || value.status<0) { success=value.status>=1; break; }
                if(attempt==0) Log(std::string("{\"event\":\"native_track_lookup_waiting\",\"status\":")+std::to_string(value.status)+",\"game_thread\":"+std::to_string(value.executedThread)+"}");
                Sleep(1000);
            }
            if(!success || value.executedThread!=queue.ThreadId()) {
                Log("{\"event\":\"native_track_lookup_failed\",\"track_id\":"+Json(alias)+",\"status\":"+std::to_string(value.status)+
                    ",\"executed_thread\":"+std::to_string(value.executedThread)+",\"game_thread\":"+std::to_string(queue.ThreadId())+"}");
                if(value.executedThread!=queue.ThreadId())queue.Stop();
                CloseHandle(value.complete);throw std::runtime_error("Native track definition unavailable");
            }
            initialized=true; CloseHandle(value.complete);
            Log(std::string("{\"event\":\"native_track_definition\",\"track_id\":")+Json(alias)+",\"status\":"+std::to_string(value.status)+",\"native_level_name\":"+Json(Utf8(value.level))+
                ",\"native_mode_mask\":"+std::to_string(value.modeMask)+",\"native_laps\":"+std::to_string(value.laps)+",\"game_thread\":"+std::to_string(value.executedThread)+"}");
            if(value.status==1) ++found; else ++empty;
        }
        Log(std::string("{\"event\":\"native_track_queries_complete\",\"found\":")+std::to_string(found)+",\"empty\":"+std::to_string(empty)+",\"race_started\":false}");
        } catch(const std::exception& error) {
            Log("{\"event\":\"native_track_capture_failed\",\"reason\":"+Json(error.what())+",\"catalog_verified\":false}");
            // A malformed/not-ready metadata capture cannot silently remove
            // already installed native lobby controls or the render activity
            // poll. A stopped/failed dispatcher remains a fatal bridge error.
            if((!c && !graphics) || !queue.Submit({[](void*){},nullptr,nullptr}))throw;
        }
        if(c) {
            wchar_t readyPath[32768]{};
            if(swprintf_s(readyPath,L"%s.ready",control)<=0)throw std::runtime_error("Native room readiness path unavailable");
            HANDLE ready=CreateFileW(readyPath,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(ready==INVALID_HANDLE_VALUE)throw std::runtime_error("Native room readiness marker unavailable");
            DWORD written=0;bool saved=WriteFile(ready,ClientSha,64,&written,nullptr) && written==64;CloseHandle(ready);
            if(!saved)throw std::runtime_error("Native room readiness marker failed");
            Log("{\"event\":\"native_custom_room_loading_ready\",\"race_verified\":false}");
        }
        if(c || graphics) WatchControl(c?control:nullptr);
    } catch(const std::exception& error) {
        Log("{\"event\":\"native_race_bridge_failed\",\"reason\":"+Json(error.what())+",\"can_start_race\":false}");
    }
    queue.Stop(); CloseHandle(output); output=INVALID_HANDLE_VALUE; return 0;
}
}
extern "C" __declspec(dllexport) BOOL WINAPI TwinKartRaceRenderingActive() {
    return renderActivity.Active(GetTickCount64())?TRUE:FALSE;
}
extern "C" __declspec(dllexport) BOOL WINAPI TwinKartSceneRenderingActive() {
    return sceneActivity.Active(GetTickCount64())?TRUE:FALSE;
}
extern "C" __declspec(dllexport) BOOL WINAPI TwinKartCockpitDrivingActive() {
    return cockpitDrivingActivity.Active(GetTickCount64())?TRUE:FALSE;
}
BOOL WINAPI DllMain(HINSTANCE self,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH) { module=self; DisableThreadLibraryCalls(self); HANDLE worker=CreateThread(nullptr,0,Worker,nullptr,0,nullptr); if(worker) CloseHandle(worker); }
    return TRUE;
}
