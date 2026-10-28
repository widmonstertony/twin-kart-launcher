#pragma once
#include <cstdio>
#include <cstring>
#include <cwchar>

namespace TwinKartRace {
// Session-local requests describe gameplay intent, never function addresses,
// executable paths or arbitrary engine console commands.
struct Request {
    char identity[33]{};
    char track[97]{};
    int mode=1, ai=7, laps=3;
};
inline bool Valid(const Request& request) {
    if(strlen(request.identity)!=32 || !request.track[0] || strlen(request.track)>96 ||
       (request.mode!=1 && request.mode!=4) || request.ai<1 || request.ai>7 || request.laps<1 || request.laps>9) return false;
    for(char ch:request.identity) if(ch && !((ch>='a' && ch<='f') || (ch>='0' && ch<='9'))) return false;
    if(!((request.track[0]>='A' && request.track[0]<='Z') || (request.track[0]>='a' && request.track[0]<='z'))) return false;
    for(char ch:request.track) if(ch && !((ch>='A' && ch<='Z') || (ch>='a' && ch<='z') || (ch>='0' && ch<='9') || ch=='_')) return false;
    return true;
}
inline bool Parse(const char* text,Request* request) {
    if(!text || !request || strlen(text)>256) return false;
    Request parsed; char extra=0;
    if(sscanf_s(text,"TKRACE1 %32s %96s %d %d %d %c",parsed.identity,33u,parsed.track,97u,&parsed.mode,&parsed.ai,&parsed.laps,&extra,1u)!=5 || !Valid(parsed)) return false;
    *request=parsed; return true;
}
inline bool Options(const Request& request,wchar_t* target,size_t capacity) {
    if(!Valid(request) || !target || capacity<2) return false;
    const wchar_t* mode=request.mode==1?L"SpeedSingle":L"ItemSingle";
    // PC MajorConnectRule::ApplyOptions consumes players and ais separately.
    // This creates an experimental local host; it does not establish racing,
    // AI movement, item rules, finish order or settlement acceptance.
    return swprintf_s(target,capacity,L"?game=/Script/KartDrift.%sGameMode?gamemode=%s?team=0?teamsize=1?players=1?observers=0?ais=%d?lap=%d",mode,mode,request.ai,request.laps)>0;
}
}
