#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <cmath>

namespace TwinKartEquipment {
// Supported PC FKtItemPreset / TMap layout, independently checked against
// generated PC property metadata and its native FString map lookup. Borrowed
// strings live only during one serialized game-thread registration call.
struct String {const wchar_t* data=nullptr;int count=0,capacity=0;};
static_assert(sizeof(String)==16,"Supported PC FString layout");
struct Preset {String character[6]{},kart[10]{},livery{};int revision=0;};
struct Body {BYTE bytes[0xd8]{};};
static_assert(sizeof(Body)==0xd8,"Supported PC FKtKartBodyParam extent");
inline bool ValidBody(const Body& value) {
    // Float members from the PC structure's own generated properties. Packed
    // booleans, padding and integer ticks/slot capacities are not float data.
    constexpr unsigned fields[]{0,4,8,12,16,20,0x1c,0x20,0x24,0x28,0x2c,0x30,0x34,0x38,0x3c,0x40,0x44,
        0x48,0x4c,0x50,0x54,0x58,0x5c,0x60,0x64,0x68,0x6c,0x70,0x74,0x78,0x7c,0x80,0x84,0x88,
        0x90,0x98,0xa0,0xa4,0xb8,0xc0,0xc4,0xc8,0xcc,0xd0,0xd4};
    for(unsigned offset:fields){float number;memcpy(&number,value.bytes+offset,sizeof(number));if(!std::isfinite(number))return false;}
    float mass;memcpy(&mass,value.bytes,sizeof(mass));return mass>0;
}
inline constexpr const wchar_t* CharacterKeys[]{L"Character",L"Costume",L"MotionFinish",L"MotionRetire",L"MotionVictory",L"MotionDefeat"};
inline constexpr const wchar_t* KartKeys[]{L"Kart",L"PartFront",L"PartSide",L"PartBack",L"PartWheel",L"PartHandle",L"PartPlate",L"KartBooster",L"Balloon",L"SkidMark"};
inline constexpr unsigned KartOffsets[]{0,0x10,0x20,0x30,0x40,0x50,0x60,0x70,0x98,0xa8};
inline bool Valid(const String& value,int maximum=2048) {
    if(value.count<0 || value.capacity<value.count || value.capacity>1048576 || value.count>maximum)return false;
    if(!value.count)return true;
    if(!value.data || value.data[value.count-1])return false;
    for(int n=0;n<value.count-1;++n)if(!value.data[n])return false;
    return true;
}
inline bool Nonempty(const String& value){return value.count>1;}
inline bool Equal(const String& a,const String& b) {
    if(!Nonempty(a) && !Nonempty(b))return true;
    return a.count==b.count && a.data && b.data && !wmemcmp(a.data,b.data,a.count);
}
inline bool Snapshot(const void* raw,Preset& output) {
    __try {
        if(!raw)return false;const auto bytes=static_cast<const BYTE*>(raw);
        auto entries=*reinterpret_cast<const BYTE* const*>(bytes);
        int count=*reinterpret_cast<const int*>(bytes+8),capacity=*reinterpret_cast<const int*>(bytes+12);
        int bitCount=*reinterpret_cast<const int*>(bytes+0x28),bitCapacity=*reinterpret_cast<const int*>(bytes+0x2c);
        int freeCount=*reinterpret_cast<const int*>(bytes+0x34);
        if(count<0 || count>4096 || capacity<count || capacity>65536 || bitCount!=count || bitCapacity<bitCount ||
           bitCapacity>65536 || freeCount<0 || freeCount>count || (count && !entries))return false;
        auto bits=*reinterpret_cast<const unsigned* const*>(bytes+0x20);
        if(!bits){if(count>128)return false;bits=reinterpret_cast<const unsigned*>(bytes+0x10);}
        Preset next{};unsigned characterSeen=0,kartSeen=0;int allocated=0;
        // Validate the whole sparse map before any native descriptor is changed.
        // A free slot's stale pointer/length fields are never read.
        for(int index=0;index<count;++index) {
            if(!(bits[index/32]&(1u<<(index%32))))continue;++allocated;
            auto pair=entries+static_cast<size_t>(index)*40;
            const auto& key=*reinterpret_cast<const String*>(pair);
            const auto& value=*reinterpret_cast<const String*>(pair+16);
            if(!Valid(key,128) || !Nonempty(key) || !Valid(value))return false;
            for(unsigned field=0;field<6;++field)if(!wcscmp(key.data,CharacterKeys[field])) {
                if(characterSeen&(1u<<field))return false;characterSeen|=1u<<field;next.character[field]=value;
            }
            for(unsigned field=0;field<10;++field)if(!wcscmp(key.data,KartKeys[field])) {
                if(kartSeen&(1u<<field))return false;kartSeen|=1u<<field;next.kart[field]=value;
            }
        }
        if(allocated!=count-freeCount || !Nonempty(next.character[0]) || !Nonempty(next.character[1]) || !Nonempty(next.kart[0]))return false;
        const auto& livery=*reinterpret_cast<const String*>(bytes+0x60);
        const auto& liveryKart=*reinterpret_cast<const String*>(bytes+0x78);
        int revision=*reinterpret_cast<const int*>(bytes+0x70);
        if(!Valid(livery) || !Valid(liveryKart) || revision<0)return false;
        // Native liveries belong to a particular kart body. Never carry a
        // previous body's paint to the selected body or reinterpret timestamps.
        if(Equal(liveryKart,next.kart[0]) && Nonempty(livery)){next.livery=livery;next.revision=revision;}
        output=next;return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
using CopyString=void*(__fastcall*)(void*,const void*);
inline void ApplyCharacter(void* descriptor,const Preset& preset,CopyString copy) {
    for(unsigned field=0;field<6;++field)copy(static_cast<BYTE*>(descriptor)+field*16,&preset.character[field]);
}
inline void ApplyKart(void* descriptor,const Preset& preset,CopyString copy) {
    for(unsigned field=0;field<10;++field)copy(static_cast<BYTE*>(descriptor)+KartOffsets[field],&preset.kart[field]);
    copy(static_cast<BYTE*>(descriptor)+0x80,&preset.livery);
    memcpy(static_cast<BYTE*>(descriptor)+0x90,&preset.revision,sizeof(preset.revision));
}
}
