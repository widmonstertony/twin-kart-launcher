#pragma once
#include "FlagGroundNavigation.h"
#include "ChaserObjectives.h"
#include <cstdint>
#include <array>
#include <utility>
namespace TwinKartObjective {
using ChaserPoint=TwinKartRace::ArenaPoint;
enum class ChaserRole {Unknown,Runaway,Tagger};
inline ChaserRole NativeChaserRole(bool runaway,bool tagger) {
    return runaway==tagger?ChaserRole::Unknown:runaway?ChaserRole::Runaway:ChaserRole::Tagger;
}
// ready means a connected native pawn with a valid observed position. Whether
// our local controller can write that pawn is a separate caller-owned gate.
struct ChaserObservation {ChaserPoint position{};ChaserRole role=ChaserRole::Unknown;bool ready=false;};
using ChaserLine=bool(*)(const ChaserPoint&,const ChaserPoint&,void*);
// A native navigation adapter can qualify the particular original prey while
// evaluating a candidate. Do not set the driver's selected target prematurely.
// Simple graph callers retain their original same-region Path contract.
template<class Navigation>
auto ChaserPursuitPath(const Navigation& field,const ChaserPoint& first,const ChaserPoint& last,int target,int,
    std::vector<ChaserPoint>& route,ChaserLine line,void* context,int)
    -> decltype(field.PathToTarget(first,last,target,route,line,context)) {
    return field.PathToTarget(first,last,target,route,line,context);
}
template<class Navigation>
bool ChaserPursuitPath(const Navigation& field,const ChaserPoint& first,const ChaserPoint& last,int,int region,
    std::vector<ChaserPoint>& route,ChaserLine line,void* context,long) {
    if(field.RegionAt(last,line,context)!=region){route.clear();return false;}
    return field.Path(first,last,route,line,context);
}
inline float ChaserSafety(const ChaserPoint& p,const ChaserObservation* seats,int count) {
    float nearest=1.e20f;
    for(int i=0;i<count;++i)if(seats[i].ready && seats[i].role==ChaserRole::Tagger)
        nearest=(std::min)(nearest,TwinKartRace::ArenaDistance(p,seats[i].position));
    return nearest;
}
inline float ChaserSegmentSafety(const ChaserPoint& a,const ChaserPoint& b,const ChaserObservation* seats,int count) {
    float nearest=1.e20f,dx=b.x-a.x,dy=b.y-a.y,length=dx*dx+dy*dy;
    for(int i=0;i<count;++i)if(seats[i].ready && seats[i].role==ChaserRole::Tagger) {
        const auto& p=seats[i].position;
        float t=length>1?std::clamp(((p.x-a.x)*dx+(p.y-a.y)*dy)/length,0.f,1.f):0;
        nearest=(std::min)(nearest,std::hypot(a.x+t*dx-p.x,a.y+t*dy-p.y));
    }
    return nearest;
}
// Independently authored objective policy over real sampled roads. Original
// components supply roles; native physics, contacts, infection and results stay
// external. This policy emits only ordinary throttle/steering controls.
class ChaserArenaDriver {
    ChaserRole role=ChaserRole::Unknown;int target=-1;
    std::vector<ChaserPoint> route;std::size_t next=0;
    ChaserPoint goal{};std::uint64_t planned=0;
    ChaserPoint previousPosition{};bool havePosition=false;
    ChaserObjectiveKind objectiveKind=ChaserObjectiveKind::None;std::uint64_t objective=0;
    TwinKartRace::ArenaDriver steering;
    template<class Navigation>
    bool PlanObjective(const Navigation& field,const ChaserObservation* seats,int count,int slot,int region,
        const ChaserObjectiveView* view,ChaserLine line,void* context) {
        if(!ValidChaserObjectives(view,count) || ChaserSafety(seats[slot].position,seats,count)<900)return false;
        bool held=NativeVaultHeld(*view,slot);
        struct Choice {ChaserPoint point{};std::uint64_t identity=0;float score=-1.e30f;};std::array<Choice,4> choices{};
        unsigned candidates=held?view->zoneCount:view->vaultCount;
        for(unsigned i=0;i<candidates;++i) {
            auto kind=held?ChaserObjectiveKind::DeliverVault:ChaserObjectiveKind::RecoverVault;
            auto identity=held?view->zones[i].identity:view->vaults[i].identity;ChaserPoint point{};
            if(!CurrentChaserObjective(view,kind,identity,slot,point) || field.RegionAt(point,line,context)!=region)continue;
            float score=(std::min)(20000.f,ChaserSafety(point,seats,count))-
                0.15f*TwinKartRace::ArenaDistance(seats[slot].position,point);
            for(unsigned at=0;at<choices.size();++at)if(score>choices[at].score) {
                for(unsigned j=static_cast<unsigned>(choices.size())-1;j>at;--j)choices[j]=choices[j-1];
                choices[at]={point,identity,score};break;
            }
        }
        float best=-1.e30f;std::vector<ChaserPoint> candidate;
        for(const auto& choice:choices) {
            if(!choice.identity || !field.Path(seats[slot].position,choice.point,candidate,line,context))continue;
            float distance=0,bottleneck=ChaserSafety(seats[slot].position,seats,count);
            for(std::size_t i=1;i<candidate.size();++i) {
                distance+=TwinKartRace::ArenaDistance(candidate[i-1],candidate[i]);
                bottleneck=(std::min)(bottleneck,ChaserSegmentSafety(candidate[i-1],candidate[i],seats,count));
            }
            if(bottleneck<600)continue;
            float score=3*(std::min)(20000.f,bottleneck)+choice.score-0.20f*distance;
            if(score>best){best=score;route=candidate;goal=choice.point;objective=choice.identity;}
        }
        if(!objective)return false;
        objectiveKind=held?ChaserObjectiveKind::DeliverVault:ChaserObjectiveKind::RecoverVault;return true;
    }
    template<class Navigation>
    bool Plan(const Navigation& field,const ChaserObservation* seats,int count,int slot,
              std::uint64_t now,ChaserLine line,void* context,const ChaserObjectiveView* objectives) {
        route.clear();next=0;target=-1;planned=now;objective=0;objectiveKind=ChaserObjectiveKind::None;
        const auto& position=seats[slot].position;
        int region=field.RegionAt(position,line,context);if(region<0)return false;
        std::vector<ChaserPoint> candidate;
        if(role==ChaserRole::Tagger) {
            float best=1.e30f;
            for(int other=0;other<count;++other) {
                const auto& prey=seats[other];
                if(!prey.ready || prey.role!=ChaserRole::Runaway ||
                   !ChaserPursuitPath(field,position,prey.position,other,region,candidate,line,context,0))continue;
                float distance=0;
                for(std::size_t i=1;i<candidate.size();++i)distance+=TwinKartRace::ArenaDistance(candidate[i-1],candidate[i]);
                if(distance<best){best=distance;route=candidate;target=other;goal=prey.position;}
            }
            return target>=0 && !route.empty();
        }
        if(PlanObjective(field,seats,count,slot,region,objectives,line,context))return true;
        if(ChaserSafety(position,seats,count)>=1.e19f)return false;
        struct Choice {ChaserPoint point{};float score=-1.e30f;};std::array<Choice,8> choices{};
        std::size_t samples=(std::min)(std::size_t{128},field.Count());
        for(std::size_t i=0;i<samples;++i) {
            std::size_t index=(i*field.Count()/samples+static_cast<unsigned>(slot)*31u)%field.Count();ChaserPoint p{};
            if(!field.ReachableSampleAt(index,region,p,line,context))continue;
            float distance=TwinKartRace::ArenaDistance(position,p);if(distance<800 || distance>16000)continue;
            float score=ChaserSafety(p,seats,count)-0.08f*distance;
            for(std::size_t at=0;at<choices.size();++at)if(score>choices[at].score) {
                for(std::size_t j=choices.size()-1;j>at;--j)choices[j]=choices[j-1];
                choices[at]={p,score};break;
            }
        }
        float best=-1.e30f;
        for(const auto& choice:choices) {
            if(choice.score<=-1.e29f || !field.Path(position,choice.point,candidate,line,context))continue;
            float bottleneck=ChaserSafety(position,seats,count),distance=0;
            for(std::size_t i=1;i<candidate.size();++i) {
                bottleneck=(std::min)(bottleneck,ChaserSegmentSafety(candidate[i-1],candidate[i],seats,count));
                distance+=TwinKartRace::ArenaDistance(candidate[i-1],candidate[i]);
            }
            float score=bottleneck+0.8f*ChaserSafety(choice.point,seats,count)-0.08f*distance;
            if(score>best){best=score;route=candidate;goal=choice.point;}
        }
        return !route.empty();
    }
public:
    void Reset(){role=ChaserRole::Unknown;target=-1;route.clear();next=0;goal={};planned=0;previousPosition={};havePosition=false;
        objective=0;objectiveKind=ChaserObjectiveKind::None;steering.Reset();}
    int TargetSlot()const{return target;}
    ChaserRole Role()const{return role;}
    std::size_t RouteSize()const{return route.size();}
    ChaserPoint Goal()const{return goal;}
    ChaserObjectiveKind ObjectiveKind()const{return objectiveKind;}
    std::uint64_t ObjectiveIdentity()const{return objective;}
    template<class Navigation>
    TwinKartRace::ArenaControl Step(const Navigation& field,const ChaserObservation* seats,int count,
        int slot,float heading,float dt,std::uint64_t now,bool active,bool allowPlan,ChaserLine line,void* context,
        const ChaserObjectiveView* objectives=nullptr,ChaserLine immediate=nullptr,void* immediateContext=nullptr,
        ChaserLine reverseClear=nullptr,void* reverseContext=nullptr) {
        if(!active || !seats || count<2 || count>16 || slot<1 || slot>=count || !field.Ready() ||
           !std::isfinite(heading) || !std::isfinite(dt) || dt<=0 || dt>1 || !seats[slot].ready ||
           !TwinKartRace::Finite(seats[slot].position) || seats[slot].role==ChaserRole::Unknown) {Reset();return {};}
        for(int i=0;i<count;++i)if(seats[i].ready && (!TwinKartRace::Finite(seats[i].position) ||
            std::fabs(seats[i].position.x)>1.e7f || std::fabs(seats[i].position.y)>1.e7f || std::fabs(seats[i].position.z)>1.e7f ||
            (seats[i].role!=ChaserRole::Unknown && seats[i].role!=ChaserRole::Runaway && seats[i].role!=ChaserRole::Tagger))) {Reset();return {};}
        if(role!=seats[slot].role){Reset();role=seats[slot].role;}
        if(havePosition && (TwinKartRace::ArenaDistance(previousPosition,seats[slot].position)>2500 ||
            std::fabs(previousPosition.z-seats[slot].position.z)>500)) {
            route.clear();target=-1;next=0;planned=0;objective=0;objectiveKind=ChaserObjectiveKind::None;steering.Reset();
        }
        previousPosition=seats[slot].position;havePosition=true;
        if(objectiveKind!=ChaserObjectiveKind::None) {
            ChaserPoint current{};
            if(!ValidChaserObjectives(objectives,count) ||
               !CurrentChaserObjective(objectives,objectiveKind,objective,slot,current) ||
               TwinKartRace::ArenaDistance(current,goal)>250 || std::fabs(current.z-goal.z)>100 ||
               (objectiveKind==ChaserObjectiveKind::RecoverVault && NativeVaultHeld(*objectives,slot)) ||
               ChaserSafety(seats[slot].position,seats,count)<900) {
                route.clear();next=0;objective=0;objectiveKind=ChaserObjectiveKind::None;steering.Reset();
            }
        }
        if(role==ChaserRole::Tagger && (target<0 || target>=count || !seats[target].ready || seats[target].role!=ChaserRole::Runaway)) {
            route.clear();target=-1;steering.Reset();
        }
        bool due=route.empty() || now<planned || now-planned>=750 || TwinKartRace::ArenaDistance(seats[slot].position,goal)<400;
        if(allowPlan && due) {
            // A temporarily blocked or budget-deferred replan must not erase
            // the controller's observed stall/recovery history. Keep the old
            // intent transactionally; every output still needs a fresh gate.
            auto oldRoute=std::move(route);auto oldNext=next;auto oldGoal=goal;auto oldTarget=target;
            auto oldObjective=objective;auto oldKind=objectiveKind;
            if(!Plan(field,seats,count,slot,now,line,context,objectives)) {
                route=std::move(oldRoute);next=oldNext;goal=oldGoal;target=oldTarget;
                objective=oldObjective;objectiveKind=oldKind;
                if(route.empty()){steering.Reset();return {};}
            }
        }
        if(route.empty())return {};
        const auto& position=seats[slot].position;
        while(next<route.size() && TwinKartRace::ArenaDistance(position,route[next])<250 && std::fabs(position.z-route[next].z)<200)++next;
        ChaserPoint aim=next<route.size()?route[next]:goal;
        if(role==ChaserRole::Tagger && target>=0 && next+1>=route.size() &&
           TwinKartRace::ArenaDistance(goal,seats[target].position)<250 &&
           (immediate || (line && line(position,seats[target].position,context))))aim=seats[target].position;
        // The stronger immediate adapter qualifies the selected target and
        // actual body below. An earlier unscoped ray must not veto its target
        // contact or make the driver keep aiming at an outdated prey position.
        // Revalidate the immediate segment on every control sample. An actor,
        // gate or native objective can block an originally valid route later.
        auto control=steering.Step(position,heading,aim,dt,slot);
        if(control.reverse) {
            // The production body query currently proves fixed-attitude
            // translations only. Back straight along the actual heading and
            // qualify that rear segment, never the forward route/prey aim.
            // A forward target-contact permission must not authorize reversing.
            control.steer=0;
            ChaserPoint rear{position.x-250.f*std::cos(heading),position.y-250.f*std::sin(heading),position.z};
            auto check=reverseClear?reverseClear:immediate?nullptr:line;
            auto checkContext=reverseClear?reverseContext:context;
            if(!check || !check(position,rear,checkContext))return {};
        } else {
            auto check=immediate?immediate:line;auto checkContext=immediate?immediateContext:context;
            if(!check || !check(position,aim,checkContext))return {};
        }
        return control;
    }
};
}
