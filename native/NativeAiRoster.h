#pragma once
#include "NativeEquipmentPreset.h"
#include <bcrypt.h>
#include <array>
#include <vector>
#include <string>
#include <sstream>
#include <set>
#include <utility>
#include <map>

namespace TwinKartRace {
// Private, session-local catalog: resource identifiers only. The original
// engine still loads models, animates, drives, distributes items and scores.
class AiRoster {
    using Character=std::array<std::wstring,6>;
    using Kart=std::array<std::wstring,10>;
    std::vector<Character> characters;
    std::vector<Kart> karts;
    struct Outfit {Character character;Kart kart;};
    std::array<Outfit,8> current{};
    int opponents=0;
    static bool Identifier(const std::string& text) {
        if(text=="-")return true;
        size_t colon=text.find(':');
        if(colon==std::string::npos || !colon || colon+1==text.size() || text.size()>96)return false;
        for(size_t i=0;i<text.size();++i) {
            char ch=text[i];if(i==colon)continue;
            if(i>colon?(ch<'0'||ch>'9'):!((ch>='A'&&ch<='Z')||(ch>='a'&&ch<='z')||ch=='_'))return false;
        }
        return true;
    }
    static bool Draw(unsigned bound,unsigned& result) {
        if(!bound)return false;
        const unsigned threshold=(0u-bound)%bound;unsigned value;
        do {if(BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&value),sizeof(value),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)return false;}while(value<threshold);
        result=value%bound;return true;
    }
    template<class T> static bool Select(const std::vector<T>& pool,const std::set<std::wstring>& used,const std::wstring& old,T& selected) {
        std::vector<unsigned> choices;
        for(unsigned i=0;i<pool.size();++i)if(!used.count(pool[i][0]) && pool[i][0]!=old)choices.push_back(i);
        if(choices.empty())for(unsigned i=0;i<pool.size();++i)if(!used.count(pool[i][0]))choices.push_back(i);
        unsigned index;if(!Draw(static_cast<unsigned>(choices.size()),index))return false;
        selected=pool[choices[index]];return true;
    }
    static TwinKartEquipment::String Borrow(const std::wstring& text) {
        return {text.c_str(),static_cast<int>(text.size()+1),static_cast<int>(text.size()+1)};
    }
public:
    bool Load(const char* bytes,size_t size) {
        if(!bytes || !size || size>4*1024*1024 || memchr(bytes,0,size))return false;
        std::istringstream input(std::string(bytes,size));std::string signature;
        if(!std::getline(input,signature) || signature!="TKAI1")return false;
        std::vector<Character> nextCharacters;std::vector<Kart> nextKarts;
        std::set<std::wstring> seenCharacters,seenKarts;std::string line;
        while(std::getline(input,line)) {
            if(line.empty() || line.size()>2048)return false;
            std::istringstream row(line);char kind;row>>kind;
            unsigned count=kind=='C'?6:kind=='K'?10:0;if(!count)return false;
            Kart fields{};std::string value;
            for(unsigned i=0;i<count;++i) {
                if(!(row>>value) || !Identifier(value))return false;
                if(value!="-")fields[i]=std::wstring(value.begin(),value.end());
            }
            if(row>>value)return false;
            if(kind=='C') {
                if(fields[0].rfind(L"Character:",0)!=0 || fields[1].rfind(L"Costume:",0)!=0 || !seenCharacters.insert(fields[0]).second)return false;
                Character item{};for(unsigned i=0;i<6;++i)item[i]=std::move(fields[i]);nextCharacters.push_back(std::move(item));
            } else {
                if(fields[0].rfind(L"Kart:",0)!=0 || !seenKarts.insert(fields[0]).second)return false;
                nextKarts.push_back(std::move(fields));
            }
            if(nextCharacters.size()>10000 || nextKarts.size()>10000)return false;
        }
        if(nextCharacters.size()<8 || nextKarts.size()<8)return false;
        characters=std::move(nextCharacters);karts=std::move(nextKarts);opponents=0;current={};return true;
    }
    bool LoadEnvironment() {
        wchar_t path[32768]{};DWORD n=GetEnvironmentVariableW(L"TWIN_KART_AI_CATALOG",path,ARRAYSIZE(path));
        if(!n || n>=ARRAYSIZE(path))return false;
        HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
        if(file==INVALID_HANDLE_VALUE)return false;
        LARGE_INTEGER size{};bool valid=GetFileSizeEx(file,&size) && size.QuadPart>0 && size.QuadPart<=4*1024*1024;
        std::string bytes(valid?static_cast<size_t>(size.QuadPart):0,'\0');DWORD read=0;
        valid=valid && ReadFile(file,&bytes[0],static_cast<DWORD>(bytes.size()),&read,nullptr) && read==bytes.size();CloseHandle(file);
        return valid && Load(bytes.data(),bytes.size());
    }
    // Callback: 1 = registered native asset, 0 = absent, -1 = unavailable
    // lookup. Validate before drawing; never silently sample unverified IDs.
    using Available=int(*)(const std::wstring&,void*);
    struct AvailabilityStats {unsigned characterFields[6]{};unsigned clearedMotions=0;};
    bool Prepare(int count,const TwinKartEquipment::Preset& human,Available available,void* context,
                 unsigned* characterCount=nullptr,unsigned* kartCount=nullptr,AvailabilityStats* stats=nullptr) {
        if(count<1 || count>7)return false;
        if(!available)return false;
        std::map<std::wstring,int> availability;
        bool failed=false;
        auto lookup=[&](const std::wstring& item) {
            auto found=availability.find(item);
            if(found==availability.end())found=availability.emplace(item,available(item,context)).first;
            if(found->second<0 || found->second>1)failed=true;
            return found->second;
        };
        auto registered=[&](const auto& outfit) {
            bool valid=true;
            for(const auto& item:outfit) {
                if(item.empty())continue;
                if(lookup(item)!=1)valid=false;
            }
            return valid;
        };
        std::vector<Character> nativeCharacters;std::vector<Kart> nativeKarts;
        for(auto item:characters) {
            bool valid=true;
            for(unsigned field=0;field<6;++field) {
                if(item[field].empty())continue;
                int result=lookup(item[field]);
                if(result==1){if(stats)++stats->characterFields[field];continue;}
                if(field<2)valid=false;
                else if(result==0) {
                    // Podium/finish emotes are optional. Missing emotes in a
                    // newer reference catalog must not disqualify an otherwise
                    // registered character+costume or be passed to the loader.
                    item[field].clear();if(stats)++stats->clearedMotions;
                }
            }
            if(valid)nativeCharacters.push_back(std::move(item));
        }
        for(const auto& item:karts)if(registered(item))nativeKarts.push_back(item);
        if(characterCount)*characterCount=static_cast<unsigned>(nativeCharacters.size());
        if(kartCount)*kartCount=static_cast<unsigned>(nativeKarts.size());
        if(failed)return false;
        std::array<Outfit,8> next{};std::set<std::wstring> usedCharacters,usedKarts;
        if(TwinKartEquipment::Valid(human.character[0]) && TwinKartEquipment::Nonempty(human.character[0]))usedCharacters.insert(human.character[0].data);
        if(TwinKartEquipment::Valid(human.kart[0]) && TwinKartEquipment::Nonempty(human.kart[0]))usedKarts.insert(human.kart[0].data);
        for(int slot=1;slot<=count;++slot) {
            if(!Select(nativeCharacters,usedCharacters,current[slot].character[0],next[slot].character) ||
               !Select(nativeKarts,usedKarts,current[slot].kart[0],next[slot].kart))return false;
            usedCharacters.insert(next[slot].character[0]);usedKarts.insert(next[slot].kart[0]);
        }
        current=std::move(next);opponents=count;return true;
    }
    bool Get(int slot,TwinKartEquipment::Preset& value) const {
        if(slot<1 || slot>opponents)return false;
        value={};for(unsigned i=0;i<6;++i)value.character[i]=Borrow(current[slot].character[i]);
        for(unsigned i=0;i<10;++i)value.kart[i]=Borrow(current[slot].kart[i]);return true;
    }
};
}
