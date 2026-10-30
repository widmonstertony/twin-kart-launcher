#pragma once
#include <windows.h>
#include <cwchar>
#include <cstring>
#include "NativeTrackModes.h"

namespace TwinKartRace {
struct NativeTrackEntry {wchar_t alias[97]{};unsigned modeMask=0;int laps=0;};
struct NativeTrackRows {const void** data;int count,capacity;};
struct NativeTrackEnumeration {int rows=0,count=0,nonRacing=0,failureStage=0,rowIndex=-1,failureValue=0;};
using GetNativeTrackRows=void*(__fastcall*)(void*,NativeTrackRows*,unsigned char);
using FreeNativeTrackRows=void(__fastcall*)(void*);
// The exact-build table getter can briefly reject table 0x14 while the lobby
// data root is being populated. Only that empty stage-2 condition is safe to
// retry. Record validation failures, truncated output and later-stage faults
// indicate incompatible/corrupt metadata and must remain terminal.
inline bool RetryNativeTrackEnumeration(int status,const NativeTrackEnumeration& value) {
    return status==0 || (status==-1 && value.failureStage==2 && value.rows==0 &&
        value.count==0 && value.nonRacing==0 && value.rowIndex==-1 && value.failureValue==0);
}
// Exact PC table consumer: GetTrackDef selects table0x14 and compares the
// requested alias directly with each 0x160-byte record's first FString.
// GetRows returns an owned pointer array with borrowed records. Copy only
// bounded scalar metadata on the game thread, then free that array using the
// original allocator. No borrowed record pointer escapes this call.
// 1=complete metadata,0=not ready,-1=malformed/fault,-2=output too small.
inline int EnumerateNativeTracks(void* root,GetNativeTrackRows get,FreeNativeTrackRows release,
                                NativeTrackEntry* entries,int capacity,NativeTrackEnumeration& output) {
    output={};output.failureStage=1;if(!root || !get || !release || !entries || capacity<1 || capacity>10000)return -1;
    NativeTrackRows rows{};NativeTrackEnumeration result{};
    __try {
        __try {
            output.failureStage=2;
            if(get(root,&rows,0x14)!=&rows || rows.count<0 || rows.count>10000 || rows.capacity<rows.count || rows.capacity>20000 ||
               (rows.count && !rows.data))return -1;
            if(!rows.count)return 0;
            result.rows=rows.count;
            output.rows=rows.count;
            for(int index=0;index<rows.count;++index) {
                output.rowIndex=index;output.failureStage=3;
                const auto record=static_cast<const unsigned char*>(rows.data[index]);unsigned mask=0;
                if(!ReadNativeTrackModeMask(record,0x160,mask))return -1;
                // Retain event maps (Spitem/Flag) in discovery. Their explicit
                // mode masks must still pass the separate race-mode adapter;
                // discovery does not make them ordinary speed/item tracks.
                if((mask&1) || !(mask&30)){++result.nonRacing;continue;}
                struct String {const wchar_t* data;int count,capacity;} name{};
                output.failureStage=4;
                memcpy(&name,record,sizeof(name));
                if(!name.data || name.count<2 || name.count>97 || name.capacity<name.count || name.data[name.count-1])return -1;
                output.failureStage=5;for(int i=0;i<name.count-1;++i) {
                    wchar_t ch=name.data[i];bool alpha=(ch>=L'A' && ch<=L'Z') || (ch>=L'a' && ch<=L'z');
                    if(!alpha && (i==0 || !((ch>=L'0' && ch<=L'9') || ch==L'_')))return -1;
                }
                output.failureStage=6;int laps=0;memcpy(&laps,record+0x10,sizeof(laps));if(!ValidNativeTrackLaps(laps)){output.failureValue=laps;return -1;}
                output.failureStage=7;
                for(int i=0;i<result.count;++i)if(!_wcsicmp(entries[i].alias,name.data))return -1;
                if(result.count==capacity)return -2;
                auto& entry=entries[result.count++];entry={};
                memcpy(entry.alias,name.data,static_cast<size_t>(name.count)*sizeof(wchar_t));entry.modeMask=mask;entry.laps=laps;
            }
            output=result;return 1;
        } __finally {if(rows.data)release(rows.data);}
    } __except(EXCEPTION_EXECUTE_HANDLER) {output.count=0;return -1;}
}
}
