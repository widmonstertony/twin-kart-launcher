#pragma once
#include <cmath>
#include <algorithm>
namespace TwinKartRace {
struct ArenaPoint {float x=0,y=0,z=0;};
inline bool Finite(const ArenaPoint& p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
inline float ArenaDistance(const ArenaPoint& a,const ArenaPoint& b){return std::hypot(a.x-b.x,a.y-b.y);}
struct ArenaControl {float throttle=0,steer=0;bool reverse=false;};
// The original kart input distinguishes its exact reverse-key sentinel from
// ordinary analog acceleration/braking. Keep this explicit in local adapters.
inline float NativeKartThrottle(const ArenaControl& control){return control.reverse?-1.f:control.throttle;}
// Original local feedback controller. The native game retains vehicle physics,
// collision, pickups and scoring. No position writes or fabricated captures.
class ArenaDriver {
    ArenaPoint previous{};float stalled=0,reverse=0,reverseSteer=0;bool initialized=false;
public:
    void Reset(){*this=ArenaDriver{};}
    ArenaControl Step(const ArenaPoint& position,float heading,const ArenaPoint& target,float dt,int slot) {
        if(!Finite(position)||!Finite(target)||!std::isfinite(heading)||!std::isfinite(dt)||dt<=0||dt>1)return {};
        const float distance=ArenaDistance(position,target);
        float speed=initialized?ArenaDistance(position,previous)/dt:0;
        float forwardSpeed=initialized?((position.x-previous.x)*std::cos(heading)+(position.y-previous.y)*std::sin(heading))/dt:0;
        if(distance<100){previous=position;stalled=0;return {forwardSpeed>200?-0.8f:forwardSpeed<-200?0.8f:0.f,0};}
        if(initialized)stalled=speed<80?stalled+dt:0;
        previous=position;initialized=true;
        float angle=std::atan2(target.y-position.y,target.x-position.x)-heading;
        angle=std::atan2(std::sin(angle),std::cos(angle));
        float steering=std::clamp(angle*1.5f,-1.f,1.f);
        if(stalled>2.0f){reverse=1.15f;reverseSteer=slot%2?-1.f:1.f;stalled=0;}
        if(!reverse && std::fabs(angle)>2.2f && speed<700){reverse=1.f;reverseSteer=angle>0?-1.f:1.f;}
        if(reverse>0){reverse=(std::max)(0.f,reverse-dt);return {-0.8f,reverseSteer,true};}
        // Large heading errors use a low-speed turn, then accelerate along the
        // direct arena approach. Stuck recovery backs away using real controls.
        float throttle=std::fabs(angle)>1.2f?0.32f:std::fabs(angle)>0.5f?0.65f:1.f;
        float desired=std::clamp(distance/1.8f,500.f,6500.f);
        if(std::fabs(angle)>1.2f)desired=450;
        else if(std::fabs(angle)>0.5f)desired=(std::min)(desired,1800.f);
        // Proportional speed feedback avoids alternating full acceleration and
        // reverse braking at every sample, especially on short ramp segments.
        throttle=std::clamp((desired-forwardSpeed)/1500.f,-0.65f,throttle);
        return {throttle,steering};
    }
};
}
