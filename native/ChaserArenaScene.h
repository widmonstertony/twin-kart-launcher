#pragma once
#include "NativeObjectiveArena.h"
namespace TwinKartObjective {
// Independently authored placement in the explicitly selected original Flag_F01
// environment. This is not a recovered vendor Chaser map or captured layout.
struct ChaserArenaPlan {
    ObjectiveTransform starts[8]{},vaults[3]{},zones[2]{};
};
inline bool BuildChaserArenaPlan(const TwinKartRace::ArenaPoint* anchors,int count,ArenaGround ground,
                                void* context,ChaserArenaPlan& output) {
    ObjectiveArenaPlan sampled;
    if(!BuildObjectiveArenaPlan(anchors,count,ground,context,sampled))return false;
    ChaserArenaPlan plan;
    for(int slot=0;slot<8;++slot)plan.starts[slot]=sampled.starts[(slot%2)*8+slot/2];
    for(int i=0;i<3;++i) {
        plan.vaults[i]=sampled.areas[i];
        for(int axis=0;axis<3;++axis)plan.vaults[i].scale[axis]=1;
        plan.vaults[i].translation[2]-=180; // Road +120, native pickup actor pivot.
    }
    for(int i=0;i<2;++i) {
        plan.zones[i]=sampled.starts[i*8];
        for(int axis=0;axis<3;++axis)plan.zones[i].scale[axis]=4;
    }
    output=plan;return true;
}
enum class ChaserSceneActor {Start,Vault,Zone};
using SpawnChaserSceneActor=bool(*)(ChaserSceneActor,unsigned,const ObjectiveTransform&,void*);
using RollbackChaserSceneActors=void(*)(void*);
// All road/layout checks precede spawning. A partial native spawn is rolled
// back by the serial/owner-aware adapter; it must never publish scene readiness.
inline bool SpawnChaserScene(const ChaserArenaPlan& plan,SpawnChaserSceneActor spawn,
                            RollbackChaserSceneActors rollback,void* context) {
    if(!spawn || !rollback)return false;
    auto valid=[](const ObjectiveTransform& t) {
        float norm=0;for(float v:t.rotation){if(!std::isfinite(v))return false;norm+=v*v;}
        if(norm<.99f || norm>1.01f)return false;
        for(int axis=0;axis<3;++axis)if(!std::isfinite(t.translation[axis]) || std::fabs(t.translation[axis])>1.e7f ||
            !std::isfinite(t.scale[axis]) || t.scale[axis]<=0 || t.scale[axis]>64)return false;
        return true;
    };
    for(const auto& t:plan.starts)if(!valid(t))return false;
    for(const auto& t:plan.vaults)if(!valid(t))return false;
    for(const auto& t:plan.zones)if(!valid(t))return false;
    bool complete=true;
    try {
        for(unsigned i=0;i<8 && complete;++i)complete=spawn(ChaserSceneActor::Start,i,plan.starts[i],context);
        for(unsigned i=0;i<3 && complete;++i)complete=spawn(ChaserSceneActor::Vault,i,plan.vaults[i],context);
        for(unsigned i=0;i<2 && complete;++i)complete=spawn(ChaserSceneActor::Zone,i,plan.zones[i],context);
    } catch(...) {rollback(context);throw;}
    if(!complete)rollback(context);
    return complete;
}
}
