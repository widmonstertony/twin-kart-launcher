#include "../native/RaceControl.h"
#include "../native/NativeTrackModes.h"
#include "../native/NativeReturnRoute.h"
#include "../native/NativeRacePreparation.h"
#include "../native/NativeModeBinding.h"
#include "../native/NativeEventMenu.h"
#include "../native/NativeCubeWorld.h"
#include <stdexcept>
#include <fstream>
#include <iterator>
void Need(bool value) { if(!value) throw std::runtime_error("Race request contract failed"); }
static wchar_t ownedRoute[512];
static int routeCopies=0;
static unsigned nativePreparations=0;
bool InspectFixture(void* asset,TwinKartRace::ModeDefinition& out,void*) {
    if(!asset)return false;out=*static_cast<TwinKartRace::ModeDefinition*>(asset);return true;
}
void ModeBindingChecks() {
    using namespace TwinKartRace;
    wchar_t cubeObject[256]{};
    auto cube=CubeWorldForTrack(116,L"Desert_I06");Need(cube && CubeWorldObjectPath(*cube,cubeObject,256));
    Need(!wcscmp(cubeObject,L"/Game/ART/Env/Stage_ModeLab/CubeCube/Cube_Desert_I06/Cube_Desert_I06.Cube_Desert_I06"));
    Need(CubeWorldForTrack(118,L"Desert_I06")==cube && !CubeWorldObjectPath(*cube,cubeObject,12));
    Need(!CubeWorldForTrack(4,L"Desert_I06") && !CubeWorldForTrack(117,L"Desert_I06") && !CubeWorldForTrack(116,L"Village_I04"));
    Need(!CubeWorldForTrack(116,L"Cube_Desert_I06") && !CubeWorldForTrack(116,L"Desert_I06_Mirror"));
    Need(EventTrackSupports(1,116,L"Village_I05") && EventTrackSupports(1,118,L"Village_I05"));
    Need(EventTrackSupports(1,116,L"World_I11") && EventTrackSupports(1,118,L"World_I11") && !EventTrackSupports(1,4,L"World_I11"));
    Need(!EventTrackSupports(1,4,L"Village_I05") && !EventTrackSupports(1,116,L"Desert_I06") && !EventTrackSupports(2,116,L"Village_I04"));
    Need(NativeCharacterRules("s062_speed_solo_dao") && NativeCharacterRules("item_squad_miso_tiera"));
    Need(!NativeCharacterRules("speed_solo_ghost") && !NativeCharacterRules("") && !NativeCharacterRules(nullptr));
    EventMenuCopySource menu;Need(!menu.Build(nullptr));Need(menu.Build(&menu));
    Need(menu.source.rows.count==12 && menu.source.rows.capacity==12 && menu.source.baseVtable==&menu);
    for(int index=0;index<menu.source.rows.count;++index) {
        auto& row=menu.source.rows.data[index];Need(row.active==1 && row.index==index+1 && row.sort==index+1 && row.modes.count==1);
        auto& mode=row.modes.data[0];char profile[97]{};for(int n=0;n<mode.profile.count-1;++n)profile[n]=static_cast<char>(mode.profile.data[n]);
        auto rule=EventProfile(profile);Need(rule && rule->mode==mode.mode && row.noParty==(TeamSize(mode.mode)==1?1:0));
        Need(!wcsncmp(row.id.data,L"TwinKart_",9) && !wcscmp(row.id.data+9,mode.profile.data));
        Need(!row.hours.count && !row.legacyModes.count && !wcscmp(row.start.data,L"2000/1/1 0:00:00"));
    }
    Request request{};strcpy_s(request.identity,"0123456789abcdef0123456789abcdef");strcpy_s(request.track,"Pirate_R01");
    request.mode=110;request.ai=7;request.laps=3;strcpy_s(request.profile,"item_solo_brodi");
    ModeDefinition definition{};definition.asset=&definition;definition.kind=0;definition.teamSize=1;definition.teamCount=8;
    wcscpy_s(definition.gameClass,L"/Script/KartDrift.SkillSingleGameMode");wcscpy_s(definition.travelName,L"ItemSingle");
    wchar_t profile[]=L"item_solo_brodi",fallback[]=L"ItemLabSolo";
    ModeMapEntry entries[]={{{fallback,12,12},&definition,0,0},{{profile,16,16},&definition,0,0}};
    ModeDefinition resolved{};wchar_t options[512]{};
    Need(ResolveModeBinding(request,entries,2,InspectFixture,nullptr,resolved));
    Need(ModeOptions(request,resolved,options,512) && wcsstr(options,L"SkillSingleGameMode?gamemode=ItemSingle") && wcsstr(options,L"?gameprofile=item_solo_brodi"));
    Need(wcsstr(options,L"?racemode=110?gameprofile=item_solo_brodi"));
    Need(wcsstr(options,L"?game.useskill=true?game.skill.activesp=80p") && ValidCustomOptions("?game.skill.activesp=80p?game.useskill=true"));
    strcpy_s(request.custom,"?game.useskill=false");Need(!ModeOptions(request,resolved,options,512));
    strcpy_s(request.custom,"?game.useskill=true");Need(ModeOptions(request,resolved,options,512));request.custom[0]=0;
    // Native room controls must affect gameplay and cannot bypass mandatory
    // event rules by spelling the same setting in the UI namespace.
    strcpy_s(request.custom,"?item.UseSkill=false");Need(!ModeOptions(request,resolved,options,512));
    strcpy_s(request.custom,"?item.ActiveSkillCoolTimePercentage=20?item.UseSkill=true");
    Need(ModeOptions(request,resolved,options,512) && wcsstr(options,L"?game.skill.activeinitialsp=80p"));
    strcpy_s(request.custom,"?item.ActiveSkillCoolTimePercentage=50");Need(!ModeOptions(request,resolved,options,512));
    Request converted=request;request.profile[0]=0;
    strcpy_s(request.custom,"?item.ActiveSkillCoolTime=false?item.SkillPercentage=false");
    Need(NormalizeUiOptions(request,converted) && strstr(converted.custom,"?game.skill.activesp=1?game.skill.activeinitialsp=0") && strstr(converted.custom,"?game.skill.percentage100=true"));
    strcpy_s(request.custom,"?item.ActiveSkillCoolTimePercentage=101");Need(!NormalizeUiOptions(request,converted));
    strcpy_s(request.custom,"?item.UseSkill=maybe");Need(!NormalizeUiOptions(request,converted));
    request.mode=113;strcpy_s(request.custom,"?speed.Collision=false?speed.Draft=false?speed.TeamBooster=true");
    Need(NormalizeUiOptions(request,converted) && strstr(converted.custom,"?game.ignorealways=true") && strstr(converted.custom,"?game.draft=false") && strstr(converted.custom,"?game.teambooster=true"));
    strcpy_s(request.custom,"?speed.Collision=false?game.ignorealways=false");Need(!NormalizeUiOptions(request,converted));
    request.mode=110;strcpy_s(request.profile,"item_solo_brodi");request.custom[0]=0;
    Need(!ValidCustomOptions("?game.command=quit") && !ValidCustomOptions("?game.skill.exe=bad"));
    auto hitRun=EventProfile("s062_item_squad_runner_attacker");Need(hitRun && !wcscmp(EventCharacter(*hitRun,0,8),L"Character:100000043") && !wcscmp(EventCharacter(*hitRun,7,8),L"Character:100000027"));
    strcpy_s(request.profile,"unknown");Need(!ResolveModeBinding(request,entries,2,InspectFixture,nullptr,resolved));
    request.profile[0]=0;Need(ResolveModeBinding(request,entries,2,InspectFixture,nullptr,resolved));
    Need(ModeOptions(request,resolved,options,512) && !wcsstr(options,L"?gameprofile="));
    entries[1]=entries[0];Need(!ResolveModeBinding(request,entries,2,InspectFixture,nullptr,resolved));
    Need(!ResolveModeBinding(request,entries,513,InspectFixture,nullptr,resolved));
    definition.teamSize=2;Need(!ResolveModeBinding(request,entries,1,InspectFixture,nullptr,resolved));definition.teamSize=1;
    definition.teamCount=4;Need(!ResolveModeBinding(request,entries,1,InspectFixture,nullptr,resolved));definition.teamCount=8;
    wcscpy_s(definition.travelName,L"SpeedSingle");Need(!ResolveModeBinding(request,entries,1,InspectFixture,nullptr,resolved));
    wcscpy_s(definition.travelName,L"ItemSingle");
    wcscpy_s(definition.gameClass,L"/Script/KartDrift.SkillSingleGameMode?listen");Need(!ResolveModeBinding(request,entries,1,InspectFixture,nullptr,resolved));
    request.mode=112;definition.teamSize=4;definition.teamCount=2;
    wcscpy_s(definition.gameClass,L"/Script/KartDrift.SkillTeamGameMode");wcscpy_s(definition.travelName,L"ItemTeam");
    Need(ModeOptions(request,definition,options,512) && wcsstr(options,L"SkillTeamGameMode?gamemode=ItemTeam?team=2?teamsize=4"));
    Need(!ModeOptions(request,definition,options,8));
    request.mode=116;CanonicalModeProfile(request);Need(!strcmp(request.profile,"cubecubeitem_solo"));
    request.mode=118;request.profile[0]=0;CanonicalModeProfile(request);Need(!strcmp(request.profile,"cubecubeitem_squad"));
    request.mode=110;strcpy_s(request.profile,"s062_item_solo_brodi");CanonicalModeProfile(request);Need(!strcmp(request.profile,"item_solo_brodi"));
    request.mode=113;strcpy_s(request.profile,"s062_item_solo_brodi");CanonicalModeProfile(request);Need(!strcmp(request.profile,"s062_item_solo_brodi"));
}
void* __fastcall CopyPreparation(TwinKartRace::PreparationString* out,const TwinKartRace::PreparationString* in) {
    *out={new wchar_t[in->count],in->count,in->count};std::wmemcpy(out->data,in->data,in->count);return out;
}
bool __fastcall InitializePreparation(void* traveler,const TwinKartRace::PreparationMode* mode,TwinKartRace::PreparationString* track,void* details,const TwinKartRace::PreparationHints* hints,bool random) {
    Need(traveler==&nativePreparations && mode->mode==4 && !wcscmp(mode->profile.data,L"item_solo_test") &&
        !wcscmp(track->data,L"Pirate_R01") && !details && !hints->count && !hints->data && !random);
    ++nativePreparations;delete[] track->data;return true;
}
void* __fastcall AssignRoute(TwinKartRace::ReturnString* out,const TwinKartRace::ReturnString* in) {
    ++routeCopies;std::wmemcpy(ownedRoute,in->data,in->count);*out={ownedRoute,in->count,512};return out;
}
int main(int argc,char** argv) {
    ModeBindingChecks();
    wchar_t lobbyText[]=L"/Game/Test/Lobby";
    TwinKartRace::ReturnString lobby{lobbyText,static_cast<int>(sizeof(lobbyText)/sizeof(wchar_t)),static_cast<int>(sizeof(lobbyText)/sizeof(wchar_t))},route{};
    Need(TwinKartRace::PrepareReturnRoute(route,lobby,false,AssignRoute)==-1 && routeCopies==0);
    Need(TwinKartRace::PrepareReturnRoute(route,lobby,true,AssignRoute)==2 && routeCopies==1 && route.data!=lobby.data);
    Need(TwinKartRace::PrepareReturnRoute(route,lobby,true,AssignRoute)==1 && routeCopies==1);
    lobbyText[1]=L'X';Need(route.data[1]==L'G'); // Native-owned copy survives source changes/travel.
    TwinKartRace::ReturnString empty{},broken{lobbyText,600,600};
    Need(TwinKartRace::PrepareReturnRoute(empty,broken,true,AssignRoute)==-1 && !empty.data && routeCopies==1);
    broken={lobbyText,4,2};Need(!TwinKartRace::HasReturnRoute(broken));
    Need(TwinKartRace::PrepareReturnRoute(empty,empty,true,AssignRoute)==-1 && routeCopies==1);
    // Native byte-enum array, independent of track name conventions. Check
    // malformed records leave the caller's existing evidence unchanged.
    unsigned char definition[0x160]{};unsigned char types[]={1,2};
    struct Array {const unsigned char* data;int count,capacity;} array{types,2,2};
    memcpy(definition+0x18,&array,sizeof(array));unsigned mask=99;
    Need(TwinKartRace::ReadNativeTrackModeMask(definition,sizeof(definition),mask) && mask==6);
    Need(TwinKartRace::NativeTrackSupports(mask,1) && TwinKartRace::NativeTrackSupports(mask,4));
    types[0]=0;Need(TwinKartRace::ReadNativeTrackModeMask(definition,sizeof(definition),mask));
    Need(TwinKartRace::NativeTrackSupports(mask,1) && !TwinKartRace::NativeTrackSupports(mask,4));
    types[0]=3;types[1]=4;Need(TwinKartRace::ReadNativeTrackModeMask(definition,sizeof(definition),mask));
    Need(!TwinKartRace::NativeTrackSupports(mask,1) && !TwinKartRace::NativeTrackSupports(mask,4));
    types[1]=5;mask=99;Need(!TwinKartRace::ReadNativeTrackModeMask(definition,sizeof(definition),mask) && mask==99);
    array.count=3;memcpy(definition+0x18,&array,sizeof(array));
    Need(!TwinKartRace::ReadNativeTrackModeMask(definition,sizeof(definition),mask));
    array={nullptr,1,1};memcpy(definition+0x18,&array,sizeof(array));
    Need(!TwinKartRace::ReadNativeTrackModeMask(definition,sizeof(definition),mask));
    array={nullptr,0,0};memcpy(definition+0x18,&array,sizeof(array));
    Need(TwinKartRace::ReadNativeTrackModeMask(definition,sizeof(definition),mask) && mask==0);
    Need(!TwinKartRace::ReadNativeTrackModeMask(definition,0x27,mask) && !TwinKartRace::NativeTrackSupports(0xffffffff,1));
    TwinKartRace::Request request; const char* valid="TKRACE1 0123456789abcdef0123456789abcdef Pirate_R01 1 7 3\n";
    Need(TwinKartRace::Parse(valid,&request)); wchar_t options[512]{};
    Need(TwinKartRace::Options(request,options,512) && wcsstr(options,L"SpeedSingleGameMode") && wcsstr(options,L"players=1?observers=0?ais=7?lap=3"));
    request.mode=4; Need(TwinKartRace::Options(request,options,512) && wcsstr(options,L"ItemSingleGameMode"));
    request.ai=8; Need(!TwinKartRace::Options(request,options,512));
    Need(!TwinKartRace::Parse("TKRACE1 bad Pirate_R01 1 7 3",&request));
    Need(!TwinKartRace::Parse("TKRACE1 0123456789abcdef0123456789abcdef Pirate_R01?listen 1 7 3",&request));
    Need(TwinKartRace::Parse("TKRACE1 0123456789abcdef0123456789abcdef Pirate_R01 2 7 3",&request));
    Need(TwinKartRace::Options(request,options,512) && wcsstr(options,L"SpeedTeamGameMode") && wcsstr(options,L"team=4?teamsize=2"));
    request.mode=6;Need(TwinKartRace::Options(request,options,512) && wcsstr(options,L"ItemTeamGameMode") && wcsstr(options,L"team=2?teamsize=4"));
    for(int mode=1;mode<=6;++mode) {
        Need(TwinKartRace::NativeTrackSupports(6,mode));
        Need(!TwinKartRace::NativeTrackSupports(mode<=3?2:4,mode));
        request.mode=mode;request.ai=7;Need(TwinKartRace::Valid(request));
        int counts[5]{};
        for(int slot=0;slot<8;++slot) {int team=TwinKartRace::SlotTeam(mode,slot,8);Need(team>=0 && team<=4);++counts[team];}
        int size=TwinKartRace::TeamSize(mode);
        if(size==1)Need(counts[0]==8);
        else for(int team=1;team<=8/size;++team)Need(counts[team]==size);
        Need(TwinKartRace::SlotTeam(mode,-1,8)==-1 && TwinKartRace::SlotTeam(mode,8,8)==-1);
    }
    request.mode=3;request.ai=3;Need(!TwinKartRace::Valid(request));
    request.mode=2;request.ai=2;Need(!TwinKartRace::Valid(request));
    request.mode=2;request.ai=3;Need(TwinKartRace::Valid(request));
    request.mode=104;request.ai=7;Need(TwinKartRace::Valid(request));
    Need(TwinKartRace::Options(request,options,512) && wcsstr(options,L"FlagGameMode?gamemode=FlagSquad?team=2?teamsize=4?players=1?observers=0?ais=7") && !wcsstr(options,L"?lap="));
    Need(TwinKartRace::NativeTrackSupports(16,104) && !TwinKartRace::NativeTrackSupports(6,104));
    Need(TwinKartRace::SlotTeam(104,0,8)==1 && TwinKartRace::SlotTeam(104,1,8)==2 && TwinKartRace::SlotTeam(104,7,8)==2);
    request.ai=3;Need(!TwinKartRace::Valid(request));
    for(int mode=105;mode<=107;++mode) {
        request.mode=mode;request.ai=7;
        Need(TwinKartRace::Valid(request) && TwinKartRace::Options(request,options,512));
        Need(wcsstr(options,mode==105?L"SpitemSingleGameMode":L"SpitemTeamGameMode"));
        Need(TwinKartRace::NativeTrackSupports(8,mode) && !TwinKartRace::NativeTrackSupports(6,mode) && !TwinKartRace::NativeTrackSupports(16,mode));
        Need(TwinKartRace::BehaviorGetter(mode)==(mode==105?0x220fed0u:0x22174e0u));
        Need(TwinKartRace::ResultFactory(mode)==(mode==105?0x168dbc0u:0x168db70u));
        request.ai=3;Need(TwinKartRace::Valid(request)==(mode!=107));
    }
    for(int mode=101;mode<=103;++mode) {
        request.mode=mode;request.ai=7;Need(TwinKartRace::Valid(request));
        Need(TwinKartRace::Options(request,options,512) && wcsstr(options,L"game=/Script/KartDrift.ModeLabGameMode?gamemode=InfinityBooster?"));
        Need(TwinKartRace::NativeTrackSupports(4,mode) && !TwinKartRace::NativeTrackSupports(2,mode) && !TwinKartRace::NativeTrackSupports(16,mode));
        Need(TwinKartRace::SpeedMode(mode) && TwinKartRace::BehaviorGetter(mode)==(mode==101?0x2218050u:0x2218390u));
        request.ai=3;Need(TwinKartRace::Valid(request)==(mode!=103));
    }
    for(int mode:{110,111,112,113,114,115,116,118}) {
        request.mode=mode;request.ai=7;Need(TwinKartRace::Valid(request) && TwinKartRace::ModeName(mode));
        Need(TwinKartRace::NativeTrackSupports(TwinKartRace::SpeedMode(mode)?4:2,mode));
        Need(TwinKartRace::TeamSize(mode)==((mode==116 || mode==110 || mode==113)?1:(mode==111 || mode==114)?2:4));
    }
    for(int mode:{100,108,109,117,119,120,121,122}) {
        request.mode=mode;request.ai=7;Need(!TwinKartRace::Valid(request) && !TwinKartRace::ModeName(mode));
    }
    Need(!TwinKartRace::Parse("TKRACE1 0123456789abcdef0123456789abcdef Pirate_R01 1 7 3 extra",&request));
    Need(!TwinKartRace::Parse("TKRACE1 0123456789abcdef0123456789abcdef Pirate_R01 1 0 3",&request));
    Need(TwinKartRace::Parse("TKRACE2 0123456789abcdef0123456789abcdef Pirate_R01 4 7 3 item_solo_test ?item.IgnoreFaraway=false?item.UseSkill=true?item.ActiveSkillCoolTime=false?item.CustomItem=Invalid",&request));
    Need(TwinKartRace::Options(request,options,512) && wcsstr(options,L"?item.UseSkill=true") && wcsstr(options,L"?item.ActiveSkillCoolTime=false"));
    Need(TwinKartRace::PrepareNativeRace(&nativePreparations,request,CopyPreparation,InitializePreparation) && nativePreparations==1);
    Need(!TwinKartRace::PrepareNativeRace(nullptr,request,CopyPreparation,InitializePreparation) && nativePreparations==1);
    for(const char* bad:{"?game=OtherGameMode","?item.UseSkill=1?ITEM.UseSkill=0","?item.UseSkill=","?item.CustomItem=foo/bar","?item.CustomItem=x;quit","?item.CustomItem=x extra","?players=99"}) {
        char text[1025]{};sprintf_s(text,"TKRACE2 0123456789abcdef0123456789abcdef Pirate_R01 4 7 3 - %s",bad);
        Need(!TwinKartRace::Parse(text,&request));
    }
    Need(TwinKartRace::Parse("TKRACE2 0123456789abcdef0123456789abcdef Pirate_R01 4 7 3 - -",&request) && !request.profile[0] && !request.custom[0]);
    Need(!TwinKartRace::Parse("TKRACE2 0123456789abcdef0123456789abcdef Pirate_R01 4 7 3 bad/profile -",&request));
    if(argc==2) {
        std::ifstream file(argv[1],std::ios::binary); std::string text((std::istreambuf_iterator<char>(file)),{});
        Need(file.good() || file.eof()); Need(TwinKartRace::Parse(text.c_str(),&request) && request.mode==4 && request.ai==7 && request.laps==3 && !strcmp(request.track,"Pirate_R01"));
    }
    puts("PASS native race request: bounded syntax, supported modes, player/AI separation and no command injection.");
}
