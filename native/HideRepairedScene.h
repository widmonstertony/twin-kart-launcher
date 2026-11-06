#pragma once
#include "NativeObjectiveArena.h"
#include <array>
namespace TwinKartObjective {
// Project-authored layout for the retained HideNSeekTestLevel. This is a
// repaired test arena, not a recovered production map. All coordinates below
// are relative to the actual loaded floor bounds, never captured vendor data.
inline constexpr const wchar_t* HideRepairedLabel=L"\u8eb2\u732b\u732b\uff1a\u4fee\u590d\u7248\u6d4b\u8bd5\u573a\u5730";
// Native start discovery searches the tag text for Hider/Seeker, including
// numbered FNames. Exact FName equality misses original HiderStart_N tags.
inline int HideRepairStartRole(const wchar_t* text) {
    if(!text || wcsnlen_s(text,128)>=128)return 0;
    const wchar_t* words[]{L"hider",L"seeker"};
    for(int role=0;role<2;++role)for(size_t at=0;text[at];++at) {
        size_t i=0;
        while(words[role][i] && text[at+i]) {
            wchar_t value=text[at+i];if(value>=L'A' && value<=L'Z')value+=L'a'-L'A';
            if(value!=words[role][i])break;++i;
        }
        if(!words[role][i])return role+1;
    }
    return 0;
}
enum class HideRepairActor {Start,Wall,Crate,Tree,Sky,Sun,Ambient};
struct HideRepairPlacement {ObjectiveTransform transform{};HideRepairActor kind{};unsigned slot=0;unsigned padding[2]{};};
inline constexpr unsigned HideRepairActorCount=16+8+6+18+3;
inline constexpr float HidePerimeterThickness=380.f;
struct HideRepairPlan {
    std::array<HideRepairPlacement,HideRepairActorCount> actors{};
    unsigned count=0;
    TwinKartRace::ArenaPoint low{},high{};
};
inline bool HideRepairNavigationBounds(const HideRepairPlan& plan,TwinKartRace::ArenaPoint& low,TwinKartRace::ArenaPoint& high) {
    using namespace TwinKartRace;
    constexpr float clearance=625.f;
    if(plan.count!=HideRepairActorCount || !Finite(plan.low) || !Finite(plan.high) ||
       plan.high.x-plan.low.x<=2*clearance || plan.high.y-plan.low.y<=2*clearance)return false;
    // Preserve the caller's vertical road-query interval. Only the authored
    // physical arena, not the much larger source-floor asset, bounds patrols.
    low.x=plan.low.x+clearance;low.y=plan.low.y+clearance;
    high.x=plan.high.x-clearance;high.y=plan.high.y-clearance;return true;
}
enum class HidePerimeterPosition { Invalid, SafeInterior, NearWall, Outside };
inline HidePerimeterPosition ClassifyHidePerimeter(const HideRepairPlan& plan,const TwinKartRace::ArenaPoint& position) {
    using namespace TwinKartRace;
    if(!Finite(position) || plan.count!=HideRepairActorCount || !Finite(plan.low) || !Finite(plan.high) ||
       plan.high.x-plan.low.x<=800.f || plan.high.y-plan.low.y<=800.f ||
       position.z<plan.low.z-180.f || position.z>plan.high.z+180.f)return HidePerimeterPosition::Invalid;
    // The cubes' outer face is on the authored rectangle. Crossing it
    // means the kart has escaped, rather than just contacted a wall.
    if(position.x<plan.low.x || position.x>plan.high.x || position.y<plan.low.y || position.y>plan.high.y)
        return HidePerimeterPosition::Outside;
    // Keep at least a kart length of margin from the inner face for recovery.
    constexpr float margin=HidePerimeterThickness+250.f;
    if(position.x>=plan.low.x+margin && position.x<=plan.high.x-margin &&
       position.y>=plan.low.y+margin && position.y<=plan.high.y-margin &&
       position.z>=plan.low.z-30.f && position.z<=plan.high.z-260.f)return HidePerimeterPosition::SafeInterior;
    return HidePerimeterPosition::NearWall;
}
// Brake the rigid body's outward velocity, including sideways drift.
// Throttle-only braking cannot contain a kart already sliding in PhysX.
inline bool LimitHidePerimeterVelocity(const HideRepairPlan& plan,const TwinKartRace::ArenaPoint& position,
    const TwinKartRace::ArenaPoint& velocity,TwinKartRace::ArenaPoint& output) {
    using namespace TwinKartRace;
    if(ClassifyHidePerimeter(plan,position)==HidePerimeterPosition::Invalid || !Finite(velocity))return false;
    constexpr float inset=HidePerimeterThickness+70.f,anticipation=.30f;
    auto axis=[&](float p,float v,float low,float high) {
        if(v<0.f)return (std::max)(v,-(std::max)(0.f,p-low-inset)/anticipation);
        if(v>0.f)return (std::min)(v,(std::max)(0.f,high-p-inset)/anticipation);
        return v;
    };
    const float x=axis(position.x,velocity.x,plan.low.x,plan.high.x);
    const float y=axis(position.y,velocity.y,plan.low.y,plan.high.y);
    if(x==velocity.x && y==velocity.y)return false;
    output={x,y,velocity.z};
    return true;
}
// The sampled patrol grid begins 625 cm inside the outer faces. A kart
// stopped by the wall can therefore be safe but have no path anchor.
inline bool HideSeekerInwardGoal(const HideRepairPlan& plan,const TwinKartRace::ArenaPoint& position,
    TwinKartRace::ArenaPoint& goal) {
    using namespace TwinKartRace;
    const auto edge=ClassifyHidePerimeter(plan,position);
    if(edge==HidePerimeterPosition::Invalid || edge==HidePerimeterPosition::Outside ||
       plan.high.x-plan.low.x<=2400.f || plan.high.y-plan.low.y<=2400.f)return false;
    const float clearance=(std::min)({position.x-plan.low.x,plan.high.x-position.x,
        position.y-plan.low.y,plan.high.y-position.y});
    if(clearance>=850.f)return false;
    constexpr float destinationInset=1200.f;
    goal={std::clamp(position.x,plan.low.x+destinationInset,plan.high.x-destinationInset),
        std::clamp(position.y,plan.low.y+destinationInset,plan.high.y-destinationInset),position.z};
    return true;
}
inline TwinKartRace::ArenaControl SteerHideSeekerInward(const TwinKartRace::ArenaPoint& position,float heading,
    const TwinKartRace::ArenaPoint& goal) {
    using namespace TwinKartRace;
    if(!Finite(position) || !Finite(goal) || !std::isfinite(heading) || ArenaDistance(position,goal)<80.f)return {};
    float angle=std::atan2(goal.y-position.y,goal.x-position.x)-heading;
    angle=std::atan2(std::sin(angle),std::cos(angle));
    if(std::fabs(angle)>1.75f) {
        const float rear=angle>0?angle-3.14159265f:angle+3.14159265f;
        // Keep a proportional negative value. Exactly -1 is the native
        // reverse-key sentinel and can negate a previous value of zero.
        return {-.48f,std::clamp(-rear*1.3f,-.75f,.75f),false};
    }
    return {.38f,std::clamp(angle*1.3f,-.75f,.75f),false};
}
// The generic kart driver may reverse to turn toward an in-bounds waypoint.
// Near a wall the reverse arc can leave the arena before the turn finishes.
// Keep this check local to the repaired Hide arena and never reposition here.
inline TwinKartRace::ArenaControl BoundHideSeekerSteering(const HideRepairPlan& plan,
    const TwinKartRace::ArenaPoint& position,float heading,TwinKartRace::ArenaControl input) {
    using namespace TwinKartRace;
    const auto state=ClassifyHidePerimeter(plan,position);
    if(state==HidePerimeterPosition::Invalid || !std::isfinite(heading))return {};
    if(!input.reverse && input.throttle<=0.f)return input;
    auto clearance=[&](float x,float y) {
        return (std::min)({x-plan.low.x,plan.high.x-x,y-plan.low.y,plan.high.y-y});
    };
    constexpr float lookahead=800.f,margin=HidePerimeterThickness+240.f;
    const float dx=lookahead*std::cos(heading),dy=lookahead*std::sin(heading);
    const float current=clearance(position.x,position.y);
    const float ahead=clearance(position.x+dx,position.y+dy),behind=clearance(position.x-dx,position.y-dy);
    const float projected=input.reverse?behind:ahead;
    if(projected>=margin || projected>=current)return input;
    if(input.reverse && ahead>current+100.f)return {.28f,0.f,false};
    // Brake rather than driving through the wall. A subsequent valid
    // reverse or interior steering action still comes from the planner.
    return {input.reverse?0.f:-.65f,input.steer,false};
}
inline bool BuildHideRepairPlan(const TwinKartRace::ArenaPoint& low,const TwinKartRace::ArenaPoint& high,
    ArenaGround ground,void* context,HideRepairPlan& output) {
    using namespace TwinKartRace;
    if(!Finite(low) || !Finite(high) || !ground || low.x>=high.x || low.y>=high.y || low.z>high.z ||
       std::fabs(low.x)>1.e7f || std::fabs(low.y)>1.e7f || std::fabs(high.x)>1.e7f || std::fabs(high.y)>1.e7f ||
       high.x-low.x>120000 || high.y-low.y>120000 || high.z-low.z>8000)return false;
    ArenaPoint center{(low.x+high.x)*.5f,(low.y+high.y)*.5f,(low.z+high.z)*.5f};
    const auto origin=center;
    const float initialX=(std::min)(5400.f,(high.x-low.x)*.5f-220.f);
    const float initialY=(std::min)(4500.f,(high.y-low.y)*.5f-220.f);
    float hx=initialX,hy=initialY;
    bool supported=false;float floorLow=0,floorHigh=0;
    const int candidates[9][2]{{0,0},{-1,0},{1,0},{0,-1},{0,1},{-1,-1},{1,-1},{-1,1},{1,1}};
    for(auto& candidate:candidates) {
      if(supported)break;
      center={origin.x+candidate[0]*(std::min)(initialX*3,8400.f),origin.y+candidate[1]*(std::min)(initialY*3,7200.f),origin.z};
      hx=initialX;hy=initialY;
      for(unsigned attempt=0;attempt<5 && hx>=1800 && hy>=1500;++attempt) {
        if(center.x-hx<low.x+200 || center.x+hx>high.x-200 || center.y-hy<low.y+200 || center.y+hy>high.y-200)break;
        bool valid=true;float minimum=1.e20f,maximum=-1.e20f;
        const int xSteps=2*(std::max)(8,static_cast<int>(std::ceil(hx/350)));
        const int ySteps=2*(std::max)(8,static_cast<int>(std::ceil(hy/350)));
        float previousRow[33]{};
        // A bounding box is not a drivable rectangle. Reject holes, cliffs and
        // unsupported connectors BEFORE any native object/material mutation.
        for(int y=0;y<=ySteps && valid;++y)for(int x=0;x<=xSteps && valid;++x) {
            ArenaPoint sample{center.x+hx*(2.f*x/xSteps-1),center.y+hy*(2.f*y/ySteps-1),center.z},contact{};
            valid=ground(sample,contact,context) && Finite(contact) && ArenaDistance(sample,contact)<=5 &&
                std::fabs(contact.z-center.z)<=4000;
            if(valid && x)valid=std::fabs(contact.z-previousRow[x-1])<=2*hx/xSteps*.25f+8;
            if(valid && y)valid=std::fabs(contact.z-previousRow[x])<=2*hy/ySteps*.25f+8;
            previousRow[x]=contact.z;
            if(valid){minimum=(std::min)(minimum,contact.z);maximum=(std::max)(maximum,contact.z);}
        }
        if(valid && maximum-minimum<=1200){supported=true;floorLow=minimum;floorHigh=maximum;break;}
        hx*=.8f;hy*=.8f;
      }
    }
    if(!supported)return false;
    HideRepairPlan plan;plan.low={center.x-hx,center.y-hy,floorLow};plan.high={center.x+hx,center.y+hy,floorHigh+450};
    auto append=[&](HideRepairActor kind,unsigned slot,float x,float y,float lift,float sx=1,float sy=1,float sz=1) {
        if(plan.count>=plan.actors.size())return false;
        ArenaPoint sample{center.x+x,center.y+y,center.z},contact{};
        if(!ground(sample,contact,context) || !Finite(contact) || ArenaDistance(sample,contact)>5 ||
           contact.z<floorLow-5 || contact.z>floorHigh+5)return false;
        auto& item=plan.actors[plan.count++];item.kind=kind;item.slot=slot;
        item.transform.translation[0]=contact.x;item.transform.translation[1]=contact.y;item.transform.translation[2]=contact.z+lift;
        item.transform.scale[0]=sx;item.transform.scale[1]=sy;item.transform.scale[2]=sz;
        return true;
    };
    // Keep both role groups in opposing launch areas, aligned with the two
    // passages common to both staggered dividers. A straight start along X
    // must not drive any of the four spawn columns into the middle wall.
    constexpr float launchLanes[]{-.37f,-.30f,.30f,.37f};
    for(unsigned role=0;role<2;++role)for(unsigned seat=0;seat<8;++seat) {
        float sign=role?-1.f:1.f;
        if(!append(HideRepairActor::Start,role*8+seat,-sign*hx*.83f+sign*(seat/4)*hx*.10f,
            launchLanes[seat%4]*hy,160))return false;
        if(role){plan.actors[plan.count-1].transform.rotation[2]=1;plan.actors[plan.count-1].transform.rotation[3]=0;}
    }
    // Split the perimeter into overlapping pieces so every engine Cube stays
    // under the safe scale cap. Terrain height is included in every wall.
    const float wallHeight=(450+2*(floorHigh-floorLow))/100;
    constexpr float halfThickness=HidePerimeterThickness*.5f,thicknessScale=HidePerimeterThickness/100.f;
    unsigned wall=0;
    for(float side:{-1.f,1.f})for(float half:{-1.f,1.f}) {
        if(!append(HideRepairActor::Wall,wall++,half*hx*.5f,side*(hy-halfThickness),225,(hx+12)/100,thicknessScale,wallHeight) ||
           !append(HideRepairActor::Wall,wall++,side*(hx-halfThickness),half*hy*.5f,225,thicknessScale,(hy+12)/100,wallHeight))return false;
    }
    // Three neighborhoods separated by staggered walls. Two wide central
    // openings in each divider give players independent escape routes.
    for(float side:{-1.f,1.f})for(float section:{-1.f,0.f,1.f}) {
        const float y=(section*.67f+side*.07f)*hy;
        if(!append(HideRepairActor::Wall,wall++,side*hx*.31f,y,225,1.35f,hy*.37f/100,wallHeight))return false;
    }
    // Keep the large scenery out of the launch bays as well as the lanes.
    // The closest prop center is at least .20*hx inside the front start row.
    for(unsigned i=0;i<18;++i) {
        const float group=static_cast<float>(i/6)-1.f;
        const unsigned spot=i%6;
        float x=(group*.45f+(static_cast<float>(spot%3)-1.f)*.075f)*hx;
        float y=((spot<3?-.57f:.57f)+(spot%3==1?.14f:0.f))*hy;
        auto kind=spot%3==1?HideRepairActor::Tree:HideRepairActor::Crate;
        if(!append(kind,i,x,y,5,1,1,1))return false;
        auto& t=plan.actors[plan.count-1].transform;float half=(i%4)*.39269908f;
        t.rotation[2]=std::sin(half);t.rotation[3]=std::cos(half);
    }
    if(!append(HideRepairActor::Sky,0,0,0,0,64,64,64) ||
       !append(HideRepairActor::Sun,0,0,0,1400) || !append(HideRepairActor::Ambient,0,0,0,800))return false;
    // Directional light points diagonally down; native light defaults supply
    // color/shadow behavior, with instance-only intensity via native setters.
    auto& sun=plan.actors[plan.count-2].transform;
    sun.rotation[1]=.38268343f;sun.rotation[3]=.92387953f;
    output=plan;return true;
}
using ApplyHideRepairActor=bool(*)(const HideRepairPlacement&,void*);
using RollbackHideRepair=void(*)(void*);
inline bool ApplyHideRepairPlan(const HideRepairPlan& plan,ApplyHideRepairActor apply,RollbackHideRepair rollback,void* context) {
    if(!apply || !rollback || plan.count!=HideRepairActorCount)return false;
    for(unsigned i=0;i<plan.count;++i) {
        auto& t=plan.actors[i].transform;float norm=0;
        for(float v:t.rotation){if(!std::isfinite(v))return false;norm+=v*v;}
        if(norm<.99f || norm>1.01f)return false;
        for(int axis=0;axis<3;++axis)if(!std::isfinite(t.translation[axis]) || std::fabs(t.translation[axis])>1.e7f ||
            !std::isfinite(t.scale[axis]) || t.scale[axis]<=0 || t.scale[axis]>64)return false;
    }
    try {for(unsigned i=0;i<plan.count;++i)if(!apply(plan.actors[i],context)){rollback(context);return false;}}
    catch(...){rollback(context);throw;}
    return true;
}
}
