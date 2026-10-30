#pragma once
#include <cstdio>
#include <cmath>
namespace TwinKartCamera {
struct Settings {
    int active, forward, height, pitch, fov, headAnchor, eyeForward, eyeHeight;
};
inline Settings VehicleSettings(Settings value,bool tallFront) {
    // The observed Rudolph front blocks the default eye line. Keep its
    // original hands/handle and use the measured rearward, raised cockpit
    // viewpoint only when the user has not supplied custom eye settings.
    if(tallFront && value.headAnchor && value.eyeForward==12 && value.eyeHeight==8 &&
        value.pitch==-10 && value.fov==90) {
        value.eyeForward=-40;value.eyeHeight=40;value.pitch=-5;value.fov=100;
    }
    return value;
}
inline bool ParseSettings(const char* bytes, Settings* result) {
    if (!bytes || !result) return false;
    Settings value{1,-30,140,-10,90,0,12,8}; char extra;
    int count=sscanf_s(bytes,"%d %d %d %d %d %d %d %d %c",&value.active,&value.forward,&value.height,
        &value.pitch,&value.fov,&value.headAnchor,&value.eyeForward,&value.eyeHeight,&extra,1u);
    if(count!=8) {
        value=Settings{1,-30,140,-10,90,0,12,8};
        if(sscanf_s(bytes,"%d %d %d %d %d %c",&value.active,&value.forward,&value.height,&value.pitch,&value.fov,&extra,1u)!=5) return false;
    }
    if ((value.active!=0 && value.active!=1) || value.forward < -200 || value.forward > 300 ||
        value.height < 20 || value.height > 250 || value.pitch < -40 || value.pitch > 30 || value.fov < 60 || value.fov > 120 ||
        (value.headAnchor!=0 && value.headAnchor!=1) || value.eyeForward < -50 || value.eyeForward > 80 || value.eyeHeight < -30 || value.eyeHeight > 80) return false;
    *result=value; return true;
}

// A nearby camera is not necessarily the live driving camera: item-hit,
// finish, replay and spectator shots can all be close to the pawn. Learn the
// stable stock chase view and suspend the cockpit as soon as that relationship
// changes. The learned baseline is retained while suspended, so a long steady
// cinematic cannot become a new driving view.
struct CockpitSample { float localX, localY, localZ, fov, yawDelta; };
struct CockpitGate {
    CockpitSample baseline{}, previous{};
    int stableFrames=0;
    bool baselineValid=false, previousValid=false, active=false;
    void Reset() {
        baseline=CockpitSample{};previous=CockpitSample{};
        stableFrames=0;baselineValid=previousValid=active=false;
    }
    void Suspend() { stableFrames=0;previousValid=false;active=false; }
    static float Square(float value) { return value*value; }
    static float PositionDelta(const CockpitSample& a,const CockpitSample& b) {
        return Square(a.localX-b.localX)+Square(a.localY-b.localY)+Square(a.localZ-b.localZ);
    }
    static bool Finite(const CockpitSample& value) {
        return std::isfinite(value.localX) && std::isfinite(value.localY) &&
            std::isfinite(value.localZ) && std::isfinite(value.fov) && std::isfinite(value.yawDelta);
    }
    static bool DrivingEnvelope(const CockpitSample& value) {
        float distance=Square(value.localX)+Square(value.localY)+Square(value.localZ);
        return Finite(value) && distance>Square(50.0f) && distance<Square(3000.0f) &&
            value.localX<1500.0f && std::fabs(value.yawDelta)<175.0f &&
            value.fov>=5.0f && value.fov<=175.0f;
    }
    bool Observe(const CockpitSample& value,bool allowed) {
        if(!allowed || !DrivingEnvelope(value)) { Suspend(); return false; }
        if(!baselineValid) {
            bool continuous=previousValid && PositionDelta(value,previous)<Square(120.0f) &&
                std::fabs(value.fov-previous.fov)<8.0f &&
                std::fabs(value.yawDelta-previous.yawDelta)<18.0f;
            stableFrames=continuous ? stableFrames+1 : 1;
            previous=value;previousValid=true;
            if(stableFrames>=4) { baseline=value;baselineValid=true;active=true; }
            return active;
        }
        // Drifting deliberately rotates the kart under a lagging chase camera,
        // so pawn-local lateral position and yaw can move far from the initial
        // baseline while remaining continuous.  Preserve that motion; cuts are
        // identified by their much larger single-frame discontinuity.
        bool nearBaseline=PositionDelta(value,baseline)<Square(1200.0f) &&
            std::fabs(value.fov-baseline.fov)<45.0f &&
            std::fabs(value.yawDelta-baseline.yawDelta)<150.0f;
        bool continuous=!previousValid || (PositionDelta(value,previous)<Square(500.0f) &&
            std::fabs(value.fov-previous.fov)<25.0f &&
            std::fabs(value.yawDelta-previous.yawDelta)<80.0f);
        previous=value;previousValid=true;
        if(!nearBaseline) { active=false;stableFrames=0;return false; }
        if(active && continuous)return true;
        if(active) { active=false;stableFrames=0;return false; }
        bool nearReentry=PositionDelta(value,baseline)<Square(220.0f) &&
            std::fabs(value.fov-baseline.fov)<28.0f &&
            std::fabs(value.yawDelta-baseline.yawDelta)<55.0f;
        if(!nearReentry) { stableFrames=0;return false; }
        if(!continuous) { stableFrames=1;return false; }
        if(++stableFrames>=4)active=true;
        return active;
    }
};
}
