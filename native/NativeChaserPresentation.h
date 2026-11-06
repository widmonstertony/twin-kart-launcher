#pragma once
#include "NativeEquipmentPreset.h"
#include <cstdint>
namespace TwinKartObjective {
struct ChaserText {void* data=nullptr;void* references=nullptr;unsigned flags=0,padding=0;};
static_assert(sizeof(ChaserText)==24,"Original x64 FText value");
using CreateChaserText=ChaserText*(__fastcall*)(ChaserText*,const TwinKartEquipment::String*);
using NameChaserText=void*(__fastcall*)(std::uint64_t*,const wchar_t*,int);
using SetChaserText=void(__fastcall*)(void*,std::uint64_t,const ChaserText*);
// Project-authored presentation for the recovered native prototype. The
// engine's reflected text setter owns FText reference counting and property
// validation. No scalar mode/configuration, object class or gameplay state is
// written. Text storage intentionally has process lifetime, like the CDO.
class ChaserPresentation {
    struct Label {const wchar_t* property;const wchar_t* text;};
    static inline const Label labels[]{
        {L"Title",L"\u8ffd\u9010\u8d5b"},
        {L"ShortName",L"\u8ffd\u9010"},
        {L"FullName",L"\u8ffd\u9010\u8d5b"},
        {L"LoadingName",L"\u8ffd\u9010\u8d5b"},
        {L"ResultName",L"\u8ffd\u9010\u8d5b"},
        {L"DetailText",L"\u8ffd\u9010\u4e0e\u9003\u8131\uff0c\u539f\u5382\u7ade\u6280\u573a\u517c\u5bb9\u573a\u5730"},
        {L"Description",L"\u8ffd\u9010\u8005\u901a\u8fc7\u78b0\u649e\u8f6c\u5316\u9003\u8131\u8005\u3002\u9003\u8131\u8005\u6536\u96c6\u5e76\u4ea4\u4ed8\u8d44\u6e90\uff0c\u575a\u6301\u5230\u5012\u8ba1\u65f6\u7ed3\u675f\u3002"}
    };
    static inline const Label hideLabels[]{
        {L"Title",L"\u8eb2\u732b\u732b\uff1a\u4fee\u590d\u7248\u6d4b\u8bd5\u573a\u5730"},
        {L"ShortName",L"\u8eb2\u732b\u732b"},
        {L"FullName",L"\u8eb2\u732b\u732b\uff1a\u4fee\u590d\u7248\u6d4b\u8bd5\u573a\u5730"},
        {L"LoadingName",L"\u8eb2\u732b\u732b\uff1a\u4fee\u590d\u7248\u6d4b\u8bd5\u573a\u5730"},
        {L"ResultName",L"\u8eb2\u732b\u732b"},
        {L"DetailText",L"\u539f\u5382\u8eb2\u85cf\u4e0e\u641c\u7d22\u89c4\u5219\uff0c\u4fee\u590d\u7248\u6d4b\u8bd5\u573a\u5730"},
        {L"Description",L"\u8eb2\u85cf\u8005\u53d8\u6210\u573a\u666f\u9053\u5177\uff0c\u641c\u7d22\u8005\u5bfb\u627e\u5e76\u6293\u6355\u3002\u4e00\u540d\u73a9\u5bb6\u4e0e\u4e03\u540d AI\u3002"}
    };
    static inline const Label factoryLabels[]{
        {L"Title",L"\u5de5\u5382\u9003\u8131"},
        {L"ShortName",L"\u5de5\u5382\u9003\u8131"},
        {L"FullName",L"\u5de5\u5382\u9003\u8131"},
        {L"LoadingName",L"\u5de5\u5382\u9003\u8131"},
        {L"ResultName",L"\u5de5\u5382\u9003\u8131"},
        {L"DetailText",L"\u56db\u4eba\u5408\u4f5c\uff0c\u4e94\u8f6e\u5de5\u5382\u9003\u8131"},
        {L"Description",L"\u4e0e\u4e09\u540d AI \u961f\u53cb\u5408\u4f5c\uff0c\u5e94\u5bf9\u4e09\u5341\u516d\u4e2a\u539f\u5382\u673a\u5668\u4eba\u3002"}
    };
    static inline const Label freeDriveLabels[]{
        {L"Title",L"\u81ea\u7531\u9a7e\u9a76"},
        {L"ShortName",L"\u81ea\u7531\u9a7e\u9a76"},
        {L"FullName",L"\u81ea\u7531\u9a7e\u9a76"},
        {L"LoadingName",L"\u81ea\u7531\u9a7e\u9a76"},
        {L"ResultName",L"\u81ea\u7531\u9a7e\u9a76"},
        {L"DetailText",L"\u9009\u62e9\u539f\u5382\u8d5b\u9053\uff0c\u5355\u4eba\u7ec3\u4e60"},
        {L"Description",L"\u5355\u4eba\u81ea\u7531\u9a7e\u9a76\uff0c\u4e0d\u8bb0\u5f55\u6210\u7ee9\uff0c\u968f\u65f6\u9000\u51fa\u3002"}
    };
    ChaserText values[sizeof(labels)/sizeof(labels[0])]{};
    std::uint64_t names[sizeof(labels)/sizeof(labels[0])]{};
    bool initialized=false;
    bool hideMode=false,factoryMode=false,freeDriveMode=false;
public:
    explicit ChaserPresentation(bool hide=false,bool factory=false,bool freeDrive=false):hideMode(hide),factoryMode(factory),freeDriveMode(freeDrive){}
    bool Apply(void* asset,CreateChaserText create,NameChaserText name,SetChaserText set) {
        if(!asset || !create || !name || !set)return false;
        if(!initialized) {
            for(size_t i=0;i<sizeof(labels)/sizeof(labels[0]);++i) {
                const auto& label=freeDriveMode?freeDriveLabels[i]:factoryMode?factoryLabels[i]:hideMode?hideLabels[i]:labels[i];
                int size=static_cast<int>(wcslen(label.text)+1);
                TwinKartEquipment::String text{label.text,size,size};
                if(create(&values[i],&text)!=&values[i] || !values[i].data || !values[i].references)return false;
                name(&names[i],label.property,1);if(!names[i])return false;
            }
            initialized=true;
        }
        for(size_t i=0;i<sizeof(labels)/sizeof(labels[0]);++i)set(asset,names[i],&values[i]);
        return true;
    }
};
}
