#pragma once
#include "RaceControl.h"
namespace TwinKartRace {
enum class EventRoster { Ordinary,Dao,Brodi,MisoTiera,HitRun,Derek,MayorZipi,TanyaSpirit,RandomSkill };
struct EventRules {const char* profile;int mode;const char* options;EventRoster roster;};
// These existing native families were previously available only through the
// custom-room mode picker. Include each native solo/duo/squad identity in the
// lobby as well, without inventing new rule profiles or unsupported enum117.
inline const int* LocalLobbyModes(size_t& count) {
    static const int modes[]={101,102,103,104,105,106,107,110,111,112,113,114,115,116,118};
    count=sizeof(modes)/sizeof(modes[0]);return modes;
}
// Independently described compatibility rules from the original published
// events. These are gameplay values for FGameSettingOptions, not copied
// proprietary rule implementations or external launcher source.
inline const EventRules* EventProfiles(size_t& count) {
    static const EventRules profiles[]={
        {"speed_solo_ghost",113,"?game.ignorealways=true?game.useskill=false?game.draft=false",EventRoster::Ordinary},
        {"speed_solo_derek",113,"?game.ignorealways=true?game.useskill=true?game.leadcompensation=false?game.draft=false?game.showskillinfo=true",EventRoster::Derek},
        {"speed_squad_skill",115,"?game.useskill=true?game.skill.activesp=80p?game.draft=false?game.showskillinfo=true",EventRoster::Ordinary},
        {"item_squad_miso_tiera",112,"?game.useskill=true?game.skill.activesp=50p?game.showskillinfo=true",EventRoster::MisoTiera},
        {"item_solo_brodi",110,"?game.useskill=true?game.skill.activesp=80p?game.showskillinfo=true",EventRoster::Brodi},
        {"s062_speed_solo_dao",113,"?game.useskill=true?game.showskillinfo=true",EventRoster::Dao},
        {"s062_item_squad_runner_attacker",112,"?game.useskill=true?game.showskillinfo=true",EventRoster::HitRun},
        {"s062_spitem_solo_skill",105,"?game.useskill=true?game.showskillinfo=true",EventRoster::Ordinary},
        {"s062_spitem_squad_skill",107,"?game.useskill=true?game.showskillinfo=true",EventRoster::Ordinary},
        {"s062_flag_skill",104,"?game.useskill=true?game.showskillinfo=true",EventRoster::Ordinary},
        {"cubecubeitem_solo",116,"?game.useitem=true?game.itemcube=true?game.useskill=false",EventRoster::Ordinary},
        {"cubecubeitem_squad",118,"?game.useitem=true?game.itemcube=true?game.useskill=false",EventRoster::Ordinary}
    };
    count=sizeof(profiles)/sizeof(profiles[0]);return profiles;
}
inline const EventRules* EventProfile(const char* profile) {
    size_t count=0;auto rules=EventProfiles(count);
    if(profile)for(size_t i=0;i<count;++i)if(!strcmp(profile,rules[i].profile))return rules+i;
    return nullptr;
}
inline bool NativeCharacterRules(const char* profile) {
    auto rule=EventProfile(profile);return rule && rule->roster!=EventRoster::Ordinary;
}
inline const wchar_t* EventCharacter(const EventRules& rule,int slot,int racers) {
    if(slot<0 || slot>=racers || racers<2 || racers>8)return nullptr;
    switch(rule.roster) {
    case EventRoster::Dao:return L"Character:100000001";
    case EventRoster::Brodi:return L"Character:100000003";
    case EventRoster::Derek:return L"Character:100000025";
    case EventRoster::MisoTiera:return SlotTeam(rule.mode,slot,racers)==1?L"Character:100000043":L"Character:100000006";
    case EventRoster::HitRun: {
        if(racers!=8)return nullptr;
        // Exact original character IDs verified against the client's English
        // localization; team membership follows the official event roster.
        const wchar_t* red[]={L"Character:100000043",L"Character:100000013",L"Character:100000037",L"Character:100000017"};
        const wchar_t* blue[]={L"Character:100000001",L"Character:100000025",L"Character:100000005",L"Character:100000027"};
        return SlotTeam(rule.mode,slot,racers)==1?red[slot/2]:blue[slot/2];
    }
    default:return nullptr;
    }
}
// Find an exact key in a request that already passed ValidCustomOptions.
inline const char* FindEventOption(const char* options,const char* key,size_t keySize,size_t& valueSize) {
    for(const char* at=options;at && *at;) {
        if(*at++!='?')return nullptr;auto equals=strchr(at,'=');if(!equals)return nullptr;
        const char* end=strchr(equals+1,'?');if(!end)end=equals+strlen(equals);
        if(static_cast<size_t>(equals-at)==keySize && !_strnicmp(at,key,keySize)) {valueSize=static_cast<size_t>(end-equals-1);return equals+1;}
        at=end;
    }
    return nullptr;
}
inline bool AppendEventRules(const Request& request,wchar_t* output,size_t capacity,size_t& written) {
    auto rule=EventProfile(request.profile);if(!rule)return true;
    if(rule->mode!=request.mode || !ValidCustomOptions(rule->options))return false;
    for(const char* at=rule->options;*at;) {
        auto equals=strchr(at+1,'=');if(!equals)return false;
        const char* end=strchr(equals+1,'?');if(!end)end=equals+strlen(equals);
        size_t keySize=static_cast<size_t>(equals-at-1),valueSize=0;
        auto custom=FindEventOption(request.custom,at+1,keySize,valueSize);
        if(custom) {if(valueSize!=static_cast<size_t>(end-equals-1) || _strnicmp(custom,equals+1,valueSize))return false;}
        else {
            size_t size=static_cast<size_t>(end-at);if(written+size>=capacity)return false;
            for(size_t i=0;i<size;++i)output[written++]=static_cast<unsigned char>(at[i]);output[written]=0;
        }
        at=end;
    }
    return true;
}
}
