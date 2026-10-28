#pragma once
#include <windows.h>
namespace TwinKartCamera {
// Supported client's chunked UObject slot layout. Never allocates a serial or
// keeps an object alive. Zero-serial objects cannot be retained for later writes.
struct ObjectIdentity { void* object; int index, serial; };
struct ObjectSlot { void* object; unsigned flags; int cluster, serial, padding; };
static_assert(sizeof(ObjectSlot) == 24, "Supported x64 object item stride");
inline bool Live(const ObjectIdentity& identity, ObjectSlot** chunks, int count) {
    __try {
        if (!identity.object || identity.index < 0 || identity.index >= count || !chunks || identity.serial <= 0) return false;
        ObjectSlot* chunk = chunks[identity.index / 65536];
        if (!chunk) return false;
        const ObjectSlot& slot = chunk[identity.index % 65536];
        if (slot.object != identity.object || slot.serial != identity.serial || (slot.flags & 0x30000000)) return false;
        BYTE* object = static_cast<BYTE*>(identity.object);
        return *reinterpret_cast<int*>(object + 0xc) == identity.index && !(*reinterpret_cast<unsigned*>(object + 8) & 0x18000);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
inline bool Capture(void* object, ObjectSlot** chunks, int count, ObjectIdentity* result) {
    __try {
        if (!object || !chunks || !result) return false;
        int index = *reinterpret_cast<int*>(static_cast<BYTE*>(object) + 0xc);
        if (index < 0 || index >= count) return false;
        ObjectSlot* chunk = chunks[index / 65536];
        if (!chunk) return false;
        ObjectIdentity candidate{object,index,chunk[index % 65536].serial};
        if (!Live(candidate,chunks,count)) return false;
        *result = candidate; return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
}
