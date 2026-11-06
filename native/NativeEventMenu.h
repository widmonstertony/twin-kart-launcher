#pragma once
#include <array>
#include <string>
#include <cstddef>
#include "NativeEventRules.h"
#include "NativeEquipmentPreset.h"
#include "NativeRacePreparation.h"
#include "RaceControl.h"
namespace TwinKartRace {
inline constexpr size_t FeaturedLocalModeCount=3; // unlimited boost solo, duo and squad
inline int EventMenuLocalModeIndex(size_t row,size_t profileCount,size_t baseCount) {
    if(row<FeaturedLocalModeCount)return static_cast<int>(row);
    return row>=FeaturedLocalModeCount+profileCount && row<baseCount?static_cast<int>(row-profileCount):-1;
}
template<class T> struct MenuArray {T* data=nullptr;int count=0,capacity=0;};
// Exact reflected PC FKtClientModeLabDef and its enclosing table row. These
// are borrowed copy sources for UDataTable::AddRow/CopyScriptStruct. They must
// never be installed as raw engine-owned arrays or freed by the engine.
struct EventMenuRow {
    TwinKartEquipment::String id{};
    unsigned char active=1,pad0[3]{};int index=0,sort=0,pad1=0;
    TwinKartEquipment::String start{},end{};
    MenuArray<TwinKartEquipment::String> hours{};
    unsigned char noParty=0,pad2[7]{};
    MenuArray<unsigned char> legacyModes{};
    MenuArray<PreparationMode> modes{};
};
static_assert(sizeof(EventMenuRow)==120 && offsetof(EventMenuRow,active)==16 && offsetof(EventMenuRow,index)==20 &&
    offsetof(EventMenuRow,start)==32 && offsetof(EventMenuRow,hours)==64 && offsetof(EventMenuRow,modes)==104,"Original menu row ABI");
struct EventMenuRowSet {void* baseVtable=nullptr;MenuArray<EventMenuRow> rows{};};
static_assert(sizeof(EventMenuRowSet)==24 && offsetof(EventMenuRowSet,rows)==8,"Original mode-table row ABI");
class EventMenuCopySource {
    static constexpr size_t Maximum=32;
    std::array<EventMenuRow,Maximum> entries{};
    std::array<PreparationMode,Maximum> modes{};
    std::array<std::wstring,Maximum> ids{},profiles{};
    static TwinKartEquipment::String Text(const wchar_t* text) {int n=static_cast<int>(wcslen(text)+1);return {text,n,n};}
public:
    EventMenuRowSet source{};
    EventMenuCopySource()=default;
    EventMenuCopySource(const EventMenuCopySource&)=delete;
    EventMenuCopySource& operator=(const EventMenuCopySource&)=delete;
    bool Build(void* originalRowVtable,bool includeFactory=false,bool includeChaser=false,bool includeHide=false,bool includeFreeDrive=false) {
        size_t profilesCount=0,localCount=0;auto rules=EventProfiles(profilesCount);auto local=LocalLobbyModes(localCount);
        size_t count=profilesCount+localCount,total=count+(includeFactory?1u:0u)+(includeChaser?1u:0u)+(includeHide?1u:0u)+(includeFreeDrive?1u:0u);
        if(!originalRowVtable || !profilesCount || localCount<FeaturedLocalModeCount || total>Maximum)return false;
        for(size_t i=0;i<total;++i) {
            auto& entry=entries[i];entry={};
            bool factory=includeFactory && i==count,chaser=includeChaser && i==count+(includeFactory?1u:0u);
            bool hide=includeHide && i==count+(includeFactory?1u:0u)+(includeChaser?1u:0u);
            bool freeDrive=includeFreeDrive && i==total-1,special=factory || chaser || hide || freeDrive;
            int localIndex=EventMenuLocalModeIndex(i,profilesCount,count);
            bool localMode=localIndex>=0;
            const auto* rule=special || localMode?nullptr:&rules[i-FeaturedLocalModeCount];
            int mode=factory?100:chaser?108:hide?109:freeDrive?1:localMode?local[localIndex]:rule->mode;
            profiles[i]=freeDrive?FreeDriveProfileWide:rule?std::wstring(rule->profile,rule->profile+strlen(rule->profile)):L"";
            ids[i]=factory?L"TwinKart_FactoryRun":chaser?L"TwinKart_Chaser":hide?L"TwinKart_Hide_Repaired":freeDrive?L"TwinKart_FreeDrive":localMode?L"TwinKart_LocalMode_"+std::to_wstring(mode):L"TwinKart_"+profiles[i];entry.id=Text(ids[i].c_str());
            entry.index=entry.sort=static_cast<int>(i+1);entry.start=Text(L"2000/1/1 0:00:00");entry.end=Text(L"2099/12/31 23:59:59");
            entry.noParty=hide?1:(factory || chaser)?0:TeamSize(mode)==1?1:0;
            modes[i]={};modes[i].mode=static_cast<unsigned char>(mode);
            auto profile=Text(profiles[i].c_str());modes[i].profile={const_cast<wchar_t*>(profile.data),profile.count,profile.capacity};
            entry.modes={&modes[i],1,1};
        }
        source.baseVtable=originalRowVtable;source.rows={entries.data(),static_cast<int>(total),static_cast<int>(total)};return true;
    }
};
}
