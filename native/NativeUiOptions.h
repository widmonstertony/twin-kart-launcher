#pragma once
#include "RaceControl.h"
#include <cstdlib>
namespace TwinKartRace {
// The original room settings and in-world options are separate interfaces.
// These mappings describe the observed FItemGameOption/FSpeedGameOption
// conversions. Preserve the UI values for metadata; add only explicit values,
// never parser defaults that could override a selected event profile.
inline bool UiOption(const char* options,const char* key,char (&value)[97]) {
    for(const char* at=options;at && *at;) {
        auto equals=strchr(at+1,'=');if(!equals)return false;
        auto end=strchr(equals+1,'?');if(!end)end=equals+strlen(equals);
        if(static_cast<size_t>(equals-at-1)==strlen(key) && !_strnicmp(at+1,key,strlen(key))) {
            size_t n=static_cast<size_t>(end-equals-1);if(n>96)return false;
            memcpy(value,equals+1,n);value[n]=0;return true;
        }
        at=end;
    }
    return false;
}
inline bool UiBoolean(const char* value,bool& result) {
    if(!_stricmp(value,"true") || !strcmp(value,"1")){result=true;return true;}
    if(!_stricmp(value,"false") || !strcmp(value,"0")){result=false;return true;}
    return false;
}
inline bool AddUiOption(Request& result,const char* key,const char* value) {
    char previous[97]{};
    if(UiOption(result.custom,key,previous)) {
        bool a=false,b=false;
        return (UiBoolean(previous,a) && UiBoolean(value,b))?a==b:!_stricmp(previous,value);
    }
    size_t n=strlen(result.custom),extra=strlen(key)+strlen(value)+2;
    if(n+extra>=sizeof(result.custom))return false;
    if(sprintf_s(result.custom+n,sizeof(result.custom)-n,"?%s=%s",key,value)<=0)return false;
    return ValidCustomOptions(result.custom);
}
inline bool NormalizeUiOptions(const Request& request,Request& result) {
    if(!Valid(request))return false;result=request;
    const bool speed=SpeedMode(request.mode);
    struct Mapping {const char* ui;const char* game;bool invert;};
    const Mapping item[]={
        {"item.IgnoreFaraway","game.ignorefaraway",false},
        {"item.UseSkill","game.useskill",false},
        {"item.SkillPercentage","game.skill.percentage100",true}
    };
    const Mapping racing[]={
        {"speed.IgnoreFaraway","game.ignorefaraway",false},
        {"speed.TeamBooster","game.teambooster",false},
        {"speed.Draft","game.draft",false},
        {"speed.Collision","game.ignorealways",true}
    };
    auto mappings=speed?racing:item;size_t count=speed?4:3;
    // Flag has its own settings contract; do not apply item defaults to it.
    if(FlagMode(request.mode))return true;
    for(size_t i=0;i<count;++i) {
        char value[97]{};bool enabled=false;
        if(!UiOption(request.custom,mappings[i].ui,value))continue;
        if(!UiBoolean(value,enabled) || !AddUiOption(result,mappings[i].game,
            enabled!=mappings[i].invert?"true":"false"))return false;
    }
    if(speed)return true;
    char cooldown[97]{},percentage[97]{},custom[97]{};bool enabled=true;
    bool hasCooldown=UiOption(request.custom,"item.ActiveSkillCoolTime",cooldown);
    bool hasPercentage=UiOption(request.custom,"item.ActiveSkillCoolTimePercentage",percentage);
    if(hasCooldown && !UiBoolean(cooldown,enabled))return false;
    if(!enabled) {
        // Original disabled-cooldown conversion: SP cost1, initialSP0.
        if(!AddUiOption(result,"game.skill.activesp","1") || !AddUiOption(result,"game.skill.activeinitialsp","0"))return false;
    } else if(hasPercentage) {
        for(const char* p=percentage;*p;++p)if(*p<'0' || *p>'9')return false;
        if(strlen(percentage)>3)return false;
        int reduction=atoi(percentage);if(reduction>100)return false;
        char amount[8]{};sprintf_s(amount,"%dp",100-reduction);
        if(!AddUiOption(result,"game.skill.activesp",amount) || !AddUiOption(result,"game.skill.activeinitialsp",amount))return false;
    }
    if(UiOption(request.custom,"item.CustomItem",custom) && !AddUiOption(result,"game.customitem",custom))return false;
    return true;
}
}
