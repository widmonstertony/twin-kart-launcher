#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <map>
#include <string>
#include <stdexcept>
#include <cstdio>
#include "../native/NativeLoadingPreview.h"
namespace R=TwinKartRace;
using S=TwinKartEquipment::String;using T=TwinKartObjective::ChaserText;
std::map<void*,std::wstring> strings;std::wstring korean=L"\ud55c\uae00",chinese=L"\u77ff\u5c71 \u5b9d\u77f3\u5f00\u91c7\u573a",fallback;
bool hasChinese=true,wrongAlias=false;int lookups=0,destroys=0,builds=0,creates=0;
void Need(bool value){if(!value)throw std::runtime_error("Loading preview regression");}
void* __fastcall Copy(void* raw,const void* source) {
    auto& value=strings[raw];value=static_cast<const S*>(source)->data;
    *static_cast<S*>(raw)=R::PreviewString(value.c_str());return raw;
}
void* __fastcall Lookup(void* out,const S* alias) {
    ++lookups;memset(out,0,0x160);auto actual=R::PreviewString(wrongAlias?L"Wrong_I01":alias->data);Copy(out,&actual);
    *reinterpret_cast<T*>(static_cast<BYTE*>(out)+0x40)={&korean,&korean,0,0};
    *reinterpret_cast<unsigned long long*>(static_cast<BYTE*>(out)+0x98)=987654;return out;
}
void* __fastcall Move(void* out,void* in){memcpy(out,in,0x160);memset(in,0,0x160);return out;}
void* __fastcall Build(void* out,const void* in){++builds;memcpy(out,in,0x160);return out;}
void __fastcall Destroy(void* raw){++destroys;memset(raw,0,0x160);}
bool __fastcall Find(const S* ns,const S* key,void* out) {
    Need(!wcscmp(ns->data,L"String") && !wcscmp(key->data,L"TrackName2_Mine_I01"));
    if(hasChinese)*static_cast<T*>(out)={&chinese,&chinese,0,0};return hasChinese;
}
bool Read(const BYTE* raw,wchar_t* text,size_t size) {
    auto data=static_cast<const std::wstring*>(reinterpret_cast<const T*>(raw)->data);
    if(!data || data->size()+1>size)return false;wcscpy_s(text,size,data->c_str());return true;
}
T* __fastcall Create(T* out,const S* value){++creates;fallback=value->data;*out={&fallback,&fallback,0,0};return out;}
int main(){try {
    R::LoadingPreviewApi api;api.lookup=Lookup;api.move=Move;api.build=Build;api.destroy=Destroy;api.copy=Copy;api.find=Find;api.read=Read;api.create=Create;
    alignas(16) BYTE traveler[0x600]{};R::Request request;
    strcpy_s(request.identity,"0123456789abcdef0123456789abcdef");strcpy_s(request.track,"Mine_I01");request.mode=112;request.ai=7;request.laps=3;
    strcpy_s(request.profile,"item_squad_miso_tiera");auto stale=R::PreviewString(L"Old_I01");Copy(traveler+0x2f8,&stale);Copy(traveler+0x3b8,&stale);
    bool official=false;Need(R::RefreshLoadingPreview(traveler,request,api,official) && official && lookups==1 && destroys==1 && !creates);
    Need(traveler[0x260]==112 && traveler[0x308]==112 && !wcscmp(reinterpret_cast<S*>(traveler+0x268)->data,L"item_squad_miso_tiera"));
    Need(!wcscmp(reinterpret_cast<S*>(traveler+0x100)->data,L"Mine_I01") && !wcscmp(reinterpret_cast<S*>(traveler+0x3b8)->data,L"Mine_I01"));
    Need(*reinterpret_cast<unsigned long long*>(traveler+0x100+0x98)==987654 && reinterpret_cast<T*>(traveler+0x140)->data==&chinese);
    hasChinese=false;Need(R::RefreshLoadingPreview(traveler,request,api,official) && !official && creates==1 && destroys==3 && builds==2);
    Need(fallback==L"\u8d5b\u9053 Mine_I01" && R::ChinesePreviewText(fallback.c_str()) && !R::ChinesePreviewText(korean.c_str()));
    auto before=builds;wrongAlias=true;Need(!R::RefreshLoadingPreview(traveler,request,api,official) && builds==before && destroys==4);
    Need(!R::RefreshLoadingPreview(nullptr,request,api,official) && lookups==3);
    api.copy=nullptr;Need(!R::RefreshLoadingPreview(traveler,request,api,official) && lookups==3);
    for(int mode=1;mode<=118;++mode) {
        const auto labels=R::LocalLoadingLabels(mode,"");
        const bool supported=R::PlayableMode(mode) || mode==100 || mode==109;
        Need(supported==(labels.mode!=nullptr));
        if(supported)Need(R::ChinesePreviewText(labels.mode) && R::ChinesePreviewText(labels.group) && R::ChinesePreviewText(labels.guide));
    }
    Need(wcscmp(R::LocalLoadingLabels(112,"item_squad_miso_tiera").mode,
        R::LocalLoadingLabels(112,"s062_item_squad_runner_attacker").mode)!=0);
    Need(wcscmp(R::LocalLoadingLabels(116,"cubecubeitem_solo").group,
        R::LocalLoadingLabels(118,"cubecubeitem_squad").group)!=0);
    puts("PASS loading preview: exact selected track in cached/active metadata, retained image reference, original Chinese lookup, truthful missing-key fallback, native owner cleanup and wrong-map/null rejection. No game started.");return 0;
}catch(const std::exception& e){puts(e.what());return 1;}}
