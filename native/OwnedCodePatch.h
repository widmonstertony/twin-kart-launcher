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
    // Resident hooks remain allocated until the owned game process exits.
    bool Call(BYTE* site,const void* expected,const void* replacement) {
        if(target || site[0]!=0xe8) return false;
        int displacement=0; memcpy(&displacement,site+1,4);
        if(site+5+displacement!=expected) return false;
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
        if(!allocation) return false;
        Jump(static_cast<BYTE*>(allocation),replacement);
        DWORD ignored=0;
        if(!VirtualProtect(allocation,4096,PAGE_EXECUTE_READ,&ignored)) { VirtualFree(allocation,0,MEM_RELEASE); allocation=nullptr; return false; }
        FlushInstructionCache(GetCurrentProcess(),allocation,14);
        target=site; length=5; memcpy(before,site,length);
        BYTE bytes[5]{0xe8};
        displacement=static_cast<int>(reinterpret_cast<intptr_t>(allocation)-reinterpret_cast<intptr_t>(site+5));
        memcpy(bytes+1,&displacement,4);
        if(!Write(bytes)) { Restore(); return false; }
        return true;
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
