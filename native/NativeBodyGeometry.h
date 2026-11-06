#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <cstddef>
namespace TwinKartRace {
struct PhysicsReadRef {void(__fastcall* invoke)(void*,const void*)=nullptr;void* context=nullptr;};
using ReadPhysicsActor=bool(__fastcall*)(const void*,PhysicsReadRef*);
using CollectBodyShapes=void(__fastcall*)(const void*,void*);
using BodyGeometryType=unsigned char(__fastcall*)(const void*);
using BodyActorScene=void*(__fastcall*)(const void*);
using FreeBodyShapeStorage=void(__fastcall*)(void*);
static_assert(sizeof(PhysicsReadRef)==16,"Original native TFunctionRef callback/context ABI");
struct BodyGeometryApi {ReadPhysicsActor read=nullptr;CollectBodyShapes collect=nullptr;BodyGeometryType type=nullptr;
    BodyActorScene scene=nullptr;FreeBodyShapeStorage release=nullptr;};
struct BodyGeometrySnapshot {void* scene=nullptr;std::uint64_t signature=0;unsigned count=0,supported=0;bool valid=false;};
namespace BodyGeometryDetail {
constexpr std::uint64_t Guard=0x184d3f6a9cb027e5ull;
struct alignas(16) Shapes {
    void* inlineShapes[32]{};void* allocation=nullptr;int count=0,capacity=32;
    std::uint64_t guard=Guard,padding=0;
};
static_assert(offsetof(Shapes,allocation)==0x100 && offsetof(Shapes,count)==0x108 && offsetof(Shapes,guard)==0x110,
    "Original TInlineAllocator<32> physics-shape ABI");
struct ReadContext {const BodyGeometryApi* api=nullptr;const void* handle=nullptr;void* actor=nullptr;
    BodyGeometrySnapshot result{};unsigned callbacks=0;};
inline void* Actor(const void* handle){void* actor=nullptr;if(handle)memcpy(&actor,handle,8);return actor;}
inline void Collect(ReadContext& context,const void* handle) {
    Shapes shapes;
    __try {
        if(handle!=context.handle || Actor(handle)!=context.actor)return;
        auto scene=context.api->scene(handle);if(!scene)return;
        context.api->collect(handle,&shapes);
        if(shapes.guard!=Guard || shapes.count<1 || shapes.count>256 || shapes.capacity<shapes.count || shapes.capacity>4096 ||
           (!shapes.allocation && shapes.capacity>32))return;
        auto values=shapes.allocation?static_cast<void**>(shapes.allocation):shapes.inlineShapes;
        // Original ComponentSweepMulti skips triangle meshes/heightfields.
        // At least one actual supported body geometry must be present.
        BodyGeometrySnapshot result;result.scene=scene;result.count=static_cast<unsigned>(shapes.count);
        std::uint64_t hash=1469598103934665603ull;
        for(int i=0;i<shapes.count;++i) {
            if(!values[i])return;
            unsigned type=context.api->type(values+i);if(type==1 || type>6)return;
            if(type<5)++result.supported;
            // Pointer identity and type are retained as a fingerprint only;
            // no native shape object escapes the original physics read lock.
            hash^=reinterpret_cast<std::uintptr_t>(values[i]);hash*=1099511628211ull;hash^=type;hash*=1099511628211ull;
        }
        if(!result.supported || Actor(handle)!=context.actor || context.api->scene(handle)!=scene)return;
        result.signature=hash?hash:1;result.valid=true;context.result=result;
    } __finally {
        void* allocation=shapes.allocation;shapes.allocation=nullptr;
        // The array owns only optional external pointer storage. Shape
        // handles are borrowed native objects and are never released here.
        auto value=reinterpret_cast<std::uintptr_t>(allocation),begin=reinterpret_cast<std::uintptr_t>(&shapes);
        if(allocation && !(value>=begin && value-begin<sizeof(shapes)))context.api->release(allocation);
    }
}
inline void CollectWithoutCppException(ReadContext& context,const void* handle) {
    try {Collect(context,handle);}catch(...){context.result={};}
}
inline void __fastcall Invoke(void* raw,const void* handle) {
    auto& context=*static_cast<ReadContext*>(raw);
    if(context.callbacks++){context.result={};return;}
    // Native ExecuteRead's unlock follows this callback. Neither C++ nor SEH
    // exceptions may escape it and strand the original physics read lock.
    __try {CollectWithoutCppException(context,handle);}
    __except(EXCEPTION_EXECUTE_HANDLER){context.result={};}
}
}
inline BodyGeometrySnapshot ReadBodyGeometry(const void* handle,void* expectedActor,const BodyGeometryApi& api) {
    if(!handle || !expectedActor || !api.read || !api.collect || !api.type || !api.scene || !api.release)return {};
    BodyGeometryDetail::ReadContext context;context.api=&api;context.handle=handle;context.actor=expectedActor;
    PhysicsReadRef callback{BodyGeometryDetail::Invoke,&context};
    __try {
        if(BodyGeometryDetail::Actor(handle)!=expectedActor || !api.read(handle,&callback) || context.callbacks!=1 ||
           BodyGeometryDetail::Actor(handle)!=expectedActor || !context.result.valid)return {};
        return context.result;
    } __except(EXCEPTION_EXECUTE_HANDLER){return {};}
}
}
