#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>

namespace TwinKartNative {
// Only exact, prevalidated instruction boundaries from the owned build may be
// used. This is not an instruction decoder or a general-purpose detour engine.
class OwnedCodePatch {
    BYTE* target=nullptr;
    BYTE before[32]{};
    unsigned length=0;
    void* allocation=nullptr;
    static void Jump(BYTE* bytes,const void* destination) {
        bytes[0]=0xff; bytes[1]=0x25; memset(bytes+2,0,4);
        memcpy(bytes+6,&destination,sizeof(destination));
    }
    bool Write(const BYTE* bytes) {
        DWORD protection=0;
        if(!VirtualProtect(target,length,PAGE_EXECUTE_READWRITE,&protection)) return false;
        memcpy(target,bytes,length); FlushInstructionCache(GetCurrentProcess(),target,length);
        DWORD ignored=0; return VirtualProtect(target,length,protection,&ignored)!=FALSE;
    }
public:
    OwnedCodePatch()=default;
    OwnedCodePatch(const OwnedCodePatch&)=delete;
    OwnedCodePatch& operator=(const OwnedCodePatch&)=delete;
    bool Active() const {return target!=nullptr;}
    // Resident hooks remain allocated until the owned game process exits.
    bool Call(BYTE* site,const void* expected,const void* replacement) {
        if(target || site[0]!=0xe8) return false;
        int displacement=0; memcpy(&displacement,site+1,4);
        if(site+5+displacement!=expected) return false;
        return CallRelay(site,5,replacement);
    }
    // One complete x64 call [rdi+disp32]. The caller pins its exact six bytes;
    // the near CALL plus trailing NOP preserves the original call-frame ABI.
    bool IndirectCall(BYTE* site,const BYTE* expected,unsigned bytes,const void* replacement) {
        if(target || !site || !expected || bytes!=6 || expected[0]!=0xff || expected[1]!=0x97 || memcmp(site,expected,bytes))return false;
        return CallRelay(site,bytes,replacement);
    }
    // Exact owned Chaser result display: MOV R8,[RAX]; CALL [R8+0x280].
    // RCX already holds the widget and DL its visibility. Replace both whole
    // instructions so a missing optional widget is checked before its vtable
    // is dereferenced. This is not a general instruction relocation API.
    bool WidgetVisibilityCall(BYTE* site,const void* replacement) {
        const BYTE expected[]{0x4c,0x8b,0x00,0x41,0xff,0x90,0x80,0x02,0x00,0x00};
        if(target || !site || memcmp(site,expected,sizeof(expected)))return false;
        return CallRelay(site,sizeof(expected),replacement);
    }
    // Exact loading track-name setter. RCX is the TextBlock and RDX is its
    // native FText argument; preserve that same call frame and ownership.
    bool LoadingTrackTextCall(BYTE* site,const void* replacement) {
        const BYTE expected[]{0xff,0x90,0xd8,0x02,0x00,0x00};
        if(target || !site || memcmp(site,expected,sizeof(expected)))return false;
        return CallRelay(site,sizeof(expected),replacement);
    }
private:
    bool AllocateNear(BYTE* site) {
        SYSTEM_INFO system{}; GetSystemInfo(&system);
        const uintptr_t origin=reinterpret_cast<uintptr_t>(site);
        const uintptr_t aligned=origin&~(static_cast<uintptr_t>(system.dwAllocationGranularity)-1);
        for(uintptr_t distance=system.dwAllocationGranularity;distance<0x70000000 && !allocation;distance+=system.dwAllocationGranularity) {
            const uintptr_t choices[]={aligned+distance,aligned>distance?aligned-distance:0};
            for(auto address:choices) {
                if(!address) continue;
                auto delta=static_cast<int64_t>(address)-static_cast<int64_t>(origin+5);
                if(delta<INT32_MIN || delta>INT32_MAX) continue;
                allocation=VirtualAlloc(reinterpret_cast<void*>(address),4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
                if(allocation) break;
            }
        }
        return allocation!=nullptr;
    }
    bool InstallNear(BYTE* site,unsigned width,unsigned relaySize,BYTE opcode) {
        DWORD ignored=0;
        if(!VirtualProtect(allocation,4096,PAGE_EXECUTE_READ,&ignored)) { VirtualFree(allocation,0,MEM_RELEASE); allocation=nullptr; return false; }
        FlushInstructionCache(GetCurrentProcess(),allocation,relaySize);
        target=site; length=width; memcpy(before,site,length);
        BYTE bytes[32];memset(bytes,0x90,sizeof(bytes));bytes[0]=opcode;
        int displacement=static_cast<int>(reinterpret_cast<intptr_t>(allocation)-reinterpret_cast<intptr_t>(site+5));
        memcpy(bytes+1,&displacement,4);
        if(!Write(bytes)) { Restore(); return false; }
        return true;
    }
    bool CallRelay(BYTE* site,unsigned width,const void* replacement) {
        if(target || !replacement || (width!=5 && width!=6 && width!=10) || !AllocateNear(site))return false;
        Jump(static_cast<BYTE*>(allocation),replacement);
        return InstallNear(site,width,14,0xe8);
    }
public:
    // The native box boost resolves an optional AnimSequence before registering
    // its animation-duration timer. A failed cast arrives at the dereference with
    // RBX=null. Skip just that timer block; retain the boost and its cleanup.
    bool ChaserBoosterAnimationGuard(BYTE* site) {
        const BYTE expected[]{0x48,0x8b,0x03,0x48,0x8b,0xcb,0xff,0x90,0x90,0x02,0x00,0x00};
        constexpr unsigned width=6, resume=0x162;
        const BYTE continuation[]{0x49,0x8b,0xce};
        if(target || !site || memcmp(site,expected,sizeof(expected)) ||
           memcmp(site+resume,continuation,sizeof(continuation)) || !AllocateNear(site))return false;
        auto relay=static_cast<BYTE*>(allocation);
        // TEST RBX,RBX; JZ missing; both exits are tail jumps, with no new
        // frame, and the live branch replays the two displaced instructions.
        const BYTE branch[]{0x48,0x85,0xdb,0x74,width+14};
        memcpy(relay,branch,sizeof(branch));memcpy(relay+5,expected,width);
        Jump(relay+5+width,site+width);
        Jump(relay+5+width+14,site+resume);
        return InstallNear(site,width,5+width+28,0xe9);
    }
    bool Entry(BYTE* site,const BYTE* prefix,unsigned bytes,const void* replacement,void** original) {
        if(target || !original || bytes<14 || bytes>sizeof(before) || memcmp(site,prefix,bytes)) return false;
        allocation=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
        if(!allocation) return false;
        memcpy(allocation,site,bytes);
        Jump(static_cast<BYTE*>(allocation)+bytes,site+bytes);
        DWORD ignored=0;
        if(!VirtualProtect(allocation,4096,PAGE_EXECUTE_READ,&ignored)) { VirtualFree(allocation,0,MEM_RELEASE); allocation=nullptr; return false; }
        FlushInstructionCache(GetCurrentProcess(),allocation,bytes+14);
        target=site; length=bytes; memcpy(before,site,bytes);
        BYTE patch[32]; memset(patch,0x90,bytes); Jump(patch,replacement);
        *original=allocation;
        if(!Write(patch)) { Restore(); *original=nullptr; return false; }
        return true;
    }
    bool Restore() {
        if(target && !Write(before)) return false;
        target=nullptr; length=0;
        if(allocation) { VirtualFree(allocation,0,MEM_RELEASE); allocation=nullptr; }
        return true;
    }
};
}
