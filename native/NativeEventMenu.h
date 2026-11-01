#pragma once
#include <array>
#include <string>
#include <cstddef>
#include "NativeEventRules.h"
#include "NativeEquipmentPreset.h"
#include "NativeRacePreparation.h"
namespace TwinKartRace {
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
    bool Build(void* originalRowVtable) {
        size_t count=0;auto rules=EventProfiles(count);if(!originalRowVtable || !count || count>Maximum)return false;
        for(size_t i=0;i<count;++i) {
            auto& entry=entries[i];entry={};
            profiles[i]=std::wstring(rules[i].profile,rules[i].profile+strlen(rules[i].profile));
            ids[i]=L"TwinKart_"+profiles[i];entry.id=Text(ids[i].c_str());
            entry.index=entry.sort=static_cast<int>(i+1);entry.start=Text(L"2000/1/1 0:00:00");entry.end=Text(L"2099/12/31 23:59:59");
            entry.noParty=TeamSize(rules[i].mode)==1?1:0;
            modes[i]={};modes[i].mode=static_cast<unsigned char>(rules[i].mode);
            auto profile=Text(profiles[i].c_str());modes[i].profile={const_cast<wchar_t*>(profile.data),profile.count,profile.capacity};
            entry.modes={&modes[i],1,1};
        }
        source.baseVtable=originalRowVtable;source.rows={entries.data(),static_cast<int>(count),static_cast<int>(count)};return true;
    }
};
}
