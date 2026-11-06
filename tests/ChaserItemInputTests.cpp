#include <cstdio>
#include <stdexcept>
#include "../native/ChaserItemInput.h"
namespace O=TwinKartObjective;
void Need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
unsigned nativeConditionCalls=0,nativeGainCalls=0;
bool __fastcall NativeCondition(void*){++nativeConditionCalls;return true;}
void __fastcall NativeGain(void*){++nativeGainCalls;}
struct Fixture {
    O::ChaserItemInputs input;O::ChaserItemObservation seats[16]{};
    unsigned calls[16]{},reads[16]{},submissions=0;int expire=-1,resetRead=-1,resetSubmit=-1,throwRead=-1;
    bool reenter=false;
    Fixture(){for(int slot=1;slot<16;++slot) {
        for(unsigned i=0;i<5;++i)seats[slot].lease[i]=static_cast<unsigned>(slot*10)+i+1;
        seats[slot].role=slot%2?O::ChaserRole::Runaway:O::ChaserRole::Tagger;
        seats[slot].ready=seats[slot].usable=true;
    }}
};
bool Read(int slot,O::ChaserItemObservation& observation,void* raw) {
    auto& f=*static_cast<Fixture*>(raw);Need(slot>0 && slot<16,"Human/out-of-range item seat read");++f.reads[slot];
    if(slot==f.throwRead){f.throwRead=-1;throw std::runtime_error("fixture read rejection");}
    if(slot==f.resetRead){f.resetRead=-1;f.input.Reset();}
    observation=f.seats[slot];return true;
}
bool Submit(int slot,const O::ChaserItemObservation& expected,void* raw) {
    auto& f=*static_cast<Fixture*>(raw);++f.submissions;Need(slot>0 && slot<16,"Human item input submitted");
    if(f.reenter)Need(f.input.Step(99999,true,16,Read,Submit,raw).slot<0,"Reentrant callback duplicated input");
    if(slot==f.resetSubmit){f.resetSubmit=-1;f.input.Reset();return false;}
    if(slot==f.expire){++f.seats[slot].lease[2];f.expire=-1;return false;}
    Need(O::SameChaserItemLease(expected,f.seats[slot]) && f.seats[slot].ready && f.seats[slot].usable,
         "Submission did not retain observed native inventory lease");
    ++f.calls[slot];return true;
}
int main(){try {
    O::ChaserRpcDelivery rpc{reinterpret_cast<void*>(1),10000,32};
    rpc.Observe(reinterpret_cast<void*>(2),10000,32);rpc.Observe(reinterpret_cast<void*>(1),10001,32);rpc.Observe(reinterpret_cast<void*>(1),10000,29);
    Need(!rpc.entered,"Foreign receiver/sequence/type satisfied native RPC delivery");
    Need(!rpc.ReserveLocal(false,true,true) && !rpc.ReserveLocal(true,false,true) && !rpc.ReserveLocal(true,true,false),
        "Unowned/networked/expired RPC obtained local delivery");
    Need(rpc.ReserveLocal(true,true,true) && !rpc.ReserveLocal(true,true,true),"Standalone RPC duplicated during reentry");
    O::ChaserRpcDelivery entered{reinterpret_cast<void*>(1),10000,32};entered.Observe(reinterpret_cast<void*>(1),10000,32);
    Need(!entered.ReserveLocal(true,true,true),"Normally delivered native RPC was repeated");
    O::ChaserEffectDelivery effect{reinterpret_cast<void*>(1)};
    Need(!effect.ReserveLocal(false,true,true) && !effect.ReserveLocal(true,false,true) && !effect.ReserveLocal(true,true,false),
         "Unowned/networked/expired effect acquired local delivery");
    effect.Observe(reinterpret_cast<void*>(2));Need(!effect.delivered,"Another actor satisfied native delivery");
    Need(effect.ReserveLocal(true,true,true) && !effect.ReserveLocal(true,true,true),"Missing local effect duplicated on reentry");
    O::ChaserEffectDelivery nativeEffect{reinterpret_cast<void*>(1)};nativeEffect.Observe(reinterpret_cast<void*>(1));
    Need(!nativeEffect.ReserveLocal(true,true,true),"Already delivered native effect was repeated");
    Need(O::ChooseChaserItemSlot(true,true,0)==0 && O::ChooseChaserItemSlot(true,true,1)==1 &&
         O::ChooseChaserItemSlot(false,true,0)==1 && O::ChooseChaserItemSlot(true,false,1)==0 &&
         O::ChooseChaserItemSlot(false,false,0)==-1 && O::ChooseChaserItemSlot(true,true,2)==-1,
         "Native two-slot selection starved a ready slot or accepted invalid preference");
    O::ChaserItemObservation slotLease;slotLease.lease={1,2,3,4,5};slotLease.ready=slotLease.usable=true;
    slotLease.role=O::ChaserRole::Tagger;auto changedSlot=slotLease;changedSlot.inventorySlot=1;
    Need(!O::SameChaserItemLease(slotLease,changedSlot),"Native slot replacement retained old due intent");
    changedSlot.inventorySlot=2;Need(!O::ValidChaserItemSeat(changedSlot),"Invalid native item slot accepted");
    Need(!O::ResolveChaserLegacyCondition(nullptr,true,NativeCondition) && !nativeConditionCalls,
        "Owned Chaser used the circuit item condition");
    O::ResolveChaserLegacyGain(nullptr,true,NativeGain);Need(!nativeGainCalls,"Owned Chaser appended legacy gain work");
    Need(O::ResolveChaserLegacyCondition(nullptr,false,NativeCondition) && nativeConditionCalls==1,"Foreign native condition forwarding changed");
    O::ResolveChaserLegacyGain(nullptr,false,NativeGain);Need(nativeGainCalls==1,"Foreign native gain forwarding changed");
    constexpr std::uintptr_t base=0x140000000ull;
    Need(O::ChaserKnownItemControllerAbi(O::ChaserItemControllerKind::Speed,base,base+0x14e18d0,base+0x14c1520,base+0x14c0ec0),
        "Actual original speed-derived item controller was not recognized");
    Need(O::ChaserKnownItemControllerAbi(O::ChaserItemControllerKind::Generic,base,base+0x14e18d0,base+0x14c1320,base+0x14c0ec0) &&
         !O::ChaserKnownItemControllerAbi(O::ChaserItemControllerKind::Unknown,base,base+0x14e18d0,base+0x14c1520,base+0x14c0ec0) &&
         !O::ChaserKnownItemControllerAbi(O::ChaserItemControllerKind::Generic,base,base+0x14e18d0,base+0x14c1520,base+0x14c0ec0) &&
         !O::ChaserKnownItemControllerAbi(O::ChaserItemControllerKind::Speed,base,base+0x14e18d1,base+0x14c1520,base+0x14c0ec0),
        "Unknown or changed native item controller ABI was accepted");
    Need(!O::ChaserNativeControllerOwnsIntent(true,0,0,0) && !O::ChaserNativeControllerOwnsIntent(true,0,16,0),
        "Idle exact generic controller suppressed local intent");
    Need(!O::ChaserNativeControllerOwnsIntent(true,1,16,1,false),"Native controller with no item cache suppressed local intent");
    Need(O::ChaserNativeControllerOwnsIntent(true,1,16,0) && O::ChaserNativeControllerOwnsIntent(true,0,16,1),
        "Queued native use/gain intent was duplicated");
    Need(O::ChaserNativeControllerOwnsIntent(false,0,0,0) && O::ChaserNativeControllerOwnsIntent(true,-1,0,0) &&
         O::ChaserNativeControllerOwnsIntent(true,2,1,0) && O::ChaserNativeControllerOwnsIntent(true,0,1025,0) &&
         O::ChaserNativeControllerOwnsIntent(true,0,0,2),"Unknown/malformed native controller yielded control");
    Fixture f;auto step=[&](std::uint64_t time,bool active=true,int count=16){return f.input.Step(time,active,count,Read,Submit,&f);};
    Need(step(1000).slot<0 && !f.submissions,"AI item intent did not wait after first observation");
    f.reenter=true;unsigned completedMask=0;
    for(std::uint64_t now=1050;now<=4000;now+=50) {
        unsigned before=f.submissions;auto result=step(now);
        Need(f.submissions-before<=1,"One pump emitted multiple native item requests");
        if(result.submitted)completedMask|=1u<<result.slot;
    }
    Need(completedMask==65534 && !f.calls[0] && !f.reads[0],"Round-robin item intent starved a bot or controlled the human");
    auto frozen=f.seats[1];f.seats[1].usable=false;unsigned before=f.calls[1];
    for(std::uint64_t now=4050;now<=5100;now+=50)step(now);
    Need(f.calls[1]==before,"Empty/cooldown-blocked inventory was used");
    f.seats[1]=frozen;Need(step(5150).slot!=1,"Newly usable inventory inherited an immediately due old intent");
    f.seats[2].ready=false;f.seats[3].role=O::ChaserRole::Unknown;f.seats[4].lease[4]=0;
    unsigned before2=f.calls[2],before3=f.calls[3],before4=f.calls[4];
    for(std::uint64_t now=5200;now<=7000;now+=50)step(now);
    Need(f.calls[2]==before2 && f.calls[3]==before3 && f.calls[4]==before4,
         "Expired/disconnected/ambiguous-role/invalid-serial item seats produced use input");
    before=f.submissions;Need(step(7050,false).slot<0 && f.submissions==before,"Paused scene emitted item input");
    Need(step(7100).slot<0,"Resume inherited an old due intent");
    before=f.submissions;Need(step(7200,true,17).slot<0 && step(7250,true,1).slot<0 && f.submissions==before,
        "Malformed native population emitted item input");
    Need(step(7300,true,2).slot<0,"Reduced roster retained a stale due item intent");
    auto token=f.seats[1];f.seats[1].role=O::ChaserRole::Tagger;Need(step(7900,true,2).slot<0,"Role infection inherited the old intent");
    ++f.seats[1].lease[0];Need(step(8500,true,2).slot<0,"Respawn/reused address inherited the old pawn's item intent");
    f.expire=1;auto expired=step(9200,true,2);Need(expired.slot==1 && !expired.submitted,"Submission-time native item replacement was accepted");
    Need(step(9250,true,2).slot<0,"Rejected replacement caused a hot retry of expired intent");
    Need(step(100,true,2).slot<0,"Clock rewind produced an overdue input burst");
    f.resetRead=1;Need(step(800,true,2).slot<0,"World reset inside native read retained old intent");
    Need(step(850,true,2).slot<0,"Callback-reset world immediately inherited a due item intent");
    f.resetSubmit=1;auto reset=step(1550,true,2);Need(reset.slot==1 && !reset.submitted,"World reset inside use was treated as accepted");
    Need(step(1600,true,2).slot<0,"Use callback reset was overwritten by the old pump");
    f.throwRead=1;bool thrown=false;
    try{step(2300,true,2);}catch(const std::runtime_error&){thrown=true;}
    Need(thrown,"Fixture read exception did not run");
    auto recovered=step(2350,true,2);Need(recovered.slot==1,"Exceptional callback permanently latched the item pump");
    f.input.Reset();f.seats[1]=token;
    Need(f.input.Step(3000,true,2,nullptr,Submit,&f).slot<0 &&
         f.input.Step(3000,true,2,Read,nullptr,&f).slot<0,"Absent original item interface was accepted");
    Need(step((std::numeric_limits<std::uint64_t>::max)()-10,true,2).slot<0 &&
         step((std::numeric_limits<std::uint64_t>::max)()-5,true,2).slot<0,"Deadline overflow created immediate native item input");
    puts("PASS Chaser item intent: 15-bot fairness, one-request budget, original inventory gating, human isolation, pause/role/respawn/replacement, submission rejection, callback reset/reentry/exception and clock bounds. Synthetic callbacks only; no native item used or consumed.");return 0;
}catch(const std::exception& e){fprintf(stderr,"%s\n",e.what());return 1;}}
