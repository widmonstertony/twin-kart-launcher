#pragma once
namespace TwinKartRace {
// Exact PC ERaceMode metadata and the original solo/duo/squad data assets.
// Additional event enums exist; they require their own rule adapters.
inline bool StandardMode(int mode) { return mode>=1 && mode<=6; }
inline bool SpitemMode(int mode) { return mode>=105 && mode<=107; }
inline bool InfiniteBoostMode(int mode) { return mode>=101 && mode<=103; }
inline bool FlagMode(int mode) { return mode==104; }
// Spitem has distinct native game modes and result stylers, but shares the
// RaceAiPawn/MajorConnect lifecycle. Flag and arena modes do not.
inline bool CircuitMode(int mode) { return StandardMode(mode) || SpitemMode(mode) || InfiniteBoostMode(mode); }
inline bool PlayableMode(int mode) { return CircuitMode(mode) || FlagMode(mode); }
inline bool SpeedMode(int mode) { return (mode>=1 && mode<=3) || InfiniteBoostMode(mode); }
inline int TeamSize(int mode) {
    if(FlagMode(mode))return 4;
    if(!CircuitMode(mode))return 0;
    int group=SpitemMode(mode)?mode-105:InfiniteBoostMode(mode)?mode-101:(mode-1)%3;return group==0?1:group==1?2:4;
}
inline bool TeamMode(int mode) { return TeamSize(mode)>1; }
inline int TeamCount(int mode,int racers) {
    int size=TeamSize(mode);
    return size>1 && racers>=size*2 && racers<=8 && racers%size==0?racers/size:0;
}
inline bool PopulationValid(int mode,int ai) {
    if(FlagMode(mode))return ai==7;
    return CircuitMode(mode) && ai>=1 && ai<=7 && (!TeamMode(mode) || TeamCount(mode,ai+1)>0);
}
inline int SlotTeam(int mode,int slot,int racers) {
    if(!PlayableMode(mode) || slot<0 || slot>=racers)return -1;
    if(!TeamMode(mode))return 0;
    int count=TeamCount(mode,racers);return count?1+slot%count:-1;
}
inline const wchar_t* ModeName(int mode) {
    if(FlagMode(mode))return L"FlagSquad";
    if(!CircuitMode(mode))return nullptr;
    if(InfiniteBoostMode(mode))return L"InfinityBooster";
    if(SpitemMode(mode))return TeamMode(mode)?L"SpitemTeam":L"SpitemSingle";
    return SpeedMode(mode)?(TeamMode(mode)?L"SpeedTeam":L"SpeedSingle"):
        (TeamMode(mode)?L"ItemTeam":L"ItemSingle");
}
// Native InfiniteBooster data assets name the rules InfinityBooster while
// selecting AModeLabGameMode. Travel identity and UClass are not synonymous.
inline const wchar_t* GameModeClass(int mode) { return FlagMode(mode)?L"Flag":InfiniteBoostMode(mode)?L"ModeLab":ModeName(mode); }
inline unsigned BehaviorGetter(int mode) {
    if(!CircuitMode(mode))return 0;
    return SpeedMode(mode)?(TeamMode(mode)?0x2218390:0x2218050):
        (TeamMode(mode)?0x22174e0:0x220fed0);
}
inline unsigned ResultFactory(int mode) {
    if(!CircuitMode(mode))return 0;
    if(SpitemMode(mode))return TeamMode(mode)?0x168db70:0x168dbc0;
    return SpeedMode(mode)?(TeamMode(mode)?0x168da30:0x168da80):
        (TeamMode(mode)?0x168dad0:0x168db20);
}
}
