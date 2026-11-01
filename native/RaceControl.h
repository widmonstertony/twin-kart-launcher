#pragma once
#include <cstdio>
#include <cstring>
#include <cwchar>
#include "NativeRaceModes.h"

namespace TwinKartRace {
// Session-local requests describe gameplay intent, never function addresses,
// executable paths or arbitrary engine console commands.
struct Request {
    char identity[33]{};
    char track[97]{};
    int mode=1, ai=7, laps=3;
    char profile[97]{},custom[513]{};
};
inline bool ValidProfile(const char* value) {
    size_t n=strnlen_s(value,97);if(n>96)return false;
    for(size_t i=0;i<n;++i)if(!((value[i]>='A'&&value[i]<='Z') || (value[i]>='a'&&value[i]<='z') ||
        (i>0 && ((value[i]>='0'&&value[i]<='9') || value[i]=='_'))))return false;
    return true;
}
inline bool ValidCustomOptions(const char* value) {
    size_t n=strnlen_s(value,513);if(n>512)return false;
    if(!n)return true;
    const char* prefixes[]={"item.","speed.","spitem.","flag.","modeLab.","infinitybooster.","track."};
    char keys[32][65]{};unsigned count=0;
    for(size_t pos=0;pos<n;) {
        if(value[pos++]!='?' || count==32)return false;
        size_t start=pos;while(pos<n && value[pos]!='=' && value[pos]!='?')++pos;
        size_t keySize=pos-start;if(pos==n || value[pos]!='=' || !keySize || keySize>=65)return false;
        char key[65]{};memcpy(key,value+start,keySize);bool allowed=false;
        for(const char* prefix:prefixes)if(!_strnicmp(key,prefix,strlen(prefix))) {
            size_t first=strlen(prefix);if(first==keySize || keySize-first>48)return false;
            for(size_t i=first;i<keySize;++i)if(!((key[i]>='A'&&key[i]<='Z') || (key[i]>='a'&&key[i]<='z') || (i>first && ((key[i]>='0'&&key[i]<='9') || key[i]=='_'))))return false;
            allowed=true;break;
        }
        if(!allowed)return false;
        for(unsigned i=0;i<count;++i)if(!_stricmp(keys[i],key))return false;
        strcpy_s(keys[count++],key);start=++pos;
        while(pos<n && value[pos]!='?') {
            char ch=value[pos++];if(!((ch>='A'&&ch<='Z') || (ch>='a'&&ch<='z') || (ch>='0'&&ch<='9') || ch=='_' || ch==',' || ch=='.' || ch=='+' || ch=='-'))return false;
        }
        if(pos==start || pos-start>96)return false;
    }
    return true;
}
inline bool Valid(const Request& request) {
    if(strlen(request.identity)!=32 || !request.track[0] || strlen(request.track)>96 ||
       !PopulationValid(request.mode,request.ai) || request.laps<1 || request.laps>9 ||
       !ValidProfile(request.profile) || !ValidCustomOptions(request.custom)) return false;
    for(char ch:request.identity) if(ch && !((ch>='a' && ch<='f') || (ch>='0' && ch<='9'))) return false;
    if(!((request.track[0]>='A' && request.track[0]<='Z') || (request.track[0]>='a' && request.track[0]<='z'))) return false;
    for(char ch:request.track) if(ch && !((ch>='A' && ch<='Z') || (ch>='a' && ch<='z') || (ch>='0' && ch<='9') || ch=='_')) return false;
    return true;
}
inline bool Parse(const char* text,Request* request) {
    if(!text || !request || strnlen_s(text,1025)>1024) return false;
    Request parsed; char extra=0;
    if(!strncmp(text,"TKRACE2 ",8)) {
        if(sscanf_s(text,"TKRACE2 %32s %96s %d %d %d %96s %512s %c",parsed.identity,33u,parsed.track,97u,&parsed.mode,&parsed.ai,&parsed.laps,parsed.profile,97u,parsed.custom,513u,&extra,1u)!=7)return false;
        if(!strcmp(parsed.profile,"-"))parsed.profile[0]=0;if(!strcmp(parsed.custom,"-"))parsed.custom[0]=0;
    } else if(sscanf_s(text,"TKRACE1 %32s %96s %d %d %d %c",parsed.identity,33u,parsed.track,97u,&parsed.mode,&parsed.ai,&parsed.laps,&extra,1u)!=5)return false;
    if(!Valid(parsed))return false;
    *request=parsed; return true;
}
inline bool Options(const Request& request,wchar_t* target,size_t capacity) {
    if(!Valid(request) || !target || capacity<2) return false;
    const wchar_t* mode=ModeName(request.mode);
    // PC MajorConnectRule::ApplyOptions consumes players and ais separately.
    // This creates an experimental local host; it does not establish racing,
    // AI movement, item rules, finish order or settlement acceptance.
    wchar_t base[512]{};int written=FlagMode(request.mode)?
        swprintf_s(base,L"game=/Script/KartDrift.FlagGameMode?gamemode=FlagSquad?team=2?teamsize=4?players=1?observers=0?ais=7"):
        swprintf_s(base,L"game=/Script/KartDrift.%sGameMode?gamemode=%s?team=%d?teamsize=%d?players=1?observers=0?ais=%d?lap=%d",GameModeClass(request.mode),mode,TeamCount(request.mode,request.ai+1),TeamSize(request.mode),request.ai,request.laps);
    size_t customSize=strlen(request.custom);if(written<=0 || static_cast<size_t>(written)+customSize>=capacity)return false;
    memcpy(target,base,static_cast<size_t>(written)*sizeof(wchar_t));
    for(size_t i=0;i<customSize;++i)target[written+i]=static_cast<unsigned char>(request.custom[i]);
    target[written+customSize]=0;return true;
}
}
