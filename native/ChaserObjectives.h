#pragma once
#include "FlagArenaDriver.h"
#include <cstdint>
namespace TwinKartObjective {
enum class ChaserObjectiveKind {None,RecoverVault,DeliverVault};
struct ChaserVaultObservation {
    std::uint64_t identity=0;TwinKartRace::ArenaPoint position{};int holder=-1;bool ready=false;
};
struct ChaserZoneObservation {std::uint64_t identity=0;TwinKartRace::ArenaPoint position{};bool ready=false;};
struct ChaserObjectiveView {
    ChaserVaultObservation vaults[64]{};ChaserZoneObservation zones[64]{};
    unsigned vaultCount=0,zoneCount=0;
};
inline bool ChaserObjectivePoint(const TwinKartRace::ArenaPoint& p) {
    return TwinKartRace::Finite(p) && std::fabs(p.x)<=1.e7f && std::fabs(p.y)<=1.e7f && std::fabs(p.z)<=1.e7f;
}
inline bool ValidChaserObjectives(const ChaserObjectiveView* view,int seats) {
    if(!view || seats<2 || seats>16 || view->vaultCount>64 || view->zoneCount>64)return false;
    for(unsigned i=0;i<view->vaultCount;++i)if(view->vaults[i].ready) {
        const auto& v=view->vaults[i];
        if(!v.identity || !ChaserObjectivePoint(v.position) || v.holder< -1 || v.holder>=seats)return false;
        for(unsigned j=0;j<i;++j)if(view->vaults[j].ready && view->vaults[j].identity==v.identity)return false;
    }
    for(unsigned i=0;i<view->zoneCount;++i)if(view->zones[i].ready) {
        const auto& z=view->zones[i];if(!z.identity || !ChaserObjectivePoint(z.position))return false;
        for(unsigned j=0;j<i;++j)if(view->zones[j].ready && view->zones[j].identity==z.identity)return false;
        for(unsigned j=0;j<view->vaultCount;++j)if(view->vaults[j].ready && view->vaults[j].identity==z.identity)return false;
    }
    return true;
}
inline bool NativeVaultHeld(const ChaserObjectiveView& view,int slot) {
    if(view.vaultCount>64 || slot<0 || slot>=16)return false;
    for(unsigned i=0;i<view.vaultCount;++i)if(view.vaults[i].ready && view.vaults[i].holder==slot)return true;
    return false;
}
// Only observed original state qualifies a goal. No pickup, holder, delivery,
// level, score or result is written by this source policy.
inline bool CurrentChaserObjective(const ChaserObjectiveView* view,ChaserObjectiveKind kind,
    std::uint64_t identity,int slot,TwinKartRace::ArenaPoint& position) {
    if(!view || view->vaultCount>64 || view->zoneCount>64 || !identity)return false;
    if(kind==ChaserObjectiveKind::RecoverVault) {
        for(unsigned i=0;i<view->vaultCount;++i) {
            const auto& v=view->vaults[i];
            if(v.ready && v.identity==identity && v.holder== -1){position=v.position;return true;}
        }
    } else if(kind==ChaserObjectiveKind::DeliverVault && NativeVaultHeld(*view,slot)) {
        for(unsigned i=0;i<view->zoneCount;++i) {
            const auto& z=view->zones[i];if(z.ready && z.identity==identity){position=z.position;return true;}
        }
    }
    return false;
}
}
