#pragma once
#include "RaceControl.h"
#include <cstdint>
namespace TwinKartRace {
struct PreparationString {wchar_t* data;int count,capacity;};
struct PreparationMode {unsigned char mode;unsigned char padding[7];PreparationString profile;};
struct PreparationHints {const void* data;int count,capacity;};
static_assert(sizeof(PreparationMode)==24,"Original FKtGameMode x64 layout");
using CopyPreparationString=void*(__fastcall*)(PreparationString*,const PreparationString*);
using InitializeNativeRace=bool(__fastcall*)(void*,const PreparationMode*,PreparationString*,void*,const PreparationHints*,bool);
// The original PreInitializeByGame takes its track FString by value and
// destroys that value. Supply a copy made by the original allocator; never
// pass a borrowed stack buffer or free the transferred string a second time.
inline bool PrepareNativeRace(void* traveler,const Request& request,CopyPreparationString copy,InitializeNativeRace initialize) {
    if(!traveler || !Valid(request) || !copy || !initialize)return false;
    wchar_t track[97]{},profile[97]{};
    size_t trackSize=strlen(request.track),profileSize=strlen(request.profile);
    for(size_t i=0;i<trackSize;++i)track[i]=request.track[i];
    for(size_t i=0;i<profileSize;++i)profile[i]=request.profile[i];
    PreparationString view{track,static_cast<int>(trackSize+1),static_cast<int>(trackSize+1)},owned{};
    PreparationMode mode{};mode.mode=static_cast<unsigned char>(request.mode);
    mode.profile={profile,profileSize?static_cast<int>(profileSize+1):0,profileSize?static_cast<int>(profileSize+1):0};
    if(copy(&owned,&view)!=&owned || !owned.data || owned.data==track || owned.count!=view.count || owned.capacity<owned.count)return false;
    PreparationHints hints{};
    return initialize(traveler,&mode,&owned,nullptr,&hints,false);
}
}
