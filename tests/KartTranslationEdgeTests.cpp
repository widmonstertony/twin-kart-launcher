#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <stdexcept>
#include "../native/KartTranslationEdge.h"
namespace R=TwinKartRace;
void Need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Fixture {
    unsigned reads=0,casts=0;bool live=true,blocked=false,expireRead=false,expireBody=false,hole=false,bank=false,bend=false,penetrating=false;
    R::ArenaPoint first{},last{};
};
bool Alive(void* raw){return static_cast<Fixture*>(raw)->live;}
R::RoadSurfaceResult Read(const R::ArenaPoint& first,const R::ArenaPoint& last,R::RoadSurface& out,void* raw) {
    auto& f=*static_cast<Fixture*>(raw);++f.reads;
    Need(first.x==last.x && first.y==last.y && first.z-last.z==144,"Original support ray extent changed");
    if(f.expireRead)f.live=false;
    if(f.hole && first.x==600)return R::RoadSurfaceResult::Miss;
    out={{first.x,first.y,f.bend && first.x==600?1.f:0.f},f.bank?0.8f:1.f,f.penetrating,
        f.bank?R::ArenaPoint{0,0.6f,0.8f}:R::ArenaPoint{0,0,1}};
    return R::RoadSurfaceResult::Hit;
}
R::VolumeSweep Body(const R::ArenaPoint& first,const R::ArenaPoint& last,const R::ComponentRotation& rotation,void* raw) {
    auto& f=*static_cast<Fixture*>(raw);++f.casts;f.first=first;f.last=last;
    Need(R::KartSameAttitude(rotation,{}),"Body query changed original orientation");
    if(f.expireBody)f.live=false;
    return {f.blocked?R::VolumeSweepResult::Blocked:R::VolumeSweepResult::Clear,f.blocked?0.5f:1.f};
}
R::KartRoadCalibration Calibration() {
    R::KartContactSnapshot contacts;
    int supports[4]{};
    contacts.wheels[0]={{-25,-20,0},supports};contacts.wheels[1]={{25,-20,0},supports+1};
    contacts.wheels[2]={{25,20,0},supports+2};contacts.wheels[3]={{-25,20,0},supports+3};
    R::KartRoadCalibration calibration;
    Need(R::CalibrateKartRoad(contacts,{0,0,20},{},calibration),"Actual wheel/body calibration failed");return calibration;
}
R::VolumeSweep Query(Fixture& f,const R::KartRoadCalibration& calibration) {
    return R::QueryKartTranslationEdge(calibration,{}, {400,0,0},{800,0,0},Read,Body,Alive,&f);
}
int main(){try {
    auto calibration=Calibration();Fixture f;
    auto result=Query(f,calibration);
    Need(result.result==R::VolumeSweepResult::Clear && f.reads==9 && f.casts==1 && f.first.x==400 &&
         f.last.x==800 && f.first.z==20 && f.last.z==20,"Graph edge did not query actual calibrated body translation");
    f={};f.blocked=true;result=Query(f,calibration);
    Need(result.result==R::VolumeSweepResult::Blocked && result.time==0.5f && f.casts==1,
         "Body-width obstacle was accepted because sampled centerline support was clear");
    f={};f.hole=true;Need(Query(f,calibration).result==R::VolumeSweepResult::Invalid && f.reads==5 && !f.casts,
         "Middle-of-edge missing support reached a body-clear result");
    f={};f.bank=true;Need(Query(f,calibration).result==R::VolumeSweepResult::Invalid && !f.casts,
         "A fixed-attitude sweep was mislabeled as an angular bank transition");
    f={};f.bend=true;Need(Query(f,calibration).result==R::VolumeSweepResult::Invalid && f.reads==9 && !f.casts,
         "A sampled body bend was silently replaced with a straight sweep");
    f={};f.penetrating=true;Need(Query(f,calibration).result==R::VolumeSweepResult::Invalid && !f.casts,
         "Penetrating support was accepted");
    f={};f.expireRead=true;Need(Query(f,calibration).result==R::VolumeSweepResult::Invalid && f.reads==1 && !f.casts,
         "Expired native support lease reached the body query");
    f={};f.expireBody=true;Need(Query(f,calibration).result==R::VolumeSweepResult::Invalid && f.casts==1,
         "Body callback expiry published stale clearance");
    f={};result=R::QueryKartTranslationEdge(calibration,{}, {0,0,0},{1500,0,0},Read,Body,Alive,&f);
    Need(result.result==R::VolumeSweepResult::Clear && f.reads==31 && f.casts==1,"Longest supported edge exceeded its fixed query bound");
    f={};Need(R::QueryKartTranslationEdge(calibration,{}, {0,0,0},{1501,0,0},Read,Body,Alive,&f).result==R::VolumeSweepResult::Invalid &&
         !f.reads && !f.casts,"Oversized future edge issued native queries");
    f={};Need(R::QueryKartTranslationEdge(calibration,{}, {0,0,0},{0,0,0},Read,Body,Alive,&f).result==R::VolumeSweepResult::Invalid &&
         !f.reads && !f.casts,"No-op edge was fabricated as native sweep clearance");
    puts("PASS fixed-attitude body-qualified road edges: observed wheel/body mapping, body-width blocker, sampled missing support, bank/bend rejection, lease expiry and strict 31-ray/one-body bound. Synthetic callbacks only; no angular, airborne, native driving or performance acceptance.");return 0;
}catch(const std::exception& e){fprintf(stderr,"%s\n",e.what());return 1;}}
