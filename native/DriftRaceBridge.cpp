#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_set>
#include <stdexcept>
#include <intrin.h>
#include "ClientBuildGuard.h"
#include "GameThreadQueue.h"
#include "DriftObjectIdentity.h"
#include "RaceControl.h"
#include "LocalRaceRuntime.h"
#include "NativeObjectiveCapture.h"
#include "NativeChaserMode.h"
#include "NativeChaserConfig.h"
#include "NativeChaserPresentation.h"
#include "NativePrototypeLobby.h"
#include "NativeLoadingPreview.h"
#include "PrototypeSettlement.h"
#include "PrototypeResultPanel.h"
#include "ChaserArenaScene.h"
#include "NativeWorldBounds.h"
#include "ChaserArenaDriver.h"
#include "ChaserItemInput.h"
#include "NativeVehicleSweep.h"
#include "NativeComponentSweep.h"
#include "NativeHitObjects.h"
#include "NativeBodyGeometry.h"
#include "NativeKartContacts.h"
#include "KartRoadPose.h"
#include "KartTranslationEdge.h"
#include "LayeredRoadNavigation.h"
#include "RoadSegmentSupport.h"
#include "RoadEdgeQueries.h"
#include "NativeHideRules.h"
#include "HideRepairedScene.h"
#include "NativeStaticMaterials.h"
#include "RenderActivity.h"
#include "NativeRacePhase.h"
#include "NativeRenderScale.h"
#include "NativeTrackModes.h"
#include "NativeTrackCatalog.h"
#include "NativeReturnRoute.h"
#include "NativeRacePreparation.h"
#include "NativeModeBinding.h"
#include "NativeEventMenu.h"
#include "NativeCubeWorld.h"

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
volatile LONG controlInterval=250;
TwinKartGraphics::NativeRenderScale renderScale;
int lastScaleResult=-1;
struct NativeString { const wchar_t* data; int count,capacity; };
bool ModeLabelText(const BYTE* field,wchar_t* destination,size_t capacity);
bool InstallLoadingPresentation();
void ApplyRetainedLoadingWidget();
void DispatchPreparedFreeDrive(BYTE* traveler,BYTE* world);
struct Query { std::wstring alias; wchar_t level[512]{}; unsigned modeMask=0;int laps=0,status=-1; DWORD executedThread=0; HANDLE complete=nullptr; };
struct StartQuery { TwinKartRace::Request request; bool factoryCapture=false,hideCapture=false;wchar_t track[97]{},options[1024]{},factoryAndroidKart[97]{};unsigned modeMask=0;int status=-1,stage=0; DWORD fault=0; uintptr_t faultRva=0; DWORD executedThread=0; HANDLE complete=nullptr; };
volatile LONG objectiveCapturePending=0;
struct CatalogQuery {TwinKartRace::NativeTrackEntry* entries=nullptr;int capacity=0,status=-1;TwinKartRace::NativeTrackEnumeration summary;DWORD executedThread=0;HANDLE complete=nullptr;};
HWND gameWindow=nullptr;
wchar_t practiceTicketPath[32768]{},practiceActivePath[32768]{},practiceTemporaryPath[32768]{};
TwinKartCamera::ObjectIdentity practiceFinish{};
TwinKartCamera::ObjectIdentity practiceDrivingWorld{};
TwinKartCamera::ObjectIdentity practiceReverseEdgeFinish{};
ULONGLONG practiceReverseEdgeUntil=0;
bool practiceRequested=false,practiceConsumeF8=false;
ULONGLONG practiceLobbyUntil=0;
ULONGLONG practicePrepareUntil=0;
TwinKartCamera::ObjectIdentity practicePrepareWorld{},practicePrepareAccount{};
TwinKartNative::OwnedCodePatch practicePrepareNotificationPatch;
bool practicePrepareInstalled=false,practicePrepareAccepted=false;
char practicePriorTicket[32]{};
char practiceLobbyTrack[97]{};
volatile LONG practiceHud=0;
volatile LONG64 practiceHeartbeat=0;
bool practiceResultBoundaryInstalled=false;
bool practiceLobbyLease=false;
TwinKartCamera::ObjectIdentity practiceResultSuppressionLogged{};
SRWLOCK prototypeResultLock=SRWLOCK_INIT;
TwinKartObjective::PrototypeSettlementMailbox prototypeResults;
TwinKartObjective::HideSettlementJournal hideSettlementJournal;
TwinKartObjective::HideMatchSummary hideSettlement;
bool hideSettlementAvailable=false;
struct ResultReturnQuery {TwinKartObjective::PrototypePresentationGrant grant;std::uint64_t request=0;};
TwinKartObjective::PrototypeResultPanel prototypeResultPanel;
TwinKartObjective::PrototypeHintPanel hideHintPanel;
volatile LONG hideHintState=0; // only small display values cross threads
volatile LONG chaserHintState=0;
TwinKartObjective::PrototypePresentationGrant prototypeDisplayedResult{};
TwinKartObjective::PrototypePresentationState prototypePresentation;
volatile LONG flagFaultCount=0;
volatile LONG factoryDiagnosticArmed=0;
bool hideCaptureArmed=false;
bool hideArenaCapture=false,hideArenaPrepared=false,hideArenaPreparationAttempted=false;
bool hideRepairedSceneEnabled=false;
bool hideLobbyEnabled=false;
bool EnvironmentFlag(const wchar_t* name) {
    wchar_t value[4]{};return GetEnvironmentVariableW(name,value,4)==1 && value[0]==L'1';
}
struct HideRepairedFloor {
    TwinKartCamera::ObjectIdentity actor{},component{},mesh{};
    TwinKartCamera::ObjectIdentity previousIdentity[32]{};
    void* previous[32]{};unsigned changed=0;
};
struct HideRepairedStart {TwinKartCamera::ObjectIdentity actor{};unsigned long long tag=0;};
struct HideRepairedSceneState {
    TwinKartObjective::HideRepairPlan layout{};
    TwinKartCamera::ObjectIdentity world{},actors[TwinKartObjective::HideRepairActorCount]{},assets[6]{};
    HideRepairedFloor floors[8]{};HideRepairedStart starts[64]{};
    unsigned actorCount=0,floorCount=0,startCount=0;
    unsigned long long mutedTag=0;
    bool attempted=false,ready=false;
    int stage=-1,census=-1;
    int groundDiagnostic=0;unsigned groundQueries=0;unsigned char groundTypes[3]{};int groundTypeCount=0;
    ULONGLONG next=0;
    TwinKartRace::ArenaPoint low{},high{};
};
HideRepairedSceneState hideRepairedScene;
TwinKartNative::OwnedCodePatch hideArenaModePatch;
bool objectiveArenaModeInstalled=false,chaserArenaEnabled=false;
bool hideWaitRequested=false;
int hideControllerTarget=1;
bool hideBotPreparationAttempted=false;
bool hideBotPreparationSucceeded=false;
TwinKartNative::OwnedCodePatch hideSessionConstructorPatch;
TwinKartLocalRace::Construct hideBaseConstructor=nullptr;
using HideRuleTick=void(__fastcall*)(void*,float,int,void*);
HideRuleTick hideRuleTick=nullptr;
TwinKartNative::OwnedCodePatch hideRuleTickPatch;
unsigned hideTickSamples=0;
double hideTickSeconds=0;
using HideStageChange=void(__fastcall*)(void*,unsigned long long);
HideStageChange hideStageChange=nullptr;
TwinKartNative::OwnedCodePatch hideStagePatch;
unsigned hideStageSamples=0;
using HideSearchCallback=void(__fastcall*)(void*);
HideSearchCallback hideSearchBegin=nullptr,hideSearchEnd=nullptr;
TwinKartNative::OwnedCodePatch hideSearchBeginPatch,hideSearchEndPatch;
HideSearchCallback hideGrantRoles=nullptr;
TwinKartNative::OwnedCodePatch hideGrantRolesPatch;
TwinKartObjective::HideSearchClock hideSearchClock;
TwinKartCamera::ObjectIdentity hideSearchOwner{};
TwinKartCamera::ObjectIdentity hideDeadlineSignalOwner{};
BYTE* hideDeadlineSignal=nullptr;
TwinKartObjective::HideSeat hideRoundSeats[8]{};
TwinKartCamera::ObjectIdentity hideRoundPawns[8]{},hideDisguiseControllers[8]{},hideRosterOwner{};
bool hideRoundRosterKnown=false,hideRoundRosterPending=false;
ULONGLONG hideRosterRetry=0;
bool hideInputCapture=false,hideSearchActive=false,hideInputCaptureDone=false;
bool hideAutonomyCompatibility=false;
ULONGLONG hideDisguiseNext=0;unsigned hideDisguiseRequests[8]{};
struct HideMaterialBinding {
    TwinKartVisual::MaterialRepairLease lease;
    TwinKartCamera::ObjectIdentity rules{},pawn{},owner{},component{},mesh{};
    TwinKartCamera::ObjectIdentity replacements[32]{};
    std::uint64_t slotNames[32]{};
    int slot=-1;unsigned failures=0;
};
HideMaterialBinding hideMaterialBindings[8][2];
struct HideMaterialSourceSpec {const wchar_t* samplePackage;const wchar_t* sampleName;
    const wchar_t* sourcePackage;const wchar_t* sourceName;const wchar_t* sourcePath;};
const HideMaterialSourceSpec hideMaterialSources[]{
    {L"/Game/Dev/HideNSeek/SampleCollisionMesh/Des_mesh_tree003",L"Des_mesh_tree003",
     L"/Game/ART/Env/Theme_Desert/Asset/Mesh/Des_mesh_tree003",L"Des_mesh_tree003",
     L"/Game/ART/Env/Theme_Desert/Asset/Mesh/Des_mesh_tree003.Des_mesh_tree003"},
    {L"/Game/Dev/HideNSeek/SampleCollisionMesh/BC_mesh_marketbox06",L"BC_mesh_marketbox06",
     L"/Game/ART/Env/Theme_Village/Asset/Mesh/BC_mesh_marketbox01",L"BC_mesh_marketbox01",
     L"/Game/ART/Env/Theme_Village/Asset/Mesh/BC_mesh_marketbox01.BC_mesh_marketbox01"}
};
TwinKartCamera::ObjectIdentity hideMaterialSourceMeshes[2]{};
ULONGLONG hideMaterialNext=0,hideMaterialSourceRetry[2]{};
unsigned hideMaterialSourceAttempts[2]{};
bool hideRoundInputsActive=false;
bool hideObjectiveAi=false;
TwinKartRace::LayeredRoadNavigation hideGround;
TwinKartRace::RoadSegmentSupport hideRoadSupport;
bool hideGroundRejected=false;
TwinKartRace::FlagGroundRoute hideRoutes[8];
TwinKartRace::ArenaDriver hideDrivers[8];
TwinKartObjective::HidePatrolProgress hidePatrolProgress[8];
TwinKartObjective::HideObservation hidePreviousObservation[8]{};
TwinKartRace::ArenaPoint hideAiOrigin[8]{};
bool hideAiOriginValid[8]{};
TwinKartRace::ArenaPoint hideGoals[8]{};
ULONGLONG hideAiNext=0,hideAiGoalAt[8]{},hideAiLogAt=0;
float hideGroundTop=0,hideGroundBottom=0;
unsigned hideAiEnabled=0;
float hideAiForward[8]{},hideAiSide[8]{};
bool hideAiControlValid[8]{};
size_t hideAdjacencySample=0;
unsigned hideBlockedEdges=0;
unsigned char hideObjectTypes[3]{};
int hideObjectTypeCount=0;
TwinKartCamera::ObjectIdentity hideInputPawn{};
TwinKartCamera::ObjectIdentity hideInputController{};
double hideInputSeconds=0;
TwinKartRace::ArenaPoint hideInputOrigin{};
bool hideSeekerInputDone=false;
TwinKartCamera::ObjectIdentity hideSeekerAppearanceAttempted[8]{};
TwinKartCamera::ObjectIdentity hideSeekerAppearanceOwner[8]{};
int hideSeekerAppearanceStatus[8]{};
ULONGLONG hideSeekerAppearanceRetry[8]{};
int hideSeekerMeshMask[8]{};
unsigned hideSeekerAppearanceRequests[8]{};
TwinKartRace::AiRoster hideAppearanceRoster;
bool hideAppearanceRosterReady=false;
ULONGLONG hideAppearanceRosterRetry=0;
TwinKartCamera::ObjectIdentity hideSeekerInputPawn{},hideSeekerInputMovement{};
TwinKartCamera::ObjectIdentity hideSeekerInputController{};
double hideSeekerInputSeconds=0;
TwinKartRace::ArenaPoint hideSeekerInputOrigin{};
using HideSubstep=void(__fastcall*)(void*,float,void*);
HideSubstep hideSubstep=nullptr;
TwinKartNative::OwnedCodePatch hideSubstepPatch;
using HideThrottle=void(__fastcall*)(void*,float);
HideThrottle hideThrottle=nullptr;
TwinKartNative::OwnedCodePatch hideThrottlePatch;
unsigned hideThrottleCallers[32]{};unsigned hideThrottleCallerCount=0;
alignas(8) volatile LONG64 hideObservedMovement=0;
volatile LONG hideObservedSubsteps=0,hideObservedBodies=0;
volatile LONG hideObservedThrottle=0,hideObservedContact=0;
using HideHit=void(__fastcall*)(void*,void*,void*,void*,bool,const TwinKartRace::ArenaPoint*,
    const TwinKartRace::ArenaPoint*,const TwinKartRace::ArenaPoint*,const void*);
HideHit hideHit=nullptr;
TwinKartNative::OwnedCodePatch hideHitPatch;
TwinKartCamera::ObjectIdentity hideRoleChildren[8]{};
TwinKartCamera::ObjectIdentity hideBotInputIsolated[8]{};
struct HidePhysicalContact {
    TwinKartCamera::ObjectIdentity hider{},seeker{},actor{};
    ULONGLONG at=0;
};
HidePhysicalContact hidePhysicalContacts[8]{};
struct HideOverlapPair {TwinKartCamera::ObjectIdentity hiderRoot{},seekerRoot{};};
HideOverlapPair hideOverlapPairs[8][8]{};
ULONGLONG hideContactNext=0;
bool hideDeadlineCompatibility=false;
void StopHideObjectiveInputs();
void ForgetHideMaterialRepairs();
void ReleaseHideMaterialRepairs();
void ReleaseHideRepairedScene();
void RestoreHideLocalDisguiseView();
bool PrepareHideRepairedScene(BYTE*,BYTE*);
TwinKartRace::RoadSurfaceResult ReadHideRepairedSurface(const TwinKartRace::ArenaPoint&,const TwinKartRace::ArenaPoint&,TwinKartRace::RoadSurface&,void*);
bool InstallChaserModeDefinition();
bool PrepareChaserArena(BYTE*);
bool SelectChaserArenaStart(void*,void*,int,void*&);
void LogMissingChaserConfig(const wchar_t*,const wchar_t*);
bool OwnedChaserWorld(BYTE*);
bool UpdateChaserAi(BYTE*,bool);
bool NativeDefaultLoadingTitleBound(int mode);
void RecoverArenaHumanKartTick(BYTE*,TwinKartLocalRace::RawForward);
void ResetChaserNavigation();
void ResetChaserSession();
void ObserveHideCapture(BYTE*);
bool InstallHideCaptureCompatibility();
void MaintainOfflineModeLayout();
LONG CALLBACK CaptureFlagFault(EXCEPTION_POINTERS* error) {
    bool factory=InterlockedCompareExchange(&factoryDiagnosticArmed,0,0)==1;
    bool flag=TwinKartRace::FlagMode(TwinKartLocalRace::armedMode) && TwinKartLocalRace::armed && GetCurrentThreadId()==queue.ThreadId();
    bool hide=hideCaptureArmed && GetCurrentThreadId()==queue.ThreadId();
    bool chaser=chaserArenaEnabled && TwinKartLocalRace::armed && TwinKartLocalRace::armedMode==108;
    if(!error || (!factory && !flag && !hide && !chaser))return EXCEPTION_CONTINUE_SEARCH;
    if(chaser && error->ExceptionRecord->ExceptionCode!=EXCEPTION_ACCESS_VIOLATION &&
       error->ExceptionRecord->ExceptionCode!=EXCEPTION_ILLEGAL_INSTRUCTION)return EXCEPTION_CONTINUE_SEARCH;
    if(InterlockedIncrement(&flagFaultCount)>12)return EXCEPTION_CONTINUE_SEARCH;
    __try {
        CONTEXT context=*error->ContextRecord;char line[1400]{};
        int length=sprintf_s(line,"{\"event\":\"%s\",\"code\":%lu,\"game_frames\":[",chaser?"native_chaser_fault_trace":factory?"native_factory_fault_trace":hide?"native_hide_fault_trace":"native_flag_fault_trace",error->ExceptionRecord->ExceptionCode);
        unsigned frames=0;
        for(unsigned i=0;i<24 && context.Rip && length<1200;++i) {
            auto base=reinterpret_cast<ULONG64>(image);
            if(context.Rip>=base && context.Rip-base<0x8657000)length+=sprintf_s(line+length,sizeof(line)-length,"%s%llu",frames++?",":"",context.Rip-base);
            ULONG64 moduleBase=0;auto function=RtlLookupFunctionEntry(context.Rip,&moduleBase,nullptr);
            if(function){void* handler=nullptr;ULONG64 frame=0;RtlVirtualUnwind(UNW_FLAG_NHANDLER,moduleBase,context.Rip,function,&context,&handler,&frame,nullptr);}
            else {context.Rip=*reinterpret_cast<ULONG64*>(context.Rsp);context.Rsp+=8;}
        }
        length+=sprintf_s(line+length,sizeof(line)-length,"]}\r\n");DWORD written=0;
        if(frames && output!=INVALID_HANDLE_VALUE)WriteFile(output,line,static_cast<DWORD>(length),&written,nullptr);
    } __except(EXCEPTION_EXECUTE_HANDLER){}
    return EXCEPTION_CONTINUE_SEARCH;
}
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
    bool active=false,sceneActive=false,cockpitDriving=false,chaserControl=false;
    TwinKartObjective::PrototypePresentationWorld presentationWorld{};
    __try {
        if(GetCurrentThreadId()==queue.ThreadId()) {
            ApplyRetainedLoadingWidget();
            MaintainOfflineModeLayout();
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
                DispatchPreparedFreeDrive(traveler,world);
                TwinKartLocalRace::RetryLocalStartWhenReady(world);
                TwinKartLocalRace::ObserveFactoryPhase(world);
                if(hideCaptureArmed)ObserveHideCapture(world);
                if(chaserArenaEnabled && TwinKartLocalRace::armedMode==108)PrepareChaserArena(world);
                // Lobby, garage and results have live 3D worlds without a
                // BeginRace phase. Rendering eligibility is separate from
                // race eligibility; the renderer still requires current-frame
                // depth/motion/camera and real simulation/present markers.
                using Paused=bool(__fastcall*)(void*);
                sceneActive=!reinterpret_cast<Paused>(image+0x4011c70)(world);
                TwinKartLocalRace::UpdateFlagArena(world,sceneActive);
                chaserControl=UpdateChaserAi(world,sceneActive);
                // Native automatic return does not pass through our ExitRace.
                // Observe its actual owned world here so queued and visible
                // result panels cannot retain a grant after native travel.
                TwinKartLocalRace::LocalRaceWorld presentationLease;TwinKartCamera::ObjectIdentity identity{};
                bool owned=TwinKartLocalRace::CaptureOwnedWorld(world,presentationLease);
                unsigned presentationMode=TwinKartLocalRace::armedMode==108 && OwnedChaserWorld(world)?108u:
                    hideCaptureArmed && owned?109u:0u;
                if(presentationMode && owned && TwinKartLocalRace::CaptureFlagObject(world,identity))
                    presentationWorld={reinterpret_cast<std::uintptr_t>(world),identity.index,identity.serial,presentationMode};
                BYTE* state=*reinterpret_cast<BYTE**>(world+0x120);
                if(TwinKartLocalRace::Live(state)) {
                    using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
                    bool flagMode=TwinKartRace::FlagMode(TwinKartLocalRace::armedMode);
                    bool ownedFactory=TwinKartLocalRace::factoryCompatEnabled && TwinKartLocalRace::factoryWorld.Matches(world,
                        *reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),*reinterpret_cast<int*>(image+0x69c28c4));
                    bool ownedChaser=OwnedChaserWorld(world);
                    auto phaseType=reinterpret_cast<Type>(image+TwinKartGraphics::ScenePhaseComponentGetter(TwinKartLocalRace::armedMode,ownedFactory,ownedChaser))();
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
                            if(ownedChaser) {
                                active=TwinKartGraphics::ClassifyNativeDrivingPhase(actualClass,phaseClass,currentName,
                                    *reinterpret_cast<unsigned long long*>(image+TwinKartGraphics::ChaserGameNameRva),paused);
                                cockpitDriving=active;
                            } else if(flagMode) {
                                unsigned long long game=0,overtime=0,lastSpurt=0;
                                using Name=void(__fastcall*)(void*,const wchar_t*,int);
                                reinterpret_cast<Name>(image+0x2589130)(&game,L"Game",0);
                                reinterpret_cast<Name>(image+0x2589130)(&overtime,L"Overtime",0);
                                reinterpret_cast<Name>(image+0x2589130)(&lastSpurt,L"LastSpurt",0);
                                active=TwinKartGraphics::ClassifyNativeRacePhase(actualClass,phaseClass,currentName,game,overtime,paused);
                                active=active || TwinKartGraphics::ClassifyNativeDrivingPhase(actualClass,phaseClass,currentName,lastSpurt,paused);
                                cockpitDriving=active;
                            } else {
                                active=TwinKartGraphics::ClassifyNativeRacePhase(actualClass,phaseClass,currentName,beginName,
                                    *reinterpret_cast<unsigned long long*>(image+TwinKartGraphics::RetireCountdownNameRva),paused);
                                cockpitDriving=TwinKartGraphics::ClassifyNativeDrivingPhase(actualClass,phaseClass,currentName,beginName,paused);
                                if(cockpitDriving) {
                                    TwinKartLocalRace::ObserveHumanDriving(component);
                                    TwinKartLocalRace::MaintainInfiniteBooster(component,true);
                                }
                            }
                        }
                    }
                }
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {active=false;sceneActive=false;cockpitDriving=false;chaserControl=false;presentationWorld={};}
    AcquireSRWLockExclusive(&prototypeResultLock);
    if(prototypePresentation.Observe(presentationWorld)) {
        prototypeResults.Reset();hideSettlement={};hideSettlementAvailable=false;
    }
    ReleaseSRWLockExclusive(&prototypeResultLock);
    sceneActivity.Publish(sceneActive,GetTickCount64());
    // Arena steering needs a 20 Hz feedback loop. Circuit AI is native and
    // retains the existing low-frequency activity polling. No extra queue task
    // is submitted until the previous game-thread task has completed.
    InterlockedExchange(&controlInterval,(active && TwinKartRace::FlagMode(TwinKartLocalRace::armedMode)) || chaserControl?50:250);
    cockpitDrivingActivity.Publish(cockpitDriving,GetTickCount64());
    renderActivity.Publish(active,GetTickCount64());InterlockedExchange(&renderActivityPending,0);
}
void CancelRenderActivity(void*){sceneActivity.Publish(false,0);cockpitDrivingActivity.Publish(false,0);renderActivity.Publish(false,0);InterlockedExchange(&controlInterval,250);InterlockedExchange(&renderActivityPending,0);}
// 0=model not ready; 1=native travel submitted; 2=definition absent;
// -1=native fault; no value means race acceptance.
void RuntimeEvent(const char* name,int slot,int value) {
    char line[256]; int bytes=sprintf_s(line,"{\"event\":\"%s\",\"slot\":%d,\"value\":%d,\"race_verified\":false}\r\n",name,slot,value);
    DWORD written=0; if(bytes>0) WriteFile(output,line,static_cast<DWORD>(bytes),&written,nullptr);
}
BYTE* OwnedRaceWorld() {
    if(GetCurrentThreadId()!=queue.ThreadId() || (!TwinKartLocalRace::armed && !TwinKartLocalRace::factoryCompatEnabled &&
       !hideCaptureArmed && TwinKartLocalRace::armedMode!=108))return nullptr;
    BYTE* traveler=Traveler();if(!traveler)return nullptr;
    using GetWorld=BYTE*(__fastcall*)(void*);
    BYTE* world=reinterpret_cast<GetWorld>((*reinterpret_cast<void***>(traveler))[0x160/8])(traveler);
    if(hideCaptureArmed && TwinKartLocalRace::Live(world) && !*reinterpret_cast<void**>(world+0x38)) {
        using Type=void*(__fastcall*)();auto mode=*reinterpret_cast<BYTE**>(world+0x118);
        if(TwinKartLocalRace::Live(mode) && *reinterpret_cast<void**>(mode+0x10)==reinterpret_cast<Type>(image+0x10f34f0)()) {
            auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);int count=*reinterpret_cast<int*>(image+0x69c28c4);
            TwinKartCamera::ObjectIdentity identity;
            if(TwinKartLocalRace::CaptureFlagObject(world,identity))TwinKartLocalRace::objectiveWorld.Bind(identity,chunks,count);
        }
    }
    TwinKartLocalRace::LocalRaceWorld lease;
    return TwinKartLocalRace::CaptureOwnedWorld(world,lease)?world:nullptr;
}
bool ChaserSecondSlotKey(const MSG& message);
void ChaserForwardKeyEdge(const MSG& message);
bool RaceInput(const MSG& message) {
    if(message.wParam=='W' && message.message==WM_KEYDOWN)ChaserForwardKeyEdge(message);
    if(message.wParam=='S' && message.message==WM_KEYDOWN && !(message.lParam&(1LL<<30)) &&
       GetCurrentThreadId()==queue.ThreadId() && message.hwnd==gameWindow && GetForegroundWindow()==gameWindow &&
       InterlockedCompareExchange(&practiceHud,0,0)==2 && !(GetAsyncKeyState(VK_MENU)&0x8000)) {
        practiceReverseEdgeFinish=practiceFinish;
        practiceReverseEdgeUntil=GetTickCount64()+100;
    }
    if(message.wParam=='2' && (message.message==WM_KEYDOWN || message.message==WM_KEYUP))
        return ChaserSecondSlotKey(message);
    if(message.wParam==VK_F8 && (message.message==WM_KEYDOWN || message.message==WM_KEYUP)) {
        if(message.message==WM_KEYUP) {
            const bool consumed=practiceConsumeF8;practiceConsumeF8=false;return consumed;
        }
        const auto seen=InterlockedCompareExchange64(&practiceHeartbeat,0,0);
        if(message.hwnd==gameWindow && GetForegroundWindow()==gameWindow &&
           seen && GetTickCount64()-seen<300 && InterlockedCompareExchange(&practiceHud,0,0)==1) {
            if(!(message.lParam&(1LL<<30))){practiceRequested=true;practiceConsumeF8=true;}
            return practiceConsumeF8;
        }
        return practiceConsumeF8;
    }
    if(message.wParam!=VK_ESCAPE || (message.message!=WM_KEYDOWN && message.message!=WM_KEYUP))return false;
    DWORD foreground=0;GetWindowThreadProcessId(GetForegroundWindow(),&foreground);
    if(foreground!=GetCurrentProcessId())return false;
    __try {
        auto world=OwnedRaceWorld();if(!world)return false;
        if(message.message==WM_KEYDOWN && !(message.lParam&(1LL<<30)) &&
            InterlockedCompareExchange(&exitPending,0,0)==0) {
            if(!TwinKartLocalRace::CaptureOwnedWorld(world,exitWorld))return false;
            InterlockedExchange(&exitPending,1);
        }
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
void CancelExit(void*){InterlockedExchange(&exitPending,0);}
int SubmitRaceExit() {
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
            RestoreHideLocalDisguiseView();ReleaseHideMaterialRepairs();ReleaseHideRepairedScene();
            reinterpret_cast<Return>(image+0x1bbc280)(traveler,&options);
            if(options.data)reinterpret_cast<void(__fastcall*)(void*)>(image+0x248b950)(options.data);
            TwinKartLocalRace::armed=false;TwinKartLocalRace::raceWorld.Reset();
            TwinKartLocalRace::factoryWorld.Reset();TwinKartLocalRace::factoryPopulation.Reset();
            TwinKartLocalRace::objectiveWorld.Reset();hideCaptureArmed=false;
            InterlockedExchange(&hideHintState,0);
            ForgetHideMaterialRepairs();
            ResetChaserSession();
            hideSearchClock.Stop();hideSearchOwner={};hideDeadlineSignalOwner={};hideDeadlineSignal=nullptr;
            hideSearchActive=false;hideRoundRosterPending=hideRoundRosterKnown=false;
            InterlockedExchange64(&hideObservedMovement,0);
            InterlockedExchange(&factoryDiagnosticArmed,0);
            result=1;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {result=-2;}
    RuntimeEvent("native_mid_race_return_submitted",-1,result);
    InterlockedExchange(&exitPending,0);
    return result;
}
void ExitRace(void*){SubmitRaceExit();}
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
TwinKartObjective::PrototypeReturnOutcome TryReturnPrototypeResult(const ResultReturnQuery& query) {
    using Outcome=TwinKartObjective::PrototypeReturnOutcome;
    const auto& value=query.grant.world;auto mode=value.mode;
    __try {
        AcquireSRWLockShared(&prototypeResultLock);bool current=prototypePresentation.Current(query.grant);ReleaseSRWLockShared(&prototypeResultLock);
        if(!current)return Outcome::Stale;
        auto world=OwnedRaceWorld();TwinKartCamera::ObjectIdentity identity{};
        if(!world || reinterpret_cast<std::uintptr_t>(world)!=value.world || (mode!=108 && mode!=109) ||
           (mode==108?!OwnedChaserWorld(world):!hideCaptureArmed) ||
           !TwinKartLocalRace::CaptureFlagObject(world,identity) || identity.index!=value.index || identity.serial!=value.serial)return Outcome::Stale;
        // Like Escape, take a fresh lease from the verified active race. A
        // prior exit may still hold a different world's single-use lease.
        if(!TwinKartLocalRace::CaptureOwnedWorld(world,exitWorld))return Outcome::Stale;
        // Uses the same original cleanup/travel boundary as the native exit.
        // No result packet, winner or reward is invented by this fallback UI.
        return SubmitRaceExit()==1?Outcome::Submitted:Outcome::Failed;
    } __except(EXCEPTION_EXECUTE_HANDLER) {RuntimeEvent("native_chaser_result_return_rejected",-1,0);return Outcome::Failed;}
}
void ReturnPrototypeResult(void* raw) {
    auto query=static_cast<ResultReturnQuery*>(raw);
    // Queue cancellation owns the context only if execution fails. Handle
    // exceptions before freeing it, so cancellation cannot delete it twice.
    auto outcome=TwinKartObjective::PrototypeReturnOutcome::Failed;
    try {outcome=TryReturnPrototypeResult(*query);}
    catch(...) {RuntimeEvent("native_prototype_result_return_failed",-1,0);}
    AcquireSRWLockExclusive(&prototypeResultLock);
    prototypePresentation.CompleteReturn(query->grant,query->request,outcome);
    ReleaseSRWLockExclusive(&prototypeResultLock);
    RuntimeEvent("native_prototype_result_return_acknowledged",-1,static_cast<int>(outcome));
    delete query;
}
void CancelResultReturn(void* raw){
    auto query=static_cast<ResultReturnQuery*>(raw);
    AcquireSRWLockExclusive(&prototypeResultLock);
    prototypePresentation.CompleteReturn(query->grant,query->request,TwinKartObjective::PrototypeReturnOutcome::Failed);
    ReleaseSRWLockExclusive(&prototypeResultLock);delete query;
}
void CheckPrototypeResult() {
    using Outcome=TwinKartObjective::PrototypeReturnOutcome;
    static std::uint64_t observedEpoch=0;
    AcquireSRWLockShared(&prototypeResultLock);auto epoch=prototypePresentation.Generation();ReleaseSRWLockShared(&prototypeResultLock);
    if(epoch!=observedEpoch){prototypeResultPanel.Close();prototypeDisplayedResult={};observedEpoch=epoch;}
    prototypeResultPanel.Pump();
    if(prototypeResultPanel.TakeReturn()) {
        std::uint64_t request=0;
        AcquireSRWLockExclusive(&prototypeResultLock);bool begin=prototypePresentation.BeginReturn(prototypeDisplayedResult,request);ReleaseSRWLockExclusive(&prototypeResultLock);
        if(begin) {
            auto query=new ResultReturnQuery{prototypeDisplayedResult,request};
            if(queue.Submit({ReturnPrototypeResult,query,CancelResultReturn}))prototypeResultPanel.ShowReturnPending();
            else {CancelResultReturn(query);prototypeResultPanel.ShowReturnUnavailable();}
        }
    }
    if(prototypeResultPanel.IsOpen()) {
        AcquireSRWLockShared(&prototypeResultLock);
        bool current=prototypePresentation.Current(prototypeDisplayedResult);
        auto outcome=prototypePresentation.ReturnOutcome(prototypeDisplayedResult);
        ReleaseSRWLockShared(&prototypeResultLock);
        if(!current || outcome==Outcome::Submitted || outcome==Outcome::Stale)prototypeResultPanel.Close();
        else if(outcome==Outcome::Failed)prototypeResultPanel.ShowReturnUnavailable();
    }
    if(prototypeResultPanel.IsOpen())return;
    DWORD foreground=0;GetWindowThreadProcessId(GetForegroundWindow(),&foreground);
    if(foreground!=GetCurrentProcessId() || InterlockedCompareExchange(&exitPending,0,0)!=0)return;
    TwinKartObjective::PrototypeSettlement value;
    TwinKartObjective::PrototypePresentationGrant grant;
    AcquireSRWLockExclusive(&prototypeResultLock);
    bool available=prototypeResults.Take(value);
    if(available)grant=prototypePresentation.Qualify({value.world,value.index,value.serial,108});
    ReleaseSRWLockExclusive(&prototypeResultLock);
    if(!available) {
        TwinKartObjective::HideMatchSummary summary;
        AcquireSRWLockExclusive(&prototypeResultLock);
        bool hide=hideSettlementAvailable;if(hide){summary=hideSettlement;hideSettlementAvailable=false;
            grant=prototypePresentation.Qualify({summary.world,summary.index,summary.serial,109});}
        ReleaseSRWLockExclusive(&prototypeResultLock);
        if(!hide || !summary.Valid() || !grant.generation)return;
        wchar_t message[2048]{};
        wcscpy_s(message,L"\u8eb2\u732b\u732b\uff1a\u4fee\u590d\u7248\u6d4b\u8bd5\u573a\u5730\n\n\u672c\u5c40\u5df2\u7ed3\u675f\u3002\n\n");
        for(int i=0;i<summary.rounds;++i) {
            wchar_t row[96]{};swprintf_s(row,L"\u7b2c %d \u8f6e\uff1a%s\n",i+1,summary.outcomes[i]==2?
                L"\u641c\u7d22\u65b9\u6293\u83b7\u4e86\u5168\u90e8\u8eb2\u85cf\u8005":L"\u8eb2\u85cf\u65b9\u5b58\u6d3b\u5230\u672c\u8f6e\u9650\u65f6");
            wcscat_s(message,row);
        }
        wcscat_s(message,L"\nEsc\uff1a\u5173\u95ed\u7ed3\u7b97\uff0c\u7ee7\u7eed\u67e5\u770b\u3002");
        AcquireSRWLockShared(&prototypeResultLock);bool current=prototypePresentation.Current(grant);ReleaseSRWLockShared(&prototypeResultLock);
        if(!current)return;prototypeDisplayedResult=grant;
        if(!prototypeResultPanel.Open(module,gameWindow,L"TwinKart \u8eb2\u732b\u732b\u7ed3\u7b97",message)) {
            AcquireSRWLockExclusive(&prototypeResultLock);
            if(prototypePresentation.Current(grant)){hideSettlement=summary;hideSettlementAvailable=true;}
            ReleaseSRWLockExclusive(&prototypeResultLock);
            RuntimeEvent("native_hide_result_panel_create_failed",-1,GetLastError());
        }
        return;
    }
    auto winner=TwinKartObjective::PrototypeWinnerText(value.winner);
    if(!winner || !grant.generation){RuntimeEvent("native_chaser_result_grant_rejected",-1,value.winner);return;}
    wchar_t message[512]{};
    swprintf_s(message,L"\u8ffd\u9010\u8d5b\uff08\u517c\u5bb9\u6a21\u5f0f\uff09\u5df2\u7ed3\u675f\n\n\u672c\u5c40\u80dc\u65b9\uff1a%s\n\n"
        L"Esc\uff1a\u5173\u95ed\u7ed3\u7b97\uff0c\u7ee7\u7eed\u67e5\u770b\u3002",winner);
    // The worker displays a snapshot only. All UObject work and the return
    // action are serialized on the game thread, with serial/world rechecks.
    AcquireSRWLockShared(&prototypeResultLock);bool current=prototypePresentation.Current(grant);ReleaseSRWLockShared(&prototypeResultLock);
    if(!current)return;prototypeDisplayedResult=grant;
    if(!prototypeResultPanel.Open(module,gameWindow,L"TwinKart \u8ffd\u9010\u8d5b\u7ed3\u7b97",message)) {
        AcquireSRWLockExclusive(&prototypeResultLock);if(prototypePresentation.Current(grant))prototypeResults.Restore(value);ReleaseSRWLockExclusive(&prototypeResultLock);
        RuntimeEvent("native_chaser_result_panel_create_failed",-1,GetLastError());
    }else RuntimeEvent("native_chaser_result_panel_opened",-1,value.winner);
}
void CheckHideHint() {
    const LONG status=InterlockedCompareExchange(&hideHintState,0,0);
    const LONG chase=InterlockedCompareExchange(&chaserHintState,0,0);
    const LONG64 flag=InterlockedCompareExchange64(&TwinKartLocalRace::flagHudPacket,0,0);
    const LONG factory=InterlockedCompareExchange(&TwinKartLocalRace::factoryHudPacket,0,0);
    const auto practiceAt=InterlockedCompareExchange64(&practiceHeartbeat,0,0);
    const LONG practice=practiceAt && GetTickCount64()-practiceAt<600?InterlockedCompareExchange(&practiceHud,0,0):0;
    if(practice && !status && !chase && !flag && !factory) {
        hideHintPanel.Show(module,gameWindow,practice==2?
            L"\u81ea\u7531\u9a7e\u9a76\uff5c\u4e0d\u8bb0\u5f55\u6210\u7ee9\nEsc \u6682\u505c\uff0c\u9009\u62e9\u7ed3\u675f\u5373\u53ef\u8fd4\u56de\u9009\u56fe":
            L"F8\uff1a\u8fdb\u5165\u81ea\u7531\u9a7e\u9a76\uff08\u672c\u5c40\u4e0d\u8bb0\u6210\u7ee9\uff09");
        return;
    }
    if((!status && !chase && !flag && !factory) || prototypeResultPanel.IsOpen() || InterlockedCompareExchange(&exitPending,0,0)) {
        hideHintPanel.Close();return;
    }
    if(factory && !status && !chase && !flag) {
        const int phase=factory&3,items=(factory>>2)&3,round=(factory>>5)&7,seconds=(factory>>8)&4095;
        wchar_t message[180]{},inventory[26]{},clock[28]{},stage[24]{};
        if(phase==2 && items<=2)swprintf_s(inventory,L" \u00b7 \u9053\u5177 %d",items);
        if(phase==2 && seconds<4095)swprintf_s(clock,L" \u00b7 %02d:%02d",seconds/60,seconds%60);
        if(phase==1 && seconds>=1 && seconds<=3)swprintf_s(clock,L" \u00b7 %d",seconds);
        if(phase==2 && round>=1 && round<=TwinKartFactory::Laps)swprintf_s(stage,L" \u00b7 \u7b2c %d/%d \u8f6e",round,TwinKartFactory::Laps);
        swprintf_s(message,L"\u5de5\u5382\u9003\u8131\uff5c%s%s%s%s\n%s",
            phase==1?L"\u51c6\u5907\u53d1\u8f66":L"\u6bd4\u8d5b\u8fdb\u884c\u4e2d",stage,clock,inventory,
            (factory&16)?L"\u4f60\u5df2\u53d8\u6210\u673a\u5668\u4eba\u3002":
                L"\u907f\u5f00\u673a\u5668\u4eba\uff0c\u4e0e\u961f\u53cb\u5b8c\u6210\u4e94\u5708\u9003\u8131\u3002");
        hideHintPanel.Show(module,gameWindow,message);return;
    }
    if(flag && !status && !chase) {
        const int phase=static_cast<int>(flag&3),team=static_cast<int>((flag>>2)&3),carrying=static_cast<int>((flag>>4)&1);
        const int items=static_cast<int>((flag>>5)&3),winner=static_cast<int>((flag>>7)&3);
        const int red=static_cast<int>((flag>>9)&65535),blue=static_cast<int>((flag>>25)&65535);
        wchar_t message[180]{},inventory[26]{};
        if(items<=2)swprintf_s(inventory,L" \u00b7 \u9053\u5177 %d",items);
        const auto teamName=team==1?L"\u7ea2\u961f":L"\u84dd\u961f";
        if(phase==2)swprintf_s(message,L"\u593a\u65d7\u8d5b\uff5c\u7ea2\u961f %d : %d \u84dd\u961f\n%s\u83b7\u80dc",red,blue,winner==1?L"\u7ea2\u961f":L"\u84dd\u961f");
        else swprintf_s(message,L"\u593a\u65d7\u8d5b\uff5c\u7ea2\u961f %d : %d \u84dd\u961f\n\u4f60\u5728%s\uff5c%s%s",red,blue,teamName,
            carrying?L"\u5df2\u6301\u65d7\uff0c\u5e26\u56de\u5df1\u65b9\u57fa\u5730":L"\u62fe\u53d6\u65d7\u5e1c\u5e76\u5e26\u56de\u57fa\u5730",inventory);
        hideHintPanel.Show(module,gameWindow,message);return;
    }
    if(!status) {
        const int role=chase&7,seconds=(chase>>3)&1023;
        const int taggers=(chase>>13)&31,runaways=(chase>>18)&31;
        wchar_t message[180]{};
        if(role==1)wcscpy_s(message,L"\u8ffd\u9010\u8d5b\uff5c\u6d77\u6ee8\u7ade\u6280\u573a\uff08\u517c\u5bb9\u573a\u5730\uff09\n\u6b63\u5728\u51c6\u5907\u573a\u5730\u548c\u5206\u914d\u8eab\u4efd\u3002");
        else {
            wchar_t clock[36]{};
            if(seconds<=600)swprintf_s(clock,L" %02d:%02d",seconds/60,seconds%60);
            const auto identity=role==2?L"\u9003\u8131\u8005":L"\u8ffd\u9010\u8005";
            const auto goal=role==2?L"\u907f\u5f00\u8ffd\u9010\u8005\uff0c\u6536\u96c6\u5e76\u4ea4\u4ed8\u8d44\u6e90":L"\u78b0\u649e\u9003\u8131\u8005\u6765\u8f6c\u5316\u4ed6\u4eec";
            swprintf_s(message,L"%s%s\uff5c\u8ffd\u9010 %d \u00b7 \u9003\u8131 %d\n%s",identity,clock,taggers,runaways,goal);
            const unsigned first=(chase>>23)&3,second=(chase>>25)&3;
            constexpr const wchar_t* names[]{L"\u7a7a",L"\u6682\u4e0d\u53ef\u7528",L"\u53ef\u7528",L"\u672a\u8bfb\u53d6"};
            if(first!=3 || second!=3) {
                wchar_t inventory[62]{};
                swprintf_s(inventory,L"\n\u9053\u5177 1\uff1a%s \u00b7 2\uff1a%s\uff08\u6309 2 \u4f7f\u7528\uff09",names[first],names[second]);
                wcscat_s(message,inventory);
            }
        }
        hideHintPanel.Show(module,gameWindow,message);return;
    }
    const int phase=status&15,seconds=(status>>4)&511,disguise=(status>>13)&7;
    wchar_t message[180]{};
    switch(phase) {
    case 1:wcscpy_s(message,L"\u8eb2\u732b\u732b\uff5c\u6b63\u5728\u5e03\u7f6e\u573a\u5730\u548c\u5206\u914d\u8eab\u4efd\n\u8eb2\u85cf\u8005\u4f2a\u88c5\u6210\u9053\u5177\uff0c\u641c\u5bfb\u8005\u9700\u8981\u627e\u5230\u5e76\u78b0\u5230\u4ed6\u4eec\u3002");break;
    case 2:
        if(seconds)swprintf_s(message,L"\u4f60\u662f\u8eb2\u85cf\u8005\uff5c\u641c\u5bfb\u5c06\u5728 %d \u79d2\u540e\u5f00\u59cb\n\u7528 WASD \u79fb\u52a8\uff0c\u627e\u5730\u65b9\u8eb2\u597d\u3002",seconds);
        else wcscpy_s(message,L"\u4f60\u662f\u8eb2\u85cf\u8005\uff5c\u51c6\u5907\u9636\u6bb5\n\u7528 WASD \u79fb\u52a8\uff0c\u627e\u5730\u65b9\u8eb2\u597d\u3002");
        break;
    case 3:
        if(seconds)swprintf_s(message,L"\u4f60\u662f\u641c\u5bfb\u8005\uff5c\u641c\u5bfb\u5c06\u5728 %d \u79d2\u540e\u5f00\u59cb\n\u5148\u8ba9\u5bf9\u624b\u8eb2\u597d\uff0c\u73b0\u5728\u6682\u65f6\u4e0d\u80fd\u79fb\u52a8\u3002",seconds);
        else wcscpy_s(message,L"\u4f60\u662f\u641c\u5bfb\u8005\uff5c\u7b49\u5f85\u641c\u5bfb\u5f00\u59cb\n\u5148\u8ba9\u5bf9\u624b\u8eb2\u597d\uff0c\u73b0\u5728\u6682\u65f6\u4e0d\u80fd\u79fb\u52a8\u3002");
        break;
    case 4:swprintf_s(message,L"\u4f60\u662f\u8eb2\u85cf\u8005\uff5c\u8fd8\u6709 %d \u79d2\n\u907f\u5f00\u641c\u5bfb\u8005\uff0c\u522b\u88ab\u78b0\u5230\u3002",seconds);break;
    case 5:swprintf_s(message,L"\u4f60\u662f\u641c\u5bfb\u8005\uff5c\u8fd8\u6709 %d \u79d2\n\u7528 WASD \u79fb\u52a8\uff0c\u9760\u8fd1\u5e76\u78b0\u5230\u53ef\u7591\u7684\u9053\u5177\u3002",seconds);break;
    case 6:wcscpy_s(message,L"\u5df2\u88ab\u627e\u5230\uff5c\u7b49\u5f85\u672c\u8f6e\u7ed3\u675f");break;
    case 7:wcscpy_s(message,L"\u672c\u8f6e\u7ed3\u675f\uff5c\u7b49\u5f85\u6362\u8fb9\u6216\u7ed3\u7b97");break;
    default:break;
    }
    constexpr const wchar_t* disguises[]{L"\u68d5\u6988\u6811",L"\u9c7c\u644a",L"\u62db\u724c",L"\u7389\u7c73"};
    if((phase==2 || phase==4) && disguise>0 && disguise<=ARRAYSIZE(disguises)) {
        wcscat_s(message,L" \u00b7 \u5f53\u524d\u4f2a\u88c5\uff1a");
        wcscat_s(message,disguises[disguise-1]);
    }
    hideHintPanel.Show(module,gameWindow,message);
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
BYTE* NativeAccountModule(BYTE* account,uintptr_t getter) {
    if(!TwinKartLocalRace::Live(account) || GetCurrentThreadId()!=queue.ThreadId())return nullptr;
    using Type=void*(__fastcall*)();auto type=reinterpret_cast<Type>(image+getter)();
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
BYTE* NativeTrackSelection(BYTE* account) {return NativeAccountModule(account,0x2210ea0);}
TwinKartNative::OwnedCodePatch offlineRoomNotificationPatch;
void __fastcall OfflineRoomNotification(void* delegate) {
    // One original notification site, after native room deserialization and
    // before the original UI subscribers run. A local room has no online
    // matchmaking phase. Use its own original reset/broadcast implementation;
    // do not hide the timer widget or write matching-state bytes ourselves.
    auto custom=static_cast<BYTE*>(delegate)-0x240;
    __try {
        using Type=void*(__fastcall*)();
        if(GetCurrentThreadId()==queue.ThreadId() && TwinKartLocalRace::Live(custom) &&
           !(*reinterpret_cast<unsigned*>(custom+8)&0x18030) &&
           *reinterpret_cast<void**>(custom+0x10)==reinterpret_cast<Type>(image+0x21eae30)()) {
            auto account=*reinterpret_cast<BYTE**>(custom+0x20);
            auto match=NativeAccountModule(account,0x21fd280);
            if(match && match[0x30]) {
                int before=match[0x30];
                reinterpret_cast<void(__fastcall*)(void*)>(image+0x1ad69a0)(match);
                RuntimeEvent("native_offline_room_matching_reset",before,match[0x30]);
            }
            RuntimeEvent("native_offline_room_notification",custom[0x30],*reinterpret_cast<int*>(custom+0x88));
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {RuntimeEvent("native_offline_room_notification_rejected",-1,0);}
    reinterpret_cast<void(__fastcall*)(void*)>(image+0x9143c0)(delegate);
}
bool InstallOfflineRoomNotification() {
    const BYTE reset[]{0xc6,0x41,0x30,0x00,0x48,0x81,0xc1,0xb0,0x01,0x00,0x00};
    if(memcmp(image+0x1ad69a0,reset,sizeof(reset)))return false;
    return offlineRoomNotificationPatch.Call(image+0x1ac0b33,image+0x9143c0,reinterpret_cast<void*>(OfflineRoomNotification));
}
void InitializeRuntime(void* raw) {
    auto& value=*static_cast<Query*>(raw); value.executedThread=GetCurrentThreadId();
    bool ready=TwinKartLocalRace::Install(image,RuntimeEvent) && TwinKartLocalRace::InstallEventDiagnostics() &&
        TwinKartLocalRace::InstallFactoryCompatibility() && InstallHideCaptureCompatibility() && InstallChaserModeDefinition() &&
        InstallOfflineRoomNotification() && InstallLoadingPresentation();
    if(ready)TwinKartLocalRace::arenaKartTick=RecoverArenaHumanKartTick;
    wchar_t flagDiagnostic[4]{},chaserDiagnostic[4]{};
    bool diagnostics=(GetEnvironmentVariableW(L"TWIN_KART_FLAG_DIAGNOSTICS",flagDiagnostic,4)==1 && flagDiagnostic[0]==L'1') ||
        (GetEnvironmentVariableW(L"TWIN_KART_CHASER_DIAGNOSTICS",chaserDiagnostic,4)==1 && chaserDiagnostic[0]==L'1');
    if(ready && diagnostics) {
        TwinKartLocalRace::flagDiagnostics=true;AddVectoredExceptionHandler(1,CaptureFlagFault);
    }
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
bool ModeNameText(unsigned long long name,wchar_t* destination,size_t capacity) {
    NativeString value{};bool valid=false;
    __try {
        using Text=NativeString*(__fastcall*)(NativeString*,unsigned long long);
        reinterpret_cast<Text>(image+0x3bcd370)(&value,name);
        valid=value.data && value.count>1 && static_cast<size_t>(value.count)<=capacity && value.capacity>=value.count && !value.data[value.count-1];
        if(valid)memcpy(destination,value.data,static_cast<size_t>(value.count)*sizeof(wchar_t));
    } __finally {if(value.data)reinterpret_cast<void(__fastcall*)(void*)>(image+0x248b950)(const_cast<wchar_t*>(value.data));}
    return valid;
}
TwinKartCamera::ObjectIdentity offlineModeListLayout{};
ULONGLONG offlineModeLayoutRetry=0;
bool UiName(BYTE* object,const wchar_t* expected,bool className=false) {
    if(!TwinKartLocalRace::Live(object))return false;
    auto selected=className?*reinterpret_cast<BYTE**>(object+0x10):object;wchar_t name[128]{};
    return TwinKartLocalRace::Live(selected) && ModeNameText(*reinterpret_cast<unsigned long long*>(selected+0x18),name,ARRAYSIZE(name)) && !wcscmp(name,expected);
}
void MaintainOfflineModeLayout() {
    if(GetCurrentThreadId()!=queue.ThreadId() || GetTickCount64()<offlineModeLayoutRetry)return;
    offlineModeLayoutRetry=GetTickCount64()+1000;
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);int count=*reinterpret_cast<int*>(image+0x69c28c4);
    if(TwinKartCamera::Live(offlineModeListLayout,chunks,count))return;
    if(!chunks || count<1 || count>2000000)return;
    const BYTE widthPrefix[]{0x48,0x83,0xec,0x48,0x80,0x89,0x50,0x01,0x00,0x00,0x01};
    const BYTE focusPrefix[]{0x88,0x91,0x50,0x08,0x00,0x00,0x48,0x8b,0x89,0x70,0x08,0x00,0x00};
    const BYTE visiblePrefix[]{0x88,0x91,0x29,0x08,0x00,0x00,0x48,0x8b,0x89,0x70,0x08,0x00,0x00};
    if(memcmp(image+0x3ab2150,widthPrefix,sizeof(widthPrefix)) || memcmp(image+0x3aaf6c0,focusPrefix,sizeof(focusPrefix)) ||
       memcmp(image+0x3aaf610,visiblePrefix,sizeof(visiblePrefix)))return;
    for(int index=0;index<count;++index) {
        auto chunk=chunks[index/65536];if(!chunk || (chunk[index%65536].flags&0x30000000))continue;
        auto list=static_cast<BYTE*>(chunk[index%65536].object);
        if(!TwinKartLocalRace::Live(list) || (*reinterpret_cast<unsigned*>(list+8)&0x18030) ||
           *reinterpret_cast<void**>(list)!=image+0x5383ba0 || !UiName(list,L"BP_ModeSelect_ModeLab_ButtonList_C",true))continue;
        auto scroll=*reinterpret_cast<BYTE**>(list+0x2a0);auto slot=*reinterpret_cast<BYTE**>(list+0x28);
        if(!UiName(scroll,L"ScrollBox",true) || !UiName(slot,L"SizeBoxSlot",true) || *reinterpret_cast<BYTE**>(slot+0x30)!=list)continue;
        auto box=*reinterpret_cast<BYTE**>(slot+0x28);
        if(!UiName(box,L"SizeBox",true) || !UiName(box,L"SB_List") ||
           *reinterpret_cast<void**>(box+0x20)!=*reinterpret_cast<void**>(list+0x20) || scroll[0x828]!=0)continue;
        TwinKartCamera::ObjectIdentity listId{},boxId{},scrollId{};
        if(!TwinKartLocalRace::CaptureFlagObject(list,listId) || !TwinKartLocalRace::CaptureFlagObject(box,boxId) ||
           !TwinKartLocalRace::CaptureFlagObject(scroll,scrollId))continue;
        // The stock horizontal ScrollBox already owns all native cards. Its
        // unconstrained desired width grows with the full list, causing both
        // enclosing ScaleBoxes to shrink the entire page. Bound only SB_List
        // in the original 1920-unit UI design space, then let Slate scroll.
        reinterpret_cast<void(__fastcall*)(void*,float)>(image+0x3ab2150)(box,1920.0f);
        chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);count=*reinterpret_cast<int*>(image+0x69c28c4);
        if(!TwinKartCamera::Live(listId,chunks,count) || !TwinKartCamera::Live(boxId,chunks,count) || !TwinKartCamera::Live(scrollId,chunks,count))return;
        reinterpret_cast<void(__fastcall*)(void*,unsigned char)>(image+0x3aaf6c0)(scroll,1); // InstantScroll on native focus changes.
        chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);count=*reinterpret_cast<int*>(image+0x69c28c4);
        if(!TwinKartCamera::Live(listId,chunks,count) || !TwinKartCamera::Live(boxId,chunks,count) || !TwinKartCamera::Live(scrollId,chunks,count))return;
        reinterpret_cast<void(__fastcall*)(void*,unsigned char)>(image+0x3aaf610)(scroll,0); // Original visible scroll bar.
        if(*reinterpret_cast<float*>(box+0x130)!=1920.0f || !(box[0x150]&1) || scroll[0x850]!=1)return;
        offlineModeListLayout=listId;
        RuntimeEvent("native_offline_mode_list_scroll_layout",*reinterpret_cast<int*>(list+0x298),1920);
        return;
    }
}
void* __fastcall HideArenaModeOverride(void* raw,void* outputClass,void* selected,const NativeString* map,
                                      const NativeString* options,const NativeString* portal) {
    using Override=void*(__fastcall*)(void*,void*,void*,const NativeString*,const NativeString*,const NativeString*);
    auto instance=static_cast<BYTE*>(raw);
    auto original=reinterpret_cast<Override>((*reinterpret_cast<void***>(instance))[0x348/8]);
    // The original virtual implementation, hidden return buffer and all six
    // arguments are preserved. No existing game-mode instance is replaced.
    auto result=original(raw,outputClass,selected,map,options,portal);
    bool chaser=chaserArenaEnabled && TwinKartLocalRace::armed && TwinKartLocalRace::armedMode==108;
    if((!chaser && (!hideCaptureArmed || !hideArenaCapture)) || GetCurrentThreadId()!=queue.ThreadId() || result!=outputClass ||
       !outputClass || !TwinKartLocalRace::Live(instance) || !map || !map->data || map->count<2 || map->count>256 ||
       map->capacity<map->count || map->data[map->count-1])return result;
    using Type=void*(__fastcall*)();
    bool owned=*reinterpret_cast<void**>(instance+0x10)==reinterpret_cast<Type>(image+0x2321310)();
    auto& actual=*static_cast<void**>(outputClass);
    auto chosen=TwinKartObjective::ObjectiveArenaModeClass(true,true,owned,map->data,actual,
        reinterpret_cast<Type>(image+0x22a77b0)(),reinterpret_cast<Type>(image+(chaser?0x22507c0:0x10f34f0))());
    if(chosen!=actual){actual=chosen;RuntimeEvent(chaser?"native_chaser_arena_original_mode_class_selected":"native_hide_arena_original_mode_class_selected",-1,chaser?108:109);}
    return result;
}
void* __fastcall HideBaseConstructor(void* raw) {
    auto result=hideBaseConstructor(raw);auto mode=static_cast<BYTE*>(result);
    if(!hideCaptureArmed || hideControllerTarget!=8 || GetCurrentThreadId()!=queue.ThreadId() ||
       !TwinKartLocalRace::Live(mode))return result;
    using Type=void*(__fastcall*)();using GetWorld=BYTE*(__fastcall*)(void*);
    if(*reinterpret_cast<void**>(mode+0x10)!=reinterpret_cast<Type>(image+0x10f34f0)())return result;
    auto world=reinterpret_cast<GetWorld>((*reinterpret_cast<void***>(mode))[0x160/8])(mode);wchar_t name[128]{};
    if(!TwinKartLocalRace::Live(world) || *reinterpret_cast<void**>(world+0x38) ||
       !ModeNameText(*reinterpret_cast<unsigned long long*>(world+0x18),name,ARRAYSIZE(name)) ||
       wcscmp(name,hideArenaCapture?L"Flag_F01":L"HideNSeekTestLevel"))return result;
    auto& current=*reinterpret_cast<void**>(mode+0x230);
    auto selected=TwinKartObjective::HideSessionClass(true,(*reinterpret_cast<unsigned*>(mode+8)&0x30)!=0,current,
        reinterpret_cast<Type>(image+0x2323670)(),reinterpret_cast<Type>(image+0x22a23e0)());
    if(selected!=current) {current=selected;RuntimeEvent("native_hide_original_local_session_selected",-1,1);}
    return result;
}
bool OwnedHideRules(BYTE* rules) {
    using Type=void*(__fastcall*)();
    return hideCaptureArmed && GetCurrentThreadId()==queue.ThreadId() && TwinKartLocalRace::Live(rules) &&
        !(*reinterpret_cast<unsigned*>(rules+8)&0x30) &&
        *reinterpret_cast<void**>(rules+0x10)==reinterpret_cast<Type>(image+0x10f3df0)() &&
        TwinKartLocalRace::objectiveWorld.Matches(TwinKartLocalRace::World(rules),
            *reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),*reinterpret_cast<int*>(image+0x69c28c4));
}
bool HideMaterialIdentityLive(const TwinKartCamera::ObjectIdentity& value);
bool HideRolePawn(BYTE* pawn,BYTE* world) {
    using GetWorld=BYTE*(__fastcall*)(void*);
    return TwinKartLocalRace::Live(pawn) && !(*reinterpret_cast<unsigned*>(pawn+8)&0x30) &&
        !(pawn[0x5c]&0x10) &&
        (*reinterpret_cast<BYTE**>(pawn)==image+0x5017c10 || *reinterpret_cast<BYTE**>(pawn)==image+0x501a0c8) &&
        reinterpret_cast<GetWorld>((*reinterpret_cast<void***>(pawn))[0x160/8])(pawn)==world;
}
using HideHumanAxis=void(__fastcall*)(void*,float);
HideHumanAxis hideOriginalHumanAxis[3]{};
TwinKartNative::OwnedCodePatch hideHumanAxisPatch[3];
bool HideHumanGameplayInput(BYTE* world) {
    DWORD foreground=0;auto window=GetForegroundWindow();GetWindowThreadProcessId(window,&foreground);
    return world && foreground==GetCurrentProcessId() && window==gameWindow &&
        InterlockedCompareExchange(&exitPending,0,0)==0 && !(GetAsyncKeyState(VK_MENU)&0x8000) &&
        !reinterpret_cast<bool(__fastcall*)(void*)>(image+0x4011c70)(world);
}
bool HideHumanKey(BYTE* controller,unsigned keyIndex) {
    struct Key {unsigned long long name;void* detail;void* reference;};
    static constexpr const wchar_t* names[]{L"W",L"S",L"A",L"D"};
    static unsigned long long ids[4]{};
    if(!ids[keyIndex])reinterpret_cast<void(__fastcall*)(void*,const wchar_t*,int)>(image+0x2589130)(&ids[keyIndex],names[keyIndex],0);
    Key key{ids[keyIndex],nullptr,nullptr};using Query=bool(__fastcall*)(void*,Key*);
    return key.name && (reinterpret_cast<Query>(image+0x41d8d60)(controller,&key) ||
        reinterpret_cast<Query>(image+0x41e4930)(controller,&key));
}
float RecoverHideHumanAxis(BYTE* pawn,float value,unsigned axis) {
    if(value!=0.f || !hideLobbyEnabled || !hideCaptureArmed || !hideRoundRosterKnown || !hideRoundInputsActive ||
       GetCurrentThreadId()!=queue.ThreadId())return value;
    __try {
        auto world=OwnedRaceWorld();
        if(!world || !HideRolePawn(pawn,world) || *reinterpret_cast<BYTE**>(pawn)!=image+0x5017c10)return value;
        auto controller=*reinterpret_cast<BYTE**>(pawn+0x258);
        if(!TwinKartLocalRace::Live(controller) || !TwinKartObjective::HideControllerHasPlayer(controller) ||
           *reinterpret_cast<void**>(controller+0x250)!=pawn)return value;
        bool current=false;
        for(unsigned i=0;i<8;++i)if(hideRoundPawns[i].object==pawn && hideDisguiseControllers[i].object==controller &&
           HideMaterialIdentityLive(hideRoundPawns[i]) && HideMaterialIdentityLive(hideDisguiseControllers[i]))current=true;
        if(!current || !HideHumanGameplayInput(world))return value;
        if(!axis){bool forward=HideHumanKey(controller,0),back=HideHumanKey(controller,1);return float(forward)-float(back);}
        bool left=HideHumanKey(controller,2),right=HideHumanKey(controller,3);
        return left==right?0.f:axis==1 && right?1.f:axis==2 && left?-1.f:0.f;
    } __except(EXCEPTION_EXECUTE_HANDLER){return value;}
}
float RecoverHideHumanSeekerAxis(BYTE* movement,float value) {
    if((value!=0.f && value!=-1.f) || !hideLobbyEnabled || !hideSearchActive || !hideRoundInputsActive || !hideRoundRosterKnown ||
       GetCurrentThreadId()!=queue.ThreadId())return value;
    __try {
        if(!TwinKartLocalRace::Live(movement) || *reinterpret_cast<BYTE**>(movement)!=image+0x5118590)return value;
        auto world=OwnedRaceWorld(),owner=*reinterpret_cast<BYTE**>(movement+0xa0);
        if(!world || !HideRolePawn(owner,world) || *reinterpret_cast<BYTE**>(owner)!=image+0x501a0c8)return value;
        for(unsigned i=0;i<8;++i) {
            if(hideRoundPawns[i].object!=owner || !(hideAiEnabled&(1u<<i)) || !HideMaterialIdentityLive(hideRoundPawns[i]) ||
               !HideMaterialIdentityLive(hideDisguiseControllers[i]))continue;
            auto controller=static_cast<BYTE*>(hideDisguiseControllers[i].object);
            if(!TwinKartObjective::HideControllerHasPlayer(controller) || *reinterpret_cast<void**>(controller+0x250)!=owner ||
               *reinterpret_cast<void**>(owner+0x258)!=controller)return value;
            auto ignored=(*reinterpret_cast<BYTE***>(controller))[0x740/8];
            const bool allowed=ignored==image+0x3e9a1f0 && HideHumanGameplayInput(world) &&
                !reinterpret_cast<bool(__fastcall*)(void*)>(ignored)(controller);
            // Repair the original axis argument before the native input
            // dispatcher applies it; never race its zero/release in a rule tick.
            const bool forward=allowed && HideHumanKey(controller,0);
            const bool reverse=allowed && HideHumanKey(controller,1);
            if(value==0.f && forward && !reverse) {
                static TwinKartCamera::ObjectIdentity logged{};
                if(logged.object!=owner || logged.serial!=hideRoundPawns[i].serial) {
                    logged=hideRoundPawns[i];RuntimeEvent("native_hide_seeker_forward_axis_recovered",i,1);
                }
                return 1.f;
            }
            // Exactly -1 is an original special command that negates the
            // previous throttle. From rest that leaves zero. Use the native
            // proportional reverse range for an actual held S key instead.
            if(reverse && !forward) {
                static TwinKartCamera::ObjectIdentity logged{};
                if(logged.object!=owner || logged.serial!=hideRoundPawns[i].serial) {
                    logged=hideRoundPawns[i];RuntimeEvent("native_hide_seeker_reverse_axis_recovered",i,1);
                }
                return -.85f;
            }
            return value;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER){}
    return value;
}
void __fastcall HideHumanForward(void* pawn,float value){hideOriginalHumanAxis[0](pawn,RecoverHideHumanAxis(static_cast<BYTE*>(pawn),value,0));}
void __fastcall HideHumanRight(void* pawn,float value){hideOriginalHumanAxis[1](pawn,RecoverHideHumanAxis(static_cast<BYTE*>(pawn),value,1));}
void __fastcall HideHumanLeft(void* pawn,float value){hideOriginalHumanAxis[2](pawn,RecoverHideHumanAxis(static_cast<BYTE*>(pawn),value,2));}
void InstallHideHumanAxis() {
    const unsigned targets[]{0x10eea60,0x10ee980,0x10eea50};
    const HideHumanAxis replacements[]{HideHumanForward,HideHumanRight,HideHumanLeft};
    for(unsigned i=0;i<3;++i) {
        BYTE prefix[]{0x48,0x8b,0x89,0xd0,0x04,0,0,0x48,0x8b,0x01,0x48,0xff,0x60,static_cast<BYTE>(8*(i+1))};
        if(!hideHumanAxisPatch[i].Entry(image+targets[i],prefix,sizeof(prefix),reinterpret_cast<void*>(replacements[i]),
                reinterpret_cast<void**>(&hideOriginalHumanAxis[i]))) {
            for(unsigned j=0;j<i;++j)hideHumanAxisPatch[j].Restore();
            RuntimeEvent("native_hide_human_axis_recovery_unavailable",i,0);return;
        }
    }
    RuntimeEvent("native_hide_human_axis_recovery_installed",-1,3);
}
using HideHiderView=void(__fastcall*)(void*,float,void*);
HideHiderView hideOriginalHiderView=nullptr;
TwinKartNative::OwnedCodePatch hideHiderViewPatch;
TwinKartCamera::ObjectIdentity hideLocalDisguiseActor{};
int hideLocalDisguiseKind=0; // Game thread; published together with the current role.
void __fastcall HideHiderViewWithWallClearance(void* raw,float delta,void* outputView) {
    hideOriginalHiderView(raw,delta,outputView);
    if(!outputView || !hideRoundInputsActive || !hideRoundRosterKnown || !hideRepairedScene.ready ||
       GetCurrentThreadId()!=queue.ThreadId())return;
    __try {
        using namespace TwinKartRace;
        auto world=OwnedRaceWorld(),camera=static_cast<BYTE*>(raw);
        if(!world || hideRepairedScene.world.object!=world || !HideMaterialIdentityLive(hideRepairedScene.world) ||
           !TwinKartLocalRace::Live(camera) || *reinterpret_cast<BYTE**>(camera)!=image+0x501ce08)return;
        auto pawn=*reinterpret_cast<BYTE**>(camera+0xa0);
        if(pawn!=*reinterpret_cast<BYTE**>(camera+0x898) || !HideRolePawn(pawn,world) ||
           *reinterpret_cast<BYTE**>(pawn)!=image+0x5017c10)return;
        for(unsigned i=0;i<8;++i) {
            if(hideRoundPawns[i].object!=pawn || !HideMaterialIdentityLive(hideRoundPawns[i]) ||
               !HideMaterialIdentityLive(hideDisguiseControllers[i]))continue;
            auto controller=static_cast<BYTE*>(hideDisguiseControllers[i].object);
            if(!TwinKartObjective::HideControllerHasPlayer(controller) || *reinterpret_cast<void**>(controller+0x250)!=pawn ||
               *reinterpret_cast<void**>(pawn+0x258)!=controller)return;
            ArenaPoint pivot{},eye{};memcpy(&eye,outputView,sizeof(eye));
            if(!TwinKartLocalRace::FlagPosition(pawn,pivot) || !Finite(eye))return;
            pivot.z+=90.f;
            static int staticQuery=-1;
            if(staticQuery<0)for(int q=0;q<32;++q)if(reinterpret_cast<unsigned char(__fastcall*)(int)>(image+0x3f159a0)(q)==0){staticQuery=q;break;}
            if(staticQuery<0)return;
            void* ignoredActors[2]{pawn,nullptr};int ignoredCount=1;
            BYTE* localDisguise=nullptr;
            auto disguise=TwinKartLocalRace::FlagFind(pawn,0x10f43e0);
            if(disguise) {
                auto child=*reinterpret_cast<BYTE**>(disguise+0x210);
                using GetWorld=BYTE*(__fastcall*)(void*);
                if(TwinKartLocalRace::Live(child) && *reinterpret_cast<void**>(disguise+0xa0)==pawn &&
                   reinterpret_cast<GetWorld>((*reinterpret_cast<void***>(child))[0x160/8])(child)==world) {
                    ignoredActors[ignoredCount++]=child;
                    if(hideLocalDisguiseKind && hideLocalDisguiseActor.object==child && HideMaterialIdentityLive(hideLocalDisguiseActor))
                        localDisguise=child;
                }
            }
            unsigned char channel=static_cast<unsigned char>(staticQuery);
            QueryArray types{&channel,1,1},ignored{ignoredActors,ignoredCount,2};QueryColor color{};
            alignas(16) unsigned char hit[0x90]{};
            auto trace=reinterpret_cast<ObjectTrace>(image+0x3ff5990);
            if(localDisguise) {
                ArenaPoint center{},extent{},stockRotation{},childPivot{};float fov=0.f;
                const auto bytes=static_cast<BYTE*>(outputView);
                memcpy(&stockRotation,bytes+0xc,sizeof(stockRotation));memcpy(&fov,bytes+0x18,sizeof(fov));
                using Bounds=void(__fastcall*)(void*,bool,ArenaPoint*,ArenaPoint*,bool);
                reinterpret_cast<Bounds>(image+0x3cb2830)(localDisguise,false,&center,&extent,false);
                int rejected=0;
                if(TwinKartLocalRace::FlagPosition(localDisguise,childPivot) &&
                   TwinKartObjective::FitHideDisguiseTarget(hideLocalDisguiseKind,pivot,childPivot,center,extent)) {
                    rejected|=4;
                    for(unsigned n=0;n<TwinKartObjective::HideDisguiseViewCount;++n) {
                        ArenaPoint desired{},rotation{};
                        if(!TwinKartObjective::FrameHideDisguise(center,extent,eye,stockRotation.y,fov,n,desired,rotation))break;
                        rejected|=1;
                        // Keep the camera in the market even when a ray
                        // misses the low wall above its collision shape.
                        const auto& bounds=hideRepairedScene.layout;
                        if(desired.x<bounds.low.x || desired.x>bounds.high.x || desired.y<bounds.low.y || desired.y>bounds.high.y)continue;
                        rejected|=2;memset(hit,0,sizeof(hit));
                        if(trace(world,&center,&desired,&types,false,&ignored,0,hit,false,&color,&color,0.f))continue;
                        memcpy(outputView,&desired,sizeof(desired));memcpy(bytes+0xc,&rotation,sizeof(rotation));
                        static TwinKartCamera::ObjectIdentity framed{};
                        if(framed.object!=localDisguise || framed.serial!=hideLocalDisguiseActor.serial) {
                            framed=hideLocalDisguiseActor;RuntimeEvent("native_hide_visible_disguise_framed",i,hideLocalDisguiseKind);
                            RuntimeEvent("native_hide_disguise_camera_choice",i,n);
                        }
                        return;
                    }
                }
                static TwinKartCamera::ObjectIdentity blocked{};
                if(blocked.object!=localDisguise || blocked.serial!=hideLocalDisguiseActor.serial) {
                    blocked=hideLocalDisguiseActor;RuntimeEvent("native_hide_disguise_camera_unresolved",i,rejected);
                }
                memset(hit,0,sizeof(hit));
            }
            if(ArenaDistance(pivot,eye)>2400.f || std::fabs(pivot.z-eye.z)>1000.f)return;
            if(!trace(world,&pivot,&eye,&types,false,&ignored,0,hit,false,&color,&color,0.f) || (hit[0x5d]&3)!=1)return;
            HitWeakIdentity weak{};memcpy(&weak,hit+0x68,sizeof(weak));
            auto actor=reinterpret_cast<ResolveHitWeak>(image+0x2799770)(&weak);
            bool authoredObstacle=false;
            for(unsigned obstacle=16;obstacle<48 && obstacle<hideRepairedScene.actorCount;++obstacle) {
                const auto& id=hideRepairedScene.actors[obstacle];
                authoredObstacle|=id.object==actor && id.index==weak.index && id.serial==weak.serial && HideMaterialIdentityLive(id);
            }
            float time=0;memcpy(&time,hit+4,sizeof(time));
            if(!authoredObstacle || !TwinKartObjective::ClipHideCamera(pivot,eye,time))return;
            memcpy(outputView,&eye,sizeof(eye));
            static TwinKartCamera::ObjectIdentity logged{};
            if(logged.object!=pawn || logged.serial!=hideRoundPawns[i].serial) {
                logged=hideRoundPawns[i];RuntimeEvent("native_hide_hider_camera_wall_clipped",i,static_cast<int>(time*1000));
            }
            return;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER){}
}
void InstallHideHiderView() {
    constexpr BYTE prefix[]{0x48,0x89,0x5c,0x24,0x20,0x57,0x48,0x81,0xec,0xb0,0,0,0,0x0f,0x29,0xb4,0x24,0xa0,0,0,0};
    const bool installed=*reinterpret_cast<BYTE**>(image+0x501ce08+0x548)==image+0x10ebc50 &&
        hideHiderViewPatch.Entry(image+0x10ebc50,prefix,sizeof(prefix),reinterpret_cast<void*>(HideHiderViewWithWallClearance),
            reinterpret_cast<void**>(&hideOriginalHiderView));
    RuntimeEvent("native_hide_hider_camera_wall_adapter_installed",-1,installed?1:0);
}
int ReadNativeHidePreparationSeconds(BYTE* rules) {
    auto config=*reinterpret_cast<BYTE**>(rules+0xb0),helper=*reinterpret_cast<BYTE**>(rules+0xc0);
    using Type=void*(__fastcall*)();
    if(!TwinKartLocalRace::Live(config) || *reinterpret_cast<void**>(config+0x10)!=reinterpret_cast<Type>(image+0x10f3b90)() ||
       !TwinKartLocalRace::Live(helper))return 0;
    auto helperClass=*reinterpret_cast<BYTE**>(helper+0x10);
    if(!TwinKartLocalRace::Live(helperClass))return 0;
    static unsigned long long helperName=0;
    if(!helperName)reinterpret_cast<void(__fastcall*)(void*,const wchar_t*,int)>(image+0x2589130)(&helperName,L"ProgramBuilderHelper",0);
    if(!helperName || *reinterpret_cast<unsigned long long*>(helperClass+0x18)!=helperName)return 0;
    // ProgramBuilderHelper holds 24-byte native shared-task entries. Read
    // only its unique live timer matching this round's RunAway duration.
    struct TaskEntry {const BYTE* task;const void* shared;int next,index;};
    struct TaskSlots {const TaskEntry* data;int count,capacity;unsigned bits[4];const void* secondary;int bitCount,bitCapacity,firstFree,freeCount;};
    static_assert(sizeof(TaskEntry)==24 && offsetof(TaskSlots,bitCount)==40,"original timer set observation");
    const auto& entries=*reinterpret_cast<const TaskSlots*>(helper+0x78);
    if(!entries.data || entries.count<1 || entries.count>8 || entries.capacity<entries.count || entries.capacity>32 ||
       entries.secondary || entries.bitCount!=entries.count || entries.bitCapacity<entries.count || entries.bitCapacity>128 ||
       entries.freeCount<0 || entries.freeCount>entries.count)return 0;
    const float duration=*reinterpret_cast<float*>(config+0x38);
    int seconds=0,occupied=0;
    for(int i=0;i<entries.count;++i)if(entries.bits[0]&(1u<<i)) {
        ++occupied;const auto* task=entries.data[i].task;
        if(!task || *reinterpret_cast<BYTE* const*>(task)!=image+0x5018548)continue;
        int found=TwinKartObjective::HidePreparationSeconds(duration,*reinterpret_cast<const float*>(task+0x30),
            *reinterpret_cast<const float*>(task+0x34));
        if(found && seconds)return 0;
        if(found)seconds=found;
    }
    return occupied==entries.count-entries.freeCount?seconds:0;
}
void PublishHideHint(BYTE* rules) {
    __try {
        if(!hideLobbyEnabled || !OwnedHideRules(rules))return;
        if(hideRoundRosterPending || !hideRoundRosterKnown){InterlockedExchange(&hideHintState,1);return;}
        if(!hideRoundInputsActive){InterlockedExchange(&hideHintState,7);return;}
        int local=-1;
        for(int slot=0;slot<8;++slot) {
            auto controller=reinterpret_cast<BYTE*>(hideRoundSeats[slot].controller);
            if(hideDisguiseControllers[slot].object!=controller || !HideMaterialIdentityLive(hideDisguiseControllers[slot]) ||
               !TwinKartObjective::HideControllerHasPlayer(controller))continue;
            if(local>=0)return;
            local=slot;
        }
        if(local<0)return;
        auto pawn=static_cast<BYTE*>(hideRoundPawns[local].object);
        if(!HideMaterialIdentityLive(hideRoundPawns[local]) || !HideRolePawn(pawn,TwinKartLocalRace::World(rules))) {
            if(hideSearchActive)InterlockedExchange(&hideHintState,6);
            return;
        }
        bool hider=*reinterpret_cast<BYTE**>(pawn)==image+0x5017c10;
        const int phase=hideSearchActive?(hider?4:5):(hider?2:3);
        const int seconds=std::clamp(hideSearchActive?hideSearchClock.RemainingSeconds():ReadNativeHidePreparationSeconds(rules),0,511);
        const LONG previous=InterlockedExchange(&hideHintState,((hider?hideLocalDisguiseKind:0)<<13)|(seconds<<4)|phase);
        if(!hideSearchActive && seconds>0 && !(previous&0x1ff0))RuntimeEvent("native_hide_preparation_countdown_visible",local,seconds);
        if((previous&15)!=phase)RuntimeEvent("native_hide_human_role_hint",local,phase);
    } __except(EXCEPTION_EXECUTE_HANDLER) {}
}
void ForgetHideMaterialRepairs() {
    for(auto& role:hideMaterialBindings)for(auto& binding:role)binding=HideMaterialBinding{};
    memset(hideMaterialSourceMeshes,0,sizeof(hideMaterialSourceMeshes));hideMaterialNext=0;
    memset(hideMaterialSourceRetry,0,sizeof(hideMaterialSourceRetry));memset(hideMaterialSourceAttempts,0,sizeof(hideMaterialSourceAttempts));
}
bool HideMaterialIdentityLive(const TwinKartCamera::ObjectIdentity& value) {
    return TwinKartCamera::Live(value,*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),
        *reinterpret_cast<int*>(image+0x69c28c4));
}
bool ExactHideMeshPath(BYTE* mesh,const wchar_t* package,const wchar_t* objectName) {
    __try {
        if(!TwinKartLocalRace::Live(mesh) || !TwinKartRace::ObjectiveAssetIsA(image,mesh,0x454a5e0))return false;
        auto outer=*reinterpret_cast<BYTE**>(mesh+0x20);wchar_t name[96]{},path[512]{};
        return TwinKartLocalRace::Live(outer) && !*reinterpret_cast<void**>(outer+0x20) &&
            ModeNameText(*reinterpret_cast<unsigned long long*>(mesh+0x18),name,ARRAYSIZE(name)) &&
            ModeNameText(*reinterpret_cast<unsigned long long*>(outer+0x18),path,ARRAYSIZE(path)) &&
            !_wcsicmp(name,objectName) && !_wcsicmp(path,package);
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool HideEngineLoaderFallback(void* material) {
    __try {
        auto object=static_cast<BYTE*>(material);
        if(!TwinKartLocalRace::Live(object) || !TwinKartRace::ObjectiveAssetIsA(image,object,0x44d98e0))return false;
        auto outer=*reinterpret_cast<BYTE**>(object+0x20);wchar_t name[96]{},path[256]{};
        if(!TwinKartLocalRace::Live(outer) || *reinterpret_cast<void**>(outer+0x20) ||
           !ModeNameText(*reinterpret_cast<unsigned long long*>(object+0x18),name,ARRAYSIZE(name)) ||
           !ModeNameText(*reinterpret_cast<unsigned long long*>(outer+0x18),path,ARRAYSIZE(path)))return false;
        return (!_wcsicmp(name,L"WorldGridMaterial") && !_wcsicmp(path,L"/Engine/EngineMaterials/WorldGridMaterial")) ||
            (!_wcsicmp(name,L"DefaultMaterial") && !_wcsicmp(path,L"/Engine/EngineMaterials/DefaultMaterial"));
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool HideKnownSampleMesh(BYTE* mesh) {
    for(const auto& spec:hideMaterialSources)if(ExactHideMeshPath(mesh,spec.samplePackage,spec.sampleName))return true;
    return false;
}
bool HideMaterialScope(HideMaterialBinding& binding,const TwinKartVisual::MaterialRepairKey& key,bool sameMesh) {
    if(GetCurrentThreadId()!=queue.ThreadId() || !hideAutonomyCompatibility || binding.slot<0 || binding.slot>=8 ||
       key.component!=binding.component.object || key.componentSerial!=static_cast<unsigned>(binding.component.serial) ||
       key.mesh!=binding.mesh.object || key.meshSerial!=static_cast<unsigned>(binding.mesh.serial) ||
       !HideMaterialIdentityLive(binding.rules) || !OwnedHideRules(static_cast<BYTE*>(binding.rules.object)) ||
       !hideRoundRosterKnown || hideRoundPawns[binding.slot].object!=binding.pawn.object ||
       hideRoundPawns[binding.slot].serial!=binding.pawn.serial || !HideMaterialIdentityLive(binding.pawn) ||
       !HideMaterialIdentityLive(binding.owner) || !HideMaterialIdentityLive(binding.component))return false;
    auto pawn=static_cast<BYTE*>(binding.pawn.object),owner=static_cast<BYTE*>(binding.owner.object);
    auto component=static_cast<BYTE*>(binding.component.object),world=TwinKartLocalRace::World(static_cast<BYTE*>(binding.rules.object));
    using GetWorld=BYTE*(__fastcall*)(void*);
    if(!HideRolePawn(pawn,world) || *reinterpret_cast<BYTE**>(pawn)!=image+0x5017c10 ||
       !TwinKartRace::ObjectiveAssetIsA(image,owner,0x43d82e0) ||
       reinterpret_cast<GetWorld>((*reinterpret_cast<void***>(owner))[0x160/8])(owner)!=world ||
       !TwinKartRace::ObjectiveAssetIsA(image,component,0x454d3d0) || *reinterpret_cast<void**>(component+0xa0)!=owner)return false;
    return !sameMesh || (HideMaterialIdentityLive(binding.mesh) &&
        *reinterpret_cast<void**>(component+0x4c8)==binding.mesh.object);
}
bool TryHideMaterialScope(HideMaterialBinding& binding,const TwinKartVisual::MaterialRepairKey& key,bool sameMesh) {
    __try {return HideMaterialScope(binding,key,sameMesh);} __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool ReadHideRepairSlot(const TwinKartVisual::MaterialRepairKey& key,unsigned index,bool sameMesh,
    TwinKartVisual::MaterialSlot& outputSlot,void* raw) {
    __try {
        auto& binding=*static_cast<HideMaterialBinding*>(raw);
        if(index>=32 || !HideMaterialScope(binding,key,sameMesh))return false;
        void* overrides[32]{};unsigned overrideCount=0;
        if(!TwinKartVisual::ReadMaterialOverrides(binding.component.object,overrides,overrideCount))return false;
        TwinKartVisual::MaterialSlot value{};value.overrideValue=overrides[index];
        if(sameMesh) {
            TwinKartVisual::MaterialSlot defaults[32]{};unsigned count=0;
            if(!TwinKartVisual::ReadStaticMaterials(binding.mesh.object,defaults,count) || index>=count)return false;
            value.name=defaults[index].name;value.material=defaults[index].material;
            value.verifiedLoaderFallback=value.material && HideKnownSampleMesh(static_cast<BYTE*>(binding.mesh.object)) &&
                HideEngineLoaderFallback(value.material);
        }
        outputSlot=value;return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool WriteHideRepairSlot(const TwinKartVisual::MaterialRepairKey& key,unsigned index,void* expected,
    void* next,bool sameMesh,void* raw) {
    TwinKartVisual::MaterialSlot before;
    if(!ReadHideRepairSlot(key,index,sameMesh,before,raw) || before.overrideValue!=expected)return false;
    auto& binding=*static_cast<HideMaterialBinding*>(raw);BYTE* method=nullptr;
    if(next && (before.name!=binding.slotNames[index] || (before.material && !before.verifiedLoaderFallback)))return false;
    __try {
        if(!HideMaterialScope(binding,key,sameMesh) || (next && (binding.replacements[index].object!=next ||
           !HideMaterialIdentityLive(binding.replacements[index]) ||
           !TwinKartRace::ObjectiveAssetIsA(image,next,0x44d98e0))))return false;
        method=(*reinterpret_cast<BYTE***>(binding.component.object))[0x570/8];
        if(method<image || method>=image+0x4ed0000)return false;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
    // Reflected original PrimitiveComponent SetMaterial: RCX component,
    // EDX element index, R8 material. Original notification/render updates run
    // outside metadata SEH. This adapter writes no global mesh, shape,
    // transform or collision-response configuration.
    reinterpret_cast<void(__fastcall*)(void*,int,void*)>(method)(binding.component.object,static_cast<int>(index),next);
    TwinKartVisual::MaterialSlot after;
    return ReadHideRepairSlot(key,index,sameMesh,after,raw) && after.overrideValue==next;
}
void ReleaseHideMaterialRepairs() {
    for(auto& role:hideMaterialBindings)for(auto& binding:role)
        binding.lease.Restore(ReadHideRepairSlot,WriteHideRepairSlot,&binding);
    ForgetHideMaterialRepairs();
}
BYTE* HideOriginalMaterialSource(BYTE* rules,int profile) {
    if(profile<0 || profile>=static_cast<int>(ARRAYSIZE(hideMaterialSources)) || !OwnedHideRules(rules))return nullptr;
    const auto& spec=hideMaterialSources[profile];auto& identity=hideMaterialSourceMeshes[profile];
    if(HideMaterialIdentityLive(identity))return static_cast<BYTE*>(identity.object);
    auto now=GetTickCount64();if(now<hideMaterialSourceRetry[profile] || hideMaterialSourceAttempts[profile]>=3)return nullptr;
    hideMaterialSourceRetry[profile]=now+1500;++hideMaterialSourceAttempts[profile];
    TwinKartRace::ObjectiveSoftReference reference{};
    using Name=void(__fastcall*)(void*,const wchar_t*,int);
    reinterpret_cast<Name>(image+0x2589130)(&reference.path.name,spec.sourcePath,1);
    if(!reference.path.name)return nullptr;
    using Load=BYTE*(__fastcall*)(TwinKartRace::ObjectiveSoftReference*);
    auto source=reinterpret_cast<Load>(image+0xa42300)(&reference);
    if(!OwnedHideRules(rules) || !ExactHideMeshPath(source,spec.sourcePackage,spec.sourceName) ||
       !TwinKartLocalRace::CaptureFlagObject(source,identity))return nullptr;
    RuntimeEvent("native_hide_original_material_source_loaded",profile,1);return source;
}
bool ReadHideMaterialSnapshot(BYTE* component,BYTE*& mesh,TwinKartVisual::MaterialSlot (&slots)[32],unsigned& count,int& profile) {
    profile=-1;
    __try {
        if(!TwinKartLocalRace::Live(component) || !TwinKartRace::ObjectiveAssetIsA(image,component,0x454d3d0))return false;
        mesh=*reinterpret_cast<BYTE**>(component+0x4c8);
        for(unsigned i=0;i<ARRAYSIZE(hideMaterialSources);++i)if(ExactHideMeshPath(mesh,
            hideMaterialSources[i].samplePackage,hideMaterialSources[i].sampleName))profile=static_cast<int>(i);
        if(profile<0 || !TwinKartVisual::ReadStaticMaterials(mesh,slots,count))return false;
        void* overrides[32]{};unsigned overrideCount=0;
        if(!TwinKartVisual::ReadMaterialOverrides(component,overrides,overrideCount))return false;
        for(unsigned i=0;i<count;++i) {
            slots[i].overrideValue=overrides[i];
            slots[i].verifiedLoaderFallback=slots[i].material && HideEngineLoaderFallback(slots[i].material);
        }
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
void UpdateHideMaterialBinding(BYTE* rules,int slot,int target,BYTE* pawn,BYTE* owner,BYTE* component) {
    auto& binding=hideMaterialBindings[slot][target];
    if(binding.lease.Active()) {
        if(!HideMaterialIdentityLive(binding.rules) || !HideMaterialIdentityLive(binding.pawn) ||
           !HideMaterialIdentityLive(binding.owner) || !HideMaterialIdentityLive(binding.component))binding=HideMaterialBinding{};
        else {
            if(component==binding.component.object && binding.lease.Healthy(ReadHideRepairSlot,&binding))return;
            if(!binding.lease.Restore(ReadHideRepairSlot,WriteHideRepairSlot,&binding))return;
        }
    }
    if(!component || !owner || !OwnedHideRules(rules))return;
    BYTE* mesh=nullptr;TwinKartVisual::MaterialSlot slots[32]{},sourceSlots[32]{};unsigned count=0,sourceCount=0;int profile=-1;
    if(!ReadHideMaterialSnapshot(component,mesh,slots,count,profile))return;
    bool missing=false;for(unsigned i=0;i<count;++i)missing|=(!slots[i].material || slots[i].verifiedLoaderFallback) && !slots[i].overrideValue;
    if(!missing)return;
    HideMaterialBinding candidate;candidate.slot=slot;
    if(!TwinKartLocalRace::CaptureFlagObject(rules,candidate.rules) || !TwinKartLocalRace::CaptureFlagObject(pawn,candidate.pawn) ||
       !TwinKartLocalRace::CaptureFlagObject(owner,candidate.owner) || !TwinKartLocalRace::CaptureFlagObject(component,candidate.component) ||
       !TwinKartLocalRace::CaptureFlagObject(mesh,candidate.mesh))return;
    TwinKartVisual::MaterialRepairKey key{component,static_cast<unsigned>(candidate.component.serial),mesh,static_cast<unsigned>(candidate.mesh.serial)};
    if(!TryHideMaterialScope(candidate,key,true))return;
    if(binding.component.object==component && binding.component.serial==candidate.component.serial &&
       binding.mesh.object==mesh && binding.mesh.serial==candidate.mesh.serial)candidate.failures=binding.failures;
    if(candidate.failures>=3)return;
    auto source=HideOriginalMaterialSource(rules,profile);int refreshedProfile=-1;
    if(!source || !TryHideMaterialScope(candidate,key,true) ||
       !HideMaterialIdentityLive(hideMaterialSourceMeshes[profile]) || hideMaterialSourceMeshes[profile].object!=source ||
       !TwinKartVisual::ReadStaticMaterials(source,sourceSlots,sourceCount) ||
       !ReadHideMaterialSnapshot(component,mesh,slots,count,refreshedProfile) || profile!=refreshedProfile || mesh!=candidate.mesh.object)return;
    for(unsigned i=0;i<sourceCount;++i) {
        auto material=static_cast<BYTE*>(sourceSlots[i].material);
        if(!TwinKartLocalRace::Live(material) || !TwinKartRace::ObjectiveAssetIsA(image,material,0x44d98e0) || HideEngineLoaderFallback(material))return;
    }
    TwinKartVisual::MaterialRepairPlan plan;
    if(TwinKartVisual::PlanMissingMaterials(slots,count,sourceSlots,sourceCount,plan)!=TwinKartVisual::MaterialPlanStatus::Ready)return;
    for(unsigned i=0;i<plan.count;++i) {
        const auto& entry=plan.entries[i];candidate.slotNames[entry.index]=entry.name;
        if(!TwinKartLocalRace::CaptureFlagObject(static_cast<BYTE*>(entry.replacement),candidate.replacements[entry.index]))return;
    }
    if(!TryHideMaterialScope(candidate,key,true))return;
    binding=candidate;
    bool applied=binding.lease.Apply(key,plan,ReadHideRepairSlot,WriteHideRepairSlot,&binding);
    binding.failures=applied?0:binding.failures+1;
    RuntimeEvent("native_hide_missing_original_material_slots_requested",slot,applied?static_cast<int>(plan.count):-1);
}
BYTE* FindHidePropVisualComponent(BYTE* rules,BYTE* actor) {
    TwinKartCamera::ObjectIdentity identity;void* type=nullptr;
    __try {
        if(!OwnedHideRules(rules) || !TwinKartLocalRace::Live(actor) ||
           !TwinKartRace::ObjectiveAssetIsA(image,actor,0x43d82e0) || !TwinKartLocalRace::CaptureFlagObject(actor,identity))return nullptr;
        using Type=void*(__fastcall*)();type=reinterpret_cast<Type>(image+0x454d3d0)();
        if(!TwinKartLocalRace::Live(static_cast<BYTE*>(type)))return nullptr;
        using GetWorld=BYTE*(__fastcall*)(void*);
        if(reinterpret_cast<GetWorld>((*reinterpret_cast<void***>(actor))[0x160/8])(actor)!=TwinKartLocalRace::World(rules))return nullptr;
    } __except(EXCEPTION_EXECUTE_HANDLER){return nullptr;}
    // Exact original Actor.GetComponentByClass reflected wrapper0x43da060
    // forwards RCX=actor/RDX=class to0x3cb3550 and returns the component.
    // Blueprint props can keep a scene root above their actual render mesh.
    using Find=BYTE*(__fastcall*)(void*,void*);
    auto component=reinterpret_cast<Find>(image+0x3cb3550)(actor,type);
    __try {
        return OwnedHideRules(rules) && HideMaterialIdentityLive(identity) && TwinKartLocalRace::Live(component) &&
            TwinKartRace::ObjectiveAssetIsA(image,component,0x454d3d0) &&
            *reinterpret_cast<void**>(component+0xa0)==actor?component:nullptr;
    } __except(EXCEPTION_EXECUTE_HANDLER){return nullptr;}
}
void RestoreHideLocalDisguiseView() {
    if(GetCurrentThreadId()!=queue.ThreadId())return;
    hideLocalDisguiseActor={};hideLocalDisguiseKind=0;
}
void UpdateHideLocalDisguiseView(BYTE* rules) {
    if(!hideLobbyEnabled)return;
    __try {
        BYTE* actor=nullptr;
        if(hideRoundInputsActive && hideRoundRosterKnown && OwnedHideRules(rules)) {
            auto world=TwinKartLocalRace::World(rules);
            for(unsigned slot=0;slot<8;++slot) {
                if(!HideMaterialIdentityLive(hideDisguiseControllers[slot]) || !HideMaterialIdentityLive(hideRoundPawns[slot]))continue;
                auto controller=static_cast<BYTE*>(hideDisguiseControllers[slot].object),pawn=static_cast<BYTE*>(hideRoundPawns[slot].object);
                if(!TwinKartObjective::HideControllerHasPlayer(controller) || *reinterpret_cast<void**>(controller+0x250)!=pawn ||
                   *reinterpret_cast<void**>(pawn+0x258)!=controller || !HideRolePawn(pawn,world) ||
                   *reinterpret_cast<BYTE**>(pawn)!=image+0x5017c10)continue;
                auto disguise=TwinKartLocalRace::FlagFind(pawn,0x10f43e0);
                if(TwinKartLocalRace::Live(disguise) && *reinterpret_cast<void**>(disguise+0xa0)==pawn)
                    actor=*reinterpret_cast<BYTE**>(disguise+0x210);
                break;
            }
        }
        if(actor && actor==hideLocalDisguiseActor.object && HideMaterialIdentityLive(hideLocalDisguiseActor))return;
        RestoreHideLocalDisguiseView();
        // Frame the live disguise's bounds; its mesh visibility stays native.
        if(!TwinKartLocalRace::Live(actor))return;
        auto type=*reinterpret_cast<BYTE**>(actor+0x10);wchar_t name[64]{};
        if(!TwinKartLocalRace::Live(type) || !ModeNameText(*reinterpret_cast<unsigned long long*>(type+0x18),name,ARRAYSIZE(name)))return;
        const int kind=TwinKartObjective::HideDisguiseKind(name);
        if(!kind)return;
        if(TwinKartLocalRace::CaptureFlagObject(actor,hideLocalDisguiseActor))hideLocalDisguiseKind=kind;
    } __except(EXCEPTION_EXECUTE_HANDLER){RestoreHideLocalDisguiseView();}
}
void PrepareHideMaterialRepairs(BYTE* rules) {
    if(!hideAutonomyCompatibility || !hideRoundRosterKnown || !OwnedHideRules(rules))return;
    auto now=GetTickCount64();if(now<hideMaterialNext)return;hideMaterialNext=now+500;
    for(int slot=0;slot<8;++slot) {
        if(!OwnedHideRules(rules))return;
        auto identity=hideRoundPawns[slot];auto pawn=static_cast<BYTE*>(identity.object),world=TwinKartLocalRace::World(rules);
        if(!HideMaterialIdentityLive(identity) || !HideRolePawn(pawn,world) ||
           *reinterpret_cast<BYTE**>(pawn)!=image+0x5017c10)continue;
        auto body=*reinterpret_cast<BYTE**>(pawn+0x4b8);
        UpdateHideMaterialBinding(rules,slot,0,pawn,pawn,body);
        if(!OwnedHideRules(rules) || !HideMaterialIdentityLive(identity) ||
           hideRoundPawns[slot].object!=identity.object || hideRoundPawns[slot].serial!=identity.serial)return;
        auto disguise=TwinKartLocalRace::FlagFind(pawn,0x10f43e0);
        auto child=disguise?*reinterpret_cast<BYTE**>(disguise+0x210):nullptr;
        auto visual=TwinKartLocalRace::Live(child)?FindHidePropVisualComponent(rules,child):nullptr;
        UpdateHideMaterialBinding(rules,slot,1,pawn,child,visual==body?nullptr:visual);
    }
}
bool ReadHideSeats(BYTE* rules,TwinKartObjective::HideSeat* seats,TwinKartCamera::ObjectIdentity* pawns,bool complete) {
    __try {
        if(!OwnedHideRules(rules) || !seats || !pawns)return false;
        auto world=TwinKartLocalRace::World(rules);
        auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
        int count=*reinterpret_cast<int*>(image+0x69c28c4),size=0;
        if(!chunks || count<1 || count>2000000)return false;
        using Type=void*(__fastcall*)();using GetWorld=BYTE*(__fastcall*)(void*);
        auto type=reinterpret_cast<Type>(image+0x10f3d30)();
        auto swapType=reinterpret_cast<Type>(image+0x10f49b0)();
        for(int index=0;index<count;++index) {
            auto chunk=chunks[index/65536];if(!chunk || (chunk[index%65536].flags&0x30000000))continue;
            auto controller=static_cast<BYTE*>(chunk[index%65536].object);
            if(!TwinKartLocalRace::Live(controller) || (*reinterpret_cast<unsigned*>(controller+8)&0x18030) ||
               *reinterpret_cast<void**>(controller+0x10)!=type ||
               reinterpret_cast<GetWorld>((*reinterpret_cast<void***>(controller))[0x160/8])(controller)!=world)continue;
            if(size>=8)return false;
            // Controller.Pawn can intentionally remain the original viewer
            // during role presentation/blinding. The original swap component
            // already owns the new role target before that possession switch.
            using Find=BYTE*(__fastcall*)(void*,void*);
            auto swap=reinterpret_cast<Find>((*reinterpret_cast<void***>(controller))[0x620/8])(controller,swapType);
            if(!TwinKartLocalRace::OwnedComponent(swap,controller,swapType))return false;
            auto pawn=*reinterpret_cast<BYTE**>(swap+0xb8);
            bool role=HideRolePawn(pawn,world);
            if(complete && !role)return false;
            seats[size]={reinterpret_cast<std::uintptr_t>(controller),role?reinterpret_cast<std::uintptr_t>(pawn):0};
            // New role actors need not have acquired a weak serial yet. Use
            // the original weak-pointer assignment, without rooting them;
            // waiting for their model/input initialization creates a cycle.
            if(role && !TwinKartLocalRace::CaptureFlagObject(pawn,pawns[size]))return false;
            ++size;
        }
        return TwinKartObjective::HideSeatSet(seats,size,complete);
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
void PrepareHideInitialDisguises(BYTE* rules,const TwinKartObjective::HideSeat* seats,const TwinKartCamera::ObjectIdentity* pawns) {
    if(!hideAutonomyCompatibility || !seats || !pawns || !OwnedHideRules(rules))return;
    auto now=GetTickCount64();if(now<hideDisguiseNext)return;hideDisguiseNext=now+500;
    using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
    auto type=reinterpret_cast<Type>(image+0x10f43e0)();
    auto infoType=reinterpret_cast<Type>(image+0x10f4500)();
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);int count=*reinterpret_cast<int*>(image+0x69c28c4);
    for(int slot=0;slot<8;++slot) {
        if(!OwnedHideRules(rules))return;
        chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);count=*reinterpret_cast<int*>(image+0x69c28c4);
        auto controller=reinterpret_cast<BYTE*>(seats[slot].controller);
        auto pawn=static_cast<BYTE*>(pawns[slot].object);
        if(hideDisguiseControllers[slot].object!=controller || !TwinKartCamera::Live(hideDisguiseControllers[slot],chunks,count) ||
           !TwinKartCamera::Live(pawns[slot],chunks,count) || *reinterpret_cast<BYTE**>(pawn)!=image+0x5017c10 ||
           *reinterpret_cast<void**>(controller+0x250)!=pawn)continue;
        bool human=TwinKartObjective::HideControllerHasPlayer(controller);
        if(hideDisguiseRequests[slot]>=3)continue;
        auto component=reinterpret_cast<Find>((*reinterpret_cast<void***>(pawn))[0x620/8])(pawn,type);
        if(!TwinKartLocalRace::OwnedComponent(component,pawn,type) || *reinterpret_cast<BYTE**>(component)!=image+0x5016e90 ||
           (*reinterpret_cast<BYTE***>(component))[0x548/8]!=image+0x10f0fd0)continue;
        bool selected=TwinKartLocalRace::Live(*reinterpret_cast<BYTE**>(component+0x280));
        bool child=TwinKartLocalRace::Live(*reinterpret_cast<BYTE**>(component+0x210));
        struct Choices {BYTE** data;int count,capacity;};
        auto choices=*reinterpret_cast<const Choices*>(component+0x258);
        std::uint32_t entropy=0;
        if(!choices.data || (!human && !selected && !child &&
            BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&entropy),sizeof(entropy),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0))continue;
        int first=TwinKartObjective::HideInitialDisguiseChoice(entropy,choices.count,choices.capacity,controller,
            hideSearchActive,selected,child);if(first<0)continue;
        for(int attempt=0;attempt<choices.count;++attempt) {
            int index=(first+attempt)%choices.count;auto info=choices.data[index];
            if(!TwinKartLocalRace::Live(info) || *reinterpret_cast<void**>(info+0x10)!=infoType)continue;
            TwinKartCamera::ObjectIdentity componentIdentity;
            if(!TwinKartLocalRace::CaptureFlagObject(component,componentIdentity))break;
            // Reuse the authoritative original selection implementation. It
            // creates the real original child actor and consumes this choice
            // list, just as a native player selection does. No class/asset is
            // supplied by the launcher. Selected/pending props are preserved.
            // The retained BP_HiderPawn has no external body mesh in its
            // cooked name table. If a human still has no choice after native
            // Search begins, initialize a real original prop instead of an
            // empty body. Subsequent native selection is never intercepted.
            ++hideDisguiseRequests[slot];
            reinterpret_cast<void(__fastcall*)(void*,int)>(image+0x10f0fd0)(component,index);
            chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);count=*reinterpret_cast<int*>(image+0x69c28c4);
            if(!OwnedHideRules(rules) || !TwinKartCamera::Live(componentIdentity,chunks,count) || !TwinKartCamera::Live(pawns[slot],chunks,count) ||
               !TwinKartCamera::Live(hideDisguiseControllers[slot],chunks,count) ||
               *reinterpret_cast<void**>(component+0xa0)!=pawn || *reinterpret_cast<void**>(controller+0x250)!=pawn) {
                RuntimeEvent("native_hide_original_prop_request_retired_during_callback",slot,0);break;
            }
            int result=*reinterpret_cast<void**>(component+0x280)==info?1:0;
            if(TwinKartLocalRace::Live(*reinterpret_cast<BYTE**>(component+0x210)))result|=2;
            RuntimeEvent(human?"native_hide_empty_human_original_prop_requested":"native_hide_bot_disguise_selected",slot,result);break;
        }
    }
}
void RefreshHideRoundRoster(BYTE* rules) {
    if(!hideRoundRosterPending || GetTickCount64()<hideRosterRetry)return;
    hideRosterRetry=GetTickCount64()+100;
    if(!OwnedHideRules(rules) || rules!=hideRosterOwner.object ||
       !TwinKartCamera::Live(hideRosterOwner,*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),
           *reinterpret_cast<int*>(image+0x69c28c4)))return;
    TwinKartObjective::HideSeat after[8]{};TwinKartCamera::ObjectIdentity newPawns[8]{};
    if(!ReadHideSeats(rules,after,newPawns,true))return;
    TwinKartObjective::HideSeat actual[8]{};TwinKartCamera::ObjectIdentity newControllers[8]{};
    for(int index=0;index<8;++index) {
        auto controller=reinterpret_cast<BYTE*>(after[index].controller);
        if(!TwinKartLocalRace::Live(controller) || !TwinKartLocalRace::CaptureFlagObject(controller,newControllers[index]))return;
        actual[index]={after[index].controller,reinterpret_cast<std::uintptr_t>(*reinterpret_cast<void**>(controller+0x250))};
    }
    ReleaseHideMaterialRepairs();
    if(!OwnedHideRules(rules))return;
    bool failed=false;
    if(hideRoundRosterKnown)for(int index=0;index<8;++index) {
        if(!TwinKartObjective::RetiredHidePawn(hideRoundSeats,after,8,index) ||
           !TwinKartCamera::Live(hideRoundPawns[index],*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),
               *reinterpret_cast<int*>(image+0x69c28c4)))continue;
        if(!TwinKartObjective::UncontrolledHidePawn(hideRoundSeats[index].pawn,actual,8)){failed=true;continue;}
        if(static_cast<BYTE*>(hideRoundPawns[index].object)[0x5c]&0x10) {
            RuntimeEvent("native_hide_previous_role_already_destroying",index,1);continue;
        }
        // Preserve weak identities across the original unpossess step. Old
        // actors can otherwise no longer be found through Controller.Pawn.
        bool retired=reinterpret_cast<bool(__fastcall*)(void*,bool,bool)>(image+0x3cac200)(hideRoundPawns[index].object,false,true);
        RuntimeEvent("native_hide_compat_retired_role_pawn",index,retired?1:0);failed|=!retired;
    }
    if(failed)return;
    ForgetHideMaterialRepairs();
    memcpy(hideRoundSeats,after,sizeof(after));memcpy(hideRoundPawns,newPawns,sizeof(newPawns));
    memcpy(hideDisguiseControllers,newControllers,sizeof(newControllers));
    hideRoundRosterKnown=true;hideRoundRosterPending=false;
    hideGround.Reset();hideRoadSupport.Reset();hideGroundRejected=false;hideAiNext=hideAiLogAt=0;hideAiEnabled=0;
    memset(hideAiControlValid,0,sizeof(hideAiControlValid));
    hideAdjacencySample=0;hideBlockedEdges=0;hideObjectTypeCount=0;
    hideAppearanceRosterReady=false;hideAppearanceRosterRetry=0;
    memset(hidePreviousObservation,0,sizeof(hidePreviousObservation));
    memset(hideAiOriginValid,0,sizeof(hideAiOriginValid));
    memset(hideRoleChildren,0,sizeof(hideRoleChildren));memset(hidePhysicalContacts,0,sizeof(hidePhysicalContacts));
    memset(hideOverlapPairs,0,sizeof(hideOverlapPairs));hideContactNext=0;
    memset(hideAiGoalAt,0,sizeof(hideAiGoalAt));
    for(int slot=0;slot<8;++slot){hideRoutes[slot].Reset();hideDrivers[slot].Reset();hidePatrolProgress[slot].Reset();}
    RuntimeEvent("native_hide_compat_complete_role_roster_retained",-1,8);
    hideDisguiseNext=0;memset(hideDisguiseRequests,0,sizeof(hideDisguiseRequests));
    PrepareHideInitialDisguises(rules,hideRoundSeats,hideRoundPawns);
}
void __fastcall HideGrantRolesCompatibility(void* capture) {
    void* notify=nullptr;
    BYTE* rules=nullptr;
    __try {
        rules=capture?*reinterpret_cast<BYTE**>(capture):nullptr;
        if(hideDeadlineCompatibility && rules==hideDeadlineSignalOwner.object && OwnedHideRules(rules) &&
           TwinKartCamera::Live(hideDeadlineSignalOwner,*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),
               *reinterpret_cast<int*>(image+0x69c28c4))) {
            auto condition=*reinterpret_cast<BYTE**>(rules+0x130);
            if(condition && condition==hideDeadlineSignal && *reinterpret_cast<BYTE**>(condition)==image+0x5017460 &&
               *reinterpret_cast<BYTE**>(condition+0x20)==image+0x5017428) {
                condition[0x40]=0;notify=condition+0x20;hideDeadlineSignalOwner={};hideDeadlineSignal=nullptr;
                RuntimeEvent("native_hide_compat_previous_deadline_released",-1,1);
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {RuntimeEvent("native_hide_previous_deadline_release_rejected",-1,0);}
    if(notify)reinterpret_cast<void(__fastcall*)(void*)>(image+0x10ee910)(notify);
    hideGrantRoles(capture);
    __try {
        if(hideDeadlineCompatibility && OwnedHideRules(rules) && TwinKartLocalRace::CaptureFlagObject(rules,hideRosterOwner)) {
            hideRoundRosterPending=true;hideRosterRetry=0;hideRoundInputsActive=true;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {RuntimeEvent("native_hide_round_roster_request_rejected",-1,0);}
}
void __fastcall HideSearchBeginCompatibility(void* capture) {
    hideSearchBegin(capture);
    if(!hideDeadlineCompatibility)return;
    __try {
        auto rules=*reinterpret_cast<BYTE**>(capture);
        if(!OwnedHideRules(rules))return;
        auto config=*reinterpret_cast<BYTE**>(rules+0xb0);using Type=void*(__fastcall*)();
        if(!TwinKartLocalRace::Live(config) || *reinterpret_cast<void**>(config+0x10)!=reinterpret_cast<Type>(image+0x10f3b90)())return;
        TwinKartCamera::ObjectIdentity owner{};
        if(!TwinKartLocalRace::CaptureFlagObject(rules,owner))return;
        float duration=*reinterpret_cast<float*>(config+0x3c);
        if(hideSearchClock.Begin(rules,static_cast<unsigned>(owner.serial),duration)) {
            hideSearchActive=true;
            hideSearchOwner=owner;RuntimeEvent("native_hide_compat_search_deadline_started",-1,static_cast<int>(duration*1000));
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {hideSearchClock.Stop();hideSearchOwner={};RuntimeEvent("native_hide_search_deadline_rejected",-1,0);}
}
void ObserveHideSettlement(BYTE* rules,bool searchEnded) {
    if(!hideLobbyEnabled || !OwnedHideRules(rules))return;
    __try {
        auto config=*reinterpret_cast<BYTE**>(rules+0xb0);using Type=void*(__fastcall*)();
        if(!TwinKartLocalRace::Live(config) || *reinterpret_cast<void**>(config+0x10)!=reinterpret_cast<Type>(image+0x10f3b90)())return;
        int rounds=*reinterpret_cast<int*>(config+0x30),round=*reinterpret_cast<int*>(rules+0x108);
        auto world=TwinKartLocalRace::World(rules);TwinKartCamera::ObjectIdentity identity{};
        if(!TwinKartLocalRace::CaptureFlagObject(world,identity))return;
        if(searchEnded) {
            auto timeout=*reinterpret_cast<BYTE**>(rules+0x130),empty=*reinterpret_cast<BYTE**>(rules+0x148);
            if(!timeout || !empty || *reinterpret_cast<BYTE**>(timeout)!=image+0x5017460 ||
               *reinterpret_cast<BYTE**>(empty)!=image+0x5017460 ||
               *reinterpret_cast<BYTE**>(timeout+0x20)!=image+0x5017428 ||
               *reinterpret_cast<BYTE**>(empty+0x20)!=image+0x5017428 || timeout[0x40]>1 || empty[0x40]>1)return;
            if(hideSettlementJournal.Record(reinterpret_cast<std::uintptr_t>(world),identity.index,identity.serial,rounds,round,
                timeout[0x40]!=0,empty[0x40]!=0))RuntimeEvent("native_hide_round_end_condition_retained",round,empty[0x40]?2:1);
            return;
        }
        auto program=*reinterpret_cast<BYTE**>(rules+0x188);
        if(!program || *reinterpret_cast<BYTE**>(program)!=image+0x5018518)return;
        TwinKartObjective::HideMatchSummary result;
        if(!hideSettlementJournal.Finish(reinterpret_cast<std::uintptr_t>(world),identity.index,identity.serial,
            *reinterpret_cast<unsigned*>(program+0x38),round,rounds,result))return;
        AcquireSRWLockExclusive(&prototypeResultLock);hideSettlement=result;hideSettlementAvailable=true;ReleaseSRWLockExclusive(&prototypeResultLock);
        RuntimeEvent("native_hide_completed_program_result_presentation_queued",-1,rounds);
    } __except(EXCEPTION_EXECUTE_HANDLER){RuntimeEvent("native_hide_result_presentation_rejected",-1,0);}
}
void __fastcall HideSearchEndCompatibility(void* capture) {
    __try {
        if(capture && *reinterpret_cast<void**>(capture)==hideSearchOwner.object &&
           TwinKartCamera::Live(hideSearchOwner,*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),
               *reinterpret_cast<int*>(image+0x69c28c4)) && OwnedHideRules(static_cast<BYTE*>(hideSearchOwner.object))) {
            ObserveHideSettlement(static_cast<BYTE*>(hideSearchOwner.object),true);
            StopHideObjectiveInputs();
            hideRoundInputsActive=false;
            hideSearchActive=false;
            InterlockedExchange64(&hideObservedMovement,0);
            hideSearchClock.Stop();hideSearchOwner={};RuntimeEvent("native_hide_compat_search_scope_ended",-1,1);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {hideSearchClock.Stop();hideSearchOwner={};}
    hideSearchEnd(capture);
}
void CaptureHideHiderInput(BYTE* rules,float delta) {
    if(!hideInputCapture || hideInputCaptureDone || !hideSearchActive || !OwnedHideRules(rules) ||
       !hideRoundRosterKnown || !std::isfinite(delta) || delta<=0 || delta>5)return;
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
    int count=*reinterpret_cast<int*>(image+0x69c28c4);
    if(!hideInputPawn.object) {
        for(int index=0;index<8;++index) {
            auto controller=reinterpret_cast<BYTE*>(hideRoundSeats[index].controller);
            auto pawn=static_cast<BYTE*>(hideRoundPawns[index].object);
            if(!TwinKartLocalRace::Live(controller) || TwinKartObjective::HideControllerHasPlayer(controller) ||
               *reinterpret_cast<void**>(controller+0x250)!=pawn || !TwinKartCamera::Live(hideRoundPawns[index],chunks,count) ||
               *reinterpret_cast<BYTE**>(pawn)!=image+0x5017c10)continue;
            auto control=*reinterpret_cast<BYTE**>(pawn+0x4d0);
            // Exact native default FPS-style Hider input adapter and owner.
            if(!control || *reinterpret_cast<BYTE**>(control)!=image+0x501d450 || *reinterpret_cast<void**>(control+8)!=pawn)continue;
            if(!TwinKartLocalRace::FlagPosition(pawn,hideInputOrigin) ||
               !TwinKartCamera::Capture(controller,chunks,count,&hideInputController))continue;
            hideInputPawn=hideRoundPawns[index];hideInputSeconds=0;
            RuntimeEvent("native_hide_hider_input_probe_selected",index,1);break;
        }
    }
    if(!TwinKartCamera::Live(hideInputPawn,chunks,count))return;
    auto pawn=static_cast<BYTE*>(hideInputPawn.object);
    if(!TwinKartCamera::Live(hideInputController,chunks,count) ||
       TwinKartObjective::HideControllerHasPlayer(hideInputController.object) ||
       *reinterpret_cast<void**>(static_cast<BYTE*>(hideInputController.object)+0x250)!=pawn)return;
    hideInputSeconds+=delta;
    using Input=void(__fastcall*)(void*,float);
    reinterpret_cast<Input>(image+0x10eea60)(pawn,hideInputSeconds<5?0.35f:0.f);
    if(hideInputSeconds>=5) {
        TwinKartRace::ArenaPoint position{};
        if(TwinKartLocalRace::FlagPosition(pawn,position))
            RuntimeEvent("native_hide_hider_input_probe_physical_distance",-1,static_cast<int>(TwinKartRace::ArenaDistance(hideInputOrigin,position)));
        hideInputCaptureDone=true;
    }
}
void PrepareHideSeekerAppearances(BYTE* rules) {
    if((!hideInputCapture && !hideObjectiveAi) || !OwnedHideRules(rules) || !hideRoundRosterKnown)return;
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
    int count=*reinterpret_cast<int*>(image+0x69c28c4);
    if(!hideAppearanceRosterReady && GetTickCount64()>=hideAppearanceRosterRetry) {
        hideAppearanceRosterRetry=GetTickCount64()+1000;
        TwinKartEquipment::Preset playerPreset;TwinKartRace::AiAssetLookup lookup;
        if(TwinKartLocalRace::ReadEquipmentPreset(playerPreset) && TwinKartRace::BeginAiAssetLookup(image,lookup) &&
           hideAppearanceRoster.LoadEnvironment() && hideAppearanceRoster.Prepare(7,playerPreset,TwinKartRace::RegisteredAiAsset,&lookup)) {
            hideAppearanceRosterReady=true;RuntimeEvent("native_hide_random_appearance_roster_ready",-1,7);
        }
    }
    int botSlot=0;
    for(int index=0;index<8;++index) {
        auto controller=reinterpret_cast<BYTE*>(hideRoundSeats[index].controller);
        if(!TwinKartLocalRace::Live(controller))continue;
        bool human=TwinKartObjective::HideControllerHasPlayer(controller);
        int appearanceSlot=human?0:++botSlot;
        auto pawn=static_cast<BYTE*>(hideRoundPawns[index].object);
        if(!TwinKartCamera::Live(hideRoundPawns[index],chunks,count) ||
           *reinterpret_cast<BYTE**>(pawn)!=image+0x501a0c8)continue;
        bool sameOwner=hideSeekerAppearanceOwner[index].object==pawn &&
            hideSeekerAppearanceOwner[index].serial==hideRoundPawns[index].serial;
        if(!sameOwner) {
            hideSeekerAppearanceOwner[index]=hideRoundPawns[index];hideSeekerAppearanceAttempted[index]={};
            hideSeekerAppearanceRequests[index]=0;hideSeekerMeshMask[index]=-2;
            hideSeekerAppearanceStatus[index]=0;hideSeekerAppearanceRetry[index]=0;
        }
        int meshMask=TwinKartLocalRace::ObjectiveAppearanceMeshMask(pawn);
        if(meshMask!=hideSeekerMeshMask[index]) {
            hideSeekerMeshMask[index]=meshMask;
            RuntimeEvent("native_hide_seeker_loaded_mesh_asset_mask",index,meshMask);
        }
        bool requested=hideSeekerAppearanceAttempted[index].object==pawn &&
            hideSeekerAppearanceAttempted[index].serial==hideRoundPawns[index].serial;
        bool needsPreparation=!requested || (meshMask>=0 && meshMask!=3);
        if(needsPreparation && hideSeekerAppearanceRequests[index]<3 && GetTickCount64()>=hideSeekerAppearanceRetry[index]) {
            TwinKartEquipment::Preset selected;
            if(human || (hideAppearanceRosterReady && hideAppearanceRoster.Get(appearanceSlot,selected))) {
                ++hideSeekerAppearanceRequests[index];
                TwinKartRace::ObjectiveModelFailure failure;
                int preparation=TwinKartLocalRace::PrepareObjectiveAppearance(pawn,appearanceSlot,human?nullptr:&selected,&failure);
                hideSeekerAppearanceRetry[index]=GetTickCount64()+(preparation==1?5000:1000);
                if(preparation!=hideSeekerAppearanceStatus[index]) {
                    RuntimeEvent("native_hide_seeker_appearance_preparation_status",index,preparation);
                    hideSeekerAppearanceStatus[index]=preparation;
                }
                if(preparation==1) {
                    hideSeekerAppearanceAttempted[index]=hideRoundPawns[index];
                    RuntimeEvent("native_hide_seeker_original_appearance_requested",index,1);
                } else if((preparation==-7 || (preparation==-6 && failure.required)) && !human && hideSeekerAppearanceRequests[index]<3) {
                    TwinKartEquipment::Preset playerPreset;TwinKartRace::AiAssetLookup lookup;
                    bool replaced=TwinKartLocalRace::ReadEquipmentPreset(playerPreset) && TwinKartRace::BeginAiAssetLookup(image,lookup) &&
                        hideAppearanceRoster.RejectAndReplace(appearanceSlot,failure.group==1,playerPreset,TwinKartRace::RegisteredAiAsset,&lookup);
                    RuntimeEvent("native_hide_unloadable_bot_outfit_reselected",index,replaced?1:0);
                }
                if(hideSeekerAppearanceRequests[index]==3)
                    RuntimeEvent("native_hide_seeker_model_preparation_retry_limit",index,preparation);
            }
        }
        // Enable the native movement path independently of model readiness.
        // Waiting for a mesh before enabling its original movement callbacks
        // must not leave a newly requested random appearance indefinitely idle.
        if(hideObjectiveAi && hideSearchActive && !(hideAiEnabled&(1u<<index)) &&
           *reinterpret_cast<void**>(controller+0x250)==pawn && *reinterpret_cast<void**>(pawn+0x258)==controller) {
            auto movement=TwinKartLocalRace::FlagFind(pawn,0x22f9030);
            auto interface=movement?movement+0x140:nullptr;
            if(interface && *reinterpret_cast<BYTE**>(interface)==image+0x5118b80 &&
               (*reinterpret_cast<BYTE***>(interface))[0x1f8/8]==image+0x15be550) {
                // Preserve a present human physics delegate. A missing one
                // must use the same native enable path as the owned seekers.
                if(!human || *reinterpret_cast<int*>(interface+0xc0)==0)
                    reinterpret_cast<void(__fastcall*)(void*,bool)>(image+0x15be550)(interface,true);
                hideAiEnabled|=1u<<index;RuntimeEvent(human?"native_hide_human_original_movement_checked":"native_hide_ai_original_movement_enabled",index,1);
            }
        }
    }
}
void CaptureHideSeekerInput(BYTE* rules,float delta) {
    if(!hideInputCapture || hideSeekerInputDone || !hideSearchActive || !OwnedHideRules(rules) ||
       !hideRoundRosterKnown || !std::isfinite(delta) || delta<=0 || delta>5)return;
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
    int count=*reinterpret_cast<int*>(image+0x69c28c4);
    if(!hideSeekerInputPawn.object)for(int index=0;index<8;++index) {
        auto controller=reinterpret_cast<BYTE*>(hideRoundSeats[index].controller);
        auto pawn=static_cast<BYTE*>(hideRoundPawns[index].object);
        if(!TwinKartLocalRace::Live(controller) || TwinKartObjective::HideControllerHasPlayer(controller) ||
           *reinterpret_cast<void**>(controller+0x250)!=pawn || !TwinKartCamera::Live(hideRoundPawns[index],chunks,count) ||
           *reinterpret_cast<BYTE**>(pawn)!=image+0x501a0c8)continue;
        using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
        auto type=reinterpret_cast<Type>(image+0x22f9030)();
        auto movement=reinterpret_cast<Find>((*reinterpret_cast<void***>(pawn))[0x620/8])(pawn,type);
        if(!TwinKartLocalRace::OwnedComponent(movement,pawn,type) || TwinKartLocalRace::Live(*reinterpret_cast<BYTE**>(movement+0xf08)))continue;
        auto kartType=reinterpret_cast<Type>(image+0x22fef20)();
        auto kart=reinterpret_cast<Find>((*reinterpret_cast<void***>(pawn))[0x620/8])(pawn,kartType);
        if(!TwinKartLocalRace::OwnedComponent(kart,pawn,kartType) ||
           !TwinKartLocalRace::Live(*reinterpret_cast<BYTE**>(kart+0x4c0)))continue;
        if(!TwinKartCamera::Capture(movement,chunks,count,&hideSeekerInputMovement) ||
           !TwinKartCamera::Capture(controller,chunks,count,&hideSeekerInputController) ||
           !TwinKartLocalRace::FlagPosition(pawn,hideSeekerInputOrigin))continue;
        // The retained Hide prototype has no ordinary EnableMovement phase
        // rule. Reuse that rule's exact original movement-interface operation
        // before supplying input; changing throttle alone leaves physics idle.
        auto interfaceType=reinterpret_cast<Type>(image+0x239be10)();
        using Query=BYTE*(__fastcall*)(void*,void*);
        auto movementInterface=reinterpret_cast<Query>(image+0x277d780)(movement,interfaceType);
        if(movementInterface!=movement+0x140 || *reinterpret_cast<BYTE**>(movementInterface)!=image+0x5118b80 ||
           (*reinterpret_cast<BYTE***>(movementInterface))[0x1f8/8]!=image+0x15be550)continue;
        InterlockedExchange(&hideObservedSubsteps,0);InterlockedExchange(&hideObservedBodies,0);
        InterlockedExchange(&hideObservedThrottle,0);InterlockedExchange(&hideObservedContact,0);
        InterlockedExchange64(&hideObservedMovement,reinterpret_cast<LONG64>(movement));
        reinterpret_cast<void(__fastcall*)(void*,bool)>(image+0x15be550)(movementInterface,true);
        RuntimeEvent("native_hide_seeker_original_movement_enabled",index,1);
        RuntimeEvent("native_hide_seeker_updated_component_present",index,TwinKartLocalRace::Live(*reinterpret_cast<BYTE**>(movement+0xb8))?1:0);
        hideSeekerInputPawn=hideRoundPawns[index];hideSeekerInputSeconds=0;
        RuntimeEvent("native_hide_seeker_input_probe_selected",index,1);break;
    }
    if(!TwinKartCamera::Live(hideSeekerInputPawn,chunks,count) || !TwinKartCamera::Live(hideSeekerInputMovement,chunks,count))return;
    if(!TwinKartCamera::Live(hideSeekerInputController,chunks,count) ||
       TwinKartObjective::HideControllerHasPlayer(hideSeekerInputController.object) ||
       *reinterpret_cast<void**>(static_cast<BYTE*>(hideSeekerInputController.object)+0x250)!=hideSeekerInputPawn.object)return;
    hideSeekerInputSeconds+=delta;using Input=void(__fastcall*)(void*,float);
    reinterpret_cast<Input>(image+0x15d4320)(hideSeekerInputMovement.object,hideSeekerInputSeconds<5?0.35f:0.f);
    reinterpret_cast<Input>(image+0x15d3fd0)(hideSeekerInputMovement.object,0.f);
    if(hideSeekerInputSeconds>=5) {
        TwinKartRace::ArenaPoint position{};
        if(TwinKartLocalRace::FlagPosition(static_cast<BYTE*>(hideSeekerInputPawn.object),position))
            RuntimeEvent("native_hide_seeker_input_probe_physical_distance",-1,static_cast<int>(TwinKartRace::ArenaDistance(hideSeekerInputOrigin,position)));
        RuntimeEvent("native_hide_seeker_actual_physics_substeps",-1,InterlockedCompareExchange(&hideObservedSubsteps,0,0));
        RuntimeEvent("native_hide_seeker_nonnull_physics_bodies",-1,InterlockedCompareExchange(&hideObservedBodies,0,0));
        RuntimeEvent("native_hide_seeker_positive_throttle_substeps",-1,InterlockedCompareExchange(&hideObservedThrottle,0,0));
        RuntimeEvent("native_hide_seeker_tire_contact_substeps",-1,InterlockedCompareExchange(&hideObservedContact,0,0));
        InterlockedExchange64(&hideObservedMovement,0);
        hideSeekerInputDone=true;
    }
}
void __fastcall HideSubstepObserver(void* raw,float delta,void* body) {
    // Physics workers must not query UObjects, worlds or game-thread state.
    // The game thread publishes only its current exact diagnostic target.
    auto expected=InterlockedCompareExchange64(&hideObservedMovement,0,0);
    if(expected && reinterpret_cast<LONG64>(raw)==expected) {
        InterlockedIncrement(&hideObservedSubsteps);
        if(body)InterlockedIncrement(&hideObservedBodies);
        // Read only this native callback's live receiver, without any engine
        // query or object lookup on its physics worker. Fields corroborated by
        // SetThrottleInput and the original IsTireContact interface getter.
        if(*reinterpret_cast<float*>(static_cast<BYTE*>(raw)+0x18c)>0.1f)InterlockedIncrement(&hideObservedThrottle);
        if(*reinterpret_cast<unsigned char*>(static_cast<BYTE*>(raw)+0x4b8))InterlockedIncrement(&hideObservedContact);
    }
    hideSubstep(raw,delta,body); // original once, all original arguments
}
void __fastcall HideThrottleObserver(void* raw,float value) {
    auto caller=reinterpret_cast<BYTE*>(_ReturnAddress());
    hideThrottle(raw,value); // Observe only; preserve every native input.
    if(!hideInputCapture || !hideCaptureArmed || !hideSearchActive || !hideRoundRosterKnown ||
       GetCurrentThreadId()!=queue.ThreadId() || caller<image || caller>=image+0x4ed0000 ||
       !std::isfinite(value))return;
    auto movement=static_cast<BYTE*>(raw);
    if(!TwinKartLocalRace::Live(movement))return;
    auto pawn=*reinterpret_cast<void**>(movement+0xa0);int slot=-1;
    for(int index=0;index<8;++index)if(hideRoundPawns[index].object==pawn){slot=index;break;}
    if(slot<0)return;
    unsigned key=static_cast<unsigned>(caller-image)|(value==0?0x80000000u:0);
    for(unsigned index=0;index<hideThrottleCallerCount;++index)if(hideThrottleCallers[index]==key)return;
    if(hideThrottleCallerCount>=32)return;
    hideThrottleCallers[hideThrottleCallerCount++]=key;
    RuntimeEvent(value==0?"native_hide_input_zero_original_caller":"native_hide_input_nonzero_original_caller",slot,
        static_cast<int>(caller-image));
}
struct HideLineContext {BYTE* world=nullptr;void* ignored[4]{};int count=0;};
bool HideSightClear(HideLineContext& context,TwinKartRace::ArenaPoint start,TwinKartRace::ArenaPoint end) {
    auto world=context.world;
    if(!world || !TwinKartRace::Finite(start) || !TwinKartRace::Finite(end))return false;
    start.z+=80;end.z+=80;
    TwinKartRace::QueryArray types{hideObjectTypes,hideObjectTypeCount,hideObjectTypeCount};
    TwinKartRace::QueryArray ignored{context.ignored,context.count,context.count};
    return TwinKartRace::QueryLineClear(world,reinterpret_cast<TwinKartRace::ObjectTrace>(image+0x3ff5990),types,ignored,start,end);
}
bool HidePathLine(const TwinKartRace::ArenaPoint& start,const TwinKartRace::ArenaPoint& end,void* context) {
    return context && HideSightClear(*static_cast<HideLineContext*>(context),start,end);
}
TwinKartRace::RoadSurfaceResult HideReadSupport(const TwinKartRace::ArenaPoint& start,
    const TwinKartRace::ArenaPoint& end,TwinKartRace::RoadSurface& surface,void* raw) {
    auto world=static_cast<BYTE*>(raw);
    if(GetCurrentThreadId()!=queue.ThreadId() || !hideObjectiveAi || !hideRoundInputsActive ||
       !hideRoundRosterKnown || OwnedRaceWorld()!=world)return TwinKartRace::RoadSurfaceResult::Invalid;
    if(hideRepairedSceneEnabled && !hideArenaCapture)return ReadHideRepairedSurface(start,end,surface,world);
    return TwinKartRace::QueryRoadSurface(world,reinterpret_cast<TwinKartRace::RoadTrace>(image+0x3de0910),
        image+0x6b01810,start,end,surface);
}
void IgnoreHideRole(HideLineContext& context,BYTE* pawn) {
    if(!pawn || context.count>=4)return;context.ignored[context.count++]=pawn;
    if(*reinterpret_cast<BYTE**>(pawn)!=image+0x5017c10)return;
    auto disguise=TwinKartLocalRace::FlagFind(pawn,0x10f43e0);
    auto child=disguise?*reinterpret_cast<BYTE**>(disguise+0x210):nullptr;
    if(TwinKartLocalRace::Live(child) && context.count<4)context.ignored[context.count++]=child;
}
void __fastcall HideHitObserver(void* self,void* component,void* other,void* otherComponent,bool selfMoved,
    const TwinKartRace::ArenaPoint* location,const TwinKartRace::ArenaPoint* normal,const TwinKartRace::ArenaPoint* impulse,const void* hit) {
    // Keep the complete original Blueprint/native notification, with all nine
    // arguments forwarded exactly once. Never destroy actors while the engine
    // is dispatching collision callbacks over its component/body collections.
    hideHit(self,component,other,otherComponent,selfMoved,location,normal,impulse,hit);
    if(!hideObjectiveAi || !hideCaptureArmed || !hideSearchActive || !hideRoundRosterKnown ||
       GetCurrentThreadId()!=queue.ThreadId() || !hit || !(static_cast<const BYTE*>(hit)[0x5d]&1) ||
       !location || !normal || !impulse || !TwinKartRace::Finite(*location) || !TwinKartRace::Finite(*normal) ||
       !TwinKartRace::Finite(*impulse))return;
    float length=normal->x*normal->x+normal->y*normal->y+normal->z*normal->z;
    if(length<0.25f || length>2.25f)return;
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);int count=*reinterpret_cast<int*>(image+0x69c28c4);
    int seeker=-1,hider=-1;TwinKartCamera::ObjectIdentity actor{};
    for(int slot=0;slot<8;++slot) {
        auto identity=hideRoundPawns[slot];if(!TwinKartCamera::Live(identity,chunks,count))continue;
        auto pawn=static_cast<BYTE*>(identity.object);
        if(pawn[0x5c]&0x10)continue;
        if(*reinterpret_cast<BYTE**>(pawn)==image+0x501a0c8 && (self==pawn || other==pawn))seeker=slot;
        if(*reinterpret_cast<BYTE**>(pawn)!=image+0x5017c10)continue;
        if(self==pawn || other==pawn){hider=slot;actor=identity;}
        else if(TwinKartCamera::Live(hideRoleChildren[slot],chunks,count) &&
            (self==hideRoleChildren[slot].object || other==hideRoleChildren[slot].object)) {
            hider=slot;actor=hideRoleChildren[slot];
        }
    }
    if(seeker<0 || hider<0)return;
    hidePhysicalContacts[hider]={hideRoundPawns[hider],hideRoundPawns[seeker],actor,GetTickCount64()};
    RuntimeEvent("native_hide_actual_blocking_contact_observed",hider,seeker);
}
bool HideOwnedBot(int slot,BYTE*& pawn) {
    if(slot<0 || slot>=8 || !hideRoundRosterKnown)return false;
    auto controller=reinterpret_cast<BYTE*>(hideRoundSeats[slot].controller);
    pawn=static_cast<BYTE*>(hideRoundPawns[slot].object);
    if(!TwinKartLocalRace::Live(controller) || TwinKartObjective::HideControllerHasPlayer(controller) ||
       *reinterpret_cast<void**>(controller+0x250)!=pawn || !TwinKartCamera::Live(hideRoundPawns[slot],
           *reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),*reinterpret_cast<int*>(image+0x69c28c4)))return false;
    if(!TwinKartCamera::Live(hideRosterOwner,*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),
        *reinterpret_cast<int*>(image+0x69c28c4)))return false;
    auto world=TwinKartLocalRace::World(static_cast<BYTE*>(hideRosterOwner.object));
    return HideRolePawn(pawn,world);
}
void IsolateHideBotPlayerInput(BYTE* rules) {
    if((!hideInputCapture && !hideObjectiveAi) || !OwnedHideRules(rules) ||
       GetCurrentThreadId()!=queue.ThreadId())return;
    for(int slot=0;slot<8;++slot) {
        BYTE* pawn=nullptr;if(!HideOwnedBot(slot,pawn))continue;
        const auto& identity=hideRoundPawns[slot];
        bool sameLease=hideBotInputIsolated[slot].object==pawn && hideBotInputIsolated[slot].serial==identity.serial;
        // Original phases may enable input again on the same role pawn. A
        // cached identity must not hide that real change from this adapter.
        if(sameLease && !(*reinterpret_cast<unsigned*>(pawn+0x228)&0x10))continue;
        auto controller=reinterpret_cast<BYTE*>(hideRoundSeats[slot].controller);
        // Standalone PlayerControllers also build a keyboard axis stack for
        // these unconnected bots. It submits zero input after our rule tick.
        // Original APawn::DisableInput removes only this owned bot pawn from
        // that stack; its real movement component still consumes AI controls.
        // Preserve actual UPlayer controllers and every native role transition.
        if(*reinterpret_cast<void**>(pawn+0x258)!=controller ||
           (*reinterpret_cast<BYTE***>(pawn))[0x2f8/8]!=image+0x4154230)continue;
        using Disable=void(__fastcall*)(void*,void*);
        reinterpret_cast<Disable>(image+0x4154230)(pawn,controller);
        if(*reinterpret_cast<unsigned*>(pawn+0x228)&0x10)continue;
        hideBotInputIsolated[slot]=identity;
        RuntimeEvent(sameLease?"native_hide_bot_axis_reisolated_after_native_enable":"native_hide_bot_player_axis_isolated",slot,1);
    }
}
void StopHideObjectiveInputs() {
    if(!hideObjectiveAi)return;
    memset(hideAiControlValid,0,sizeof(hideAiControlValid));
    for(int slot=0;slot<8;++slot) {
        BYTE* pawn=nullptr;if(!HideOwnedBot(slot,pawn))continue;
        using Input=void(__fastcall*)(void*,float);
        if(*reinterpret_cast<BYTE**>(pawn)==image+0x5017c10) {
            auto control=*reinterpret_cast<BYTE**>(pawn+0x4d0);
            if(!control || *reinterpret_cast<BYTE**>(control)!=image+0x501d450 || *reinterpret_cast<void**>(control+8)!=pawn)continue;
            reinterpret_cast<Input>(image+0x10eea60)(pawn,0);
            reinterpret_cast<Input>(image+0x10ee980)(pawn,0);
        } else {
            auto movement=TwinKartLocalRace::FlagFind(pawn,0x22f9030);if(!movement)continue;
            reinterpret_cast<Input>(image+0x15d4320)(movement,0);
            reinterpret_cast<Input>(image+0x15d3fd0)(movement,0);
        }
    }
}
void ApplyHideObjectiveInputs(BYTE* rules) {
    if(!hideObjectiveAi || !hideRoundInputsActive || !OwnedHideRules(rules))return;
    using Input=void(__fastcall*)(void*,float);
    for(int slot=0;slot<8;++slot) {
        BYTE* pawn=nullptr;if(!hideAiControlValid[slot] || !HideOwnedBot(slot,pawn))continue;
        if(*reinterpret_cast<BYTE**>(pawn)==image+0x5017c10) {
            auto control=*reinterpret_cast<BYTE**>(pawn+0x4d0);
            if(!control || *reinterpret_cast<BYTE**>(control)!=image+0x501d450 || *reinterpret_cast<void**>(control+8)!=pawn)continue;
            reinterpret_cast<Input>(image+0x10eea60)(pawn,hideAiForward[slot]);
            reinterpret_cast<Input>(image+0x10ee980)(pawn,hideAiSide[slot]);
        } else {
            if(!hideSearchActive)continue;
            auto movement=TwinKartLocalRace::FlagFind(pawn,0x22f9030);if(!movement)continue;
            reinterpret_cast<Input>(image+0x15d4320)(movement,hideAiForward[slot]);
            reinterpret_cast<Input>(image+0x15d3fd0)(movement,hideAiSide[slot]);
        }
    }
}
void PrepareHideContactPair(int hider,int seeker) {
    auto hidden=static_cast<BYTE*>(hideRoundPawns[hider].object),searching=static_cast<BYTE*>(hideRoundPawns[seeker].object);
    auto hiderRoot=*reinterpret_cast<BYTE**>(hidden+0x130),seekerRoot=*reinterpret_cast<BYTE**>(searching+0x130);
    if(!TwinKartLocalRace::Live(hiderRoot) || !TwinKartLocalRace::Live(seekerRoot) ||
       *reinterpret_cast<void**>(hiderRoot+0xa0)!=hidden || *reinterpret_cast<void**>(seekerRoot+0xa0)!=searching ||
       !TwinKartRace::ObjectiveAssetIsA(image,hiderRoot,0x45115c0) ||
       !TwinKartRace::ObjectiveAssetIsA(image,seekerRoot,0x45115c0))return;
    TwinKartCamera::ObjectIdentity first{},second{};
    if(!TwinKartLocalRace::CaptureFlagObject(hiderRoot,first) || !TwinKartLocalRace::CaptureFlagObject(seekerRoot,second))return;
    auto& cached=hideOverlapPairs[hider][seeker];
    if(cached.hiderRoot.object==first.object && cached.hiderRoot.serial==first.serial &&
       cached.seekerRoot.object==second.object && cached.seekerRoot.serial==second.serial)return;
    auto firstTable=*reinterpret_cast<BYTE***>(hiderRoot),secondTable=*reinterpret_cast<BYTE***>(seekerRoot);
    for(auto table:{firstTable,secondTable})for(unsigned slot:{0x4f8u,0x4f0u,0x830u})
        if(table[slot/8]<image || table[slot/8]>=image+0x4ed0000)return;
    using Channel=unsigned char(__fastcall*)(void*);using SetResponse=void(__fastcall*)(void*,int,int);
    using Response=unsigned char(__fastcall*)(void*,int);using Generate=void(__fastcall*)(void*,bool);
    int firstChannel=reinterpret_cast<Channel>(firstTable[0x4f8/8])(hiderRoot);
    int secondChannel=reinterpret_cast<Channel>(secondTable[0x4f8/8])(seekerRoot);
    RuntimeEvent("native_hide_role_contact_channels",hider,(firstChannel<<8)|secondChannel);
    if(!TwinKartObjective::HideContactChannels(firstChannel,secondChannel))return;
    // Enable actual role-pawn overlaps through original component APIs. Only
    // this pair's instance responses change; floor/wall channels and class
    // defaults are preserved. Blocking hits on disguise children remain valid.
    reinterpret_cast<SetResponse>(firstTable[0x830/8])(hiderRoot,secondChannel,1);
    reinterpret_cast<SetResponse>(secondTable[0x830/8])(seekerRoot,firstChannel,1);
    reinterpret_cast<Generate>(image+0x3e51500)(hiderRoot,true);
    reinterpret_cast<Generate>(image+0x3e51500)(seekerRoot,true);
    bool ready=reinterpret_cast<Response>(firstTable[0x4f0/8])(hiderRoot,secondChannel)==1 &&
        reinterpret_cast<Response>(secondTable[0x4f0/8])(seekerRoot,firstChannel)==1 &&
        (hiderRoot[0x222]&1) && (seekerRoot[0x222]&1);
    if(ready){cached={first,second};RuntimeEvent("native_hide_original_role_overlap_ready",hider,seeker);}
}
void ProcessHideObjectiveContacts(BYTE* rules) {
    if(!hideObjectiveAi || !hideSearchActive || !OwnedHideRules(rules) || !hideRoundRosterKnown)return;
    ULONGLONG now=GetTickCount64();if(now<hideContactNext)return;hideContactNext=now+50;
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);int count=*reinterpret_cast<int*>(image+0x69c28c4);
    auto world=TwinKartLocalRace::World(rules);TwinKartObjective::HideObservation observations[8]{};
    for(int slot=0;slot<8;++slot) {
        auto pawn=static_cast<BYTE*>(hideRoundPawns[slot].object);
        if(!TwinKartCamera::Live(hideRoundPawns[slot],chunks,count) || !HideRolePawn(pawn,world))continue;
        observations[slot].valid=TwinKartLocalRace::FlagPosition(pawn,observations[slot].position);
        observations[slot].hider=*reinterpret_cast<BYTE**>(pawn)==image+0x5017c10;
        if(observations[slot].hider) {
            auto disguise=TwinKartLocalRace::FlagFind(pawn,0x10f43e0);
            auto child=disguise?*reinterpret_cast<BYTE**>(disguise+0x210):nullptr;
            if(TwinKartLocalRace::Live(child))TwinKartLocalRace::CaptureFlagObject(child,hideRoleChildren[slot]);
            else hideRoleChildren[slot]={};
        }
    }
    using Overlap=bool(__fastcall*)(void*,void*);
    for(int hider=0;hider<8;++hider)if(observations[hider].valid && observations[hider].hider) {
        if(!hideSearchActive || !OwnedHideRules(rules))return;
        auto hidden=static_cast<BYTE*>(hideRoundPawns[hider].object);
        if(!TwinKartCamera::Live(hideRoundPawns[hider],chunks,count) || hidden[0x5c]&0x10)continue;
        for(int seeker=0;seeker<8;++seeker)if(observations[seeker].valid && !observations[seeker].hider) {
            auto searching=hideRoundPawns[seeker].object;
            if(!TwinKartCamera::Live(hideRoundPawns[seeker],chunks,count))continue;
            PrepareHideContactPair(hider,seeker);
            auto contact=hidePhysicalContacts[hider];
            bool physical=contact.at && now>=contact.at && now-contact.at<=500 &&
                contact.hider.object==hidden && contact.hider.serial==hideRoundPawns[hider].serial &&
                contact.seeker.object==searching && contact.seeker.serial==hideRoundPawns[seeker].serial &&
                TwinKartCamera::Live(contact.actor,chunks,count) &&
                (contact.actor.object==hidden || (contact.actor.object==hideRoleChildren[hider].object &&
                    contact.actor.serial==hideRoleChildren[hider].serial));
            bool contactNearby=TwinKartRace::ArenaDistance(observations[hider].position,observations[seeker].position)<500 &&
                std::fabs(observations[hider].position.z-observations[seeker].position.z)<250;
            if(!physical && (!contactNearby || !reinterpret_cast<Overlap>(image+0x3cba2a0)(searching,hidden)))continue;
            hidePhysicalContacts[hider]={};
            bool caught=reinterpret_cast<bool(__fastcall*)(void*,bool,bool)>(image+0x3cac200)(hidden,false,true);
            RuntimeEvent(physical?"native_hide_ai_original_blocking_catch":"native_hide_ai_original_overlap_catch",hider,caught?1:0);break;
        }
    }
}
void UpdateHideObjectiveAi(BYTE* rules) {
    if(!hideObjectiveAi || !hideRoundInputsActive || hideGroundRejected || !OwnedHideRules(rules) || !hideRoundRosterKnown ||
       (hideInputCapture && (!hideInputCaptureDone || !hideSeekerInputDone)))return;
    ULONGLONG now=GetTickCount64();bool planningDue=now>=hideAiNext;
    // Physics probes run on the game thread. Keep graph construction on the
    // same cadence as planning so a fast-rendered frame never does another
    // full burst of line traces while the first round is starting.
    if(!planningDue)return;
    float dt=hideAiNext?static_cast<float>(now-(hideAiNext-50))/1000.f:0.05f;
    if(planningDue)hideAiNext=now+50;if(dt<=0 || dt>0.5f)dt=0.05f;
    auto world=TwinKartLocalRace::World(rules);
    const bool repairedLayout=hideRepairedSceneEnabled && !hideArenaCapture && hideRepairedScene.ready &&
        HideMaterialIdentityLive(hideRepairedScene.world) && hideRepairedScene.world.object==world;
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);int count=*reinterpret_cast<int*>(image+0x69c28c4);
    TwinKartObjective::HideObservation observations[8]{};
    float headings[8]{};bool any=false;
    for(int slot=0;slot<8;++slot) {
        auto pawn=static_cast<BYTE*>(hideRoundPawns[slot].object);
        if(!TwinKartCamera::Live(hideRoundPawns[slot],chunks,count) || !HideRolePawn(pawn,world))continue;
        auto& observation=observations[slot];
        observation.valid=TwinKartLocalRace::FlagPosition(pawn,observation.position,&headings[slot]);
        observation.hider=*reinterpret_cast<BYTE**>(pawn)==image+0x5017c10;any|=observation.valid;
        if(observation.valid && !hideAiOriginValid[slot]){hideAiOrigin[slot]=observation.position;hideAiOriginValid[slot]=true;}
        if(observation.hider) {
            auto disguise=TwinKartLocalRace::FlagFind(pawn,0x10f43e0);
            auto child=disguise?*reinterpret_cast<BYTE**>(disguise+0x210):nullptr;
            if(TwinKartLocalRace::Live(child))TwinKartLocalRace::CaptureFlagObject(child,hideRoleChildren[slot]);
            else hideRoleChildren[slot]={};
        }
    }
    if(!any){StopHideObjectiveInputs();return;}
    if(!hideObjectTypeCount) {
        // Resolve native object-query indices through the original collision
        // profile conversion. Do not assume project-specific enum ordering.
        using Channel=unsigned char(__fastcall*)(int);unsigned seenChannels=0;
        for(unsigned char query=0;query<32 && hideObjectTypeCount<3;++query) {
            auto channel=reinterpret_cast<Channel>(image+0x3f159a0)(query);
            if((channel==0 || channel==1 || channel==17) && !(seenChannels&(1u<<channel))) {
                seenChannels|=1u<<channel;hideObjectTypes[hideObjectTypeCount++]=query;
            }
        }
        if(hideObjectTypeCount!=3){hideObjectTypeCount=0;return;}
    }
    if(!hideGround.Started()) {
        TwinKartRace::ArenaPoint low{},high{};bool first=true;
        for(const auto& observation:observations)if(observation.valid) {
            auto p=observation.position;if(first){low=high=p;first=false;}
            low.x=(std::min)(low.x,p.x);low.y=(std::min)(low.y,p.y);low.z=(std::min)(low.z,p.z);
            high.x=(std::max)(high.x,p.x);high.y=(std::max)(high.y,p.y);high.z=(std::max)(high.z,p.z);
        }
        int areas=0;
        // In the repaired arena the authored walls and role spawn heights are
        // already known. Its huge test-map prop volumes are not playable bounds.
        for(int index=0;!repairedLayout && index<count;++index) {
            auto chunk=chunks[index/65536];if(!chunk || (chunk[index%65536].flags&0x30000000))continue;
            auto actor=static_cast<BYTE*>(chunk[index%65536].object);
            if(!TwinKartLocalRace::Live(actor) || (*reinterpret_cast<unsigned*>(actor+8)&0x18030) ||
               (*reinterpret_cast<BYTE**>(actor)!=image+0x501d658 &&
                !(hideArenaCapture && TwinKartRace::ObjectiveAssetIsA(image,actor,0x10f3610))))continue;
            using GetWorld=BYTE*(__fastcall*)(void*);
            if(reinterpret_cast<GetWorld>((*reinterpret_cast<void***>(actor))[0x160/8])(actor)!=world)continue;
            TwinKartRace::ArenaPoint center{},extent{};
            using Bounds=void(__fastcall*)(void*,bool,TwinKartRace::ArenaPoint*,TwinKartRace::ArenaPoint*,bool);
            reinterpret_cast<Bounds>(image+0x3cb2830)(actor,false,&center,&extent,false);
            if(!TwinKartRace::Finite(center) || !TwinKartRace::Finite(extent) || extent.x<0 || extent.y<0 || extent.z<0 ||
               extent.x>15000 || extent.y>15000 || extent.z>15000)continue;
            low.x=(std::min)(low.x,center.x-extent.x);low.y=(std::min)(low.y,center.y-extent.y);
            low.z=(std::min)(low.z,center.z-extent.z);
            high.x=(std::max)(high.x,center.x+extent.x);high.y=(std::max)(high.y,center.y+extent.y);
            high.z=(std::max)(high.z,center.z+extent.z);++areas;
        }
        RuntimeEvent("native_hide_ai_native_spawn_areas",-1,areas);
        hideGroundTop=high.z+2500;hideGroundBottom=low.z-3000;
        low.x-=1600;low.y-=1600;high.x+=1600;high.y+=1600;
        low.z=hideGroundBottom;high.z=hideGroundTop;
        if(repairedLayout) {
            // Keep patrols inside the authored physical walls, independently
            // of the prototype's much larger original prop-spawn volumes.
            if(!TwinKartObjective::HideRepairNavigationBounds(hideRepairedScene.layout,low,high)) {
                hideGroundRejected=true;RuntimeEvent("native_hide_ai_authored_navigation_bounds_rejected",-1,0);return;
            }
        }
        // This authored floor and its divider entrances are already sampled
        // during layout validation; a coarser grid still resolves both routes
        // without forcing AI to wait through most of the first search.
        if(!hideGround.InitializeBounded(low,high,repairedLayout?475.f:300.f)) {
            hideGroundRejected=true;RuntimeEvent("native_hide_ai_road_sampling_bounds_rejected",-1,1);return;
        }
        hideGround.AllowSmoothing(false);
        RuntimeEvent("native_hide_ai_road_sampling_started",-1,static_cast<int>(hideGround.ColumnCount()));
    }
    LARGE_INTEGER frequency{},begin{};QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&begin);
    const auto insideBudget=[&]() {
        LARGE_INTEGER current{};QueryPerformanceCounter(&current);
        return !frequency.QuadPart || current.QuadPart-begin.QuadPart<frequency.QuadPart/400;
    };
    for(int sample=0;sample<32 && !hideGround.SamplingDone() && (sample==0 || insideBudget());++sample) {
        TwinKartRace::ArenaPoint start{},end{};TwinKartRace::RoadSurface contact{};if(!hideGround.NextRay(start,end))break;
        auto result=HideReadSupport(start,end,contact,world);hideGround.Sample(result,contact);
    }
    hideGround.BuildEdges(64);
    if(hideGround.Rejected()) {
        hideGroundRejected=true;StopHideObjectiveInputs();
        RuntimeEvent("native_hide_ai_layered_road_rejected",-1,hideGround.Rejection());return;
    }
    if(!hideGround.Ready() || !hideGround.ValidCount())return;
    HideLineContext geometry{world};
    unsigned supportBudget=32;
    for(int sample=0;sample<32 && supportBudget && insideBudget() && hideAdjacencySample<hideGround.AdjacencyCount();++sample) {
        int a=0,b=0;TwinKartRace::ArenaPoint first{},second{};
        if(!hideGround.AdjacencyAt(hideAdjacencySample,a,b,first,second)) {
            hideGroundRejected=true;StopHideObjectiveInputs();return;
        }
        if(hideRoadSupport.State()==TwinKartRace::SupportState::Idle)hideRoadSupport.Begin(first,second,repairedLayout?100.f:50.f);
        supportBudget-=hideRoadSupport.Step((std::min)(supportBudget,4u),HideReadSupport,world);
        if(hideRoadSupport.State()==TwinKartRace::SupportState::Reading)continue;
        bool clear=hideRoadSupport.State()==TwinKartRace::SupportState::Supported && HideSightClear(geometry,first,second);
        hideGround.ResolveTransition(hideAdjacencySample,clear);if(!clear)++hideBlockedEdges;
        hideRoadSupport.Reset();++hideAdjacencySample;
        if(hideGround.Verified())RuntimeEvent("native_hide_ai_road_sampling_ready",static_cast<int>(hideGround.ValidCount()),
            static_cast<int>(hideGround.ColumnCount()));
    }
    if(!hideGround.Verified())return;
    if(!planningDue)return;
    bool log=now>=hideAiLogAt;if(log)hideAiLogAt=now+5000;
    memset(hideAiControlValid,0,sizeof(hideAiControlValid));
    for(int slot=0;slot<8;++slot) {
        BYTE* pawn=nullptr;if(!HideOwnedBot(slot,pawn))continue;
        // With its keyboard axes isolated, a bot no longer receives the
        // engine's automatic zero input. A failed anchor/route must therefore
        // explicitly replace the preceding acceleration with neutral controls.
        hideAiForward[slot]=hideAiSide[slot]=0;hideAiControlValid[slot]=true;
        if(!observations[slot].valid)continue;
        auto position=observations[slot].position;bool hider=observations[slot].hider;
        if(!hider && !hideSearchActive)continue;
        HideLineContext pathContext{world};IgnoreHideRole(pathContext,pawn);
        TwinKartRace::ArenaPoint inward{};
        if(!hider && repairedLayout && (hideAiEnabled&(1u<<slot)) &&
           TwinKartObjective::HideSeekerInwardGoal(hideRepairedScene.layout,position,inward)) {
            // A recently contacted wall can overlap the pawn center by a
            // few centimeters. Start this clearance ray slightly inward.
            const float distance=TwinKartRace::ArenaDistance(position,inward);
            const float lead=(std::min)(.4f,120.f/distance);
            TwinKartRace::ArenaPoint from{position.x+(inward.x-position.x)*lead,
                position.y+(inward.y-position.y)*lead,position.z};
            if(HideSightClear(pathContext,from,inward)) {
                const auto control=TwinKartObjective::SteerHideSeekerInward(position,headings[slot],inward);
                hideAiForward[slot]=control.throttle;hideAiSide[slot]=control.steer;
                hideRoutes[slot].Reset();hideDrivers[slot].Reset();hidePatrolProgress[slot].Reset();hideAiGoalAt[slot]=0;
                if(log)RuntimeEvent("native_hide_seeker_boundary_reentry_input",slot,static_cast<int>(control.throttle*1000));
                continue;
            }
        }
        auto goal=hideGoals[slot];int target=-1;
        if(!hider) {
            target=TwinKartObjective::HideMovingTarget(observations,hidePreviousObservation,8,slot);
            if(target>=0) {
                auto sight=pathContext;IgnoreHideRole(sight,static_cast<BYTE*>(hideRoundPawns[target].object));
                const auto& targetPosition=observations[target].position;
                if(repairedLayout && TwinKartObjective::ClassifyHidePerimeter(hideRepairedScene.layout,targetPosition)==
                    TwinKartObjective::HidePerimeterPosition::Outside)target=-1;
                else if(!HideSightClear(sight,position,targetPosition))target=-1;
            }
        }
        float safety=hider?TwinKartObjective::HideSafety(position,observations,8):0;
        float goalDistance=TwinKartRace::ArenaDistance(position,goal);
        auto& route=hideRoutes[slot];
        bool failedRoute=route.planned && route.points.empty() && now-route.planned>1000;
        if(failedRoute){route.Reset();hideAiGoalAt[slot]=0;}
        bool settled=hider && hideAiGoalAt[slot] && goalDistance<200 && safety>3000;
        bool renew=!hideAiGoalAt[slot] || (hider?(goalDistance<200 && !settled) ||
            (now-hideAiGoalAt[slot]>4000 && safety<2500):hidePatrolProgress[slot].Renew(position,goal,now));
        if(settled)goal=position;
        if(target>=0){goal=observations[target].position;hideAiGoalAt[slot]=now;}
        else if(renew) {
            int region=hideGround.RegionAt(position,HidePathLine,&pathContext);
            if(region<0){if(log)RuntimeEvent("native_hide_ai_start_anchor_rejected",slot,region);continue;}
            std::uint32_t entropy=0;
            if(BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&entropy),sizeof(entropy),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)continue;
            float best=-1.e30f;bool found=false;
            for(int attempt=0;attempt<48;++attempt) {
                size_t at=(static_cast<size_t>(entropy)+static_cast<size_t>(attempt)*2654435761u)%hideGround.Count();
                TwinKartRace::ArenaPoint candidate{};
                // A sampled cell can belong to the start region while its
                // collision-validated goal anchor resolves across a wall.
                // Validate the same anchor that actual Path will consume.
                if(!hideGround.ReachableSampleAt(at,region,candidate,HidePathLine,&pathContext) || std::fabs(candidate.z-position.z)>350)continue;
                float distance=TwinKartRace::ArenaDistance(position,candidate);if(distance<500 || distance>7000)continue;
                float value=hider?TwinKartObjective::HideSafety(candidate,observations,8)-0.15f*distance:distance;
                if(value>best){best=value;goal=candidate;found=true;}
            }
            if(!found){if(log)RuntimeEvent("native_hide_ai_reachable_goal_missing",slot,region);continue;}
            hideAiGoalAt[slot]=now;if(!hider)hidePatrolProgress[slot].Reset();
        }
        hideGoals[slot]=goal;
        // Require a real sampled path. The generic route helper's direct-goal
        // fallback is deliberately not used for these development actors.
        if(!route.planned || TwinKartRace::ArenaDistance(route.goal,goal)>400 || now-route.planned>2000) {
            route.planned=now;route.goal=goal;route.next=0;
            hideGround.Path(position,goal,route.points,HidePathLine,&pathContext);
        }
        while(route.next<route.points.size() && TwinKartRace::ArenaDistance(position,route.points[route.next])<220 &&
              std::fabs(position.z-route.points[route.next].z)<200)++route.next;
        TwinKartRace::ArenaPoint steering=route.next<route.points.size()?route.points[route.next]:position;
        if(TwinKartRace::ArenaDistance(position,steering)>1 && !HideSightClear(pathContext,position,steering)) {
            route.Reset();hideAiGoalAt[slot]=0;hideDrivers[slot].Reset();continue;
        }
        if(hider) {
            auto control=*reinterpret_cast<BYTE**>(pawn+0x4d0);
            if(!control || *reinterpret_cast<BYTE**>(control)!=image+0x501d450 || *reinterpret_cast<void**>(control+8)!=pawn)continue;
            float angle=std::atan2(steering.y-position.y,steering.x-position.x)-headings[slot];
            float magnitude=TwinKartRace::ArenaDistance(position,steering)>100?0.8f:0;
            hideAiForward[slot]=std::cos(angle)*magnitude;hideAiSide[slot]=std::sin(angle)*magnitude;
        } else {
            auto movement=TwinKartLocalRace::FlagFind(pawn,0x22f9030);
            if(!movement || TwinKartLocalRace::Live(*reinterpret_cast<BYTE**>(movement+0xf08)))continue;
            auto kart=TwinKartLocalRace::FlagFind(pawn,0x22fef20);
            if(!kart || !TwinKartLocalRace::Live(*reinterpret_cast<BYTE**>(kart+0x4c0)))continue;
            if(!(hideAiEnabled&(1u<<slot))) {
                auto interface=movement+0x140;
                if(*reinterpret_cast<BYTE**>(interface)!=image+0x5118b80)continue;
                reinterpret_cast<void(__fastcall*)(void*,bool)>(image+0x15be550)(interface,true);hideAiEnabled|=1u<<slot;
            }
            auto input=hideDrivers[slot].Step(position,headings[slot],steering,dt,slot);
            if(repairedLayout)input=TwinKartObjective::BoundHideSeekerSteering(hideRepairedScene.layout,position,headings[slot],input);
            hideAiForward[slot]=TwinKartRace::NativeKartThrottle(input);hideAiSide[slot]=input.steer;
        }
        hideAiControlValid[slot]=true;
        if(log)RuntimeEvent("native_hide_ai_native_control",slot,(hider?1:2)|(target>=0?4:0)|(route.next<route.points.size()?8:0));
        if(log) {
            if(hideAiOriginValid[slot])RuntimeEvent("native_hide_ai_original_displacement",slot,
                static_cast<int>(TwinKartRace::ArenaDistance(hideAiOrigin[slot],position)));
            RuntimeEvent("native_hide_ai_route_size",slot,static_cast<int>(route.points.size()));
            RuntimeEvent("native_hide_ai_route_cursor",slot,static_cast<int>(route.next));
            RuntimeEvent("native_hide_ai_goal_distance",slot,static_cast<int>(TwinKartRace::ArenaDistance(position,goal)));
            RuntimeEvent("native_hide_ai_forward_milli",slot,static_cast<int>(hideAiForward[slot]*1000));
            RuntimeEvent("native_hide_ai_side_milli",slot,static_cast<int>(hideAiSide[slot]*1000));
        }
    }
    if(log)RuntimeEvent("native_hide_ai_blocked_road_edges",-1,static_cast<int>(hideBlockedEdges));
    memcpy(hidePreviousObservation,observations,sizeof(observations));
}
void AdvanceHideSearchDeadline(BYTE* rules,float delta) {
    if(!hideDeadlineCompatibility || !hideSearchOwner.object)return;
    void* notify=nullptr;
    __try {
        if(!OwnedHideRules(rules) || !TwinKartCamera::Live(hideSearchOwner,
            *reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),*reinterpret_cast<int*>(image+0x69c28c4))) {
            hideSearchClock.Stop();hideSearchOwner={};return;
        }
        if(!hideSearchClock.Advance(rules,static_cast<unsigned>(hideSearchOwner.serial),delta))return;
        // Original GameFlow's first end-expression, reset along with the
        // real no-hiders expression at scope exit. The retained prototype
        // does not schedule Config.Game; supply only this elapsed-time event.
        // Its native callbacks retain all role/result decisions and cleanup.
        auto condition=*reinterpret_cast<BYTE**>(rules+0x130);
        if(!condition || *reinterpret_cast<BYTE**>(condition)!=image+0x5017460 ||
           *reinterpret_cast<BYTE**>(condition+0x20)!=image+0x5017428) {
            RuntimeEvent("native_hide_search_end_expression_rejected",-1,0);return;
        }
        if(!condition[0x40]) {
            condition[0x40]=1;
            hideDeadlineSignalOwner=hideSearchOwner;
            hideDeadlineSignal=condition;
            RuntimeEvent("native_hide_compat_search_deadline_elapsed",-1,1);
            notify=condition+0x20;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {hideSearchClock.Stop();hideSearchOwner={};RuntimeEvent("native_hide_search_deadline_rejected",-1,1);}
    // Original notification and any resulting original callbacks run outside
    // the observer read boundary; their failures cannot be swallowed as data.
    if(notify)reinterpret_cast<void(__fastcall*)(void*)>(image+0x10ee910)(notify);
}
void GuardHideSeekerPerimeter(BYTE* rules) {
    using namespace TwinKartRace;
    struct SafeSample {
        TwinKartCamera::ObjectIdentity pawn;ArenaPoint at;ComponentRotation rotation;
        ULONGLONG retry,confirmation;unsigned stableTicks;
        int velocitySerial;
    };
    static SafeSample safe[8]{};
    if(!hideSearchActive || !hideRoundInputsActive || !hideRoundRosterKnown || !hideRepairedScene.ready || !OwnedHideRules(rules)) {
        for(auto& sample:safe)sample={};
        return;
    }
    __try {
        auto world=TwinKartLocalRace::World(rules);
        if(world!=hideRepairedScene.world.object || !HideMaterialIdentityLive(hideRepairedScene.world))return;
        const auto now=GetTickCount64();
        for(int slot=0;slot<8;++slot) {
            const auto id=hideRoundPawns[slot];auto pawn=static_cast<BYTE*>(id.object);
            auto controller=static_cast<BYTE*>(hideDisguiseControllers[slot].object);
            auto& sample=safe[slot];
            if(!HideMaterialIdentityLive(id) || !HideMaterialIdentityLive(hideDisguiseControllers[slot]) ||
               !HideRolePawn(pawn,world) || *reinterpret_cast<BYTE**>(pawn)!=image+0x501a0c8 ||
               *reinterpret_cast<void**>(controller+0x250)!=pawn || *reinterpret_cast<void**>(pawn+0x258)!=controller) {
                sample={};continue;
            }
            if(sample.pawn.object!=pawn || sample.pawn.serial!=id.serial)sample={};
            ArenaPoint position{};if(!TwinKartLocalRace::FlagPosition(pawn,position))continue;
            auto root=*reinterpret_cast<BYTE**>(pawn+0x130);
            if(!TwinKartLocalRace::Live(root) || *reinterpret_cast<BYTE**>(root)!=image+0x5116878 ||
               *reinterpret_cast<void**>(root+0xa0)!=pawn)continue;
            using Position=TwinKartObjective::HidePerimeterPosition;
            auto edge=TwinKartObjective::ClassifyHidePerimeter(hideRepairedScene.layout,position);
            auto table=*reinterpret_cast<BYTE***>(root);
            const auto& layout=hideRepairedScene.layout;
            const float edgeClearance=(std::min)({position.x-layout.low.x,layout.high.x-position.x,
                position.y-layout.low.y,layout.high.y-position.y});
            if(edge!=Position::Invalid && edgeClearance<2000.f && table[0x610/8]==image+0x41e0a50) {
                // Original PrimitiveComponent GetPhysicsLinearVelocity
                // reflected wrapper returns the live body velocity through
                // the hidden result pointer. Use the matching original
                // setter, retaining tangential and vertical velocity.
                using Velocity=void*(__fastcall*)(void*,ArenaPoint*,unsigned long long);
                ArenaPoint actual{},bounded{};
                if(reinterpret_cast<Velocity>(image+0x41d3a80)(root,&actual,0)==&actual &&
                   TwinKartObjective::LimitHidePerimeterVelocity(layout,position,actual,bounded)) {
                    reinterpret_cast<void(__fastcall*)(void*,const ArenaPoint*,bool,unsigned long long)>(table[0x610/8])(root,&bounded,false,0);
                    if(sample.velocitySerial!=id.serial) {
                        sample.velocitySerial=id.serial;
                        RuntimeEvent("native_hide_seeker_outward_body_velocity_limited",slot,1);
                    }
                }
            }
            if(sample.confirmation) {
                if(edge==Position::SafeInterior) {
                    if(++sample.stableTicks<3 || now-sample.confirmation<150)continue;
                    RuntimeEvent("native_hide_seeker_perimeter_recovered",slot,1);
                } else RuntimeEvent("native_hide_seeker_perimeter_recovery_rebounded",slot,1);
                sample.confirmation=0;sample.stableTicks=0;
            }
            if(edge==Position::SafeInterior) {
                ComponentRotation rotation{};memcpy(&rotation,root+0x1d0,sizeof(rotation));
                if(SweepQuaternion(rotation)){sample.pawn=id;sample.at=position;sample.rotation=rotation;}
                continue;
            }
            if(edge!=Position::Outside)continue;
            // An escaped kart cannot submit a catch from outside the arena.
            for(auto& contact:hidePhysicalContacts)if(contact.seeker.object==pawn && contact.seeker.serial==id.serial)contact={};
            if(!HideMaterialIdentityLive(sample.pawn) || now<sample.retry || ArenaDistance(sample.at,position)>3600.f)continue;
            auto movement=TwinKartLocalRace::FlagFind(pawn,0x22f9030);
            if(!TwinKartLocalRace::Live(movement) || *reinterpret_cast<BYTE**>(movement)!=image+0x5118590 ||
               *reinterpret_cast<void**>(movement+0xb8)!=root || *reinterpret_cast<void**>(movement+0xa0)!=pawn)continue;
            auto actorTable=*reinterpret_cast<BYTE***>(pawn),movementTable=*reinterpret_cast<BYTE***>(movement);
            if(table[0x610/8]!=image+0x41e0a50 || actorTable[0x500/8]!=image+0x3cc60c0 ||
               actorTable[0x508/8]!=image+0x4164c50 || movementTable[0x520/8]!=image+0x3e4dd60)continue;
            sample.retry=now+250;
            // K2_TeleportTo calls the original TeleportTo virtual with both
            // flags false. Its success callback notifies the original pawn
            // movement component, which SetActorLocation never called.
            // Reuse the exact Quaternion::Rotator from K2_GetActorRotation.
            ArenaPoint rotation{};
            using ToRotator=void*(__fastcall*)(const ComponentRotation*,ArenaPoint*);
            if(reinterpret_cast<ToRotator>(image+0x250de60)(&sample.rotation,&rotation)!=&rotation || !Finite(rotation))continue;
            using Teleport=bool(__fastcall*)(void*,const ArenaPoint*,const ArenaPoint*,bool,bool);
            if(!reinterpret_cast<Teleport>(actorTable[0x500/8])(pawn,&sample.at,&rotation,false,false) ||
               !HideMaterialIdentityLive(id) || *reinterpret_cast<void**>(pawn+0x130)!=root)continue;
            const ArenaPoint zero{};
            reinterpret_cast<void(__fastcall*)(void*,const ArenaPoint*,bool,unsigned long long)>(table[0x610/8])(root,&zero,false,0);
            ArenaPoint after{};
            if(TwinKartLocalRace::FlagPosition(pawn,after) && TwinKartObjective::ClassifyHidePerimeter(hideRepairedScene.layout,after)==Position::SafeInterior) {
                if(!TwinKartObjective::HideControllerHasPlayer(controller)) {
                    // The previous route led out; clear its goal and native
                    // controls so the next planning tick chooses a new route.
                    hideRoutes[slot].Reset();hideDrivers[slot].Reset();hidePatrolProgress[slot].Reset();
                    hideAiGoalAt[slot]=0;hideAiForward[slot]=hideAiSide[slot]=0;
                }
                sample.confirmation=now;sample.stableTicks=0;
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER){}
}
void __fastcall HideRuleTickObserver(void* raw,float delta,int tickType,void* function) {
    // Observe the real engine callback. The original tick is forwarded once
    // with unchanged parameters, outside the observer's exception boundary.
    __try {
        auto component=static_cast<BYTE*>(raw);
        if(hideCaptureArmed && GetCurrentThreadId()==queue.ThreadId() && TwinKartLocalRace::Live(component)) {
            auto world=TwinKartLocalRace::World(component);
            bool owned=TwinKartLocalRace::objectiveWorld.Matches(world,
                *reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),*reinterpret_cast<int*>(image+0x69c28c4));
            ++hideTickSamples;
            if(owned && std::isfinite(delta) && delta>0 && delta<=5)hideTickSeconds+=delta;
            if(hideTickSamples==1 || !(hideTickSamples%128)) {
                RuntimeEvent("native_hide_rule_actual_tick_count",-1,static_cast<int>(hideTickSamples));
                RuntimeEvent("native_hide_rule_actual_tick_owned",-1,owned?1:0);
                RuntimeEvent("native_hide_rule_actual_tick_milliseconds",-1,static_cast<int>((std::min)(hideTickSeconds*1000,2147483647.0)));
                if(owned) {
                    using Paused=bool(__fastcall*)(void*);
                    RuntimeEvent("native_hide_world_paused",-1,reinterpret_cast<Paused>(image+0x4011c70)(world)?1:0);
                    float seconds=*reinterpret_cast<float*>(world+0x5a0);
                    if(std::isfinite(seconds) && seconds>=0 && seconds<2147483.0f)
                        RuntimeEvent("native_hide_world_time_milliseconds",-1,static_cast<int>(seconds*1000));
                }
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {RuntimeEvent("native_hide_tick_observer_read_rejected",-1,0);}
    hideRuleTick(raw,delta,tickType,function);
    RefreshHideRoundRoster(static_cast<BYTE*>(raw));
    if(hideRoundRosterKnown)PrepareHideInitialDisguises(static_cast<BYTE*>(raw),hideRoundSeats,hideRoundPawns);
    PrepareHideMaterialRepairs(static_cast<BYTE*>(raw));
    UpdateHideLocalDisguiseView(static_cast<BYTE*>(raw));
    PrepareHideSeekerAppearances(static_cast<BYTE*>(raw));
    GuardHideSeekerPerimeter(static_cast<BYTE*>(raw));
    IsolateHideBotPlayerInput(static_cast<BYTE*>(raw));
    CaptureHideHiderInput(static_cast<BYTE*>(raw),delta);
    CaptureHideSeekerInput(static_cast<BYTE*>(raw),delta);
    UpdateHideObjectiveAi(static_cast<BYTE*>(raw));
    ApplyHideObjectiveInputs(static_cast<BYTE*>(raw));
    ProcessHideObjectiveContacts(static_cast<BYTE*>(raw));
    AdvanceHideSearchDeadline(static_cast<BYTE*>(raw),delta);
    PublishHideHint(static_cast<BYTE*>(raw));
    ObserveHideSettlement(static_cast<BYTE*>(raw),false);
}
bool HideStageText(unsigned long long value,char* outputText,size_t capacity) {
    wchar_t text[128]{};
    if(!outputText || capacity<2 || !ModeNameText(value,text,ARRAYSIZE(text)))return false;
    size_t length=wcslen(text);if(length>=capacity)return false;
    for(size_t i=0;i<length;++i) {
        auto c=text[i];
        if(!((c>=L'A' && c<=L'Z') || (c>=L'a' && c<=L'z') || (c>=L'0' && c<=L'9') || c==L'_'))return false;
    }
    for(size_t i=0;i<=length;++i)outputText[i]=static_cast<char>(text[i]);return true;
}
void __fastcall HideStageObserver(void* raw,unsigned long long requested) {
    bool owned=false;unsigned long long before=0;
    __try {
        auto component=static_cast<BYTE*>(raw);
        owned=hideCaptureArmed && GetCurrentThreadId()==queue.ThreadId() && TwinKartLocalRace::Live(component) &&
            *reinterpret_cast<BYTE**>(component)==image+0x50252d8 &&
            TwinKartLocalRace::objectiveWorld.Matches(TwinKartLocalRace::World(component),
                *reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),*reinterpret_cast<int*>(image+0x69c28c4));
        if(owned)before=*reinterpret_cast<unsigned long long*>(component+0x150);
    } __except(EXCEPTION_EXECUTE_HANDLER) {owned=false;}
    // Preserve the original change and all of its notification/stack logic.
    // This observer never supplies a stage, timer, victory or result itself.
    hideStageChange(raw,requested);
    if(!owned || hideStageSamples>=256)return;
    __try {
        auto component=static_cast<BYTE*>(raw);char from[128]{},to[128]{},after[128]{},notify[128]{},line[768]{};
        HideStageText(before,from,ARRAYSIZE(from));HideStageText(requested,to,ARRAYSIZE(to));
        HideStageText(*reinterpret_cast<unsigned long long*>(component+0x150),after,ARRAYSIZE(after));
        HideStageText(*reinterpret_cast<unsigned long long*>(component+0xd0),notify,ARRAYSIZE(notify));
        int size=sprintf_s(line,"{\"event\":\"native_hide_stage_transition\",\"before\":\"%s\",\"requested\":\"%s\",\"after\":\"%s\",\"notify\":\"%s\"}\r\n",from,to,after,notify);
        DWORD written=0;if(size>0)WriteFile(output,line,static_cast<DWORD>(size),&written,nullptr);++hideStageSamples;
    } __except(EXCEPTION_EXECUTE_HANDLER) {RuntimeEvent("native_hide_stage_observer_read_rejected",-1,0);}
}
bool InstallHideCaptureCompatibility() {
    hideLobbyEnabled=EnvironmentFlag(L"TWIN_KART_HIDE_COMPAT") && EnvironmentFlag(L"TWIN_KART_HIDE_REPAIRED_SCENE");
    if(!hideLobbyEnabled && !(EnvironmentFlag(L"TWIN_KART_HIDE_BOT_CAPTURE") && EnvironmentFlag(L"TWIN_KART_HIDE_DATA_CAPTURE")))return true;
    if(GetCurrentThreadId()!=queue.ThreadId())return false;
    wchar_t deadline[4]{};
    hideDeadlineCompatibility=hideLobbyEnabled || (GetEnvironmentVariableW(L"TWIN_KART_HIDE_DEADLINE_COMPAT",deadline,4)==1 && deadline[0]==L'1');
    wchar_t inputCapture[4]{};
    hideInputCapture=hideDeadlineCompatibility && GetEnvironmentVariableW(L"TWIN_KART_HIDE_INPUT_CAPTURE",inputCapture,4)==1 && inputCapture[0]==L'1';
    wchar_t autonomy[4]{};
    hideAutonomyCompatibility=hideDeadlineCompatibility && (hideLobbyEnabled || (GetEnvironmentVariableW(L"TWIN_KART_HIDE_AUTONOMY",autonomy,4)==1 && autonomy[0]==L'1'));
    wchar_t objectiveAi[4]{};
    hideObjectiveAi=hideAutonomyCompatibility && (hideLobbyEnabled || (GetEnvironmentVariableW(L"TWIN_KART_HIDE_OBJECTIVE_AI",objectiveAi,4)==1 && objectiveAi[0]==L'1'));
    wchar_t repaired[4]{};
    hideRepairedSceneEnabled=hideObjectiveAi && GetEnvironmentVariableW(L"TWIN_KART_HIDE_REPAIRED_SCENE",repaired,4)==1 && repaired[0]==L'1';
    // This one audited Hide constructor call precedes AGameModeBase creating
    // its GameSession. The retained base KdGameSession returns null player
    // properties; SessionBasedPlayerComponentInitializer then dereferences it
    // while creating the original Seeker kart. Select the original local
    // DummyGameSession implementation, retaining its real property generation.
    // No fake session object, native method body or result is provided here.
    hideBaseConstructor=reinterpret_cast<TwinKartLocalRace::Construct>(image+0x1668130);
    if(!hideSessionConstructorPatch.Call(image+0x10de12d,reinterpret_cast<void*>(hideBaseConstructor),reinterpret_cast<void*>(HideBaseConstructor)))return false;
    const BYTE tickPrefix[]={0x40,0x53,0x48,0x83,0xec,0x30,0x0f,0x29,0x74,0x24,0x20,0x48,0x8b,0xd9,0x0f,0x28,0xf1};
    if(!hideRuleTickPatch.Entry(image+0x10f30d0,tickPrefix,sizeof(tickPrefix),reinterpret_cast<void*>(HideRuleTickObserver),reinterpret_cast<void**>(&hideRuleTick))) {
        hideRuleTickPatch.Restore();hideSessionConstructorPatch.Restore();return false;
    }
    const BYTE stagePrefix[]={0x48,0x89,0x54,0x24,0x10,0x53,0x56,0x57,0x48,0x83,0xec,0x30,0x48,0x8b,0xda,0x48,0x8b,0xf1};
    if(!hideStagePatch.Entry(image+0x10e4310,stagePrefix,sizeof(stagePrefix),reinterpret_cast<void*>(HideStageObserver),reinterpret_cast<void**>(&hideStageChange))) {
        hideStagePatch.Restore();hideRuleTickPatch.Restore();hideSessionConstructorPatch.Restore();return false;
    }
    if(hideDeadlineCompatibility) {
        hideGrantRoles=reinterpret_cast<HideSearchCallback>(image+0x10e18d0);
        hideSearchBegin=reinterpret_cast<HideSearchCallback>(image+0x10e2720);
        hideSearchEnd=reinterpret_cast<HideSearchCallback>(image+0x10e20d0);
        if(!hideGrantRolesPatch.Call(image+0x10eb5da,reinterpret_cast<void*>(hideGrantRoles),reinterpret_cast<void*>(HideGrantRolesCompatibility)) ||
           !hideSearchBeginPatch.Call(image+0x10eb74a,reinterpret_cast<void*>(hideSearchBegin),reinterpret_cast<void*>(HideSearchBeginCompatibility)) ||
           !hideSearchEndPatch.Call(image+0x10eb6ba,reinterpret_cast<void*>(hideSearchEnd),reinterpret_cast<void*>(HideSearchEndCompatibility))) {
            hideSearchEndPatch.Restore();hideSearchBeginPatch.Restore();hideGrantRolesPatch.Restore();hideStagePatch.Restore();hideRuleTickPatch.Restore();hideSessionConstructorPatch.Restore();return false;
        }
    }
    if(hideInputCapture) {
        const BYTE hidePhysicsPrefix[]={0x4c,0x8b,0xdc,0x53,0x41,0x56,0x48,0x81,0xec,0x08,0x01,0x00,0x00,0x41,0x0f,0x29,0x73,0xd8};
        if(!hideSubstepPatch.Entry(image+0x15d82d0,hidePhysicsPrefix,sizeof(hidePhysicsPrefix),reinterpret_cast<void*>(HideSubstepObserver),reinterpret_cast<void**>(&hideSubstep))) {
            hideSubstepPatch.Restore();hideSearchEndPatch.Restore();hideSearchBeginPatch.Restore();hideGrantRolesPatch.Restore();
            hideStagePatch.Restore();hideRuleTickPatch.Restore();hideSessionConstructorPatch.Restore();return false;
        }
        const BYTE throttlePrefix[]{0x4c,0x8b,0xdc,0x49,0x89,0x5b,0x10,0x49,0x89,0x73,0x18,0x57,0x48,0x81,0xec,0x70,0x01,0x00,0x00};
        if(!hideThrottlePatch.Entry(image+0x15d4320,throttlePrefix,sizeof(throttlePrefix),reinterpret_cast<void*>(HideThrottleObserver),reinterpret_cast<void**>(&hideThrottle))) {
            hideThrottlePatch.Restore();hideSubstepPatch.Restore();hideSearchEndPatch.Restore();hideSearchBeginPatch.Restore();hideGrantRolesPatch.Restore();
            hideStagePatch.Restore();hideRuleTickPatch.Restore();hideSessionConstructorPatch.Restore();return false;
        }
    }
    if(hideObjectiveAi) {
        const BYTE hideHitPrefix[]={0x4c,0x8b,0xdc,0x48,0x81,0xec,0x88,0x00,0x00,0x00,0x48,0x8b,0x84,0x24,0xc8,0x00,0x00,0x00};
        if(!hideHitPatch.Entry(image+0x3cbc4d0,hideHitPrefix,sizeof(hideHitPrefix),reinterpret_cast<void*>(HideHitObserver),reinterpret_cast<void**>(&hideHit))) {
            hideHitPatch.Restore();hideThrottlePatch.Restore();hideSubstepPatch.Restore();hideSearchEndPatch.Restore();hideSearchBeginPatch.Restore();hideGrantRolesPatch.Restore();
            hideStagePatch.Restore();hideRuleTickPatch.Restore();hideSessionConstructorPatch.Restore();return false;
        }
    }
    wchar_t arena[4]{};
    if(GetEnvironmentVariableW(L"TWIN_KART_HIDE_ARENA_CAPTURE",arena,4)==1 && arena[0]==L'1') {
        const BYTE modeCall[]{0xff,0x97,0x48,0x03,0x00,0x00};
        if(!hideArenaModePatch.IndirectCall(image+0x3f3b632,modeCall,sizeof(modeCall),reinterpret_cast<void*>(HideArenaModeOverride))) {
            hideHitPatch.Restore();hideThrottlePatch.Restore();hideSubstepPatch.Restore();hideSearchEndPatch.Restore();hideSearchBeginPatch.Restore();
            hideGrantRolesPatch.Restore();hideStagePatch.Restore();hideRuleTickPatch.Restore();hideSessionConstructorPatch.Restore();return false;
        }
        objectiveArenaModeInstalled=true;
    }
    if(hideLobbyEnabled) {
        InstallHideHumanAxis();
        InstallHideHiderView();
        TwinKartLocalRace::arenaForwardAxis=RecoverHideHumanSeekerAxis;
    }
    RuntimeEvent("native_hide_rule_tick_observer_installed",-1,1);return true;
}
bool InspectOriginalMode(void* raw,TwinKartRace::ModeDefinition& result,void*) {
    BYTE* asset=static_cast<BYTE*>(raw);if(!TwinKartLocalRace::Live(asset) || (*reinterpret_cast<unsigned*>(asset+8)&0x18030))return false;
    const auto& travel=*reinterpret_cast<TwinKartEquipment::String*>(asset+0x238);
    if(!TwinKartEquipment::Valid(travel,64) || !TwinKartEquipment::Nonempty(travel))return false;
    result.asset=asset;result.kind=*reinterpret_cast<int*>(asset+0x230);
    result.teamSize=*reinterpret_cast<int*>(asset+0x248);result.teamCount=*reinterpret_cast<int*>(asset+0x24c);
    memcpy(result.travelName,travel.data,static_cast<size_t>(travel.count)*sizeof(wchar_t));
    return ModeNameText(*reinterpret_cast<unsigned long long*>(asset+0x40),result.gameClass,ARRAYSIZE(result.gameClass));
}
TwinKartObjective::FindModeDefinition originalModeLookup=nullptr;
TwinKartNative::OwnedCodePatch chaserModeLookupPatch;
TwinKartNative::OwnedCodePatch chaserSelectRolesPatch;
TwinKartNative::OwnedCodePatch chaserRoleAuthorityPatch;
TwinKartNative::OwnedCodePatch chaserRoleEventsPatch;
TwinKartNative::OwnedCodePatch chaserLoadingReadyPatch;
TwinKartNative::OwnedCodePatch chaserResultVisibilityPatch;
TwinKartNative::OwnedCodePatch chaserMissingConfigPatch;
TwinKartNative::OwnedCodePatch chaserSpeedItemConditionPatch,chaserLegacyItemGainPatch;
TwinKartNative::OwnedCodePatch chaserCastingObserverPatch;
TwinKartNative::OwnedCodePatch chaserEndCastingObserverPatch;
TwinKartNative::OwnedCodePatch chaserSlotAimObserverPatch,chaserSlotCastObserverPatch;
TwinKartNative::OwnedCodePatch chaserBoosterAppliedPatch;
TwinKartNative::OwnedCodePatch chaserBoosterAuthorityPatch;
TwinKartNative::OwnedCodePatch chaserBoosterAnimationPatch;
TwinKartNative::OwnedCodePatch infiniteBoosterPressPatch;
using ChaserBoosterApplied=void(__fastcall*)(void*,void*);
ChaserBoosterApplied originalChaserBoosterApplied=nullptr;
ChaserBoosterApplied originalChaserBoosterAuthority=nullptr;
thread_local TwinKartObjective::ChaserEffectDelivery* chaserBoosterDelivery=nullptr;
thread_local TwinKartObjective::ChaserRpcDelivery* chaserBoosterRpcDelivery=nullptr;
using ChaserCasting=void(__fastcall*)(void*,int,unsigned char);
ChaserCasting originalChaserCasting=nullptr;
using ChaserEndCasting=void(__fastcall*)(void*,int,unsigned char,void*);
ChaserEndCasting originalChaserEndCasting=nullptr;
thread_local void* chaserSubmittingItem=nullptr;
thread_local int chaserSubmittingSlot=-1;
TwinKartObjective::ChaserLegacyCondition originalChaserSpeedItemCondition=nullptr;
bool chaserItemPolicyInstalled=false;
TwinKartObjective::ChaserNativeNetMode chaserNativeNetMode=nullptr;
TwinKartObjective::ChaserNativeCanTransit chaserCanTransit=nullptr;
bool chaserModeDefinitionEnabled=false,chaserModeLookupInstalled=false;
using ChaserSelectRoles=void(__fastcall*)(void*);
ChaserSelectRoles chaserSelectRoles=nullptr;
TwinKartLocalRace::LocalRaceWorld chaserRoleWorld;
struct ChaserSceneState {
    TwinKartCamera::ObjectIdentity world{},vaultClass{},actors[13]{};
    unsigned count=0;ULONGLONG retry=0;bool prepared=false,rejected=false,building=false;
} chaserScene;
TwinKartCamera::ObjectIdentity chaserObservedRule{},chaserNavigationWorld{};
TwinKartCamera::ObjectIdentity chaserLoadingTransition{},chaserLoadingWorld{};
TwinKartNavigation::WorldBounds chaserBounds;
TwinKartRace::LayeredRoadNavigation chaserGround;
TwinKartRace::RoadSegmentSupport chaserSupport;
TwinKartCamera::ObjectIdentity chaserVaultActors[64]{},chaserZoneActors[64]{};
unsigned chaserVaultCount=0,chaserZoneCount=0;ULONGLONG chaserObjectiveRefresh=0;
TwinKartObjective::ChaserArenaDriver chaserDrivers[16];
TwinKartRace::RoadEdgeQueries chaserEdgeQueries[16];
TwinKartObjective::ChaserRole chaserEdgeRoles[16]{};
TwinKartObjective::ChaserItemInputs chaserItemInputs;
unsigned chaserPreferredItemSlot[16]{};
ULONGLONG chaserItemNextAllowed[16][2]{};
TwinKartCamera::ObjectIdentity chaserDriverPawns[16]{},chaserDriverStates[16]{},chaserDriverMovements[16]{};
TwinKartCamera::ObjectIdentity chaserObservedRolePawns[16]{};
TwinKartObjective::ChaserRole chaserObservedRoles[16]{};
bool chaserDriverPrepared[16]{};
TwinKartCamera::ObjectIdentity chaserHumanForwardOwner{};
TwinKartCamera::ObjectIdentity chaserForwardEdgePawn{};
ULONGLONG chaserForwardEdgeUntil=0;
bool chaserHumanForwardFallback=false,chaserHumanForwardLogged=false;
unsigned char chaserObjectQueries[3]{};int chaserObjectQueryCount=0;
size_t chaserAdjacency=0;float chaserRoadTop=0,chaserRoadBottom=0;
ULONGLONG chaserAiNext=0,chaserAiLast=0,chaserAiLog=0;
unsigned chaserPlannerCursor=0;bool chaserNavigationRejected=false,chaserNavigationReported=false;
thread_local bool preparingChaserConnections=false;
bool PrepareOriginalChaserRoster(BYTE* rules,bool notify);
struct ChaserConnectionContext {
    BYTE* world=nullptr;
    TwinKartCamera::ObjectIdentity rules{};
    TwinKartCamera::ObjectIdentity pawns[16]{},states[16]{};
    unsigned nativeConnectCalls=0;
};
void ClassifyOriginalChaserSpawn(void* raw,void* rawMode,int slot,bool* isAi) {
    __try {
        if(!chaserModeDefinitionEnabled || TwinKartLocalRace::armedMode!=108 || !isAi ||
           GetCurrentThreadId()!=queue.ThreadId())return;
        auto rule=static_cast<BYTE*>(raw),mode=static_cast<BYTE*>(rawMode);
        using Type=void*(__fastcall*)();
        if(!TwinKartLocalRace::Live(rule) || *reinterpret_cast<BYTE**>(rule)!=image+0x51dd410 ||
           !TwinKartLocalRace::Live(mode) || *reinterpret_cast<void**>(mode+0x10)!=reinterpret_cast<Type>(image+0x22507c0)())return;
        auto world=TwinKartLocalRace::World(rule);
        if(!world || *reinterpret_cast<void**>(world+0x118)!=mode || *reinterpret_cast<void**>(rule+0xa0)!=mode)return;
        bool candidate=false;
        if(!TwinKartObjective::ChaserSpawnSlot(*reinterpret_cast<int*>(rule+0x130),*reinterpret_cast<int*>(rule+0x138),slot,candidate))return;
        TwinKartCamera::ObjectIdentity identity;
        if(!TwinKartLocalRace::CaptureFlagObject(world,identity) || !chaserRoleWorld.Bind(identity,
            *reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),*reinterpret_cast<int*>(image+0x69c28c4)))return;
        if(!TwinKartLocalRace::objectiveWorld.Bind(identity,*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),
            *reinterpret_cast<int*>(image+0x69c28c4)))return;
        *isAi=candidate;
        RuntimeEvent("native_chaser_original_spawn_slot_classified",slot,candidate?1:0);
    } __except(EXCEPTION_EXECUTE_HANDLER) {RuntimeEvent("native_chaser_spawn_classification_rejected",slot,0);}
}
bool ChaserConnectionWorld(BYTE* rules,BYTE*& world) {
    if(!chaserModeDefinitionEnabled || TwinKartLocalRace::armedMode!=108 || GetCurrentThreadId()!=queue.ThreadId() ||
       !TwinKartLocalRace::Live(rules) || *reinterpret_cast<BYTE**>(rules)!=image+0x51ce018)return false;
    using Type=void*(__fastcall*)();
    auto mode=*reinterpret_cast<BYTE**>(rules+0xa0);world=TwinKartLocalRace::World(rules);
    if(!world || *reinterpret_cast<BYTE**>(world+0x118)!=mode || !TwinKartLocalRace::Live(mode) ||
       *reinterpret_cast<void**>(mode+0x10)!=reinterpret_cast<Type>(image+0x22507c0)())return false;
    TwinKartCamera::ObjectIdentity identity;
    return TwinKartLocalRace::CaptureFlagObject(world,identity) && chaserRoleWorld.Bind(identity,
        *reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),*reinterpret_cast<int*>(image+0x69c28c4));
}
bool OwnedChaserWorld(BYTE* world) {
    if(!chaserModeDefinitionEnabled || TwinKartLocalRace::armedMode!=108 || GetCurrentThreadId()!=queue.ThreadId())return false;
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);int count=*reinterpret_cast<int*>(image+0x69c28c4);
    if(!chaserRoleWorld.Matches(world,chunks,count))return false;
    if(!TwinKartCamera::Live(chaserObservedRule,chunks,count)) {
        auto mode=*reinterpret_cast<BYTE**>(world+0x118);using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
        if(!TwinKartLocalRace::Live(mode))return false;
        auto type=reinterpret_cast<Type>(image+0x2251300)();
        auto rules=reinterpret_cast<Find>((*reinterpret_cast<void***>(mode))[0x620/8])(mode,type);
        if(!TwinKartLocalRace::OwnedComponent(rules,mode,type) || !TwinKartLocalRace::CaptureFlagObject(rules,chaserObservedRule))return false;
    }
    BYTE* actual=nullptr;
    return ChaserConnectionWorld(static_cast<BYTE*>(chaserObservedRule.object),actual) && actual==world;
}
void ResetChaserNavigation() {
    chaserItemInputs.Reset();
    memset(chaserPreferredItemSlot,0,sizeof(chaserPreferredItemSlot));
    memset(chaserItemNextAllowed,0,sizeof(chaserItemNextAllowed));
    chaserBounds.Reset();chaserGround.Reset();chaserSupport.Reset();chaserNavigationWorld={};chaserObservedRule={};
    chaserLoadingTransition={};chaserLoadingWorld={};
    chaserAdjacency=0;chaserObjectQueryCount=0;chaserRoadTop=chaserRoadBottom=0;
    chaserAiNext=chaserAiLast=chaserAiLog=0;chaserPlannerCursor=0;
    chaserNavigationRejected=chaserNavigationReported=false;
    memset(chaserVaultActors,0,sizeof(chaserVaultActors));memset(chaserZoneActors,0,sizeof(chaserZoneActors));
    chaserVaultCount=chaserZoneCount=0;chaserObjectiveRefresh=0;
    for(int slot=0;slot<16;++slot){chaserDrivers[slot].Reset();chaserEdgeQueries[slot].Reset();
        chaserEdgeRoles[slot]=TwinKartObjective::ChaserRole::Unknown;chaserDriverPawns[slot]={};chaserDriverStates[slot]={};
        chaserDriverMovements[slot]={};chaserDriverPrepared[slot]=false;chaserObservedRolePawns[slot]={};
        chaserObservedRoles[slot]=TwinKartObjective::ChaserRole::Unknown;}
}
void ResetChaserSession(){
    ResetChaserNavigation();chaserRoleWorld.Reset();chaserScene={};
    chaserHumanForwardOwner={};chaserForwardEdgePawn={};chaserForwardEdgeUntil=0;
    chaserHumanForwardFallback=chaserHumanForwardLogged=false;
    InterlockedExchange(&chaserHintState,0);
    hideSettlementJournal.Reset();
    AcquireSRWLockExclusive(&prototypeResultLock);prototypePresentation.Reset();prototypeResults.Reset();hideSettlement={};hideSettlementAvailable=false;ReleaseSRWLockExclusive(&prototypeResultLock);
}
bool ChaserPhase(BYTE* world,unsigned long long& name) {
    using Type=void*(__fastcall*)();
    auto state=*reinterpret_cast<BYTE**>(world+0x120);
    if(!TwinKartLocalRace::Live(state) || *reinterpret_cast<void**>(state+0x10)!=reinterpret_cast<Type>(image+0x2250ab0)())return false;
    auto phase=TwinKartLocalRace::FlagFind(state,TwinKartGraphics::ChaserPhaseComponentGetter);
    auto current=phase?*reinterpret_cast<BYTE**>(phase+TwinKartGraphics::CurrentPhaseOffset):nullptr;
    if(!TwinKartLocalRace::Live(current) || *reinterpret_cast<void**>(current+0x10)!=reinterpret_cast<Type>(image+TwinKartGraphics::PhaseObjectGetter)())return false;
    name=*reinterpret_cast<unsigned long long*>(current+TwinKartGraphics::PhaseNameOffset);return name!=0;
}
bool ChaserIdentityLive(const TwinKartCamera::ObjectIdentity& identity) {
    return TwinKartCamera::Live(identity,*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),
        *reinterpret_cast<int*>(image+0x69c28c4));
}
bool ChaserOwnsItemPolicy(BYTE* controller) {
    __try {
        if(!chaserItemPolicyInstalled || !chaserArenaEnabled || GetCurrentThreadId()!=queue.ThreadId() ||
           !TwinKartLocalRace::Live(controller) || (*reinterpret_cast<unsigned*>(controller+8)&0x18030))return false;
        using Type=void*(__fastcall*)();auto type=reinterpret_cast<Type>(image+0x2218050)();
        if(*reinterpret_cast<void**>(controller+0x10)!=type)return false;
        auto pawn=*reinterpret_cast<BYTE**>(controller+0xa0);
        if(!TwinKartLocalRace::Live(pawn) || !TwinKartLocalRace::OwnedComponent(controller,pawn,type) ||
           *reinterpret_cast<void**>(pawn+0x10)!=reinterpret_cast<Type>(image+0x2413310)())return false;
        auto world=TwinKartLocalRace::World(controller);if(!OwnedChaserWorld(world))return false;
        auto state=TwinKartLocalRace::FlagFind(pawn,0x23afeb0);
        if(!TwinKartLocalRace::OwnedComponent(state,pawn,reinterpret_cast<Type>(image+0x23afeb0)()) ||
           TwinKartLocalRace::World(state)!=world || !state[0x321] || (state[0x338]!=1 && state[0x338]!=3))return false;
        int slot=*reinterpret_cast<int*>(state+0x168);if(slot<1 || slot>15)return false;
        auto table=*reinterpret_cast<BYTE***>(controller);
        return TwinKartObjective::ChaserKnownItemControllerAbi(TwinKartObjective::ChaserItemControllerKind::Speed,
            reinterpret_cast<std::uintptr_t>(image),reinterpret_cast<std::uintptr_t>(table[0x350/8]),
            reinterpret_cast<std::uintptr_t>(table[0x408/8]),reinterpret_cast<std::uintptr_t>(table[0x418/8]));
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool __fastcall ChaserSpeedItemCondition(void* controller) {
    return TwinKartObjective::ResolveChaserLegacyCondition(controller,ChaserOwnsItemPolicy(static_cast<BYTE*>(controller)),
        originalChaserSpeedItemCondition);
}
void __fastcall ChaserLegacyItemGain(void* controller) {
    bool owned=ChaserOwnsItemPolicy(static_cast<BYTE*>(controller));
    TwinKartObjective::ResolveChaserLegacyGain(controller,owned,
        reinterpret_cast<TwinKartObjective::ChaserLegacyGain>(image+0x14e5e30));
    static ULONGLONG next=0;auto now=GetTickCount64();
    if(owned && now>=next){next=now+5000;RuntimeEvent("native_chaser_circuit_item_policy_deferred",-1,1);}
}
bool ChaserLoadingNavigationReady(void* raw,bool& owned,void*) {
    auto rule=static_cast<BYTE*>(raw);using Type=void*(__fastcall*)();using GetWorld=BYTE*(__fastcall*)(void*);
    if(!TwinKartLocalRace::Live(rule) || (*reinterpret_cast<unsigned*>(rule+8)&0x18030) ||
       *reinterpret_cast<void**>(rule+0x10)!=reinterpret_cast<Type>(image+0x2251600)())return false;
    auto world=reinterpret_cast<GetWorld>((*reinterpret_cast<void***>(rule))[0x160/8])(rule);
    unsigned long long phase=0;auto loading=*reinterpret_cast<unsigned long long*>(image+0x68b9e10);
    if(!OwnedChaserWorld(world) || !loading || !ChaserPhase(world,phase) || phase!=loading)return false;
    owned=true;TwinKartCamera::ObjectIdentity ruleId{},worldId{};
    if(chaserArenaEnabled && !PrepareChaserArena(world))return false;
    if(!TwinKartLocalRace::CaptureFlagObject(rule,ruleId) || !TwinKartLocalRace::CaptureFlagObject(world,worldId))return false;
    chaserLoadingTransition=ruleId;chaserLoadingWorld=worldId;
    // Native Loading has completed client setup. Recheck/supply the original
    // bot cores before allowing role initialization; do not send connection
    // notifications until the original role-selection callback.
    if(preparingChaserConnections || !ChaserIdentityLive(chaserObservedRule))return false;
    bool coreReady=false;preparingChaserConnections=true;
    __try {coreReady=PrepareOriginalChaserRoster(static_cast<BYTE*>(chaserObservedRule.object),false);}
    __finally {preparingChaserConnections=false;}
    if(!coreReady || !ChaserIdentityLive(ruleId) || !ChaserIdentityLive(worldId))return false;
    return !chaserNavigationRejected && chaserNavigationReported && ChaserIdentityLive(chaserNavigationWorld) &&
        chaserNavigationWorld.object==world && chaserNavigationWorld.serial==worldId.serial &&
        chaserGround.Verified() && chaserGround.ValidCount()>=2;
}
bool __fastcall OnChaserCanTransit(void* rule) {
    bool result=TwinKartObjective::ResolveChaserLoadingReady(rule,chaserModeDefinitionEnabled,
        GetCurrentThreadId()==queue.ThreadId(),chaserCanTransit,ChaserLoadingNavigationReady,nullptr);
    __try {
        auto object=static_cast<BYTE*>(rule);using Type=void*(__fastcall*)();
        if(chaserModeDefinitionEnabled && GetCurrentThreadId()==queue.ThreadId() && TwinKartLocalRace::Live(object) &&
           *reinterpret_cast<void**>(object+0x10)==reinterpret_cast<Type>(image+0x2251600)()) {
            static ULONGLONG next=0;auto now=GetTickCount64();
            if(now>=next){next=now+5000;
                RuntimeEvent("native_chaser_loading_counter_snapshot",*reinterpret_cast<int*>(object+0x60),*reinterpret_cast<int*>(object+0x64));
                RuntimeEvent("native_chaser_loading_transition_result",-1,result?1:0);}
        }
    } __except(EXCEPTION_EXECUTE_HANDLER){}
    return result;
}
bool ChaserLoadingCanPrepare(BYTE* world) {
    if(!chaserCanTransit || !ChaserIdentityLive(chaserLoadingTransition) || !ChaserIdentityLive(chaserLoadingWorld) ||
       chaserLoadingWorld.object!=world || !OwnedChaserWorld(world))return false;
    auto rule=static_cast<BYTE*>(chaserLoadingTransition.object);using Type=void*(__fastcall*)();using GetWorld=BYTE*(__fastcall*)(void*);
    if(*reinterpret_cast<void**>(rule+0x10)!=reinterpret_cast<Type>(image+0x2251600)() ||
       reinterpret_cast<GetWorld>((*reinterpret_cast<void***>(rule))[0x160/8])(rule)!=world)return false;
    // The retained native predicate must still report loading complete. The
    // hook records this lease only after that predicate first becomes true.
    if(!chaserCanTransit(rule))return false;
    return ChaserIdentityLive(chaserLoadingTransition) && ChaserIdentityLive(chaserLoadingWorld) && OwnedChaserWorld(world);
}
bool ChaserObjectiveActor(BYTE* world,BYTE* actor,unsigned getter) {
    if(!OwnedChaserWorld(world) || !TwinKartLocalRace::Live(actor) || (*reinterpret_cast<unsigned*>(actor+8)&0x18030) ||
       (actor[0x5c]&0x10) || !TwinKartRace::ObjectiveAssetIsA(image,actor,getter))return false;
    using GetWorld=BYTE*(__fastcall*)(void*);
    return reinterpret_cast<GetWorld>((*reinterpret_cast<void***>(actor))[0x160/8])(actor)==world;
}
bool CollectChaserObjectiveActors(BYTE* world,unsigned getter,TwinKartCamera::ObjectIdentity (&outputActors)[64],unsigned& count) {
    count=0;TwinKartNavigation::ActorArray actors{};TwinKartCamera::ObjectIdentity pending[64]{},worldIdentity{};
    if(!OwnedChaserWorld(world) || !TwinKartLocalRace::CaptureFlagObject(world,worldIdentity))return false;
    using Type=void*(__fastcall*)();auto type=reinterpret_cast<Type>(image+getter)();
    if(!TwinKartLocalRace::Live(static_cast<BYTE*>(type)))return false;
    unsigned used=0;
    __try {
        reinterpret_cast<TwinKartNavigation::CollectActors>(image+0x3f41d30)(world,type,&actors);
        if(actors.count<0 || actors.count>64 || actors.capacity<actors.count || actors.capacity>65536 ||
           (actors.count && !actors.data))return false;
        for(int i=0;i<actors.count;++i) {
            auto actor=static_cast<BYTE*>(actors.data[i]);
            if(!ChaserObjectiveActor(world,actor,getter))continue;
            if(!TwinKartLocalRace::CaptureFlagObject(actor,pending[used]))return false;
            for(unsigned j=0;j<used;++j)if(pending[j].object==actor)return false;
            ++used;
        }
        if(!ChaserIdentityLive(worldIdentity) || !OwnedChaserWorld(world))return false;
        memcpy(outputActors,pending,sizeof(pending));count=used;return true;
    } __finally {
        if(actors.data)reinterpret_cast<TwinKartNavigation::ReleaseActors>(image+0x248b950)(actors.data);
    }
}
std::uint64_t ChaserObjectiveIdentity(const TwinKartCamera::ObjectIdentity& identity) {
    return (static_cast<std::uint64_t>(static_cast<unsigned>(identity.serial))<<32)|
        static_cast<unsigned>(identity.index+1);
}
bool ReadChaserObjectives(BYTE* world,const TwinKartCamera::ObjectIdentity (&pawns)[16],int seats,
                         ULONGLONG now,TwinKartObjective::ChaserObjectiveView& outputView) {
    if(!OwnedChaserWorld(world) || seats<2 || seats>16)return false;
    if(now>=chaserObjectiveRefresh) {
        TwinKartCamera::ObjectIdentity vaults[64]{},zones[64]{};unsigned vaultCount=0,zoneCount=0;
        chaserObjectiveRefresh=now+2000;
        bool valid=CollectChaserObjectiveActors(world,0x2257b70,vaults,vaultCount) &&
            CollectChaserObjectiveActors(world,0x22591c0,zones,zoneCount);
        if(!valid) {
            chaserVaultCount=chaserZoneCount=0;RuntimeEvent("native_chaser_objective_actor_snapshot_rejected",-1,0);return false;
        }
        memcpy(chaserVaultActors,vaults,sizeof(vaults));memcpy(chaserZoneActors,zones,sizeof(zones));
        chaserVaultCount=vaultCount;chaserZoneCount=zoneCount;
        if(!vaultCount || !zoneCount)chaserObjectiveRefresh=now+500;
        RuntimeEvent("native_chaser_original_vault_actor_count",-1,static_cast<int>(vaultCount));
        RuntimeEvent("native_chaser_original_vault_zone_count",-1,static_cast<int>(zoneCount));
    }
    TwinKartObjective::ChaserObjectiveView view;view.vaultCount=chaserVaultCount;view.zoneCount=chaserZoneCount;
    for(unsigned i=0;i<view.vaultCount;++i) {
        const auto& identity=chaserVaultActors[i];auto actor=static_cast<BYTE*>(identity.object);auto& item=view.vaults[i];
        item.identity=ChaserObjectiveIdentity(identity);
        if(!ChaserIdentityLive(identity) || !ChaserObjectiveActor(world,actor,0x2257b70) ||
           !TwinKartLocalRace::FlagPosition(actor,item.position))continue;
        // Exact reflected AChaserVault.HoldPawn at+0x240. An unknown native
        // holder is never treated as an available/free pickup.
        auto holder=*reinterpret_cast<void**>(actor+0x240);
        if(holder) {
            int found=-1;
            for(int slot=0;slot<seats;++slot)if(pawns[slot].object==holder && ChaserIdentityLive(pawns[slot]))found=slot;
            if(found<0)continue;item.holder=found;
        }
        item.ready=TwinKartObjective::ChaserObjectivePoint(item.position);
    }
    for(unsigned i=0;i<view.zoneCount;++i) {
        const auto& identity=chaserZoneActors[i];auto actor=static_cast<BYTE*>(identity.object);auto& zone=view.zones[i];
        zone.identity=ChaserObjectiveIdentity(identity);
        if(!ChaserIdentityLive(identity) || !ChaserObjectiveActor(world,actor,0x22591c0))continue;
        auto box=*reinterpret_cast<BYTE**>(actor+0x220);
        if(!TwinKartLocalRace::Live(box) || !TwinKartRace::ObjectiveAssetIsA(image,box,0x440b310) ||
           *reinterpret_cast<void**>(box+0xa0)!=actor)continue;
        // Use the original collision volume's world center, including its
        // attachment offset; a missing decorative vault plate is not a zone.
        memcpy(&zone.position,box+0x1e0,sizeof(zone.position));
        zone.ready=TwinKartObjective::ChaserObjectivePoint(zone.position);
    }
    if(!OwnedChaserWorld(world) || !TwinKartObjective::ValidChaserObjectives(&view,seats))return false;
    outputView=view;return true;
}
struct ChaserItemNativeSeat {
    TwinKartCamera::ObjectIdentity pawn{},state{},item{},context{},inventory{};
    TwinKartObjective::ChaserRole role=TwinKartObjective::ChaserRole::Unknown;
};
struct ChaserItemNativeContext {
    BYTE* world=nullptr;TwinKartCamera::ObjectIdentity worldIdentity{};
    ChaserItemNativeSeat seats[16]{};int count=0;
};
bool ChaserItemGame(BYTE* world) {
    if(!OwnedChaserWorld(world))return false;
    unsigned long long phase=0;auto game=*reinterpret_cast<unsigned long long*>(image+TwinKartGraphics::ChaserGameNameRva);
    return game && ChaserPhase(world,phase) && phase==game;
}
bool ReadPracticeTicket(const wchar_t* path,char (&id)[32],const char* expectedTrack=nullptr) {
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    LARGE_INTEGER length{};DWORD bytes=0;char ticket[130]{};
    const bool read=GetFileSizeEx(file,&length) && length.QuadPart>=34 && length.QuadPart<sizeof(ticket) &&
        ReadFile(file,ticket,static_cast<DWORD>(length.QuadPart),&bytes,nullptr) && bytes==length.QuadPart;
    CloseHandle(file);
    if(!read || ticket[32]!=':' || memchr(ticket,0,bytes) || (expectedTrack && strcmp(ticket+33,expectedTrack)))return false;
    memcpy(id,ticket,sizeof(id));
    for(char digit:id)if(!((digit>='0' && digit<='9') || (digit>='a' && digit<='f')))return false;
    return true;
}
bool MarkPracticeUnranked(const char* expectedTrack=nullptr,const char* previous=nullptr) {
    char id[32]{},confirmed[32]{};
    if(!ReadPracticeTicket(practiceTicketPath,id,expectedTrack) || (previous && !memcmp(id,previous,sizeof(id))))return false;
    HANDLE file=CreateFileW(practiceTemporaryPath,GENERIC_WRITE,0,nullptr,CREATE_NEW,0,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    DWORD bytes=0;const bool wrote=WriteFile(file,id,sizeof(id),&bytes,nullptr) && bytes==sizeof(id) && FlushFileBuffers(file);
    CloseHandle(file);
    if(wrote && ReadPracticeTicket(practiceTicketPath,confirmed,expectedTrack) && !memcmp(id,confirmed,sizeof(id)) &&
       MoveFileExW(practiceTemporaryPath,practiceActivePath,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return true;
    DeleteFileW(practiceTemporaryPath);return false;
}
bool SuppressPracticeResult(BYTE* callback) {
    if(GetCurrentThreadId()!=queue.ThreadId() || !practiceDrivingWorld.object || !practiceFinish.object)return false;
    __try {
        if(!HideMaterialIdentityLive(practiceFinish) || !HideMaterialIdentityLive(practiceDrivingWorld) ||
           *reinterpret_cast<BYTE**>(callback)!=image+0x5213fb0)return false;
        auto logic=*reinterpret_cast<BYTE**>(callback+0x20);
        if(!TwinKartLocalRace::Live(logic) || *reinterpret_cast<BYTE**>(logic)!=image+0x52cacc0)return false;
        auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
        const int count=*reinterpret_cast<int*>(image+0x69c28c4);
        TwinKartCamera::ObjectIdentity identity{};
        if(!TwinKartCamera::Capture(logic,chunks,count,&identity) || identity.index!=*reinterpret_cast<int*>(callback+0x18) ||
           identity.serial!=*reinterpret_cast<int*>(callback+0x1c))return false;
        auto world=static_cast<BYTE*>(practiceDrivingWorld.object),finish=static_cast<BYTE*>(practiceFinish.object);
        auto state=*reinterpret_cast<BYTE**>(world+0x120),pawn=*reinterpret_cast<BYTE**>(logic+0xf8);
        if(TwinKartLocalRace::World(logic)!=world || !TwinKartLocalRace::Live(state) ||
           *reinterpret_cast<BYTE**>(state)!=image+0x520d6d8 || *reinterpret_cast<BYTE**>(logic+0xd0)!=state ||
           !TwinKartLocalRace::Live(pawn) || *reinterpret_cast<BYTE**>(pawn)!=image+0x520de78 ||
           *reinterpret_cast<BYTE**>(finish+0xa0)!=pawn || *reinterpret_cast<BYTE**>(finish)!=image+0x5117670 ||
           finish[0xb0] || finish[0xb1] || finish[0xb2]!=1)return false;
        if(practiceResultSuppressionLogged.object!=finish || practiceResultSuppressionLogged.serial!=practiceFinish.serial) {
            practiceResultSuppressionLogged=practiceFinish;
            RuntimeEvent("native_timeattack_free_drive_terminal_result_ignored",0,1);
        }
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
void __fastcall PracticeResultInvoke(BYTE* callback) {
    if(!SuppressPracticeResult(callback))reinterpret_cast<void(__fastcall*)(BYTE*)>(image+0x19d9c40)(callback);
}
bool __fastcall PracticeResultInvokeIfLive(BYTE* callback) {
    return SuppressPracticeResult(callback) || reinterpret_cast<bool(__fastcall*)(BYTE*)>(image+0x19da820)(callback);
}
bool InstallPracticeResultBoundary() {
    if(practiceResultBoundaryInstalled)return true;
    // These paired methods alone publish the original Time Attack result and
    // request its terminal phase. The pause actions and ordinary checkpoint
    // delegates use different methods. Keep a suppressed delegate bound.
    auto entries=reinterpret_cast<void* volatile*>(image+0x5213ff8);
    void* direct=image+0x19d9c40,*guarded=image+0x19da820;
    if(entries[0]!=direct || entries[1]!=guarded)return false;
    auto page=reinterpret_cast<void*>(image+0x5213ff8);
    DWORD protection=0,ignored=0;
    if(!VirtualProtect(page,2*sizeof(void*),PAGE_READWRITE,&protection))return false;
    auto hookDirect=reinterpret_cast<void*>(PracticeResultInvoke);
    auto hookGuarded=reinterpret_cast<void*>(PracticeResultInvokeIfLive);
    bool first=InterlockedCompareExchangePointer(entries,hookDirect,direct)==direct;
    bool second=first && InterlockedCompareExchangePointer(entries+1,hookGuarded,guarded)==guarded;
    if(first && !second)InterlockedCompareExchangePointer(entries,direct,hookDirect);
    if(!VirtualProtect(page,2*sizeof(void*),protection,&ignored)) {
        if(second) {
            InterlockedCompareExchangePointer(entries+1,guarded,hookGuarded);
            InterlockedCompareExchangePointer(entries,direct,hookDirect);
        }
        VirtualProtect(page,2*sizeof(void*),protection,&ignored);
        return false;
    }
    practiceResultBoundaryInstalled=second;
    return second;
}
void UpdateTimeAttackPractice(BYTE* pawn,BYTE* world,bool gamePhase,bool gameplayInput) {
    if(!practiceTicketPath[0])return;
    // The pawn is an actor. World() is the native component accessor;
    // use the Time Attack world already verified from its movement component.
    if(practiceDrivingWorld.object && (practiceDrivingWorld.object!=world ||
       !HideMaterialIdentityLive(practiceDrivingWorld))) {
        practiceDrivingWorld={};practiceLobbyLease=false;
    }
    BYTE* finish=gamePhase?TwinKartLocalRace::FlagFind(pawn,0x2402120):nullptr;
    if(finish && *reinterpret_cast<BYTE**>(finish)!=image+0x5117670)finish=nullptr;
    if(practiceFinish.object && (!HideMaterialIdentityLive(practiceFinish) || finish!=practiceFinish.object)) {
        if(HideMaterialIdentityLive(practiceFinish)) {
            auto previous=static_cast<BYTE*>(practiceFinish.object);
            if(*reinterpret_cast<BYTE**>(previous)==image+0x5117670 && previous[0xb0]==0 && previous[0xb1]==0)
                previous[0xb0]=previous[0xb1]=1;
        }
        practiceFinish={};
        practiceRequested=false;
    }
    if(practiceFinish.object && (finish[0xb0]!=0 || finish[0xb1]!=0))practiceFinish={};
    if(!finish || (!practiceFinish.object && (finish[0xb0]!=1 || finish[0xb1]!=1 || finish[0xb2]!=1))) {
        practiceRequested=false;InterlockedExchange(&practiceHud,0);return;
    }
    const auto now=GetTickCount64();
    static ULONGLONG retryAt=0;
    const bool autoStart=practiceLobbyUntil && now<practiceLobbyUntil && now>=retryAt;
    const bool autoRestart=practiceLobbyLease && practiceDrivingWorld.object && !practiceFinish.object && now>=retryAt;
    if(practiceRequested || autoStart || autoRestart) {
        if(autoStart || autoRestart)retryAt=now+125;
        practiceRequested=false;
        TwinKartCamera::ObjectIdentity identity{};
        const bool fromLobby=autoStart || autoRestart;
        TwinKartCamera::ObjectIdentity worldId{};
        if((gameplayInput || fromLobby) && !practiceFinish.object && TwinKartLocalRace::CaptureFlagObject(finish,identity) &&
           TwinKartLocalRace::CaptureFlagObject(world,worldId) && InstallPracticeResultBoundary() &&
           MarkPracticeUnranked(fromLobby?practiceLobbyTrack:nullptr,
               autoStart?practicePriorTicket:nullptr)) {
            // The original TimeAttack constructor enables all three gates.
            // 0x19a75e2/0x198bd5d gate finish-trigger submission and
            // 0x1640e9a gates the lap-limit finish. Keep checkpoints enabled.
            finish[0xb0]=finish[0xb1]=0;practiceFinish=identity;
            practiceDrivingWorld=worldId;practiceLobbyLease=fromLobby;
            practiceLobbyUntil=0;
            RuntimeEvent(autoRestart?"native_timeattack_free_drive_restart_enabled":
                "native_timeattack_free_drive_enabled",0,1);
        }else if(!fromLobby)RuntimeEvent("native_timeattack_free_drive_request_rejected",0,0);
    }
    InterlockedExchange(&practiceHud,practiceFinish.object?2:gameplayInput?1:0);
    InterlockedExchange64(&practiceHeartbeat,GetTickCount64());
}
void RecoverLocalHumanSteering(BYTE* movement,BYTE* controller,bool allowed,const char* event) {
    static TwinKartCamera::ObjectIdentity owner{};
    static float supplied=0.f;
    static bool logged=false;
    if(owner.object!=movement || !HideMaterialIdentityLive(owner)) {
        if(!TwinKartLocalRace::CaptureFlagObject(movement,owner))return;
        supplied=0.f;logged=false;
    }
    // Both original keyboard steering axes converge on this signed setter.
    // Leave a nonzero native axis alone and release only our own input.
    static const BYTE prefix[]{0x40,0x53,0x48,0x83,0xec,0x30,0x80,0xb9,0xfd,0x0e,0,0,0};
    if(memcmp(image+0x15d3fd0,prefix,sizeof(prefix)))return;
    auto steer=reinterpret_cast<void(__fastcall*)(void*,float)>(image+0x15d3fd0);
    const bool left=allowed && ((GetAsyncKeyState('A')&0x8000) || HideHumanKey(controller,2));
    const bool right=allowed && ((GetAsyncKeyState('D')&0x8000) || HideHumanKey(controller,3));
    const float current=*reinterpret_cast<const float*>(movement+0x1e8);
    const float desired=left==right?0.f:left?-1.f:1.f;
    const bool stillOurs=supplied!=0.f && current==supplied;
    if(desired!=0.f && (current==0.f || stillOurs)) {
        if(current!=desired)steer(movement,desired);
        supplied=*reinterpret_cast<const float*>(movement+0x1e8)==desired?desired:0.f;
        if(supplied && !logged) {
            logged=true;RuntimeEvent(event,0,static_cast<int>(desired));
        }
    }else {
        if(!desired && stillOurs)steer(movement,0.f);
        supplied=0.f;
    }
}
void RecoverTimeAttackHumanForward(BYTE* movement,TwinKartLocalRace::RawForward apply) {
    static TwinKartCamera::ObjectIdentity inputOwner{};
    static float supplied=0.f;
    static bool forwardLogged=false,reverseLogged=false;
    auto pawn=*reinterpret_cast<BYTE**>(movement+0xa0);
    if(!TwinKartLocalRace::Live(pawn) || *reinterpret_cast<BYTE**>(pawn)!=image+0x520de78)return;
    auto world=TwinKartLocalRace::World(movement);
    if(!TwinKartLocalRace::Live(world) || *reinterpret_cast<void**>(world+0x38))return;
    auto mode=*reinterpret_cast<BYTE**>(world+0x118),gameState=*reinterpret_cast<BYTE**>(world+0x120);
    if(!TwinKartLocalRace::Live(mode) || *reinterpret_cast<BYTE**>(mode)!=image+0x520ce90 ||
       !TwinKartLocalRace::Live(gameState) || *reinterpret_cast<BYTE**>(gameState)!=image+0x520d6d8)return;
    auto controller=*reinterpret_cast<BYTE**>(pawn+0x258),body=*reinterpret_cast<BYTE**>(movement+0xf38);
    auto state=TwinKartLocalRace::FlagFind(pawn,0x23afeb0);
    if(!TwinKartLocalRace::Live(controller) || *reinterpret_cast<BYTE**>(controller)!=image+0x520fb18 ||
       *reinterpret_cast<void**>(controller+0x250)!=pawn || !TwinKartLocalRace::Live(*reinterpret_cast<BYTE**>(controller+0x298)) ||
       !TwinKartLocalRace::Live(body) || *reinterpret_cast<BYTE**>(body)!=image+0x511e978 ||
       *reinterpret_cast<BYTE**>(body+0xa0)!=pawn || !TwinKartLocalRace::Live(state) ||
       *reinterpret_cast<BYTE**>(state+0xa0)!=pawn || state[0x321] || *reinterpret_cast<int*>(state+0x168)!=0)return;
    auto phase=TwinKartLocalRace::FlagFind(gameState,0x2427230);
    auto current=phase && *reinterpret_cast<BYTE**>(phase)==image+0x5689ef0 ?
        *reinterpret_cast<BYTE**>(phase+TwinKartGraphics::CurrentPhaseOffset):nullptr;
    const auto ignored=(*reinterpret_cast<BYTE***>(controller))[0x740/8];
    // Chaser initializes its own Game FName only when its rules run.
    // Match the exact observed TimeAttack phase name and cache that FName
    // without depending on the state of another original game mode.
    static unsigned long long observedPhase=0;
    static bool observedDriving=false;
    const auto phaseName=TwinKartLocalRace::Live(current) && *reinterpret_cast<BYTE**>(current)==image+0x51e8028 ?
        *reinterpret_cast<unsigned long long*>(current+TwinKartGraphics::PhaseNameOffset):0;
    if(phaseName!=observedPhase) {
        wchar_t label[32]{};
        observedPhase=phaseName;
        observedDriving=phaseName && ModeNameText(phaseName,label,ARRAYSIZE(label)) && !wcscmp(label,L"Game");
    }
    const bool allowed=phaseName && observedDriving && state[0x338]==1 && !body[0x359] && HideHumanGameplayInput(world) &&
        ignored==image+0x3e9a1f0 && !reinterpret_cast<bool(__fastcall*)(void*)>(ignored)(controller) &&
        !(GetAsyncKeyState(VK_SPACE)&0x8000) && !(GetAsyncKeyState(VK_RCONTROL)&0x8000);
    UpdateTimeAttackPractice(pawn,world,phaseName && observedDriving,allowed);
    if(!apply || !TwinKartLocalRace::wasdForwardEnabled)return;
    RecoverLocalHumanSteering(movement,controller,allowed,"native_timeattack_human_wasd_steering_restored");
    const bool reverseTap=practiceFinish.object && practiceReverseEdgeFinish.object==practiceFinish.object &&
        practiceReverseEdgeFinish.serial==practiceFinish.serial && GetTickCount64()<practiceReverseEdgeUntil;
    const bool reverse=allowed && ((GetAsyncKeyState('S')&0x8000) || HideHumanKey(controller,1) || reverseTap);
    const bool forward=allowed && ((GetAsyncKeyState('W')&0x8000) || HideHumanKey(controller,0));
    if(inputOwner.object!=movement || !HideMaterialIdentityLive(inputOwner)) {
        if(!TwinKartLocalRace::CaptureFlagObject(movement,inputOwner))return;
        supplied=0.f;forwardLogged=reverseLogged=false;
    }
    // Time Attack is outside the custom-room lease. Use its native
    // MoveForward callback for missing W. Only unranked free driving may
    // supply proportional S to avoid the original -1 toggle from rest.
    const float desired=forward==reverse?0.f:forward?1.f:practiceFinish.object?-.85f:0.f;
    const float raw=*reinterpret_cast<const float*>(movement+0x18c);
    if(!std::isfinite(raw)){supplied=0.f;return;}
    const bool ours=supplied && std::fabs(raw-supplied)<.001f;
    if(desired && (std::fabs(raw)<.001f || ours)) {
        if(!ours || desired!=supplied)apply(movement,desired);
        supplied=desired;
        if(desired>0 && !forwardLogged){forwardLogged=true;RuntimeEvent("native_timeattack_human_w_forward_restored",0,1);}
        if(desired<0 && !reverseLogged &&
           std::fabs(*reinterpret_cast<const float*>(movement+0x18c)-desired)<.001f) {
            reverseLogged=true;RuntimeEvent("native_timeattack_practice_s_reverse_restored",0,1);
        }
    }else {
        if(!desired && ours)apply(movement,0.f);
        supplied=0.f;
    }
}
void RecoverArenaHumanKartTick(BYTE* movement,TwinKartLocalRace::RawForward apply) {
    const bool flag=TwinKartRace::FlagMode(TwinKartLocalRace::armedMode);
    const bool factory=TwinKartLocalRace::armedMode==100 && TwinKartLocalRace::factoryCompatEnabled;
    if(GetCurrentThreadId()!=queue.ThreadId())return;
    __try {
        if(!TwinKartLocalRace::Live(movement) || *reinterpret_cast<BYTE**>(movement)!=image+0x5118590)return;
        auto pawn=*reinterpret_cast<BYTE**>(movement+0xa0);
        if(TwinKartLocalRace::Live(pawn) && *reinterpret_cast<BYTE**>(pawn)==image+0x520de78) {
            RecoverTimeAttackHumanForward(movement,apply);return;
        }
        if(!apply || (!flag && !factory && TwinKartLocalRace::armedMode!=108))return;
        using Type=void*(__fastcall*)();
        if(!TwinKartLocalRace::Live(pawn) || *reinterpret_cast<void**>(pawn+0x10)!=
           reinterpret_cast<Type>(image+(factory?0x2431240:flag?0x22ae2e0:0x2250e50))())return;
        auto world=TwinKartLocalRace::World(movement);
        if(factory?!TwinKartLocalRace::InFactoryComponentWorld(movement):
           flag?(!TwinKartLocalRace::StandaloneFlagWorld(world) || !TwinKartLocalRace::InRaceWorld(movement)):
            !OwnedChaserWorld(world))return;
        auto controller=*reinterpret_cast<BYTE**>(pawn+0x258);
        auto state=TwinKartLocalRace::FlagFind(pawn,0x23afeb0);
        auto body=*reinterpret_cast<BYTE**>(movement+0xf38);
        if(!TwinKartLocalRace::Live(controller) || *reinterpret_cast<BYTE**>(controller)!=image+0x50fa2c0 ||
           !TwinKartLocalRace::Live(*reinterpret_cast<BYTE**>(controller+0x298)) || *reinterpret_cast<void**>(controller+0x250)!=pawn ||
           !TwinKartLocalRace::OwnedComponent(state,pawn,reinterpret_cast<Type>(image+0x23afeb0)()) ||
           state[0x321] || *reinterpret_cast<int*>(state+0x168)!=0 || !TwinKartLocalRace::Live(body) ||
           *reinterpret_cast<BYTE**>(body+0xa0)!=pawn)return;
        auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
        const int count=*reinterpret_cast<int*>(image+0x69c28c4);
        if(chaserHumanForwardOwner.object!=movement || !TwinKartCamera::Live(chaserHumanForwardOwner,chunks,count)) {
            if(!TwinKartLocalRace::CaptureFlagObject(movement,chaserHumanForwardOwner))return;
            chaserHumanForwardFallback=chaserHumanForwardLogged=false;
        }
        bool active=false;
        if(flag || factory) {
            auto phase=TwinKartLocalRace::FlagFind(*reinterpret_cast<BYTE**>(world+0x120),factory?0x2431330:0x22ac6a0);
            auto current=phase?*reinterpret_cast<BYTE**>(phase+0x110):nullptr;
            active=factory?(TwinKartLocalRace::Live(current) &&
                *reinterpret_cast<unsigned long long*>(current+TwinKartGraphics::PhaseNameOffset)==
                *reinterpret_cast<unsigned long long*>(image+TwinKartGraphics::BeginRaceNameRva)):
                TwinKartLocalRace::FlagDrivingPhase(current);
        }else {
            unsigned long long phase=0;
            const auto game=*reinterpret_cast<unsigned long long*>(image+TwinKartGraphics::ChaserGameNameRva);
            const auto role=TwinKartObjective::NativeChaserRole(TwinKartLocalRace::FlagFind(pawn,0x2256c10)!=nullptr,
                TwinKartLocalRace::FlagFind(pawn,0x22570a0)!=nullptr);
            active=game && ChaserPhase(world,phase) && phase==game && role!=TwinKartObjective::ChaserRole::Unknown;
        }
        auto ignored=(*reinterpret_cast<BYTE***>(controller))[0x740/8];
        const bool driving=state[0x338]==1 && active && !body[0x359] && HideHumanGameplayInput(world) &&
            ignored==image+0x3e9a1f0 && !reinterpret_cast<bool(__fastcall*)(void*)>(ignored)(controller);
        if(factory && TwinKartLocalRace::wasdForwardEnabled)
            RecoverLocalHumanSteering(movement,controller,driving,"native_factory_human_wasd_steering_restored");
        const bool allowed=driving && !(GetAsyncKeyState('S')&0x8000);
        const auto now=GetTickCount64();
        const bool shortTap=!flag && !factory && chaserForwardEdgePawn.object==pawn &&
            now<chaserForwardEdgeUntil && ChaserIdentityLive(chaserForwardEdgePawn);
        const bool held=allowed && !HideHumanKey(controller,1) &&
            ((GetAsyncKeyState('W')&0x8000) || HideHumanKey(controller,0) || shortTap);
        float raw=*reinterpret_cast<float*>(movement+0x18c);
        // Existing nonzero native axes have priority. Send a release only
        // when this adapter previously supplied the exact positive value.
        switch(TwinKartLocalRace::RecoverForwardDecision(allowed,held,chaserHumanForwardFallback,raw)) {
        case TwinKartLocalRace::ForwardRecovery::Press:
            apply(movement,1.f);chaserHumanForwardFallback=true;
            if(!chaserHumanForwardLogged){chaserHumanForwardLogged=true;RuntimeEvent(factory?"native_factory_human_forward_restored":
                flag?"native_flag_human_forward_restored":"native_chaser_human_forward_restored",0,1);}
            break;
        case TwinKartLocalRace::ForwardRecovery::Release:apply(movement,0.f);chaserHumanForwardFallback=false;break;
        default:if(!held || raw!=1.f)chaserHumanForwardFallback=false;break;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {chaserHumanForwardOwner={};chaserHumanForwardFallback=false;}
}
bool ChaserHasNativeItemController(BYTE* pawn) {
    using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
    auto find=reinterpret_cast<Find>((*reinterpret_cast<void***>(pawn))[0x620/8]);
    for(unsigned getter:{0x220fed0u,0x22174e0u}) {
        auto type=reinterpret_cast<Type>(image+getter)();
        auto controller=find(pawn,type);if(!controller)continue;
        if(getter==0x220fed0u && ChaserOwnsItemPolicy(controller))continue;
        // Preserve unknown subclasses and the team controller. For the exact
        // generic class, native Tick can submit via its scheduled-use queue or
        // pending gain work. An empty/unarmed controller cannot provide intent
        // just because the component was registered on a speed-AI pawn.
        TwinKartCamera::ObjectIdentity identity{};
        auto actualType=*reinterpret_cast<void**>(controller+0x10);
        bool speed=getter==0x220fed0u && actualType==reinterpret_cast<Type>(image+0x2218050)();
        if(getter!=0x220fed0u || (actualType!=type && !speed) || !TwinKartLocalRace::OwnedComponent(controller,pawn,actualType) ||
           !TwinKartLocalRace::CaptureFlagObject(controller,identity))return true;
        auto table=*reinterpret_cast<BYTE***>(controller);
        bool known=TwinKartObjective::ChaserKnownItemControllerAbi(speed?TwinKartObjective::ChaserItemControllerKind::Speed:
            TwinKartObjective::ChaserItemControllerKind::Generic,reinterpret_cast<std::uintptr_t>(image),
            reinterpret_cast<std::uintptr_t>(table[0x350/8]),reinterpret_cast<std::uintptr_t>(table[0x408/8]),
            reinterpret_cast<std::uintptr_t>(table[0x418/8]));
        if(!known)return true;
        int count=*reinterpret_cast<int*>(controller+0xb8),capacity=*reinterpret_cast<int*>(controller+0xbc);
        unsigned gain=controller[0x100];auto cached=*reinterpret_cast<BYTE**>(controller+0x110);
        auto actual=TwinKartLocalRace::FlagFind(pawn,0x22d7240);
        if(!ChaserIdentityLive(identity) || (cached && cached!=actual) ||
           TwinKartObjective::ChaserNativeControllerOwnsIntent(known,count,capacity,gain,cached!=nullptr))return true;
    }
    return false;
}
bool ChaserItemDiagnostic(int slot,int stage);
bool LiveChaserItemSeat(const ChaserItemNativeContext& view,int slot,const ChaserItemNativeSeat& seat,bool requireIntent=true) {
    if(slot<1 || slot>=view.count || !ChaserIdentityLive(view.worldIdentity) || !ChaserItemGame(view.world) ||
       !ChaserIdentityLive(seat.pawn) || !ChaserIdentityLive(seat.state) || !ChaserIdentityLive(seat.item) ||
       !ChaserIdentityLive(seat.context) || !ChaserIdentityLive(seat.inventory))return ChaserItemDiagnostic(slot,40);
    auto pawn=static_cast<BYTE*>(seat.pawn.object),state=static_cast<BYTE*>(seat.state.object);
    auto item=static_cast<BYTE*>(seat.item.object),context=static_cast<BYTE*>(seat.context.object);
    auto inventory=static_cast<BYTE*>(seat.inventory.object);using Type=void*(__fastcall*)();
    auto configured=*reinterpret_cast<void**>(item+0x318);
    auto chaserContext=reinterpret_cast<Type>(image+0x2250d20)();
    bool configuredContext=configured==chaserContext || (configured==reinterpret_cast<Type>(image+0x22d8460)() &&
        TwinKartRace::ObjectiveAssetIsA(image,pawn,0x2413310));
    if((requireIntent && ChaserHasNativeItemController(pawn)) || !TwinKartRace::ObjectiveAssetIsA(image,pawn,0x23237f0))return ChaserItemDiagnostic(slot,41);
    if(!TwinKartLocalRace::OwnedComponent(state,pawn,reinterpret_cast<Type>(image+0x23afeb0)()))return ChaserItemDiagnostic(slot,42);
    if(!TwinKartLocalRace::OwnedComponent(item,pawn,reinterpret_cast<Type>(image+0x22d7240)()))return ChaserItemDiagnostic(slot,43);
    if(!TwinKartLocalRace::OwnedComponent(context,pawn,reinterpret_cast<Type>(image+0x2250d20)()))return ChaserItemDiagnostic(slot,44);
    if(!TwinKartLocalRace::OwnedComponent(inventory,pawn,reinterpret_cast<Type>(image+0x22ef370)()))return ChaserItemDiagnostic(slot,45);
    if(*reinterpret_cast<void**>(inventory+0x20)!=item || *reinterpret_cast<void**>(item+0x320)!=context ||
       *reinterpret_cast<void**>(item+0x548)!=inventory)return ChaserItemDiagnostic(slot,46);
    if(!configuredContext)return ChaserItemDiagnostic(slot,47);
    if(*reinterpret_cast<int*>(state+0x168)!=slot || !state[0x321] || state[0x338]!=1)return ChaserItemDiagnostic(slot,48);
    if(TwinKartLocalRace::FlagFind(pawn,0x23afeb0)!=state || TwinKartLocalRace::FlagFind(pawn,0x22d7240)!=item)return ChaserItemDiagnostic(slot,49);
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);int count=*reinterpret_cast<int*>(image+0x69c28c4);
    for(auto component:{state,item,context,inventory})
        if(!chaserRoleWorld.Matches(TwinKartLocalRace::World(component),chunks,count))return ChaserItemDiagnostic(slot,50);
    auto role=TwinKartObjective::NativeChaserRole(TwinKartLocalRace::FlagFind(pawn,0x2256c10)!=nullptr,
        TwinKartLocalRace::FlagFind(pawn,0x22570a0)!=nullptr);
    return role==seat.role && role!=TwinKartObjective::ChaserRole::Unknown?true:ChaserItemDiagnostic(slot,51);
}
bool ChaserItemDiagnostic(int slot,int stage) {
    static ULONGLONG next[16]{};auto now=GetTickCount64();
    if(slot>0 && slot<16 && now>=next[slot]){next[slot]=now+5000;RuntimeEvent("native_chaser_item_read_stage",slot,stage);}
    return false;
}
bool ReadChaserItemSeat(int slot,TwinKartObjective::ChaserItemObservation& observation,void* raw) {
    __try {
        auto& view=*static_cast<ChaserItemNativeContext*>(raw);
        if(slot<1 || slot>=view.count || !ChaserIdentityLive(view.seats[slot].pawn) || !ChaserIdentityLive(view.seats[slot].state) ||
           !ChaserIdentityLive(view.worldIdentity) || !ChaserItemGame(view.world))return false;
        auto seat=view.seats[slot];auto pawn=static_cast<BYTE*>(seat.pawn.object);
        // A native item controller already supplies intent when present. Do
        // not duplicate its gain/hit delegates or compete with its tick.
        if(ChaserHasNativeItemController(pawn))return ChaserItemDiagnostic(slot,1);
        auto item=TwinKartLocalRace::FlagFind(pawn,0x22d7240);
        if(!item || !TwinKartLocalRace::CaptureFlagObject(item,seat.item))return ChaserItemDiagnostic(slot,2);
        auto context=*reinterpret_cast<BYTE**>(item+0x320),inventory=*reinterpret_cast<BYTE**>(item+0x548);
        if(!TwinKartLocalRace::CaptureFlagObject(context,seat.context) || !TwinKartLocalRace::CaptureFlagObject(inventory,seat.inventory))return ChaserItemDiagnostic(slot,3);
        seat.role=TwinKartObjective::NativeChaserRole(TwinKartLocalRace::FlagFind(pawn,0x2256c10)!=nullptr,
            TwinKartLocalRace::FlagFind(pawn,0x22570a0)!=nullptr);
        if(!LiveChaserItemSeat(view,slot,seat))return ChaserItemDiagnostic(slot,4);
        if((*reinterpret_cast<BYTE***>(context))[0x428/8]!=image+0x18a6f40)return ChaserItemDiagnostic(slot,5);
        // Original Chaser context determines actual item-playing state. The
        // native first-slot predicate retains cooldown/blocked/aim semantics.
        using Playing=bool(__fastcall*)(void*);using Usable=bool(__fastcall*)(void*,int);
        if(!reinterpret_cast<Playing>(image+0x18a6f40)(context) || !LiveChaserItemSeat(view,slot,seat))return ChaserItemDiagnostic(slot,6);
        auto now=GetTickCount64();
        bool first=now>=chaserItemNextAllowed[slot][0] && reinterpret_cast<Usable>(image+0x17bf2b0)(inventory,0);
        if(!LiveChaserItemSeat(view,slot,seat))return false;
        bool second=now>=chaserItemNextAllowed[slot][1] && reinterpret_cast<Usable>(image+0x17bf2b0)(inventory,1);
        int selected=TwinKartObjective::ChooseChaserItemSlot(first,second,chaserPreferredItemSlot[slot]);
        bool usable=selected>=0;
        ChaserItemDiagnostic(slot,usable?8:7);
        if(!LiveChaserItemSeat(view,slot,seat))return false;
        observation.lease={ChaserObjectiveIdentity(seat.pawn),ChaserObjectiveIdentity(seat.state),ChaserObjectiveIdentity(seat.item),
            ChaserObjectiveIdentity(seat.context),ChaserObjectiveIdentity(seat.inventory)};
        observation.role=seat.role;observation.ready=true;observation.usable=usable;
        observation.inventorySlot=selected>=0?static_cast<unsigned>(selected):0;
        view.seats[slot]=seat;return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool ReadChaserStack(BYTE* inventory,unsigned inventorySlot,TwinKartCamera::ObjectIdentity& identity,int& count) {
    __try {
    using Get=BYTE*(__fastcall*)(void*,int);
    if(inventorySlot>1 || !TwinKartLocalRace::InChaserComponentWorld(inventory))return false;
    auto stack=reinterpret_cast<Get>(image+0x17b95f0)(inventory,static_cast<int>(inventorySlot));
    if(!TwinKartLocalRace::CaptureFlagObject(stack,identity))return false;
    auto type=*reinterpret_cast<BYTE**>(stack+0x10);wchar_t name[96]{};
    if(!TwinKartLocalRace::Live(type) || !ModeNameText(*reinterpret_cast<unsigned long long*>(type+0x18),name,ARRAYSIZE(name)) ||
       wcscmp(name,L"ItemStackComponent"))return false;
    // The original IsItemUsable at 17bf2b0 checks this Int32 count and native
    // cooldown at +e8. Observe only; never decrement inventory ourselves.
    int value=*reinterpret_cast<int*>(stack+0xc8);
    if(value<0 || value>10000 || !ChaserIdentityLive(identity))return false;
    count=value;return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool ReadChaserItemStack(const ChaserItemNativeContext& view,int slot,unsigned inventorySlot,TwinKartCamera::ObjectIdentity& identity,int& count) {
    if(slot<1 || slot>=view.count || !LiveChaserItemSeat(view,slot,view.seats[slot],false))return false;
    return ReadChaserStack(static_cast<BYTE*>(view.seats[slot].inventory.object),inventorySlot,identity,count) &&
        LiveChaserItemSeat(view,slot,view.seats[slot],false);
}
int RecordChaserItemFault(EXCEPTION_POINTERS* exception,int slot) {
    if(exception && exception->ExceptionRecord) {
        const auto* record=exception->ExceptionRecord;
        const auto at=reinterpret_cast<std::uintptr_t>(record->ExceptionAddress),base=reinterpret_cast<std::uintptr_t>(image);
        if(at>=base && at-base<0x4ed0000)RuntimeEvent("native_chaser_ai_item_fault_rva",slot,static_cast<int>(at-base));
        RuntimeEvent("native_chaser_ai_item_fault_code",slot,static_cast<int>(record->ExceptionCode));
    }
    return EXCEPTION_EXECUTE_HANDLER;
}
bool SubmitChaserItem(int slot,const TwinKartObjective::ChaserItemObservation& expected,void* raw) {
    __try {
        auto& view=*static_cast<ChaserItemNativeContext*>(raw);TwinKartObjective::ChaserItemObservation current;
        if(!ReadChaserItemSeat(slot,current,raw) || !current.usable || !TwinKartObjective::SameChaserItemLease(expected,current))return false;
        using Paused=bool(__fastcall*)(void*);
        if(reinterpret_cast<Paused>(image+0x4011c70)(view.world) || !LiveChaserItemSeat(view,slot,view.seats[slot]))return false;
        TwinKartCamera::ObjectIdentity before{},after{};int beforeCount=0,afterCount=0;
        bool observedBefore=ReadChaserItemStack(view,slot,current.inventorySlot,before,beforeCount);
        // Original API owns the item table, aim/timers, targeting, native use
        // permission, casting and consumption. Return means only call returned.
        // Unlike AI_UseItem (hardwired first slot), the original slot input
        // supports Chaser's trap/scanner and separate booster inventory. Its
        // native permission, aiming, casting and server callbacks stay intact.
        auto previousItem=chaserSubmittingItem;int previousSlot=chaserSubmittingSlot;
        chaserSubmittingItem=view.seats[slot].item.object;chaserSubmittingSlot=slot;
        __try {reinterpret_cast<void(__fastcall*)(void*,int)>(image+0x1555690)(chaserSubmittingItem,static_cast<int>(current.inventorySlot));}
        __finally {chaserSubmittingItem=previousItem;chaserSubmittingSlot=previousSlot;}
        chaserPreferredItemSlot[slot]=1-current.inventorySlot;
        // Native Chaser replenishes its fixed inventory during casting. A
        // nonempty slot is therefore not a new pickup or permission to fire
        // every polling interval. Pace only our intent; native cooldown and
        // actual item execution remain untouched.
        chaserItemNextAllowed[slot][current.inventorySlot]=GetTickCount64()+3500+static_cast<ULONGLONG>(slot)*263+current.inventorySlot*1000;
        if(observedBefore && ReadChaserItemStack(view,slot,current.inventorySlot,after,afterCount) && before.object==after.object &&
           before.index==after.index && before.serial==after.serial && beforeCount>afterCount)
            RuntimeEvent("native_chaser_ai_item_synchronous_consumption",slot,beforeCount-afterCount);
        RuntimeEvent("native_chaser_ai_item_slot_request_returned",slot,static_cast<int>(current.inventorySlot));return true;
    } __except(RecordChaserItemFault(GetExceptionInformation(),slot)){RuntimeEvent("native_chaser_ai_item_request_rejected",slot,0);return false;}
}
int __fastcall ObserveChaserSlotAim(void* item,void* actor,int inventorySlot) {
    auto result=reinterpret_cast<int(__fastcall*)(void*,void*,int)>(image+0x1547f40)(item,actor,inventorySlot);
    if(item==chaserSubmittingItem && chaserSubmittingSlot>0) {
        RuntimeEvent("native_chaser_slot_aim_returned",chaserSubmittingSlot,result);
        RuntimeEvent("native_chaser_slot_aim_inventory",chaserSubmittingSlot,inventorySlot);
    }
    return result;
}
bool IsOwnedChaserBooster(void* raw);
void __fastcall ObserveChaserCasting(void* raw,int sequence,unsigned char type);
TwinKartNative::OwnedCodePatch chaserBoosterSequencePatch;
BYTE* ChaserBoosterHead(BYTE* inventory,int sequence) {
    __try {
        TwinKartCamera::ObjectIdentity stack{},actorLease{};int uses=0;
        if(sequence<0 || !ReadChaserStack(inventory,1,stack,uses) || !uses)return nullptr;
        const auto data=*reinterpret_cast<BYTE***>(static_cast<BYTE*>(stack.object)+0xc0);
        if(!data)return nullptr;
        auto actor=data[0];
        // The original stack-head getter also requires a live actor and
        // valid slot/sequence. Do not search sparse chunks from other slots.
        return TwinKartLocalRace::CaptureFlagObject(actor,actorLease) && IsOwnedChaserBooster(actor) &&
            *reinterpret_cast<int*>(actor+0x5c0)==sequence && *reinterpret_cast<int*>(actor+0x5c4)==1 &&
            ChaserIdentityLive(stack) && ChaserIdentityLive(actorLease)?actor:nullptr;
    } __except(EXCEPTION_EXECUTE_HANDLER){return nullptr;}
}
int ChaserBoosterInputSeat(BYTE* item) {
    __try {
        if(!TwinKartLocalRace::InChaserComponentWorld(item) || !ChaserItemGame(TwinKartLocalRace::World(item)))return -1;
        if(item==chaserSubmittingItem && chaserSubmittingSlot>0)return chaserSubmittingSlot;
        auto pawn=*reinterpret_cast<BYTE**>(item+0xa0),state=TwinKartLocalRace::FlagFind(pawn,0x23afeb0);
        auto controller=TwinKartLocalRace::Live(pawn)?*reinterpret_cast<BYTE**>(pawn+0x258):nullptr;
        if(!TwinKartLocalRace::InChaserItemWorld(pawn) || TwinKartLocalRace::FlagFind(pawn,0x22d7240)!=item ||
           !TwinKartLocalRace::Live(state) || *reinterpret_cast<BYTE**>(state+0xa0)!=pawn ||
           *reinterpret_cast<int*>(state+0x168)!=0 || state[0x321] || state[0x338]!=1 ||
           !TwinKartLocalRace::Live(controller) || *reinterpret_cast<BYTE**>(controller+0x250)!=pawn ||
           !TwinKartObjective::HideControllerHasPlayer(controller))return -1;
        return 0;
    } __except(EXCEPTION_EXECUTE_HANDLER){return -1;}
}
struct ChaserSecondSlotPress {
    TwinKartCamera::ObjectIdentity pawn{},item{};
    ULONGLONG expires=0;
};
ChaserSecondSlotPress chaserSecondSlotPress{};
bool chaserSecondKeyConsumed=false;
void ChaserForwardKeyEdge(const MSG& message) {
    if((message.lParam&(1LL<<30)) || GetCurrentThreadId()!=queue.ThreadId() ||
       message.hwnd!=gameWindow || GetForegroundWindow()!=gameWindow ||
       InterlockedCompareExchange(&exitPending,0,0) || (GetAsyncKeyState(VK_MENU)&0x8000))return;
    __try {
        const auto pawn=chaserObservedRolePawns[0];
        if(!ChaserIdentityLive(pawn))return;
        auto item=TwinKartLocalRace::FlagFind(static_cast<BYTE*>(pawn.object),0x22d7240);
        if(ChaserBoosterInputSeat(item)!=0 || !HideHumanGameplayInput(TwinKartLocalRace::World(item)))return;
        // Input can go down and up between the original movement ticks.
        // Preserve a short physical tap; ordinary held/released W remains
        // under the original input and native movement state checks.
        chaserForwardEdgePawn=pawn;chaserForwardEdgeUntil=GetTickCount64()+100;
    } __except(EXCEPTION_EXECUTE_HANDLER){chaserForwardEdgePawn={};chaserForwardEdgeUntil=0;}
}
bool ChaserSecondSlotKey(const MSG& message) {
    if(message.message==WM_KEYUP) {
        const bool consumed=chaserSecondKeyConsumed;chaserSecondKeyConsumed=false;return consumed;
    }
    if(message.lParam&(1LL<<30))return chaserSecondKeyConsumed;
    chaserSecondKeyConsumed=false;chaserSecondSlotPress={};
    if(GetCurrentThreadId()!=queue.ThreadId() || message.hwnd!=gameWindow || GetForegroundWindow()!=gameWindow ||
       InterlockedCompareExchange(&exitPending,0,0) || (GetAsyncKeyState(VK_CONTROL)&0x8000) ||
       (GetAsyncKeyState(VK_SHIFT)&0x8000) || (GetAsyncKeyState(VK_MENU)&0x8000))return false;
    __try {
        const auto pawn=chaserObservedRolePawns[0];
        if(!ChaserIdentityLive(pawn))return false;
        auto item=TwinKartLocalRace::FlagFind(static_cast<BYTE*>(pawn.object),0x22d7240);
        if(ChaserBoosterInputSeat(item)!=0 || !HideHumanGameplayInput(TwinKartLocalRace::World(item)) ||
           !TwinKartLocalRace::CaptureFlagObject(item,chaserSecondSlotPress.item))return false;
        chaserSecondSlotPress.pawn=pawn;chaserSecondSlotPress.expires=GetTickCount64()+450;
        chaserSecondKeyConsumed=true;return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){chaserSecondSlotPress={};return false;}
}
void PumpChaserSecondSlot(BYTE* world,ULONGLONG now) {
    const auto press=chaserSecondSlotPress;chaserSecondSlotPress={};
    if(!press.expires || now>press.expires || GetForegroundWindow()!=gameWindow ||
       InterlockedCompareExchange(&exitPending,0,0))return;
    __try {
        if(!ChaserIdentityLive(press.pawn) || !ChaserIdentityLive(press.item))return;
        auto item=static_cast<BYTE*>(press.item.object);
        if(ChaserBoosterInputSeat(item)!=0 || TwinKartLocalRace::World(item)!=world || !HideHumanGameplayInput(world) ||
           *reinterpret_cast<void**>(item+0xa0)!=press.pawn.object)return;
        auto inventory=*reinterpret_cast<BYTE**>(item+0x548);
        TwinKartCamera::ObjectIdentity stack{};int uses=0;
        if(!ReadChaserStack(inventory,1,stack,uses) || !uses ||
           !reinterpret_cast<bool(__fastcall*)(void*,int)>(image+0x17bf2b0)(inventory,1) ||
           !ChaserIdentityLive(stack) || !ChaserIdentityLive(press.item))return;
        if(static_cast<BYTE*>(stack.object)[0xb0]==32) {
            auto mover=TwinKartLocalRace::FlagFind(static_cast<BYTE*>(press.pawn.object),0x22f9030);
            if(!TwinKartLocalRace::InChaserComponentWorld(mover) ||
               *reinterpret_cast<void**>(mover+0xa0)!=press.pawn.object)return;
            // The original booster rejects a press before the real
            // accelerator is positive. Retain the edge briefly so a W tap
            // and the item key on neighboring frames behave together.
            if(!(reinterpret_cast<float(__fastcall*)(void*)>(image+0x15f5260)(mover+0x1d8)>0.f)) {
                chaserSecondSlotPress=press;return;
            }
        }
        // Exactly one genuine slot input for this physical key edge. The
        // native handler owns aim, cooldown, consumption and effect delivery.
        reinterpret_cast<void(__fastcall*)(void*,int)>(image+0x1555690)(item,1);
        RuntimeEvent("native_chaser_human_second_slot_input_returned",0,1);
    } __except(RecordChaserItemFault(GetExceptionInformation(),0)) {
        RuntimeEvent("native_chaser_human_second_slot_input_fault",0,1);
    }
}
BYTE* __fastcall ReadOriginalChaserCastSequence(BYTE* inventory,int sequence) {
    using Lookup=BYTE*(__fastcall*)(void*,int);
    auto active=chaserBoosterRpcDelivery;
    if(GetCurrentThreadId()==queue.ThreadId() && active && active->type==32 && active->sequence==sequence && active->receiver) {
        auto item=static_cast<BYTE*>(active->receiver);
        if(ChaserBoosterInputSeat(item)>=0 && *reinterpret_cast<BYTE**>(item+0x548)==inventory)
            return ChaserBoosterHead(inventory,sequence);
    }
    return reinterpret_cast<Lookup>(image+0x17a92e0)(inventory,sequence);
}
void __fastcall ObserveChaserSlotCast(void* item,int sequence,unsigned char type) {
    if(item==chaserSubmittingItem && chaserSubmittingSlot>0)
        RuntimeEvent("native_chaser_slot_begin_cast_dispatched",chaserSubmittingSlot,type);
    const int boosterSeat=type==32?ChaserBoosterInputSeat(static_cast<BYTE*>(item)):-1;
    const bool owned=boosterSeat>=0;
    bool infinite=false;
    __try {infinite=(type==4 || type==9) && TwinKartLocalRace::InfiniteBoosterPlayerItem(item);}
    __except(EXCEPTION_EXECUTE_HANDLER){infinite=false;}
    TwinKartCamera::ObjectIdentity itemLease{},inventoryLease{},actorLease{};
    BYTE* inventory=nullptr;BYTE* actor=nullptr;
    bool captured=false;int observed=0;
    if((owned || infinite) && TwinKartLocalRace::CaptureFlagObject(static_cast<BYTE*>(item),itemLease)) {
        observed|=1;
        inventory=*reinterpret_cast<BYTE**>(static_cast<BYTE*>(item)+0x548);
        if(TwinKartLocalRace::CaptureFlagObject(inventory,inventoryLease)) {
            observed|=2;
            actor=owned?ChaserBoosterHead(inventory,sequence):
                reinterpret_cast<BYTE*(__fastcall*)(void*,int)>(image+0x17a92e0)(inventory,sequence);
            if(actor)observed|=4;
            const bool live=TwinKartLocalRace::CaptureFlagObject(actor,actorLease);
            if(live)observed|=8;
            captured=live &&
                (infinite?TwinKartLocalRace::InfiniteBoosterActor(actor,type):IsOwnedChaserBooster(actor));
            if(captured)observed|=16;
        }
    }
    TwinKartObjective::ChaserRpcDelivery delivery{item,sequence,type};auto previous=chaserBoosterRpcDelivery;
    chaserBoosterRpcDelivery=&delivery;
    __try {
        reinterpret_cast<ChaserCasting>(image+0x22d8ac0)(item,sequence,type); // Preserve the original RPC route once.
        auto world=captured?TwinKartLocalRace::World(static_cast<BYTE*>(item)):nullptr;
        bool fresh=captured && ChaserIdentityLive(itemLease) && ChaserIdentityLive(inventoryLease) && ChaserIdentityLive(actorLease) &&
            *reinterpret_cast<void**>(static_cast<BYTE*>(item)+0x548)==inventory &&
            (infinite?(TwinKartLocalRace::InfiniteBoosterActor(actor,type) && TwinKartLocalRace::InfiniteBoosterPlayerItem(item)):
                (IsOwnedChaserBooster(actor) && ChaserItemGame(world) && !reinterpret_cast<bool(__fastcall*)(void*)>(image+0x4011c70)(world))) &&
            (owned?ChaserBoosterHead(inventory,sequence):
                reinterpret_cast<BYTE*(__fastcall*)(void*,int)>(image+0x17a92e0)(inventory,sequence))==actor;
        if(owned && !delivery.entered && !fresh)RuntimeEvent("native_chaser_booster_sequence_unqualified",boosterSeat,observed);
        if(delivery.ReserveLocal(owned || infinite,world && !*reinterpret_cast<void**>(world+0x38),fresh)) {
            // The native slot input already checked both original use
            // predicates. Deliver a missing standalone RPC to the original
            // server implementation, which retains inventory/role/cast rules.
            ObserveChaserCasting(item,sequence,type);
            RuntimeEvent(infinite?"native_infinite_booster_standalone_cast_returned":"native_chaser_booster_standalone_rpc_returned",infinite?chaserSubmittingSlot:boosterSeat,1);
        }
    } __finally {chaserBoosterRpcDelivery=previous;}
}
void __fastcall ObserveInfiniteBoosterPress(void* raw,int slot) {
    __try {
        if(slot==0 && TwinKartLocalRace::InfiniteBoosterPlayerItem(raw)) {
            auto item=static_cast<BYTE*>(raw),pawn=*reinterpret_cast<BYTE**>(item+0xa0);
            auto inventory=TwinKartLocalRace::FlagFind(pawn,0x22ef370);
            auto movement=TwinKartLocalRace::FlagFind(pawn,0x22f9030);
            bool usable=TwinKartLocalRace::InRaceWorld(inventory) &&
                reinterpret_cast<bool(__fastcall*)(void*,int)>(image+0x17bf2b0)(inventory,0);
            int result=usable?1:0;
            if(usable && TwinKartLocalRace::InRaceWorld(movement) && *reinterpret_cast<BYTE**>(movement+0x140)==image+0x5118b80) {
                auto api=movement+0x140;
                if(reinterpret_cast<bool(__fastcall*)(void*)>(image+0x15f9490)(api))result|=2;
                if(reinterpret_cast<bool(__fastcall*)(void*)>(image+0x15f9780)(api))result|=4;
                if(reinterpret_cast<float(__fastcall*)(void*)>(image+0x15f5260)(movement+0x1d8)>0.f)result|=8;
                auto world=TwinKartLocalRace::World(item);
                auto timer=TwinKartLocalRace::FlagFind(*reinterpret_cast<BYTE**>(world+0x120),0x23d2930);
                if(TwinKartLocalRace::InRaceWorld(timer)) {
                    result|=16;
                    if(reinterpret_cast<float(__fastcall*)(void*)>(image+0x1993c90)(timer)>=0.f)result|=32;
                }
                // A simultaneous W/Ctrl edge can run before the movement
                // tick computes real accelerator. Keep that press for at
                // most 250 ms; only positive native throttle can deliver it.
                if(result==49 && TwinKartLocalRace::CaptureFlagObject(item,TwinKartLocalRace::pendingBoosterPress))
                    TwinKartLocalRace::pendingBoosterUntil=GetTickCount64()+250;
                else TwinKartLocalRace::pendingBoosterPress={};
            }
            RuntimeEvent("native_infinite_booster_press_gates",TwinKartLocalRace::armedMode,result);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER){RuntimeEvent("native_infinite_booster_press_diagnostic_fault",0,0);}
    reinterpret_cast<void(__fastcall*)(void*,int)>(image+0x1555690)(raw,slot);
}
bool IsOwnedChaserBooster(void* raw) {
    __try {
        if(!raw || GetCurrentThreadId()!=queue.ThreadId() || *static_cast<BYTE**>(raw)!=image+0x5171d28)return false;
        using GetWorld=BYTE*(__fastcall*)(void*);
        auto world=reinterpret_cast<GetWorld>((*static_cast<void***>(raw))[0x160/8])(raw);
        return OwnedChaserWorld(world);
    }
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
void __fastcall ObserveChaserBoosterApplied(void* actor,void* item) {
    if(chaserBoosterDelivery)chaserBoosterDelivery->Observe(actor);
    if(chaserBoosterRpcDelivery)chaserBoosterRpcDelivery->Observe(item,*reinterpret_cast<int*>(static_cast<BYTE*>(actor)+0x5c0),32);
    bool owned=IsOwnedChaserBooster(actor);
    originalChaserBoosterApplied(actor,item);
    if(owned)RuntimeEvent("native_chaser_box_booster_effect_returned",-1,1);
}
void __fastcall DeliverChaserBoosterAuthority(void* actor,void* item) {
    int seat=IsOwnedChaserBooster(actor)?ChaserBoosterInputSeat(static_cast<BYTE*>(item)):-1;
    bool owned=seat>=0;
    TwinKartCamera::ObjectIdentity actorLease{},itemLease{};
    bool captured=owned && TwinKartLocalRace::CaptureFlagObject(static_cast<BYTE*>(actor),actorLease) &&
        TwinKartLocalRace::CaptureFlagObject(static_cast<BYTE*>(item),itemLease);
    TwinKartObjective::ChaserEffectDelivery delivery{actor};auto previous=chaserBoosterDelivery;
    chaserBoosterDelivery=&delivery;
    __try {
        originalChaserBoosterAuthority(actor,item); // Native inventory/permission checks precede this authoritative callback.
        if(owned)RuntimeEvent("native_chaser_box_booster_authority_returned",seat,delivery.delivered?1:0);
        auto world=captured?TwinKartLocalRace::World(static_cast<BYTE*>(item)):nullptr;
        bool fresh=captured && ChaserIdentityLive(actorLease) && ChaserIdentityLive(itemLease) && IsOwnedChaserBooster(actor) &&
            ChaserItemGame(world) && !reinterpret_cast<bool(__fastcall*)(void*)>(image+0x4011c70)(world);
        bool standalone=world && !*reinterpret_cast<void**>(world+0x38);
        // A standalone authoritative call has no remote recipient. Preserve
        // normal native multicast delivery; only an actually missing local
        // callback may be delivered here, after original authority returned.
        if(delivery.ReserveLocal(owned,standalone,fresh)) {
            originalChaserBoosterApplied(actor,item);
            RuntimeEvent("native_chaser_box_booster_local_delivery_returned",seat,1);
        }
    } __finally {chaserBoosterDelivery=previous;}
}
void __fastcall ObserveChaserCasting(void* raw,int sequence,unsigned char type) {
    if(chaserBoosterRpcDelivery)chaserBoosterRpcDelivery->Observe(raw,sequence,type);
    BYTE* pawn=nullptr;int slot=-1,before=0;
    bool observed=TwinKartLocalRace::ReadNativeChaserItemUse(raw,pawn,slot,before);
    bool infinite=false;
    __try {
        if((type==4 || type==9) && TwinKartLocalRace::InfiniteBoosterPlayerItem(raw)) {
            pawn=*reinterpret_cast<BYTE**>(static_cast<BYTE*>(raw)+0xa0);
            infinite=TwinKartLocalRace::CubeItemCount(pawn,before);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER){infinite=false;}
    originalChaserCasting(raw,sequence,type); // Native server implementation, once.
    if(infinite) {
        int after=0;
        if(TwinKartLocalRace::CubeItemCount(pawn,after) && after<before)
            RuntimeEvent("native_infinite_booster_consumed",TwinKartLocalRace::armedMode,type);
    }
    if(observed) {
        BYTE* afterPawn=nullptr;int afterSlot=-1,after=0;
        if(TwinKartLocalRace::ReadNativeChaserItemUse(raw,afterPawn,afterSlot,after) && afterPawn==pawn && afterSlot==slot) {
            RuntimeEvent("native_chaser_ai_server_cast_returned",slot,type);
            if(after<before)RuntimeEvent("native_chaser_ai_server_cast_consumed",slot,before-after);
        }
    }
}
void __fastcall ObserveChaserEndCasting(void* raw,int sequence,unsigned char type,void* target) {
    BYTE* pawn=nullptr;int slot=-1,before=0;
    bool observed=TwinKartLocalRace::ReadNativeChaserItemUse(raw,pawn,slot,before);
    originalChaserEndCasting(raw,sequence,type,target);
    if(observed) {
        BYTE* afterPawn=nullptr;int afterSlot=-1,after=0;
        if(TwinKartLocalRace::ReadNativeChaserItemUse(raw,afterPawn,afterSlot,after) && afterPawn==pawn && afterSlot==slot) {
            RuntimeEvent("native_chaser_ai_server_end_cast_returned",slot,type);
            if(after<before)RuntimeEvent("native_chaser_ai_server_end_cast_consumed",slot,before-after);
        }
    }
}
bool CollectChaserItemRoster(BYTE* world,ChaserItemNativeContext& view) {
    if(!ChaserItemGame(world) || !TwinKartLocalRace::CaptureFlagObject(world,view.worldIdentity))return false;
    view.world=world;auto rules=static_cast<BYTE*>(chaserObservedRule.object);
    if(!ChaserIdentityLive(chaserObservedRule))return false;
    auto list=*reinterpret_cast<BYTE**>(rules+0xc0),state=*reinterpret_cast<BYTE**>(world+0x120);
    using Type=void*(__fastcall*)();auto type=reinterpret_cast<Type>(image+0x23b0c20)();
    if(!TwinKartLocalRace::OwnedComponent(list,state,type) &&
       !TwinKartLocalRace::OwnedComponent(list,*reinterpret_cast<BYTE**>(world+0x118),type))return false;
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);int objectCount=*reinterpret_cast<int*>(image+0x69c28c4);
    if(!chaserRoleWorld.Matches(TwinKartLocalRace::World(list),chunks,objectCount))return false;
    struct Pawns {BYTE** data;int count,capacity;};auto pawns=*reinterpret_cast<Pawns*>(list+0x100);
    if(!pawns.data || pawns.count<2 || pawns.count>16 || pawns.capacity<pawns.count || pawns.capacity>4096)return false;
    unsigned seen=0;auto stateType=reinterpret_cast<Type>(image+0x23afeb0)();
    for(int i=0;i<pawns.count;++i) {
        auto pawn=pawns.data[i];if(!TwinKartLocalRace::Live(pawn) || !TwinKartRace::ObjectiveAssetIsA(image,pawn,0x23237f0))return false;
        auto pawnState=TwinKartLocalRace::FlagFind(pawn,0x23afeb0);
        if(!TwinKartLocalRace::OwnedComponent(pawnState,pawn,stateType) ||
           !chaserRoleWorld.Matches(TwinKartLocalRace::World(pawnState),chunks,objectCount))return false;
        int slot=*reinterpret_cast<int*>(pawnState+0x168);
        if(slot<0 || slot>=pawns.count || (seen&(1u<<slot)) || (slot==0?pawnState[0x321]!=0:pawnState[0x321]==0))return false;
        for(int previous=0;previous<16;++previous)
            if(view.seats[previous].pawn.object==pawn || view.seats[previous].state.object==pawnState)return false;
        if(!TwinKartLocalRace::CaptureFlagObject(pawn,view.seats[slot].pawn) ||
           !TwinKartLocalRace::CaptureFlagObject(pawnState,view.seats[slot].state))return false;
        seen|=1u<<slot;
    }
    view.count=pawns.count;return seen==((1u<<pawns.count)-1) && ChaserItemGame(world) && ChaserIdentityLive(view.worldIdentity);
}
void UpdateChaserItems(BYTE* world,ULONGLONG now) {
    __try {
        ChaserItemNativeContext view;
        if(!CollectChaserItemRoster(world,view)){chaserItemInputs.Reset();return;}
        auto result=chaserItemInputs.Step(now,true,view.count,ReadChaserItemSeat,SubmitChaserItem,&view);
        if(result.slot>=0 && !result.submitted)RuntimeEvent("native_chaser_ai_item_intent_expired",result.slot,0);
    } __except(EXCEPTION_EXECUTE_HANDLER){chaserItemInputs.Reset();RuntimeEvent("native_chaser_ai_item_poll_rejected",-1,0);}
}
bool ReadChaserGeometryBounds(void* raw,void* world,TwinKartRace::ArenaPoint& center,TwinKartRace::ArenaPoint& extent,void*) {
    __try {
        auto actor=static_cast<BYTE*>(raw);
        if(!OwnedChaserWorld(static_cast<BYTE*>(world)) || !TwinKartLocalRace::Live(actor) ||
           (*reinterpret_cast<unsigned*>(actor+8)&0x18030) || !TwinKartRace::ObjectiveAssetIsA(image,actor,0x43d82e0))return false;
        using World=BYTE*(__fastcall*)(void*);
        if(reinterpret_cast<World>((*reinterpret_cast<void***>(actor))[0x160/8])(actor)!=world)return false;
        using Bounds=void(__fastcall*)(void*,bool,TwinKartRace::ArenaPoint*,TwinKartRace::ArenaPoint*,bool);
        // Exact AActor reflected wrapper: colliding-components bool in DL,
        // Origin/Extent output references in R8/R9, child-actor bool on stack.
        reinterpret_cast<Bounds>(image+0x3cb2830)(actor,true,&center,&extent,false);return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
struct ChaserPursuitTarget {
    BYTE* world=nullptr;TwinKartCamera::ObjectIdentity worldIdentity{},pawn{},state{},root{};
    int driver=-1,slot=-1;
    std::uint64_t Key()const{return (static_cast<std::uint64_t>(static_cast<unsigned>(pawn.index))<<32)|static_cast<unsigned>(pawn.serial);}
};
struct ChaserBodyLease {
    TwinKartRace::ComponentRotation rotation{};
    BYTE* world=nullptr;BYTE* body=nullptr;void* actorHandle=nullptr;void* physicsScene=nullptr;void* actorScene=nullptr;
    TwinKartCamera::ObjectIdentity worldIdentity{},pawn{},state{},movement{},root{};
    TwinKartRace::ArenaPoint position{},scale{};
    void* bodyGetter=nullptr;void* zeroExtent=nullptr;void* initialize=nullptr;std::uint64_t shapeSignature=0;
    int slot=-1;TwinKartObjective::ChaserRole role=TwinKartObjective::ChaserRole::Unknown;unsigned shapeCount=0,padding=0;
    const ChaserPursuitTarget* planningTarget=nullptr;
};
void* __fastcall ChaserActorScene(const void* handle) {
    auto actor=static_cast<BYTE*>(TwinKartRace::BodyGeometryDetail::Actor(handle));if(!actor)return nullptr;
    // Exact original ExecuteRead queries this PhysX actor interface before
    // locking. Here it runs inside that read callback and only observes scene.
    using Scene=void*(__fastcall*)(void*);return reinterpret_cast<Scene>((*reinterpret_cast<void***>(actor))[0x30/8])(actor);
}
TwinKartRace::BodyGeometrySnapshot ChaserBodyGeometry(const ChaserBodyLease& lease) {
    TwinKartRace::BodyGeometryApi api{reinterpret_cast<TwinKartRace::ReadPhysicsActor>(image+0x41a1c90),
        reinterpret_cast<TwinKartRace::CollectBodyShapes>(image+0x4191fb0),
        reinterpret_cast<TwinKartRace::BodyGeometryType>(image+0x41a4830),ChaserActorScene,
        reinterpret_cast<TwinKartRace::FreeBodyShapeStorage>(image+0x248b950)};
    return TwinKartRace::ReadBodyGeometry(lease.body+0x120,lease.actorHandle,api);
}
bool ChaserOriginalFunction(const void* pointer) {
    auto address=reinterpret_cast<std::uintptr_t>(pointer),base=reinterpret_cast<std::uintptr_t>(image);
    return address>=base+0x1000 && address<base+0x4ed0000;
}
bool ChaserBodyWorldActive(BYTE* world) {
    if(!ChaserItemGame(world))return false;
    using Paused=bool(__fastcall*)(void*);return !reinterpret_cast<Paused>(image+0x4011c70)(world);
}
bool CurrentChaserPursuitTarget(const ChaserPursuitTarget& target) {
    __try {
        if(target.driver<1 || target.driver>=16 || target.slot<0 || target.slot>=16 || target.slot==target.driver ||
           !ChaserBodyWorldActive(target.world) || !ChaserIdentityLive(target.worldIdentity) || target.worldIdentity.object!=target.world ||
           !ChaserIdentityLive(target.pawn) || !ChaserIdentityLive(target.state) || !ChaserIdentityLive(target.root))return false;
        auto pawn=static_cast<BYTE*>(target.pawn.object),state=static_cast<BYTE*>(target.state.object);
        auto root=static_cast<BYTE*>(target.root.object);
        if(*reinterpret_cast<void**>(pawn+0x130)!=root || *reinterpret_cast<void**>(root+0xa0)!=pawn ||
           *reinterpret_cast<void**>(state+0xa0)!=pawn || *reinterpret_cast<int*>(state+0x168)!=target.slot || state[0x338]!=1 ||
           (target.slot==0?state[0x321]!=0:state[0x321]==0) || TwinKartLocalRace::FlagFind(pawn,0x23afeb0)!=state ||
           TwinKartObjective::NativeChaserRole(TwinKartLocalRace::FlagFind(pawn,0x2256c10)!=nullptr,
               TwinKartLocalRace::FlagFind(pawn,0x22570a0)!=nullptr)!=TwinKartObjective::ChaserRole::Runaway)return false;
        using World=BYTE*(__fastcall*)(void*);
        if(reinterpret_cast<World>((*reinterpret_cast<void***>(pawn))[0x160/8])(pawn)!=target.world ||
           TwinKartLocalRace::World(state)!=target.world || TwinKartLocalRace::World(root)!=target.world)return false;
        auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);int count=*reinterpret_cast<int*>(image+0x69c28c4);
        return chaserRoleWorld.Matches(target.world,chunks,count) && ChaserIdentityLive(target.pawn) &&
            ChaserIdentityLive(target.state) && ChaserIdentityLive(target.root);
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool ChaserPursuitRosterCurrent(const ChaserPursuitTarget& target) {
    ChaserItemNativeContext roster;
    if(!CurrentChaserPursuitTarget(target) || !CollectChaserItemRoster(target.world,roster) ||
       target.slot>=roster.count || target.driver>=roster.count)return false;
    const auto& seat=roster.seats[target.slot];
    return seat.pawn.object==target.pawn.object && seat.pawn.index==target.pawn.index && seat.pawn.serial==target.pawn.serial &&
        seat.state.object==target.state.object && seat.state.index==target.state.index && seat.state.serial==target.state.serial &&
        roster.seats[target.driver].pawn.object==chaserDriverPawns[target.driver].object &&
        roster.seats[target.driver].pawn.index==chaserDriverPawns[target.driver].index &&
        roster.seats[target.driver].pawn.serial==chaserDriverPawns[target.driver].serial &&
        roster.seats[target.driver].state.object==chaserDriverStates[target.driver].object &&
        roster.seats[target.driver].state.index==chaserDriverStates[target.driver].index &&
        roster.seats[target.driver].state.serial==chaserDriverStates[target.driver].serial && CurrentChaserPursuitTarget(target);
}
bool CaptureChaserPursuitTarget(BYTE* world,int driver,int slot,ChaserPursuitTarget& outputTarget) {
    __try {
        ChaserItemNativeContext roster;
        if(driver<1 || driver>=16 || slot<0 || slot>=16 || driver==slot || !CollectChaserItemRoster(world,roster) ||
           slot>=roster.count || driver>=roster.count)return false;
        ChaserPursuitTarget target;target.world=world;target.worldIdentity=roster.worldIdentity;
        target.driver=driver;target.slot=slot;target.pawn=roster.seats[slot].pawn;target.state=roster.seats[slot].state;
        if(!ChaserIdentityLive(target.pawn) || !TwinKartLocalRace::CaptureFlagObject(
            *reinterpret_cast<BYTE**>(static_cast<BYTE*>(target.pawn.object)+0x130),target.root) ||
           !ChaserPursuitRosterCurrent(target))return false;
        outputTarget=target;return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool ValidateChaserBody(void* rawWorld,void* rawComponent,void* rawOwner,void* raw) {
    __try {
        auto& lease=*static_cast<ChaserBodyLease*>(raw);
        if(rawWorld!=lease.world || rawComponent!=lease.root.object || rawOwner!=lease.pawn.object ||
           lease.slot<1 || lease.slot>=16 || !ChaserIdentityLive(lease.worldIdentity) || !ChaserIdentityLive(lease.pawn) ||
           !ChaserIdentityLive(lease.state) || !ChaserIdentityLive(lease.movement) || !ChaserIdentityLive(lease.root) ||
           !ChaserBodyWorldActive(lease.world) || !lease.physicsScene ||
           *reinterpret_cast<void**>(lease.world+0x1f0)!=lease.physicsScene)return false;
        auto pawn=static_cast<BYTE*>(lease.pawn.object),state=static_cast<BYTE*>(lease.state.object);
        auto root=static_cast<BYTE*>(lease.root.object),movement=static_cast<BYTE*>(lease.movement.object);
        if(chaserDriverPawns[lease.slot].object!=pawn || chaserDriverPawns[lease.slot].serial!=lease.pawn.serial ||
           chaserDriverStates[lease.slot].object!=state || chaserDriverStates[lease.slot].serial!=lease.state.serial ||
           chaserDriverMovements[lease.slot].object!=movement || chaserDriverMovements[lease.slot].serial!=lease.movement.serial ||
           !chaserDriverPrepared[lease.slot] || !movement[0xed8] ||
           *reinterpret_cast<void**>(pawn+0x130)!=root || *reinterpret_cast<void**>(root+0xa0)!=pawn ||
           *reinterpret_cast<void**>(root+0x20)!=pawn || *reinterpret_cast<void**>(movement+0xa0)!=pawn ||
           *reinterpret_cast<void**>(state+0xa0)!=pawn || *reinterpret_cast<int*>(state+0x168)!=lease.slot ||
           !state[0x321] || state[0x338]!=1 || TwinKartLocalRace::FlagFind(pawn,0x23afeb0)!=state ||
           TwinKartLocalRace::FlagFind(pawn,0x22f9030)!=movement ||
           !TwinKartRace::ObjectiveAssetIsA(image,pawn,0x23237f0) || !TwinKartRace::ObjectiveAssetIsA(image,root,0x22fef20))return false;
        auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);int count=*reinterpret_cast<int*>(image+0x69c28c4);
        for(auto component:{root,state,movement})if(!chaserRoleWorld.Matches(TwinKartLocalRace::World(component),chunks,count))return false;
        if(lease.role==TwinKartObjective::ChaserRole::Unknown ||
           TwinKartObjective::NativeChaserRole(TwinKartLocalRace::FlagFind(pawn,0x2256c10)!=nullptr,
               TwinKartLocalRace::FlagFind(pawn,0x22570a0)!=nullptr)!=lease.role)return false;
        auto table=*reinterpret_cast<void***>(root);
        if(table[0x6d8/8]!=lease.bodyGetter || table[0x6f8/8]!=lease.zeroExtent || table[0x748/8]!=lease.initialize ||
           !ChaserOriginalFunction(lease.bodyGetter) || !ChaserOriginalFunction(lease.zeroExtent) || !ChaserOriginalFunction(lease.initialize))return false;
        using Zero=bool(__fastcall*)(void*);using Body=BYTE*(__fastcall*)(void*,unsigned long long,bool);
        // A point/ray fallback is not an actual vehicle-body sweep.
        if(reinterpret_cast<Zero>(lease.zeroExtent)(root))return false;
        auto body=reinterpret_cast<Body>(lease.bodyGetter)(root,0,true);
        using ValidBody=bool(__fastcall*)(void*);
        if(!body || body!=lease.body || !reinterpret_cast<ValidBody>(image+0x415aa50)(body) ||
           !lease.actorHandle || *reinterpret_cast<void**>(body+0x120)!=lease.actorHandle)return false;
        auto geometry=ChaserBodyGeometry(lease);
        if(!geometry.valid || geometry.scene!=lease.actorScene || geometry.count!=lease.shapeCount || geometry.signature!=lease.shapeSignature)return false;
        // A synchronous query must use one stable actual component pose/scale.
        // Do not reuse this lease through a physics/model/role/respawn callback.
        return !memcmp(root+0x1d0,&lease.rotation,sizeof(lease.rotation)) &&
            !memcmp(root+0x1e0,&lease.position,sizeof(lease.position)) && !memcmp(root+0x1f0,&lease.scale,sizeof(lease.scale)) &&
            ChaserIdentityLive(lease.worldIdentity) && ChaserIdentityLive(lease.root) && ChaserBodyWorldActive(lease.world) &&
            *reinterpret_cast<void**>(lease.world+0x1f0)==lease.physicsScene;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool CaptureChaserBody(BYTE* world,int slot,ChaserBodyLease& outputLease) {
    __try {
        if(slot<1 || slot>=16 || !ChaserBodyWorldActive(world) || !chaserDriverPrepared[slot] ||
           !ChaserIdentityLive(chaserDriverPawns[slot]) || !ChaserIdentityLive(chaserDriverStates[slot]) ||
           !ChaserIdentityLive(chaserDriverMovements[slot]))return false;
        ChaserBodyLease lease;lease.world=world;lease.slot=slot;lease.pawn=chaserDriverPawns[slot];
        lease.state=chaserDriverStates[slot];lease.movement=chaserDriverMovements[slot];
        if(!TwinKartLocalRace::CaptureFlagObject(world,lease.worldIdentity))return false;
        lease.physicsScene=*reinterpret_cast<void**>(world+0x1f0);if(!lease.physicsScene)return false;
        auto pawn=static_cast<BYTE*>(lease.pawn.object),root=*reinterpret_cast<BYTE**>(pawn+0x130);
        if(!TwinKartLocalRace::CaptureFlagObject(root,lease.root) || !TwinKartRace::ObjectiveAssetIsA(image,root,0x22fef20))return false;
        if(!TwinKartRace::ObjectiveAssetIsA(image,pawn,0x23237f0) || *reinterpret_cast<void**>(root+0xa0)!=pawn ||
           *reinterpret_cast<void**>(root+0x20)!=pawn || !chaserRoleWorld.Matches(TwinKartLocalRace::World(root),
               *reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),*reinterpret_cast<int*>(image+0x69c28c4)))return false;
        auto state=static_cast<BYTE*>(lease.state.object),movement=static_cast<BYTE*>(lease.movement.object);
        if(*reinterpret_cast<void**>(state+0xa0)!=pawn || *reinterpret_cast<void**>(movement+0xa0)!=pawn ||
           *reinterpret_cast<int*>(state+0x168)!=slot || !state[0x321] || state[0x338]!=1 || !movement[0xed8])return false;
        lease.role=TwinKartObjective::NativeChaserRole(TwinKartLocalRace::FlagFind(pawn,0x2256c10)!=nullptr,
            TwinKartLocalRace::FlagFind(pawn,0x22570a0)!=nullptr);
        if(lease.role==TwinKartObjective::ChaserRole::Unknown)return false;
        auto table=*reinterpret_cast<void***>(root);lease.bodyGetter=table[0x6d8/8];lease.zeroExtent=table[0x6f8/8];lease.initialize=table[0x748/8];
        if(!ChaserOriginalFunction(lease.bodyGetter) || !ChaserOriginalFunction(lease.zeroExtent) || !ChaserOriginalFunction(lease.initialize))return false;
        using Zero=bool(__fastcall*)(void*);using Body=BYTE*(__fastcall*)(void*,unsigned long long,bool);
        if(reinterpret_cast<Zero>(lease.zeroExtent)(root))return false;
        lease.body=reinterpret_cast<Body>(lease.bodyGetter)(root,0,true);
        using ValidBody=bool(__fastcall*)(void*);
        if(!lease.body || !reinterpret_cast<ValidBody>(image+0x415aa50)(lease.body))return false;
        lease.actorHandle=*reinterpret_cast<void**>(lease.body+0x120);if(!lease.actorHandle)return false;
        auto geometry=ChaserBodyGeometry(lease);if(!geometry.valid)return false;
        lease.actorScene=geometry.scene;lease.shapeCount=geometry.count;lease.shapeSignature=geometry.signature;
        memcpy(&lease.rotation,root+0x1d0,sizeof(lease.rotation));memcpy(&lease.position,root+0x1e0,sizeof(lease.position));
        memcpy(&lease.scale,root+0x1f0,sizeof(lease.scale));
        if(!TwinKartRace::SweepPoint(lease.position) || !TwinKartRace::SweepQuaternion(lease.rotation) || !TwinKartRace::Finite(lease.scale) ||
           std::fabs(lease.scale.x)<0.0001f || std::fabs(lease.scale.y)<0.0001f || std::fabs(lease.scale.z)<0.0001f ||
           std::fabs(lease.scale.x)>1000 || std::fabs(lease.scale.y)>1000 || std::fabs(lease.scale.z)>1000 ||
           !ValidateChaserBody(world,root,pawn,&lease))return false;
        outputLease=lease;return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool ChaserContactApproachScope(void* raw) {
    __try {
        auto& body=*static_cast<ChaserBodyLease*>(raw);
        if(body.slot<1 || body.slot>=16 || !ChaserBodyWorldActive(body.world) || !ChaserIdentityLive(body.worldIdentity) ||
           !ChaserIdentityLive(body.pawn) || !ChaserIdentityLive(body.state) || !ChaserIdentityLive(body.movement) ||
           !ChaserIdentityLive(body.root) || !chaserDriverPrepared[body.slot])return false;
        auto pawn=static_cast<BYTE*>(body.pawn.object),state=static_cast<BYTE*>(body.state.object);
        auto movement=static_cast<BYTE*>(body.movement.object),root=static_cast<BYTE*>(body.root.object);
        if(chaserDriverPawns[body.slot].object!=pawn || chaserDriverPawns[body.slot].serial!=body.pawn.serial ||
           chaserDriverStates[body.slot].object!=state || chaserDriverStates[body.slot].serial!=body.state.serial ||
           chaserDriverMovements[body.slot].object!=movement || chaserDriverMovements[body.slot].serial!=body.movement.serial ||
           *reinterpret_cast<void**>(pawn+0x130)!=root || *reinterpret_cast<void**>(state+0xa0)!=pawn ||
           *reinterpret_cast<void**>(movement+0xa0)!=pawn || *reinterpret_cast<int*>(state+0x168)!=body.slot ||
           !state[0x321] || state[0x338]!=1 || !movement[0xed8] ||
           TwinKartLocalRace::FlagFind(pawn,0x23afeb0)!=state || TwinKartLocalRace::FlagFind(pawn,0x22f9030)!=movement)return false;
        return TwinKartObjective::NativeChaserRole(TwinKartLocalRace::FlagFind(pawn,0x2256c10)!=nullptr,
            TwinKartLocalRace::FlagFind(pawn,0x22570a0)!=nullptr)==body.role &&
            !memcmp(root+0x1d0,&body.rotation,sizeof(body.rotation)) && !memcmp(root+0x1e0,&body.position,sizeof(body.position)) &&
            !memcmp(root+0x1f0,&body.scale,sizeof(body.scale));
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool ChaserHitObjectIdentity(void* rawObject,const TwinKartRace::HitWeakIdentity& expected,bool actor,void* raw) {
    __try {
        auto& body=*static_cast<ChaserBodyLease*>(raw);auto object=static_cast<BYTE*>(rawObject);
        if(!ChaserContactApproachScope(raw) || !TwinKartRace::ObjectiveAssetIsA(image,object,actor?0x23237f0:0x4512540))return false;
        auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);int count=*reinterpret_cast<int*>(image+0x69c28c4);
        TwinKartCamera::ObjectIdentity identity{};
        if(!TwinKartCamera::Capture(object,chunks,count,&identity) || identity.index!=expected.index || identity.serial!=expected.serial)return false;
        using World=BYTE*(__fastcall*)(void*);
        auto world=actor?reinterpret_cast<World>((*reinterpret_cast<void***>(object))[0x160/8])(object):TwinKartLocalRace::World(object);
        return chaserRoleWorld.Matches(world,chunks,count) && world==body.world;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
TwinKartRace::ComponentContactDecision ChaserBodyContactPolicy(void* rawWorld,void* component,void* owner,
    const unsigned char* hit,void* raw) {
    using Decision=TwinKartRace::ComponentContactDecision;
    __try {
        auto& body=*static_cast<ChaserBodyLease*>(raw);
        if(rawWorld!=body.world || component!=body.root.object || owner!=body.pawn.object || !ChaserContactApproachScope(raw))return Decision::Invalid;
        if(body.role!=TwinKartObjective::ChaserRole::Tagger)return Decision::Obstacle;
        const auto candidate=body.planningTarget;
        if(candidate && (candidate->world!=body.world || candidate->driver!=body.slot || !CurrentChaserPursuitTarget(*candidate)))return Decision::Invalid;
        int target=candidate?candidate->slot:chaserDrivers[body.slot].TargetSlot();if(target<0 || target>=16 || target==body.slot)return Decision::Obstacle;
        TwinKartRace::HitObjects objects;
        if(!TwinKartRace::ReadHitObjects(hit,reinterpret_cast<TwinKartRace::ResolveHitWeak>(image+0x2799770),
            ChaserContactApproachScope,ChaserHitObjectIdentity,raw,objects))
            return ChaserContactApproachScope(raw)?Decision::Obstacle:Decision::Invalid;
        auto pawn=static_cast<BYTE*>(objects.actor),primitive=static_cast<BYTE*>(objects.component);
        if(pawn==owner || primitive==component || *reinterpret_cast<void**>(primitive+0xa0)!=pawn)return Decision::Obstacle;
        ChaserItemNativeContext roster;
        if(!CollectChaserItemRoster(body.world,roster) || target>=roster.count || roster.seats[target].pawn.object!=pawn ||
           roster.seats[target].pawn.index!=objects.actorIdentity.index || roster.seats[target].pawn.serial!=objects.actorIdentity.serial ||
           !ChaserIdentityLive(roster.seats[target].state))return Decision::Obstacle;
        auto state=static_cast<BYTE*>(roster.seats[target].state.object);
        if(candidate && (candidate->pawn.object!=pawn || candidate->pawn.index!=objects.actorIdentity.index ||
           candidate->pawn.serial!=objects.actorIdentity.serial || candidate->state.object!=state ||
           candidate->state.serial!=roster.seats[target].state.serial))return Decision::Invalid;
        if(*reinterpret_cast<void**>(state+0xa0)!=pawn || *reinterpret_cast<int*>(state+0x168)!=target || state[0x338]!=1 ||
           (target==0?state[0x321]!=0:state[0x321]==0) ||
           TwinKartObjective::NativeChaserRole(TwinKartLocalRace::FlagFind(pawn,0x2256c10)!=nullptr,
               TwinKartLocalRace::FlagFind(pawn,0x22570a0)!=nullptr)!=TwinKartObjective::ChaserRole::Runaway)return Decision::Obstacle;
        // This permits a fresh query toward the selected original runaway or
        // an explicitly leased planning candidate. Native physics/threshold and
        // original collision -> server-infection permissions/results remain
        // unchanged. No infection RPC or actor response is invoked here.
        if(!ChaserHitObjectIdentity(pawn,objects.actorIdentity,true,raw) ||
           !ChaserHitObjectIdentity(primitive,objects.componentIdentity,false,raw) ||
           !ChaserIdentityLive(roster.worldIdentity) || !ChaserIdentityLive(roster.seats[target].state) ||
           TwinKartLocalRace::FlagFind(pawn,0x23afeb0)!=state || *reinterpret_cast<void**>(primitive+0xa0)!=pawn ||
           *reinterpret_cast<int*>(state+0x168)!=target || state[0x338]!=1 ||
           TwinKartObjective::NativeChaserRole(TwinKartLocalRace::FlagFind(pawn,0x2256c10)!=nullptr,
               TwinKartLocalRace::FlagFind(pawn,0x22570a0)!=nullptr)!=TwinKartObjective::ChaserRole::Runaway ||
           (candidate?body.planningTarget!=candidate || !ChaserPursuitRosterCurrent(*candidate):
               chaserDrivers[body.slot].TargetSlot()!=target) || !ChaserContactApproachScope(raw))return Decision::Invalid;
        return Decision::Approach;
    } __except(EXCEPTION_EXECUTE_HANDLER){return Decision::Invalid;}
}
TwinKartRace::VolumeSweep QueryChaserBody(ChaserBodyLease& lease,const TwinKartRace::ArenaPoint& first,
    const TwinKartRace::ArenaPoint& last,const TwinKartRace::ComponentRotation& rotation,bool approach=true) {
    __try {
        TwinKartRace::ComponentSweepApi api{
            reinterpret_cast<TwinKartRace::ConstructCollisionParams>(image+0x3da90a0),
            reinterpret_cast<TwinKartRace::ConstructCollisionResponses>(image+0x4193460),
            reinterpret_cast<TwinKartRace::SetCollisionResponses>(image+0x41ab200),
            reinterpret_cast<TwinKartRace::InitComponentSweep>(lease.initialize),
            reinterpret_cast<TwinKartRace::ComponentBodySweep>(image+0x3dd7a30),
            reinterpret_cast<TwinKartRace::ReleaseSweepAllocation>(image+0x248b950)};
        return TwinKartRace::QueryComponentBody(lease.world,lease.root.object,lease.pawn.object,api,ValidateChaserBody,&lease,first,last,rotation,
            approach?ChaserBodyContactPolicy:nullptr,&lease);
    } __except(EXCEPTION_EXECUTE_HANDLER){return {};}
}
struct ChaserGroundLease {
    ChaserBodyLease body;
    TwinKartRace::QueryArray flags{},points{},components{};
    TwinKartRace::KartContactSnapshot contacts;
    TwinKartCamera::ObjectIdentity support[4]{};
    TwinKartRace::KartRoadCalibration calibration;
};
bool ChaserContactArray(const TwinKartRace::QueryArray& array) {
    return array.data && array.count==4 && array.capacity>=4 && array.capacity<=64;
}
bool ChaserContactsValid(void* rawInterface,void* raw) {
    __try {
        auto& lease=*static_cast<ChaserGroundLease*>(raw);const auto& body=lease.body;
        auto movement=static_cast<BYTE*>(body.movement.object),pawn=static_cast<BYTE*>(body.pawn.object);
        auto state=static_cast<BYTE*>(body.state.object),root=static_cast<BYTE*>(body.root.object);
        if(rawInterface!=movement+0x140 || body.slot<1 || body.slot>=16 || !ChaserBodyWorldActive(body.world) ||
           !ChaserIdentityLive(body.worldIdentity) || !ChaserIdentityLive(body.pawn) || !ChaserIdentityLive(body.state) ||
           !ChaserIdentityLive(body.movement) || !ChaserIdentityLive(body.root) ||
           chaserDriverPawns[body.slot].object!=pawn || chaserDriverPawns[body.slot].serial!=body.pawn.serial ||
           chaserDriverStates[body.slot].object!=state || chaserDriverStates[body.slot].serial!=body.state.serial ||
           chaserDriverMovements[body.slot].object!=movement || chaserDriverMovements[body.slot].serial!=body.movement.serial ||
           !chaserDriverPrepared[body.slot] || !movement[0xed8] || *reinterpret_cast<void**>(state+0xa0)!=pawn ||
           *reinterpret_cast<int*>(state+0x168)!=body.slot || !state[0x321] || state[0x338]!=1 ||
           *reinterpret_cast<void**>(movement+0xa0)!=pawn || *reinterpret_cast<void**>(pawn+0x130)!=root ||
           *reinterpret_cast<void**>(root+0xa0)!=pawn || TwinKartLocalRace::FlagFind(pawn,0x23afeb0)!=state ||
           TwinKartLocalRace::FlagFind(pawn,0x22f9030)!=movement ||
           TwinKartObjective::NativeChaserRole(TwinKartLocalRace::FlagFind(pawn,0x2256c10)!=nullptr,
               TwinKartLocalRace::FlagFind(pawn,0x22570a0)!=nullptr)!=body.role)return false;
        // These are original IMovementInterface entries, with RCX already
        // adjusted by +0x140. Never call them with the component base pointer.
        auto table=*reinterpret_cast<BYTE***>(rawInterface);
        if(reinterpret_cast<void*>(table)!=image+0x5118b80 || table[0x140/8]!=image+0x15c3730 ||
           table[0x190/8]!=image+0x15c1fe0 || table[0x1a0/8]!=image+0x15c1f90)return false;
        if(!ChaserContactArray(lease.flags) || !ChaserContactArray(lease.points) || !ChaserContactArray(lease.components) ||
           lease.flags.data==lease.points.data || lease.flags.data==lease.components.data || lease.points.data==lease.components.data ||
           memcmp(movement+0x4e0,&lease.flags,sizeof(lease.flags)) || memcmp(movement+0x528,&lease.points,sizeof(lease.points)) ||
           memcmp(movement+0x548,&lease.components,sizeof(lease.components)))return false;
        auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);int count=*reinterpret_cast<int*>(image+0x69c28c4);
        for(auto component:{root,state,movement})if(!chaserRoleWorld.Matches(TwinKartLocalRace::World(component),chunks,count))return false;
        return *reinterpret_cast<void**>(body.world+0x1f0)==body.physicsScene &&
            !memcmp(root+0x1d0,&body.rotation,sizeof(body.rotation)) && !memcmp(root+0x1e0,&body.position,sizeof(body.position)) &&
            !memcmp(root+0x1f0,&body.scale,sizeof(body.scale));
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool ChaserSupportComponentValid(void* rawComponent,void* raw) {
    __try {
        auto& lease=*static_cast<ChaserGroundLease*>(raw);auto component=static_cast<BYTE*>(rawComponent);
        if(component==lease.body.root.object || !TwinKartLocalRace::Live(component) ||
           *reinterpret_cast<void**>(component+0xa0)==lease.body.pawn.object ||
           !TwinKartRace::ObjectiveAssetIsA(image,component,0x4512540))return false;
        auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);int count=*reinterpret_cast<int*>(image+0x69c28c4);
        TwinKartCamera::ObjectIdentity identity{};
        return TwinKartCamera::Capture(component,chunks,count,&identity) &&
            chaserRoleWorld.Matches(TwinKartLocalRace::World(component),chunks,count);
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool CaptureChaserGround(BYTE* world,int slot,ChaserGroundLease& outputLease) {
    __try {
        ChaserGroundLease lease;if(!CaptureChaserBody(world,slot,lease.body))return false;
        auto movement=static_cast<BYTE*>(lease.body.movement.object);auto interface=movement+0x140;
        memcpy(&lease.flags,movement+0x4e0,sizeof(lease.flags));memcpy(&lease.points,movement+0x528,sizeof(lease.points));
        memcpy(&lease.components,movement+0x548,sizeof(lease.components));
        TwinKartRace::KartContactsApi api{reinterpret_cast<TwinKartRace::AllKartTiresContact>(image+0x15c3730),
            reinterpret_cast<TwinKartRace::KartWheelLocation>(image+0x15c1fe0),
            reinterpret_cast<TwinKartRace::KartWheelComponent>(image+0x15c1f90)};
        if(!TwinKartRace::ReadKartContacts(interface,api,ChaserContactsValid,ChaserSupportComponentValid,&lease,lease.contacts))return false;
        auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);int count=*reinterpret_cast<int*>(image+0x69c28c4);
        for(unsigned wheel=0;wheel<4;++wheel)
            if(!TwinKartCamera::Capture(lease.contacts.wheels[wheel].component,chunks,count,&lease.support[wheel]))return false;
        if(!TwinKartRace::CalibrateKartRoad(lease.contacts,lease.body.position,lease.body.rotation,lease.calibration) ||
           !ChaserContactsValid(interface,&lease) || !ValidateChaserBody(world,lease.body.root.object,lease.body.pawn.object,&lease.body))return false;
        for(unsigned wheel=0;wheel<4;++wheel)if(!ChaserIdentityLive(lease.support[wheel]) ||
            !ChaserSupportComponentValid(lease.support[wheel].object,&lease))return false;
        outputLease=lease;return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool ValidateChaserGround(ChaserGroundLease& lease) {
    __try {
        auto interface=static_cast<BYTE*>(lease.body.movement.object)+0x140;
        if(!ChaserContactsValid(interface,&lease))return false;
        TwinKartRace::KartContactsApi api{reinterpret_cast<TwinKartRace::AllKartTiresContact>(image+0x15c3730),
            reinterpret_cast<TwinKartRace::KartWheelLocation>(image+0x15c1fe0),
            reinterpret_cast<TwinKartRace::KartWheelComponent>(image+0x15c1f90)};
        TwinKartRace::KartContactSnapshot current;
        if(!TwinKartRace::ReadKartContacts(interface,api,ChaserContactsValid,ChaserSupportComponentValid,&lease,current))return false;
        for(unsigned wheel=0;wheel<4;++wheel) {
            if(!ChaserIdentityLive(lease.support[wheel]) || current.wheels[wheel].component!=lease.support[wheel].object ||
               memcmp(&current.wheels[wheel].point,&lease.contacts.wheels[wheel].point,sizeof(TwinKartRace::ArenaPoint)))return false;
        }
        return ChaserContactsValid(interface,&lease) &&
            ValidateChaserBody(lease.body.world,lease.body.root.object,lease.body.pawn.object,&lease.body);
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool ChaserPathClear(const TwinKartRace::ArenaPoint& a,const TwinKartRace::ArenaPoint& b,void* raw) {
    auto world=static_cast<BYTE*>(raw);
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);int count=*reinterpret_cast<int*>(image+0x69c28c4);
    if(GetCurrentThreadId()!=queue.ThreadId() || chaserObjectQueryCount!=3 || !chaserRoleWorld.Matches(world,chunks,count))return false;
    TwinKartRace::QueryArray types{chaserObjectQueries,3,3},ignored{};
    auto start=a,end=b;start.z+=80;end.z+=80;
    return TwinKartRace::QueryLineClear(world,reinterpret_cast<TwinKartRace::ObjectTrace>(image+0x3ff5990),types,ignored,start,end);
}
struct ChaserDriverQuery {BYTE* world=nullptr;int slot=-1;const ChaserPursuitTarget* target=nullptr;};
bool ChaserDriverPathClear(const TwinKartRace::ArenaPoint& a,const TwinKartRace::ArenaPoint& b,void* raw) {
    auto& query=*static_cast<ChaserDriverQuery*>(raw);
    if(query.slot<1 || query.slot>=16 || !ChaserBodyWorldActive(query.world) || chaserObjectQueryCount!=3 ||
       !ChaserIdentityLive(chaserDriverPawns[query.slot]) || !ChaserIdentityLive(chaserDriverStates[query.slot]) ||
       !ChaserIdentityLive(chaserDriverMovements[query.slot]) || !chaserDriverPrepared[query.slot])return false;
    auto identity=chaserDriverPawns[query.slot],stateId=chaserDriverStates[query.slot],movementId=chaserDriverMovements[query.slot];
    auto state=static_cast<BYTE*>(stateId.object),movement=static_cast<BYTE*>(movementId.object);
    if(*reinterpret_cast<void**>(state+0xa0)!=identity.object || *reinterpret_cast<int*>(state+0x168)!=query.slot ||
       !state[0x321] || state[0x338]!=1 || *reinterpret_cast<void**>(movement+0xa0)!=identity.object || !movement[0xed8])return false;
    if(query.target && (query.target->world!=query.world || query.target->driver!=query.slot ||
       !CurrentChaserPursuitTarget(*query.target)))return false;
    void* ignoredPawns[2]{identity.object,query.target?query.target->pawn.object:nullptr};int ignoredCount=query.target?2:1;
    TwinKartRace::QueryArray types{chaserObjectQueries,3,3},ignored{ignoredPawns,ignoredCount,ignoredCount};
    auto start=a,end=b;start.z+=80;end.z+=80;
    bool clear=TwinKartRace::QueryLineClear(query.world,reinterpret_cast<TwinKartRace::ObjectTrace>(image+0x3ff5990),types,ignored,start,end);
    return clear && ChaserIdentityLive(identity) && chaserDriverPawns[query.slot].object==identity.object &&
        chaserDriverPawns[query.slot].serial==identity.serial && ChaserIdentityLive(stateId) && ChaserIdentityLive(movementId) &&
        chaserDriverStates[query.slot].object==stateId.object && chaserDriverStates[query.slot].serial==stateId.serial &&
        chaserDriverMovements[query.slot].object==movementId.object && chaserDriverMovements[query.slot].serial==movementId.serial &&
        TwinKartLocalRace::FlagFind(static_cast<BYTE*>(identity.object),0x23afeb0)==state &&
        TwinKartLocalRace::FlagFind(static_cast<BYTE*>(identity.object),0x22f9030)==movement &&
        state[0x338]==1 && state[0x321] && movement[0xed8] && ChaserBodyWorldActive(query.world) &&
        (!query.target || CurrentChaserPursuitTarget(*query.target));
}
TwinKartRace::RoadSurfaceResult ChaserReadSupport(const TwinKartRace::ArenaPoint& start,
    const TwinKartRace::ArenaPoint& end,TwinKartRace::RoadSurface& contact,void* raw);
struct ChaserTranslationContext {ChaserGroundLease* lease=nullptr;bool approach=false;};
bool ChaserTranslationAlive(void* raw) {
    auto& context=*static_cast<ChaserTranslationContext*>(raw);
    if(!context.lease)return false;
    auto& lease=*context.lease;
    if(!ChaserContactsValid(static_cast<BYTE*>(lease.body.movement.object)+0x140,&lease))return false;
    for(const auto& support:lease.support)if(!ChaserIdentityLive(support) || !ChaserSupportComponentValid(support.object,&lease))return false;
    return !lease.body.planningTarget || (lease.body.planningTarget->world==lease.body.world &&
        lease.body.planningTarget->driver==lease.body.slot && CurrentChaserPursuitTarget(*lease.body.planningTarget));
}
TwinKartRace::RoadSurfaceResult ChaserTranslationSurface(const TwinKartRace::ArenaPoint& first,
    const TwinKartRace::ArenaPoint& last,TwinKartRace::RoadSurface& surface,void* raw) {
    auto& context=*static_cast<ChaserTranslationContext*>(raw);
    return ChaserTranslationAlive(raw)?ChaserReadSupport(first,last,surface,context.lease->body.world):TwinKartRace::RoadSurfaceResult::Invalid;
}
TwinKartRace::VolumeSweep ChaserTranslationBody(const TwinKartRace::ArenaPoint& first,
    const TwinKartRace::ArenaPoint& last,const TwinKartRace::ComponentRotation& rotation,void* raw) {
    auto& context=*static_cast<ChaserTranslationContext*>(raw);
    if(!ChaserTranslationAlive(raw))return {};
    return QueryChaserBody(context.lease->body,first,last,rotation,context.approach);
}
bool ChaserRoadPoint(const TwinKartRace::ArenaPoint& point,ChaserDriverQuery& query,int region,TwinKartRace::ArenaPoint& outputPoint) {
    TwinKartRace::ArenaPoint anchor{};
    if(region<0 || !chaserGround.AnchorAt(point,anchor,ChaserDriverPathClear,&query) ||
       chaserGround.RegionAt(anchor,ChaserDriverPathClear,&query)!=region)return false;
    TwinKartRace::RoadSurface road;
    if(ChaserReadSupport({point.x,point.y,anchor.z+72},{point.x,point.y,anchor.z-72},road,query.world)!=TwinKartRace::RoadSurfaceResult::Hit ||
       road.penetrating || road.normalZ<=0.70f || std::fabs(road.point.z-anchor.z)>45)return false;
    outputPoint=road.point;return true;
}
TwinKartRace::VolumeSweep ChaserImmediateBodyTranslation(const TwinKartRace::ArenaPoint& first,
    const TwinKartRace::ArenaPoint& target,ChaserDriverQuery& query,bool approach=true) {
    __try {
        if(query.slot<1 || query.slot>=16 || !ChaserIdentityLive(chaserNavigationWorld) ||
           chaserNavigationWorld.object!=query.world || !chaserGround.Verified())return {};
        ChaserGroundLease lease;if(!CaptureChaserGround(query.world,query.slot,lease))return {};
        const auto& body=lease.body;
        if(std::fabs(first.x-body.position.x)>0.01f || std::fabs(first.y-body.position.y)>0.01f ||
           std::fabs(first.z-body.position.z)>0.01f)return {};
        TwinKartRace::ArenaPoint anchor{};
        if(!chaserGround.AnchorAt(target,anchor,ChaserDriverPathClear,&query))return {};
        int region=chaserGround.RegionAt(lease.calibration.roadOrigin,ChaserDriverPathClear,&query);
        if(region<0 || chaserGround.RegionAt(anchor,ChaserDriverPathClear,&query)!=region)return {};
        // The destination may be a road sample OR an original objective/pawn
        // pivot. Take its layer from the verified graph, then query the actual
        // road at its XY. Do not interpret an actor's Z as a kart floor height.
        TwinKartRace::ArenaPoint start{target.x,target.y,anchor.z+72},end{target.x,target.y,anchor.z-72};
        TwinKartRace::RoadSurface road;
        if(ChaserReadSupport(start,end,road,query.world)!=TwinKartRace::RoadSurfaceResult::Hit || road.penetrating ||
           road.normalZ<=0.70f || std::fabs(road.point.z-anchor.z)>45 || !ValidateChaserGround(lease))return {};
        ChaserTranslationContext context{&lease,approach};
        auto result=TwinKartRace::QueryKartTranslationEdge(lease.calibration,body.rotation,
            lease.calibration.roadOrigin,road.point,ChaserTranslationSurface,ChaserTranslationBody,ChaserTranslationAlive,&context);
        if(!ValidateChaserGround(lease))return {};
        return result;
    } __except(EXCEPTION_EXECUTE_HANDLER){return {};}
}
bool ChaserImmediatePathClear(const TwinKartRace::ArenaPoint& a,const TwinKartRace::ArenaPoint& b,void* raw) {
    auto query=*static_cast<ChaserDriverQuery*>(raw);query.target=nullptr;
    if(query.slot<1 || query.slot>=16)return false;
    int selected=chaserDrivers[query.slot].TargetSlot();ChaserPursuitTarget target;
    bool targetReady=selected<0 || CaptureChaserPursuitTarget(query.world,query.slot,selected,target);
    if(selected>=0 && targetReady)query.target=&target;
    bool clear=targetReady && ChaserDriverPathClear(a,b,&query);
    if(clear) {
        auto result=ChaserImmediateBodyTranslation(a,b,query);
        clear=result.result==TwinKartRace::VolumeSweepResult::Clear ||
            (result.result==TwinKartRace::VolumeSweepResult::Blocked && result.onlyApproachContacts);
        static ULONGLONG reportAfter[16]{};auto now=GetTickCount64();
        if(query.slot>0 && query.slot<16 && now>=reportAfter[query.slot]) {
            reportAfter[query.slot]=now+5000;
            RuntimeEvent("native_chaser_immediate_body_translation",query.slot,static_cast<int>(result.result));
            if(result.onlyApproachContacts)RuntimeEvent("native_chaser_original_target_contact_approach",query.slot,static_cast<int>(result.approachContacts));
        }
    }
    // A newly blocked immediate segment invalidates the old future-edge
    // observations before the next plan, rather than choosing that path again.
    if(clear && selected>=0)clear=selected==chaserDrivers[query.slot].TargetSlot() && ChaserPursuitRosterCurrent(target);
    if(!clear && query.slot>0 && query.slot<16)chaserEdgeQueries[query.slot].Invalidate();
    return clear;
}
bool ChaserReversePathClear(const TwinKartRace::ArenaPoint& a,const TwinKartRace::ArenaPoint& b,void* raw) {
    auto query=*static_cast<ChaserDriverQuery*>(raw);query.target=nullptr;
    if(query.slot<1 || query.slot>=16 || !ChaserDriverPathClear(a,b,&query))return false;
    // No leased prey is ignored and no approach-contact exception applies.
    // The existing serial body, support, world and full road checks remain.
    auto result=ChaserImmediateBodyTranslation(a,b,query,false);
    bool clear=result.result==TwinKartRace::VolumeSweepResult::Clear;
    if(!clear)chaserEdgeQueries[query.slot].Invalidate();
    return clear;
}
struct ChaserPlanningRoad {
    const TwinKartRace::LayeredRoadNavigation& road;mutable ChaserDriverQuery query;
    mutable ChaserGroundLease lease{};mutable bool qualified=false,preparationAttempted=false;mutable int region=-1;
    mutable bool pathActive=false;
    bool Ready()const{return road.Ready();}size_t Count()const{return road.Count();}
    int RegionAt(const TwinKartRace::ArenaPoint& p,TwinKartObjective::ChaserLine line,void* context)const{return road.RegionAt(p,line,context);}
    bool ReachableSampleAt(size_t at,int wantedRegion,TwinKartRace::ArenaPoint& p,TwinKartObjective::ChaserLine line,void* context)const {
        return road.ReachableSampleAt(at,wantedRegion,p,line,context);
    }
    static TwinKartRace::RoadEdgeObservation ReadBodyEdge(const TwinKartRace::ArenaPoint& a,const TwinKartRace::ArenaPoint& b,void* raw) {
        using Result=TwinKartRace::RoadEdgeObservation;
        auto& self=*static_cast<ChaserPlanningRoad*>(raw);
        ChaserTranslationContext context{&self.lease,self.lease.body.planningTarget!=nullptr};
        if(!self.qualified || !ChaserTranslationAlive(&context))return Result::Invalid;
        TwinKartRace::ArenaPoint first{},last{};
        if(!ChaserRoadPoint(a,self.query,self.region,first) || !ChaserRoadPoint(b,self.query,self.region,last))
            return ChaserTranslationAlive(&context)?Result::Blocked:Result::Invalid;
        // A root-to-road connector can have identical mapped road endpoints.
        // That is no translation; actual movement still has its fresh gate.
        if(TwinKartRace::ArenaDistance(first,last)<0.01f)return ChaserTranslationAlive(&context)?Result::Clear:Result::Invalid;
        auto result=TwinKartRace::QueryKartTranslationEdge(self.lease.calibration,self.lease.body.rotation,first,last,
            ChaserTranslationSurface,ChaserTranslationBody,ChaserTranslationAlive,&context);
        if(!ValidateChaserGround(self.lease) || !ChaserTranslationAlive(&context))return Result::Invalid;
        if(result.result==TwinKartRace::VolumeSweepResult::Clear)return Result::Clear;
        if(result.result==TwinKartRace::VolumeSweepResult::Blocked)
            return result.onlyApproachContacts?Result::Approach:Result::Blocked;
        return Result::Invalid;
    }
    static bool Traverse(const TwinKartRace::ArenaPoint& a,const TwinKartRace::ArenaPoint& b,void* raw) {
        using Result=TwinKartRace::RoadEdgeObservation;
        auto& self=*static_cast<ChaserPlanningRoad*>(raw);ChaserTranslationContext context{&self.lease,false};
        if(!self.qualified || !ChaserTranslationAlive(&context))return false;
        auto target=self.lease.body.planningTarget;
        auto result=chaserEdgeQueries[self.query.slot].Observe(a,b,ReadBodyEdge,&self,target?target->Key():0);
        return (result==Result::Clear || result==Result::Approach) && ChaserTranslationAlive(&context);
    }
    bool Qualify()const {
        // One stack-scoped planner turn may compare several targets. Reuse
        // its captured lease only within that turn, validating it again;
        // do not repeat native shape/contact capture for every candidate.
        if(preparationAttempted)return qualified && ValidateChaserGround(lease);
        preparationAttempted=true;
        qualified=false;
        if(query.slot<1 || query.slot>=16 || !CaptureChaserGround(query.world,query.slot,lease))return false;
        auto copy=query;region=road.RegionAt(lease.calibration.roadOrigin,ChaserDriverPathClear,&copy);if(region<0)return false;
        std::array<unsigned char,256> signature{};size_t cursor=0;
        auto append=[&](const auto& value){if(cursor+sizeof(value)>signature.size())return false;
            memcpy(signature.data()+cursor,&value,sizeof(value));cursor+=sizeof(value);return true;};
        // Serialize individual scalar/vector fields; no object padding or hash
        // equivalence. A new body, model, scale, attitude, support or calibration
        // must not reuse old clearance observations from the same pawn serial.
        const auto& body=lease.body;
        for(const auto& identity:{body.worldIdentity,body.pawn,body.state,body.movement,body.root,
                                 lease.support[0],lease.support[1],lease.support[2],lease.support[3]})
            if(!append(identity.index) || !append(identity.serial))return false;
        if(!append(body.body) || !append(body.actorHandle) || !append(body.physicsScene) || !append(body.actorScene) ||
           !append(body.shapeSignature) || !append(body.shapeCount) || !append(body.rotation) || !append(body.scale) ||
           !append(lease.calibration.bodyOffset) || !append(lease.calibration.bodyRelativeRotation) ||
           !append(lease.calibration.forward) || !append(lease.calibration.roadNormal) || !append(region) ||
           !ValidateChaserGround(lease) || !chaserEdgeQueries[query.slot].Qualify(signature))return false;
        qualified=true;return true;
    }
    bool FindPath(const TwinKartRace::ArenaPoint& a,const TwinKartRace::ArenaPoint& b,int targetSlot,
        std::vector<TwinKartRace::ArenaPoint>& route,TwinKartObjective::ChaserLine line,void* context)const {
        route.clear();if(pathActive || line!=ChaserDriverPathClear || context!=&query)return false;
        pathActive=true;auto previousQuery=query.target;auto previousBody=lease.body.planningTarget;
        ChaserPursuitTarget target;bool accepted=false;
        __try {
            if(!Qualify())return false;
            if(targetSlot>=0) {
                TwinKartRace::ArenaPoint position{};
                if(lease.body.role!=TwinKartObjective::ChaserRole::Tagger ||
                   !CaptureChaserPursuitTarget(query.world,query.slot,targetSlot,target) ||
                   !TwinKartLocalRace::FlagPosition(static_cast<BYTE*>(target.pawn.object),position) ||
                   TwinKartRace::ArenaDistance(position,b)>500 || std::fabs(position.z-b.z)>200)return false;
                query.target=&target;lease.body.planningTarget=&target;
            } else {query.target=nullptr;lease.body.planningTarget=nullptr;}
            bool result=road.Path(a,b,route,line,context,Traverse,const_cast<ChaserPlanningRoad*>(this));
            if(!ValidateChaserGround(lease) || (targetSlot>=0 && !ChaserPursuitRosterCurrent(target))){route.clear();return false;}
            accepted=result;return accepted;
        } __finally {if(!accepted)route.clear();query.target=previousQuery;lease.body.planningTarget=previousBody;pathActive=false;}
    }
    bool Path(const TwinKartRace::ArenaPoint& a,const TwinKartRace::ArenaPoint& b,std::vector<TwinKartRace::ArenaPoint>& route,
              TwinKartObjective::ChaserLine line,void* context)const {
        return FindPath(a,b,-1,route,line,context);
    }
    bool PathToTarget(const TwinKartRace::ArenaPoint& a,const TwinKartRace::ArenaPoint& b,int target,
        std::vector<TwinKartRace::ArenaPoint>& route,TwinKartObjective::ChaserLine line,void* context)const {
        if(target<0 || target>=16){route.clear();return false;}
        return FindPath(a,b,target,route,line,context);
    }
};
TwinKartRace::RoadSurfaceResult ChaserReadSupport(const TwinKartRace::ArenaPoint& start,
    const TwinKartRace::ArenaPoint& end,TwinKartRace::RoadSurface& contact,void* raw) {
    if(!OwnedChaserWorld(static_cast<BYTE*>(raw)))return TwinKartRace::RoadSurfaceResult::Invalid;
    return TwinKartRace::QueryRoadSurface(raw,reinterpret_cast<TwinKartRace::RoadTrace>(image+0x3de0910),
        image+0x6b01810,start,end,contact);
}
// Native gameplay control uses the verified layered road graph and fresh
// native road/obstacle queries. Kart bodies keep their original collision and
// movement solver; they are not moved by this adapter. The separate private
// body-volume experiments above are retained for diagnostics, not a mandatory
// condition for an ordinary throttle/steering request.
struct ChaserRoadPlanning {
    const TwinKartRace::LayeredRoadNavigation& road;mutable ChaserDriverQuery query;
    bool Ready()const{return road.Verified();}size_t Count()const{return road.Count();}
    int RegionAt(const TwinKartRace::ArenaPoint& p,TwinKartObjective::ChaserLine line,void* raw)const{return road.RegionAt(p,line,raw);}
    bool ReachableSampleAt(size_t at,int region,TwinKartRace::ArenaPoint& p,TwinKartObjective::ChaserLine line,void* raw)const {
        return road.ReachableSampleAt(at,region,p,line,raw);
    }
    bool Path(const TwinKartRace::ArenaPoint& first,const TwinKartRace::ArenaPoint& last,
        std::vector<TwinKartRace::ArenaPoint>& route,TwinKartObjective::ChaserLine line,void* raw)const {
        route.clear();if(line!=ChaserDriverPathClear || raw!=&query || !Ready())return false;
        return road.Path(first,last,route,line,raw);
    }
    bool PathToTarget(const TwinKartRace::ArenaPoint& first,const TwinKartRace::ArenaPoint& last,int target,
        std::vector<TwinKartRace::ArenaPoint>& route,TwinKartObjective::ChaserLine line,void* raw)const {
        route.clear();if(query.target || line!=ChaserDriverPathClear || raw!=&query || !Ready())return false;
        ChaserPursuitTarget lease;if(!CaptureChaserPursuitTarget(query.world,query.slot,target,lease))return false;
        query.target=&lease;bool result=false;
        __try {result=road.Path(first,last,route,line,raw) && ChaserPursuitRosterCurrent(lease);return result;}
        __finally {query.target=nullptr;if(!result)route.clear();}
    }
};
bool ChaserImmediateRoadControl(const TwinKartRace::ArenaPoint& first,const TwinKartRace::ArenaPoint& last,
    void* raw,bool approach) {
    auto query=*static_cast<ChaserDriverQuery*>(raw);
    if(query.slot<1 || query.slot>=16 || !chaserGround.Verified())return false;
    ChaserPursuitTarget target;int selected=approach?chaserDrivers[query.slot].TargetSlot():-1;
    if(selected>=0) {if(!CaptureChaserPursuitTarget(query.world,query.slot,selected,target))return false;query.target=&target;}
    TwinKartRace::ArenaPoint a{},b{};
    if(!chaserGround.AnchorAt(first,a,ChaserDriverPathClear,&query) ||
       !chaserGround.AnchorAt(last,b,ChaserDriverPathClear,&query))return false;
    int region=chaserGround.RegionAt(a,ChaserDriverPathClear,&query);
    if(region<0 || chaserGround.RegionAt(b,ChaserDriverPathClear,&query)!=region ||
       !ChaserDriverPathClear(first,last,&query))return false;
    TwinKartRace::RoadSurface initial{},final{};
    if(ChaserReadSupport({first.x,first.y,a.z+72},{first.x,first.y,a.z-72},initial,query.world)!=TwinKartRace::RoadSurfaceResult::Hit ||
       ChaserReadSupport({last.x,last.y,b.z+72},{last.x,last.y,b.z-72},final,query.world)!=TwinKartRace::RoadSurfaceResult::Hit ||
       initial.penetrating || final.penetrating || initial.normalZ<=.70f || final.normalZ<=.70f)return false;
    bool supported=false;
    if(TwinKartRace::ArenaDistance(initial.point,final.point)<1)supported=true;
    else {
        TwinKartRace::RoadSegmentSupport support;
        if(!support.Begin(initial.point,final.point))return false;
        support.Step(256,ChaserReadSupport,query.world);
        supported=support.State()==TwinKartRace::SupportState::Supported;
    }
    return supported && ChaserDriverPathClear(first,last,&query) &&
        (selected<0 || (selected==chaserDrivers[query.slot].TargetSlot() && ChaserPursuitRosterCurrent(target)));
}
bool ChaserImmediateRoadForward(const TwinKartRace::ArenaPoint& a,const TwinKartRace::ArenaPoint& b,void* raw) {
    return ChaserImmediateRoadControl(a,b,raw,true);
}
bool ChaserImmediateRoadReverse(const TwinKartRace::ArenaPoint& a,const TwinKartRace::ArenaPoint& b,void* raw) {
    return ChaserImmediateRoadControl(a,b,raw,false);
}
bool BuildChaserNavigation(BYTE* world,bool pregame) {
    using State=TwinKartNavigation::BoundsState;using Type=void*(__fastcall*)();
    // Spend a bounded loading-screen slice on real queries, rather than
    // throttling a whole arena to the per-driving-frame query allowance.
    // Every surface and transition is still independently checked.
    auto deadline=GetTickCount64()+4;
    if(chaserNavigationRejected)return false;
    if(!chaserNavigationWorld.object && !TwinKartLocalRace::CaptureFlagObject(world,chaserNavigationWorld))return false;
    if(chaserBounds.State()==State::Idle) {
        auto actorClass=reinterpret_cast<Type>(image+0x43d82e0)();
        if(!TwinKartLocalRace::Live(static_cast<BYTE*>(actorClass)) || !chaserBounds.Begin(world,actorClass,
            reinterpret_cast<TwinKartNavigation::CollectActors>(image+0x3f41d30),ReadChaserGeometryBounds,
            reinterpret_cast<TwinKartNavigation::ReleaseActors>(image+0x248b950)))chaserNavigationRejected=true;
    }
    if(chaserBounds.State()==State::Reading)chaserBounds.Step(world,128);
    if(chaserBounds.State()==State::Rejected){chaserNavigationRejected=true;RuntimeEvent("native_chaser_scene_bounds_rejected",-1,0);return false;}
    if(chaserBounds.State()!=State::Ready)return false;
    if(!chaserObjectQueryCount) {
        using Channel=unsigned char(__fastcall*)(int);unsigned channels=0;
        for(unsigned char query=0;query<32 && chaserObjectQueryCount<3;++query) {
            auto channel=reinterpret_cast<Channel>(image+0x3f159a0)(query);
            if((channel==0 || channel==1 || channel==17) && !(channels&(1u<<channel))) {
                channels|=1u<<channel;chaserObjectQueries[chaserObjectQueryCount++]=query;
            }
        }
        if(chaserObjectQueryCount!=3){chaserObjectQueryCount=0;return false;}
    }
    if(!chaserGround.Started()) {
        TwinKartRace::ArenaPoint low{},high{};if(!chaserBounds.Result(low,high))return false;
        chaserRoadTop=high.z+2000;chaserRoadBottom=low.z-2000;
        low.x-=400;low.y-=400;high.x+=400;high.y+=400;
        low.z=chaserRoadBottom;high.z=chaserRoadTop;
        bool built=false;for(float spacing=400;spacing<=1000 && !built;spacing+=200)built=chaserGround.Initialize(low,high,spacing);
        if(!built){chaserNavigationRejected=true;RuntimeEvent("native_chaser_scene_navigation_bounds_rejected",chaserBounds.Accepted(),0);return false;}
        chaserGround.AllowSmoothing(false);
        RuntimeEvent("native_chaser_loaded_collision_bounds_collected",chaserBounds.Accepted(),static_cast<int>(chaserGround.ColumnCount()));
    }
    for(int i=0;i<(pregame?1024:128) && !chaserGround.Ready() && (!pregame || GetTickCount64()<deadline);++i) {
        TwinKartRace::ArenaPoint start{},end{};TwinKartRace::RoadSurface hit{};if(!chaserGround.NextRay(start,end))break;
        auto result=TwinKartRace::QueryRoadSurface(world,reinterpret_cast<TwinKartRace::RoadTrace>(image+0x3de0910),image+0x6b01810,start,end,hit);
        chaserGround.Sample(result,hit);
    }
    if(chaserGround.Rejected()){chaserNavigationRejected=true;RuntimeEvent("native_chaser_layered_road_samples_rejected",-1,chaserGround.Rejection());return false;}
    chaserGround.BuildEdges(pregame?256:128);
    if(chaserGround.Rejected()){chaserNavigationRejected=true;RuntimeEvent("native_chaser_layered_road_edges_rejected",-1,chaserGround.Rejection());return false;}
    static ULONGLONG nextProgress=0;auto now=GetTickCount64();
    if(now>=nextProgress){nextProgress=now+5000;
        RuntimeEvent("native_chaser_road_sampling_progress",static_cast<int>(chaserGround.SampledColumnCount()),static_cast<int>(chaserGround.ColumnCount()));
        RuntimeEvent("native_chaser_road_edge_progress",static_cast<int>(chaserAdjacency),static_cast<int>(chaserGround.PendingEdgeCount()));}
    if(!chaserGround.Ready())return false;
    if(chaserGround.ValidCount()<2) {
        chaserNavigationRejected=true;RuntimeEvent("native_chaser_scene_has_no_routable_roads",-1,0);return false;
    }
    unsigned supportBudget=pregame?1024:64;
    for(int i=0;i<(pregame?512:192) && supportBudget && chaserAdjacency<chaserGround.AdjacencyCount() &&
        (!pregame || GetTickCount64()<deadline);++i) {
        int a=0,b=0;TwinKartRace::ArenaPoint start{},end{};
        if(!chaserGround.AdjacencyAt(chaserAdjacency,a,b,start,end)) {
            chaserNavigationRejected=true;return false;
        }
        if(chaserSupport.State()==TwinKartRace::SupportState::Idle)chaserSupport.Begin(start,end);
        supportBudget-=chaserSupport.Step((std::min)(supportBudget,256u),ChaserReadSupport,world);
        if(chaserSupport.State()==TwinKartRace::SupportState::Reading)break;
        bool supported=chaserSupport.State()==TwinKartRace::SupportState::Supported;
        chaserGround.ResolveTransition(chaserAdjacency,supported && ChaserPathClear(start,end,world));
        chaserSupport.Reset();++chaserAdjacency;
    }
    bool ready=chaserAdjacency==chaserGround.AdjacencyCount() && chaserGround.Verified();
    if(ready && !chaserNavigationReported){chaserNavigationReported=true;RuntimeEvent("native_chaser_layered_road_graph_ready",
        static_cast<int>(chaserGround.ValidCount()),static_cast<int>(chaserGround.ColumnCount()));}
    return ready;
}
bool PauseChaserComponent(BYTE* pawn,unsigned getter) {
    auto component=TwinKartLocalRace::FlagFind(pawn,getter);if(!component)return true;
    auto table=*reinterpret_cast<BYTE***>(component);
    if(table[0x358/8]!=image+0x3de6fb0 || table[0x368/8]!=image+0x3de02a0)return false;
    using Enabled=bool(__fastcall*)(void*);using Tick=void(__fastcall*)(void*,bool);
    if(reinterpret_cast<Enabled>(image+0x3de02a0)(component))reinterpret_cast<Tick>(image+0x3de6fb0)(component,false);
    return !reinterpret_cast<Enabled>(image+0x3de02a0)(component);
}
bool PrepareChaserMovement(BYTE* pawn,BYTE* movement,int slot,int& stage) {
    stage=1;
    if(!TwinKartLocalRace::PrepareChaserMovementMode(pawn,movement,slot))return false;
    stage=2;
    auto path=*reinterpret_cast<BYTE**>(movement+0xf08);
    if(path) {
        using Type=void*(__fastcall*)();
        if(!TwinKartLocalRace::OwnedComponent(path,pawn,reinterpret_cast<Type>(image+0x22b1e10)()) || !PauseChaserComponent(pawn,0x22b1e10))return false;
        reinterpret_cast<void(__fastcall*)(void*,void*)>(image+0x15d1100)(movement,nullptr);
        if(*reinterpret_cast<void**>(movement+0xf08))return false;
    }
    // Preserve components, original kart parameters and physics. Only the
    // standalone circuit controller/smoothing ticks must stop writing this AI.
    stage=3;if(!PauseChaserComponent(pawn,0x2218050))return false;
    stage=4;if(!PauseChaserComponent(pawn,0x2218390))return false;
    stage=5;if(!PauseChaserComponent(pawn,0x2402870))return false;
    stage=6;if(!TwinKartLocalRace::autoDrive)return false;
    TwinKartLocalRace::autoDrive(movement,false);if(movement[0xedd])return false;
    stage=0;return true;
}
bool WriteChaserControl(BYTE* world,int slot,const TwinKartRace::ArenaControl& control) {
    __try {
        if(slot<1 || slot>=16 || !OwnedChaserWorld(world) || !std::isfinite(control.throttle) || !std::isfinite(control.steer) ||
           std::fabs(control.throttle)>1 || std::fabs(control.steer)>1)return false;
        auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);int count=*reinterpret_cast<int*>(image+0x69c28c4);
        if(!TwinKartCamera::Live(chaserDriverPawns[slot],chunks,count) || !TwinKartCamera::Live(chaserDriverStates[slot],chunks,count) ||
           !TwinKartCamera::Live(chaserDriverMovements[slot],chunks,count))return false;
        auto pawn=static_cast<BYTE*>(chaserDriverPawns[slot].object),state=static_cast<BYTE*>(chaserDriverStates[slot].object);
        auto movement=static_cast<BYTE*>(chaserDriverMovements[slot].object);
        if(*reinterpret_cast<void**>(state+0xa0)!=pawn || *reinterpret_cast<int*>(state+0x168)!=slot || !state[0x321] ||
           *reinterpret_cast<void**>(movement+0xa0)!=pawn || !chaserRoleWorld.Matches(TwinKartLocalRace::World(movement),chunks,count))return false;
        auto safe=control;bool accepted=true;
        if(control.throttle || control.steer || control.reverse) {
            unsigned long long phase=0;auto game=*reinterpret_cast<unsigned long long*>(image+TwinKartGraphics::ChaserGameNameRva);
            if(state[0x338]!=1 || !game || !ChaserPhase(world,phase) || phase!=game ||
               TwinKartObjective::NativeChaserRole(TwinKartLocalRace::FlagFind(pawn,0x2256c10)!=nullptr,
                   TwinKartLocalRace::FlagFind(pawn,0x22570a0)!=nullptr)==TwinKartObjective::ChaserRole::Unknown){safe={};accepted=false;}
        }
        using Input=void(__fastcall*)(void*,float);
        reinterpret_cast<Input>(image+0x15d4320)(movement,TwinKartRace::NativeKartThrottle(safe));
        reinterpret_cast<Input>(image+0x15d3fd0)(movement,safe.steer);return accepted;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
void PublishChaserHud(BYTE* world,const TwinKartObjective::ChaserObservation* observations,
                      const TwinKartCamera::ObjectIdentity* pawns,int count) {
    LONG packet=0;
    __try {
        if(!OwnedChaserWorld(world) || !pawns || !observations || count<2 || count>16 ||
           !ChaserIdentityLive(pawns[0]))return;
        auto pawn=static_cast<BYTE*>(pawns[0].object);
        const auto role=observations[0].role;
        if(role==TwinKartObjective::ChaserRole::Unknown)return;
        auto hud=TwinKartLocalRace::FlagFind(pawn,0x22c2670);
        if(TwinKartLocalRace::Live(hud) && *reinterpret_cast<BYTE**>(hud)==image+0x51092c0) {
            auto root=*reinterpret_cast<BYTE**>(hud+0x110);
            unsigned long long slot=0;
            reinterpret_cast<void(__fastcall*)(void*,const wchar_t*,int)>(image+0x2589130)(&slot,L"AddtionalOverlay_Slot",1);
            if(TwinKartLocalRace::Live(root) && slot && reinterpret_cast<bool(__fastcall*)(void*)>(image+0x3a73d20)(root) &&
               TwinKartLocalRace::Live(reinterpret_cast<BYTE*(__fastcall*)(void*,unsigned long long)>(image+0x192aa50)(hud,slot)))return;
        }
        int taggers=0,runaways=0;
        for(int i=0;i<count;++i) {
            taggers+=observations[i].role==TwinKartObjective::ChaserRole::Tagger;
            runaways+=observations[i].role==TwinKartObjective::ChaserRole::Runaway;
        }
        if(taggers+runaways!=count)return;
        int time=1023;
        auto gameState=*reinterpret_cast<BYTE**>(world+0x120);
        using Type=void*(__fastcall*)();
        auto state=TwinKartLocalRace::Live(gameState)?TwinKartLocalRace::FlagFind(gameState,0x2250ff0):nullptr;
        if(TwinKartLocalRace::OwnedComponent(state,gameState,reinterpret_cast<Type>(image+0x2250ff0)())) {
            const float now=*reinterpret_cast<float*>(world+0x5a0);
            const float start=*reinterpret_cast<float*>(state+0x20c);
            const float end=*reinterpret_cast<float*>(state+0x210);
            if(std::isfinite(now) && std::isfinite(start) && std::isfinite(end) &&
               now>=0.f && start>0.f && end>start && now>=start-1.f && end-now<=600.f)
                time=static_cast<int>(std::ceil((std::max)(0.f,end-now)));
        }
        unsigned itemStatus[2]{3,3};
        auto inventory=TwinKartLocalRace::FlagFind(pawn,0x22ef370);
        if(TwinKartLocalRace::OwnedComponent(inventory,pawn,reinterpret_cast<Type>(image+0x22ef370)())) {
            struct Stacks {const void* data;int count,capacity;};
            const auto stacks=*reinterpret_cast<const Stacks*>(inventory+0x170);
            if(stacks.count>=0 && stacks.count<=8 && stacks.capacity>=stacks.count && stacks.capacity<=64 &&
               (stacks.data || !stacks.count))for(unsigned slot=0;slot<2;++slot) {
                if(slot>=static_cast<unsigned>(stacks.count)){itemStatus[slot]=0;continue;}
                TwinKartCamera::ObjectIdentity stack{};int uses=0;
                if(ReadChaserStack(inventory,slot,stack,uses)) {
                    const bool usable=uses>0 && reinterpret_cast<bool(__fastcall*)(void*,int)>(image+0x17bf2b0)(inventory,slot);
                    if(ChaserIdentityLive(stack))itemStatus[slot]=!uses?0:usable?2:1;
                }
            }
        }
        packet=(role==TwinKartObjective::ChaserRole::Runaway?2:3)|(time<<3)|(taggers<<13)|(runaways<<18)|
            (itemStatus[0]<<23)|(itemStatus[1]<<25);
    } __finally {InterlockedExchange(&chaserHintState,packet);}
}
void StopChaserControls(BYTE* world) {
    for(int slot=1;slot<16;++slot){WriteChaserControl(world,slot,{});chaserDrivers[slot].Reset();chaserEdgeQueries[slot].Reset();}
}
bool UpdateChaserAiImpl(BYTE* world,bool sceneActive) {
    if(!OwnedChaserWorld(world)) {InterlockedExchange(&chaserHintState,0);if(chaserNavigationWorld.object)ResetChaserNavigation();return false;}
    unsigned long long phase=0;if(!sceneActive || !ChaserPhase(world,phase)){
        InterlockedExchange(&chaserHintState,0);chaserItemInputs.Reset();StopChaserControls(world);return false;}
    static unsigned long long observedPhase=0;static ULONGLONG nextState=0;auto stateNow=GetTickCount64();
    if(phase!=observedPhase || stateNow>=nextState) {
        observedPhase=phase;nextState=stateNow+5000;
        int phaseIndex=-1;unsigned addresses[]{0x68b9e10,0x68b9e20,0x68b9e30,0x68b9e40,
            TwinKartGraphics::ChaserGameNameRva,0x68b9e60,0x68b9e70,0x68b9e90};
        for(int i=0;i<8;++i)if(phase==*reinterpret_cast<unsigned long long*>(image+addresses[i]))phaseIndex=i;
        RuntimeEvent("native_chaser_observed_phase",-1,phaseIndex);
        auto nativeState=*reinterpret_cast<BYTE**>(world+0x120);using StateType=void*(__fastcall*)();
        auto modeState=TwinKartLocalRace::FlagFind(nativeState,0x2250ff0);
        if(TwinKartLocalRace::OwnedComponent(modeState,nativeState,reinterpret_cast<StateType>(image+0x2250ff0)())) {
            RuntimeEvent("native_chaser_observed_mode_state",-1,*reinterpret_cast<int*>(modeState+0x208));
            RuntimeEvent("native_chaser_observed_winner",-1,modeState[0x218]);
        }
    }
    auto game=*reinterpret_cast<unsigned long long*>(image+TwinKartGraphics::ChaserGameNameRva);
    bool driving=game && phase==game,build=driving;
    auto loading=*reinterpret_cast<unsigned long long*>(image+0x68b9e10);
    bool preparing=loading && phase==loading;
    if(preparing)build=ChaserLoadingCanPrepare(world);
    for(unsigned address:{0x68b9e20u,0x68b9e30u,0x68b9e40u}) {
        auto name=*reinterpret_cast<unsigned long long*>(image+address);
        preparing|=name && phase==name;build|=name && phase==name;
    }
    if(!driving)InterlockedExchange(&chaserHintState,preparing && !NativeDefaultLoadingTitleBound(108)?1:0);
    if(!build){chaserItemInputs.Reset();StopChaserControls(world);return false;}
    ULONGLONG now=GetTickCount64();if(now<chaserAiNext)return true;chaserAiNext=now+50;
    if(driving){PumpChaserSecondSlot(world,now);UpdateChaserItems(world,now);}
    else {chaserSecondSlotPress={};chaserItemInputs.Reset();}
    if(!OwnedChaserWorld(world))return false;
    bool navigation=BuildChaserNavigation(world,!driving);
    if(!navigation || !driving){StopChaserControls(world);return !chaserNavigationRejected;}
    auto rules=static_cast<BYTE*>(chaserObservedRule.object);using Type=void*(__fastcall*)();
    auto list=*reinterpret_cast<BYTE**>(rules+0xc0);auto state=*reinterpret_cast<BYTE**>(world+0x120);
    auto listType=reinterpret_cast<Type>(image+0x23b0c20)();
    if((!TwinKartLocalRace::OwnedComponent(list,state,listType) &&
        !TwinKartLocalRace::OwnedComponent(list,*reinterpret_cast<BYTE**>(world+0x118),listType)) ||
       !chaserRoleWorld.Matches(TwinKartLocalRace::World(list),*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),
           *reinterpret_cast<int*>(image+0x69c28c4))) {StopChaserControls(world);return true;}
    struct Pawns {BYTE** data;int count,capacity;};auto pawns=*reinterpret_cast<Pawns*>(list+0x100);
    if(!pawns.data || pawns.count<2 || pawns.count>16 || pawns.capacity<pawns.count || pawns.capacity>4096){StopChaserControls(world);return true;}
    TwinKartObjective::ChaserObservation observations[16]{};TwinKartCamera::ObjectIdentity observedPawns[16]{};
    float headings[16]{};bool drivable[16]{};int preparationStages[16]{};unsigned seen=0;
    auto stateType=reinterpret_cast<Type>(image+0x23afeb0)();
    for(int i=0;i<pawns.count;++i) {
        auto pawn=pawns.data[i];if(!TwinKartLocalRace::Live(pawn) || !TwinKartRace::ObjectiveAssetIsA(image,pawn,0x23237f0))continue;
        auto pawnState=TwinKartLocalRace::FlagFind(pawn,0x23afeb0);
        if(!TwinKartLocalRace::OwnedComponent(pawnState,pawn,stateType) ||
           !chaserRoleWorld.Matches(TwinKartLocalRace::World(pawnState),*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),
               *reinterpret_cast<int*>(image+0x69c28c4)))continue;
        int slot=*reinterpret_cast<int*>(pawnState+0x168);
        if(slot<0 || slot>=pawns.count || (seen&(1u<<slot)) || (slot==0?pawnState[0x321]!=0:pawnState[0x321]==0)) {
            StopChaserControls(world);return true;
        }
        seen|=1u<<slot;auto& observation=observations[slot];
        if(!TwinKartLocalRace::CaptureFlagObject(pawn,observedPawns[slot]))continue;
        auto runaway=TwinKartLocalRace::FlagFind(pawn,0x2256c10),tagger=TwinKartLocalRace::FlagFind(pawn,0x22570a0);
        observation.role=TwinKartObjective::NativeChaserRole(runaway!=nullptr,tagger!=nullptr);
        observation.ready=pawnState[0x338]==1 && TwinKartLocalRace::FlagPosition(pawn,observation.position,&headings[slot]);
        if(observation.ready && (std::fabs(observation.position.x)>1.e7f || std::fabs(observation.position.y)>1.e7f ||
           std::fabs(observation.position.z)>1.e7f))observation.ready=false;
        if(observation.ready && observation.role!=TwinKartObjective::ChaserRole::Unknown) {
            const auto& previous=chaserObservedRolePawns[slot];auto current=observedPawns[slot];
            if(previous.object==current.object && previous.index==current.index && previous.serial==current.serial &&
               chaserObservedRoles[slot]==TwinKartObjective::ChaserRole::Runaway && observation.role==TwinKartObjective::ChaserRole::Tagger)
                RuntimeEvent("native_chaser_infection_role_transition_observed",slot,1);
            chaserObservedRolePawns[slot]=current;chaserObservedRoles[slot]=observation.role;
        }
        if(!slot)continue;
        auto movement=TwinKartLocalRace::FlagFind(pawn,0x22f9030);TwinKartCamera::ObjectIdentity pawnId{},stateId{},movementId{};
        if(!movement || !TwinKartLocalRace::CaptureFlagObject(pawn,pawnId) || !TwinKartLocalRace::CaptureFlagObject(pawnState,stateId) ||
           !TwinKartLocalRace::CaptureFlagObject(movement,movementId))continue;
        if(chaserDriverPawns[slot].object!=pawn || chaserDriverPawns[slot].serial!=pawnId.serial ||
           chaserDriverStates[slot].object!=pawnState || chaserDriverStates[slot].serial!=stateId.serial ||
           chaserDriverMovements[slot].object!=movement || chaserDriverMovements[slot].serial!=movementId.serial) {
            WriteChaserControl(world,slot,{});chaserDrivers[slot].Reset();chaserEdgeQueries[slot].Reset();chaserDriverPrepared[slot]=false;
        }
        chaserDriverPawns[slot]=pawnId;chaserDriverStates[slot]=stateId;chaserDriverMovements[slot]=movementId;
        // Native role/respawn callbacks may replace or re-enable a component
        // without replacing the pawn. Recheck ownership of control, rather
        // than trusting a cached "prepared" bit across those transitions.
        chaserDriverPrepared[slot]=pawnState[0x338]==1 && PrepareChaserMovement(pawn,movement,slot,preparationStages[slot]);
        drivable[slot]=chaserDriverPrepared[slot] && movement[0xed8]!=0;
    }
    PublishChaserHud(world,observations,observedPawns,pawns.count);
    TwinKartObjective::ChaserObjectiveView objectives;
    bool objectivesReady=ReadChaserObjectives(world,observedPawns,pawns.count,now,objectives);
    float dt=chaserAiLast?static_cast<float>(now-chaserAiLast)/1000.f:0.05f;chaserAiLast=now;if(dt<=0 || dt>1)dt=0.05f;
    int planner=1+static_cast<int>(chaserPlannerCursor++%static_cast<unsigned>(pawns.count-1));
    bool log=now>=chaserAiLog;if(log)chaserAiLog=now+5000;
    for(int slot=1;slot<16;++slot) {
        bool active=slot<pawns.count && driving && drivable[slot] && observations[slot].ready;
        auto identity=chaserDriverPawns[slot];
        if(log && slot<pawns.count)RuntimeEvent("native_chaser_observed_role",slot,static_cast<int>(observations[slot].role));
        if(!active || chaserEdgeRoles[slot]!=observations[slot].role)chaserEdgeQueries[slot].Reset();
        chaserEdgeRoles[slot]=observations[slot].role;
        auto generation=(static_cast<std::uint64_t>(static_cast<unsigned>(identity.index))<<32)|static_cast<unsigned>(identity.serial);
        // Fifteen bots can put selected turns750ms apart. A250ms negative
        // cache would expire before the next bounded turn and repeatedly
        // spend all four queries on the same blocked nearest connectors.
        bool budget=active && chaserEdgeQueries[slot].Begin(generation,now,slot==planner?4u:0u,5000);
        ChaserRoadPlanning road{chaserGround,{world,slot}};
        auto control=slot<pawns.count?chaserDrivers[slot].Step(road,observations,pawns.count,slot,headings[slot],dt,now,
            active && budget,slot==planner,ChaserDriverPathClear,&road.query,objectivesReady?&objectives:nullptr,
            ChaserImmediateRoadForward,&road.query,ChaserImmediateRoadReverse,&road.query):TwinKartRace::ArenaControl{};
        bool written=WriteChaserControl(world,slot,control);
        if(log && slot<pawns.count)RuntimeEvent("native_chaser_objective_ai_control",slot,
            (observations[slot].ready?1:0)|(written?2:0)|(chaserDrivers[slot].RouteSize()?4:0)|(drivable[slot]?8:0));
        if(log && slot<pawns.count) {
            RuntimeEvent("native_chaser_driver_preparation_stage",slot,preparationStages[slot]);
            RuntimeEvent("native_chaser_throttle_milli",slot,static_cast<int>(TwinKartRace::NativeKartThrottle(control)*1000));
            RuntimeEvent("native_chaser_steer_milli",slot,static_cast<int>(control.steer*1000));
            RuntimeEvent("native_chaser_observed_objective_goal",slot,static_cast<int>(chaserDrivers[slot].ObjectiveKind()));
            if(observations[slot].ready) {
                RuntimeEvent("native_chaser_position_x",slot,static_cast<int>(observations[slot].position.x));
                RuntimeEvent("native_chaser_position_y",slot,static_cast<int>(observations[slot].position.y));
            }
        }
    }
    return true;
}
bool UpdateChaserAi(BYTE* world,bool sceneActive) {
    __try {return UpdateChaserAiImpl(world,sceneActive);}
    __except(EXCEPTION_EXECUTE_HANDLER){chaserItemInputs.Reset();StopChaserControls(world);RuntimeEvent("native_chaser_objective_ai_update_rejected",-1,0);return false;}
}
bool CheckChaserAuthorityWorld(void* world,void*) {return OwnedChaserWorld(static_cast<BYTE*>(world));}
void ObserveChaserOutcomeRosterImpl(BYTE* world) {
    if(!OwnedChaserWorld(world) || !ChaserIdentityLive(chaserObservedRule))return;
    auto list=*reinterpret_cast<BYTE**>(static_cast<BYTE*>(chaserObservedRule.object)+0xc0);
    using Type=void*(__fastcall*)();auto type=reinterpret_cast<Type>(image+0x23b0c20)();
    if(!TwinKartLocalRace::OwnedComponent(list,*reinterpret_cast<BYTE**>(world+0x120),type) &&
       !TwinKartLocalRace::OwnedComponent(list,*reinterpret_cast<BYTE**>(world+0x118),type))return;
    struct Pawns {BYTE** data;int count,capacity;};auto pawns=*reinterpret_cast<Pawns*>(list+0x100);
    if(!pawns.data || pawns.count<2 || pawns.count>16 || pawns.capacity<pawns.count || pawns.capacity>4096)return;
    int taggers=0,runaways=0;unsigned slots=0;TwinKartCamera::ObjectIdentity identities[16]{};
    auto stateType=reinterpret_cast<Type>(image+0x23afeb0)();
    for(int i=0;i<pawns.count;++i) {
        auto pawn=pawns.data[i];if(!TwinKartLocalRace::CaptureFlagObject(pawn,identities[i]))return;
        auto state=TwinKartLocalRace::FlagFind(pawn,0x23afeb0);
        if(!TwinKartLocalRace::OwnedComponent(state,pawn,stateType) || state[0x338]!=1 ||
           TwinKartLocalRace::World(state)!=world)return;
        int slot=*reinterpret_cast<int*>(state+0x168);
        if(slot<0 || slot>=pawns.count || (slots&(1u<<slot)))return;slots|=1u<<slot;
        auto role=TwinKartObjective::NativeChaserRole(TwinKartLocalRace::FlagFind(pawn,0x2256c10)!=nullptr,
            TwinKartLocalRace::FlagFind(pawn,0x22570a0)!=nullptr);
        if(role==TwinKartObjective::ChaserRole::Tagger)++taggers;
        else if(role==TwinKartObjective::ChaserRole::Runaway)++runaways;
        else return;
    }
    for(int i=0;i<pawns.count;++i)if(!ChaserIdentityLive(identities[i]))return;
    if(OwnedChaserWorld(world))RuntimeEvent("native_chaser_result_role_counts",taggers,runaways);
}
void ObserveChaserOutcomeRoster(BYTE* world) {
    __try {ObserveChaserOutcomeRosterImpl(world);}
    __except(EXCEPTION_EXECUTE_HANDLER){RuntimeEvent("native_chaser_result_roster_observation_rejected",-1,0);}
}
void __fastcall ShowOriginalChaserResultWidget(void* widget,unsigned char visibility) {
    // This exact native rule can request a stripped cooked widget class. Its
    // normal factory then returns null. Keep the original rules, winner and
    // result progression; do not dereference that failed optional UI result.
    if(!widget){
        RuntimeEvent("native_chaser_result_widget_missing",-1,0);
        __try {
            auto world=OwnedRaceWorld();unsigned long long phase=0;
            if(!world || !OwnedChaserWorld(world) || !ChaserPhase(world,phase))return;
            ObserveChaserOutcomeRoster(world);
            auto post=*reinterpret_cast<unsigned long long*>(image+0x68b9e60),podium=*reinterpret_cast<unsigned long long*>(image+0x68b9e70);
            if(!((post && phase==post) || (podium && phase==podium)))return;
            auto state=*reinterpret_cast<BYTE**>(world+0x120);using Type=void*(__fastcall*)();
            auto type=reinterpret_cast<Type>(image+0x2250ff0)();auto component=TwinKartLocalRace::FlagFind(state,0x2250ff0);
            TwinKartCamera::ObjectIdentity identity{};
            if(!TwinKartLocalRace::OwnedComponent(component,state,type) || !TwinKartLocalRace::CaptureFlagObject(world,identity))return;
            TwinKartObjective::PrototypeSettlement value{reinterpret_cast<std::uintptr_t>(world),identity.index,identity.serial,component[0x218]};
            AcquireSRWLockExclusive(&prototypeResultLock);bool published=prototypeResults.Publish(value);ReleaseSRWLockExclusive(&prototypeResultLock);
            if(published)RuntimeEvent("native_chaser_authored_result_presentation_queued",-1,value.winner);
        } __except(EXCEPTION_EXECUTE_HANDLER){RuntimeEvent("native_chaser_result_presentation_rejected",-1,0);}
        return;
    }
    using Show=void(__fastcall*)(void*,unsigned char);
    auto table=*reinterpret_cast<void***>(widget);
    reinterpret_cast<Show>(table[0x280/8])(widget,visibility);
    RuntimeEvent("native_chaser_original_result_widget_shown",-1,1);
}
int __fastcall ChaserRoleAuthorityNetMode(void* world) {
    return TwinKartObjective::ResolveChaserRoleAuthority(world,chaserModeDefinitionEnabled,
        GetCurrentThreadId()==queue.ThreadId(),chaserNativeNetMode,CheckChaserAuthorityWorld,nullptr);
}
int __fastcall ChaserRoleEventsNetMode(void* world) {
    return TwinKartObjective::ResolveChaserRoleEvents(world,chaserModeDefinitionEnabled,
        GetCurrentThreadId()==queue.ThreadId(),chaserNativeNetMode,CheckChaserAuthorityWorld,nullptr);
}
bool ConnectChaserBot(const TwinKartObjective::ChaserConnectionSeat& seat,void* raw) {
    auto& context=*static_cast<ChaserConnectionContext*>(raw);
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);int count=*reinterpret_cast<int*>(image+0x69c28c4);
    BYTE* currentWorld=nullptr;
    if(GetCurrentThreadId()!=queue.ThreadId() || seat.slot<1 || seat.slot>=16 ||
       !TwinKartCamera::Live(context.rules,chunks,count) ||
       !ChaserConnectionWorld(static_cast<BYTE*>(context.rules.object),currentWorld) || currentWorld!=context.world ||
       !chaserRoleWorld.Matches(context.world,chunks,count) ||
       !TwinKartCamera::Live(context.pawns[seat.slot],chunks,count) || !TwinKartCamera::Live(context.states[seat.slot],chunks,count) ||
       context.pawns[seat.slot].object!=seat.pawn || context.states[seat.slot].object!=seat.state)return false;
    auto state=static_cast<BYTE*>(seat.state);
    if(*reinterpret_cast<void**>(state+0xa0)!=seat.pawn || *reinterpret_cast<int*>(state+0x168)!=seat.slot || !state[0x321] ||
       !chaserRoleWorld.Matches(TwinKartLocalRace::World(state),chunks,count))return false;
    // Recheck after any preceding native notification. Never reconnect an
    // intentionally disconnected seat or write the replicated status byte.
    if(state[0x338]==1)return true;
    if(state[0x338]!=0 && state[0x338]!=3)return false;
    reinterpret_cast<void(__fastcall*)(void*)>(image+0x164f110)(state);
    ++context.nativeConnectCalls;
    // SetConnect broadcasts a native notification. Refresh registry storage
    // and the complete affected relationship after that callback, including
    // a world change or a recycled pawn, before inspecting the status byte.
    chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);count=*reinterpret_cast<int*>(image+0x69c28c4);
    currentWorld=nullptr;
    if(!TwinKartCamera::Live(context.rules,chunks,count) || !TwinKartCamera::Live(context.pawns[seat.slot],chunks,count) ||
       !TwinKartCamera::Live(context.states[seat.slot],chunks,count) ||
       !ChaserConnectionWorld(static_cast<BYTE*>(context.rules.object),currentWorld) || currentWorld!=context.world ||
       !chaserRoleWorld.Matches(context.world,chunks,count) ||
       TwinKartLocalRace::FlagFind(static_cast<BYTE*>(seat.pawn),0x23afeb0)!=state ||
       *reinterpret_cast<void**>(state+0xa0)!=seat.pawn || *reinterpret_cast<int*>(state+0x168)!=seat.slot ||
       !chaserRoleWorld.Matches(TwinKartLocalRace::World(state),chunks,count))return false;
    return state[0x321] && state[0x338]==1;
}
bool PrepareOriginalChaserRoster(BYTE* rules,bool notify) {
    BYTE* world=nullptr;if(!ChaserConnectionWorld(rules,world))return false;
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);int count=*reinterpret_cast<int*>(image+0x69c28c4);
    using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
    auto gameState=*reinterpret_cast<BYTE**>(world+0x120);
    if(!TwinKartLocalRace::Live(gameState) || *reinterpret_cast<void**>(gameState+0x10)!=reinterpret_cast<Type>(image+0x2250ab0)())return false;
    auto list=*reinterpret_cast<BYTE**>(rules+0xc0);auto listType=reinterpret_cast<Type>(image+0x23b0c20)();
    if(!TwinKartLocalRace::Live(list) || *reinterpret_cast<void**>(list+0x10)!=listType ||
       (*reinterpret_cast<void**>(list+0xa0)!=gameState && *reinterpret_cast<void**>(list+0xa0)!=*reinterpret_cast<void**>(world+0x118)) ||
       !chaserRoleWorld.Matches(TwinKartLocalRace::World(list),chunks,count))return false;
    struct Pawns {BYTE** data;int count,capacity;};auto pawns=*reinterpret_cast<Pawns*>(list+0x100);
    if(!pawns.data || pawns.count<2 || pawns.count>16 || pawns.capacity<pawns.count || pawns.capacity>4096)return false;
    TwinKartObjective::ChaserConnectionSeat seats[16]{};ChaserConnectionContext context;context.world=world;
    TwinKartCamera::ObjectIdentity listId{};
    if(!TwinKartLocalRace::CaptureFlagObject(rules,context.rules) || !TwinKartLocalRace::CaptureFlagObject(list,listId))return false;
    auto stateType=reinterpret_cast<Type>(image+0x23afeb0)();
    unsigned observedSlots=0;
    for(int i=0;i<pawns.count;++i) {
        auto pawn=pawns.data[i];if(!TwinKartLocalRace::Live(pawn) ||
           !TwinKartRace::ObjectiveAssetIsA(image,pawn,0x23237f0))return false;
        auto state=reinterpret_cast<Find>((*reinterpret_cast<void***>(pawn))[0x620/8])(pawn,stateType);
        if(!TwinKartLocalRace::OwnedComponent(state,pawn,stateType) ||
           !chaserRoleWorld.Matches(TwinKartLocalRace::World(state),chunks,count))return false;
        int slot=*reinterpret_cast<int*>(state+0x168);
        if(slot<0 || slot>=pawns.count || (observedSlots&(1u<<slot)) || (!slot && state[0x321]))return false;
        if(*reinterpret_cast<void**>(pawn+0x10)!=reinterpret_cast<Type>(image+(slot?0x2413310:0x2250e50))())return false;
        observedSlots|=1u<<slot;
        // Validate the complete expected original roster before any component
        // registration or setter; native AI flags are checked again below.
        seats[i]={pawn,state,slot,slot!=0,state[0x338]};
        if(!TwinKartLocalRace::CaptureFlagObject(pawn,context.pawns[slot]) ||
           !TwinKartLocalRace::CaptureFlagObject(state,context.states[slot]))return false;
    }
    unsigned pending=0;if(!TwinKartObjective::ChaserConnectionPlan(seats,pawns.count,pending))return false;
    for(int i=0;i<pawns.count;++i)if(seats[i].slot && seats[i].connection!=2) {
        if(!TwinKartObjective::ChaserConnectionPlan(seats,pawns.count,pending) ||
           !TwinKartLocalRace::PrepareChaserAiCore(static_cast<BYTE*>(seats[i].pawn),seats[i].slot))return false;
    }
    // RegisterComponent can synchronously invoke arbitrary original callbacks.
    // Do not publish readiness from an old list or an earlier bot's old state.
    chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);count=*reinterpret_cast<int*>(image+0x69c28c4);
    BYTE* currentWorld=nullptr;
    if(!TwinKartCamera::Live(context.rules,chunks,count) || !TwinKartCamera::Live(listId,chunks,count) ||
       !ChaserConnectionWorld(rules,currentWorld) || currentWorld!=world || *reinterpret_cast<BYTE**>(rules+0xc0)!=list)return false;
    auto current=*reinterpret_cast<Pawns*>(list+0x100);
    if(current.data!=pawns.data || current.count!=pawns.count || current.capacity!=pawns.capacity)return false;
    for(int i=0;i<pawns.count;++i) {
        const auto& seat=seats[i];auto state=static_cast<BYTE*>(seat.state);
        if(!TwinKartCamera::Live(context.pawns[seat.slot],chunks,count) || !TwinKartCamera::Live(context.states[seat.slot],chunks,count) ||
           current.data[i]!=seat.pawn || TwinKartLocalRace::FlagFind(static_cast<BYTE*>(seat.pawn),0x23afeb0)!=state ||
           *reinterpret_cast<void**>(state+0xa0)!=seat.pawn || *reinterpret_cast<int*>(state+0x168)!=seat.slot ||
           !chaserRoleWorld.Matches(TwinKartLocalRace::World(state),chunks,count))return false;
        seats[i].ai=state[0x321]!=0;seats[i].connection=state[0x338];
    }
    if(!TwinKartObjective::ChaserConnectionPlan(seats,pawns.count,pending))return false;
    if(!notify)return true;
    // Rebuild from current native statuses on every selection callback. A
    // partial notification failure must remain retryable; already connected
    // or intentionally disconnected seats are excluded by the fresh plan.
    // OnChaserSelectRoles owns the reentrancy guard, not a success cache.
    unsigned completed=0;
    bool prepared=TwinKartObjective::PrepareChaserConnections(seats,pawns.count,ConnectChaserBot,&context,completed);
    RuntimeEvent("native_chaser_bot_connections_prepared",static_cast<int>(completed),prepared?1:0);
    RuntimeEvent("native_chaser_set_connect_calls_returned",static_cast<int>(context.nativeConnectCalls),prepared?1:0);
    return prepared;
}
void __fastcall OnChaserSelectRoles(void* raw) {
    if(!preparingChaserConnections) {
        preparingChaserConnections=true;
        __try {
            __try {PrepareOriginalChaserRoster(static_cast<BYTE*>(raw),true);}
            __finally {preparingChaserConnections=false;}
        } __except(EXCEPTION_EXECUTE_HANDLER) {RuntimeEvent("native_chaser_connection_preparation_rejected",-1,0);}
    }
    // Native selection, RNG, role components and their BeginPlay execute once,
    // unchanged. No role, infection, winning team or result is manufactured.
    chaserSelectRoles(raw);
}
thread_local bool readingChaserDefault=false;
TwinKartObjective::ChaserPresentation chaserPresentation;
TwinKartCamera::ObjectIdentity chaserPresentationAsset{};
TwinKartObjective::ChaserPresentation hidePresentation(true);
TwinKartCamera::ObjectIdentity hidePresentationAsset{};
bool hidePresentationAttempted=false;
TwinKartObjective::ChaserPresentation factoryPresentation(false,true);
TwinKartCamera::ObjectIdentity factoryPresentationAsset{};
bool factoryPresentationAttempted=false;
TwinKartObjective::ChaserPresentation freeDrivePresentation(false,false,true);
TwinKartCamera::ObjectIdentity freeDrivePresentationAsset{};
bool freeDrivePresentationAttempted=false;
void* AuthoredPrototypeDefinition(unsigned mode) {
    bool factory=mode==100,freeDrive=mode==1;TwinKartObjective::AuthoredLobbyDefinition specification;
    if(!TwinKartObjective::PrototypeLobbyDefinition(mode,specification) ||
       !(freeDrive?practiceTicketPath[0]!=0:factory?TwinKartLocalRace::factoryCompatEnabled:hideLobbyEnabled) || GetCurrentThreadId()!=queue.ThreadId())return nullptr;
    __try {
        auto& identity=freeDrive?freeDrivePresentationAsset:factory?factoryPresentationAsset:hidePresentationAsset;
        auto& attempted=freeDrive?freeDrivePresentationAttempted:factory?factoryPresentationAttempted:hidePresentationAttempted;
        auto& presentation=freeDrive?freeDrivePresentation:factory?factoryPresentation:hidePresentation;
        if(ChaserIdentityLive(identity))return identity.object;
        if(attempted)return nullptr;
        using Type=void*(__fastcall*)();
        auto type=reinterpret_cast<Type>(image+0x22b9690)();
        // GetTransientPackage is also used by the original parameter initializer.
        // Never put this process-lifetime descriptor under a traveled world.
        auto outer=static_cast<BYTE*>(reinterpret_cast<Type>(image+0x27302d0)());
        if(!TwinKartLocalRace::Live(static_cast<BYTE*>(type)) || !TwinKartLocalRace::Live(outer))return nullptr;
        attempted=true;
        auto asset=static_cast<BYTE*>(TwinKartObjective::CreateAuthoredLobbyObject(type,outer,
            reinterpret_cast<TwinKartObjective::InitializeAuthoredObject>(image+0x2771dd0),
            reinterpret_cast<TwinKartObjective::ConstructAuthoredObject>(image+0x278aa90)));
        if(!TwinKartLocalRace::Live(asset) || *reinterpret_cast<void**>(asset+0x10)!=type ||
           *reinterpret_cast<void**>(asset+0x20)!=outer || (*reinterpret_cast<unsigned*>(asset+8)&0x18030))return nullptr;
        // Author only our instance, preserving its native-constructed members.
        reinterpret_cast<TwinKartObjective::NameChaserText>(image+0x2589130)(
            reinterpret_cast<std::uint64_t*>(asset+0x40),specification.gameClass,1);
        wchar_t travelText[16]{};wcscpy_s(travelText,specification.travel);
        int length=static_cast<int>(wcslen(travelText)+1);
        TwinKartRace::ReturnString travel{travelText,length,length};
        reinterpret_cast<TwinKartRace::AssignReturnString>(image+0x9118f0)(
            reinterpret_cast<TwinKartRace::ReturnString*>(asset+0x238),&travel);
        *reinterpret_cast<int*>(asset+0x230)=2;
        *reinterpret_cast<int*>(asset+0x248)=specification.teamSize;*reinterpret_cast<int*>(asset+0x24c)=specification.teamCount;
        TwinKartRace::ModeDefinition check;
        if(!InspectOriginalMode(asset,check,nullptr) || check.kind!=2 || check.teamSize!=specification.teamSize || check.teamCount!=specification.teamCount ||
           wcscmp(check.gameClass,specification.gameClass) || wcscmp(check.travelName,specification.travel) ||
           !presentation.Apply(asset,reinterpret_cast<TwinKartObjective::CreateChaserText>(image+0x3fe0a60),
               reinterpret_cast<TwinKartObjective::NameChaserText>(image+0x2589130),
               reinterpret_cast<TwinKartObjective::SetChaserText>(image+0x3ffe670)) ||
           !TwinKartLocalRace::CaptureFlagObject(asset,identity))return nullptr;
        RuntimeEvent(freeDrive?"native_free_drive_authored_lobby_descriptor_created":factory?"native_factory_authored_lobby_descriptor_created":"native_hide_authored_lobby_descriptor_created",-1,mode);return asset;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return nullptr;}
}
void* AuthoredHideDefinition(){return AuthoredPrototypeDefinition(109);}
void* RetainedChaserDefinition(void*) {
    if(readingChaserDefault || !chaserModeDefinitionEnabled || GetCurrentThreadId()!=queue.ThreadId())return nullptr;
    readingChaserDefault=true;
    __try {
        __try {
            using Type=void*(__fastcall*)();
            auto type=static_cast<BYTE*>(reinterpret_cast<Type>(image+0x22b7d40)());
            if(!TwinKartLocalRace::Live(type))return nullptr;
            auto table=*reinterpret_cast<BYTE***>(type);
            auto create=table[0x3a0/8];
            if(create<image || create>=image+0x4ed0000)return nullptr;
            // The original PC lazy-default call sites independently establish
            // UClass+0x118 and virtual+0x3a0. This calls the engine constructor,
            // never copies a vendor body or creates a replacement game mode.
            auto asset=static_cast<BYTE*>(TwinKartObjective::OriginalClassDefault(type,
                reinterpret_cast<TwinKartObjective::CreateClassDefault>(create)));
            if(!TwinKartLocalRace::Live(asset) || !TwinKartRace::ObjectiveAssetIsA(image,asset,0x22b9690))return nullptr;
            TwinKartObjective::ChaserModeDefinition definition;
            if(!TwinKartObjective::ReadOriginalChaserDefault(type,asset,ModeNameText,definition))return nullptr;
            // Only the explicitly enabled recovered Chaser CDO needs authored
            // display text: this build has no matching localization entries.
            // The original reflected setter assigns FText properties, never
            // a guessed raw pointer or the mode's scalar/physics metadata.
            if(chaserArenaEnabled && !ChaserIdentityLive(chaserPresentationAsset)) {
                if(!chaserPresentation.Apply(asset,reinterpret_cast<TwinKartObjective::CreateChaserText>(image+0x3fe0a60),
                    reinterpret_cast<TwinKartObjective::NameChaserText>(image+0x2589130),
                    reinterpret_cast<TwinKartObjective::SetChaserText>(image+0x3ffe670)) ||
                    !TwinKartLocalRace::CaptureFlagObject(asset,chaserPresentationAsset))return nullptr;
                RuntimeEvent("native_chaser_authored_display_text_applied",-1,1);
            }
            static unsigned notices=0;
            if(notices++<3)RuntimeEvent("native_chaser_original_class_defaults_available",-1,definition.teamSize);
            // Native UClass owns its CDO for its own lifetime. No transient raw
            // object is retained, no table/asset is overwritten, and no mutable
            // game settings are written into the class defaults by this adapter.
            return asset;
        } __finally {readingChaserDefault=false;}
    } __except(EXCEPTION_EXECUTE_HANDLER) {return nullptr;}
}
void* __fastcall OnModeDefinitionLookup(const TwinKartRace::PreparationMode* mode) {
    __try {
        if(mode && mode->mode==1 && GetCurrentThreadId()==queue.ThreadId()) {
            TwinKartEquipment::String text{mode->profile.data,mode->profile.count,mode->profile.capacity};
            if(TwinKartEquipment::Valid(text,97) && TwinKartEquipment::Nonempty(text) && !wcscmp(text.data,TwinKartRace::FreeDriveProfileWide))
                return AuthoredPrototypeDefinition(1);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER){return nullptr;}
    auto result=TwinKartObjective::ResolveRetainedChaser(mode,chaserModeDefinitionEnabled,
        GetCurrentThreadId()==queue.ThreadId(),originalModeLookup,RetainedChaserDefinition,nullptr);
    if(result || GetCurrentThreadId()!=queue.ThreadId())return result;
    __try {
        if(!mode || !((mode->mode==109 && hideLobbyEnabled) || (mode->mode==100 && TwinKartLocalRace::factoryCompatEnabled)))return result;
        TwinKartEquipment::String text{mode->profile.data,mode->profile.count,mode->profile.capacity};
        if(!TwinKartEquipment::Valid(text,97) || TwinKartEquipment::Nonempty(text))return result;
        return AuthoredPrototypeDefinition(mode->mode);
    } __except(EXCEPTION_EXECUTE_HANDLER) {return result;}
}
TwinKartNative::OwnedCodePatch prototypeFindTeamPatch,prototypeMenuTeamPatch;
int __fastcall PrototypeMenuTeamSize(const TwinKartRace::PreparationMode* mode) {
    auto original=reinterpret_cast<TwinKartObjective::ReadLobbyTeamSize>(image+0x16a3c00);
    __try {return TwinKartObjective::PrototypeLobbyTeamSize(mode,GetCurrentThreadId()==queue.ThreadId(),
        chaserModeDefinitionEnabled,hideLobbyEnabled,original);}
    __except(EXCEPTION_EXECUTE_HANDLER) {return 0;}
}
bool InstallPrototypeMenuSelection() {
    if(!prototypeFindTeamPatch.Call(image+0x1dd24ac,image+0x16a3c00,reinterpret_cast<void*>(PrototypeMenuTeamSize)))return false;
    if(!prototypeMenuTeamPatch.Call(image+0x1e21263,image+0x16a3c00,reinterpret_cast<void*>(PrototypeMenuTeamSize))) {
        prototypeFindTeamPatch.Restore();return false;
    }
    return true;
}
void* AuthoredChaserCaptureConfig(BYTE* type,BYTE* world,int taggers) {
    __try {
        if(!type || !OwnedChaserWorld(world) || taggers<1 || taggers>7)return nullptr;
        static TwinKartCamera::ObjectIdentity authored{};static int configured=0;
        if(!ChaserIdentityLive(authored) || configured!=taggers) {
            using Type=BYTE*(__fastcall*)();auto outer=reinterpret_cast<Type>(image+0x27302d0)();
            if(!TwinKartLocalRace::Live(outer))return nullptr;
            auto instance=TwinKartObjective::CreateAuthoredLobbyObject(type,outer,
                reinterpret_cast<TwinKartObjective::InitializeAuthoredObject>(image+0x2771dd0),
                reinterpret_cast<TwinKartObjective::ConstructAuthoredObject>(image+0x278aa90));
            if(!TwinKartLocalRace::Live(static_cast<BYTE*>(instance)) || !OwnedChaserWorld(world) ||
               !TwinKartObjective::ConfigureAuthoredChaserCapture(instance,type,taggers) ||
               !TwinKartLocalRace::CaptureFlagObject(static_cast<BYTE*>(instance),authored))return nullptr;
            configured=taggers;
        }
        return OwnedChaserWorld(world)?authored.object:nullptr;
    } __except(EXCEPTION_EXECUTE_HANDLER){return nullptr;}
}
void* MissingOriginalChaserConfig(void* raw,void*) {
    __try {
        if(!chaserArenaEnabled || GetCurrentThreadId()!=queue.ThreadId() || !raw)return nullptr;
        auto reference=static_cast<TwinKartRace::ObjectiveSoftReference*>(raw);
        auto rule=reinterpret_cast<BYTE*>(raw)-0x60;
        using Type=BYTE*(__fastcall*)();using GetWorld=BYTE*(__fastcall*)(void*);
        if(!TwinKartLocalRace::Live(rule))return nullptr;
        auto actualClass=*reinterpret_cast<BYTE**>(rule+0x10);wchar_t actualName[128]{},actualPath[512]{};
        if(TwinKartLocalRace::Live(actualClass) && ModeNameText(*reinterpret_cast<unsigned long long*>(actualClass+0x18),actualName,ARRAYSIZE(actualName)) &&
           ModeNameText(reference->path.name,actualPath,ARRAYSIZE(actualPath)))
            LogMissingChaserConfig(actualName,actualPath);
        if(actualClass!=reinterpret_cast<Type>(image+0x2251170)())return nullptr;
        auto world=reinterpret_cast<GetWorld>((*reinterpret_cast<void***>(rule))[0x160/8])(rule);
        unsigned long long phase=0;
        if(!OwnedChaserWorld(world) || !ChaserPhase(world,phase) ||
           (phase!=*reinterpret_cast<unsigned long long*>(image+0x68b9e10) && phase!=*reinterpret_cast<unsigned long long*>(image+0x68b9e20))) {
            RuntimeEvent("native_chaser_missing_config_scope_rejected",-1,2);return nullptr;
        }
        wchar_t path[512]{};
        if(!ModeNameText(reference->path.name,path,ARRAYSIZE(path)) ||
           wcscmp(path,L"/Game/DataTable/Chaser/ChaserGameRulesConfig.ChaserGameRulesConfig") ||
           !TwinKartEquipment::Valid(reference->path.subpath,512) || TwinKartEquipment::Nonempty(reference->path.subpath)) {
            RuntimeEvent("native_chaser_missing_config_scope_rejected",-1,3);return nullptr;
        }
        auto type=reinterpret_cast<Type>(image+0x22510b0)();
        if(!TwinKartLocalRace::Live(type))return nullptr;
        auto create=(*reinterpret_cast<BYTE***>(type))[0x3a0/8];
        if(create<image || create>=image+0x4ed0000)return nullptr;
        auto asset=static_cast<BYTE*>(TwinKartObjective::OriginalClassDefault(type,
            reinterpret_cast<TwinKartObjective::CreateClassDefault>(create)));
        if(!TwinKartLocalRace::Live(asset) || !(*reinterpret_cast<unsigned*>(asset+8)&0x10) ||
           *reinterpret_cast<unsigned*>(asset+8)&0x18000 || *reinterpret_cast<void**>(asset+0x10)!=type ||
           *reinterpret_cast<void**>(type+0x118)!=asset || !OwnedChaserWorld(world))return nullptr;
        wchar_t capture[4]{};
        if(GetEnvironmentVariableW(L"TWIN_KART_CHASER_TEST_INITIAL_TAGGERS",capture,ARRAYSIZE(capture))==1) {
            int taggers=TwinKartObjective::ChaserCaptureTaggers(capture);
            if(taggers) {
                if(auto instance=AuthoredChaserCaptureConfig(type,world,taggers)) {
                    RuntimeEvent("native_chaser_authored_capture_initial_taggers",-1,taggers);return instance;
                }
                // A rejected optional fixture must never replace the valid
                // native fallback with null and fault the original serializer.
                RuntimeEvent("native_chaser_capture_config_rejected_using_default",-1,taggers);
                if(!OwnedChaserWorld(world) || !TwinKartLocalRace::Live(asset))return nullptr;
            }
        }
        RuntimeEvent("native_chaser_missing_config_original_class_default_selected",-1,1);
        // No timer/role/result field is changed. The unchanged original caller
        // serializes this native configuration through ServerConfigComponent.
        return asset;
    } __except(EXCEPTION_EXECUTE_HANDLER){return nullptr;}
}
void* __fastcall LoadOriginalChaserConfig(void* reference) {
    return TwinKartObjective::ResolveChaserConfigAsset(reference,chaserArenaEnabled,GetCurrentThreadId()==queue.ThreadId(),
        reinterpret_cast<TwinKartObjective::LoadChaserConfigAsset>(image+0xa42300),MissingOriginalChaserConfig,nullptr);
}
bool InstallChaserModeDefinition() {
    bool chaser=EnvironmentFlag(L"TWIN_KART_CHASER_COMPAT");
    if(!chaser && !hideLobbyEnabled && !TwinKartLocalRace::factoryCompatEnabled && !practiceTicketPath[0])return true;
    if(chaserModeLookupInstalled)return true;
    if(GetCurrentThreadId()!=queue.ThreadId())return false;
    wchar_t arenaEnabled[4]{};
    bool arena=chaser && GetEnvironmentVariableW(L"TWIN_KART_CHASER_ARENA",arenaEnabled,4)==1 && arenaEnabled[0]==L'1';
    if(arena && !objectiveArenaModeInstalled) {
        const BYTE modeCall[]{0xff,0x97,0x48,0x03,0x00,0x00};
        if(!hideArenaModePatch.IndirectCall(image+0x3f3b632,modeCall,sizeof(modeCall),reinterpret_cast<void*>(HideArenaModeOverride)))return false;
        objectiveArenaModeInstalled=true;
    }
    // Complete nineteen-byte register/stack prologue, before its first CALL.
    // The original FKtGameMode reader remains responsible for all other modes
    // and profiles, including any real Chaser asset it already resolves.
    const BYTE prefix[]={0x48,0x89,0x5c,0x24,0x18,0x48,0x89,0x7c,0x24,0x20,0x41,0x56,0x48,0x83,0xec,0x60,0x4c,0x8b,0xf1};
    const BYTE connectPrefix[]={0xc6,0x81,0x38,0x03,0x00,0x00,0x01,0x48,0x81,0xc1,0x28,0x01,0x00,0x00};
    const BYTE selectPrefix[]={0x48,0x8b,0xc4,0x48,0x89,0x58,0x20,0x48,0x89,0x48,0x08,0x55,0x56,0x57,0x41,0x54};
    const BYTE aiFlagPrefix[]={0xc6,0x81,0x21,0x03,0x00,0x00,0x01,0xc3};
    if(memcmp(image+0x23b02c0,aiFlagPrefix,sizeof(aiFlagPrefix)) ||
       memcmp(image+0x164f110,connectPrefix,sizeof(connectPrefix)) || image[0x164f11e]!=0xe9 ||
       image+0x164f123+*reinterpret_cast<const int*>(image+0x164f11f)!=image+0x9143c0)return false;
    if(!chaserModeLookupPatch.Entry(image+0x169a930,prefix,sizeof(prefix),reinterpret_cast<void*>(OnModeDefinitionLookup),
        reinterpret_cast<void**>(&originalModeLookup)))return false;
    if(!chaser) {
        if(hideLobbyEnabled && !InstallPrototypeMenuSelection()) {chaserModeLookupPatch.Restore();originalModeLookup=nullptr;return false;}
        chaserModeLookupInstalled=true;return true;
    }
    if(!chaserSelectRolesPatch.Entry(image+0x18bfa10,selectPrefix,sizeof(selectPrefix),reinterpret_cast<void*>(OnChaserSelectRoles),
        reinterpret_cast<void**>(&chaserSelectRoles))) {chaserModeLookupPatch.Restore();originalModeLookup=nullptr;return false;}
    chaserNativeNetMode=reinterpret_cast<TwinKartObjective::ChaserNativeNetMode>(image+0x43beaf0);
    // Exact server-side original rule/item gate in Tagger::BeginPlay.
    // This does not bind its client-side original movement-hit subscription.
    if(!chaserRoleAuthorityPatch.Call(image+0x188753f,reinterpret_cast<void*>(chaserNativeNetMode),
        reinterpret_cast<void*>(ChaserRoleAuthorityNetMode))) {
        chaserSelectRolesPatch.Restore();chaserModeLookupPatch.Restore();originalModeLookup=nullptr;chaserSelectRoles=nullptr;return false;
    }
    // That earlier client query gates the original lazy movement listener
    // and native OnCollision delegate used to request infection. Admit only
    // our exact standalone Chaser world here, before the unchanged native
    // authority block. Never replay BeginPlay or manufacture an infection.
    if(!chaserRoleEventsPatch.Call(image+0x18874fb,reinterpret_cast<void*>(chaserNativeNetMode),
        reinterpret_cast<void*>(ChaserRoleEventsNetMode))) {
        chaserRoleAuthorityPatch.Restore();chaserSelectRolesPatch.Restore();chaserModeLookupPatch.Restore();
        originalModeLookup=nullptr;chaserSelectRoles=nullptr;return false;
    }
    // Entire ten-byte native counter predicate plus its six verified INT3
    // alignment bytes. No relative instructions or neighboring function are
    // copied. The shared method is qualified by exact owned Loading rule.
    const BYTE loadingPrefix[]{0x8b,0x41,0x60,0x39,0x41,0x64,0x0f,0x95,0xc0,0xc3,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc};
    if(!chaserLoadingReadyPatch.Entry(image+0x2254310,loadingPrefix,sizeof(loadingPrefix),reinterpret_cast<void*>(OnChaserCanTransit),
        reinterpret_cast<void**>(&chaserCanTransit))) {
        chaserRoleEventsPatch.Restore();chaserRoleAuthorityPatch.Restore();chaserSelectRolesPatch.Restore();chaserModeLookupPatch.Restore();
        originalModeLookup=nullptr;chaserSelectRoles=nullptr;chaserCanTransit=nullptr;return false;
    }
    if(arena && !chaserMissingConfigPatch.Call(image+0x1686e16,image+0xa42300,reinterpret_cast<void*>(LoadOriginalChaserConfig))) {
        chaserLoadingReadyPatch.Restore();chaserRoleEventsPatch.Restore();chaserRoleAuthorityPatch.Restore();
        chaserSelectRolesPatch.Restore();chaserModeLookupPatch.Restore();
        originalModeLookup=nullptr;chaserSelectRoles=nullptr;chaserCanTransit=nullptr;return false;
    }
    if(arena && !chaserResultVisibilityPatch.WidgetVisibilityCall(image+0x188b524,reinterpret_cast<void*>(ShowOriginalChaserResultWidget))) {
        chaserMissingConfigPatch.Restore();chaserLoadingReadyPatch.Restore();chaserRoleEventsPatch.Restore();
        chaserRoleAuthorityPatch.Restore();chaserSelectRolesPatch.Restore();chaserModeLookupPatch.Restore();
        originalModeLookup=nullptr;chaserSelectRoles=nullptr;chaserCanTransit=nullptr;return false;
    }
    if(!TwinKartLocalRace::InstallNativeAiItemObserver()) {
        chaserResultVisibilityPatch.Restore();chaserMissingConfigPatch.Restore();chaserLoadingReadyPatch.Restore();
        chaserRoleEventsPatch.Restore();chaserRoleAuthorityPatch.Restore();chaserSelectRolesPatch.Restore();chaserModeLookupPatch.Restore();
        originalModeLookup=nullptr;chaserSelectRoles=nullptr;chaserCanTransit=nullptr;return false;
    }
    if(!InstallPrototypeMenuSelection()) {
        chaserResultVisibilityPatch.Restore();chaserMissingConfigPatch.Restore();chaserLoadingReadyPatch.Restore();
        chaserRoleEventsPatch.Restore();chaserRoleAuthorityPatch.Restore();chaserSelectRolesPatch.Restore();chaserModeLookupPatch.Restore();
        originalModeLookup=nullptr;chaserSelectRoles=nullptr;chaserCanTransit=nullptr;return false;
    }
    // The ordinary speed controller's gain loop repeatedly schedules work in
    // Chaser without satisfying its circuit-only condition (observed >1400
    // queued entries). Defer only these two decisions for our exact owned
    // Chaser bots. Native Tick/base upkeep and gain/hit delegates stay native;
    // the dedicated cadence calls the original Chaser item API/permissions.
    const BYTE speedItemPrefix[]{0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,0x48,0x83,0xec,0x40};
    if(!chaserSpeedItemConditionPatch.Entry(image+0x14c1520,speedItemPrefix,sizeof(speedItemPrefix),
            reinterpret_cast<void*>(ChaserSpeedItemCondition),reinterpret_cast<void**>(&originalChaserSpeedItemCondition)) ||
       !chaserLegacyItemGainPatch.Call(image+0x14e1958,image+0x14e5e30,reinterpret_cast<void*>(ChaserLegacyItemGain))) {
        chaserLegacyItemGainPatch.Restore();chaserSpeedItemConditionPatch.Restore();originalChaserSpeedItemCondition=nullptr;
        chaserResultVisibilityPatch.Restore();chaserMissingConfigPatch.Restore();chaserLoadingReadyPatch.Restore();
        chaserRoleEventsPatch.Restore();chaserRoleAuthorityPatch.Restore();chaserSelectRolesPatch.Restore();chaserModeLookupPatch.Restore();
        originalModeLookup=nullptr;chaserSelectRoles=nullptr;chaserCanTransit=nullptr;return false;
    }
    const BYTE castingPrefix[]{0x40,0x55,0x53,0x41,0x56,0x41,0x57,0x48,0x8d,0x6c,0x24,0xc1,0x48,0x81,0xec,0xc8,0x00,0x00,0x00};
    if(!chaserCastingObserverPatch.Entry(image+0x154bd10,castingPrefix,sizeof(castingPrefix),
            reinterpret_cast<void*>(ObserveChaserCasting),reinterpret_cast<void**>(&originalChaserCasting))) {
        chaserLegacyItemGainPatch.Restore();chaserSpeedItemConditionPatch.Restore();originalChaserSpeedItemCondition=nullptr;
        chaserResultVisibilityPatch.Restore();chaserMissingConfigPatch.Restore();chaserLoadingReadyPatch.Restore();
        chaserRoleEventsPatch.Restore();chaserRoleAuthorityPatch.Restore();chaserSelectRolesPatch.Restore();chaserModeLookupPatch.Restore();
        originalModeLookup=nullptr;chaserSelectRoles=nullptr;chaserCanTransit=nullptr;return false;
    }
    const BYTE endCastingPrefix[]{0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,0x24,0x18,0x57,0x41,0x56,0x41,0x57,0x48,0x83,0xec,0x40};
    if(!chaserEndCastingObserverPatch.Entry(image+0x154c0f0,endCastingPrefix,sizeof(endCastingPrefix),
            reinterpret_cast<void*>(ObserveChaserEndCasting),reinterpret_cast<void**>(&originalChaserEndCasting))) {
        chaserCastingObserverPatch.Restore();originalChaserCasting=nullptr;
        chaserLegacyItemGainPatch.Restore();chaserSpeedItemConditionPatch.Restore();originalChaserSpeedItemCondition=nullptr;
        chaserResultVisibilityPatch.Restore();chaserMissingConfigPatch.Restore();chaserLoadingReadyPatch.Restore();
        chaserRoleEventsPatch.Restore();chaserRoleAuthorityPatch.Restore();chaserSelectRolesPatch.Restore();chaserModeLookupPatch.Restore();
        originalModeLookup=nullptr;chaserSelectRoles=nullptr;chaserCanTransit=nullptr;return false;
    }
    if(!chaserSlotAimObserverPatch.Call(image+0x1555958,image+0x1547f40,reinterpret_cast<void*>(ObserveChaserSlotAim)) ||
       !chaserSlotCastObserverPatch.Call(image+0x15559d7,image+0x22d8ac0,reinterpret_cast<void*>(ObserveChaserSlotCast))) {
        chaserSlotCastObserverPatch.Restore();chaserSlotAimObserverPatch.Restore();
        chaserEndCastingObserverPatch.Restore();originalChaserEndCasting=nullptr;
        chaserCastingObserverPatch.Restore();originalChaserCasting=nullptr;
        chaserLegacyItemGainPatch.Restore();chaserSpeedItemConditionPatch.Restore();originalChaserSpeedItemCondition=nullptr;
        chaserResultVisibilityPatch.Restore();chaserMissingConfigPatch.Restore();chaserLoadingReadyPatch.Restore();
        chaserRoleEventsPatch.Restore();chaserRoleAuthorityPatch.Restore();chaserSelectRolesPatch.Restore();chaserModeLookupPatch.Restore();
        originalModeLookup=nullptr;chaserSelectRoles=nullptr;chaserCanTransit=nullptr;return false;
    }
    bool pressObserved=infiniteBoosterPressPatch.Call(image+0x1548872,image+0x1555690,reinterpret_cast<void*>(ObserveInfiniteBoosterPress));
    RuntimeEvent("native_infinite_booster_press_observer_installed",-1,pressObserved?1:0);
    const BYTE boosterAuthorityPrefix[]{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0xda,0x48,0x8b,0xf9};
    const BYTE boosterAppliedPrefix[]{0x48,0x8b,0xc4,0x48,0x89,0x58,0x18,0x55,0x56,0x57,0x41,0x56,0x41,0x57};
    if(!chaserBoosterAnimationPatch.ChaserBoosterAnimationGuard(image+0x1741b71) ||
       !chaserBoosterSequencePatch.Call(image+0x154bdbf,image+0x17a92e0,reinterpret_cast<void*>(ReadOriginalChaserCastSequence)) ||
       !chaserBoosterAppliedPatch.Entry(image+0x1741630,boosterAppliedPrefix,sizeof(boosterAppliedPrefix),
           reinterpret_cast<void*>(ObserveChaserBoosterApplied),reinterpret_cast<void**>(&originalChaserBoosterApplied)) ||
       !chaserBoosterAuthorityPatch.Entry(image+0x172d830,boosterAuthorityPrefix,sizeof(boosterAuthorityPrefix),
            reinterpret_cast<void*>(DeliverChaserBoosterAuthority),reinterpret_cast<void**>(&originalChaserBoosterAuthority))) {
        chaserBoosterAuthorityPatch.Restore();chaserBoosterAppliedPatch.Restore();chaserBoosterSequencePatch.Restore();
        chaserBoosterAnimationPatch.Restore();
        originalChaserBoosterAuthority=nullptr;originalChaserBoosterApplied=nullptr;
        chaserSlotCastObserverPatch.Restore();chaserSlotAimObserverPatch.Restore();
        chaserEndCastingObserverPatch.Restore();originalChaserEndCasting=nullptr;chaserCastingObserverPatch.Restore();originalChaserCasting=nullptr;
        chaserLegacyItemGainPatch.Restore();chaserSpeedItemConditionPatch.Restore();originalChaserSpeedItemCondition=nullptr;
        chaserResultVisibilityPatch.Restore();chaserMissingConfigPatch.Restore();chaserLoadingReadyPatch.Restore();
        chaserRoleEventsPatch.Restore();chaserRoleAuthorityPatch.Restore();chaserSelectRolesPatch.Restore();chaserModeLookupPatch.Restore();
        originalModeLookup=nullptr;chaserSelectRoles=nullptr;chaserCanTransit=nullptr;return false;
    }
    chaserItemPolicyInstalled=true;
    chaserModeDefinitionEnabled=chaserModeLookupInstalled=true;
    chaserArenaEnabled=arena;
    TwinKartLocalRace::chaserItemContextEnabled=true;
    TwinKartLocalRace::chaserSpawnClassifier=ClassifyOriginalChaserSpawn;
    TwinKartLocalRace::chaserStartSelector=arena?SelectChaserArenaStart:nullptr;
    RuntimeEvent("native_chaser_mode_definition_adapter_installed",-1,108);return true;
}
bool OriginalMode(const TwinKartRace::Request& request,TwinKartRace::ModeDefinition& result) {
    if(GetCurrentThreadId()!=queue.ThreadId())return false;
    if(TwinKartRace::ChaserMode(request.mode)) {
        if(!chaserArenaEnabled || !TwinKartRace::Valid(request))return false;
        auto asset=RetainedChaserDefinition(nullptr);
        using ChaserType=void*(__fastcall*)();
        auto type=reinterpret_cast<ChaserType>(image+0x22b7d40)();
        TwinKartObjective::ChaserModeDefinition original;
        TwinKartRace::ModeDefinition definition;
        // The retained descriptor is an engine-owned CDO. The ordinary cooked
        // instance inspector intentionally rejects RF_ClassDefaultObject; use
        // the already audited exact native CDO reader, preserving that guard.
        if(!asset || !TwinKartObjective::ReadOriginalChaserDefault(type,asset,ModeNameText,original))return false;
        definition.asset=original.asset;definition.kind=original.kind;definition.teamSize=original.teamSize;definition.teamCount=original.teamCount;
        wcscpy_s(definition.gameClass,original.gameClass);wcscpy_s(definition.travelName,original.travelName);
        if(!TwinKartRace::ValidModeDefinition(request,definition))return false;
        result=definition;return true;
    }
    using Type=void*(__fastcall*)();void* type=reinterpret_cast<Type>(image+0x22b8d70)();
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
    int count=*reinterpret_cast<int*>(image+0x69c28c4);BYTE* found=nullptr;
    if(!type || !chunks || count<1 || count>2000000)return false;
    for(int index=0;index<count;++index) {
        auto chunk=chunks[index/65536];if(!chunk || (chunk[index%65536].flags&0x30000000))continue;
        auto object=static_cast<BYTE*>(chunk[index%65536].object);
        if(!TwinKartLocalRace::Live(object) || (*reinterpret_cast<unsigned*>(object+8)&0x18030) || *reinterpret_cast<void**>(object+0x10)!=type)continue;
        if(found)return false;found=object;
    }
    if(!found)return false;
    struct Entries {TwinKartRace::ModeMapEntry* data;int count,capacity;};
    auto entries=*reinterpret_cast<Entries*>(found+0x80);
    if(entries.count<1 || entries.count>512 || entries.capacity<entries.count || entries.capacity>4096)return false;
    return TwinKartRace::ResolveModeBinding(request,entries.data,entries.count,InspectOriginalMode,nullptr,result);
}
bool RegisteredWorldObject(const wchar_t* expected) {
    // Same pinned IAssetRegistry/GetAllAssets ABI used by this project's
    // existing read-only inventory. Retain one bounded engine-owned snapshot
    // until process exit: FAssetData's tag destructor ABI is not guessed here.
    struct Assets {BYTE* data;int count,capacity;};static Assets snapshot{};
    if(!expected || wcsnlen_s(expected,1024)>=1024 || wcsncmp(expected,L"/Game/",6))return false;
    auto registry=*reinterpret_cast<BYTE**>(image+0x699f298);if(!registry)return false;
    auto table=*reinterpret_cast<BYTE***>(registry);
    if(table[0x60/8]<image || table[0x60/8]>=image+0x4ed0000 || table[0x248/8]<image || table[0x248/8]>=image+0x4ed0000)return false;
    if(!snapshot.data) {
        using Loading=bool(__fastcall*)(void*);using All=bool(__fastcall*)(void*,Assets*,bool);
        if(reinterpret_cast<Loading>(table[0x248/8])(registry) ||
           !reinterpret_cast<All>(table[0x60/8])(registry,&snapshot,true))return false;
    }
    if(!snapshot.data || snapshot.count<1 || snapshot.count>snapshot.capacity || snapshot.capacity>1000000)return false;
    unsigned long long worldType=0;using Name=void*(__fastcall*)(void*,const wchar_t*,int);
    reinterpret_cast<Name>(image+0x2589130)(&worldType,L"World",0);if(!worldType)return false;
    int matches=0;
    for(int index=0;index<snapshot.count;++index) {
        auto asset=snapshot.data+static_cast<size_t>(index)*0x60;
        wchar_t name[1024]{};
        if(*reinterpret_cast<unsigned long long*>(asset+0x20)!=worldType)continue;
        if(ModeNameText(*reinterpret_cast<unsigned long long*>(asset),name,ARRAYSIZE(name)) && !wcscmp(name,expected))++matches;
    }
    return matches==1;
}
bool RegisteredCubeWorld(const TwinKartRace::CubeWorld& definition) {
    wchar_t expected[1024]{};
    return TwinKartRace::CubeWorldObjectPath(definition,expected,ARRAYSIZE(expected)) && RegisteredWorldObject(expected);
}
bool HideArenaRoad(const TwinKartRace::ArenaPoint& sample,TwinKartRace::ArenaPoint& contact,void* context) {
    return TwinKartRace::QueryRoad(context,reinterpret_cast<TwinKartRace::RoadTrace>(image+0x3de0910),image+0x6b01810,
        {sample.x,sample.y,sample.z+4000},{sample.x,sample.y,sample.z-4000},contact);
}
bool CurrentChaserScene() {
    return chaserArenaEnabled && chaserScene.world.object && ChaserIdentityLive(chaserScene.world) &&
        OwnedChaserWorld(static_cast<BYTE*>(chaserScene.world.object));
}
void RollbackChaserScene(void*) {
    for(unsigned i=chaserScene.count;i>0;--i) {
        auto id=chaserScene.actors[i-1];
        if(!CurrentChaserScene() || !ChaserIdentityLive(id))continue;
        auto actor=static_cast<BYTE*>(id.object);using GetWorld=BYTE*(__fastcall*)(void*);
        if(reinterpret_cast<GetWorld>((*reinterpret_cast<void***>(actor))[0x160/8])(actor)!=chaserScene.world.object || actor[0x5c]&0x10)continue;
        bool removed=reinterpret_cast<bool(__fastcall*)(void*,bool,bool)>(image+0x3cac200)(actor,false,true);
        RuntimeEvent("native_chaser_partial_scene_actor_retired",static_cast<int>(i-1),removed?1:0);
    }
    chaserScene.count=0;chaserScene.prepared=false;
}
BYTE* LoadChaserVaultClass() {
    if(!CurrentChaserScene())return nullptr;
    if(ChaserIdentityLive(chaserScene.vaultClass))return static_cast<BYTE*>(chaserScene.vaultClass.object);
    TwinKartRace::ObjectiveSoftReference reference{};
    using Name=void(__fastcall*)(void*,const wchar_t*,int);using Load=BYTE*(__fastcall*)(TwinKartRace::ObjectiveSoftReference*);
    reinterpret_cast<Name>(image+0x2589130)(&reference.path.name,L"/Game/Dev/ChaserMode/Vault/VaultActor.VaultActor_C",1);
    auto type=reinterpret_cast<Load>(image+0xa42300)(&reference);
    using Type=BYTE*(__fastcall*)();auto base=reinterpret_cast<Type>(image+0x2257b70)();
    if(!CurrentChaserScene() || !TwinKartLocalRace::Live(type) || !TwinKartLocalRace::Live(base))return nullptr;
    int depth=*reinterpret_cast<int*>(base+0x38),count=*reinterpret_cast<int*>(type+0x38);
    if(depth<0 || depth>count || count>1024 || (*reinterpret_cast<BYTE***>(type+0x30))[depth]!=base+0x30)return nullptr;
    wchar_t name[128]{},package[256]{};auto outer=*reinterpret_cast<BYTE**>(type+0x20);
    if(!TwinKartLocalRace::Live(outer) || !ModeNameText(*reinterpret_cast<unsigned long long*>(type+0x18),name,ARRAYSIZE(name)) ||
       wcscmp(name,L"VaultActor_C") || !ModeNameText(*reinterpret_cast<unsigned long long*>(outer+0x18),package,ARRAYSIZE(package)) ||
       wcscmp(package,L"/Game/Dev/ChaserMode/Vault/VaultActor") || !TwinKartLocalRace::CaptureFlagObject(type,chaserScene.vaultClass))return nullptr;
    return type;
}
bool SpawnChaserSceneActor(TwinKartObjective::ChaserSceneActor kind,unsigned index,
    const TwinKartObjective::ObjectiveTransform& transform,void*) {
    if(!CurrentChaserScene() || chaserScene.count>=ARRAYSIZE(chaserScene.actors))return false;
    using Type=void*(__fastcall*)();using GetWorld=BYTE*(__fastcall*)(void*);
    auto type=kind==TwinKartObjective::ChaserSceneActor::Vault?static_cast<void*>(LoadChaserVaultClass()):
        reinterpret_cast<Type>(image+(kind==TwinKartObjective::ChaserSceneActor::Start?0x450c7b0:0x22591c0))();
    if(!type || !CurrentChaserScene())return false;
    auto begin=reinterpret_cast<TwinKartObjective::BeginObjectiveActor>(image+0x3f37eb0);
    auto finish=reinterpret_cast<TwinKartObjective::FinishObjectiveActor>(image+0x3f41030);
    auto actor=static_cast<BYTE*>(begin(chaserScene.world.object,type,&transform,1,nullptr));
    TwinKartCamera::ObjectIdentity id{};
    if(!TwinKartLocalRace::Live(actor) || !TwinKartLocalRace::CaptureFlagObject(actor,id))return false;
    // Retain the created identity before FinishSpawning can invoke callbacks,
    // so a failing finish participates in rollback rather than leaking a prop.
    chaserScene.actors[chaserScene.count++]=id;
    if(!CurrentChaserScene() || finish(actor,&transform)!=actor || !ChaserIdentityLive(id) ||
       reinterpret_cast<GetWorld>((*reinterpret_cast<void***>(actor))[0x160/8])(actor)!=chaserScene.world.object ||
       !TwinKartRace::ObjectiveAssetIsA(image,actor,kind==TwinKartObjective::ChaserSceneActor::Start?0x450c7b0:
            kind==TwinKartObjective::ChaserSceneActor::Vault?0x2257b70:0x22591c0))return false;
    // The original delivery actor's native collision component is functional
    // without the absent decorative plate blueprint dependency. Its original
    // overlap/pickup/reward operations remain in the native game.
    if(kind==TwinKartObjective::ChaserSceneActor::Zone) {
        auto collision=*reinterpret_cast<BYTE**>(actor+0x220);
        if(!TwinKartLocalRace::Live(collision) || *reinterpret_cast<void**>(collision+0xa0)!=actor)return false;
    }
    RuntimeEvent(kind==TwinKartObjective::ChaserSceneActor::Start?"native_chaser_compat_start_spawned":
        kind==TwinKartObjective::ChaserSceneActor::Vault?"native_chaser_original_vault_spawned":"native_chaser_original_delivery_zone_spawned",
        static_cast<int>(index),1);
    return true;
}
bool PrepareChaserArenaImpl(BYTE* world) {
    if(!chaserArenaEnabled || !OwnedChaserWorld(world))return false;
    TwinKartCamera::ObjectIdentity worldId{};if(!TwinKartLocalRace::CaptureFlagObject(world,worldId))return false;
    if(chaserScene.world.object!=world || chaserScene.world.serial!=worldId.serial) {chaserScene={};chaserScene.world=worldId;}
    if(chaserScene.prepared)return CurrentChaserScene();
    if(chaserScene.rejected || chaserScene.building || GetTickCount64()<chaserScene.retry)return false;
    chaserScene.retry=GetTickCount64()+1000;
    wchar_t worldName[128]{};
    if(!ModeNameText(*reinterpret_cast<unsigned long long*>(world+0x18),worldName,ARRAYSIZE(worldName)) || wcscmp(worldName,L"Flag_F01"))return false;
    unsigned long long phase=0;auto game=*reinterpret_cast<unsigned long long*>(image+TwinKartGraphics::ChaserGameNameRva);
    if(ChaserPhase(world,phase) && game && phase==game)return false; // Never add objectives after play begins.
    // Native class getters and loading can allocate objects. Do not retain a
    // GUObjectArray chunk-table pointer across those calls or inspect unrelated
    // objects left over from the previous world. Borrow the engine's bounded
    // current-world actor snapshots and recheck their serials before use.
    TwinKartCamera::ObjectIdentity startActors[64]{},zoneActors[64]{};
    unsigned startCount=0,zoneCount=0;
    if(!CollectChaserObjectiveActors(world,0x450c7b0,startActors,startCount) ||
       !CollectChaserObjectiveActors(world,0x22add30,zoneActors,zoneCount))return false;
    if(startCount<8 || zoneCount>8)return false;
    TwinKartRace::ArenaPoint starts[64]{},zones[8]{};
    for(unsigned i=0;i<startCount;++i)
        if(!ChaserIdentityLive(startActors[i]) ||
           !TwinKartLocalRace::FlagPosition(static_cast<BYTE*>(startActors[i].object),starts[i]))return false;
    for(unsigned i=0;i<zoneCount;++i)
        if(!ChaserIdentityLive(zoneActors[i]) ||
           !TwinKartLocalRace::FlagPosition(static_cast<BYTE*>(zoneActors[i].object),zones[i]))return false;
    if(!ChaserIdentityLive(worldId) || !OwnedChaserWorld(world))return false;
    TwinKartObjective::ChaserArenaPlan plan;
    if(!TwinKartObjective::BuildChaserArenaPlan(zoneCount>=2?zones:starts,static_cast<int>(zoneCount>=2?zoneCount:startCount),HideArenaRoad,world,plan))return false;
    if(!LoadChaserVaultClass())return false;
    chaserScene.building=true;
    bool prepared=TwinKartObjective::SpawnChaserScene(plan,SpawnChaserSceneActor,RollbackChaserScene,nullptr);
    chaserScene.building=false;chaserScene.prepared=prepared && CurrentChaserScene() && chaserScene.count==13;
    chaserScene.rejected=!chaserScene.prepared;
    chaserObjectiveRefresh=0;
    RuntimeEvent("native_chaser_original_art_scene_prepared",-1,chaserScene.prepared?1:0);
    return chaserScene.prepared;
}
int ChaserScenePreparationFault(EXCEPTION_POINTERS* error) {
    if(error && error->ExceptionRecord) {
        auto address=reinterpret_cast<std::uintptr_t>(error->ExceptionRecord->ExceptionAddress);
        auto gameBase=reinterpret_cast<std::uintptr_t>(image),bridgeBase=reinterpret_cast<std::uintptr_t>(module);
        char row[384];int length=sprintf_s(row,"{\"event\":\"native_chaser_scene_preparation_fault\",\"code\":%lu,\"game_rva\":%llu,\"bridge_rva\":%llu,\"gameplay_accepted\":false}\r\n",
            error->ExceptionRecord->ExceptionCode,
            address>=gameBase && address-gameBase<0x8657000?static_cast<unsigned long long>(address-gameBase):0ull,
            address>=bridgeBase && address-bridgeBase<0x1000000?static_cast<unsigned long long>(address-bridgeBase):0ull);
        DWORD written=0;if(length>0 && output!=INVALID_HANDLE_VALUE)WriteFile(output,row,static_cast<DWORD>(length),&written,nullptr);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}
bool PrepareChaserArena(BYTE* world) {
    __try {return PrepareChaserArenaImpl(world);}
    __except(ChaserScenePreparationFault(GetExceptionInformation())) {
        __try {RollbackChaserScene(nullptr);} __except(EXCEPTION_EXECUTE_HANDLER) {}
        chaserScene.building=false;chaserScene.rejected=true;
        RuntimeEvent("native_chaser_scene_preparation_rejected",-1,0);return false;
    }
}
bool SelectChaserArenaStart(void* raw,void* rawMode,int slot,void*& selected) {
    __try {
        if(!chaserArenaEnabled || !chaserModeDefinitionEnabled || GetCurrentThreadId()!=queue.ThreadId() || slot<0 || slot>=8)return false;
        auto rule=static_cast<BYTE*>(raw),mode=static_cast<BYTE*>(rawMode);
        using Type=void*(__fastcall*)();using GetWorld=BYTE*(__fastcall*)(void*);
        if(!TwinKartLocalRace::Live(rule) || *reinterpret_cast<BYTE**>(rule)!=image+0x51dd410 ||
           !TwinKartLocalRace::Live(mode) || *reinterpret_cast<void**>(mode+0x10)!=reinterpret_cast<Type>(image+0x22507c0)() ||
           *reinterpret_cast<void**>(rule+0xa0)!=mode)return false;
        auto world=TwinKartLocalRace::World(rule);
        if(!OwnedChaserWorld(world) || *reinterpret_cast<void**>(world+0x118)!=mode || !PrepareChaserArena(world))return false;
        auto id=chaserScene.actors[slot];auto actor=static_cast<BYTE*>(id.object);TwinKartRace::ArenaPoint p{};
        if(!ChaserIdentityLive(id) || !TwinKartRace::ObjectiveAssetIsA(image,actor,0x450c7b0) ||
           reinterpret_cast<GetWorld>((*reinterpret_cast<void***>(actor))[0x160/8])(actor)!=world ||
           !TwinKartLocalRace::FlagPosition(actor,p) || !CurrentChaserScene())return false;
        // Original spawn1926150 reads R9's actor root+130 and its transform+1d0
        // for both human and AI. Pass the actual original PlayerStart actor,
        // not a made-up transform buffer, and leave native spawn/physics intact.
        selected=actor;RuntimeEvent("native_chaser_original_start_selected",slot,1);return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
#include "HideRepairedSceneRuntime.inl"
bool PrepareHideArena(BYTE* world,BYTE* mode) {
    if(hideRepairedSceneEnabled && !hideArenaCapture)return PrepareHideRepairedScene(world,mode);
    if(!hideArenaCapture)return true;
    if(hideArenaPrepared)return true;
    if(hideArenaPreparationAttempted || GetCurrentThreadId()!=queue.ThreadId() || !TwinKartLocalRace::Live(world) ||
       !TwinKartLocalRace::objectiveWorld.Matches(world,*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),
          *reinterpret_cast<int*>(image+0x69c28c4)))return false;
    static ULONGLONG next=0;if(GetTickCount64()<next)return false;next=GetTickCount64()+1000;
    wchar_t worldName[128]{};
    if(!ModeNameText(*reinterpret_cast<unsigned long long*>(world+0x18),worldName,ARRAYSIZE(worldName)) ||
       wcscmp(worldName,L"Flag_F01"))return false;
    using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);using GetWorld=BYTE*(__fastcall*)(void*);
    auto spawnType=reinterpret_cast<Type>(image+0x10f46e0)();
    auto spawner=reinterpret_cast<Find>((*reinterpret_cast<void***>(mode))[0x620/8])(mode,spawnType);
    if(!TwinKartLocalRace::OwnedComponent(spawner,mode,spawnType))return false;
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);int count=*reinterpret_cast<int*>(image+0x69c28c4);
    TwinKartRace::ArenaPoint anchors[64]{},zones[8]{};int anchorCount=0,zoneCount=0;
    for(int index=0;index<count;++index) {
        auto chunk=chunks[index/65536];if(!chunk || (chunk[index%65536].flags&0x30000000))continue;
        auto actor=static_cast<BYTE*>(chunk[index%65536].object);
        if(!TwinKartLocalRace::Live(actor) || (*reinterpret_cast<unsigned*>(actor+8)&0x18030))continue;
        bool start=TwinKartRace::ObjectiveAssetIsA(image,actor,0x450c7b0);
        bool zone=TwinKartRace::ObjectiveAssetIsA(image,actor,0x22add30);
        if((!start && !zone) || reinterpret_cast<GetWorld>((*reinterpret_cast<void***>(actor))[0x160/8])(actor)!=world ||
           (actor[0x5c]&0x10))continue;
        TwinKartRace::ArenaPoint point{};if(!TwinKartLocalRace::FlagPosition(actor,point))continue;
        if(start && anchorCount<64)anchors[anchorCount++]=point;
        if(zone && zoneCount<8)zones[zoneCount++]=point;
    }
    TwinKartObjective::ObjectiveArenaPlan plan;
    if(!TwinKartObjective::BuildObjectiveArenaPlan(zoneCount>=2?zones:anchors,zoneCount>=2?zoneCount:anchorCount,
        HideArenaRoad,world,plan))return false; // Original road/sublevels may still be loading.
    hideArenaPreparationAttempted=true;
    auto begin=reinterpret_cast<TwinKartObjective::BeginObjectiveActor>(image+0x3f37eb0);
    auto finish=reinterpret_cast<TwinKartObjective::FinishObjectiveActor>(image+0x3f41030);
    auto playerStartType=reinterpret_cast<Type>(image+0x450c7b0)();
    using Name=void*(__fastcall*)(void*,const wchar_t*,int);
    for(int index=0;index<16;++index) {
        auto actor=static_cast<BYTE*>(TwinKartObjective::SpawnObjectiveActor(world,playerStartType,plan.starts[index],begin,finish));
        if(!TwinKartLocalRace::Live(actor) || reinterpret_cast<GetWorld>((*reinterpret_cast<void***>(actor))[0x160/8])(actor)!=world) {
            RuntimeEvent("native_hide_arena_start_spawn_rejected",index,0);return false;
        }
        // Original APlayerStart::PlayerStartTag, independently verified by
        // generated property metadata and InitPlayerStart's actual consumer.
        reinterpret_cast<Name>(image+0x2589130)(actor+0x248,index<8?L"HiderStart":L"SeekerStart",1);
    }
    auto areaType=reinterpret_cast<Type>(image+0x10f3610)();
    for(int index=0;index<3;++index) {
        auto actor=static_cast<BYTE*>(TwinKartObjective::SpawnObjectiveActor(world,areaType,plan.areas[index],begin,finish));
        if(!TwinKartLocalRace::Live(actor) || reinterpret_cast<GetWorld>((*reinterpret_cast<void***>(actor))[0x160/8])(actor)!=world ||
           !TwinKartRace::ObjectiveAssetIsA(image,actor,0x10f3610)) {
            RuntimeEvent("native_hide_arena_prop_area_rejected",index,0);return false;
        }
        // Instance-only reflected NumberOfPropsToSpawn. Native rule callbacks
        // retain selection, original prop assets, collision and disguise logic.
        *reinterpret_cast<int*>(actor+0x238)=16;
    }
    // The original BeginPlay scan predates these compatibility start actors.
    // Repeat its original reset/discovery operation, never forge its arrays.
    reinterpret_cast<void(__fastcall*)(void*)>(image+0x10ed040)(spawner);
    int hiders=*reinterpret_cast<int*>(spawner+0xd8),seekers=*reinterpret_cast<int*>(spawner+0xe8);
    hideArenaPrepared=hiders==8 && seekers==8;
    RuntimeEvent("native_hide_arena_original_hider_starts",-1,hiders);
    RuntimeEvent("native_hide_arena_original_seeker_starts",-1,seekers);
    RuntimeEvent("native_hide_formal_art_arena_prepared",-1,hideArenaPrepared?1:0);
    return hideArenaPrepared;
}
void ObserveHideCapture(BYTE* world) {
    // An explicit diagnostic request may release the original one-player
    // developer-world wait. This does not populate bots or claim a match.
    // All roles, spawning, timers and stage transitions remain native.
    if(!hideCaptureArmed || hideWaitRequested || GetCurrentThreadId()!=queue.ThreadId() || OwnedRaceWorld()!=world)return;
    using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);using GetWorld=BYTE*(__fastcall*)(void*);
    auto mode=*reinterpret_cast<BYTE**>(world+0x118),state=*reinterpret_cast<BYTE**>(world+0x120);
    if(!TwinKartLocalRace::Live(state) || *reinterpret_cast<void**>(state+0x10)!=reinterpret_cast<Type>(image+0x10f3a10)())return;
    if(hideArenaCapture || hideRepairedSceneEnabled) {
        // Formal art sublevels can become queryable while native component
        // initialization is still pending. ForceBegin must not emit a stage
        // notification into an empty original phase map.
        using Begun=bool(__fastcall*)(void*);
        if((*reinterpret_cast<BYTE***>(state))[0x648/8]!=image+0x3f4a460 ||
           !reinterpret_cast<Begun>(image+0x3f4a460)(state))return;
        auto phaseType=reinterpret_cast<Type>(image+0x10f3c60)();
        auto phase=reinterpret_cast<Find>((*reinterpret_cast<void***>(state))[0x620/8])(state,phaseType);
        if(!TwinKartLocalRace::OwnedComponent(phase,state,phaseType) ||
           !TwinKartLocalRace::objectiveWorld.Matches(TwinKartLocalRace::World(phase),
               *reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),*reinterpret_cast<int*>(image+0x69c28c4)))return;
        unsigned long long setup=0,grant=0;using Name=void*(__fastcall*)(void*,const wchar_t*,int);
        reinterpret_cast<Name>(image+0x2589130)(&setup,L"OneTimeSetup",1);
        reinterpret_cast<Name>(image+0x2589130)(&grant,L"GrantRoles",1);
        void* first=nullptr;void* second=nullptr;
        const auto& phases=*reinterpret_cast<const TwinKartObjective::HidePhaseMap*>(phase+0x118);
        if(!TwinKartObjective::HidePhaseInitializationReady(phases,setup,grant,first,second) ||
           !TwinKartLocalRace::Live(static_cast<BYTE*>(first)) || !TwinKartLocalRace::Live(static_cast<BYTE*>(second))) {
            static ULONGLONG next=0;
            if(GetTickCount64()>=next){next=GetTickCount64()+5000;RuntimeEvent("native_hide_arena_waiting_for_original_phase_initialization",-1,phases.count);}
            return;
        }
    }
    if(!PrepareHideArena(world,mode))return;
    auto waitType=reinterpret_cast<Type>(image+0x23b4a90)();
    auto wait=reinterpret_cast<Find>((*reinterpret_cast<void***>(mode))[0x620/8])(mode,waitType);
    auto rulesType=reinterpret_cast<Type>(image+0x10f3df0)();
    auto rules=reinterpret_cast<Find>((*reinterpret_cast<void***>(mode))[0x620/8])(mode,rulesType);
    if(!TwinKartLocalRace::OwnedComponent(wait,mode,waitType) || !TwinKartLocalRace::OwnedComponent(rules,mode,rulesType))return;
    auto config=*reinterpret_cast<BYTE**>(rules+0xb0);
    if(!TwinKartLocalRace::Live(config) || *reinterpret_cast<void**>(config+0x10)!=reinterpret_cast<Type>(image+0x10f3b90)())return;
    TwinKartObjective::HideRoles roles;
    if(!TwinKartObjective::ReadHideRoles(*reinterpret_cast<const TwinKartObjective::HideRoleMap*>(config+0x60),hideControllerTarget,roles))return;
    if(hideControllerTarget>1 && !TwinKartObjective::CompetitiveHideRoles(roles,hideControllerTarget))return;
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);int count=*reinterpret_cast<int*>(image+0x69c28c4);
    auto controllerType=reinterpret_cast<Type>(image+0x10f3d30)();int controllers=0;
    if(!chunks || count<1 || count>2000000)return;
    for(int index=0;index<count;++index) {
        auto chunk=chunks[index/65536];if(!chunk || (chunk[index%65536].flags&0x30000000))continue;
        auto object=static_cast<BYTE*>(chunk[index%65536].object);
        if(!TwinKartLocalRace::Live(object) || (*reinterpret_cast<unsigned*>(object+8)&0x18030) ||
           *reinterpret_cast<void**>(object+0x10)!=controllerType)continue;
        if(reinterpret_cast<GetWorld>((*reinterpret_cast<void***>(object))[0x160/8])(object)==world)++controllers;
    }
    if(hideControllerTarget==8 && controllers==1 && !hideBotPreparationAttempted) {
        // The original Hide spawner assigns roles and creates Hider/Seeker
        // pawns from PLAYER CONTROLLERS, not MajorConnect's circuit AI seats.
        // Use the original mode's SpawnPlayerController(options) interface.
        // These seven native controllers have no LocalPlayer or viewport.
        hideBotPreparationAttempted=true;
        auto table=*reinterpret_cast<BYTE***>(mode);
        if(*reinterpret_cast<BYTE**>(mode)!=image+0x5018558 || table[239]!=image+0x3f5a780 ||
           table[251]!=image+0x3f4b7e0 || table[252]!=image+0x3f41310)return;
        NativeString options{};using SpawnController=BYTE*(__fastcall*)(void*,unsigned char,const NativeString*);
        for(int slot=1;slot<8;++slot) {
            auto controller=reinterpret_cast<SpawnController>(table[239])(mode,2,&options);
            if(!TwinKartLocalRace::Live(controller) || *reinterpret_cast<void**>(controller+0x10)!=controllerType ||
               reinterpret_cast<GetWorld>((*reinterpret_cast<void***>(controller))[0x160/8])(controller)!=world) {
                RuntimeEvent("native_hide_controller_preparation_rejected",slot,0);return;
            }
            // SpawnPlayerController constructs the actor. InitNewPlayer assigns
            // its original player ID, then GenericPlayerInitialization applies
            // original controller settings. PostLogin requires an actual UPlayer
            // or network connection; these local bots intentionally have neither.
            if(!TwinKartObjective::InitializeHideController(mode,controller,slot,
                reinterpret_cast<TwinKartObjective::InitializeLocalPlayer>(table[251]),
                reinterpret_cast<TwinKartObjective::OriginalControllerInitialization>(table[252]),
                reinterpret_cast<TwinKartObjective::ReleaseString>(image+0x248b950))) {
                RuntimeEvent("native_hide_controller_login_rejected",slot,0);return;
            }
            RuntimeEvent("native_hide_original_bot_controller_spawned",slot,1);
        }
        hideBotPreparationSucceeded=true;
        return; // Let original initialization finish before the next census.
    }
    if(hideControllerTarget==8 && !hideBotPreparationSucceeded)return;
    if(controllers!=hideControllerTarget)return;
    if(!wait[0xc8] && !wait[0xc9])reinterpret_cast<void(__fastcall*)(void*)>(image+0x1638220)(wait);
    hideWaitRequested=wait[0xc8] || wait[0xc9];
    if(hideWaitRequested) {
        RuntimeEvent("native_hide_original_wait_requested",-1,controllers);
        RuntimeEvent("native_hide_configured_hiders",-1,roles.hiders);
        RuntimeEvent("native_hide_configured_seekers",-1,roles.seekers);
    }
}
void DumpEventProfileLabels();
bool NameLocalEventCard(void* asset,int mode) {
    const bool cube=TwinKartRace::CubeMode(mode);
    if(!asset || (!cube && !TwinKartRace::InfiniteBoostMode(mode)))return false;
    const int index=cube?(mode==116?3:4):mode-101;
    // These are already registered native ModeLab data assets. Give their
    // existing reflected labels a useful Chinese name while preserving all
    // original textures, enum IDs, group sizes and mode/rule selection.
    static TwinKartObjective::ChaserText texts[5][2]{};
    static std::uint64_t names[5]{};
    static const wchar_t* properties[]{L"Title",L"ShortName",L"FullName",L"LoadingName",L"ResultName"};
    static const wchar_t* titles[]{L"\u65e0\u9650\u52a0\u901f\uff08\u4e2a\u4eba\uff09",
        L"\u65e0\u9650\u52a0\u901f\uff08\u53cc\u4eba\uff09",L"\u65e0\u9650\u52a0\u901f\uff08\u56db\u4eba\uff09",
        L"\u7acb\u65b9\u4f53\u9053\u5177\u8d5b\uff08\u4e2a\u4eba\uff09",L"\u7acb\u65b9\u4f53\u9053\u5177\u8d5b\uff08\u56db\u4eba\uff09"};
    const auto create=reinterpret_cast<TwinKartObjective::CreateChaserText>(image+0x3fe0a60);
    const auto name=reinterpret_cast<TwinKartObjective::NameChaserText>(image+0x2589130);
    const auto set=reinterpret_cast<TwinKartObjective::SetChaserText>(image+0x3ffe670);
    auto& values=texts[index];
    for(int i=0;i<2;++i)if(!values[i].references) {
        const auto label=i?(cube?L"\u7acb\u65b9\u4f53\u9053\u5177\u8d5b":L"\u65e0\u9650\u52a0\u901f"):titles[index];
        const int size=static_cast<int>(wcslen(label)+1);
        const TwinKartEquipment::String input{label,size,size};
        if(create(&values[i],&input)!=&values[i] || !values[i].data || !values[i].references)return false;
    }
    for(unsigned i=0;i<ARRAYSIZE(properties);++i) {
        if(!names[i])name(&names[i],properties[i],1);
        if(!names[i])return false;
        set(asset,names[i],&values[i==0 || i==2?0:1]);
    }
    return true;
}
bool ApplyOriginalEventMenu() {
    static bool completed=false;if(completed)return true;
    wchar_t enabled[4]{};
    if(GetEnvironmentVariableW(L"TWIN_KART_EVENT_MODES",enabled,4)!=1 || enabled[0]!=L'1')return true;
    if(GetCurrentThreadId()!=queue.ThreadId())return false;
    using Type=void*(__fastcall*)();auto type=reinterpret_cast<Type>(image+0x4425db0)();
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
    int count=*reinterpret_cast<int*>(image+0x69c28c4);BYTE* found=nullptr;
    if(!type || !chunks || count<1 || count>2000000)return false;
    for(int index=0;index<count;++index) {
        auto chunk=chunks[index/65536];if(!chunk || (chunk[index%65536].flags&0x30000000))continue;
        auto object=static_cast<BYTE*>(chunk[index%65536].object);
        if(!TwinKartLocalRace::Live(object) || (*reinterpret_cast<unsigned*>(object+8)&0x18030) || *reinterpret_cast<void**>(object+0x10)!=type)continue;
        wchar_t name[96]{};if(!ModeNameText(*reinterpret_cast<unsigned long long*>(object+0x18),name,ARRAYSIZE(name)) || wcscmp(name,L"KtModeLabSetDef"))continue;
        auto rowType=*reinterpret_cast<BYTE**>(object+0x28);wchar_t rowName[96]{};
        if(!TwinKartLocalRace::Live(rowType) || !ModeNameText(*reinterpret_cast<unsigned long long*>(rowType+0x18),rowName,ARRAYSIZE(rowName)) || wcscmp(rowName,L"KtClientModeLabDefSetDef"))return false;
        if(found)return false;found=object;
    }
    if(!found || *reinterpret_cast<BYTE**>(found)!=image+0x5b97a00 || (*reinterpret_cast<BYTE***>(found))[0x2a8/8]!=image+0x3eac100)return false;
    struct TableEntry {unsigned long long key;BYTE* row;int next,hash;};
    using TableRows=TwinKartRace::MenuArray<TableEntry>;
    auto table=*reinterpret_cast<TableRows*>(found+0x30);
    if(!table.data || table.count!=1 || table.capacity<table.count || table.capacity>4096 || !table.data[0].row)return false;
    auto original=*reinterpret_cast<const TwinKartRace::EventMenuRowSet*>(table.data[0].row);
    if(!original.baseVtable || !original.rows.data || original.rows.count<1 || original.rows.count>64 || original.rows.capacity<original.rows.count)return false;
    size_t ruleCount=0;auto rules=TwinKartRace::EventProfiles(ruleCount);
    for(size_t i=0;i<ruleCount;++i) {
        TwinKartRace::Request request;strcpy_s(request.identity,"00000000000000000000000000000000");strcpy_s(request.track,"Village_I04");
        request.mode=rules[i].mode;strcpy_s(request.profile,rules[i].profile);TwinKartRace::ModeDefinition definition;
        if(!OriginalMode(request,definition))return false;
    }
    size_t localCount=0;auto localModes=TwinKartRace::LocalLobbyModes(localCount);
    for(size_t i=0;i<localCount;++i) {
        TwinKartRace::Request request;strcpy_s(request.identity,"00000000000000000000000000000000");
        strcpy_s(request.track,localModes[i]==104?"Flag_F01":"Village_I04");request.mode=localModes[i];
        TwinKartRace::ModeDefinition definition;
        if(!OriginalMode(request,definition)) {RuntimeEvent("native_local_lobby_mode_definition_missing",localModes[i],0);return false;}
        if(TwinKartRace::InfiniteBoostMode(request.mode) || TwinKartRace::CubeMode(request.mode)) {
            if(!NameLocalEventCard(definition.asset,request.mode))return false;
            RuntimeEvent(TwinKartRace::CubeMode(request.mode)?"native_local_cube_card_named":"native_local_unlimited_boost_card_named",request.mode,1);
        }
    }
    bool includeFactory=TwinKartLocalRace::factoryCompatEnabled;
    if(includeFactory && !TwinKartLocalRace::Live(static_cast<BYTE*>(reinterpret_cast<Type>(image+0x24345d0)())))return false;
    if(includeFactory && !AuthoredPrototypeDefinition(100))return false;
    if(chaserArenaEnabled) {
        TwinKartRace::Request chaser;strcpy_s(chaser.identity,"00000000000000000000000000000000");strcpy_s(chaser.track,"Flag_F01");chaser.mode=108;
        TwinKartRace::ModeDefinition definition;if(!OriginalMode(chaser,definition))return false;
    }
    if(hideLobbyEnabled && !AuthoredHideDefinition())return false;
    const bool freeDrive=practiceTicketPath[0] && AuthoredPrototypeDefinition(1);
    TwinKartRace::EventMenuCopySource source;if(!source.Build(original.baseVtable,includeFactory,chaserArenaEnabled,hideLobbyEnabled,freeDrive))return false;
    // Original AddRow removes the old row, initializes an engine-owned
    // destination and invokes UScriptStruct::CopyScriptStruct. Our source
    // contains only independently built menu values and borrowed string
    // views; it never takes ownership of the old table's arrays or strings.
    using AddRow=void(__fastcall*)(void*,unsigned long long,const void*);
    auto key=table.data[0].key;reinterpret_cast<AddRow>(image+0x3eac100)(found,key,&source.source);
    table=*reinterpret_cast<TableRows*>(found+0x30);
    if(!table.data || table.count!=1 || table.data[0].key!=key || !table.data[0].row)return false;
    auto copied=*reinterpret_cast<const TwinKartRace::EventMenuRowSet*>(table.data[0].row);
    if(copied.rows.count!=source.source.rows.count || !copied.rows.data || copied.rows.data==source.source.rows.data)return false;
    for(int i=0;i<copied.rows.count;++i) {
        auto& row=copied.rows.data[i];auto& expected=source.source.rows.data[i];
        if(!TwinKartEquipment::Valid(row.id,128) || row.id.data==expected.id.data || wcscmp(row.id.data,expected.id.data) ||
           row.modes.count!=1 || !row.modes.data || row.modes.data==expected.modes.data ||
           row.modes.data[0].mode!=expected.modes.data[0].mode ||
           row.modes.data[0].profile.data==expected.modes.data[0].profile.data ||
           wcscmp(row.modes.data[0].profile.data,expected.modes.data[0].profile.data))return false;
    }
    completed=true;RuntimeEvent("native_event_menu_rows_installed",-1,copied.rows.count);
    if(includeFactory)RuntimeEvent("native_factory_menu_entry_installed",-1,100);
    if(chaserArenaEnabled)RuntimeEvent("native_chaser_menu_entry_installed",-1,108);
    if(hideLobbyEnabled)RuntimeEvent("native_hide_repaired_menu_entry_installed",-1,109);
    if(freeDrive)RuntimeEvent("native_free_drive_menu_entry_installed",-1,1);
    DumpEventProfileLabels();return true;
}
void DumpEventTrackResource(const wchar_t* alias,const BYTE* record);
bool PrepareFactoryCapture(void* traveler,const TwinKartRace::Request& request) {
    wchar_t options[512]{};
    if(!traveler || !TwinKartFactory::CaptureOptions(request,options,ARRAYSIZE(options)))return false;
    wchar_t track[97]{};size_t n=strlen(request.track);for(size_t i=0;i<n;++i)track[i]=request.track[i];
    TwinKartRace::PreparationString view{track,static_cast<int>(n+1),static_cast<int>(n+1)},owned{};
    auto copy=reinterpret_cast<TwinKartRace::CopyPreparationString>(image+0x923880);
    if(copy(&owned,&view)!=&owned || !owned.data || owned.data==track || owned.count!=view.count || owned.capacity<owned.count)return false;
    TwinKartRace::PreparationMode mode{};mode.mode=100;TwinKartRace::PreparationHints hints{};
    return reinterpret_cast<TwinKartRace::InitializeNativeRace>(image+0x1bba140)(traveler,&mode,&owned,nullptr,&hints,false);
}
bool RetainHideLoadingPresentation(BYTE* traveler,const TwinKartRace::Request& request);
int NativeHideCapture(StartQuery& query) {
    // This original development world has no FKtTrackDef/circuit metadata.
    // Keep it on a separate, exact-world contract. In particular,
    // never manufacture a track definition or use ordinary MajorConnect AI.
    query.stage=1;
    if(!TwinKartObjective::HideCaptureOptions(query.request,query.options,ARRAYSIZE(query.options)))return -17;
    BYTE* traveler=Traveler();using Model=void*(__fastcall*)(void*);
    if(!traveler || !reinterpret_cast<Model>(image+0x1ba3cf0)(traveler))return 0;
    if(OwnedRaceWorld())return -9;
    query.stage=2;
    bool arena=!strcmp(query.request.track,"Flag_F01");
    if(!RegisteredWorldObject(arena?TwinKartObjective::HideArenaObject:TwinKartObjective::HideWorldObject))return -18;
    using Type=void*(__fastcall*)();
    if(!TwinKartLocalRace::Live(static_cast<BYTE*>(reinterpret_cast<Type>(image+0x10f34f0)())))return -18;
    using GetWorld=BYTE*(__fastcall*)(void*);
    auto world=reinterpret_cast<GetWorld>((*reinterpret_cast<void***>(traveler))[0x160/8])(traveler);
    if(!TwinKartLocalRace::Live(world) || *reinterpret_cast<void**>(world+0x38))return -6;
    auto& route=*reinterpret_cast<TwinKartRace::ReturnString*>(traveler+0xd8);
    auto& map=*reinterpret_cast<TwinKartRace::ReturnString*>(world+0x4a8);
    if(TwinKartRace::PrepareReturnRoute(route,map,true,reinterpret_cast<TwinKartRace::AssignReturnString>(image+0x9118f0))<0)return -6;
    // Reflected execOpenLevel at 4446750 passes UObject*, FName by value,
    // bool, FString by value to this exact original interface at 3f50670.
    // The source-level contract cannot supply another package or options.
    unsigned long long level=0;using Name=void*(__fastcall*)(void*,const wchar_t*,int);
    reinterpret_cast<Name>(image+0x2589130)(&level,arena?TwinKartObjective::HideArenaPackage:TwinKartObjective::HideWorldPackage,1);
    if(!level)return -18;
    ReleaseHideRepairedScene();
    ResetChaserSession();
    TwinKartLocalRace::armed=false;TwinKartLocalRace::raceWorld.Reset();TwinKartLocalRace::factoryWorld.Reset();
    TwinKartLocalRace::objectiveWorld.Reset();hideCaptureArmed=true;hideWaitRequested=false;
    hideArenaCapture=arena;hideArenaPrepared=hideArenaPreparationAttempted=false;
    hideControllerTarget=query.request.ai+1;hideBotPreparationAttempted=false;
    hideBotPreparationSucceeded=false;
    hideTickSamples=0;hideTickSeconds=0;
    hideStageSamples=0;
    hideSearchClock.Stop();hideSearchOwner={};hideDeadlineSignalOwner={};hideDeadlineSignal=nullptr;
    hideRoundRosterKnown=hideRoundRosterPending=false;hideRosterOwner={};hideRosterRetry=0;
    memset(hideRoundSeats,0,sizeof(hideRoundSeats));memset(hideRoundPawns,0,sizeof(hideRoundPawns));
    memset(hideBotInputIsolated,0,sizeof(hideBotInputIsolated));
    hideInputCaptureDone=hideSearchActive=false;hideInputPawn={};hideInputController={};hideInputSeconds=0;
    hideRoundInputsActive=false;
    hideSeekerInputDone=false;hideSeekerInputPawn={};hideSeekerInputMovement={};hideSeekerInputController={};hideSeekerInputSeconds=0;
    memset(hideSeekerAppearanceAttempted,0,sizeof(hideSeekerAppearanceAttempted));
    memset(hideSeekerAppearanceOwner,0,sizeof(hideSeekerAppearanceOwner));
    memset(hideSeekerAppearanceRequests,0,sizeof(hideSeekerAppearanceRequests));
    memset(hideSeekerMeshMask,0,sizeof(hideSeekerMeshMask));
    memset(hideSeekerAppearanceStatus,0,sizeof(hideSeekerAppearanceStatus));
    memset(hideSeekerAppearanceRetry,0,sizeof(hideSeekerAppearanceRetry));
    InterlockedExchange64(&hideObservedMovement,0);InterlockedExchange(&hideObservedSubsteps,0);InterlockedExchange(&hideObservedBodies,0);
    if(hideLobbyEnabled)RetainHideLoadingPresentation(traveler,query.request);
    query.stage=4;
    if(!TwinKartObjective::SubmitHide(world,level,query.request,
        reinterpret_cast<TwinKartRace::CopyPreparationString>(image+0x923880),
        reinterpret_cast<TwinKartObjective::OpenLevel>(image+0x3f50670),
        reinterpret_cast<TwinKartObjective::ReleaseString>(image+0x248b950))) {hideCaptureArmed=false;return -17;}
    if(hideLobbyEnabled)InterlockedExchange(&hideHintState,1);
    RuntimeEvent(arena?"native_hide_original_art_compatibility_arena_requested":"native_hide_exact_development_world_requested",-1,109);return 1;
}
const TwinKartRace::Request* loadingPresentationRequest=nullptr;
TwinKartRace::Request retainedLoadingPresentationRequest{};
BYTE* loadingPresentationTraveler=nullptr;
TwinKartCamera::ObjectIdentity loadingPresentationIdentity{};
ULONGLONG loadingPresentationExpires=0;
unsigned loadingPresentationDepth=0;
bool loadingPresentationPrepared=false;
int loadingDefaultTitleMode=0;
ULONGLONG loadingDirectProbeAfter=0;
TwinKartCamera::ObjectIdentity loadingDirectUpdated{};
TwinKartCamera::ObjectIdentity loadingTrackTexture{},loadingBackgroundTexture{},loadingBackgroundPage{};
BYTE* loadingTrackTextureObject=nullptr;
wchar_t loadingVisibleTitle[1024]{};
bool loadingSlateApplying=false;
using TakeLoadingSlate=void*(__fastcall*)(BYTE*,void*,const void*);
TakeLoadingSlate originalTakeLoadingSlate=nullptr;
using CreateLoadingPage=void(__fastcall*)(BYTE*,const NativeString*,const NativeString*);
CreateLoadingPage originalCreateLoadingPage=nullptr;
TwinKartNative::OwnedCodePatch loadingSlatePatch,loadingPagePatch,loadingPageInfoPatch;
TwinKartNative::OwnedCodePatch loadingRefreshPatch,loadingTrackTextPatch,loadingGuideTextPatch;
TwinKartNative::OwnedCodePatch loadingPrimaryTitlePatch,loadingPrimaryGuidePatch;
TwinKartNative::OwnedCodePatch loadingLateControllerPatch,loadingLateWidgetPatch;
TwinKartNative::OwnedCodePatch loadingLateTypePatch,loadingLateGroupPatch,loadingPrimaryGroupPatch;
void ClearLoadingPresentation() {
    loadingPresentationRequest=nullptr;loadingPresentationTraveler=nullptr;loadingPresentationIdentity={};
    loadingPresentationExpires=0;loadingPresentationPrepared=false;
    loadingDirectProbeAfter=0;loadingDirectUpdated={};
    loadingTrackTexture={};loadingTrackTextureObject=nullptr;loadingVisibleTitle[0]=0;
    loadingBackgroundTexture={};loadingBackgroundPage={};
    loadingDefaultTitleMode=0;
}
bool NativeDefaultLoadingTitleBound(int mode) {
    return mode==loadingDefaultTitleMode && loadingPresentationRequest && loadingPresentationRequest->mode==mode;
}
bool LoadingPresentationOwned() {
    return GetCurrentThreadId()==queue.ThreadId() && loadingPresentationRequest && GetTickCount64()<=loadingPresentationExpires &&
        TwinKartCamera::Live(loadingPresentationIdentity,*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),
            *reinterpret_cast<int*>(image+0x69c28c4)) &&
        TwinKartLocalRace::Live(loadingPresentationTraveler) &&
        *reinterpret_cast<BYTE**>(loadingPresentationTraveler)==image+0x52618c0;
}
bool RetainHideLoadingPresentation(BYTE* traveler,const TwinKartRace::Request& request) {
    if(request.mode!=109 || (strcmp(request.track,"HideNSeekTestLevel") && strcmp(request.track,"Flag_F01")) ||
       !TwinKartLocalRace::CaptureFlagObject(traveler,loadingPresentationIdentity))return false;
    retainedLoadingPresentationRequest=request;
    loadingPresentationRequest=&retainedLoadingPresentationRequest;
    loadingPresentationTraveler=traveler;loadingPresentationExpires=GetTickCount64()+45000;
    // The repaired development world has no original circuit record or
    // stock preview brush. Use its real identity without borrowing an image
    // from a different track.
    wcscpy_s(loadingVisibleTitle,!strcmp(request.track,"Flag_F01")?
        L"\u6d77\u6ee8\u7ade\u6280\u573a\uff08\u517c\u5bb9\u573a\u5730\uff09":
        L"\u4fee\u590d\u7248\u8eb2\u85cf\u573a\u5730");
    loadingPresentationPrepared=true;
    return true;
}
bool PrepareLoadingPresentation() {
    if(!LoadingPresentationOwned())return false;
    if(loadingPresentationPrepared)return true;
    TwinKartRace::LoadingPreviewApi preview;
    preview.lookup=reinterpret_cast<TwinKartRace::LoadingPreviewApi::Lookup>(image+0x1472ce0);
    preview.move=reinterpret_cast<TwinKartRace::LoadingPreviewApi::Move>(image+0x1aed1b0);
    preview.build=reinterpret_cast<TwinKartRace::LoadingPreviewApi::Build>(image+0x1b922c0);
    preview.destroy=reinterpret_cast<TwinKartRace::LoadingPreviewApi::Destroy>(image+0x1620210);
    preview.copy=reinterpret_cast<TwinKartEquipment::CopyString>(image+0x923880);
    preview.find=reinterpret_cast<TwinKartRace::LoadingPreviewApi::FindText>(image+0x3fe9390);
    preview.read=ModeLabelText;
    preview.create=reinterpret_cast<TwinKartObjective::CreateChaserText>(image+0x3fe0a60);
    bool officialChinese=false;
    if(!TwinKartRace::RefreshLoadingPreview(loadingPresentationTraveler,*loadingPresentationRequest,preview,officialChinese))return false;
    if(!ModeLabelText(loadingPresentationTraveler+0x140,loadingVisibleTitle,ARRAYSIZE(loadingVisibleTitle)) ||
       !TwinKartRace::ChinesePreviewText(loadingVisibleTitle))return false;
    loadingPresentationPrepared=true;
    RuntimeEvent("native_loading_track_metadata_synchronized",-1,loadingPresentationRequest->mode);
    RuntimeEvent("native_loading_track_chinese_title",-1,officialChinese?1:2);return true;
}
void __fastcall BeforeOriginalLoadingWidget(void* controller,unsigned char reset) {
    // PreInitializeByGame creates/refreshes the actual widget before returning.
    // Repair its source first, including the real map's image/world record.
    bool scope=LoadingPresentationOwned() && *reinterpret_cast<void**>(loadingPresentationTraveler+0x538)==controller;
    if(scope) {
        loadingPresentationPrepared=false;
        PrepareLoadingPresentation();++loadingPresentationDepth;
    }
    __try {reinterpret_cast<void(__fastcall*)(void*,unsigned char)>(image+0x1d6c500)(controller,reset);}
    __finally {if(scope)--loadingPresentationDepth;}
}
void __fastcall BeforeOriginalLateLoadingWidget(void* widget,const void* info) {
    auto raw=static_cast<BYTE*>(widget);
    bool scope=LoadingPresentationOwned() && TwinKartLocalRace::Live(raw) &&
        *reinterpret_cast<BYTE**>(raw)==image+0x554b7c0;
    if(scope) {
        PrepareLoadingPresentation();++loadingPresentationDepth;
        RuntimeEvent("native_loading_late_widget_bound",-1,loadingPresentationPrepared?1:0);
    }
    __try {reinterpret_cast<void(__fastcall*)(void*,const void*)>(image+0x1d6d9c0)(widget,info);}
    __finally {if(scope)--loadingPresentationDepth;}
}
bool ReplaceLoadingWidgetText(void* text,const wchar_t* title) {
    return TwinKartRace::ReplaceLoadingText(text,title,
        reinterpret_cast<TwinKartObjective::CreateChaserText>(image+0x3fe0a60),
        reinterpret_cast<TwinKartRace::LoadingPreviewApi::Destroy>(image+0x1620210));
}
bool SetRetainedLoadingLabel(BYTE* widget,unsigned offset,const wchar_t* label) {
    auto block=*reinterpret_cast<BYTE**>(widget+offset);
    if(!TwinKartLocalRace::Live(block) || !label || !*label)return false;
    // The stock loading initializer passes an owned FText temporary at this
    // same virtual setter. The setter consumes that temporary's reference.
    alignas(16) BYTE storage[0x160]{};
    auto value=reinterpret_cast<TwinKartObjective::ChaserText*>(storage+0x40);
    auto input=TwinKartRace::PreviewString(label);
    auto create=reinterpret_cast<TwinKartObjective::CreateChaserText>(image+0x3fe0a60);
    if(create(value,&input)!=value || !value->data || !value->references) {
        reinterpret_cast<TwinKartRace::LoadingPreviewApi::Destroy>(image+0x1620210)(storage);return false;
    }
    reinterpret_cast<void(__fastcall*)(void*,void*)>((*reinterpret_cast<void***>(block))[0x2d8/8])(block,value);
    return true;
}
bool IsLoadingInformation(BYTE* widget) {
    return widget && *reinterpret_cast<BYTE**>(widget)==image+0x554b7c0 &&
        TwinKartLocalRace::Live(widget) && !(*reinterpret_cast<unsigned*>(widget+8)&0x18030);
}
// Image_Track is a UImage. NationalFlag (+0x280) takes a flag code, not a
// texture. Load the selected track's own soft texture path before Slate
// starts building the loading screen; never substitute a different track.
BYTE* LoadSelectedTrackTexture(const BYTE* field) {
    __try {
        struct SoftPath {uint64_t name;TwinKartEquipment::String subpath;};
        static_assert(sizeof(SoftPath)==0x18);
        auto source=reinterpret_cast<const SoftPath*>(field);
        if(!source->name || !TwinKartEquipment::Valid(source->subpath,512))return nullptr;
        SoftPath path{source->name,{}};
        auto copy=reinterpret_cast<TwinKartEquipment::CopyString>(image+0x923880);
        __try {
            copy(&path.subpath,&source->subpath);
            using Load=BYTE*(__fastcall*)(SoftPath*);
            auto texture=reinterpret_cast<Load>(image+0x278d7d0)(&path);
            if(!TwinKartLocalRace::Live(texture))return nullptr;
            auto type=reinterpret_cast<BYTE*(__fastcall*)()>(image+0x4552d90)();
            auto actual=*reinterpret_cast<BYTE**>(texture+0x10);
            int index=*reinterpret_cast<int*>(type+0x38);
            if(index<0 || index>*reinterpret_cast<int*>(actual+0x38) ||
               (*reinterpret_cast<BYTE***>(actual+0x30))[index]!=type+0x30)return nullptr;
            return texture;
        } __finally {
            if(path.subpath.data)reinterpret_cast<void(__fastcall*)(const void*)>(image+0x248b950)(path.subpath.data);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {return nullptr;}
}
void PrepareLoadingTrackTexture() {
    if(!loadingPresentationPrepared || !LoadingPresentationOwned())return;
    // MajorGameLoadingWidget uses the cached definition at Traveler+0x3b8.
    // Its image soft path is +0xf8, whereas the alternate original page
    // resolves the texture at +0x148 in the active definition.
    auto texture=LoadSelectedTrackTexture(loadingPresentationTraveler+0x3b8+0xf8);
    int result=1;
    if(!texture){texture=LoadSelectedTrackTexture(loadingPresentationTraveler+0x100+0x148);result=2;}
    if(texture && TwinKartLocalRace::CaptureFlagObject(texture,loadingTrackTexture))loadingTrackTextureObject=texture;
    RuntimeEvent("native_loading_selected_texture_resolved",-1,loadingTrackTextureObject?result:0);
    // MajorGameLoadingWidget::InitImageBackground uses the cached record's
    // separate scene image at +0x148. Never use the outline as its substitute.
    auto background=LoadSelectedTrackTexture(loadingPresentationTraveler+0x3b8+0x148);
    if(background)TwinKartLocalRace::CaptureFlagObject(background,loadingBackgroundTexture);
    RuntimeEvent("native_loading_selected_background_resolved",-1,loadingBackgroundTexture.object?1:0);
}
int ApplyLoadingInformation(BYTE* widget,bool slateHandoff) {
    if(!IsLoadingInformation(widget) || !loadingVisibleTitle[0] || loadingSlateApplying)return 0;
    loadingSlateApplying=true;
    __try {
        bool title=SetRetainedLoadingLabel(widget,0x278,loadingVisibleTitle);
        if(!title)return 0;
        auto labels=TwinKartRace::LocalLoadingLabels(loadingPresentationRequest->mode,loadingPresentationRequest->profile);
        const bool type=SetRetainedLoadingLabel(widget,0x268,labels.mode);
        const bool group=SetRetainedLoadingLabel(widget,0x270,labels.group);
        const bool guide=SetRetainedLoadingLabel(widget,0x2d8,labels.guide);
        RuntimeEvent("native_loading_chinese_labels_applied",loadingPresentationRequest->mode,
            (type?1:0)|(group?2:0)|(guide?4:0));
        // Reflected BP_LoadingGameInformation_C fields: these optional
        // online rewards and skill panels expose editor placeholder text
        // when their original server-side presentation data is absent.
        for(unsigned offset:{0x298u,0x2a0u,0x2b0u,0x2b8u,0x2d0u}) {
            auto panel=*reinterpret_cast<BYTE**>(widget+offset);
            if(TwinKartLocalRace::Live(panel))
                reinterpret_cast<void(__fastcall*)(void*,unsigned char)>((*reinterpret_cast<void***>(panel))[0x280/8])
                    (panel,1); // Collapsed
        }
        auto map=*reinterpret_cast<BYTE**>(widget+0x288);
        auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
        const int count=*reinterpret_cast<int*>(image+0x69c28c4);
        bool preview=TwinKartLocalRace::Live(map) && loadingTrackTextureObject &&
            TwinKartCamera::Live(loadingTrackTexture,chunks,count);
        if(preview) {
            // Same UImage virtual and bMatchSize=false as the original
            // loading screen's selected track texture assignment.
            reinterpret_cast<void(__fastcall*)(void*,void*,bool)>((*reinterpret_cast<void***>(map))[0x2e0/8])
                (map,loadingTrackTextureObject,false);
        } else if(TwinKartLocalRace::Live(map)) {
            // A generic default picture can depict a different circuit.
            // Remove that brush if this client's exact track asset is absent.
            reinterpret_cast<void(__fastcall*)(void*,void*,bool)>((*reinterpret_cast<void***>(map))[0x2e0/8])
                (map,nullptr,false);
        }
        RuntimeEvent(slateHandoff?"native_loading_slate_widget_bound":"native_loading_visible_widget_updated",-1,preview?2:1);
        if(slateHandoff)TwinKartLocalRace::CaptureFlagObject(widget,loadingDirectUpdated);
        return preview?2:1;
    } __finally {loadingSlateApplying=false;}
}
bool OwnsLoadingPage(BYTE* page) {
    if(!loadingPresentationPrepared || !LoadingPresentationOwned() || !TwinKartLocalRace::Live(page) ||
       *reinterpret_cast<BYTE**>(loadingPresentationTraveler+0x518)!=page)return false;
    auto type=reinterpret_cast<BYTE*(__fastcall*)()>(image+0x23824c0)();
    auto actual=*reinterpret_cast<BYTE**>(page+0x10);
    const int index=*reinterpret_cast<int*>(type+0x38);
    return index>=0 && index<=*reinterpret_cast<int*>(actual+0x38) &&
        (*reinterpret_cast<BYTE***>(actual+0x30))[index]==type+0x30;
}
bool BindDefaultLoadingPage(BYTE* page) {
    // The retained objective modes can select BP_LoadingDefault_C instead
    // of MajorGameLoadingWidget. Its reflected LoadingWidgetBase fields
    // still include the original title and background UImage.
    if(!loadingPresentationPrepared || !LoadingPresentationOwned() || !TwinKartLocalRace::Live(page) ||
       *reinterpret_cast<BYTE**>(loadingPresentationTraveler+0x518)!=page ||
       *reinterpret_cast<BYTE**>(page)!=image+0x52b31d8 || !loadingVisibleTitle[0])return false;
    auto labels=TwinKartRace::LocalLoadingLabels(loadingPresentationRequest->mode,loadingPresentationRequest->profile);
    if(!labels.mode)return false;
    wchar_t title[1100]{};
    if(swprintf_s(title,L"%s \uff5c %s",labels.mode,loadingVisibleTitle)<1)return false;
    const bool named=SetRetainedLoadingLabel(page,0x260,title);
    if(named) {
        loadingDefaultTitleMode=loadingPresentationRequest->mode;
        auto block=*reinterpret_cast<BYTE**>(page+0x260);
        if(TwinKartLocalRace::Live(block))
            reinterpret_cast<void(__fastcall*)(void*,unsigned char)>((*reinterpret_cast<void***>(block))[0x280/8])
                (block,3); // HitTestInvisible: an authored label must not consume clicks.
    }
    auto background=*reinterpret_cast<BYTE**>(page+0x268);
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
    const int count=*reinterpret_cast<int*>(image+0x69c28c4);
    auto texture=TwinKartCamera::Live(loadingBackgroundTexture,chunks,count)?loadingBackgroundTexture.object:nullptr;
    bool pictured=false;
    if(texture && TwinKartLocalRace::Live(background)) {
        auto setter=(*reinterpret_cast<BYTE***>(background))[0x2e0/8];
        if(setter>=image && setter<image+0x4ed0000) {
            reinterpret_cast<void(__fastcall*)(void*,void*,bool)>(setter)(background,texture,false);
            pictured=true;
        }
    }
    if(loadingBackgroundPage.object!=page || !TwinKartCamera::Live(loadingBackgroundPage,chunks,count)) {
        TwinKartLocalRace::CaptureFlagObject(page,loadingBackgroundPage);
        RuntimeEvent("native_loading_default_selected_presentation",loadingPresentationRequest->mode,
            (named?1:0)|(pictured?2:0));
    }
    return true;
}
void BindLoadingPage(BYTE* page,const char* event) {
    __try {
        if(BindDefaultLoadingPage(page))return;
        if(!OwnsLoadingPage(page))return;
        auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
        const int count=*reinterpret_cast<int*>(image+0x69c28c4);
        auto background=*reinterpret_cast<BYTE**>(page+0x268);
        if(TwinKartLocalRace::Live(background)) {
            auto texture=TwinKartCamera::Live(loadingBackgroundTexture,chunks,count)?loadingBackgroundTexture.object:nullptr;
            auto setter=(*reinterpret_cast<BYTE***>(background))[0x2e0/8];
            if(setter>=image && setter<image+0x4ed0000) {
                reinterpret_cast<void(__fastcall*)(void*,void*,bool)>(setter)(background,texture,false);
                if(loadingBackgroundPage.object!=page || !TwinKartCamera::Live(loadingBackgroundPage,chunks,count)) {
                    TwinKartLocalRace::CaptureFlagObject(page,loadingBackgroundPage);
                    RuntimeEvent("native_loading_selected_background_bound",-1,texture?1:0);
                }
            }
        }
        auto information=*reinterpret_cast<BYTE**>(page+0x340);
        if(!IsLoadingInformation(information))return;
        RuntimeEvent(event,-1,ApplyLoadingInformation(information,true));
    } __except(EXCEPTION_EXECUTE_HANDLER) {RuntimeEvent("native_loading_owned_page_fault",-1,0);}
}
void __fastcall OnOriginalLoadingPageInfo(BYTE* page,const void* initializer) {
    reinterpret_cast<void(__fastcall*)(BYTE*,const void*)>(image+0x1d45f60)(page,initializer);
    // Override the live page after the original initializer has finished
    // assigning its defaults, before returning to the native travel caller.
    BindLoadingPage(page,"native_loading_owned_page_after_init");
}
void __fastcall OnCreateLoadingPage(BYTE* traveler,const NativeString* mode,const NativeString* alias) {
    bool scope=loadingPresentationPrepared && LoadingPresentationOwned() && traveler==loadingPresentationTraveler;
    if(scope)RuntimeEvent("native_loading_owned_page_create",-1,loadingPresentationRequest->mode);
    originalCreateLoadingPage(traveler,mode,alias);
    if(scope)BindLoadingPage(*reinterpret_cast<BYTE**>(traveler+0x518),"native_loading_owned_page_before_travel");
}
void* __fastcall OnTakeLoadingSlate(BYTE* widget,void* output,const void* factory) {
    auto result=originalTakeLoadingSlate(widget,output,factory);
    __try {
        // Run after SynchronizeProperties / OnWidgetRebuilt (or when an
        // existing Slate widget is taken again), before handing it to its
        // parent. Only mutate the exact live loading-information instance
        // from the game thread while a prepared local launch owns the page.
        if(GetCurrentThreadId()==queue.ThreadId() && loadingPresentationRequest &&
           loadingPresentationPrepared && GetTickCount64()<=loadingPresentationExpires &&
           IsLoadingInformation(widget))ApplyLoadingInformation(widget,true);
        else if(widget && !loadingSlateApplying && loadingPresentationRequest && loadingPresentationPrepared &&
            *reinterpret_cast<BYTE**>(widget)==image+0x52b31d8)BindDefaultLoadingPage(widget);
    } __except(EXCEPTION_EXECUTE_HANDLER) {RuntimeEvent("native_loading_slate_widget_fault",-1,0);}
    return result;
}
void ApplyRetainedLoadingWidget() {
    __try {
        if(!loadingPresentationPrepared || !LoadingPresentationOwned())return;
        auto now=GetTickCount64();if(now<loadingDirectProbeAfter)return;
        loadingDirectProbeAfter=now+250;
        auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
        int count=*reinterpret_cast<int*>(image+0x69c28c4);
        if(TwinKartCamera::Live(loadingDirectUpdated,chunks,count) || OwnedRaceWorld() || !chunks || count<1 || count>2000000)return;
        BYTE* widget=nullptr;
        for(int index=0;index<count;++index) {
            auto chunk=chunks[index/65536];if(!chunk)continue;
            auto& slot=chunk[index%65536];auto object=static_cast<BYTE*>(slot.object);
            if(!object || (slot.flags&0x30000000) || *reinterpret_cast<BYTE**>(object)!=image+0x554b7c0 ||
               (*reinterpret_cast<unsigned*>(object+8)&0x18030) ||
               !TwinKartLocalRace::Live(*reinterpret_cast<BYTE**>(object+0x278)))continue;
            if(widget)return;widget=object;
        }
        if(!widget || !TwinKartLocalRace::CaptureFlagObject(widget,loadingDirectUpdated))return;
        ApplyLoadingInformation(widget,false);
    } __except(EXCEPTION_EXECUTE_HANDLER) {RuntimeEvent("native_loading_visible_widget_update_fault",-1,0);}
}
void __fastcall SetOriginalLoadingTrackText(void* raw,void* text) {
    if(loadingPresentationDepth && loadingPresentationPrepared && LoadingPresentationOwned() && TwinKartLocalRace::Live(static_cast<BYTE*>(raw)) && text) {
        wchar_t current[1024]{},selected[1024]{};
        if(ModeLabelText(loadingPresentationTraveler+0x140,selected,ARRAYSIZE(selected)) && TwinKartRace::ChinesePreviewText(selected)) {
            bool already=ModeLabelText(static_cast<BYTE*>(text),current,ARRAYSIZE(current)) && !wcscmp(current,selected);
            if(already || ReplaceLoadingWidgetText(text,selected))RuntimeEvent("native_loading_widget_chinese_track",-1,already?1:2);
        }
    }
    // Keep the very same owned argument at the original consuming boundary.
    using Set=void(__fastcall*)(void*,void*);
    reinterpret_cast<Set>((*reinterpret_cast<void***>(raw))[0x2d8/8])(raw,text);
}
void* __fastcall OriginalLoadingTitleText(const void* record,void* value) {
    auto result=reinterpret_cast<void*(__fastcall*)(const void*,void*)>(image+0x1472590)(record,value);
    if(result==value && value && loadingPresentationDepth && loadingPresentationPrepared && LoadingPresentationOwned() &&
       record==loadingPresentationTraveler+0x100) {
        wchar_t current[1024]{},selected[1024]{};
        if(ModeLabelText(loadingPresentationTraveler+0x140,selected,ARRAYSIZE(selected)) && TwinKartRace::ChinesePreviewText(selected)) {
            bool already=ModeLabelText(static_cast<BYTE*>(value),current,ARRAYSIZE(current)) && !wcscmp(current,selected);
            if(already || ReplaceLoadingWidgetText(value,selected))RuntimeEvent("native_loading_widget_primary_chinese_track",-1,already?1:2);
        }
    }
    return result;
}
void* __fastcall OriginalLoadingGuideText(void* output,const void* info) {
    auto result=reinterpret_cast<void*(__fastcall*)(void*,const void*)>(image+0x16a3a90)(output,info);
    if(result==output && output && loadingPresentationDepth && LoadingPresentationOwned()) {
        auto labels=TwinKartRace::LocalLoadingLabels(loadingPresentationRequest->mode,loadingPresentationRequest->profile);
        if(labels.guide)ReplaceLoadingWidgetText(output,labels.guide);
    }
    return result;
}
void* __fastcall OriginalLoadingEventType(void* value,const void* info) {
    auto result=reinterpret_cast<void*(__fastcall*)(void*,const void*)>(image+0x169d630)(value,info);
    if(result==value && value && loadingPresentationDepth && LoadingPresentationOwned()) {
        auto labels=TwinKartRace::LocalLoadingLabels(loadingPresentationRequest->mode,loadingPresentationRequest->profile);
        if(labels.mode)ReplaceLoadingWidgetText(value,labels.mode);
    }
    return result;
}
void* __fastcall OriginalLoadingEventGroup(void* value,const void* info,bool shortLabel) {
    auto result=reinterpret_cast<void*(__fastcall*)(void*,const void*,bool)>(image+0x16a3c70)(value,info,shortLabel);
    if(result==value && value && loadingPresentationDepth && LoadingPresentationOwned()) {
        auto labels=TwinKartRace::LocalLoadingLabels(loadingPresentationRequest->mode,loadingPresentationRequest->profile);
        if(labels.group)ReplaceLoadingWidgetText(value,labels.group);
    }
    return result;
}
bool InstallLoadingPresentation() {
    if(loadingSlatePatch.Active() && loadingPagePatch.Active() && loadingPageInfoPatch.Active() &&
       loadingRefreshPatch.Active() && loadingTrackTextPatch.Active() && loadingGuideTextPatch.Active() &&
       loadingPrimaryTitlePatch.Active() && loadingPrimaryGuidePatch.Active() && loadingLateControllerPatch.Active() &&
       loadingLateWidgetPatch.Active() && loadingLateTypePatch.Active() &&
       loadingLateGroupPatch.Active() && loadingPrimaryGroupPatch.Active())return true;
    if(loadingRefreshPatch.Call(image+0x1bba3bb,image+0x1d6c500,reinterpret_cast<void*>(BeforeOriginalLoadingWidget)) &&
       loadingTrackTextPatch.LoadingTrackTextCall(image+0x1d6e00b,reinterpret_cast<void*>(SetOriginalLoadingTrackText)) &&
       loadingGuideTextPatch.Call(image+0x1d6e0b2,image+0x16a3a90,reinterpret_cast<void*>(OriginalLoadingGuideText)) &&
       loadingPrimaryTitlePatch.Call(image+0x1d6cf99,image+0x1472590,reinterpret_cast<void*>(OriginalLoadingTitleText)) &&
       loadingPrimaryGuidePatch.Call(image+0x1d6c6da,image+0x16a3a90,reinterpret_cast<void*>(OriginalLoadingGuideText)) &&
       loadingLateControllerPatch.Call(image+0x1d4e7df,image+0x1d6c500,reinterpret_cast<void*>(BeforeOriginalLoadingWidget)) &&
       loadingLateWidgetPatch.Call(image+0x1d46dd5,image+0x1d6d9c0,reinterpret_cast<void*>(BeforeOriginalLateLoadingWidget)) &&
       loadingLateTypePatch.Call(image+0x1d6dc03,image+0x169d630,reinterpret_cast<void*>(OriginalLoadingEventType)) &&
       loadingLateGroupPatch.Call(image+0x1d6dc2f,image+0x16a3c70,reinterpret_cast<void*>(OriginalLoadingEventGroup)) &&
       loadingPrimaryGroupPatch.Call(image+0x1d6c672,image+0x16a3c70,reinterpret_cast<void*>(OriginalLoadingEventGroup))) {
        const BYTE prefix[]{0x48,0x8b,0xc4,0x57,0x41,0x54,0x48,0x83,0xec,0x58,0x48,0x89,0x58,0x10};
        const BYTE pagePrefix[]{0x40,0x55,0x53,0x56,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0x6c,0x24,0xb0};
        if(loadingPageInfoPatch.Call(image+0x1ba0314,image+0x1d45f60,reinterpret_cast<void*>(OnOriginalLoadingPageInfo)) &&
           loadingPagePatch.Entry(image+0x1b9f8e0,pagePrefix,sizeof(pagePrefix),reinterpret_cast<void*>(OnCreateLoadingPage),
                                  reinterpret_cast<void**>(&originalCreateLoadingPage)) &&
           loadingSlatePatch.Entry(image+0x3abc890,prefix,sizeof(prefix),reinterpret_cast<void*>(OnTakeLoadingSlate),
                                  reinterpret_cast<void**>(&originalTakeLoadingSlate)))return true;
    }
    loadingPagePatch.Restore();loadingPageInfoPatch.Restore();
    loadingPrimaryGroupPatch.Restore();loadingLateGroupPatch.Restore();loadingLateTypePatch.Restore();
    loadingLateWidgetPatch.Restore();loadingLateControllerPatch.Restore();
    loadingPrimaryGuidePatch.Restore();loadingPrimaryTitlePatch.Restore();
    loadingGuideTextPatch.Restore();loadingTrackTextPatch.Restore();loadingRefreshPatch.Restore();return false;
}
void ResetPracticePreparation() {
    practicePrepareWorld={};practicePrepareAccount={};
    practicePrepareUntil=0;practicePrepareAccepted=false;
}
void __fastcall OnPracticePrepareNotification(BYTE* notification,int result) {
    using Notify=void(__fastcall*)(BYTE*,int);
    reinterpret_cast<Notify>(image+0x10e5960)(notification,result);
    if(GetCurrentThreadId()!=queue.ThreadId() || !practicePrepareUntil ||
       notification!=static_cast<BYTE*>(practicePrepareAccount.object)+0x78)return;
    if(result!=0){ResetPracticePreparation();RuntimeEvent("native_free_drive_prepare_rejected",0,result);return;}
    practicePrepareAccepted=true;
}
void DispatchPreparedFreeDrive(BYTE* traveler,BYTE* world) {
    if(!practicePrepareUntil)return;
    const auto now=GetTickCount64();
    if(now>=practicePrepareUntil || world!=practicePrepareWorld.object ||
       !HideMaterialIdentityLive(practicePrepareWorld) || !HideMaterialIdentityLive(practicePrepareAccount)) {
        ResetPracticePreparation();RuntimeEvent("native_free_drive_prepare_expired",0,0);return;
    }
    using IsTraveling=bool(__fastcall*)(BYTE*);
    if(!practicePrepareAccepted || reinterpret_cast<IsTraveling>(image+0x1ba8360)(traveler))return;
    wchar_t track[97]{};int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,practiceLobbyTrack,-1,track,ARRAYSIZE(track));
    auto selection=static_cast<BYTE*>(practicePrepareAccount.object);
    const auto selected=*reinterpret_cast<const TwinKartRace::ReturnString*>(selection+0x98);
    auto instance=*reinterpret_cast<BYTE**>(world+0x180);
    if(count<=1 || !TwinKartRace::ReturnStringShape(selected) || selected.count!=count ||
       !selected.data || wmemcmp(selected.data,track,count) || !TwinKartLocalRace::Live(instance)) {
        ResetPracticePreparation();RuntimeEvent("native_free_drive_prepare_selection_changed",0,0);return;
    }
    using TravelData=BYTE*(__fastcall*)(BYTE*);
    auto data=reinterpret_cast<TravelData>(image+0x19c3ca0)(instance);
    if(!TwinKartLocalRace::Live(data)) {
        ResetPracticePreparation();RuntimeEvent("native_free_drive_travel_data_unavailable",0,0);return;
    }
    // The PC Time Attack widget clears its prior replay before opening this
    // route. The solo local room has no ghost or replay items to preload.
    reinterpret_cast<void(__fastcall*)(BYTE*,void*)>(image+0x1e3d6c0)(data,nullptr);
    reinterpret_cast<void(__fastcall*)(BYTE*,bool)>(image+0x1e3d9a0)(data,false);
    NativeString alias{track,count,count};
    struct EmptyReplayItems {void* data;int count,capacity;} items{};
    ResetPracticePreparation();
    // Limit this request to the next Time Attack start for the selected map.
    // A fresh backend ticket is still required before changing the finish rule.
    practiceLobbyUntil=now+90000;
    using Open=void(__fastcall*)(BYTE*,const NativeString*,EmptyReplayItems*,bool);
    reinterpret_cast<Open>(image+0x1bb9130)(traveler,&alias,&items,false);
    RuntimeEvent("native_free_drive_timeattack_travel_submitted",0,1);
}
int NativeLobbyPracticeStart(StartQuery& query) {
    const auto now=GetTickCount64();
    if(!practiceTicketPath[0] || OwnedRaceWorld() || now<practicePrepareUntil || now<practiceLobbyUntil)return -9;
    auto traveler=Traveler();if(!traveler)return 0;
    using GetWorld=BYTE*(__fastcall*)(void*);
    auto world=reinterpret_cast<GetWorld>((*reinterpret_cast<void***>(traveler))[0x160/8])(traveler);
    if(!TwinKartLocalRace::Live(world) || *reinterpret_cast<void**>(world+0x38))return -6;
    auto mode=*reinterpret_cast<BYTE**>(world+0x118);
    if(TwinKartLocalRace::Live(mode) && *reinterpret_cast<BYTE**>(mode)==image+0x520ce90)return -9;
    NativeString alias{query.track,static_cast<int>(wcslen(query.track)+1),static_cast<int>(wcslen(query.track)+1)};
    alignas(16) BYTE definition[0x160]{};
    using Get=void*(__fastcall*)(void*,const NativeString*);
    if(reinterpret_cast<Get>(image+0x1472ce0)(definition,&alias)!=definition)return -1;
    __try {
        const auto level=*reinterpret_cast<const NativeString*>(definition);
        if(!level.data || level.count<=1 || level.count>512 || level.capacity<level.count || level.data[level.count-1])return 2;
        wchar_t resource[1024]{};
        if(!ModeNameText(*reinterpret_cast<const unsigned long long*>(definition+0x98),resource,ARRAYSIZE(resource)) ||
           TwinKartRace::DevelopmentWorldPath(resource) ||
           !TwinKartRace::ReadNativeTrackModeMask(definition,sizeof(definition),query.modeMask) ||
           !TwinKartRace::NativeTrackSupports(query.modeMask,1))return 3;
    } __finally {reinterpret_cast<void(__fastcall*)(void*)>(image+0x1620210)(definition);}
    TwinKartEquipment::Preset player;BYTE* account=nullptr;
    if(!TwinKartLocalRace::ReadEquipmentPreset(player,&account))return 0;
    auto selection=NativeTrackSelection(account);if(!selection)return 0;
    auto& destination=*reinterpret_cast<TwinKartRace::ReturnString*>(selection+0x98);
    const BYTE startPrefix[]{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xec,0x70};
    const BYTE travelPrefix[]{0x40,0x55,0x53,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57};
    const BYTE replaySetter[]{0x48,0x89,0x51,0x30,0xc3},shadowSetter[]{0x88,0x51,0x38,0xc3};
    if(!TwinKartRace::ReturnStringShape(destination) || memcmp(image+0x1b2b170,startPrefix,sizeof(startPrefix)) ||
       memcmp(image+0x1bb9130,travelPrefix,sizeof(travelPrefix)) ||
       memcmp(image+0x1e3d6c0,replaySetter,sizeof(replaySetter)) || memcmp(image+0x1e3d9a0,shadowSetter,sizeof(shadowSetter)))return -8;
    query.stage=1;
    TwinKartCamera::ObjectIdentity worldId{},selectionId{};
    if(!TwinKartLocalRace::CaptureFlagObject(world,worldId) || !TwinKartLocalRace::CaptureFlagObject(selection,selectionId))return -8;
    if(!practicePrepareInstalled) {
        practicePrepareInstalled=practicePrepareNotificationPatch.Call(image+0x1b00c7e,image+0x10e5960,
            reinterpret_cast<void*>(OnPracticePrepareNotification));
        if(!practicePrepareInstalled)return -8;
    }
    query.stage=2;
    TwinKartRace::ReturnString chosen{query.track,alias.count,alias.capacity};
    reinterpret_cast<TwinKartRace::AssignReturnString>(image+0x9118f0)(&destination,&chosen);
    if(!destination.data || destination.count!=alias.count || wcscmp(destination.data,query.track))return -8;
    memset(practicePriorTicket,0,sizeof(practicePriorTicket));
    ReadPracticeTicket(practiceTicketPath,practicePriorTicket);
    strcpy_s(practiceLobbyTrack,query.request.track);
    practicePrepareWorld=worldId;practicePrepareAccount=selectionId;
    practicePrepareAccepted=false;practicePrepareUntil=now+10000;
    query.stage=3;
    // This is the same AccountTimeAttack prepare method reached by the
    // original Time Attack button (21b5163 -> 1e38910 -> 1b2b170).
    reinterpret_cast<void(__fastcall*)(void*)>(image+0x1b2b170)(selection);
    RuntimeEvent("native_free_drive_timeattack_prepare_submitted",0,1);
    return 1;
}
int NativeStart(StartQuery& query) {
    const wchar_t* track=query.track; const wchar_t* options=query.options; int mode=query.request.mode;
    __try {
        if(TwinKartRace::FreeDrive(query.request))return NativeLobbyPracticeStart(query);
        if(query.hideCapture)return NativeHideCapture(query);
        if(TwinKartRace::ChaserMode(mode) && (!chaserArenaEnabled || !RegisteredWorldObject(TwinKartObjective::HideArenaObject)))return -20;
        query.stage=1;
        if(query.factoryCapture) {
            using Type=void*(__fastcall*)();
            if(!TwinKartLocalRace::factoryCompatEnabled || !TwinKartLocalRace::Live(static_cast<BYTE*>(reinterpret_cast<Type>(image+0x24345d0)())) ||
               !TwinKartFactory::CaptureOptions(query.request,query.options,ARRAYSIZE(query.options)))return -14;
        } else {
            TwinKartRace::CanonicalModeProfile(query.request);
            TwinKartRace::ModeDefinition modeDefinition{};
            if(!OriginalMode(query.request,modeDefinition) || !TwinKartRace::ModeOptions(query.request,modeDefinition,query.options,ARRAYSIZE(query.options)))return -11;
        }
        RuntimeEvent(query.factoryCapture?"native_factory_capture_compiled_class_resolved":"native_original_mode_binding_resolved",-1,query.request.mode);
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
            wchar_t resourceWorld[1024]{};
            if(ModeNameText(*reinterpret_cast<const unsigned long long*>(definition+0x98),resourceWorld,ARRAYSIZE(resourceWorld)) &&
               TwinKartRace::DevelopmentWorldPath(resourceWorld)) {
                RuntimeEvent("native_local_development_scene_rejected",-1,mode);return -19;
            }
            if(TwinKartRace::CubeMode(mode))DumpEventTrackResource(track,definition);
            bool validModes=TwinKartRace::ReadNativeTrackModeMask(definition,sizeof(definition),query.modeMask);
            if(!validModes || !(query.factoryCapture?TwinKartRace::NativeTrackSupports(query.modeMask,4):
                TwinKartRace::EventTrackSupports(query.modeMask,mode,track)))return validModes?3:-5;
            using Name=void*(__fastcall*)(void*,const wchar_t*,int);
            reinterpret_cast<Name>(image+0x2589130)(&levelName,level.data,1);
        } __finally {reinterpret_cast<Destroy>(image+0x1620210)(definition);}
        query.stage=3;
        if(TwinKartRace::CubeMode(mode)) {
            auto cube=TwinKartRace::CubeWorldForTrack(mode,track);
            if(!cube || !RegisteredCubeWorld(*cube))return -13;
            using Name=void*(__fastcall*)(void*,const wchar_t*,int);
            reinterpret_cast<Name>(image+0x2589130)(&levelName,cube->package,1);
            RuntimeEvent("native_local_cube_exact_world_selected",-1,mode);
        }
        if(!TwinKartLocalRace::Install(image,RuntimeEvent)) return -3;
        if(query.factoryCapture) {
            TwinKartRace::AiAssetLookup assets;
            if(!TwinKartLocalRace::factoryRacerRoster.LoadEnvironment() || !TwinKartRace::BeginAiAssetLookup(image,assets) ||
               !TwinKartLocalRace::factoryRacerRoster.PrepareFixedRacers(TwinKartFactory::RacerCharacters,TwinKartFactory::RacerKarts,
                   TwinKartFactory::RacerCount,AuditedBelonging,&assets))return -15;
            wchar_t nativeNpcId[97]{};
            if(!TwinKartRace::RegisteredIdentityForPath(assets,TwinKartFactory::AndroidBodyObjectPath,nativeNpcId,ARRAYSIZE(nativeNpcId)))return -16;
            // Arm below clears per-race state. Retain this independently
            // resolved native ID in the start query until after that reset.
            wcscpy_s(query.factoryAndroidKart,nativeNpcId);
        }
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
        // Use the client's normal FKtGameMode/profile/track preparation, so
        // local loading screens and native mode configuration see this race
        // instead of stale room/previous-race metadata.
        const BYTE preparationPrefix[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10};
        if(TwinKartLocalRace::CaptureFlagObject(traveler,loadingPresentationIdentity)) {
            retainedLoadingPresentationRequest=query.request;
            loadingPresentationRequest=&retainedLoadingPresentationRequest;loadingPresentationTraveler=traveler;
            loadingPresentationExpires=GetTickCount64()+(mode==108?180000:45000);
        }else if(!query.factoryCapture)return -22;
        if(memcmp(image+0x1bba140,preparationPrefix,sizeof(preparationPrefix)) ||
           !(query.factoryCapture?PrepareFactoryCapture(traveler,query.request):TwinKartRace::PrepareNativeRace(traveler,query.request,
              reinterpret_cast<TwinKartRace::CopyPreparationString>(image+0x923880),
              reinterpret_cast<TwinKartRace::InitializeNativeRace>(image+0x1bba140))))return -10;
        RuntimeEvent("native_local_mode_profile_prepared",-1,mode);
        if(PrepareLoadingPresentation()) {
            PrepareLoadingTrackTexture();
            ApplyRetainedLoadingWidget();
        }else if(!query.factoryCapture)return -22;
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
        if(TwinKartLocalRace::aiRosterEnabled && !query.factoryCapture) {
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
            auto eventRules=TwinKartRace::EventProfile(query.request.profile);
            if(eventRules && eventRules->roster!=TwinKartRace::EventRoster::Ordinary) {
                const wchar_t* roles[8]{};
                for(int slot=0;slot<=query.request.ai;++slot) {
                    roles[slot]=TwinKartRace::EventCharacter(*eventRules,slot,query.request.ai+1);
                    if(!roles[slot])return -12;
                }
                TwinKartLocalRace::eventSkillRoster=TwinKartLocalRace::aiRoster;
                if(!TwinKartLocalRace::eventSkillRoster.ApplyCharacterRoles(roles,AuditedBelonging,&assets))return -12;
                RuntimeEvent("native_local_event_skill_roster_prepared",-1,query.request.ai+1);
            }
            RuntimeEvent("native_local_ai_roster_randomized",-1,query.request.ai);
        }
        if(TwinKartRace::NativeCharacterRules(query.request.profile) && !TwinKartLocalRace::aiRosterEnabled)return -12;
        ResetChaserSession();
        TwinKartLocalRace::Arm(mode);
        TwinKartLocalRace::factoryRosterReady=query.factoryCapture;
        if(query.factoryCapture)wcscpy_s(TwinKartLocalRace::factoryAndroidKart,query.factoryAndroidKart);
        InterlockedExchange(&factoryDiagnosticArmed,query.factoryCapture?1:0);
        if(TwinKartRace::CubeMode(mode))wcscpy_s(TwinKartLocalRace::cubeBaseTrack,track);
        TwinKartLocalRace::flagSkillsEnabled=TwinKartRace::FlagMode(mode) && !strcmp(query.request.profile,"s062_flag_skill");
        TwinKartLocalRace::nativeCharacterRules=TwinKartRace::NativeCharacterRules(query.request.profile);
        RuntimeEvent("native_local_character_rule_policy",-1,TwinKartLocalRace::nativeCharacterRules?1:0);
        // The single-player preparation path fabricates time-attack details
        // and is inappropriate for native major-connect speed/item hosts.
        // Retain the original loading/transition path, as established by the
        // owner's working iOS client and this PC function's native callers.
        const wchar_t* modeText=query.factoryCapture?L"TobyRush":TwinKartRace::ModeName(mode);
        NativeString modeName{modeText,static_cast<int>(wcslen(modeText)+1),static_cast<int>(wcslen(modeText)+1)};
        NativeString optionText{options,static_cast<int>(wcslen(options)+1),static_cast<int>(wcslen(options)+1)};
        using Load=bool(__fastcall*)(void*,const NativeString*,const NativeString*,unsigned long long,bool,const NativeString*,bool);
        query.stage=4;
        return reinterpret_cast<Load>(image+0x1bc6120)(traveler,&modeName,&alias,levelName,true,&optionText,true)?1:-4;
    } __except(NativeFault(GetExceptionInformation(),query)) { return -1; }
}
void ExecuteStart(void* raw) {
    auto& value=*static_cast<StartQuery*>(raw); value.executedThread=GetCurrentThreadId();
    ClearLoadingPresentation();
    __try {value.status=NativeStart(value);}
    __finally {if(value.status!=1)ClearLoadingPresentation();SetEvent(value.complete);}
}
void CancelStart(void* raw) { auto& value=*static_cast<StartQuery*>(raw); value.status=-1; SetEvent(value.complete); }
void DumpEventTrackResource(const wchar_t* alias,const BYTE* record);
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
                    if(result==1)DumpEventTrackResource(name,record);
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
        if(value.status==1 && !ApplyOriginalEventMenu()) {value.status=-1;value.summary.failureStage=8;}
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
void LogMissingChaserConfig(const wchar_t* rule,const wchar_t* path) {
    try {Log("{\"event\":\"native_chaser_missing_config_reference\",\"rule_class\":"+Json(Utf8(rule))+",\"asset_path\":"+Json(Utf8(path))+"}");}
    catch(const std::exception&) {RuntimeEvent("native_chaser_missing_config_log_rejected",-1,0);}
}
bool ModeLabelText(const BYTE* field,wchar_t* destination,size_t capacity) {
    __try {
        const BYTE prefix[]={0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0xe8};
        if(GetCurrentThreadId()!=queue.ThreadId() || !field || !destination || !capacity || memcmp(image+0x24ddba0,prefix,sizeof(prefix)))return false;
        // Original FText::ToString, independently identified through the
        // Kismet Conv_TextToString interface. Its returned FString is borrowed
        // from the asset's FText and must not be freed by this module.
        using Text=const NativeString*(__fastcall*)(const void*);
        auto value=reinterpret_cast<Text>(image+0x24ddba0)(field);
        if(!value || value->count<0 || static_cast<size_t>(value->count)>capacity || value->capacity<value->count)return false;
        if(!value->count){destination[0]=0;return true;}
        if(!value->data || value->data[value->count-1])return false;
        memcpy(destination,value->data,static_cast<size_t>(value->count)*sizeof(wchar_t));return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
void DumpEventTrackResource(const wchar_t* alias,const BYTE* record) {
    wchar_t enabled[4]{};
    if(GetEnvironmentVariableW(L"TWIN_KART_EVENT_DIAGNOSTICS",enabled,4)!=1 || enabled[0]!=L'1')return;
    const wchar_t* candidates[]={L"Desert_I06",L"Forest_I05",L"Ice_I05",L"Village_I05",L"World_I11"};
    bool selected=false;for(auto candidate:candidates)if(!wcscmp(alias,candidate)){selected=true;break;}
    if(!selected)return;
    // Read copied, original-owned FKtClientTrackDef values only while the
    // caller keeps that record alive. No borrowed pointer escapes this call.
    wchar_t path[1024]{},label[4096]{};
    bool pathValid=ModeNameText(*reinterpret_cast<const unsigned long long*>(record+0x98),path,ARRAYSIZE(path));
    bool labelValid=ModeLabelText(record+0x40,label,ARRAYSIZE(label));
    Log("{\"event\":\"native_event_track_resource\",\"alias\":"+Json(Utf8(alias))+
        ",\"world_path\":"+(pathValid?Json(Utf8(path)):"null")+
        ",\"track_name\":"+(labelValid?Json(Utf8(label)):"null")+"}");
}
void DumpEventProfileLabels() {
    wchar_t enabled[4]{};
    if(GetEnvironmentVariableW(L"TWIN_KART_EVENT_DIAGNOSTICS",enabled,4)!=1 || enabled[0]!=L'1')return;
    try {
        size_t count=0;auto rules=TwinKartRace::EventProfiles(count);
        std::vector<std::pair<int,const char*>> profiles;
        for(size_t i=0;i<count;++i)profiles.emplace_back(rules[i].mode,rules[i].profile);
        profiles.emplace_back(105,"spitem_solo_mayorzipi");profiles.emplace_back(5,"item_duo_tanya_spirit");profiles.emplace_back(4,"item_solo_randomskill");
        for(auto profile:profiles) {
            TwinKartRace::Request request;strcpy_s(request.identity,"00000000000000000000000000000000");strcpy_s(request.track,"Village_I04");
            request.mode=profile.first;strcpy_s(request.profile,profile.second);TwinKartRace::ModeDefinition definition;
            if(!OriginalMode(request,definition))continue;
            std::string row="{\"event\":\"native_event_profile_labels\",\"profile\":"+Json(profile.second);
            const char* keys[]={"full_name","description","detail","guide"};const unsigned fields[]={0x70,0x188,0x170,0x218};
            for(unsigned field=0;field<4;++field) {
                wchar_t label[4096]{};bool valid=ModeLabelText(static_cast<BYTE*>(definition.asset)+fields[field],label,ARRAYSIZE(label));
                row+=",\""+std::string(keys[field])+"\":"+(valid?Json(Utf8(label)):"null");
            }
            Log(row+"}");
        }
    } catch(const std::exception&) {RuntimeEvent("native_event_profile_labels_failed",-1,0);}
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
    wchar_t factoryData[4]{};bool factoryCaptureAllowed=TwinKartLocalRace::factoryCompatEnabled &&
        GetEnvironmentVariableW(L"TWIN_KART_FACTORY_DATA_CAPTURE",factoryData,4)==1 && factoryData[0]==L'1';
    wchar_t hideData[4]{};bool hideCaptureAllowed=GetEnvironmentVariableW(L"TWIN_KART_HIDE_DATA_CAPTURE",hideData,4)==1 && hideData[0]==L'1';
    wchar_t hideBots[4]{};bool hideBotCaptureAllowed=hideCaptureAllowed && GetEnvironmentVariableW(L"TWIN_KART_HIDE_BOT_CAPTURE",hideBots,4)==1 && hideBots[0]==L'1';
    wchar_t hideArena[4]{};bool hideArenaAllowed=hideBotCaptureAllowed &&
        GetEnvironmentVariableW(L"TWIN_KART_HIDE_ARENA_CAPTURE",hideArena,4)==1 && hideArena[0]==L'1';
    for(;;) {
        Sleep(static_cast<DWORD>(InterlockedCompareExchange(&controlInterval,0,0)));
        CheckExitPrompt();
        CheckPrototypeResult();
        CheckHideHint();
        if(InterlockedCompareExchange(&renderActivityPending,1,0)==0 && !queue.Submit({UpdateRenderActivity,nullptr,CancelRenderActivity}))CancelRenderActivity(nullptr);
        if(!path || !*path)continue;
        HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);
        if(file==INVALID_HANDLE_VALUE) continue;
        LARGE_INTEGER size{}; char bytes[1025]{}; DWORD read=0;
        bool valid=GetFileSizeEx(file,&size) && size.QuadPart>0 && size.QuadPart<=1024 &&
            ReadFile(file,bytes,static_cast<DWORD>(size.QuadPart),&read,nullptr) && read==size.QuadPart;
        CloseHandle(file);
        StartQuery value;
        if(!valid || memchr(bytes,0,read))continue;
        value.factoryCapture=factoryCaptureAllowed && TwinKartFactory::ParseCapture(bytes,&value.request);
        if(!value.factoryCapture && TwinKartLocalRace::factoryCompatEnabled)value.factoryCapture=TwinKartFactory::ParseStart(bytes,&value.request);
        value.hideCapture=hideCaptureAllowed && TwinKartObjective::ParseHideCapture(bytes,&value.request);
        if(!value.hideCapture && hideLobbyEnabled)value.hideCapture=TwinKartObjective::ParseHideRepairedStart(bytes,&value.request);
        if(!value.hideCapture && hideBotCaptureAllowed)value.hideCapture=TwinKartObjective::ParseHideBotCapture(bytes,&value.request);
        if(!value.hideCapture && hideArenaAllowed)value.hideCapture=TwinKartObjective::ParseHideArenaCapture(bytes,&value.request);
        if((!value.factoryCapture && !value.hideCapture && !TwinKartRace::Parse(bytes,&value.request)) || !strcmp(previous,value.request.identity))continue;
        strcpy_s(previous,value.request.identity);
        size_t length=strlen(value.request.track);
        for(size_t i=0;i<length;++i) value.track[i]=value.request.track[i];
        if(!(value.hideCapture?TwinKartObjective::HideCaptureOptions(value.request,value.options,ARRAYSIZE(value.options)):
            value.factoryCapture?TwinKartFactory::CaptureOptions(value.request,value.options,ARRAYSIZE(value.options)):
            TwinKartRace::Options(value.request,value.options,ARRAYSIZE(value.options))))continue;
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
            ",\"prepare_requested\":"+(value.status==1 && TwinKartRace::FreeDrive(value.request)?"true":"false")+
            ",\"travel_requested\":"+(value.status==1 && !TwinKartRace::FreeDrive(value.request)?"true":"false")+
            ",\"race_verified\":false,\"game_thread\":"+std::to_string(value.executedThread)+"}");
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
    if(c && c<ARRAYSIZE(control)-64) {
        swprintf_s(practiceTicketPath,L"%s.practice-ticket",control);
        swprintf_s(practiceActivePath,L"%s.practice-active",control);
        swprintf_s(practiceTemporaryPath,L"%s.practice-%lu.tmp",control,GetCurrentProcessId());
    }
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
            wchar_t events[4]{};
            if(GetEnvironmentVariableW(L"TWIN_KART_EVENT_MODES",events,4)==1 && events[0]==L'1') {
                // Two audited Cube bases carry ENone and are excluded from
                // ordinary racing enumeration. Query these exact aliases too,
                // preserving their raw mask/resource facts for event selection.
                const char* cubeBases[]={"Desert_I06","Forest_I05","Ice_I05","Village_I05","World_I11"};
                for(auto alias:cubeBases)if(known.insert(alias).second)aliases.push_back(alias);
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
extern "C" __declspec(dllexport) BOOL WINAPI TwinKartClipHideSeekerView(void* raw,float* xyz) {
    using namespace TwinKartRace;
    if(!xyz || !hideLobbyEnabled || !hideSearchActive || !hideRoundInputsActive || !hideRoundRosterKnown ||
       GetCurrentThreadId()!=queue.ThreadId())return FALSE;
    __try {
        auto camera=static_cast<BYTE*>(raw),world=OwnedRaceWorld();
        if(!world || !TwinKartLocalRace::Live(camera) || *reinterpret_cast<BYTE**>(camera)!=image+0x51086c8)return FALSE;
        auto pawn=*reinterpret_cast<BYTE**>(camera+0xf20);
        if(!HideRolePawn(pawn,world) || *reinterpret_cast<BYTE**>(pawn)!=image+0x501a0c8)return FALSE;
        for(unsigned i=0;i<8;++i) {
            if(hideRoundPawns[i].object!=pawn || !HideMaterialIdentityLive(hideRoundPawns[i]) ||
               !HideMaterialIdentityLive(hideDisguiseControllers[i]))continue;
            auto controller=static_cast<BYTE*>(hideDisguiseControllers[i].object);
            if(!TwinKartObjective::HideControllerHasPlayer(controller) || *reinterpret_cast<void**>(controller+0x250)!=pawn ||
               *reinterpret_cast<void**>(pawn+0x258)!=controller)return FALSE;
            ArenaPoint pivot{},eye{xyz[0],xyz[1],xyz[2]};
            if(!TwinKartLocalRace::FlagPosition(pawn,pivot) || !Finite(eye) || ArenaDistance(pivot,eye)>2400.f ||
               std::fabs(pivot.z-eye.z)>1000.f)return FALSE;
            pivot.z+=135.f;
            static int staticQuery=-1;
            if(staticQuery<0)for(int q=0;q<32;++q)if(reinterpret_cast<unsigned char(__fastcall*)(int)>(image+0x3f159a0)(q)==0) {staticQuery=q;break;}
            if(staticQuery<0)return FALSE;
            unsigned char channel=static_cast<unsigned char>(staticQuery);void* ignoredActor=pawn;
            QueryArray types{&channel,1,1},ignored{&ignoredActor,1,1};QueryColor color{};
            alignas(16) unsigned char hit[0x90]{};
            auto trace=reinterpret_cast<ObjectTrace>(image+0x3ff5990);
            if(!trace(world,&pivot,&eye,&types,false,&ignored,0,hit,false,&color,&color,0.f) || (hit[0x5d]&3)!=1)return FALSE;
            float time=0;memcpy(&time,hit+4,sizeof(time));
            if(!TwinKartObjective::ClipHideCamera(pivot,eye,time))return FALSE;
            xyz[0]=eye.x;xyz[1]=eye.y;xyz[2]=eye.z;
            static TwinKartCamera::ObjectIdentity logged{};
            if(logged.object!=pawn || logged.serial!=hideRoundPawns[i].serial) {
                logged=hideRoundPawns[i];RuntimeEvent("native_hide_seeker_camera_wall_clipped",i,static_cast<int>(time*1000));
            }
            return TRUE;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER){}
    return FALSE;
}
BOOL WINAPI DllMain(HINSTANCE self,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH) { module=self; DisableThreadLibraryCalls(self); HANDLE worker=CreateThread(nullptr,0,Worker,nullptr,0,nullptr); if(worker) CloseHandle(worker); }
    return TRUE;
}
