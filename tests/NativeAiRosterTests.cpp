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
    std::array<std::wstring,8> stableCharacters{},stableKarts{};
    for(int slot=1;slot<=7;++slot) {
        TwinKartEquipment::Preset item;Need(roster.Get(slot,item));
        stableCharacters[slot]=item.character[0].data;stableKarts[slot]=item.kart[0].data;
    }
    Need(roster.RejectAndReplace(1,false,human,All,nullptr));
    TwinKartEquipment::Preset replacement;Need(roster.Get(1,replacement));
    Need(replacement.kart[0].data!=stableKarts[1] && std::wstring(replacement.kart[0].data)!=L"Kart:1" &&
        replacement.character[0].data==stableCharacters[1]);
    for(int slot=2;slot<=7;++slot) {
        TwinKartEquipment::Preset item;Need(roster.Get(slot,item));
        Need(item.character[0].data==stableCharacters[slot] && item.kart[0].data==stableKarts[slot]);
        Need(wcscmp(replacement.kart[0].data,item.kart[0].data)!=0);
    }
    Need(!roster.RejectAndReplace(0,false,human,All,nullptr) && !roster.RejectAndReplace(8,true,human,All,nullptr));
    Need(!roster.RejectAndReplace(2,true,human,Filter,&roster));
    Need(roster.Get(2,replacement) && replacement.character[0].data==stableCharacters[2]);
    Need(roster.RejectAndReplace(2,true,human,All,nullptr));Need(roster.Get(2,replacement));
    Need(replacement.character[0].data!=stableCharacters[2] && replacement.kart[0].data==stableKarts[2] &&
        std::wstring(replacement.character[0].data).substr(10)==std::wstring(replacement.character[1].data).substr(8));
    Need(roster.Load(text.data(),text.size())); // New catalog lifetime clears failed-resource exclusions.
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
    const wchar_t* roles[8]={L"Character:2",L"Character:2",L"Character:3",L"Character:3",L"Character:2",L"Character:2",L"Character:3",L"Character:3"};
    Need(roster.Get(1,saved));std::wstring savedKart=saved.kart[0].data;
    Need(roster.ApplyCharacterRoles(roles,NoMotions,nullptr));
    for(int slot=0;slot<8;++slot) {
        TwinKartEquipment::Preset outfit;outfit.kart[0]=human.kart[0];
        Need(roster.GetCharacter(slot,outfit) && !wcscmp(outfit.character[0].data,roles[slot]) && !wcscmp(outfit.kart[0].data,human.kart[0].data));
        Need(!outfit.character[2].data[0]);
    }
    Need(roster.Get(1,saved) && savedKart==saved.kart[0].data);
    roles[7]=L"Character:999";Need(!roster.ApplyCharacterRoles(roles,All,nullptr));
    Need(roster.GetCharacter(7,saved) && !wcscmp(saved.character[0].data,L"Character:3"));
    Need(roster.Prepare(7,human,NoMotions,nullptr) && !roster.GetCharacter(0,saved));
    const wchar_t* fixedCharacters[]={L"Character:2",L"Character:3",L"Character:4",L"Character:5"};
    const wchar_t* fixedKarts[]={L"Kart:6",L"Kart:7",L"Kart:8",L"Kart:9"};
    Need(roster.PrepareFixedRacers(fixedCharacters,fixedKarts,4,NoMotions,nullptr));
    for(int i=0;i<4;++i) {
        TwinKartEquipment::Preset fixed;
        Need(roster.GetFixedRacer(i,fixed) && !wcscmp(fixed.character[0].data,fixedCharacters[i]) && !wcscmp(fixed.kart[0].data,fixedKarts[i]));
        Need(!TwinKartEquipment::Nonempty(fixed.livery) && fixed.revision==0 && !TwinKartEquipment::Nonempty(fixed.character[2]));
    }
    Need(!roster.GetFixedRacer(4,saved));fixedKarts[3]=L"Kart:999";
    Need(!roster.PrepareFixedRacers(fixedCharacters,fixedKarts,4,All,nullptr));
    Need(roster.GetFixedRacer(3,saved) && !wcscmp(saved.kart[0].data,L"Kart:9"));
    Need(!roster.PrepareFixedRacers(fixedCharacters,fixedKarts,9,All,nullptr));
    puts("PASS AI roster: 100 races, distinct characters/karts, compatible costumes, player exclusion, per-slot variation and rejected-input rollback. No game started.");return 0;
}catch(const std::exception& e){puts(e.what());return 1;}}
