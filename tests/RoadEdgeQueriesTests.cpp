#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include "../native/RoadEdgeQueries.h"
#include "../native/LayeredRoadNavigation.h"
namespace R=TwinKartRace;
void Need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Fixture {
    R::RoadEdgeQueries queries;unsigned calls=0;bool blocked=false,reset=false,reenter=false,fail=false,fault=false;
};
bool Read(const R::ArenaPoint& a,const R::ArenaPoint& b,void* raw) {
    auto& f=*static_cast<Fixture*>(raw);++f.calls;
    if(f.reenter)Need(!f.queries.Check(a,b,Read,raw),"Reentrant native edge query was issued");
    if(f.reset)f.queries.Reset();
    if(f.fail)throw std::runtime_error("fixture query exception");
    if(f.fault)RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);
    return !f.blocked;
}
bool CatchNativeFault(Fixture& f) {
    __try {f.queries.Check({0,0,0},{400,0,0},Read,&f);return false;}
    __except(EXCEPTION_EXECUTE_HANDLER){return true;}
}
bool Clear(const R::ArenaPoint&,const R::ArenaPoint&,void*){return true;}
bool RoadRead(const R::ArenaPoint& a,const R::ArenaPoint& b,void* raw) {
    ++static_cast<Fixture*>(raw)->calls;
    return !(a.y==400 && b.y==400 && (std::min)(a.x,b.x)==800 && (std::max)(a.x,b.x)==1200);
}
bool Traverse(const R::ArenaPoint& a,const R::ArenaPoint& b,void* raw) {
    auto& f=*static_cast<Fixture*>(raw);return f.queries.Check(a,b,RoadRead,raw);
}
bool ConnectorRead(const R::ArenaPoint& a,const R::ArenaPoint& b,void* raw) {
    ++static_cast<Fixture*>(raw)->calls;
    // Several nearby anchors are across the synthetic body-width obstruction.
    // A farther same-layer anchor is reachable; internal roads remain clear.
    return a.z!=100 || (b.x>=800 && b.y==400);
}
bool ConnectorTraverse(const R::ArenaPoint& a,const R::ArenaPoint& b,void* raw) {
    auto& f=*static_cast<Fixture*>(raw);return f.queries.Check(a,b,ConnectorRead,raw);
}
struct Observations {R::RoadEdgeQueries queries;unsigned calls=0;R::RoadEdgeObservation result=R::RoadEdgeObservation::Invalid;};
R::RoadEdgeObservation ReadObservation(const R::ArenaPoint&,const R::ArenaPoint&,void* raw) {
    auto& f=*static_cast<Observations*>(raw);++f.calls;return f.result;
}
int main(){try {
    Fixture f;R::ArenaPoint a{},b{400,0,0},c{800,0,0};
    Need(!f.queries.Check(a,b,Read,&f) && !f.calls,"Unscoped graph query reached native callback");
    Need(f.queries.Begin(1,1000,2) && f.queries.Check(a,b,Read,&f) && f.calls==1,"First native directed edge query failed");
    Need(f.queries.Check(a,b,Read,&f) && f.calls==1 && f.queries.Check(b,a,Read,&f) && f.calls==2,
         "Observed edge was not cached or opposite direction reused an unproven traversal");
    Need(f.queries.Begin(1,1000,256) && !f.queries.Check(b,c,Read,&f) && f.calls==2 && f.queries.QueriesUsed()==2,
         "Same pump replenished native query budget or accepted an unknown edge");
    Need(f.queries.Begin(1,1050,1) && f.queries.Check(b,c,Read,&f) && f.calls==3,"Deferred unknown edge never retried next pump");
    f.blocked=true;f.queries.Invalidate();
    Need(!f.queries.Check(a,b,Read,&f) && f.calls==3,"Invalidation replenished query allowance");
    f.queries.Begin(1,1100,2);Need(!f.queries.Check(a,b,Read,&f) && f.calls==4,"Blocked edge was accepted");
    f.blocked=false;f.queries.Begin(1,1200,2);Need(!f.queries.Check(a,b,Read,&f) && f.calls==4,"Unexpired blocked observation was ignored");
    f.queries.Begin(1,1351,2);Need(f.queries.Check(a,b,Read,&f) && f.calls==5,"Expired blocker never allowed a fresh native observation");
    f.queries.Begin(1,11352,2);Need(f.queries.Check(a,b,Read,&f) && f.calls==6,"Old clear edge remained indefinitely authoritative");
    f.queries.Begin(2,11400,2);Need(f.queries.Check(a,b,Read,&f) && f.calls==7,"Another pawn/world generation reused old edge observations");
    f.queries.Begin(2,100,2);Need(f.queries.Check(a,b,Read,&f) && f.calls==8,"Clock rollback retained old observations");
    std::array<unsigned char,256> qualification{};
    Need(f.queries.Qualify(qualification) && f.queries.Check(a,b,Read,&f) && f.calls==9,
         "First exact body qualification retained old centerline observations");
    Need(f.queries.Qualify(qualification) && f.queries.Check(a,b,Read,&f) && f.calls==9,
         "Unchanged body qualification discarded valid observations");
    qualification[200]=1;
    Need(f.queries.Qualify(qualification) && !f.queries.Check(a,b,Read,&f) && f.calls==9,
         "New shape/pose qualification reused old clearance or replenished the pump allowance");
    Need(f.queries.Begin(2,150,2) && f.queries.Check(a,b,Read,&f) && f.calls==10,
         "Changed body qualification never allowed a fresh observation");
    R::ArenaPoint bad{std::numeric_limits<float>::quiet_NaN(),0,0};
    Need(!f.queries.Check(bad,b,Read,&f) && f.calls==10 && !f.queries.Begin(0,100,2) &&
         !f.queries.Begin(2,0,2) && !f.queries.Begin(2,100,257),"Invalid scope/point/budget reached native query");
    f.reset=true;f.queries.Begin(2,100,2);Need(!f.queries.Check(a,b,Read,&f),"Reset during callback published a stale traversal");
    f.reset=false;f.reenter=true;f.queries.Begin(2,200,2);Need(f.queries.Check(a,b,Read,&f),"Contained reentry corrupted native query");
    f.reenter=false;f.fail=true;f.queries.Invalidate();bool caught=false;
    try{f.queries.Check(a,b,Read,&f);}catch(const std::runtime_error&){caught=true;}
    Need(caught && f.queries.QueriesUsed()==2,"C++ callback failure did not retain consumed budget");
    f.fail=false;Need(f.queries.Begin(2,250,2) && f.queries.Check(a,b,Read,&f),"Exception left edge query permanently reentrant");
    f.queries.Invalidate();f.fault=true;Need(CatchNativeFault(f),"Native query fault escaped its test boundary");
    f.fault=false;Need(f.queries.Begin(2,300,2) && f.queries.Check(a,b,Read,&f),"SEH left edge query permanently reentrant");
    R::LayeredRoadNavigation road;Need(road.Initialize({0,0,-100},{1600,800,100},400),"Dynamic path fixture initialization");
    R::ArenaPoint start{},end{};
    while(road.NextRay(start,end))road.Sample(start.z>=0?R::RoadSurfaceResult::Hit:R::RoadSurfaceResult::Miss,{{start.x,start.y,0},1,false});
    while(!road.Ready())road.BuildEdges(2);
    for(size_t i=0;i<road.AdjacencyCount();++i)road.ResolveTransition(i,true);
    Fixture paths;std::vector<R::ArenaPoint> route;bool found=false;
    for(unsigned pump=1;pump<=100 && !found;++pump) {
        paths.queries.Begin(3,pump*50,4);unsigned before=paths.calls;
        found=road.Path({0,400,100},{1600,400,100},route,Clear,nullptr,Traverse,&paths);
        Need(paths.calls-before<=4,"A graph expansion exceeded its shared native query budget");
    }
    Need(found && route.size()>5,"Deferred bounded graph traversal failed to discover a route around a native blocker");
    bool detour=false;
    for(size_t i=1;i<route.size();++i) {
        const auto& first=route[i-1];const auto& last=route[i];
        Need(!(first.y==400 && last.y==400 && (std::min)(first.x,last.x)==800 && (std::max)(first.x,last.x)==1200),
             "A* expansion or visibility smoothing bypassed a blocked graph edge");
        detour|=last.y!=400;
    }
    Need(detour,"Planner selected the same centerline-blocked route");
    Fixture scheduled;scheduled.blocked=true;
    Need(scheduled.queries.Begin(4,1000,1,5000) && !scheduled.queries.Check(a,b,Read,&scheduled) && scheduled.calls==1,
         "Scheduled body query did not retain its explicit blocked lifetime");
    Need(scheduled.queries.Begin(4,1750,1,5000) && !scheduled.queries.Check(a,b,Read,&scheduled) && scheduled.calls==1 &&
         !scheduled.queries.Check(b,c,Read,&scheduled) && scheduled.calls==2,
         "A fifteen-bot planner turn spent its allowance rechecking the same expired obstacle");
    scheduled.blocked=false;
    Need(scheduled.queries.Begin(4,6001,1,5000) && scheduled.queries.Check(a,b,Read,&scheduled) && scheduled.calls==3,
         "Explicitly retained blocker did not expire");
    Need(scheduled.queries.Begin(4,6001,256,250) && !scheduled.queries.Check(b,c,Read,&scheduled) && scheduled.calls==3,
         "Changing lifetime on the same pump replenished native query allowance");
    Need(!scheduled.queries.Begin(4,6100,1,0) && !scheduled.queries.Begin(4,6100,1,10001),
         "Unbounded or zero obstacle lifetime was accepted");
    Fixture alternatives;found=false;
    for(unsigned pump=1;pump<=80 && !found;++pump) {
        alternatives.queries.Begin(5,pump*750,4,5000);unsigned before=alternatives.calls;
        found=road.Path({10,400,100},{1590,400,100},route,Clear,nullptr,ConnectorTraverse,&alternatives);
        Need(alternatives.calls-before<=4,"Alternative connector search exceeded scheduled native query allowance");
    }
    Need(found && route.front().x>=800 && route.front().y==400,
         "Spaced fifteen-bot planner turns starved before a farther reachable body connector");
    using Observation=R::RoadEdgeObservation;Observations typed;
    typed.queries.Begin(6,1000,4);typed.result=Observation::Blocked;
    Need(typed.queries.Observe(a,b,ReadObservation,&typed,10)==Observation::Blocked && typed.calls==1,
         "Candidate-scoped original blocker was not recorded");
    typed.result=Observation::Clear;
    Need(typed.queries.Observe(a,b,ReadObservation,&typed,20)==Observation::Clear && typed.calls==2 &&
         typed.queries.Observe(a,b,ReadObservation,&typed,10)==Observation::Blocked && typed.calls==2,
         "Another prey identity reused or overwrote a different target's blocked observation");
    typed.queries.Invalidate();typed.result=Observation::Approach;
    Need(typed.queries.Observe(a,b,ReadObservation,&typed,30)==Observation::Approach &&
         typed.queries.Observe(a,b,ReadObservation,&typed,30)==Observation::Approach && typed.calls==4 &&
         typed.queries.Observe(a,b,ReadObservation,&typed,30)==Observation::Invalid && typed.calls==4,
         "Contact approach was cached as clearance or bypassed the strict query allowance");
    typed.queries.Begin(6,1050,2);typed.result=Observation::Invalid;
    Need(typed.queries.Observe(a,b,ReadObservation,&typed,30)==Observation::Invalid &&
         typed.queries.Observe(a,b,ReadObservation,&typed,30)==Observation::Invalid && typed.calls==6,
         "Invalid/expired observation was retained as a negative obstacle");
    typed.queries.Begin(6,1100,2);typed.result=Observation::Clear;
    Need(typed.queries.Observe(a,b,ReadObservation,&typed,30)==Observation::Clear &&
         typed.queries.Observe(a,b,ReadObservation,&typed,30)==Observation::Clear && typed.calls==7,
         "Real geometric clearance stopped using its scoped cache");
    typed.queries.Invalidate();typed.queries.Begin(6,1150,2);typed.result=static_cast<Observation>(99);
    Need(typed.queries.Observe(a,b,ReadObservation,&typed,40)==Observation::Invalid && typed.calls==8,
         "Unknown native observation kind was accepted");
    typed.result=Observation::Blocked;
    Need(typed.queries.Observe(a,b,ReadObservation,&typed,40)==Observation::Blocked && typed.calls==9,
         "Malformed observation poisoned the next fresh query");
    puts("PASS candidate-scoped road observations: target identities isolated, fresh approach distinct from cached clearance, no invalid/unknown-result caching and shared strict allowance. Synthetic callbacks only; no native contact or infection executed.");
    puts("PASS bounded directed road queries: strict native budget, deferred unknown-edge progress, blocker detour, no smoothing bypass, ownership/time/expiry invalidation, callback reentry/reset and C++/SEH recovery. Synthetic graph/native callbacks only; no body-volume or gameplay acceptance.");return 0;
}catch(const std::exception& e){fprintf(stderr,"%s\n",e.what());return 1;}}
