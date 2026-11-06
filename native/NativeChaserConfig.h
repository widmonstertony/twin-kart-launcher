#pragma once
namespace TwinKartObjective {
using LoadChaserConfigAsset=void*(__fastcall*)(void*);
using MissingChaserConfig=void*(*)(void*,void*);
// One exact native server-side load call only. Existing assets and every
// other mode/thread retain their native result. The production fallback must
// validate the actual rule/path/world and return an engine-owned native CDO.
inline void* ResolveChaserConfigAsset(void* reference,bool enabled,bool ownerThread,
    LoadChaserConfigAsset original,MissingChaserConfig fallback,void* context) {
    if(!original)return nullptr;
    auto result=original(reference);
    if(result || !reference || !enabled || !ownerThread || !fallback)return result;
    return fallback(reference,context);
}
}
