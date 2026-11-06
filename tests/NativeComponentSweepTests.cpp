#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <stdexcept>
#include <unordered_set>
#include <vector>
#include "../native/NativeComponentSweep.h"
namespace R=TwinKartRace;
void Need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Hit {unsigned char flags=0;float time=1;};
struct Fixture {
    int world=0,component=0,owner=0,stage=0,retire=0,throwStage=0,sehStage=0;
    unsigned releases=0,sweeps=0;bool alive=true,corruptGuard=false,malformed=false,aliasOutput=false;
    int returnOverride=-1;std::unordered_set<void*> allocated;std::vector<Hit> hits;
    bool contactPolicy=false,policyRetire=false,policyThrow=false,policyFault=false,policyInvalid=false,selective=false;
    unsigned policyCalls=0;
    R::ArenaPoint first{100,200,300},last{500,600,700};R::ComponentRotation rotation{0,0,0,1};
    void Stage(int value) {
        Need(value==stage+1,"Original component query preparation/call order changed");stage=value;
        if(retire==value)alive=false;
        if(throwStage==value)throw std::runtime_error("synthetic native callback rejection");
        if(sehStage==value)RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);
    }
    void* Allocate(std::size_t bytes){auto pointer=new unsigned char[bytes]{};allocated.insert(pointer);return pointer;}
}*current=nullptr;
void __fastcall Free(void* pointer) {
    auto& f=*current;Need(f.allocated.erase(pointer)==1,"Foreign allocation or duplicate native cleanup");
    ++f.releases;delete[] static_cast<unsigned char*>(pointer);
}
void SetArray(void* raw,unsigned offset,void* pointer,int count,int capacity) {
    auto bytes=static_cast<unsigned char*>(raw);memcpy(bytes+offset,&pointer,8);memcpy(bytes+offset+8,&count,4);memcpy(bytes+offset+12,&capacity,4);
}
void* __fastcall Construct(void* raw,std::uint64_t name,const void* stat,bool complex,void* owner) {
    auto& f=*current;Need(!name && !stat && !complex && owner==&f.owner,"Original query constructor argument ABI changed");
    memset(raw,0,0x70);SetArray(raw,0x40,nullptr,0,8);SetArray(raw,0x60,f.Allocate(32),5,8);
    f.Stage(1);return raw;
}
void __fastcall Responses(void* raw){memset(raw,0,32);current->Stage(2);}
void __fastcall SetResponses(void* raw,int value){Need(value==2,"Original collision response initialization changed");memset(raw,value,32);current->Stage(3);}
void __fastcall Initialize(void* component,void* raw,void* responses) {
    auto& f=*current;Need(component==&f.component && static_cast<unsigned char*>(responses)[31]==2,"Native per-component response/owner preparation skipped");
    SetArray(raw,0x40,f.Allocate(64),16,16);
    if(f.malformed)SetArray(raw,0x40,R::ComponentSweepDetail::Pointer(*reinterpret_cast<R::ComponentSweepDetail::Params*>(raw),0x40),5000,5000);
    f.Stage(4);
}
bool __fastcall Sweep(void* world,R::ComponentHits* output,void* component,const R::ArenaPoint* first,const R::ArenaPoint* last,
    const R::ComponentRotation* rotation,const void* params) {
    auto& f=*current;++f.sweeps;
    Need(world==&f.world && component==&f.component && first->x==100 && last->z==700 && rotation->w==1 && params,
         "Original component/body/start/end/quaternion/query ABI changed");
    Need(first!=&f.first && last!=&f.last && rotation!=&f.rotation,"Native sweep exposed caller-owned pose storage");
    Need(output && !output->data && !output->count && !output->capacity,"Original output array was not empty and caller-owned");
    bool blocking=false;
    if(!f.hits.empty()) {
        output->count=output->capacity=static_cast<int>(f.hits.size());output->data=f.Allocate(f.hits.size()*0x88);
        for(std::size_t i=0;i<f.hits.size();++i) {
            auto at=static_cast<unsigned char*>(output->data)+i*0x88;memcpy(at+4,&f.hits[i].time,4);at[0x5d]=f.hits[i].flags;
            std::uint64_t name=0x6348;memcpy(at+0x80,&name,8);blocking|=(f.hits[i].flags&1)!=0;
        }
    }
    if(f.aliasOutput) {
        if(output->data)Free(output->data);
        void* pointer=nullptr;memcpy(&pointer,static_cast<const unsigned char*>(params)+0x40,8);
        output->data=pointer;output->count=0;output->capacity=1;
    }
    if(f.corruptGuard)*(reinterpret_cast<unsigned char*>(output)+16)^=1;
    const_cast<R::ArenaPoint*>(first)->x=-100;const_cast<R::ComponentRotation*>(rotation)->w=-100;
    f.Stage(5);return f.returnOverride<0?blocking:f.returnOverride!=0;
}
bool Valid(void* world,void* component,void* owner,void* raw) {
    auto& f=*static_cast<Fixture*>(raw);return f.alive && world==&f.world && component==&f.component && owner==&f.owner;
}
R::ComponentSweepApi Api(){return {Construct,Responses,SetResponses,Initialize,Sweep,Free};}
R::ComponentContactDecision Policy(void* world,void* component,void* owner,const unsigned char* hit,void* raw) {
    auto& f=*static_cast<Fixture*>(raw);++f.policyCalls;
    Need(world==&f.world && component==&f.component && owner==&f.owner && (hit[0x5d]&3)==1,
         "Contact policy received wrong ownership, trigger or initial penetration");
    float time=0;memcpy(&time,hit+4,4);
    // The policy receives an owned copy, never writable native hit storage.
    const_cast<unsigned char*>(hit)[0x5d]=0;
    if(f.policyRetire)f.alive=false;
    if(f.policyThrow)throw std::runtime_error("contact policy exception");
    if(f.policyFault)RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);
    if(f.policyInvalid)return R::ComponentContactDecision::Invalid;
    return f.selective && time>.5f?R::ComponentContactDecision::Obstacle:R::ComponentContactDecision::Approach;
}
R::VolumeSweep Query(Fixture& f) {current=&f;return R::QueryComponentBody(&f.world,&f.component,&f.owner,Api(),Valid,&f,f.first,f.last,f.rotation,
    f.contactPolicy?Policy:nullptr,&f);}
bool QueryFault(Fixture* f) {
    __try {Query(*f);return false;}
    __except(EXCEPTION_EXECUTE_HANDLER){return true;}
}
void Clean(Fixture& f){Need(f.allocated.empty(),"Native parameter/output allocation leaked");Need(f.first.x==100 && f.rotation.w==1,"Native callback corrupted caller-owned path pose");}
int main(){try {
    {Fixture f;Need(Query(f).result==R::VolumeSweepResult::Clear && f.sweeps==1 && f.releases==2,"Qualified empty original body query failed");Clean(f);}
    {Fixture f;f.hits={{0,.25f},{1,.8f},{3,0.f},{1,.2f}};auto result=Query(f);
     Need(result.result==R::VolumeSweepResult::Blocked && result.time==0 && f.releases==3,"Original multi-hit/initial penetration classification lost geometry contacts");Clean(f);}
    {Fixture f;f.hits={{0,.2f},{2,0}};Need(Query(f).result==R::VolumeSweepResult::Clear,"Native nonblocking triggers/overlaps became artificial walls");Clean(f);}
    for(int stage=1;stage<=5;++stage) {
        Fixture f;f.retire=stage;f.hits={{1,.2f}};
        Need(Query(f).result==R::VolumeSweepResult::Invalid && (stage==5?f.sweeps==1:f.sweeps==0),
             "Expired body/world/physics/owner lease continued a native stage or accepted its result");Clean(f);
    }
    for(int stage=1;stage<=5;++stage) {
        Fixture f;f.throwStage=stage;f.hits={{1,.2f}};bool caught=false;
        try{Query(f);}catch(const std::runtime_error&){caught=true;}
        Need(caught,"Native C++ exception was swallowed or did not occur");Clean(f);
    }
    for(int stage=1;stage<=5;++stage) {
        Fixture f;f.sehStage=stage;f.hits={{1,.2f}};
        Need(QueryFault(&f),"Native SEH fault was swallowed or skipped");Clean(f);
    }
    {Fixture f;f.malformed=true;Need(Query(f).result==R::VolumeSweepResult::Invalid && !f.sweeps,"Malformed native ignored-component storage entered physics query");Clean(f);}
    {Fixture f;f.corruptGuard=true;f.hits={{1,.5f}};Need(Query(f).result==R::VolumeSweepResult::Invalid,"Original output-array ABI overwrite accepted");Clean(f);}
    {Fixture f;f.aliasOutput=true;Need(Query(f).result==R::VolumeSweepResult::Invalid,"Aliased query/output allocations were accepted or freed twice");Clean(f);}
    {Fixture f;f.hits={{1,.2f}};f.returnOverride=0;Need(Query(f).result==R::VolumeSweepResult::Invalid,"Native false contradicted blocking contacts without rejection");Clean(f);}
    {Fixture f;f.returnOverride=1;Need(Query(f).result==R::VolumeSweepResult::Invalid,"Native true without a blocking contact was accepted");Clean(f);}
    {Fixture f;f.hits={{1,(std::numeric_limits<float>::quiet_NaN)()}};Need(Query(f).result==R::VolumeSweepResult::Invalid,"Nonfinite native contact accepted");Clean(f);}
    {Fixture f;f.hits.resize(513);Need(Query(f).result==R::VolumeSweepResult::Invalid,"Oversized native hit array was silently truncated");Clean(f);}
    {Fixture f;f.rotation.w=2;current=&f;Need(Query(f).result==R::VolumeSweepResult::Invalid && !f.stage && f.allocated.empty(),"Malformed native quaternion entered body query");}
    {Fixture f;f.alive=false;Need(Query(f).result==R::VolumeSweepResult::Invalid && !f.stage,"Invalid native body lease constructed query storage");Clean(f);}
    {Fixture f;f.contactPolicy=true;f.hits={{1,.2f},{1,.8f}};auto result=Query(f);
     Need(result.result==R::VolumeSweepResult::Blocked && result.time==.2f && result.approachContacts==2 && result.onlyApproachContacts &&
          f.policyCalls==2 && f.hits[0].flags==1,"Intentional contacts were mislabeled clear, lost native time, mutated original hits or were not distinguished");Clean(f);}
    {Fixture f;f.contactPolicy=true;f.selective=true;f.hits={{1,.2f},{1,.8f}};auto result=Query(f);
     Need(result.result==R::VolumeSweepResult::Blocked && result.approachContacts==1 && !result.onlyApproachContacts,
          "A permitted target contact also waived a separate obstacle");Clean(f);}
    {Fixture f;f.contactPolicy=true;f.hits={{0,.2f},{3,0}};auto result=Query(f);
     Need(result.result==R::VolumeSweepResult::Blocked && !result.onlyApproachContacts && !result.approachContacts && !f.policyCalls,
          "Contact policy waived initial penetration or reclassified native triggers");Clean(f);}
    {Fixture f;f.contactPolicy=true;f.hits={{1,.2f},{1,(std::numeric_limits<float>::quiet_NaN)()}};
     Need(Query(f).result==R::VolumeSweepResult::Invalid && !f.policyCalls,"Malformed late hit emitted earlier contact callbacks");Clean(f);}
    for(bool expire:{false,true}){Fixture f;f.contactPolicy=true;f.hits={{1,.2f}};f.policyInvalid=!expire;f.policyRetire=expire;
     Need(Query(f).result==R::VolumeSweepResult::Invalid && f.policyCalls==1,"Invalid/expired contact decision published a usable native query");Clean(f);}
    {Fixture f;f.contactPolicy=true;f.hits={{1,.2f}};f.policyThrow=true;bool caught=false;
     try{Query(f);}catch(const std::runtime_error&){caught=true;}Need(caught,"Contact-policy C++ exception was swallowed");Clean(f);}
    {Fixture f;f.contactPolicy=true;f.hits={{1,.2f}};f.policyFault=true;
     Need(QueryFault(&f),"Contact-policy native fault was swallowed");Clean(f);}
    puts("PASS original component-body query boundary: constructor/setup order, native geometry/pose arguments, blocking versus trigger contacts, owned hit/inline-array cleanup, aliases/limits/ABI guards and lifetime rejection at every stage including C++ and SEH faults. Synthetic providers only; no native geometry query or route accepted.");return 0;
}catch(const std::exception& e){fprintf(stderr,"%s\n",e.what());return 1;}}
