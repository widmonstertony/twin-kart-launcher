#pragma once
#include <cwchar>
namespace TwinKartRace {
struct ReturnString { wchar_t* data; int count,capacity; };
using AssignReturnString=void*(__fastcall*)(ReturnString*,const ReturnString*);
inline bool ReturnStringShape(const ReturnString& value) {
    return value.count>=0 && value.count<=512 && value.capacity>=value.count && value.capacity<=1048576 &&
        (!value.count || (value.data && value.data[value.count-1]==0));
}
inline bool HasReturnRoute(const ReturnString& value) {
    if(!ReturnStringShape(value) || value.count<=1)return false;
    for(int i=0;i<value.count-1;++i)if(!value.data[i])return false;
    return true;
}
// Only the verified standalone lobby caller may populate an empty destination.
// Use the game's FString assignment so the traveler owns its buffer after travel.
inline int PrepareReturnRoute(ReturnString& destination,const ReturnString& lobbyMap,
                              bool standaloneLobby,AssignReturnString assign) {
    if(!standaloneLobby || !assign || !ReturnStringShape(destination))return -1;
    if(HasReturnRoute(destination))return 1;
    if(!HasReturnRoute(lobbyMap))return -1;
    assign(&destination,&lobbyMap);
    return HasReturnRoute(destination) && destination.count==lobbyMap.count &&
        !std::wmemcmp(destination.data,lobbyMap.data,lobbyMap.count)?2:-1;
}
}
