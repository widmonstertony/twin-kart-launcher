#include "../native/NativeAiRoster.h"
#include <cstdio>
#include <stdexcept>
void Need(bool value){if(!value)throw std::runtime_error("AI roster contract failed");}
int All(const std::wstring&,void*){return 1;}
int NoMotions(const std::wstring& value,void*){return value.rfind(L"Motion:",0)==0?0:1;}
int Filter(const std::wstring& value,void* context){
    if(context)return -1;
    auto id=std::stoul(value.substr(value.find(':')+1));return id<=9?1:0;
}
int main(){try {
    std::string text="TKAI1\n";
    for(unsigned i=1;i<=16;++i) {
        text+="C Character:"+std::to_string(i)+" Costume:"+std::to_string(i)+" - - - -\n";
        text+="K Kart:"+std::to_string(i)+" - - - - - - - - -\n";
    }
    TwinKartRace::AiRoster roster;Need(roster.Load(text.data(),text.size()));
    TwinKartEquipment::Preset human;human.character[0]={L"Character:1",12,12};human.kart[0]={L"Kart:1",7,7};
    std::array<std::wstring,8> previousCharacters{},previousKarts{};
    for(int race=0;race<100;++race) {
        Need(roster.Prepare(7,human,All,nullptr));std::set<std::wstring> characters,karts;
        TwinKartEquipment::Preset outfit;Need(!roster.Get(0,outfit) && !roster.Get(8,outfit));
        for(int slot=1;slot<=7;++slot) {
            Need(roster.Get(slot,outfit));
            std::wstring character=outfit.character[0].data,kart=outfit.kart[0].data,costume=outfit.character[1].data;
            Need(character!=L"Character:1" && kart!=L"Kart:1" && characters.insert(character).second && karts.insert(kart).second);
            Need(character!=previousCharacters[slot] && kart!=previousKarts[slot]);
            Need(character.substr(character.find(':'))==costume.substr(costume.find(':')));
            previousCharacters[slot]=character;previousKarts[slot]=kart;
        }
    }
    unsigned c=0,k=0;Need(roster.Prepare(7,human,Filter,nullptr,&c,&k));Need(c==9 && k==9);
    TwinKartEquipment::Preset saved;Need(roster.Get(1,saved));std::wstring before=saved.kart[0].data;
    for(int slot=1;slot<=7;++slot) {
        TwinKartEquipment::Preset p;Need(roster.Get(slot,p));
        Need(Filter(p.character[0].data,nullptr)==1 && Filter(p.kart[0].data,nullptr)==1);
    }
    Need(!roster.Prepare(7,human,Filter,&roster));Need(roster.Get(1,saved));Need(before==saved.kart[0].data);
    Need(!roster.Prepare(7,human,nullptr,nullptr));
    Need(!roster.Prepare(8,human,All,nullptr));
    auto invalid=text+"C Character:1 Costume:1 - - - -\n";
    Need(!roster.Load(invalid.data(),invalid.size()));
    invalid=text+"K Kart:99 - - - - - - - - - trailing\n";Need(!roster.Load(invalid.data(),invalid.size()));
    invalid=text;invalid[4]=0;Need(!roster.Load(invalid.data(),invalid.size()));
    Need(!roster.Load("TKAI1\n",6));Need(roster.Prepare(7,human,All,nullptr));
    std::string optional="TKAI1\n";
    for(unsigned i=1;i<=9;++i) {
        optional+="C Character:"+std::to_string(i)+" Costume:"+std::to_string(i)+" Motion:1 Motion:2 Motion:3 Motion:4\n";
        optional+="K Kart:"+std::to_string(i)+" - - - - - - - - -\n";
    }
    Need(roster.Load(optional.data(),optional.size()));
    TwinKartRace::AiRoster::AvailabilityStats stats;
    Need(roster.Prepare(7,human,NoMotions,nullptr,&c,&k,&stats));Need(c==9 && k==9 && stats.clearedMotions==36);
    for(int slot=1;slot<=7;++slot) {
        TwinKartEquipment::Preset outfit;Need(roster.Get(slot,outfit));
        for(unsigned field=2;field<6;++field)Need(!TwinKartEquipment::Nonempty(outfit.character[field]));
    }
    puts("PASS AI roster: 100 races, distinct characters/karts, compatible costumes, player exclusion, per-slot variation and rejected-input rollback. No game started.");return 0;
}catch(const std::exception& e){puts(e.what());return 1;}}
