#include <cstdio>
#include <stdexcept>
#include "../native/NativeVehicleSweep.h"
namespace R=TwinKartRace;
void Need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Fixture {
    unsigned calls=0,checks=0;bool alive=true,blocked=false,write=true,retire=false,overrun=false;
    unsigned char flags=0;float time=1;void* world=this;
    R::ArenaPoint first{100,200,300},last{500,600,700},size{50,60,70};R::QueryRotation rotation{5,25,3};
    unsigned char types[3]{0,1,17};void* ignored[1]{this};
}*current=nullptr;
bool Valid(void* world,void* raw) {
    auto& f=*static_cast<Fixture*>(raw);++f.checks;return f.alive && world==f.world;
}
bool __fastcall Trace(void* world,const R::ArenaPoint* start,const R::ArenaPoint* end,const R::ArenaPoint* half,
    const R::QueryRotation* rotation,const R::QueryArray* types,bool complex,const R::QueryArray* ignored,
    int debug,void* raw,bool ignoreSelf,const R::QueryColor* color,const R::QueryColor* hitColor,float duration) {
    auto& f=*current;++f.calls;
    Need(world==f.world && start && end && half && rotation && types && ignored && raw,"Box sweep register/stack pointer ABI changed");
    Need(start->x==f.first.x && start->y==f.first.y && start->z==f.first.z && end->x==f.last.x && end->z==f.last.z &&
         half->x==f.size.x && half->y==f.size.y && half->z==f.size.z && rotation->pitch==5 && rotation->yaw==25 && rotation->roll==3,
         "Native box dimensions, original pose or rotation were substituted");
    Need(start!=&f.first && end!=&f.last && half!=&f.size && rotation!=&f.rotation,"Caller-owned value arguments exposed as native writable storage");
    Need(types->data==f.types && types->count==3 && types->capacity==3 && ignored->data==f.ignored && ignored->count==1 &&
         ignored->capacity==1 && !complex && !debug && !ignoreSelf && color && hitColor && !duration,
         "Box sweep object filters/borrowed arrays or no-debug/self flags changed");
    Need(color->r==0 && color->g==0 && color->b==0 && color->a==1 && hitColor->a==1,"Native query colors ABI changed");
    auto hit=static_cast<unsigned char*>(raw);
    if(f.write) {
        memcpy(hit+4,&f.time,4);hit[0x5d]=f.flags;
        // The final original FName field is inside its exact 136-byte ABI.
        std::uint64_t finalName=0x13572468abcddcbaull;memcpy(hit+0x80,&finalName,8);
    }
    if(f.overrun)hit[0x88]^=1;
    if(f.retire)f.alive=false;
    // Synchronous native writes to value arguments cannot corrupt the plan.
    const_cast<R::ArenaPoint*>(start)->x=-100;const_cast<R::ArenaPoint*>(half)->z=-100;
    const_cast<R::QueryRotation*>(rotation)->yaw=-100;
    return f.blocked;
}
int main(){try {
    Fixture f;current=&f;R::QueryArray types{f.types,3,3},ignored{f.ignored,1,1};
    auto query=[&]{return R::QueryVehicleBox(f.world,Trace,Valid,&f,types,ignored,f.first,f.last,f.size,f.rotation);};
    Need(query().result==R::VolumeSweepResult::Clear && f.calls==1 && f.checks==2,
         "Qualified native miss was not distinguished from invalid input");
    Need(f.first.x==100 && f.size.z==70 && f.rotation.yaw==25,"Native callback modified caller-owned plan/box/rotation");
    f.blocked=true;f.flags=1;f.time=.25f;auto wall=query();
    Need(wall.result==R::VolumeSweepResult::Blocked && wall.time==.25f,"Volume obstruction lost the original hit time");
    f.flags=3;f.time=0;Need(query().result==R::VolumeSweepResult::Blocked,"Initial penetration was treated as clear");
    f.blocked=false;Need(query().result==R::VolumeSweepResult::Invalid,"Native false with blocking/penetrating flags became clear");
    f.flags=0;f.blocked=true;Need(query().result==R::VolumeSweepResult::Invalid,"Native true without a blocking contact became a usable query");
    f.blocked=false;f.time=.5f;Need(query().result==R::VolumeSweepResult::Invalid,"Partial native miss did not retain the original initialized no-hit time");
    f.time=(std::numeric_limits<float>::quiet_NaN)();
    Need(query().result==R::VolumeSweepResult::Invalid,"Nonfinite native time certified clearance");
    f.time=1.01f;Need(query().result==R::VolumeSweepResult::Invalid,"Out-of-segment native time certified clearance");
    f.time=1;f.write=false;Need(query().result==R::VolumeSweepResult::Invalid,"Untouched output/failed native dispatch became a clear road");f.write=true;
    f.retire=true;Need(query().result==R::VolumeSweepResult::Invalid,"Retired world or changed physics scene retained a native miss");
    f.retire=false;f.alive=true;f.overrun=true;
    Need(query().result==R::VolumeSweepResult::Invalid,"Out-of-ABI native hit write was accepted");f.overrun=false;
    unsigned calls=f.calls;f.alive=false;Need(query().result==R::VolumeSweepResult::Invalid && f.calls==calls,"Expired physics/world lease entered native trace");f.alive=true;
    types.count=33;types.capacity=33;Need(query().result==R::VolumeSweepResult::Invalid && f.calls==calls,"Oversized object query reached native trace");types={f.types,3,3};
    ignored.capacity=9;Need(query().result==R::VolumeSweepResult::Invalid && f.calls==calls,"Oversized ignore array reached native trace");ignored={f.ignored,1,1};
    ignored.data=nullptr;Need(query().result==R::VolumeSweepResult::Invalid && f.calls==calls,"Missing borrowed ignore storage reached native trace");ignored.data=f.ignored;
    f.size.x=0;Need(query().result==R::VolumeSweepResult::Invalid && f.calls==calls,"Degenerate body silently became a line query");f.size.x=50;
    f.first.z=(std::numeric_limits<float>::infinity)();Need(query().result==R::VolumeSweepResult::Invalid && f.calls==calls,"Nonfinite native path entered trace");f.first.z=300;
    f.rotation.roll=(std::numeric_limits<float>::quiet_NaN)();Need(query().result==R::VolumeSweepResult::Invalid && f.calls==calls,"Invalid body rotation entered trace");f.rotation.roll=3;
    auto last=f.last;f.last=f.first;Need(query().result==R::VolumeSweepResult::Invalid && f.calls==calls,
         "Zero-length sweep was accepted as an overlap/vehicle clearance query");f.last=last;
    Need(R::QueryVehicleBox(f.world,nullptr,Valid,&f,types,ignored,f.first,f.last,f.size,f.rotation).result==R::VolumeSweepResult::Invalid &&
         R::QueryVehicleBox(f.world,Trace,nullptr,&f,types,ignored,f.first,f.last,f.size,f.rotation).result==R::VolumeSweepResult::Invalid && f.calls==calls,
         "Missing native/world interface fabricated volume clearance");
    puts("PASS original box-sweep boundary: exact x64 value/array/rotation arguments, complete 136-byte hit output, obstruction/penetration/invalid distinction, retired physics-world isolation and no plan mutation. Synthetic callback only; no native vehicle geometry, path clearance or gameplay accepted.");return 0;
}catch(const std::exception& e){fprintf(stderr,"%s\n",e.what());return 1;}}
