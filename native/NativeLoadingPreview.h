#pragma once
#include "NativeEquipmentPreset.h"
#include "NativeChaserPresentation.h"
#include "NativeFactoryPopulation.h"
#include "RaceControl.h"
#include <utility>
namespace TwinKartRace {
struct LoadingLabels {const wchar_t* mode;const wchar_t* group;const wchar_t* guide;};
inline LoadingLabels LocalLoadingLabels(int mode,const char* profile) {
    const wchar_t* title=nullptr;
    if(profile) {
        struct EventTitle {const char* id;const wchar_t* name;};
        constexpr EventTitle names[]{
            {"speed_solo_ghost",L"\u5e7d\u7075\u7ade\u901f"},
            {"speed_solo_derek",L"\u5fb7\u91cc\u514b\u7ade\u901f"},
            {"speed_squad_skill",L"\u6280\u80fd\u7ade\u901f"},
            {"item_squad_miso_tiera",L"\u63a8\u62c9\u8d5b"},
            {"item_solo_brodi",L"\u5e03\u7f57\u8fea\u9053\u5177\u8d5b"},
            {"s062_speed_solo_dao",L"\u76ae\u86cb\u7ade\u901f"},
            {"s062_item_squad_runner_attacker",L"\u8ffd\u51fb\u9053\u5177\u8d5b"},
            {"s062_spitem_solo_skill",L"\u6280\u80fd\u9053\u5177\u8d5b"},
            {"s062_spitem_squad_skill",L"\u6280\u80fd\u9053\u5177\u8d5b"},
            {"s062_flag_skill",L"\u6280\u80fd\u593a\u65d7\u8d5b"}};
        for(const auto& entry:names)if(!strcmp(profile,entry.id)){title=entry.name;break;}
    }
    const int team=TeamSize(mode);
    const wchar_t* group=team==4?L"\u56db\u4eba\u7ec4":team==2?L"\u53cc\u4eba\u7ec4":team==1?L"\u4e2a\u4eba\u8d5b":L"\u672c\u5730\u5bf9\u5c40";
    const wchar_t* guide=TeamMode(mode)?L"\u4e0e\u961f\u53cb\u4e00\u8d77\u4e89\u53d6\u540d\u6b21":
        L"\u5b8c\u6210\u6bd4\u8d5b\uff0c\u4e89\u53d6\u66f4\u9ad8\u540d\u6b21";
    if(FlagMode(mode))return {title?title:L"\u593a\u65d7\u8d5b",group,L"\u628a\u65d7\u5e26\u56de\u5df1\u65b9\u57fa\u5730"};
    if(ChaserMode(mode))return {L"\u8ffd\u9010\u8d5b",group,L"\u8ffd\u9010\u5bf9\u624b\uff0c\u6839\u636e\u8eab\u4efd\u5b8c\u6210\u76ee\u6807"};
    if(mode==109)return {L"\u8eb2\u732b\u732b",group,L"\u4f2a\u88c5\u8eb2\u85cf\uff0c\u6216\u641c\u5bfb\u5bf9\u624b"};
    if(mode==100)return {L"\u5de5\u5382\u9003\u8131",group,L"\u9003\u79bb\u673a\u5668\u4eba\uff0c\u5b8c\u6210\u9003\u8131"};
    if(InfiniteBoostMode(mode))return {L"\u65e0\u9650\u52a0\u901f",group,L"\u4f7f\u7528\u52a0\u901f\u5668\u62a2\u5360\u540d\u6b21"};
    if(CubeMode(mode))return {L"\u65b9\u5757\u9053\u5177\u8d5b",group,L"\u62fe\u53d6\u65b9\u5757\uff0c\u4f7f\u7528\u9053\u5177\u4e89\u53d6\u540d\u6b21"};
    if(mode==112 && (!profile || !*profile || !strcmp(profile,"item_squad_miso_tiera")))
        return {L"\u63a8\u62c9\u8d5b",group,L"\u6309\u961f\u4f0d\u603b\u5206\u51b3\u5b9a\u540d\u6b21"};
    if(!CircuitMode(mode))return {};
    return {title?title:SpitemMode(mode)?L"\u7279\u6b8a\u9053\u5177\u8d5b":SpeedMode(mode)?L"\u7ade\u901f\u8d5b":L"\u9053\u5177\u8d5b",group,guide};
}
// Exact owned PC Traveler / FKtClientTrackDef presentation boundaries.
struct LoadingPreviewApi {
    using Lookup=void*(__fastcall*)(void*,const TwinKartEquipment::String*);
    using Move=void*(__fastcall*)(void*,void*);
    using Build=void*(__fastcall*)(void*,const void*);
    using Destroy=void(__fastcall*)(void*);
    using FindText=bool(__fastcall*)(const TwinKartEquipment::String*,const TwinKartEquipment::String*,void*);
    using ReadText=bool(*)(const unsigned char*,wchar_t*,size_t);
    Lookup lookup=nullptr;Move move=nullptr;Build build=nullptr;Destroy destroy=nullptr;
    TwinKartEquipment::CopyString copy=nullptr;
    FindText find=nullptr;ReadText read=nullptr;TwinKartObjective::CreateChaserText create=nullptr;
};
inline TwinKartEquipment::String PreviewString(const wchar_t* value) {
    int n=static_cast<int>(wcslen(value)+1);return {value,n,n};
}
inline bool ChinesePreviewText(const wchar_t* text) {
    bool chinese=false;
    for(size_t i=0;text && text[i] && i<1024;++i) {
        if((text[i]>=0xac00 && text[i]<=0xd7af) || (text[i]>=0x1100 && text[i]<=0x11ff))return false;
        chinese|=text[i]>=0x3400 && text[i]<=0x9fff;
    }
    return chinese;
}
inline bool ReplaceLoadingText(void* target,const wchar_t* label,TwinKartObjective::CreateChaserText create,LoadingPreviewApi::Destroy destroy) {
    if(!target || !label || !*label || !create || !destroy)return false;
    alignas(16) unsigned char displaced[0x160]{};
    auto replacement=reinterpret_cast<TwinKartObjective::ChaserText*>(displaced+0x40);
    auto view=PreviewString(label);
    __try {
        if(create(replacement,&view)!=replacement || !replacement->data || !replacement->references)return false;
        std::swap(*static_cast<TwinKartObjective::ChaserText*>(target),*replacement);return true;
    } __finally {destroy(displaced);}
}
// Native lookup constructs an owning record; native move and destruction
// retain its FString/FText allocator and reference-count contracts. The
// displaced title is released inside a separate zero-initialized record.
inline bool RefreshLoadingPreview(void* raw,const Request& request,const LoadingPreviewApi& api,bool& officialChinese) {
    officialChinese=false;
    wchar_t factoryOptions[256]{};
    if(!raw || (!Valid(request) && !TwinKartFactory::CaptureOptions(request,factoryOptions,ARRAYSIZE(factoryOptions))) ||
       !api.lookup || !api.move || !api.build || !api.destroy || !api.copy ||
       !api.find || !api.read || !api.create)return false;
    wchar_t alias[97]{},profile[97]{},key[128]{},label[1024]{};
    for(size_t i=0;request.track[i];++i)alias[i]=request.track[i];
    for(size_t i=0;request.profile[i];++i)profile[i]=request.profile[i];
    if(swprintf_s(key,L"TrackName2_%s",alias)<1)return false;
    auto track=PreviewString(alias),modeProfile=PreviewString(profile),space=PreviewString(L"String"),textKey=PreviewString(key);
    alignas(16) unsigned char record[0x160]{};
    bool lookedUp=false;
    __try {
        __try {
            lookedUp=true;if(api.lookup(record,&track)!=record)return false;
            auto& actual=*reinterpret_cast<const TwinKartEquipment::String*>(record);
            if(!TwinKartEquipment::Valid(actual,97) || !TwinKartEquipment::Equal(actual,track))return false;
            api.find(&space,&textKey,record+0x40);
            officialChinese=api.read(record+0x40,label,ARRAYSIZE(label)) && ChinesePreviewText(label);
            if(!officialChinese) {
                // Never display another map when an original locale key is
                // missing: retain a truthful Chinese label plus exact map ID.
                wchar_t fallback[128]{};if(swprintf_s(fallback,L"\u8d5b\u9053 %s",alias)<1)return false;
                if(!ReplaceLoadingText(record+0x40,fallback,api.create,api.destroy))return false;
            }
            auto traveler=static_cast<unsigned char*>(raw);
            api.copy(traveler+0x2f8,&track);
            traveler[0x308]=traveler[0x260]=static_cast<unsigned char>(request.mode);
            api.copy(traveler+0x310,&modeProfile);api.copy(traveler+0x268,&modeProfile);
            if(api.move(traveler+0x3b8,record)!=traveler+0x3b8)return false;
            api.build(traveler+0x100,traveler+0x3b8);
            const auto& cached=*reinterpret_cast<const TwinKartEquipment::String*>(traveler+0x2f8);
            const auto& selected=*reinterpret_cast<const TwinKartEquipment::String*>(traveler+0x3b8);
            const auto& active=*reinterpret_cast<const TwinKartEquipment::String*>(traveler+0x100);
            return TwinKartEquipment::Valid(cached,97) && TwinKartEquipment::Equal(cached,track) &&
                TwinKartEquipment::Valid(selected,97) && TwinKartEquipment::Equal(selected,track) &&
                TwinKartEquipment::Valid(active,97) && TwinKartEquipment::Equal(active,track) &&
                api.read(traveler+0x140,label,ARRAYSIZE(label)) && ChinesePreviewText(label) &&
                api.read(traveler+0x3f8,label,ARRAYSIZE(label)) && ChinesePreviewText(label);
        } __finally {
            if(lookedUp)api.destroy(record);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
}
