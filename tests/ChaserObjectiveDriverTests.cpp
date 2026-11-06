#include <cstdio>
#include <stdexcept>
#include <limits>
#include "../native/ChaserArenaDriver.h"
namespace O=TwinKartObjective;namespace R=TwinKartRace;
void Need(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
bool Line(const R::ArenaPoint&,const R::ArenaPoint&,void*){return true;}
bool Zero(const R::ArenaControl& c){return !c.throttle && !c.steer && !c.reverse;}
void Fill(R::FlagGroundNavigation& field,bool wall=false) {
    R::ArenaPoint p;while(field.Next(p))field.Sample(0,!wall || std::fabs(p.x-3200)>1);
}
struct CountedRoad {
    R::FlagGroundNavigation& road;mutable unsigned paths=0;
    bool Ready()const{return road.Ready();}size_t Count()const{return road.Count();}
    int RegionAt(const R::ArenaPoint& p,O::ChaserLine l,void* c)const{return road.RegionAt(p,l,c);}
    bool ReachableSampleAt(size_t i,int region,R::ArenaPoint& p,O::ChaserLine l,void* c)const{return road.ReachableSampleAt(i,region,p,l,c);}
    bool Path(const R::ArenaPoint& a,const R::ArenaPoint& b,std::vector<R::ArenaPoint>& p,O::ChaserLine l,void* c)const {
        ++paths;return road.Path(a,b,p,l,c);
    }
};
int main(){try {
    R::FlagGroundNavigation field;Need(field.Initialize({0,0,0},{6000,4000,0},400),"Objective road initialization");Fill(field);
    O::ChaserObservation seats[3]{{{400,400,0},O::ChaserRole::Tagger,true},{{2800,2800,0},O::ChaserRole::Runaway,true},
        {{4000,800,0},O::ChaserRole::Runaway,true}};
    O::ChaserObjectiveView view;view.vaultCount=view.zoneCount=1;
    view.vaults[0]={101,{4400,2800,0},-1,true};view.zones[0]={201,{5600,3200,0},true};
    Need(O::ValidChaserObjectives(&view,3),"Valid native objective observations rejected");
    O::ChaserArenaDriver driver;
    auto step=[&](std::uint64_t now,bool plan=true){return driver.Step(field,seats,3,1,0,.05f,now,true,plan,Line,nullptr,&view);};
    Need(!Zero(step(1000)) && driver.ObjectiveKind()==O::ChaserObjectiveKind::RecoverVault && driver.ObjectiveIdentity()==101 &&
         driver.Goal().x==4400 && view.vaults[0].holder== -1,"Runaway did not approach an actually free original vault, or fabricated pickup");
    view.vaults[0].holder=1;
    Need(Zero(step(1050,false)) && !driver.RouteSize(),"Observed pickup kept an obsolete free-vault route before replanning");
    step(1100);Need(driver.ObjectiveKind()==O::ChaserObjectiveKind::DeliverVault && driver.ObjectiveIdentity()==201 &&
        driver.Goal().x==5600 && view.vaults[0].holder==1,"Native holder did not approach the real delivery volume, or modified holder state");
    view.vaults[0].holder=2;
    Need(Zero(step(1150,false)) && !driver.RouteSize(),"Lost native possession retained a delivery route");
    step(1200);Need(driver.ObjectiveKind()==O::ChaserObjectiveKind::None,"A vault held by someone else was invented as free");
    view.vaults[0].holder=-1;step(2000);view.vaults[0].position.x=4800;
    Need(Zero(step(2050,false)) && !driver.RouteSize(),"A moving vault kept a stale stationary approach");
    step(2100);view.vaults[0].identity=102;
    Need(Zero(step(2150,false)) && !driver.RouteSize(),"A replaced native vault identity retained an old approach");
    step(2200);seats[1].role=O::ChaserRole::Tagger;
    step(2250);Need(driver.ObjectiveKind()==O::ChaserObjectiveKind::None && driver.TargetSlot()==2,
        "Native role change did not replace vault collection with pursuit");
    seats[1].role=O::ChaserRole::Runaway;seats[0].position={3600,2800,0};step(2300);
    Need(driver.ObjectiveKind()==O::ChaserObjectiveKind::None,"Runaway ignored immediate native tagger danger to collect a vault");
    seats[0].position={400,400,0};
    R::FlagGroundNavigation split;Need(split.Initialize({0,0,0},{6000,4000,0},400),"Disconnected road initialization");Fill(split,true);
    driver.Reset();driver.Step(split,seats,3,1,0,.05f,3000,true,true,Line,nullptr,&view);
    Need(driver.ObjectiveKind()==O::ChaserObjectiveKind::None,"Objective policy crossed disconnected road regions");
    view.vaults[0].holder=1;view.zoneCount=2;view.zones[1]=view.zones[0];
    Need(!O::ValidChaserObjectives(&view,3),"Duplicate original zone identities accepted");
    driver.Reset();step(3100);Need(driver.ObjectiveKind()==O::ChaserObjectiveKind::None,"Malformed native objectives produced a delivery goal");
    view.zoneCount=1;view.vaults[0].holder=3;Need(!O::ValidChaserObjectives(&view,3),"Out-of-roster native holder accepted");
    view.vaults[0].holder=-1;view.vaults[0].position.z=std::numeric_limits<float>::quiet_NaN();
    Need(!O::ValidChaserObjectives(&view,3),"Nonfinite original objective pose accepted");view.vaults[0].position.z=0;
    view.vaultCount=65;Need(!O::ValidChaserObjectives(&view,3),"Truncated native objective array accepted");view.vaultCount=64;
    for(unsigned i=0;i<64;++i)view.vaults[i]={500+i,{4400,1600+static_cast<float>(i)*20,0},-1,true};
    CountedRoad counted{field};driver.Reset();driver.Step(counted,seats,3,1,0,.05f,4000,true,true,Line,nullptr,&view);
    Need(driver.ObjectiveKind()==O::ChaserObjectiveKind::RecoverVault && counted.paths<=4,
        "Original objective count expanded the expensive route-planning budget");
    seats[1].position={0,0,0};
    Need(Zero(driver.Step(field,seats,3,1,0,.05f,4025,true,false,Line,nullptr,&view)) &&
         driver.ObjectiveKind()==O::ChaserObjectiveKind::None && !driver.RouteSize(),
         "Native respawn/teleport retained an objective and motion history from the old pose");
    seats[1].position={2800,2800,0};
    Need(Zero(driver.Step(field,seats,3,0,0,.05f,4050,true,true,Line,nullptr,&view)),"Vault policy wrote human controls");
    Need(Zero(driver.Step(field,seats,3,1,0,.05f,4100,false,true,Line,nullptr,&view)) && !driver.RouteSize() &&
         driver.ObjectiveKind()==O::ChaserObjectiveKind::None,"Paused/expired scene retained objective controls");
    puts("PASS observed Chaser vault objectives: free pickup approach, native-holder delivery, lost/moving/replaced targets, role/danger/disconnected-region fallback, malformed snapshots, human/pause isolation and four-route budget. No native pickup, reward, item use or match executed.");return 0;
}catch(const std::exception& e){fprintf(stderr,"%s\n",e.what());return 1;}}
