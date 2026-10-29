#pragma once
#include "DriftObjectIdentity.h"
namespace TwinKartLocalRace {
// A local-host request may bind only its first native world. An armed request
// must never carry its AI/result overrides into a later time attack or lobby.
class LocalRaceWorld {
    TwinKartCamera::ObjectIdentity identity{};
public:
    void Reset(){identity={};}
    bool Bind(const TwinKartCamera::ObjectIdentity& world,TwinKartCamera::ObjectSlot** chunks,int count) {
        if(!TwinKartCamera::Live(world,chunks,count))return false;
        if(identity.object)return Matches(world.object,chunks,count);
        identity=world;return true;
    }
    bool Matches(void* world,TwinKartCamera::ObjectSlot** chunks,int count) const {
        return world && identity.object==world && TwinKartCamera::Live(identity,chunks,count);
    }
};
}
