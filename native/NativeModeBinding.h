#pragma once
#include "RaceControl.h"
#include "NativeEquipmentPreset.h"
#include "NativeEventRules.h"
#include "NativeUiOptions.h"
namespace TwinKartRace {
struct ModeMapEntry {TwinKartEquipment::String key;void* asset;int next,hash;};
static_assert(sizeof(ModeMapEntry)==32,"Original FString/asset native map element");
struct ModeDefinition {
    void* asset=nullptr;
    wchar_t gameClass[192]{},travelName[64]{};
    int kind=-1,teamSize=0,teamCount=0;
};
using InspectModeAsset=bool(*)(void*,ModeDefinition&,void*);
inline const wchar_t* ModeKey(int mode) {
    switch(mode) {
    case 1:return L"SpeedSolo";case 2:return L"SpeedDuo";case 3:return L"SpeedSquad";
    case 4:return L"ItemSolo";case 5:return L"ItemDuo";case 6:return L"ItemSquad";
    case 101:return L"InfinityBoosterSolo";case 102:return L"InfinityBoosterDuo";case 103:return L"InfinityBoosterSquad";
    case 104:return L"FlagSquad";case 105:return L"SpitemSolo";case 106:return L"SpitemDuo";case 107:return L"SpitemSquad";
    case 110:return L"ItemLabSolo";case 111:return L"ItemLabDuo";case 112:return L"ItemLabSquad";
    case 113:return L"SpeedLabSolo";case 114:return L"SpeedLabDuo";case 115:return L"SpeedLabSquad";
    case 116:return L"cubecubeitem_solo";case 118:return L"cubecubeitem_squad";default:return nullptr;
    }
}
inline void CanonicalModeProfile(Request& request) {
    // The cooked May menu retains this older identifier; the installed
    // Brodi asset is registered under item_solo_brodi. This is one audited
    // identifier migration, never generic prefix stripping/fallback.
    if(request.mode==110 && !strcmp(request.profile,"s062_item_solo_brodi"))strcpy_s(request.profile,"item_solo_brodi");
    if(!request.profile[0] && CubeMode(request.mode))strcpy_s(request.profile,request.mode==116?"cubecubeitem_solo":"cubecubeitem_squad");
}
inline bool ValidModeDefinition(const Request& request,const ModeDefinition& definition) {
    if(!Valid(request) || !definition.asset || definition.kind<0 || definition.kind>2 ||
       definition.teamSize!=TeamSize(request.mode) || definition.teamCount!=8/definition.teamSize ||
       wcsnlen_s(definition.travelName,64)>=64 || wcscmp(definition.travelName,ModeName(request.mode)))return false;
    constexpr wchar_t prefix[]=L"/Script/KartDrift.";
    size_t n=wcsnlen_s(definition.gameClass,192),start=ARRAYSIZE(prefix)-1;
    if(n<=start+8 || n>=192 || wmemcmp(definition.gameClass,prefix,start) || wcscmp(definition.gameClass+n-8,L"GameMode"))return false;
    for(size_t i=start;i<n;++i)if(!((definition.gameClass[i]>=L'A'&&definition.gameClass[i]<=L'Z') ||
       (definition.gameClass[i]>=L'a'&&definition.gameClass[i]<=L'z') || (i>start && definition.gameClass[i]>=L'0'&&definition.gameClass[i]<=L'9')))return false;
    return true;
}
// Native GetGameModeDataAsset silently retries the empty profile on failure.
// Require an exact key and compatible native definition before invoking it.
inline bool ResolveModeBinding(const Request& request,const ModeMapEntry* entries,int count,InspectModeAsset inspect,void* context,ModeDefinition& output) {
    if(!Valid(request) || !entries || count<1 || count>512 || !inspect)return false;
    wchar_t key[97]{};
    if(request.profile[0])for(size_t i=0;request.profile[i];++i)key[i]=request.profile[i];
    else {auto canonical=ModeKey(request.mode);if(!canonical || wcslen(canonical)>=ARRAYSIZE(key))return false;wcscpy_s(key,canonical);}
    ModeDefinition found{};int matches=0;
    for(int i=0;i<count;++i) {
        const auto& entry=entries[i];
        if(!entry.key.data || entry.key.count!=static_cast<int>(wcslen(key)+1) || !TwinKartEquipment::Valid(entry.key,97) || wcscmp(entry.key.data,key))continue;
        if(++matches!=1 || !inspect(entry.asset,found,context) || !ValidModeDefinition(request,found))return false;
    }
    if(matches!=1)return false;output=found;return true;
}
inline bool ModeOptions(const Request& input,const ModeDefinition& definition,wchar_t* output,size_t capacity) {
    Request normalized;if(!NormalizeUiOptions(input,normalized))return false;const auto& request=normalized;
    if(!ValidModeDefinition(request,definition) || !output)return false;
    int written=FlagMode(request.mode)?
        _snwprintf_s(output,capacity,_TRUNCATE,L"game=%s?gamemode=%s?team=2?teamsize=4?players=1?observers=0?ais=7",definition.gameClass,definition.travelName):
        _snwprintf_s(output,capacity,_TRUNCATE,L"game=%s?gamemode=%s?team=%d?teamsize=%d?players=1?observers=0?ais=%d?lap=%d",definition.gameClass,definition.travelName,TeamCount(request.mode,request.ai+1),definition.teamSize,request.ai,request.laps);
    if(written<=0 || static_cast<size_t>(written)>=capacity)return false;
    // Native GetKtGameMode reconstructs the numeric enum from racemode.
    // gamemode is only the class/traveler name and cannot substitute for it.
    int modeBytes=_snwprintf_s(output+written,capacity-written,_TRUNCATE,L"?racemode=%d",request.mode);
    if(modeBytes<=0)return false;written+=modeBytes;
    // FKtGameMode in Traveler is lobby/loading metadata. The original
    // UOptionsStringComponent reads the separate gameprofile travel key to
    // choose the in-world configuration. Preserve it across both boundaries.
    if(request.profile[0]) {
        wchar_t profile[97]{};for(size_t i=0;request.profile[i];++i)profile[i]=request.profile[i];
        if(static_cast<size_t>(written)>=capacity)return false;
        int extra=_snwprintf_s(output+written,capacity-written,_TRUNCATE,L"?gameprofile=%s",profile);
        if(extra<=0)return false;written+=extra;
    }
    size_t position=static_cast<size_t>(written);if(!AppendEventRules(request,output,capacity,position))return false;
    written=static_cast<int>(position);
    size_t custom=strlen(request.custom);if(static_cast<size_t>(written)+custom>=capacity)return false;
    for(size_t i=0;i<custom;++i)output[written+i]=request.custom[i];output[written+custom]=0;return true;
}
}
