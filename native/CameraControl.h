#pragma once
#include <cstdio>
namespace TwinKartCamera {
struct Settings {
    int active, forward, height, pitch, fov, headAnchor, eyeForward, eyeHeight;
};
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
}
