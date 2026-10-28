#include "../native/RaceControl.h"
#include <stdexcept>
#include <fstream>
#include <iterator>
void Need(bool value) { if(!value) throw std::runtime_error("Race request contract failed"); }
int main(int argc,char** argv) {
    TwinKartRace::Request request; const char* valid="TKRACE1 0123456789abcdef0123456789abcdef Pirate_R01 1 7 3\n";
    Need(TwinKartRace::Parse(valid,&request)); wchar_t options[512]{};
    Need(TwinKartRace::Options(request,options,512) && wcsstr(options,L"SpeedSingleGameMode") && wcsstr(options,L"players=1?observers=0?ais=7?lap=3"));
    request.mode=4; Need(TwinKartRace::Options(request,options,512) && wcsstr(options,L"ItemSingleGameMode"));
    request.ai=8; Need(!TwinKartRace::Options(request,options,512));
    Need(!TwinKartRace::Parse("TKRACE1 bad Pirate_R01 1 7 3",&request));
    Need(!TwinKartRace::Parse("TKRACE1 0123456789abcdef0123456789abcdef Pirate_R01?listen 1 7 3",&request));
    Need(!TwinKartRace::Parse("TKRACE1 0123456789abcdef0123456789abcdef Pirate_R01 2 7 3",&request));
    Need(!TwinKartRace::Parse("TKRACE1 0123456789abcdef0123456789abcdef Pirate_R01 1 7 3 extra",&request));
    Need(!TwinKartRace::Parse("TKRACE1 0123456789abcdef0123456789abcdef Pirate_R01 1 0 3",&request));
    if(argc==2) {
        std::ifstream file(argv[1],std::ios::binary); std::string text((std::istreambuf_iterator<char>(file)),{});
        Need(file.good() || file.eof()); Need(TwinKartRace::Parse(text.c_str(),&request) && request.mode==4 && request.ai==7 && request.laps==3 && !strcmp(request.track,"Pirate_R01"));
    }
    puts("PASS native race request: bounded syntax, supported modes, player/AI separation and no command injection.");
}
