#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <stdexcept>
#include "../native/NativeKartContacts.h"
namespace R=TwinKartRace;
void Need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Fixture {
    int receiver=0,component[4]{};R::ArenaPoint points[4]{{-100,-70,0},{-100,70,0},{100,-70,0},{100,70,0}};
    unsigned locations=0,components=0,grounded=0,stages=0;
    int expire=-1,locationFail=-1,componentFail=-1,invalidSupport=-1,throwAt=-1,faultAt=-1;
    bool alive=true,allGrounded=true,corruptPoint=false,corruptComponent=false,changePoint=false,changeComponent=false,omitPoint=false;
    void Stage(){++stages;if(static_cast<int>(stages)==expire)alive=false;
        if(static_cast<int>(stages)==throwAt)throw std::runtime_error("contact callback exception");
        if(static_cast<int>(stages)==faultAt)RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);}
}*current=nullptr;
bool __fastcall Grounded(void* receiver,bool all) {
    auto& f=*current;Need(receiver==&f.receiver && all && f.alive,"Grounding ABI/expired native receiver changed");
    ++f.grounded;f.Stage();return f.allGrounded;
}
bool __fastcall Location(void* receiver,R::ArenaPoint* point,unsigned wheel) {
    auto& f=*current;Need(receiver==&f.receiver && wheel<4 && f.alive,"Invalid wheel index or expired location receiver");
    ++f.locations;f.Stage();if(static_cast<int>(wheel)==f.locationFail)return false;
    if(!f.omitPoint)*point=f.points[wheel];
    if(f.changePoint && f.locations>4)point->z+=1;
    if(f.corruptPoint)reinterpret_cast<unsigned*>(point)[3]=0;
    return true;
}
bool __fastcall Component(void* receiver,void** component,unsigned wheel) {
    auto& f=*current;Need(receiver==&f.receiver && wheel<4 && f.alive,"Invalid wheel index or expired component receiver");
    ++f.components;f.Stage();if(static_cast<int>(wheel)==f.componentFail)return false;
    *component=&f.component[f.changeComponent && f.components>4?(wheel+1)%4:wheel];
    if(f.corruptComponent)reinterpret_cast<std::uint64_t*>(component)[1]=0;
    return true;
}
bool Valid(void* receiver,void* raw){auto& f=*static_cast<Fixture*>(raw);return receiver==&f.receiver && f.alive;}
bool Support(void* component,void* raw) {
    auto& f=*static_cast<Fixture*>(raw);for(unsigned i=0;i<4;++i)if(component==&f.component[i])return f.alive && static_cast<int>(i)!=f.invalidSupport;
    return false;
}
R::KartContactsApi api{Grounded,Location,Component};
bool Query(Fixture& f,R::KartContactSnapshot& out){current=&f;return R::ReadKartContacts(&f.receiver,api,Valid,Support,&f,out);}
bool CatchFault(Fixture& f,R::KartContactSnapshot& out){__try {Query(f,out);return false;}__except(EXCEPTION_EXECUTE_HANDLER){return true;}}
int main(){try {
    Fixture first;R::KartContactSnapshot out;out.wheels[0].component=&out;
    Need(Query(first,out) && first.locations==8 && first.components==8 && first.grounded==4 && out.wheels[3].point.x==100 &&
         out.wheels[0].component==&first.component[0],"Stable original four-wheel contact snapshot failed");
    auto reject=[](Fixture& f,const char* message) {
        R::KartContactSnapshot output;output.wheels[0].component=&output;output.wheels[0].point={7,8,9};
        Need(!Query(f,output) && output.wheels[0].component==&output && output.wheels[0].point.z==9,message);
    };
    Fixture air;air.allGrounded=false;reject(air,"Airborne kart published a ground calibration");Need(!air.locations && !air.components,"Airborne kart read stale wheel arrays");
    Fixture dead;dead.alive=false;reject(dead,"Expired native interface was queried");Need(!dead.grounded,"Expired native interface reached original method");
    for(int stage=1;stage<=20;++stage){Fixture f;f.expire=stage;reject(f,"Native callback lifetime transition published partial contacts");}
    for(int wheel=0;wheel<4;++wheel) {
        Fixture a;a.locationFail=wheel;reject(a,"Missing wheel contact accepted");
        Fixture b;b.componentFail=wheel;reject(b,"Missing wheel support component accepted");
        Fixture c;c.invalidSupport=wheel;reject(c,"Foreign/expired support component accepted");
    }
    Fixture wrongPoint;wrongPoint.corruptPoint=true;reject(wrongPoint,"Native wheel-point ABI overwrite accepted");
    Fixture wrongComponent;wrongComponent.corruptComponent=true;reject(wrongComponent,"Native support-pointer ABI overwrite accepted");
    Fixture omitted;omitted.omitPoint=true;reject(omitted,"Success without a written original contact point was accepted");
    Fixture changed;changed.changePoint=true;reject(changed,"Changing native wheel cache published a stale contact frame");
    Fixture changedOwner;changedOwner.changeComponent=true;reject(changedOwner,"Changing support component published a stale contact frame");
    Fixture invalid;invalid.points[2].x=(std::numeric_limits<float>::quiet_NaN)();reject(invalid,"Nonfinite contact published a ground calibration");
    for(int stage:{1,4,10,14,20}) {
        Fixture f;f.throwAt=stage;R::KartContactSnapshot output;output.wheels[0].component=&output;bool caught=false;
        try{Query(f,output);}catch(const std::runtime_error&){caught=true;}
        Need(caught && output.wheels[0].component==&output,"C++ contact exception published a partial snapshot");
        Fixture n;n.faultAt=stage;Need(CatchFault(n,output) && output.wheels[0].component==&output,"Native contact fault published a partial snapshot");
    }
    Fixture missing;current=&missing;auto incomplete=api;incomplete.component=nullptr;
    Need(!R::ReadKartContacts(&missing.receiver,incomplete,Valid,Support,&missing,out) && !missing.stages,
         "Missing original contact interface emitted partial native calls");
    puts("PASS original kart contacts: exact all-four grounding and wheel0-3 ABI, stable double-read, support validation, no partial publication, missing/airborne/expired/transition/overwrite isolation and C++/SEH behavior. Synthetic callbacks only; no native tire contact accepted.");return 0;
}catch(const std::exception& e){fprintf(stderr,"%s\n",e.what());return 1;}}
