#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <stdexcept>
#include "../native/NativeHitObjects.h"
namespace R=TwinKartRace;
void Need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Fixture {
    int actor=0,component=0;R::HitWeakIdentity actorId{4,90},componentId{8,120};
    unsigned char hit[0x88]{};unsigned calls=0,actorChecks=0;
    bool alive=true,actorValid=true,componentValid=true,retireFinal=false;
    int expire=-1,throwAt=-1,faultAt=-1;bool wrongActor=false,wrongComponent=false;
    Fixture(){memcpy(hit+0x68,&actorId,8);memcpy(hit+0x70,&componentId,8);}
}*current=nullptr;
void* __fastcall Resolve(const R::HitWeakIdentity* weak) {
    auto& f=*current;Need(f.alive,"Expired hit-object scope reached native weak resolver");
    Need(reinterpret_cast<const unsigned char*>(weak)!=f.hit+0x68 && reinterpret_cast<const unsigned char*>(weak)!=f.hit+0x70,
         "Native resolver received writable caller-owned hit bytes");
    ++f.calls;if(static_cast<int>(f.calls)==f.expire)f.alive=false;
    if(static_cast<int>(f.calls)==f.throwAt)throw std::runtime_error("weak resolver exception");
    if(static_cast<int>(f.calls)==f.faultAt)RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);
    if(weak->index==f.actorId.index && weak->serial==f.actorId.serial)return f.wrongActor?&f.component:&f.actor;
    if(weak->index==f.componentId.index && weak->serial==f.componentId.serial)return f.wrongComponent?&f.actor:&f.component;
    return nullptr;
}
bool Scope(void* raw){return static_cast<Fixture*>(raw)->alive;}
bool Identity(void* object,const R::HitWeakIdentity& id,bool actor,void* raw) {
    auto& f=*static_cast<Fixture*>(raw);
    if(actor){++f.actorChecks;if(f.retireFinal && f.actorChecks==2)f.alive=false;}
    const auto& expected=actor?f.actorId:f.componentId;
    return f.alive && object==(actor?&f.actor:&f.component) && id.index==expected.index && id.serial==expected.serial &&
        (actor?f.actorValid:f.componentValid);
}
bool Query(Fixture& f,R::HitObjects& output){current=&f;return R::ReadHitObjects(f.hit,Resolve,Scope,Identity,&f,output);}
bool Fault(Fixture& f,R::HitObjects& output){__try{Query(f,output);return false;}__except(EXCEPTION_EXECUTE_HANDLER){return true;}}
int main(){try {
    Fixture f;R::HitObjects output;
    Need(Query(f,output) && f.calls==2 && f.actorChecks==2 && output.actor==&f.actor && output.component==&f.component &&
         output.actorIdentity.serial==90 && output.componentIdentity.index==8,"Exact native hit weak identities were not resolved/revalidated");
    auto reject=[](Fixture& source,const char* message){R::HitObjects out;out.actor=&out;
        Need(!Query(source,out) && out.actor==&out && !out.component,message);};
    Fixture expired;expired.alive=false;reject(expired,"Expired query scope resolved native hit objects");Need(!expired.calls,"Expired query scope made native calls");
    for(int stage:{1,2}){Fixture source;source.expire=stage;reject(source,"World/body expiry during weak resolution published native objects");Need(source.calls==static_cast<unsigned>(stage),"Weak resolution continued after expiry");}
    Fixture wrongActor;wrongActor.wrongActor=true;reject(wrongActor,"Weak actor resolved a different identity/type");Need(wrongActor.calls==1,"Invalid actor was followed by another native resolver");
    Fixture wrongComponent;wrongComponent.wrongComponent=true;reject(wrongComponent,"Weak component resolved a different identity/type");
    Fixture foreignActor;foreignActor.actorValid=false;reject(foreignActor,"Foreign actor world/identity was accepted");
    Fixture foreignComponent;foreignComponent.componentValid=false;reject(foreignComponent,"Foreign component world/identity was accepted");
    Fixture finalExpiry;finalExpiry.retireFinal=true;reject(finalExpiry,"Actor expired after component resolution but was still published");
    for(auto weak:{R::HitWeakIdentity{-1,90},R::HitWeakIdentity{4,0},R::HitWeakIdentity{4,-1}}) {
        Fixture source;memcpy(source.hit+0x68,&weak,8);reject(source,"Invalid actor weak identity reached native resolver");Need(!source.calls,"Invalid weak identity emitted native calls");
    }
    Fixture stale;R::HitWeakIdentity old{4,89};memcpy(stale.hit+0x68,&old,8);reject(stale,"Stale native weak serial was accepted");
    for(int stage:{1,2}) {
        Fixture source;source.throwAt=stage;R::HitObjects out;out.actor=&out;bool caught=false;
        try{Query(source,out);}catch(const std::runtime_error&){caught=true;}
        Need(caught && out.actor==&out,"C++ resolver failure published a partial hit pair");
        Fixture native;native.faultAt=stage;Need(Fault(native,out) && out.actor==&out,"Native weak resolver fault published a partial hit pair");
    }
    Fixture missing;current=&missing;
    Need(!R::ReadHitObjects(missing.hit,nullptr,Scope,Identity,&missing,output) &&
         !R::ReadHitObjects(missing.hit,Resolve,nullptr,Identity,&missing,output) &&
         !R::ReadHitObjects(nullptr,Resolve,Scope,Identity,&missing,output) && !missing.calls,
         "Missing hit/weak/scope interfaces emitted native calls");
    puts("PASS native hit-object boundary: exact Actor+0x68/Component+0x70 weak ABI, owned value copies, serial/type/world validation, final actor recheck, no partial publication and missing/expired/stale/C++/SEH isolation. Synthetic native callbacks only; no game hit resolved.");return 0;
}catch(const std::exception& e){fprintf(stderr,"%s\n",e.what());return 1;}}
