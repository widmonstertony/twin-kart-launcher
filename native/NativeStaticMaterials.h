#pragma once
#include <windows.h>
#include <cstring>
#include "ScopedMaterialRepair.h"
namespace TwinKartVisual {
// Supported PC reflected FStaticMaterial: material, slot/imported slot names,
// and24 bytes of UV metadata/padding. Native GetMaterial independently uses
// mesh+0x88 and this48-byte stride. No borrowed array pointer escapes a read.
struct StaticMaterial {void* material=nullptr;std::uint64_t slot=0,importedSlot=0;unsigned char uv[24]{};};
static_assert(sizeof(StaticMaterial)==48,"Exact supported FStaticMaterial ABI");
template<class T>struct MaterialArray {T* data=nullptr;int count=0,capacity=0;};
static_assert(sizeof(MaterialArray<void*>)==16,"Exact supported material-array ABI");
inline bool ReadStaticMaterials(const void* mesh,MaterialSlot (&output)[32],unsigned& count) {
    count=0;if(!mesh)return false;
    __try {
        MaterialArray<const StaticMaterial> array;
        memcpy(&array,static_cast<const unsigned char*>(mesh)+0x88,sizeof(array));
        if(array.count<1 || array.count>32 || array.capacity<array.count || array.capacity>128 || !array.data)return false;
        MaterialSlot slots[32]{};
        for(int i=0;i<array.count;++i)slots[i]={array.data[i].slot,array.data[i].material,nullptr};
        if(!UniqueMaterialSlots(slots,static_cast<unsigned>(array.count)))return false;
        memcpy(output,slots,static_cast<size_t>(array.count)*sizeof(MaterialSlot));count=static_cast<unsigned>(array.count);return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
inline bool ReadMaterialOverrides(const void* component,void* (&output)[32],unsigned& count) {
    count=0;if(!component)return false;
    __try {
        MaterialArray<void*> array;
        memcpy(&array,static_cast<const unsigned char*>(component)+0x478,sizeof(array));
        if(array.count<0 || array.count>32 || array.capacity<array.count || array.capacity>128 || (array.count && !array.data))return false;
        void* slots[32]{};
        for(int i=0;i<array.count;++i)slots[i]=array.data[i];
        memcpy(output,slots,sizeof(slots));count=static_cast<unsigned>(array.count);return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
}
