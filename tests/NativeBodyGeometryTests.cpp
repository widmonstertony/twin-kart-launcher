#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <stdexcept>
#include <unordered_set>
#include <vector>
#include "../native/NativeBodyGeometry.h"
namespace R=TwinKartRace;
void Need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Shape {unsigned char type=4;};
struct Fixture {
    int actor=0,scene=0;void* handle=&actor;bool scenePresent=true,locked=false,cppFault=false,sehFault=false;
    bool mutateActor=false,duplicate=false,malformed=false,corrupt=false;unsigned reads=0,collections=0,frees=0,typeCalls=0;
    std::vector<Shape> shapes=std::vector<Shape>(2);std::unordered_set<void*> allocations;
}*current=nullptr;
void __fastcall Free(void* pointer){auto& f=*current;Need(f.allocations.erase(pointer)==1,"Original shape object or duplicate storage released");++f.frees;delete[] static_cast<void**>(pointer);}
void* __fastcall Scene(const void* handle) {
    auto& f=*current;Need(handle==&f.handle,"Native scene query lost original actor handle");
    return f.scenePresent?&f.scene:nullptr;
}
bool __fastcall Read(const void* handle,R::PhysicsReadRef* callback) {
    auto& f=*current;++f.reads;Need(handle==&f.handle && callback && callback->invoke && callback->context,"Native read callback/context ABI changed");
    f.locked=f.scenePresent;
    callback->invoke(callback->context,handle);
    if(f.duplicate)callback->invoke(callback->context,handle);
    // This line must execute even if a shape collector/type query raised a
    // C++ or SEH exception; otherwise the real native lock would be stranded.
    Need(!f.cppFault || f.typeCalls,"C++ fault fixture did not reach geometry classification");
    f.locked=false;return true;
}
void __fastcall Collect(const void* handle,void* raw) {
    auto& f=*current;++f.collections;Need(f.locked && handle==&f.handle,"Shape enumeration escaped original physics read lock");
    auto& shapes=*static_cast<R::BodyGeometryDetail::Shapes*>(raw);
    Need(!shapes.allocation && !shapes.count && shapes.capacity==32,"Native inline shape storage was not constructed");
    shapes.count=static_cast<int>(f.shapes.size());shapes.capacity=32;
    void** values=shapes.inlineShapes;
    if(f.shapes.size()>32) {
        shapes.capacity=static_cast<int>(f.shapes.size());values=new void*[f.shapes.size()]{};
        shapes.allocation=values;f.allocations.insert(values);
    }
    for(std::size_t i=0;i<f.shapes.size();++i)values[i]=&f.shapes[i];
    if(f.malformed)shapes.capacity=shapes.count-1;
    if(f.corrupt)shapes.guard^=1;
}
unsigned char __fastcall Type(const void* raw) {
    auto& f=*current;Need(f.locked,"Native shape type escaped physics read lock");++f.typeCalls;
    if(f.cppFault)throw std::runtime_error("synthetic geometry callback exception");
    if(f.sehFault)RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);
    if(f.mutateActor)f.handle=nullptr;
    return (*static_cast<Shape*const*>(raw))->type;
}
R::BodyGeometrySnapshot Query(Fixture& f) {
    current=&f;R::BodyGeometryApi api{Read,Collect,Type,Scene,Free};return R::ReadBodyGeometry(&f.handle,&f.actor,api);
}
void Clean(const Fixture& f){Need(!f.locked && f.allocations.empty(),"Physics read lock or external shape-list storage leaked");}
int main(){try {
    {Fixture f;auto a=Query(f);Need(a.valid && a.scene==&f.scene && a.count==2 && a.supported==2 && a.signature,"Actual supported shape snapshot missing");
     auto b=Query(f);Need(b.valid && b.signature==a.signature,"Unchanged native geometry identity did not remain stable");
     f.shapes[0].type=3;auto c=Query(f);Need(c.valid && c.signature!=a.signature,"Original geometry type replacement retained a stale fingerprint");Clean(f);}
    {Fixture f;f.shapes.resize(40);auto result=Query(f);Need(result.valid && result.count==40 && f.frees==1,"Original external shape pointer storage was not released");Clean(f);}
    {Fixture f;f.shapes.clear();Need(!Query(f).valid,"Empty native body certified vehicle clearance");Clean(f);}
    {Fixture f;f.shapes={{5},{6}};Need(!Query(f).valid,"Mesh/heightfield-only body admitted a native sweep that skips both");Clean(f);}
    {Fixture f;f.shapes={{5},{6},{4}};auto result=Query(f);Need(result.valid && result.count==3 && result.supported==1,"Original supported/unsupported geometry mix misclassified");Clean(f);}
    {Fixture f;f.shapes={{7}};Need(!Query(f).valid,"Invalid original geometry type accepted");Clean(f);}
    {Fixture f;f.shapes={{1}};Need(!Query(f).valid,"Unmapped/reserved original geometry type accepted");Clean(f);}
    {Fixture f;f.shapes={{0},{2},{3},{4}};Need(Query(f).supported==4,"Original sphere/box/capsule/convex geometry mapping changed");Clean(f);}
    {Fixture f;f.scenePresent=false;Need(!Query(f).valid && !f.collections,"Unregistered actor entered shape enumeration without a scene/read lock");Clean(f);}
    {Fixture f;f.shapes.resize(40);f.cppFault=true;Need(!Query(f).valid && f.frees==1,"C++ geometry exception escaped native callback or retained shape-list storage");Clean(f);}
    {Fixture f;f.shapes.resize(40);f.sehFault=true;Need(!Query(f).valid && f.frees==1,"SEH geometry exception escaped native callback or retained shape-list storage");Clean(f);}
    {Fixture f;f.mutateActor=true;Need(!Query(f).valid,"Replaced native actor handle retained geometry validity");Clean(f);}
    {Fixture f;f.duplicate=true;Need(!Query(f).valid,"Reentrant/repeated native physics callback certified a mixed snapshot");Clean(f);}
    {Fixture f;f.shapes.resize(257);Need(!Query(f).valid && f.frees==1,"Oversized native shape list silently truncated or leaked");Clean(f);}
    {Fixture f;f.shapes.resize(40);f.malformed=true;Need(!Query(f).valid && f.frees==1,"Malformed native shape-list storage accepted or leaked");Clean(f);}
    {Fixture f;f.shapes.resize(40);f.corrupt=true;Need(!Query(f).valid && f.frees==1,"Native shape-array ABI overrun accepted or leaked");Clean(f);}
    {Fixture f;f.handle=nullptr;Need(!Query(f).valid && !f.reads,"Expired actor handle entered native physics command");Clean(f);}
    puts("PASS original body geometry snapshot: registered native actor scene, inline/external shapes, supported primitive/convex types, stable/replaced identity, strict limits and C++/SEH callback containment preserving physics unlock and allocation cleanup. Synthetic interfaces only; no native body or path accepted.");return 0;
}catch(const std::exception& e){fprintf(stderr,"%s\n",e.what());return 1;}}
