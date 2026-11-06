#pragma once
#include <cstddef>
#include <cstring>
#include <cwchar>
#include "NativeRaceModes.h"
namespace TwinKartRace {
// A native registry entry or playable mode mask does not turn a retained
// developer world into a finished visual scene. Keep such maps out of the
// ordinary race path; exact diagnostic requests have a separate contract.
inline bool DevelopmentWorldPath(const wchar_t* path) {
    if(!path)return false;
    if(!_wcsnicmp(path,L"World'",6))path+=6;
    return !_wcsnicmp(path,L"/Game/Dev/",10);
}
// Raw native metadata includes999 in the owned table. Preserve it exactly;
// it is not a request for a999-lap session (race requests remain1..9).
inline bool ValidNativeTrackLaps(int laps){return (laps>=0 && laps<=99) || laps==999;}
// Exact PC FKtTrackDef: native byte-enum TArray at +0x18. The independently
// mapped EKtLevelType enumerators are ENone=0,EItem=1,ESpeed=2,ESpitem=3,EFlag=4.
// This is native mode compatibility, never inferred from an I/R name suffix.
inline bool ReadNativeTrackModeMask(const void* record,std::size_t size,unsigned& mask) {
    if(!record || size<0x28)return false;
    struct Types {const unsigned char* data;int count,capacity;} types{};
    static_assert(sizeof(Types)==16,"Expected exact x64 native array layout");
    memcpy(&types,static_cast<const unsigned char*>(record)+0x18,sizeof(types));
    if(types.count<0 || types.count>16 || types.capacity<types.count || types.capacity>1024 || (types.count && !types.data))return false;
    unsigned decoded=0;
    for(int i=0;i<types.count;++i){unsigned value=types.data[i];if(value>4)return false;decoded|=1u<<value;}
    mask=decoded;return true;
}
inline bool NativeTrackSupports(unsigned mask,int mode) {
    return (mask&~31u)==0 && PlayableMode(mode) &&
        (mask&(1u<<((FlagMode(mode)||ChaserMode(mode))?4:SpitemMode(mode)?3:SpeedMode(mode)?2:1)))!=0;
}
}
