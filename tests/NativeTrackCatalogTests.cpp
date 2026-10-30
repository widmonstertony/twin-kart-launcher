#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <array>
#include <stdexcept>
#include <cstdio>
#include "../native/NativeTrackCatalog.h"
namespace R=TwinKartRace;
namespace {
struct Row {
    std::array<unsigned char,0x160> record{};
    std::array<unsigned char,2> modes{};
    Row(const wchar_t* name,unsigned char mode,int laps) {
        struct String {const wchar_t* data;int count,capacity;} value{name,static_cast<int>(wcslen(name)+1),static_cast<int>(wcslen(name)+1)};
        memcpy(record.data(),&value,sizeof(value));memcpy(record.data()+0x10,&laps,4);modes[0]=mode;
        struct Bytes {const unsigned char* data;int count,capacity;} array{modes.data(),1,2};memcpy(record.data()+0x18,&array,sizeof(array));
    }
};
Row speed{L"Test_R01",2,3},item{L"Test_I01",1,2},special{L"License_T01",0,1};
const void* records[]{speed.record.data(),item.record.data(),special.record.data()};
int frees=0;bool malformed=false,notReady=false;
void Need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void* __fastcall Get(void* root,R::NativeTrackRows* rows,unsigned char type) {
    Need(root==records && type==0x14,"Wrong original table interface");
    rows->count=notReady?0:3;rows->capacity=malformed?1:3;
    rows->data=static_cast<const void**>(HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(records)));
    Need(rows->data!=nullptr,"Fixture allocation");memcpy(rows->data,records,sizeof(records));return rows;
}
void __fastcall Free(void* memory){Need(HeapFree(GetProcessHeap(),0,memory)!=FALSE,"Original allocator routing");++frees;}
}
int main(){try{
    R::NativeTrackEntry entries[3]{};R::NativeTrackEnumeration summary;
    R::NativeTrackEnumeration pending{};pending.failureStage=2;
    Need(R::RetryNativeTrackEnumeration(0,pending) && R::RetryNativeTrackEnumeration(-1,pending),"Transient native table initialization was not retried");
    pending.failureStage=3;Need(!R::RetryNativeTrackEnumeration(-1,pending) && !R::RetryNativeTrackEnumeration(-2,pending),"Malformed or truncated native metadata was retried");
    Need(R::EnumerateNativeTracks(records,Get,Free,entries,3,summary)==1 && summary.rows==3 && summary.count==2 && summary.nonRacing==1 && frees==1,"Complete native catalog enumeration failed");
    Need(!wcscmp(entries[0].alias,L"Test_R01") && entries[0].modeMask==4 && entries[0].laps==3 &&
        !wcscmp(entries[1].alias,L"Test_I01") && entries[1].modeMask==2 && entries[1].laps==2,"Original record identity/mode/laps changed");
    // Entries own their names and never retain pointers into the native rows.
    auto old=records[1];records[1]=records[0];
    Need(R::EnumerateNativeTracks(records,Get,Free,entries,3,summary)==-1 && summary.count==0 && frees==2,"Ambiguous native alias accepted");records[1]=old;
    Need(R::EnumerateNativeTracks(records,Get,Free,entries,1,summary)==-2 && summary.count==0 && frees==3,"Truncated discovery reported completion");
    malformed=true;Need(R::EnumerateNativeTracks(records,Get,Free,entries,3,summary)==-1 && summary.count==0 && frees==4,"Malformed native array leaked or accepted");malformed=false;
    auto valid=records[0];records[0]=reinterpret_cast<void*>(1);
    Need(R::EnumerateNativeTracks(records,Get,Free,entries,3,summary)==-1 && summary.count==0 && frees==5,"Expired record escaped guard/cleanup");records[0]=valid;
    notReady=true;Need(R::EnumerateNativeTracks(records,Get,Free,entries,3,summary)==0 && summary.count==0 && frees==6,"Empty native table falsely ready or leaked");
    notReady=false;int observed=999;memcpy(speed.record.data()+0x10,&observed,4);
    Need(R::EnumerateNativeTracks(records,Get,Free,entries,3,summary)==1 && entries[0].laps==999 && frees==7,"Observed native999 metadata rejected, normalized or leaked");
    observed=100;memcpy(speed.record.data()+0x10,&observed,4);
    Need(R::EnumerateNativeTracks(records,Get,Free,entries,3,summary)==-1 && summary.failureStage==6 && summary.failureValue==100 && frees==8,"Unknown native lap metadata accepted or leaked");
    observed=3;memcpy(speed.record.data()+0x10,&observed,4);
    special.modes[0]=4;
    Need(R::EnumerateNativeTracks(records,Get,Free,entries,3,summary)==1 && summary.count==3 && entries[2].modeMask==16,"Flag map omitted from native resource discovery");
    for(int mode=1;mode<=6;++mode)Need(!R::NativeTrackSupports(entries[2].modeMask,mode),"Flag map silently mapped to ordinary racing");
    Need(R::NativeTrackSupports(entries[2].modeMask,104),"Flag map rejected its dedicated native mode");
    special.modes[0]=3;
    Need(R::EnumerateNativeTracks(records,Get,Free,entries,3,summary)==1 && summary.count==3 && entries[2].modeMask==8,"Spitem map omitted from native resource discovery");
    special.modes[0]=0;
    puts("PASS original native track-table boundary: exact table/record aliases, independent speed/item metadata, non-racing exclusion, duplicate/truncation/expired-record rejection and original array cleanup. Synthetic native interface only; no game started.");return 0;
}catch(const std::exception& e){fprintf(stderr,"%s\n",e.what());return 1;}}
