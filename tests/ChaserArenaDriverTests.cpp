#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <array>
#include <cstdio>
#include <stdexcept>
#include <limits>
#include "../native/ChaserArenaDriver.h"
#include "../native/NativeWorldBounds.h"
#include "../native/ChaserArenaScene.h"
#include "../native/NativeModeBinding.h"
#include "../native/NativeEventMenu.h"
namespace O=TwinKartObjective;
namespace R=TwinKartRace;
namespace N=TwinKartNavigation;
void Need(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct Bounds {void* world=nullptr;R::ArenaPoint center{},extent{};};
std::array<Bounds,4> actors;
void* actorPointers[4]{};unsigned collects=0,frees=0,reads=0;int collectedCount=4,collectedCapacity=4;
void __fastcall Collect(void*,void*,N::ActorArray* out){++collects;*out={actorPointers,collectedCount,collectedCapacity};}
void __fastcall Free(void* p){Need(p==actorPointers,"Native bounds task freed a borrowed actor rather than its array");++frees;}
bool Read(void* raw,void* world,R::ArenaPoint& p,R::ArenaPoint& e,void*) {
    ++reads;auto& a=*static_cast<Bounds*>(raw);if(a.world!=world)return false;p=a.center;e=a.extent;return true;
}
bool clear=true;
bool Line(const R::ArenaPoint&,const R::ArenaPoint&,void*){return clear;}
unsigned immediateChecks=0;
bool ImmediateBlocked(const R::ArenaPoint&,const R::ArenaPoint&,void*){++immediateChecks;return false;}
unsigned liveAimChecks=0;R::ArenaPoint liveAim{};
bool ImmediateClear(const R::ArenaPoint&,const R::ArenaPoint& aim,void*){++liveAimChecks;liveAim=aim;return true;}
unsigned rearChecks=0;R::ArenaPoint rearAim{};bool rearAllowed=true;
bool Rear(const R::ArenaPoint&,const R::ArenaPoint& aim,void*){++rearChecks;rearAim=aim;return rearAllowed;}
bool Zero(const R::ArenaControl& c){return c.throttle==0 && c.steer==0 && !c.reverse;}
bool SceneRoad(const R::ArenaPoint& p,R::ArenaPoint& contact,void*){contact={p.x,p.y,0};return true;}
unsigned sceneCalls=0,sceneRollbacks=0;int sceneReject=-1;bool sceneThrow=false;
bool SceneSpawn(O::ChaserSceneActor kind,unsigned index,const O::ObjectiveTransform&,void*) {
    unsigned expected=sceneCalls<8?sceneCalls:sceneCalls<11?sceneCalls-8:sceneCalls-11;
    auto expectedKind=sceneCalls<8?O::ChaserSceneActor::Start:sceneCalls<11?O::ChaserSceneActor::Vault:O::ChaserSceneActor::Zone;
    Need(kind==expectedKind && index==expected,"Scene native spawn order changed");
    if(sceneThrow && sceneCalls==9)throw std::runtime_error("Synthetic spawn fault");
    return static_cast<int>(sceneCalls++)!=sceneReject;
}
void SceneRollback(void*){++sceneRollbacks;}
void SceneChecks() {
    R::ArenaPoint anchors[2]{{0,0,160},{7000,0,160}};O::ChaserArenaPlan plan;
    Need(O::BuildChaserArenaPlan(anchors,2,SceneRoad,nullptr,plan),"Chaser scene not road-projected");
    Need(O::SpawnChaserScene(plan,SceneSpawn,SceneRollback,nullptr) && sceneCalls==13 && !sceneRollbacks,
        "Eight starts/three vaults/two native zones were not prepared atomically");
    for(int rejected=0;rejected<13;++rejected) {
        sceneCalls=sceneRollbacks=0;sceneReject=rejected;
        Need(!O::SpawnChaserScene(plan,SceneSpawn,SceneRollback,nullptr) && sceneCalls==static_cast<unsigned>(rejected+1) && sceneRollbacks==1,
            "Partial native scene published readiness or skipped rollback");
    }
    sceneReject=-1;sceneCalls=sceneRollbacks=0;sceneThrow=true;bool caught=false;
    try {O::SpawnChaserScene(plan,SceneSpawn,SceneRollback,nullptr);}catch(const std::runtime_error&){caught=true;}
    Need(caught && sceneRollbacks==1,"Scene callback exception lost rollback");sceneThrow=false;
    auto malformed=plan;malformed.zones[1].translation[0]=NAN;sceneCalls=sceneRollbacks=0;
    Need(!O::SpawnChaserScene(malformed,SceneSpawn,SceneRollback,nullptr) && !sceneCalls && !sceneRollbacks,
        "Malformed late transform mutated earlier native actors");
    Need(!O::BuildChaserArenaPlan(anchors,1,SceneRoad,nullptr,plan),"Missing arena anchors accepted");
    R::Request request;strcpy_s(request.identity,"0123456789abcdef0123456789abcdef");strcpy_s(request.track,"Flag_F01");request.mode=108;
    Need(R::Valid(request) && R::TeamSize(108)==16 && R::TeamCount(108,8)==1 && !R::TeamMode(108) && R::SlotTeam(108,7,8)==0 &&
        !R::CircuitMode(108) && !R::BehaviorGetter(108) && !R::ResultFactory(108),"Chaser was routed through ordinary circuit/team results");
    R::ModeDefinition mode;mode.asset=&mode;mode.kind=2;mode.teamSize=16;mode.teamCount=1;
    wcscpy_s(mode.gameClass,L"/Script/KartDrift.ChaserGameMode");wcscpy_s(mode.travelName,L"Chaser");wchar_t options[1024]{};
    Need(R::ModeOptions(request,mode,options,1024) && wcsstr(options,L"?players=1?observers=0?ais=7") &&
        wcsstr(options,L"?racemode=108") && !wcsstr(options,L"?lap="),"Original Chaser population/travel options changed");
    strcpy_s(request.track,"Flag_F02");Need(!R::Valid(request),"Unprepared Flag arena silently aliased as Chaser");
    strcpy_s(request.track,"Flag_F01");strcpy_s(request.custom,"?game.useitem=false");Need(!R::Valid(request),"Unimplemented Chaser rule override accepted");
    R::EventMenuCopySource menu;Need(menu.Build(&menu,true,true),"Combined native mode rows not built");
    auto& row=menu.source.rows.data[menu.source.rows.count-1];
    Need(!wcscmp(row.id.data,L"TwinKart_Chaser") && row.modes.count==1 && row.modes.data[0].mode==108 && row.modes.data[0].profile.count==1,
        "Chaser native menu row lost its original empty-profile mode binding");
    puts("PASS Chaser arena source contracts: eight road-projected starts, three vaults, two native delivery zones, all-stage rollback, malformed-input isolation, exact arena/mode/population, original Chaser rules/results separation and gated native menu row. Synthetic callbacks only.");
}
void Fill(R::FlagGroundNavigation& field){R::ArenaPoint p;while(field.Next(p))field.Sample(0,true);}
struct TargetRoad {
    R::FlagGroundNavigation& road;O::ChaserArenaDriver& driver;
    mutable unsigned ordinary=0;mutable std::vector<int> requested,previousTargets;bool rejectHuman=false,rejectAll=false;
    bool Ready()const{return road.Ready();}size_t Count()const{return road.Count();}
    int RegionAt(const R::ArenaPoint& p,O::ChaserLine line,void* context)const {
        // An ordinary native line query starting inside the prey can reject
        // its anchor. Only the explicit candidate scope may qualify that path.
        return p.x==800 && p.y==800?-1:road.RegionAt(p,line,context);
    }
    bool ReachableSampleAt(size_t i,int region,R::ArenaPoint& p,O::ChaserLine line,void* context)const {
        return road.ReachableSampleAt(i,region,p,line,context);
    }
    bool Path(const R::ArenaPoint&,const R::ArenaPoint&,std::vector<R::ArenaPoint>& route,O::ChaserLine,void*)const {
        ++ordinary;route.clear();return false;
    }
    bool PathToTarget(const R::ArenaPoint& first,const R::ArenaPoint& last,int target,std::vector<R::ArenaPoint>& route,
        O::ChaserLine line,void* context)const {
        requested.push_back(target);previousTargets.push_back(driver.TargetSlot());
        if(rejectAll || (rejectHuman && !target)){route.clear();return false;}
        return road.Path(first,last,route,line,context);
    }
};
int main(){try {
    SceneChecks();
    int world=0,other=0,type=0;for(int i=0;i<4;++i)actorPointers[i]=&actors[i];
    actors[0]={&world,{0,0,0},{100,100,10}};actors[1]={&world,{30000,5000,100},{200,300,20}};
    actors[2]={&other,{90000,90000,0},{100,100,10}};
    actors[3]={&world,{std::numeric_limits<float>::quiet_NaN(),0,0},{100,100,10}};
    N::WorldBounds bounds;R::ArenaPoint low{7,7,7},high{8,8,8};
    Need(bounds.Begin(&world,&type,Collect,Read,Free) && collects==1 && !reads && !frees,"Bounds query was not collected exactly once");
    Need(bounds.Step(&world,1)==N::BoundsState::Reading && reads==1 && !bounds.Result(low,high) && low.x==7 && high.x==8,
         "An unfinished world-bounds query published a spawn-local partial scene");
    Need(bounds.Step(&world,2)==N::BoundsState::Reading && reads==3 && !frees,"Bounds task exceeded or ignored its per-pump budget");
    Need(bounds.Step(&world,2)==N::BoundsState::Ready && reads==4 && frees==1 && bounds.Accepted()==2 && bounds.Result(low,high) &&
         low.x==-100 && low.y==-100 && low.z==-10 && high.x==30200 && high.y==5300 && high.z==120,
         "Full native geometry bounds omitted distant scene geometry or included foreign/nonfinite actors");
    bounds.Reset();Need(frees==1,"Completed actor array was freed twice");
    Need(bounds.Begin(&world,&type,Collect,Read,Free) && bounds.Step(&other,1)==N::BoundsState::Rejected && frees==2,
         "World replacement retained a borrowed native actor list");
    collectedCount=5;Need(!bounds.Begin(&world,&type,Collect,Read,Free) && frees==3,"Malformed native actor-array capacity was accepted or leaked");collectedCount=4;
    Need(bounds.Begin(&world,&type,Collect,Read,Free) && bounds.Step(&world,0)==N::BoundsState::Rejected && frees==4,"Zero budget accepted or leaked actor storage");
    Need(bounds.Begin(&world,&type,Collect,Read,Free),"Restart bounds task failed");bounds.Reset();Need(frees==5,"Explicit runtime reset leaked a pending native allocation");
    R::FlagGroundNavigation field;Need(field.Initialize({0,0,0},{6000,4000,0},400),"Road fixture initialization");Fill(field);
    O::ChaserObservation seats[4]{{{800,800,0},O::ChaserRole::Runaway,true},{{400,800,0},O::ChaserRole::Tagger,true},
        {{4800,3200,0},O::ChaserRole::Runaway,true},{{400,3200,0},O::ChaserRole::Tagger,true}};
    Need(O::NativeChaserRole(true,false)==O::ChaserRole::Runaway && O::NativeChaserRole(false,true)==O::ChaserRole::Tagger &&
         O::NativeChaserRole(false,false)==O::ChaserRole::Unknown && O::NativeChaserRole(true,true)==O::ChaserRole::Unknown,
         "Transient native role components were mistaken for a stable role");
    O::ChaserArenaDriver driver;
    auto control=driver.Step(field,seats,4,1,0,0.05f,1000,true,true,Line,nullptr);
    Need(driver.TargetSlot()==0 && driver.RouteSize()>0 && control.throttle>0 && !control.reverse,
         "Tagger did not pursue the connected native runaway/human through a road route");
    O::ChaserArenaDriver scopedDriver;TargetRoad targetRoad{field,scopedDriver};
    auto scopedControl=scopedDriver.Step(targetRoad,seats,4,1,0,.05f,1000,true,true,Line,nullptr);
    Need(scopedDriver.TargetSlot()==0 && !Zero(scopedControl) && !targetRoad.ordinary &&
         targetRoad.requested==std::vector<int>({0,2}) && targetRoad.previousTargets==std::vector<int>({-1,0}),
         "Candidate path scope was bypassed, filtered by unscoped prey-anchor lookup, or prematurely selected as the driver target");
    scopedDriver.Reset();targetRoad.requested.clear();targetRoad.previousTargets.clear();targetRoad.rejectHuman=true;
    scopedDriver.Step(targetRoad,seats,4,1,0,.05f,1050,true,true,Line,nullptr);
    Need(scopedDriver.TargetSlot()==2 && targetRoad.requested==std::vector<int>({0,2}) &&
         targetRoad.previousTargets==std::vector<int>({-1,-1}) && !targetRoad.ordinary,
         "Failed candidate altered selected pursuit state or prevented a later valid prey path");
    puts("PASS Chaser candidate-path dispatch: actual prey slot including human, scoped anchor responsibility, no premature target mutation, rejected-candidate recovery and ordinary graph fallback. Synthetic navigation only; no original pawn/contact executed.");
    O::ChaserArenaDriver liveDriver;auto moving=std::array<O::ChaserObservation,4>{seats[0],seats[1],seats[2],seats[3]};
    liveDriver.Step(field,moving.data(),4,1,0,.05f,1000,true,true,Line,nullptr);
    moving[0].position.x+=100;moving[1].position.x=650;clear=false;
    auto liveControl=liveDriver.Step(field,moving.data(),4,1,0,.05f,1050,true,false,Line,nullptr,nullptr,ImmediateClear,nullptr);
    Need(!Zero(liveControl) && liveAimChecks==1 && liveAim.x==moving[0].position.x && liveAim.y==moving[0].position.y,
         "An unscoped ray vetoed the stronger immediate target adapter or retained the old prey aim");clear=true;
    Need(Zero(driver.Step(field,seats,4,1,0,0.05f,1025,true,false,Line,nullptr,nullptr,ImmediateBlocked,nullptr)) &&
         immediateChecks==1 && driver.RouteSize(),"Fresh obstruction emitted input or erased the stall-recovery intent");
    O::ChaserArenaDriver recovering;TargetRoad blockedRoad{field,recovering};
    recovering.Step(blockedRoad,seats,4,1,0,.05f,1000,true,true,Line,nullptr,nullptr,ImmediateBlocked,nullptr,Rear,nullptr);
    blockedRoad.rejectAll=true;bool reversed=false;
    for(unsigned tick=1;tick<=55;++tick) {
        auto recovery=recovering.Step(blockedRoad,seats,4,1,0,.05f,1000+tick*50,true,true,Line,nullptr,
            nullptr,ImmediateBlocked,nullptr,Rear,nullptr);
        if(recovery.reverse) {
            reversed=true;Need(recovery.steer==0 && rearAim.x==seats[1].position.x-250 && rearAim.y==seats[1].position.y,
                "Reverse used the forward aim or an unqualified angular turn");
        } else Need(Zero(recovery),"Blocked forward route emitted input");
    }
    Need(reversed && rearChecks && recovering.TargetSlot()==0 && recovering.RouteSize(),
        "Deferred/blocked replan starved stall recovery or discarded the selected native target");
    rearAllowed=false;
    Need(Zero(recovering.Step(blockedRoad,seats,4,1,0,.05f,3800,true,false,Line,nullptr,nullptr,
        ImmediateBlocked,nullptr,Rear,nullptr)),"Blocked rear segment received reverse input");
    Need(Zero(recovering.Step(blockedRoad,seats,4,1,0,.05f,3850,true,false,Line,nullptr,nullptr,
        ImmediateBlocked,nullptr)),"Forward approach permission substituted for absent rear-body qualification");
    rearAllowed=true;
    Need(Zero(recovering.Step(blockedRoad,seats,4,1,0,.05f,3900,false,false,Line,nullptr,nullptr,
        ImmediateBlocked,nullptr,Rear,nullptr)) && !recovering.RouteSize(),"Pause retained recovery input");
    puts("PASS Chaser blocked-route recovery: transactional failed replan, observed stall preservation, fresh rear segment, straight reverse only, blocked/missing rear qualification and pause neutralization. Synthetic inputs only.");
    driver.Step(field,seats,4,1,0,0.05f,1030,true,true,Line,nullptr);
    seats[0].role=O::ChaserRole::Tagger;
    Need(Zero(driver.Step(field,seats,4,1,0,0.05f,1050,true,false,Line,nullptr)) && driver.TargetSlot()==-1,
         "An infected target remained a chase target until the next expensive plan");
    driver.Step(field,seats,4,1,0,0.05f,1100,true,true,Line,nullptr);
    Need(driver.TargetSlot()==2,"Tagger did not reselect a remaining runaway after native infection");
    seats[1].position={5600,3200,0};
    Need(Zero(driver.Step(field,seats,4,1,0,0.05f,1150,true,false,Line,nullptr)) && !driver.RouteSize(),
         "Native respawn/teleport retained a route and speed estimate from the old location");
    seats[1].position={400,800,0};
    seats[1].role=O::ChaserRole::Runaway;
    driver.Step(field,seats,4,1,0,0.05f,2000,true,true,Line,nullptr);
    Need(driver.Role()==O::ChaserRole::Runaway && driver.TargetSlot()==-1 && driver.RouteSize()>0 &&
         O::ChaserSafety(driver.Goal(),seats,4)>O::ChaserSafety(seats[1].position,seats,4),"Runaway did not choose a reachable escape away from taggers");
    Need(Zero(driver.Step(field,seats,4,1,0,0.05f,2050,false,true,Line,nullptr)) && !driver.RouteSize(),"Paused/post-game control retained acceleration");
    Need(Zero(driver.Step(field,seats,4,0,0,0.05f,2100,true,true,Line,nullptr)),"Objective driver controlled the human slot");
    clear=false;Need(Zero(driver.Step(field,seats,4,1,0,0.05f,2200,true,true,Line,nullptr)),"Blocked route caused direct off-road pursuit");clear=true;
    seats[1].role=O::ChaserRole::Unknown;Need(Zero(driver.Step(field,seats,4,1,0,0.05f,2250,true,true,Line,nullptr)),"Native role transition retained old controls");
    seats[1].role=O::ChaserRole::Tagger;seats[2].ready=false;
    Need(Zero(driver.Step(field,seats,4,1,0,0.05f,2300,true,true,Line,nullptr)),"Tagger fabricated a target after the last runaway disconnected");
    seats[0].position={500,0,0};
    Need(O::ChaserSegmentSafety({0,0,0},{1000,0,0},seats,4)==0,"Escape evaluation ignored a threat between route waypoints");
    puts("PASS original Chaser local policy and world-bounds contracts: distant native geometry, bounded actor work/ownership cleanup, human pursuit, infection retargeting, reachable escape, role/pause/obstacle/disconnect neutral input. Synthetic roads and observations only; no native driving, contacts or gameplay accepted.");return 0;
}catch(const std::exception& e){fprintf(stderr,"%s\n",e.what());return 1;}}
