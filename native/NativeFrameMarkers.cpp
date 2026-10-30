#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <atomic>
#include <cstdio>
#include "NativeFrameMarkers.h"
namespace TwinKartGraphics {
namespace {
using Method=void(__fastcall*)(void*,unsigned long long);
Method originals[6]{};
std::shared_ptr<NativeFrameMarkerSink> sink;
NativeMarkerLog logger=nullptr;
thread_local unsigned long long renderFrame=0;
thread_local bool haveRenderFrame=false;
bool installed=false;
SRWLOCK installLock=SRWLOCK_INIT;
void Observe(FrameStage stage,unsigned long long id) {
    if(stage==FrameStage::RenderSubmitStart || stage==FrameStage::PresentStart){renderFrame=id;haveRenderFrame=true;}
    auto selected=std::atomic_load_explicit(&sink,std::memory_order_acquire);
    if(selected)try{selected->Observe(stage,id);}catch(...){
        if(logger)logger("{\"event\":\"native_marker_sink_exception\",\"game_acceptance\":false}\r\n");
    }
}
template<unsigned Stage> void __fastcall Hook(void* self,unsigned long long id) {
    Observe(static_cast<FrameStage>(Stage),id);
    originals[Stage](self,id);
}
Method hooks[6]{Hook<0>,Hook<1>,Hook<2>,Hook<3>,Hook<4>,Hook<5>};
// These are the six native virtual methods that build NVAPI version0x10058
// with this frame ID and marker types0..5. We observe BEFORE their native
// Reflex enable checks; the original methods and CPU semantics are preserved.
constexpr unsigned Slots[6]{0x5012418,0x5012420,0x5012438,0x5012440,0x5012428,0x5012430};
constexpr unsigned Methods[6]{0x10b0f30,0x10b0e60,0x10b0d90,0x10b0cc0,0x10b0a10,0x10b0940};
bool Install(Method* const (&slots)[6],const Method (&expected)[6]) {
    for(unsigned i=0;i<6;++i)if(!slots[i] || *slots[i]!=expected[i])return false;
    unsigned changed=0;
    for(;changed<6;++changed) {
        DWORD before=0;
        if(!VirtualProtect(slots[changed],sizeof(void*),PAGE_READWRITE,&before))break;
        // A native thread can enter the replacement as soon as its slot is
        // published. Publish its forwarding target before that exchange.
        originals[changed]=expected[changed];
        auto old=InterlockedCompareExchangePointer(reinterpret_cast<PVOID volatile*>(slots[changed]),
            reinterpret_cast<void*>(hooks[changed]),reinterpret_cast<void*>(expected[changed]));
        DWORD ignored=0;bool restored=VirtualProtect(slots[changed],sizeof(void*),before,&ignored)!=FALSE;
        if(old!=reinterpret_cast<void*>(expected[changed]) || !restored)break;
    }
    if(changed==6)return true;
    // Include the failing slot: its pointer could have been exchanged before
    // restoring page protection failed. Never overwrite another hook's slot.
    for(unsigned i=0;i<=changed && i<6;++i) {
        DWORD before=0;if(!VirtualProtect(slots[i],sizeof(void*),PAGE_READWRITE,&before))continue;
        InterlockedCompareExchangePointer(reinterpret_cast<PVOID volatile*>(slots[i]),
            reinterpret_cast<void*>(expected[i]),reinterpret_cast<void*>(hooks[i]));
        DWORD ignored=0;VirtualProtect(slots[i],sizeof(void*),before,&ignored);
    }
    return false;
}
}
bool ConfigureNativeFrameMarkers(NativeMarkerLog log) {
    AcquireSRWLockExclusive(&installLock);
    struct Unlock { ~Unlock(){ReleaseSRWLockExclusive(&installLock);} } unlock;
    if(installed)return true;
    BYTE* image=reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr));
    Method* slots[6]{};Method expected[6]{};
    for(unsigned i=0;i<6;++i){slots[i]=reinterpret_cast<Method*>(image+Slots[i]);expected[i]=reinterpret_cast<Method>(image+Methods[i]);}
    // Caller already verified the exact allowed client hash before this hook.
    logger=log;installed=Install(slots,expected);
    if(logger)logger(installed?"{\"event\":\"native_simulation_render_present_markers_installed\",\"game_acceptance\":false}\r\n":
        "{\"event\":\"native_frame_marker_interface_rejected\"}\r\n");
    return installed;
}
bool BindNativeFrameMarkerSink(const std::shared_ptr<NativeFrameMarkerSink>& value) {
    if(!value)return false;
    std::shared_ptr<NativeFrameMarkerSink> empty;
    return std::atomic_compare_exchange_strong_explicit(&sink,&empty,value,std::memory_order_acq_rel,std::memory_order_acquire);
}
void RemoveNativeFrameMarkerSink(const std::shared_ptr<NativeFrameMarkerSink>& value) {
    auto expected=value;std::shared_ptr<NativeFrameMarkerSink> empty;
    std::atomic_compare_exchange_strong_explicit(&sink,&expected,empty,std::memory_order_acq_rel,std::memory_order_acquire);
}
bool CurrentNativeRenderFrame(unsigned long long& id){if(!haveRenderFrame)return false;id=renderFrame;return true;}
#ifdef TWIN_KART_SYNTHETIC_MARKER_INPUT
void ObserveSyntheticNativeMarker(FrameStage stage,unsigned long long id){Observe(stage,id);}
#endif
}
