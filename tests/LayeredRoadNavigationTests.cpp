#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <stdexcept>
#include <limits>
#include "../native/LayeredRoadNavigation.h"
#include "../native/ChaserArenaDriver.h"
namespace R=TwinKartRace;
namespace O=TwinKartObjective;
void Need(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
unsigned nativeCalls=0;bool blocking=true,penetrating=false;float nativeZ=600,nativeNormal=1,nativeOffsetX=0;
float nativeNormalX=0;bool nativeOverwrite=false;
int nativeParameters=0;
bool __fastcall NativeTrace(void* world,void* raw,const R::ArenaPoint* start,const R::ArenaPoint* end,const void* mask,const void* parameters) {
    ++nativeCalls;Need(world && parameters==&nativeParameters && *static_cast<const unsigned*>(mask)==0x20000 && start->z>end->z,
        "Layered native road query changed its original mask/arguments");
    auto hit=static_cast<unsigned char*>(raw);float initial=0;memcpy(&initial,hit+4,4);
    Need(reinterpret_cast<std::uintptr_t>(raw)%16==0 && initial==1,"Incomplete/unaligned original road hit storage");
    memset(hit,0xa5,0x88);R::ArenaPoint point{start->x+nativeOffsetX,start->y,nativeZ};
    R::ArenaPoint normal{nativeNormalX,std::sqrt((std::max)(0.f,1-nativeNormal*nativeNormal)),nativeNormal};
    memcpy(hit+0xc,&point,sizeof(point));memcpy(hit+0x30,&normal,sizeof(normal));hit[0x5d]=static_cast<unsigned char>((blocking?1:0)|(penetrating?2:0));
    if(nativeOverwrite)hit[0x88]=0;return blocking;
}
bool Clear(const R::ArenaPoint&,const R::ArenaPoint&,void*){return true;}
struct ConnectorCheck {bool rejectStart=false,rejectGoal=false,blockNearest=false;unsigned start=0,goal=0,edges=0;};
bool QualifyConnector(const R::ArenaPoint& first,const R::ArenaPoint& last,void* raw) {
    auto& f=*static_cast<ConnectorCheck*>(raw);
    if(first.z==100){++f.start;return !f.rejectStart && !(f.blockNearest && last.x==0 && last.y==400);}
    if(last.z==100){++f.goal;return !f.rejectGoal && !(f.blockNearest && first.x==1600 && first.y==400);}
    ++f.edges;return true;
}
void Feed(R::LayeredRoadNavigation& field,bool ramp=false) {
    R::ArenaPoint start{},end{};unsigned calls=0;
    while(field.NextRay(start,end)) {
        Need(++calls<10000,"Layered sample cursor failed to make progress");
        float values[3]{800,0,-10000};int count=2;
        if(ramp) {
            int x=static_cast<int>(std::round(start.x/400));float z=std::clamp((x-2)*200.f,0.f,800.f);
            values[2]=z;count=3;
        }
        bool found=false;float z=-1.e30f;
        for(int i=0;i<count;++i)if(values[i]<=start.z && values[i]>=end.z && values[i]>z){z=values[i];found=true;}
        field.Sample(found?R::RoadSurfaceResult::Hit:R::RoadSurfaceResult::Miss,{{start.x,start.y,z},1,false});
    }
    Need(field.SamplingDone() && !field.Rejected(),"Valid stacked road sampling was rejected");
    while(!field.Ready())Need(field.BuildEdges(2) || !field.Rejected(),"Bounded edge construction rejected a valid field");
}
void Resolve(R::LayeredRoadNavigation& field,bool ramp=false) {
    for(size_t i=0;i<field.AdjacencyCount();++i) {
        int a=0,b=0;R::ArenaPoint x{},y{};Need(field.AdjacencyAt(i,a,b,x,y),"Missing original candidate edge");
        bool physical=std::fabs(x.z-y.z)<1;
        if(ramp) {
            auto height=[](const R::ArenaPoint& p){return std::clamp((std::round(p.x/400)-2)*200.f,0.f,800.f);};
            physical|=std::fabs(x.z-height(x))<1 && std::fabs(y.z-height(y))<1;
        }
        Need(field.ResolveTransition(i,physical),"Native collision proof was not recorded");
    }
}
int main(){try {
    int world=0;R::RoadSurface surface{{7,8,9},0,false};
    Need(R::QueryRoadSurface(&world,NativeTrace,&nativeParameters,{0,0,1000},{0,0,-100},surface)==R::RoadSurfaceResult::Hit &&
         surface.point.z==600 && surface.normalZ==1 && !surface.penetrating && nativeCalls==1,"Original surface/contact ABI was not decoded");
    penetrating=true;Need(R::QueryRoadSurface(&world,NativeTrace,&nativeParameters,{0,0,1000},{0,0,-100},surface)==R::RoadSurfaceResult::Hit &&
         surface.penetrating,"Original initial-penetration bit was lost");penetrating=false;
    nativeNormal=-1;Need(R::QueryRoadSurface(&world,NativeTrace,&nativeParameters,{0,0,1000},{0,0,-100},surface)==R::RoadSurfaceResult::Hit &&
         surface.normalZ==-1,"A nonwalkable underside was misreported as a ray miss");nativeNormal=1;
    nativeNormal=0.8f;Need(R::QueryRoadSurface(&world,NativeTrace,&nativeParameters,{0,0,1000},{0,0,-100},surface)==R::RoadSurfaceResult::Hit &&
         std::fabs(surface.normal.y-0.6f)<0.001f && surface.normal.z==0.8f,"Full native impact normal lost a bank/slope component");nativeNormal=1;
    nativeNormalX=1;surface.point.z=42;
    Need(R::QueryRoadSurface(&world,NativeTrace,&nativeParameters,{0,0,1000},{0,0,-100},surface)==R::RoadSurfaceResult::Invalid &&
         surface.point.z==42,"Invalid full impact normal published a road frame");nativeNormalX=0;
    nativeOverwrite=true;Need(R::QueryRoadSurface(&world,NativeTrace,&nativeParameters,{0,0,1000},{0,0,-100},surface)==R::RoadSurfaceResult::Invalid &&
         surface.point.z==42,"Native FHitResult output overwrite was accepted");nativeOverwrite=false;
    surface.point.z=42;blocking=false;
    Need(R::QueryRoadSurface(&world,NativeTrace,&nativeParameters,{0,0,1000},{0,0,-100},surface)==R::RoadSurfaceResult::Miss &&
         surface.point.z==42,"A miss published a partial contact");blocking=true;
    nativeOffsetX=10;Need(R::QueryRoadSurface(&world,NativeTrace,&nativeParameters,{0,0,1000},{0,0,-100},surface)==R::RoadSurfaceResult::Invalid &&
         surface.point.z==42,"Malformed original contact escaped as a road surface");nativeOffsetX=0;
    unsigned before=nativeCalls;Need(R::QueryRoadSurface(&world,NativeTrace,&nativeParameters,{0,0,1000},{1,0,-100},surface)==R::RoadSurfaceResult::Invalid &&
         nativeCalls==before,"Nonvertical sampler arguments reached the native ray API");
    R::LayeredRoadNavigation decks;Need(decks.Initialize({0,0,-200},{1600,800,1200},400),"Deck field initialization");Feed(decks);
    Need(decks.ValidCount()==decks.ColumnCount()*2 && !decks.Verified(),"Sampling discarded the lower road or accepted unresolved collision links");
    std::vector<R::ArenaPoint> path{{7,7,7}};
    Need(!decks.Path({0,400,100},{1600,400,100},path) && path.empty(),"Unverified graph returned a route");Resolve(decks);
    R::ArenaPoint anchor{7,8,9};
    Need(decks.AnchorAt({0,400,100},anchor,Clear,nullptr) && anchor.z==0 &&
         decks.AnchorAt({0,400,900},anchor,Clear,nullptr) && anchor.z==800,
         "Road anchor used a native actor's pivot Z instead of its actual connected graph layer");
    anchor={7,8,9};Need(!decks.AnchorAt({0,400,std::numeric_limits<float>::quiet_NaN()},anchor,Clear,nullptr) && anchor.z==9,
         "Invalid road anchor published a partial target surface");
    Need(decks.Verified() && decks.Path({0,400,100},{1600,400,100},path,Clear,nullptr),"Lower road under a bridge was lost");
    for(size_t i=0;i+1<path.size();++i)Need(path[i].z==0,"Lower-road route switched onto the overlying deck");
    ConnectorCheck connectors;connectors.rejectStart=true;
    Need(!decks.Path({10,400,100},{1590,400,100},path,Clear,nullptr,QualifyConnector,&connectors) &&
         path.empty() && connectors.start>0 && !connectors.edges,
         "Start-to-road connector bypassed traversal qualification");
    connectors={};connectors.rejectGoal=true;
    Need(!decks.Path({10,400,100},{1590,400,100},path,Clear,nullptr,QualifyConnector,&connectors) &&
         path.empty() && connectors.start==1 && connectors.goal>0 && !connectors.edges,
         "Road-to-goal connector bypassed traversal qualification or published a partial route");
    connectors={};
    Need(decks.Path({10,400,100},{1590,400,100},path,Clear,nullptr,QualifyConnector,&connectors) &&
         connectors.start==1 && connectors.goal==1 && path.size()==6,
         "Qualified endpoint connectors or retained graph edges were lost");
    connectors={};connectors.blockNearest=true;
    Need(decks.Path({10,400,100},{1590,400,100},path,Clear,nullptr,QualifyConnector,&connectors) &&
         connectors.start>1 && connectors.goal>1 && path.front().x==400 && path[path.size()-2].x==1200,
         "Blocked nearest body connectors prevented selection of actual qualified alternatives");
    decks.AllowSmoothing(false);
    Need(decks.Path({0,400,100},{1600,400,100},path,Clear,nullptr) && path.size()==6,
        "Unsmoothed native route skipped individually verified support edges");
    decks.AllowSmoothing(true);
    Need(decks.Path({0,400,900},{1600,400,900},path,Clear,nullptr),"Upper bridge route was lost");
    Need(!decks.Path({0,400,100},{0,400,900},path,Clear,nullptr),"Same-XY stacked roads acquired a fabricated vertical link");
    R::LayeredRoadNavigation lowBridge;Need(lowBridge.Initialize({0,0,-100},{800,400,400},400),"Low bridge fixture initialization");
    R::ArenaPoint lowStart{},lowEnd{};
    while(lowBridge.NextRay(lowStart,lowEnd)) {
        float z=lowStart.z>=150?150.f:lowStart.z>=0?0.f:-1000.f;
        lowBridge.Sample(z>=lowEnd.z?R::RoadSurfaceResult::Hit:R::RoadSurfaceResult::Miss,{{lowStart.x,lowStart.y,z},1,false});
    }
    while(!lowBridge.Ready())lowBridge.BuildEdges(2);Resolve(lowBridge);
    Need(lowBridge.RegionAt({0,0,100},Clear,nullptr)==lowBridge.RegionAt({0,0,0},Clear,nullptr) &&
         lowBridge.RegionAt({0,0,100},Clear,nullptr)!=lowBridge.RegionAt({0,0,150},Clear,nullptr),
         "A below-bridge actor root snapped upward onto the nearer overlying deck");
    O::ChaserObservation seats[3]{{{800,400,900},O::ChaserRole::Runaway,true},{{0,400,100},O::ChaserRole::Tagger,true},
        {{1600,400,100},O::ChaserRole::Runaway,true}};
    O::ChaserArenaDriver driver;driver.Step(decks,seats,3,1,0,0.05f,1000,true,true,Clear,nullptr);
    Need(driver.TargetSlot()==2,"Tagger chose an unreachable other-deck runaway over its actual connected road");
    R::LayeredRoadNavigation ramp;Need(ramp.Initialize({0,0,-200},{2800,800,1200},400),"Ramp field initialization");Feed(ramp,true);Resolve(ramp,true);
    Need(ramp.Path({0,400,100},{2800,400,900},path,Clear,nullptr),"Collision-proven ramp did not connect distinct road layers");
    bool climbed=false;for(size_t i=1;i+1<path.size();++i)if(path[i].z>path[i-1].z) {
        Need(R::ArenaDistance(path[i],path[i-1])>=std::fabs(path[i].z-path[i-1].z),"Ramp path jumped vertically between decks");climbed=true;
    }
    Need(climbed,"Ramp route did not retain a physical height transition");
    R::LayeredRoadNavigation bounded;
    Need(bounded.InitializeBounded({0,0,-500},{60000,50000,3000},300) && bounded.ColumnCount()<=20000,
         "Large retained arena failed to select a bounded layered sampling spacing");
    Need(!bounded.InitializeBounded({0,0,-500},{300000,300000,3000},300) && !bounded.Started(),
         "Oversized retained arena silently published truncated layered geometry");
    Need(!bounded.InitializeBounded({0,0,-500},{800,800,3000},std::numeric_limits<float>::quiet_NaN()) && !bounded.Started(),
         "Invalid layered sampling spacing was accepted");
    for(size_t i=0;i<ramp.AdjacencyCount();++i) {
        int a=0,b=0;R::ArenaPoint x{},y{};ramp.AdjacencyAt(i,a,b,x,y);
        if(std::fabs(x.z-y.z)>1)ramp.ResolveTransition(i,false);
    }
    Need(!ramp.Path({0,400,100},{2800,400,900},path,Clear,nullptr),"A newly blocked ramp retained stale connected-region state");
    R::LayeredRoadNavigation overflow;Need(overflow.Initialize({0,0,-100},{400,400,300},400),"Overflow fixture initialization");
    R::ArenaPoint start{},end{};
    for(int layer=0;layer<17;++layer){Need(overflow.NextRay(start,end),"Layer overflow test ended before its seventeenth native hit");
        overflow.Sample(R::RoadSurfaceResult::Hit,{{start.x,start.y,200-layer*10.f},1,false});}
    Need(overflow.Rejected() && !overflow.Ready() && !overflow.NextRay(start,end),"Layer limit silently published a truncated road graph");
    R::LayeredRoadNavigation penetration;Need(penetration.Initialize({0,0,-100},{400,400,1000},400),"Penetration fixture initialization");
    penetration.NextRay(start,end);float oldTop=start.z;penetration.Sample(R::RoadSurfaceResult::Hit,{start,1,true});
    Need(penetration.ValidCount()==0 && penetration.NextRay(start,end) && start.z<oldTop,"Initial penetration created a fake floor or stalled the ray cursor");
    penetration.Sample(R::RoadSurfaceResult::Hit,{{start.x,start.y,600},-1,false});
    Need(penetration.ValidCount()==0 && penetration.NextRay(start,end) && start.z<600,"Road underside blocked discovery of a lower surface");
    penetration.Sample(R::RoadSurfaceResult::Hit,{{start.x,start.y,0},1,false});
    Need(penetration.ValidCount()==1,"Valid lower surface was lost after a penetrating/underside contact");
    R::LayeredRoadNavigation malformed;Need(malformed.Initialize({0,0,-100},{400,400,1000},400),"Malformed sampler initialization");
    malformed.NextRay(start,end);malformed.Sample(R::RoadSurfaceResult::Hit,{{start.x,start.y,std::numeric_limits<float>::quiet_NaN()},1,false});
    Need(malformed.Rejected() && !malformed.Ready(),"Nonfinite contact produced partial graph acceptance");
    R::LayeredRoadNavigation stuck;Need(stuck.Initialize({0,0,-100},{400,400,100000},400),"Repeated penetration fixture initialization");
    for(unsigned i=0;i<513;++i) {
        Need(stuck.NextRay(start,end),"Repeated penetration test terminated before its bounded budget");
        stuck.Sample(R::RoadSurfaceResult::Hit,{start,1,true});
    }
    Need(stuck.Rejected() && !stuck.Ready(),"Repeated native penetration contacts caused unbounded work or partial acceptance");
    puts("PASS layered road contracts: exact complete contact/penetration ABI, both bridge decks, no same-XY vertical route, proven ramp connections, blocked-ramp invalidation, native-role pursuit by reachable layer, bounded progress and explicit overflow rejection. Synthetic geometry only; no native track or driving acceptance.");return 0;
}catch(const std::exception& e){fprintf(stderr,"%s\n",e.what());return 1;}}
