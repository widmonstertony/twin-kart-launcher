#include "../native/RaceControl.h"
#include "../native/NativeTrackModes.h"
#include "../native/NativeReturnRoute.h"
#include <stdexcept>
#include <fstream>
#include <iterator>
void Need(bool value) { if(!value) throw std::runtime_error("Race request contract failed"); }
static wchar_t ownedRoute[512];
static int routeCopies=0;
void* __fastcall AssignRoute(TwinKartRace::ReturnString* out,const TwinKartRace::ReturnString* in) {
    ++routeCopies;std::wmemcpy(ownedRoute,in->data,in->count);*out={ownedRoute,in->count,512};return out;
}
int main(int argc,char** argv) {
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
    for(int mode:{100,108,109,110,111,112,113,114,115,116,117,118}) {
        request.mode=mode;request.ai=7;Need(!TwinKartRace::Valid(request) && !TwinKartRace::ModeName(mode));
    }
    Need(!TwinKartRace::Parse("TKRACE1 0123456789abcdef0123456789abcdef Pirate_R01 1 7 3 extra",&request));
    Need(!TwinKartRace::Parse("TKRACE1 0123456789abcdef0123456789abcdef Pirate_R01 1 0 3",&request));
    if(argc==2) {
        std::ifstream file(argv[1],std::ios::binary); std::string text((std::istreambuf_iterator<char>(file)),{});
        Need(file.good() || file.eof()); Need(TwinKartRace::Parse(text.c_str(),&request) && request.mode==4 && request.ai==7 && request.laps==3 && !strcmp(request.track,"Pirate_R01"));
    }
    puts("PASS native race request: bounded syntax, supported modes, player/AI separation and no command injection.");
}
