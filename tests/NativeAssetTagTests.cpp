#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <array>
#include <cwchar>
#include <cstring>
#include <cstdio>
#include <stdexcept>
#include "../native/NativeAssetTags.h"
namespace A=TwinKartAssets;
namespace {
std::array<unsigned char,0x60> snapshot{};
constexpr std::uint64_t tag=0x1122334455667788ull;
enum Mode {Present,Missing,Empty,TooSmall,BadCapacity,NoTerminator,EmbeddedNull,NegativeCount,MissingBuffer};
Mode mode=Present;
unsigned reads=0,frees=0;
void Need(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
constexpr wchar_t text[]=L"(Bundles=((BundleName=ChaserMode,BundleAssets=(/Game/Fixture.Asset))))";
bool __fastcall Read(const void* asset,const std::uint64_t* name,A::TagString* output) {
    ++reads;Need(asset==snapshot.data() && name && *name==tag,"Const-reference snapshot/FName ABI changed");
    if(mode==Empty){*output={};return true;}
    if(mode==MissingBuffer){*output={nullptr,4,4};return true;}
    constexpr int count=static_cast<int>(sizeof(text)/sizeof(wchar_t));
    auto allocated=static_cast<wchar_t*>(HeapAlloc(GetProcessHeap(),0,sizeof(text)));
    Need(allocated!=nullptr,"Fixture allocation");memcpy(allocated,text,sizeof(text));
    *output={allocated,count,count};
    if(mode==BadCapacity)output->capacity=count-1;
    if(mode==NoTerminator)allocated[count-1]=L'X';
    if(mode==EmbeddedNull)allocated[3]=0;
    if(mode==NegativeCount)output->count=-1;
    return mode!=Missing;
}
void __fastcall Free(void* value) {Need(HeapFree(GetProcessHeap(),0,value)!=FALSE,"Original output allocator mismatch");++frees;}
}
int main(){try {
    for(unsigned i=0;i<snapshot.size();++i)snapshot[i]=static_cast<unsigned char>(i);
    auto original=snapshot;wchar_t output[256]{};A::TagReadResult result;
    Need(A::CopyTagValue(snapshot.data(),tag,Read,Free,output,256,result)==1 && result.found &&
         result.characters==wcslen(text) && !wcscmp(output,text) && reads==1 && frees==1,"Present metadata was not copied/released exactly once");
    mode=Missing;
    Need(A::CopyTagValue(snapshot.data(),tag,Read,Free,output,256,result)==0 && !result.found && !result.characters &&
         !output[0] && reads==2 && frees==2,"Absent tag leaked its native output or reused stale text");
    mode=Empty;
    Need(A::CopyTagValue(snapshot.data(),tag,Read,Free,output,256,result)==1 && result.found && !result.characters &&
         !output[0] && reads==3 && frees==2,"Empty present metadata was confused with an absent tag");
    mode=TooSmall;
    Need(A::CopyTagValue(snapshot.data(),tag,Read,Free,output,4,result)==-2 && result.found &&
         result.characters==wcslen(text) && !output[0] && reads==4 && frees==3,"Truncated tag was returned as a complete dependency list");
    for(Mode invalid:{BadCapacity,NoTerminator,EmbeddedNull,NegativeCount,MissingBuffer}) {
        mode=invalid;unsigned oldFrees=frees;
        Need(A::CopyTagValue(snapshot.data(),tag,Read,Free,output,256,result)==-1 && !output[0] && !result.found,
             "Malformed native metadata was accepted or stale output escaped");
        Need(frees==oldFrees+(invalid==MissingBuffer?0u:1u),"Rejected native output was not freed exactly once");
    }
    unsigned oldReads=reads,oldFrees=frees;
    Need(A::CopyTagValue(nullptr,tag,Read,Free,output,256,result)==-1 &&
         A::CopyTagValue(snapshot.data(),0,Read,Free,output,256,result)==-1 &&
         A::CopyTagValue(snapshot.data(),tag,nullptr,Free,output,256,result)==-1 &&
         A::CopyTagValue(snapshot.data(),tag,Read,nullptr,output,256,result)==-1 &&
         A::CopyTagValue(snapshot.data(),tag,Read,Free,nullptr,256,result)==-1 &&
         A::CopyTagValue(snapshot.data(),tag,Read,Free,output,0,result)==-1 &&
         A::CopyTagValue(snapshot.data(),tag,Read,Free,output,1048577,result)==-1 &&
         reads==oldReads && frees==oldFrees,"Invalid inputs reached a native reader/allocator");
    Need(snapshot==original,"Read-only tag extraction mutated the borrowed native asset snapshot");
    puts("PASS native asset-tag boundary: exact const-reference FName/snapshot ABI, present/missing/empty metadata, original allocator cleanup, malformed strings and no partial dependency export. Synthetic native callbacks only; no game/resource load started.");
    return 0;
}catch(const std::exception& e){fprintf(stderr,"%s\n",e.what());return 1;}}
