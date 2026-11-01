#pragma once
#include "NativeRaceModes.h"
#include "NativeTrackModes.h"
#include <cwchar>
#include <cstring>
namespace TwinKartRace {
// Explicit, independently audited original CubeCube worlds. Keep the base
// track identity for its original AI path and loading metadata, while loading
// the dedicated Cube world. Never turn an arbitrary track into ordinary Item
// Mode, strip a Cube prefix, or choose between duplicate leaf names implicitly.
struct CubeWorld {const wchar_t* track;const wchar_t* package;};
inline const CubeWorld* CubeWorldForTrack(int mode,const wchar_t* track) {
    if(!CubeMode(mode) || !track)return nullptr;
    static const CubeWorld worlds[]={
        {L"Desert_I06",L"/Game/ART/Env/Stage_ModeLab/CubeCube/Cube_Desert_I06/Cube_Desert_I06"},
        {L"Forest_I05",L"/Game/ART/Env/Stage_ModeLab/CubeCube/Cube_Forest_I05/Cube_Forest_I05"},
        {L"Ice_I05",L"/Game/ART/Env/Stage_ModeLab/CubeCube/Cube_Ice_I05/Cube_Ice_I05"},
        {L"Village_I05",L"/Game/ART/Env/Stage_ModeLab/CubeCube/Cube_Village_I05/Cube_Village_I05"},
        {L"World_I11",L"/Game/ART/Env/Stage_ModeLab/CubeCube/Cube_World_I11/Cube_World_I11"}
    };
    for(const auto& world:worlds)if(!wcscmp(world.track,track))return &world;
    return nullptr;
}
inline bool CubeWorldObjectPath(const CubeWorld& world,wchar_t* output,size_t capacity) {
    if(!world.package || !output || !capacity)return false;
    auto leaf=wcsrchr(world.package,L'/');if(!leaf || !leaf[1])return false;
    size_t package=wcslen(world.package),name=wcslen(leaf+1);
    if(package+name+2>capacity)return false;
    memcpy(output,world.package,package*sizeof(wchar_t));output[package]=L'.';
    memcpy(output+package+1,leaf+1,(name+1)*sizeof(wchar_t));return true;
}
inline bool EventTrackSupports(unsigned mask,int mode,const wchar_t* track) {
    if(!CubeMode(mode))return NativeTrackSupports(mask,mode);
    // These two ordinary bases are marked ENone, but their independently paired
    // Cube world and exact AI path form a distinct event track. Preserve the
    // raw mask in reports; this exception applies only to these Cube bindings.
    return CubeWorldForTrack(mode,track) && (NativeTrackSupports(mask,mode) ||
        (mask==1 && (!wcscmp(track,L"Village_I05") || !wcscmp(track,L"World_I11"))));
}
}
