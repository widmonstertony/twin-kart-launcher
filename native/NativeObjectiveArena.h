#pragma once
#include "FlagArenaDriver.h"
#include <cwchar>
#include <cstring>

namespace TwinKartObjective {
// Explicit original compatibility arena, not a claim that the vendor released
// a finished HideNSeek map. The diagnostic adapter never substitutes a world
// for an ordinary selected circuit or accepts an arbitrary package/class.
inline constexpr const wchar_t* HideArenaPackage=L"/Game/ART/Env/Stage_ModeLab/FlagMode/Flag_F01/Flag_F01";
inline constexpr const wchar_t* HideArenaObject=L"/Game/ART/Env/Stage_ModeLab/FlagMode/Flag_F01/Flag_F01.Flag_F01";
inline void* ObjectiveArenaModeClass(bool capture,bool ownerThread,bool ownedInstance,const wchar_t* map,
                                    void* selected,void* flagClass,void* hideClass) {
    return capture && ownerThread && ownedInstance && map && selected && selected==flagClass && hideClass && hideClass!=flagClass &&
        (!wcscmp(map,L"Flag_F01") || !wcscmp(map,HideArenaPackage))?hideClass:selected;
}
struct alignas(16) ObjectiveTransform {
    float rotation[4]{0,0,0,1};
    float translation[4]{};
    float scale[4]{1,1,1,0};
};
static_assert(sizeof(ObjectiveTransform)==48,"Original PC FTransform ABI");
using ArenaGround=bool(*)(const TwinKartRace::ArenaPoint&,TwinKartRace::ArenaPoint&,void*);
struct ObjectiveArenaPlan {ObjectiveTransform starts[16],areas[3];};
inline bool BuildObjectiveArenaPlan(const TwinKartRace::ArenaPoint* anchors,int count,ArenaGround ground,
                                   void* context,ObjectiveArenaPlan& output) {
    using namespace TwinKartRace;
    if(!anchors || count<2 || count>64 || !ground)return false;
    int first=-1,second=-1;float farthest=0;
    for(int a=0;a<count;++a) {
        if(!Finite(anchors[a]))return false;
        for(int b=0;b<a;++b) {
            float d=ArenaDistance(anchors[a],anchors[b]);
            if(d>farthest){farthest=d;first=a;second=b;}
        }
    }
    if(first<0 || farthest<2000 || farthest>60000)return false;
    auto a=anchors[first],b=anchors[second];
    float dx=(b.x-a.x)/farthest,dy=(b.y-a.y)/farthest;
    // Flag receivers can be hundreds of metres apart. Reusing those endpoints
    // stranded each role in a separate patrol area. Keep the independently
    // authored Hide arena within one bounded section of the original road.
    const float span=(std::min)(farthest,6000.f);
    ArenaPoint middle{(a.x+b.x)*0.5f,(a.y+b.y)*0.5f,(a.z+b.z)*0.5f};
    a={middle.x-dx*span*0.5f,middle.y-dy*span*0.5f,middle.z};
    b={middle.x+dx*span*0.5f,middle.y+dy*span*0.5f,middle.z};
    ArenaPoint previous{};
    for(int index=0;index<=12;++index) {
        float fraction=static_cast<float>(index)/12;
        ArenaPoint sample{a.x+(b.x-a.x)*fraction,a.y+(b.y-a.y)*fraction,middle.z},contact{};
        if(!ground(sample,contact,context) || !Finite(contact) || ArenaDistance(sample,contact)>5 ||
           std::fabs(contact.z-sample.z)>4000 || (index &&
           std::fabs(contact.z-previous.z)>ArenaDistance(contact,previous)*0.25f+15))return false;
        previous=contact;
    }
    ObjectiveArenaPlan plan;
    for(int role=0;role<2;++role)for(int slot=0;slot<8;++slot) {
        auto center=role?b:a;float sign=role?-1.f:1.f;
        // Independently authored layout, projected onto the actual original
        // road. No proprietary coordinates or captured transforms are stored.
        float side=(static_cast<float>(slot%4)-1.5f)*340.f;
        float forward=static_cast<float>(slot/4)*450.f;
        ArenaPoint sample{center.x+dx*forward*sign-dy*side,center.y+dy*forward*sign+dx*side,center.z},contact{};
        if(!ground(sample,contact,context) || !Finite(contact) || ArenaDistance(sample,contact)>5 ||
           std::fabs(contact.z-sample.z)>4000)return false;
        auto& transform=plan.starts[role*8+slot];
        transform.translation[0]=contact.x;transform.translation[1]=contact.y;transform.translation[2]=contact.z+160;
        float half=std::atan2(dy*sign,dx*sign)*0.5f;
        transform.rotation[2]=std::sin(half);transform.rotation[3]=std::cos(half);
    }
    for(int index=0;index<3;++index) {
        float fraction=static_cast<float>(index+1)*0.25f;
        ArenaPoint sample{a.x+(b.x-a.x)*fraction,a.y+(b.y-a.y)*fraction,a.z+(b.z-a.z)*fraction},contact{};
        if(!ground(sample,contact,context) || !Finite(contact) || ArenaDistance(sample,contact)>5 ||
           std::fabs(contact.z-sample.z)>4000)return false;
        auto& transform=plan.areas[index];
        transform.translation[0]=contact.x;transform.translation[1]=contact.y;transform.translation[2]=contact.z+300;
        transform.scale[0]=transform.scale[1]=32;transform.scale[2]=8;
    }
    output=plan;return true;
}
using BeginObjectiveActor=void*(__fastcall*)(void*,void*,const ObjectiveTransform*,unsigned char,void*);
using FinishObjectiveActor=void*(__fastcall*)(void*,const ObjectiveTransform*);
inline void* SpawnObjectiveActor(void* world,void* type,const ObjectiveTransform& transform,
                                BeginObjectiveActor begin,FinishObjectiveActor finish) {
    if(!world || !type || !begin || !finish)return nullptr;
    float norm=0;for(float value:transform.rotation){if(!std::isfinite(value))return nullptr;norm+=value*value;}
    if(norm<0.99f || norm>1.01f)return nullptr;
    for(int axis=0;axis<3;++axis)if(!std::isfinite(transform.translation[axis]) ||
        !std::isfinite(transform.scale[axis]) || transform.scale[axis]<=0 || transform.scale[axis]>64)return nullptr;
    // Native AlwaysSpawn; these are starts/spawn-area actors, not racing pawns.
    auto actor=begin(world,type,&transform,1,nullptr);
    return actor && finish(actor,&transform)==actor?actor:nullptr;
}
}
