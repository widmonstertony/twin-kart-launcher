#include "../native/NativeEquipmentPreset.h"
#include <array>
#include <string>
#include <stdexcept>
#include <cstdio>
#include <limits>
namespace E=TwinKartEquipment;
void Need(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
E::String Text(const wchar_t* text){int count=static_cast<int>(wcslen(text)+1);return {text,count,count};}
struct Pair {E::String key,value;int next=-1,hash=0;};
static_assert(sizeof(Pair)==40,"Independently observed native map pair stride");
struct Fixture {
    alignas(8) BYTE preset[0xa0]{};
    std::array<Pair,160> pairs{};
    std::array<unsigned,5> bits{};
    int count=0,used=0;
    Fixture(){*reinterpret_cast<Pair**>(preset)=pairs.data();*reinterpret_cast<int*>(preset+12)=160;
        *reinterpret_cast<unsigned**>(preset+0x20)=bits.data();*reinterpret_cast<int*>(preset+0x2c)=160;}
    void Add(int slot,const wchar_t* key,const wchar_t* value){pairs[slot]={Text(key),Text(value),-1,0};bits[slot/32]|=1u<<(slot%32);++used;if(slot>=count)count=slot+1;Commit();}
    void Commit(){*reinterpret_cast<int*>(preset+8)=count;*reinterpret_cast<int*>(preset+0x28)=count;*reinterpret_cast<int*>(preset+0x34)=count-used;}
    void Paint(const wchar_t* body,const wchar_t* id,int revision){*reinterpret_cast<E::String*>(preset+0x60)=Text(id);*reinterpret_cast<E::String*>(preset+0x78)=Text(body);*reinterpret_cast<int*>(preset+0x70)=revision;}
};
int allocations=0;
void* __fastcall Copy(void* raw,const void* source){auto& dst=*static_cast<E::String*>(raw);auto& src=*static_cast<const E::String*>(source);
    wchar_t* memory=nullptr;if(src.count){memory=static_cast<wchar_t*>(HeapAlloc(GetProcessHeap(),0,src.count*sizeof(wchar_t)));Need(memory!=nullptr,"Fixture native allocator");++allocations;memcpy(memory,src.data,src.count*sizeof(wchar_t));}
    if(dst.data){Need(HeapFree(GetProcessHeap(),0,const_cast<wchar_t*>(dst.data))!=FALSE,"Fixture native release");--allocations;}
    dst={memory,src.count,src.count};return raw;}
int main(){try{
    Fixture f;f.Add(0,L"Character",L"ChosenCharacter");f.Add(4,L"Costume",L"ChosenCostume");f.Add(129,L"Kart",L"ChosenKart");
    // A free slot deliberately contains invalid stale fields. It must never be read.
    f.pairs[2].key={reinterpret_cast<const wchar_t*>(1),100,100};f.Paint(L"ChosenKart",L"LocalPaint",7);
    E::Preset result{};Need(E::Snapshot(f.preset,result),"Sparse preset rejected valid high-index native equipment");
    Need(E::Equal(result.kart[0],Text(L"ChosenKart")) && result.revision==7 && E::Nonempty(result.livery),"Selected body/livery lost");
    auto previous=result;f.Add(5,L"Kart",L"Duplicate");Need(!E::Snapshot(f.preset,result) && !memcmp(&previous,&result,sizeof(result)),"Duplicate selection mutated snapshot");
    f.bits[0]&=~(1u<<5);--f.used;f.Commit();
    f.Paint(L"PreviousKart",L"OtherPaint",99);Need(E::Snapshot(f.preset,result) && !E::Nonempty(result.livery) && result.revision==0,"Paint leaked between kart bodies");
    *reinterpret_cast<int*>(f.preset+0x34)=0;Need(!E::Snapshot(f.preset,result),"Sparse allocation/free-count mismatch accepted");f.Commit();
    *reinterpret_cast<int*>(f.preset+0x28)=1;Need(!E::Snapshot(f.preset,result),"Allocation bitmap size mismatch accepted");f.Commit();
    f.pairs[129].value.count=-1;Need(!E::Snapshot(f.preset,result),"Malformed selected native FString accepted");f.pairs[129].value=Text(L"ChosenKart");
    Need(!E::Snapshot(reinterpret_cast<void*>(1),result),"Expired preset pointer escaped bounded exception guard");
    Fixture full;for(unsigned i=0;i<6;++i)full.Add(i,E::CharacterKeys[i],E::CharacterKeys[i]);
    for(unsigned i=0;i<10;++i)full.Add(i+6,E::KartKeys[i],E::KartKeys[i]);full.Paint(L"Kart",L"LocalPaint",11);
    Need(E::Snapshot(full.preset,result),"Full equipment preset rejected");
    alignas(8) BYTE character[0x60]{},kart[0xb8]{};
    E::ApplyCharacter(character,result,Copy);E::ApplyKart(kart,result,Copy);
    for(unsigned i=0;i<6;++i){auto& field=*reinterpret_cast<E::String*>(character+i*16);Need(E::Equal(field,result.character[i]) && field.data!=result.character[i].data,"Character field missing or borrowed instead of deep-copied");}
    for(unsigned i=0;i<10;++i){auto& field=*reinterpret_cast<E::String*>(kart+E::KartOffsets[i]);Need(E::Equal(field,result.kart[i]) && field.data!=result.kart[i].data,"Kart field missing or borrowed instead of deep-copied");}
    Need(*reinterpret_cast<int*>(kart+0x90)==11 && E::Equal(*reinterpret_cast<E::String*>(kart+0x80),Text(L"LocalPaint")),"Paint identity/revision did not reach native descriptor");
    E::Preset cleared{};E::ApplyCharacter(character,cleared,Copy);E::ApplyKart(kart,cleared,Copy);Need(!allocations,"Native descriptor replacement leaked owned strings");
    Fixture inlineMap;inlineMap.Add(0,L"Character",L"C");inlineMap.Add(1,L"Costume",L"S");inlineMap.Add(2,L"Kart",L"K");
    *reinterpret_cast<unsigned**>(inlineMap.preset+0x20)=nullptr;*reinterpret_cast<unsigned*>(inlineMap.preset+0x10)=7;
    *reinterpret_cast<int*>(inlineMap.preset+0x2c)=128;Need(E::Snapshot(inlineMap.preset,result),"Native inline allocation bitmap rejected");
    inlineMap.count=129;inlineMap.Commit();Need(!E::Snapshot(inlineMap.preset,result),"Large sparse map used undersized inline bitmap");
    E::Body body{};float mass=100,force=10000000;memcpy(body.bytes,&mass,4);memcpy(body.bytes+4,&force,4);
    Need(E::ValidBody(body),"Finite tuned native body was arbitrarily capped");
    float invalid=std::numeric_limits<float>::quiet_NaN();memcpy(body.bytes+0xd4,&invalid,4);
    Need(!E::ValidBody(body),"Nonfinite actual native body field accepted");
    float zero=0;memcpy(body.bytes+0xd4,&zero,4);memcpy(body.bytes,&zero,4);Need(!E::ValidBody(body),"Invalid native mass accepted");
    puts("PASS native equipment preset boundary: sparse/inline maps, high slots, free-slot isolation, malformed/duplicate rejection without partial snapshot, exact PC character/kart fields, body-scoped livery and owned-string replacement. Synthetic native memory only; no game acceptance.");return 0;
}catch(const std::exception& e){fprintf(stderr,"%s\n",e.what());return 1;}}
