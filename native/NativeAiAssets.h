#pragma once
#include "NativeEquipmentPreset.h"
#include <string>

namespace TwinKartRace {
// Exact supported PC AssetManager ABI. Independently derived from its original
// belonging-path consumers (0x146c820 and 0x1444fb0); no resource extraction or
// synchronous model loading. A registered path is necessary, not proof that
// every dependent mesh/animation has successfully loaded in a race.
struct AiAssetLookup {BYTE* image=nullptr;void* manager=nullptr;unsigned long long type=0;};
struct AiAssetPath {unsigned long long name=0;TwinKartEquipment::String subpath;};
static_assert(sizeof(AiAssetPath)==24,"Supported PC soft path ABI");
inline bool BeginAiAssetLookup(BYTE* image,AiAssetLookup& lookup) {
    __try {
        const BYTE pathBytes[]{0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18};
        // Full executable identity is checked by the containing module.
        if(!image || memcmp(image+0x1444fb0,pathBytes,sizeof(pathBytes)))return false;
        using Get=void*(__fastcall*)();using Name=void*(__fastcall*)(void*,const wchar_t*,int);
        void* manager=reinterpret_cast<Get>(image+0x3d525c0)();
        if(!manager)return false;
        auto type=reinterpret_cast<BYTE*>(reinterpret_cast<Get>(image+0x23052e0)());
        auto actual=*reinterpret_cast<BYTE**>(static_cast<BYTE*>(manager)+0x10);
        if(!type || !actual)return false;
        int index=*reinterpret_cast<int*>(type+0x38),count=*reinterpret_cast<int*>(actual+0x38);
        if(index<0 || index>count || count>1024 ||
           (*reinterpret_cast<BYTE***>(actual+0x30))[index]!=type+0x30)return false;
        unsigned long long name=0;
        reinterpret_cast<Name>(image+0x2589130)(&name,L"UBelongingDataAsset",1);
        if(!static_cast<unsigned>(name))return false;
        lookup={image,manager,name};return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
inline int RegisteredAiAsset(const std::wstring& identifier,void* context) {
    auto lookup=static_cast<AiAssetLookup*>(context);
    if(!lookup || !lookup->manager || identifier.empty() || identifier.size()>96)return -1;
    AiAssetPath path{};int result=-1;
    __try {
        __try {
            TwinKartEquipment::String id{identifier.c_str(),static_cast<int>(identifier.size()+1),static_cast<int>(identifier.size()+1)};
            using Find=AiAssetPath*(__fastcall*)(void*,AiAssetPath*,unsigned long long,const TwinKartEquipment::String*);
            if(reinterpret_cast<Find>(lookup->image+0x1444fb0)(lookup->manager,&path,lookup->type,&id)==&path &&
               TwinKartEquipment::Valid(path.subpath,1024))result=static_cast<unsigned>(path.name)?1:0;
        } __finally {
            if(path.subpath.data) {
                using Free=void(__fastcall*)(const void*);
                reinterpret_cast<Free>(lookup->image+0x248b950)(path.subpath.data);
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {result=-1;}
    return result;
}
}
