#include <cstdio>
#include <stdexcept>
#include "../native/HideRepairedScene.h"
using namespace TwinKartObjective;
struct Fixture {unsigned queries=0,applied=0,rollbacks=0;bool hole=false,cliff=false,invalid=false,slope=false;unsigned failAt=0;};
bool Ground(const TwinKartRace::ArenaPoint& sample,TwinKartRace::ArenaPoint& contact,void* raw) {
    auto& f=*static_cast<Fixture*>(raw);++f.queries;
    if(f.invalid || (f.hole && std::fabs(sample.x)<10 && std::fabs(sample.y)<10))return false;
    contact={sample.x,sample.y,f.slope?sample.x*.08f:f.cliff && sample.x>0?500.f:0.f};return true;
}
bool Apply(const HideRepairPlacement&,void* raw) {auto& f=*static_cast<Fixture*>(raw);return ++f.applied!=f.failAt;}
void Rollback(void* raw){++static_cast<Fixture*>(raw)->rollbacks;}
void Need(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
int main() {
    try {
        Need(HideRepairStartRole(L"HiderStart")==1 && HideRepairStartRole(L"HiderStart_7")==1 &&
            HideRepairStartRole(L"SEEKERSTART_11")==2 && !HideRepairStartRole(L"TwinKart_Replaced_Test_Start"),"Numbered native starts remain registered");
        Fixture f;HideRepairPlan plan;
        Need(BuildHideRepairPlan({-6000,-6000,-100},{6000,6000,100},Ground,&f,plan),"Supported floor rejected");
        Need(plan.count==HideRepairActorCount && f.queries<=33*33+HideRepairActorCount,"Unbounded scene/query count");
        Need(plan.high.x-plan.low.x==10800 && plan.high.y-plan.low.y==9000,"Supported floor still uses the small room");
        TwinKartRace::ArenaPoint navLow{-20000,-20000,-3000},navHigh{20000,20000,2500};
        Need(HideRepairNavigationBounds(plan,navLow,navHigh) && navLow.x==plan.low.x+625 &&
            navHigh.x==plan.high.x-625 && navLow.y==plan.low.y+625 && navHigh.y==plan.high.y-625 &&
            navLow.z==-3000 && navHigh.z==2500,"Navigation used source-floor bounds instead of authored walls");
        Need((navHigh.x-navLow.x)==9550 && (navHigh.y-navLow.y)==7750,"Patrol graph lost clearance at the repaired perimeter");
        using P=HidePerimeterPosition;
        Need(ClassifyHidePerimeter(plan,{0,0,55})==P::SafeInterior &&
            ClassifyHidePerimeter(plan,{plan.low.x+160,0,55})==P::NearWall &&
            ClassifyHidePerimeter(plan,{plan.low.x-1,0,55})==P::Outside &&
            ClassifyHidePerimeter(plan,{0,plan.high.y+1,55})==P::Outside &&
            ClassifyHidePerimeter(plan,{plan.low.x-1,0,plan.low.z-181})==P::Invalid,
            "Perimeter recovery misclassifies interior, outer face or invalid height");
        const TwinKartRace::ArenaControl reverse{-.8f,1.f,true},forward{1.f,.4f,false};
        const TwinKartRace::ArenaPoint edge{plan.low.x+550,0,55};
        auto turnaround=BoundHideSeekerSteering(plan,edge,0,reverse);
        Need(!turnaround.reverse && turnaround.throttle>0 && turnaround.steer==0,
            "Reverse toward the wall must back off the turn and crawl inward");
        Need(BoundHideSeekerSteering(plan,edge,3.14159265f,reverse).reverse &&
            BoundHideSeekerSteering(plan,{0,0,55},0,reverse).reverse,
            "Inward or interior reverse should keep the native control");
        Need(BoundHideSeekerSteering(plan,edge,3.14159265f,forward).throttle<0 &&
            BoundHideSeekerSteering(plan,edge,0,forward).throttle==forward.throttle,
            "Outward forward motion must brake without slowing inward motion");
        TwinKartRace::ArenaPoint limited{};
        Need(LimitHidePerimeterVelocity(plan,edge,{-1500,250,45},limited) && limited.x<0 &&
            limited.x>-400 && limited.y==250 && limited.z==45,
            "Drift toward a wall must lose only its outward velocity");
        Need(LimitHidePerimeterVelocity(plan,{plan.low.x+450,plan.high.y-449,55},{-300,280,30},limited) &&
            limited.x==0 && limited.y==0 && limited.z==30,"Corner must brake both outward axes");
        Need(!LimitHidePerimeterVelocity(plan,edge,{1500,250,45},limited) &&
            !LimitHidePerimeterVelocity(plan,{0,0,55},{1500,250,45},limited),"Inward and open-floor movement must remain untouched");
        TwinKartRace::ArenaPoint inward{};
        Need(HideSeekerInwardGoal(plan,edge,inward) && inward.x==plan.low.x+1200.f && inward.y==0 &&
            !HideSeekerInwardGoal(plan,{0,0,55},inward),"Wall contact must reconnect with the sampled patrol grid");
        auto rear=SteerHideSeekerInward(edge,3.14159265f,inward);
        auto nose=SteerHideSeekerInward(edge,0,inward);
        Need(rear.throttle<0 && rear.throttle>-1 && !rear.reverse && std::fabs(rear.steer)<.01f &&
            nose.throttle>0 && std::fabs(nose.steer)<.01f,"Inward recovery should drive the nearest direction at low speed");
        auto badPlan=plan;badPlan.count=0;auto saved=navLow;
        Need(!HideRepairNavigationBounds(badPlan,navLow,navHigh) && navLow.x==saved.x,"Incomplete layout changed navigation bounds");
        unsigned starts=0,walls=0,props=0,lights=0,sky=0;
        for(unsigned i=0;i<plan.count;++i) {
            auto& p=plan.actors[i];
            if(p.kind==HideRepairActor::Start) {
                ++starts;Need(p.slot<16 && p.transform.translation[2]==160,"Bad original role start");
                Need(std::fabs(p.transform.translation[0])>3800 && std::fabs(p.transform.translation[0])<5100,"Start outside launch bay");
                const bool forwardX=p.transform.rotation[3]==1;
                Need(forwardX==(p.transform.translation[0]<0),"Start faces away from the arena");
                for(unsigned divider=24;divider<30;++divider) {
                    const auto& wall=plan.actors[divider].transform;
                    Need(std::fabs(p.transform.translation[1]-wall.translation[1])-50*wall.scale[1]>=180,
                        "Straight launch lane crosses a divider");
                }
            } else if(p.kind==HideRepairActor::Wall) {
                ++walls;
                if(p.slot<8) {
                    unsigned cross=p.slot%2?0:1;
                    const float outerFace=std::fabs(p.transform.translation[cross])+50*p.transform.scale[cross];
                    Need(outerFace==(cross?plan.high.y:plan.high.x) &&
                         100*p.transform.scale[cross]==HidePerimeterThickness,
                         "Reinforced perimeter must keep its original outer edge");
                }
            }
            else if(p.kind==HideRepairActor::Crate || p.kind==HideRepairActor::Tree) {
                ++props;
                for(unsigned start=0;start<16;++start)
                    Need(std::fabs(p.transform.translation[0]-plan.actors[start].transform.translation[0])>=1080,
                        "Large scenery can crowd the full-size launch bay");
            }
            else if(p.kind==HideRepairActor::Sky)++sky;else ++lights;
        }
        Need(starts==16 && walls==14 && props==18 && lights==2 && sky==1,"Incomplete scene contract");
        for(unsigned base:{24u,27u})for(unsigned i=base;i<base+2;++i) {
            const auto& a=plan.actors[i].transform;const auto& b=plan.actors[i+1].transform;
            Need(b.translation[1]-a.translation[1]-50*(a.scale[1]+b.scale[1])>=1200,"A partition has no wide passage");
        }
        Need(ApplyHideRepairPlan(plan,Apply,Rollback,&f) && f.applied==HideRepairActorCount && !f.rollbacks,"Complete scene failed");
        for(unsigned fail=1;fail<=HideRepairActorCount;++fail) {
            Fixture partial;partial.failAt=fail;
            Need(!ApplyHideRepairPlan(plan,Apply,Rollback,&partial) && partial.applied==fail && partial.rollbacks==1,"Partial scene not rolled back exactly once");
        }
        for(unsigned scenario=0;scenario<3;++scenario) {
            Fixture reject;reject.hole=scenario==0;reject.cliff=scenario==1;reject.invalid=scenario==2;
            HideRepairPlan sentinel;sentinel.count=7;
            Need(!BuildHideRepairPlan({-6000,-6000,-100},{6000,6000,100},Ground,&reject,sentinel) && sentinel.count==7,"Hole/cliff/invalid road published a partial plan");
            Need(!reject.applied && reject.queries<=9*5*33*33,"Rejected geometry caused mutation or exceeded budget");
        }
        Fixture graded;graded.slope=true;HideRepairPlan slope;
        Need(BuildHideRepairPlan({-6000,-6000,-100},{6000,6000,100},Ground,&graded,slope),"Gentle continuous terrain rejected");
        Need(slope.actors[16].transform.scale[2]>4.5f,"Graded road leaves wall gaps");
        Fixture tiny;HideRepairPlan rejected;
        Need(!BuildHideRepairPlan({-1000,-1000,0},{1000,1000,0},Ground,&tiny,rejected) && !tiny.queries,"Tiny ground admitted");
        auto corrupt=plan;corrupt.actors.back().transform.rotation[3]=0;Fixture untouched;
        Need(!ApplyHideRepairPlan(corrupt,Apply,Rollback,&untouched) && !untouched.applied && !untouched.rollbacks,"Invalid quaternion mutated scene");
        std::puts("PASS repaired Hide supported layout, native role starts, bounded queries and obstacle layout");
        std::puts("PASS all partial-spawn boundaries roll back exactly once; invalid plans mutate nothing");
        std::puts("PASS unsupported hole, cliff, invalid trace and undersized ground preserve output");
        return 0;
    } catch(const std::exception& error){std::fprintf(stderr,"FAIL %s\n",error.what());return 1;}
}
