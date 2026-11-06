#pragma once
#include "NativeEquipmentPreset.h"
#include <string>

namespace TwinKartRace {
// Exact supported PC AssetManager ABI. Independently derived from its original
// belonging-path consumers (0x146c820 and 0x1444fb0); no resource extraction or
// synchronous model loading in the registry predicates. A registered path is necessary, not proof that
// every dependent mesh/animation has successfully loaded in a race.
struct AiAssetLookup {BYTE* image=nullptr;void* manager=nullptr;unsigned long long type=0;};
struct AiAssetPath {unsigned long long name=0;TwinKartEquipment::String subpath;};
static_assert(sizeof(AiAssetPath)==24,"Supported PC soft path ABI");
struct ObjectiveSoftReference {
    int index=-1,serial=0,tag=0,padding=0;
    AiAssetPath path;
};
static_assert(sizeof(ObjectiveSoftReference)==40,"Supported PC soft object reference ABI");
// Explicit dependency preparation, called only after the containing adapter
// establishes its owned objective world/thread. This is separate from catalog
// registration checks and never runs during ordinary startup/catalog scans.
// The original LoadSynchronous operation loads the user's own registered data
// asset and its hard dependencies. No resource bytes are supplied or copied.
inline bool LoadObjectiveRegisteredAsset(AiAssetLookup& lookup,const TwinKartEquipment::String& identifier) {
    if(!lookup.image || !lookup.manager || !lookup.type || !TwinKartEquipment::Valid(identifier,97) ||
       !TwinKartEquipment::Nonempty(identifier))return false;
    AiAssetPath path{};ObjectiveSoftReference soft{};bool loaded=false;
    __try {
        using Find=AiAssetPath*(__fastcall*)(void*,AiAssetPath*,unsigned long long,const TwinKartEquipment::String*);
        if(reinterpret_cast<Find>(lookup.image+0x1444fb0)(lookup.manager,&path,lookup.type,&identifier)!=&path ||
           !path.name || !TwinKartEquipment::Valid(path.subpath,1024) || TwinKartEquipment::Nonempty(path.subpath))return false;
        soft.path.name=path.name;
        reinterpret_cast<TwinKartEquipment::CopyString>(lookup.image+0x923880)(&soft.path.subpath,&path.subpath);
        // The reflected LoadAsset_Blocking wrapper independently confirms the
        // forty-byte layout and that its inner operation borrows this storage;
        // the outer wrapper owns the separate FString destruction afterward.
        using Load=void*(__fastcall*)(ObjectiveSoftReference*);
        using Resolve=void*(__fastcall*)(const TwinKartEquipment::String*);
        void* asset=reinterpret_cast<Load>(lookup.image+0xa42300)(&soft);
        loaded=asset && reinterpret_cast<Resolve>(lookup.image+0x146c720)(&identifier)==asset;
    } __finally {
        using Free=void(__fastcall*)(const void*);
        if(soft.path.subpath.data)reinterpret_cast<Free>(lookup.image+0x248b950)(soft.path.subpath.data);
        if(path.subpath.data)reinterpret_cast<Free>(lookup.image+0x248b950)(path.subpath.data);
    }
    return loaded;
}
inline bool ObjectiveAssetIsA(BYTE* image,void* asset,unsigned getter) {
    if(!image || !asset)return false;
    using Type=BYTE*(__fastcall*)();auto expected=reinterpret_cast<Type>(image+getter)();
    auto actual=*reinterpret_cast<BYTE**>(static_cast<BYTE*>(asset)+0x10);
    if(!expected || !actual)return false;
    int depth=*reinterpret_cast<int*>(expected+0x38),count=*reinterpret_cast<int*>(actual+0x38);
    return depth>=0 && depth<=count && count<=1024 &&
        (*reinterpret_cast<BYTE***>(actual+0x30))[depth]==expected+0x30;
}
inline bool LoadObjectiveSoftMember(BYTE* image,void* asset,unsigned offset,bool required) {
    auto reference=reinterpret_cast<ObjectiveSoftReference*>(static_cast<BYTE*>(asset)+offset);
    if(!reference->path.name)return !required;
    if(!TwinKartEquipment::Valid(reference->path.subpath,1024))return false;
    // This is the data asset's own original soft property. LoadSynchronous
    // borrows it and updates its normal weak cache; it does not consume the
    // property's string. No destructor/free is applied to borrowed storage.
    using Load=void*(__fastcall*)(ObjectiveSoftReference*);
    return reinterpret_cast<Load>(image+0xa42300)(reference)!=nullptr;
}
struct ObjectiveModelFailure {unsigned group=0,field=0,offset=0;bool required=false;};
inline bool LoadObjectivePresetAssets(AiAssetLookup& lookup,const TwinKartEquipment::Preset& preset,
                                     ObjectiveModelFailure* failure=nullptr) {
    // Character[2..5] are optional finish/retire/victory/defeat motions, not
    // required body/costume mesh dependencies. Preserve their descriptors for
    // the original initializer's motion resolution/fallback. A missing finish
    // motion must not prevent a valid human body from reaching that initializer.
    for(unsigned group=0;group<2;++group)for(unsigned field=0;field<(group?2u:10u);++field) {
        const auto& id=group?preset.character[field]:preset.kart[field];
        if(TwinKartEquipment::Nonempty(id) && !LoadObjectiveRegisteredAsset(lookup,id)) {
            if(failure)*failure={group,field,0,true};return false;
        }
    }
    return true;
}
inline int ObjectiveResourceNumber(const TwinKartEquipment::String& identifier) {
    if(!TwinKartEquipment::Valid(identifier,97) || !TwinKartEquipment::Nonempty(identifier))return 0;
    auto colon=wcschr(identifier.data,L':');if(!colon || !colon[1])return 0;
    unsigned value=0;
    for(auto at=colon+1;*at;++at) {
        if(*at<L'0' || *at>L'9' || value>(2147483647u-static_cast<unsigned>(*at-L'0'))/10)return 0;
        value=value*10+static_cast<unsigned>(*at-L'0');
    }
    return static_cast<int>(value);
}
inline bool LoadObjectivePresetModels(AiAssetLookup& lookup,const TwinKartEquipment::Preset& preset,ObjectiveModelFailure* failure=nullptr) {
    using Resolve=void*(__fastcall*)(const TwinKartEquipment::String*);
    for(unsigned group=0;group<2;++group)for(unsigned field=0;field<(group?2u:10u);++field) {
        const auto& id=group?preset.character[field]:preset.kart[field];if(!TwinKartEquipment::Nonempty(id))continue;
        auto asset=reinterpret_cast<Resolve>(lookup.image+0x146c720)(&id);
        if(!asset){if(failure)*failure={group,field,0,true};return false;}
        auto load=[&](unsigned offset,bool required) {
            if(LoadObjectiveSoftMember(lookup.image,asset,offset,required))return true;
            if(failure)*failure={group,field,offset,required};return false;
        };
        // Exact reflected soft properties, also corroborated by their native
        // model consumers. Primary data-asset loading alone leaves these soft
        // mesh/physics/animation references unloaded in the retained prototype.
        if(ObjectiveAssetIsA(lookup.image,asset,0x221f730)) {
            if(!load(0x40,true) || !load(0x68,false) || !load(0xe8,false))return false;
        } else if(ObjectiveAssetIsA(lookup.image,asset,0x221f4e0)) {
            if(!load(0x70,true) || !load(0x98,false) || !load(0xc0,false))return false;
        } else if(ObjectiveAssetIsA(lookup.image,asset,0x221f7f0)) {
            if(!load(0x38,false) || !load(0x60,false))return false;
        }
    }
    return true;
}
// Inverse lookup through the original AssetManager, followed by an exact
// round trip. NPC resources need not have player-inventory/catalog item IDs.
// The native Kismet soft-reference wrapper independently establishes virtual
// slot2f8 and its hidden FPrimaryAssetId output buffer. No object is loaded.
inline bool RegisteredIdentityForPath(AiAssetLookup& lookup,const wchar_t* objectPath,wchar_t* output,size_t capacity) {
    struct PrimaryId {unsigned long long type=0,name=0;} identity;
    AiAssetPath desired{},verified{};TwinKartEquipment::String text{};bool valid=false;
    __try {
        if(!lookup.image || !lookup.manager || !lookup.type || !objectPath || !output || capacity<2 ||
           wcsnlen_s(objectPath,1024)>=1024)return false;
        using Name=void*(__fastcall*)(void*,const wchar_t*,int);
        reinterpret_cast<Name>(lookup.image+0x2589130)(&desired.name,objectPath,1);
        if(!desired.name)return false;
        auto table=*reinterpret_cast<BYTE***>(lookup.manager);auto method=table[0x2f8/8];
        if(method<lookup.image || method>=lookup.image+0x4ed0000)return false;
        using Reverse=PrimaryId*(__fastcall*)(void*,PrimaryId*,const AiAssetPath*);
        if(reinterpret_cast<Reverse>(method)(lookup.manager,&identity,&desired)!=&identity || identity.type!=lookup.type || !identity.name)return false;
        __try {
            using ToText=TwinKartEquipment::String*(__fastcall*)(TwinKartEquipment::String*,unsigned long long);
            if(reinterpret_cast<ToText>(lookup.image+0x3bcd370)(&text,identity.name)!=&text ||
               !TwinKartEquipment::Valid(text,97) || !TwinKartEquipment::Nonempty(text) || static_cast<size_t>(text.count)>capacity)return false;
            using Find=AiAssetPath*(__fastcall*)(void*,AiAssetPath*,unsigned long long,const TwinKartEquipment::String*);
            if(reinterpret_cast<Find>(lookup.image+0x1444fb0)(lookup.manager,&verified,lookup.type,&text)!=&verified ||
               !TwinKartEquipment::Valid(verified.subpath,1024) || TwinKartEquipment::Nonempty(verified.subpath) || verified.name!=desired.name)return false;
            valid=wcscpy_s(output,capacity,text.data)==0;
        } __finally {
            using Free=void(__fastcall*)(const void*);
            if(verified.subpath.data)reinterpret_cast<Free>(lookup.image+0x248b950)(verified.subpath.data);
            if(text.data)reinterpret_cast<Free>(lookup.image+0x248b950)(text.data);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER){valid=false;}
    return valid;
}
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
