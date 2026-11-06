#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <stdexcept>
#include <limits>
#include "../native/RoadSegmentSupport.h"
namespace R=TwinKartRace;
void Need(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
struct Scene {int kind=0;unsigned rays=0;};
R::RoadSurfaceResult Read(const R::ArenaPoint& start,const R::ArenaPoint& end,R::RoadSurface& out,void* raw) {
    auto& scene=*static_cast<Scene*>(raw);++scene.rays;
    Need(start.x==end.x && start.y==end.y && start.z>end.z,"Support probe changed vertical ray contract");
    float z=scene.kind==1?start.x*0.4f:0.f;
    if(scene.kind==2 && start.x>=175 && start.x<=225)return R::RoadSurfaceResult::Miss;
    if(scene.kind==3 && start.x>=200)z=100;
    if(scene.kind==4)z=-800; // A lower deck must not support a missing upper one.
    out={{start.x,start.y,z},scene.kind==5?-1.f:1.f,scene.kind==6};
    if(scene.kind==7)out.point.z=std::numeric_limits<float>::quiet_NaN();
    return R::RoadSurfaceResult::Hit;
}
void Finish(R::RoadSegmentSupport& probe,Scene& scene) {
    unsigned pumps=0;
    while(probe.State()==R::SupportState::Reading) {
        unsigned before=scene.rays,used=probe.Step(2,Read,&scene);
        Need(used==scene.rays-before && used<=2,"Partly completed support probe exceeded shared ray budget");
        Need(++pumps<=20,"Support validation failed to finish bounded work");
    }
}
int main(){try {
    R::RoadSegmentSupport probe;Scene scene{};
    Need(probe.Begin({0,0,0},{400,0,0}),"Flat edge initialization");
    Need(probe.Step(0,Read,&scene)==0 && scene.rays==0,"Zero budget invoked a native callback");
    Finish(probe,scene);Need(probe.State()==R::SupportState::Supported && scene.rays==9,"Supported flat edge not fully sampled");
    scene={};Need(probe.Begin({0,0,0},{400,0,0},100),"Validated arena spacing rejected");Finish(probe,scene);
    Need(probe.State()==R::SupportState::Supported && scene.rays==5,"Arena spacing did not reduce repeated support work");
    scene={2};Need(probe.Begin({0,0,0},{400,0,0},100),"Arena gap fixture rejected early");Finish(probe,scene);
    Need(probe.State()==R::SupportState::Rejected && !probe.Begin({0,0,0},{400,0,0},101),"Arena profile accepted a gap or unbounded spacing");
    scene={1};Need(probe.Begin({0,0,0},{400,0,160}),"Ramp edge initialization");Finish(probe,scene);
    Need(probe.State()==R::SupportState::Supported,"Gentle physically supported ramp rejected");
    for(int kind=2;kind<=7;++kind) {
        scene={kind};Need(probe.Begin({0,0,0},{400,0,0}),"Negative scene initialization");Finish(probe,scene);
        Need(probe.State()==R::SupportState::Rejected,"Gap/ledge/other-deck/underside/penetration/nonfinite contact accepted as support");
    }
    Need(!probe.Begin({0,0,0},{400,0,500}),"Steep unsupported height transition accepted");
    Need(!probe.Begin({0,0,0},{1600,0,0}),"Unbounded support segment accepted");
    Need(!probe.Begin({0,0,0},{0,0,0}),"Zero-length support segment accepted");
    scene={};probe.Begin({0,0,0},{400,0,0});
    Need(probe.Step(257,Read,&scene)==0 && scene.rays==0 && probe.State()==R::SupportState::Rejected,"Oversized work budget invoked native code");
    probe.Reset();Need(probe.State()==R::SupportState::Idle && probe.SamplesRead()==0,"World reset retained partial edge evidence");
    puts("PASS bounded sampled ground support: flat/ramp progress, shared ray budget, gaps, ledges, lower decks, undersides, penetration, malformed contacts and lifecycle reset. Synthetic centerline only; no native track or vehicle-volume acceptance.");return 0;
}catch(const std::exception& e){fprintf(stderr,"%s\n",e.what());return 1;}}
