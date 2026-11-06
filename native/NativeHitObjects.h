#pragma once
#include "NativeComponentSweep.h"
namespace TwinKartRace {
struct HitWeakIdentity {int index=-1,serial=0;};
struct HitObjects {
    void* actor=nullptr;void* component=nullptr;HitWeakIdentity actorIdentity{},componentIdentity{};
};
static_assert(sizeof(HitWeakIdentity)==8,"Original FWeakObjectPtr hit identity ABI");
using ResolveHitWeak=void*(__fastcall*)(const HitWeakIdentity*);
using HitObjectScope=bool(*)(void*);
using HitObjectIdentityValid=bool(*)(void*,const HitWeakIdentity&,bool,void*);
// Exact pinned FHitResult reflection: Actor weak+0x68, Component weak+0x70.
// Input is a complete owned hit copy. Resolver returns borrowed UObjects;
// caller's callback must compare native object-slot serial/type/world/owner.
inline bool ReadHitObjects(const unsigned char* hit,ResolveHitWeak resolve,HitObjectScope scope,
    HitObjectIdentityValid identity,void* context,HitObjects& output) {
    if(!hit || !resolve || !scope || !identity || !scope(context))return false;
    HitObjects objects;memcpy(&objects.actorIdentity,hit+0x68,sizeof(HitWeakIdentity));
    memcpy(&objects.componentIdentity,hit+0x70,sizeof(HitWeakIdentity));
    if(objects.actorIdentity.index<0 || objects.actorIdentity.serial<=0 || objects.componentIdentity.index<0 ||
       objects.componentIdentity.serial<=0)return false;
    objects.actor=resolve(&objects.actorIdentity);
    if(!scope(context) || !objects.actor || !identity(objects.actor,objects.actorIdentity,true,context) || !scope(context))return false;
    objects.component=resolve(&objects.componentIdentity);
    if(!scope(context) || !objects.component || !identity(objects.component,objects.componentIdentity,false,context) ||
       !scope(context) || !identity(objects.actor,objects.actorIdentity,true,context) || !scope(context))return false;
    output=objects;return true;
}
}
