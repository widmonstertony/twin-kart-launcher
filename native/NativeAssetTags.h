#pragma once
#include <windows.h>
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace TwinKartAssets {
struct TagString {wchar_t* data=nullptr;int count=0,capacity=0;};
static_assert(sizeof(TagString)==16,"Original x64 FString output ABI");
// Native UAssetRegistryHelpers::GetTagValue receives FAssetData and FName by
// const reference, and an original-allocator FString output. The borrowed
// snapshot's shared tag storage is neither copied nor destroyed here.
using GetTagValue=bool(__fastcall*)(const void*,const std::uint64_t*,TagString*);
using FreeTagString=void(__fastcall*)(void*);
struct TagReadResult {bool found=false;unsigned characters=0;};
// 1=present (possibly empty),0=absent,-1=invalid/fault,-2=output too small.
// Never return a partial tag as a complete resource dependency description.
inline int CopyTagValue(const void* asset,std::uint64_t name,GetTagValue read,FreeTagString release,
                        wchar_t* output,std::size_t capacity,TagReadResult& result) {
    result={};if(output && capacity)output[0]=0;
    if(!asset || !name || !read || !release || !output || !capacity || capacity>1048576)return -1;
    TagString value{};int status=-1;
    __try {
        __try {
            bool found=read(asset,&name,&value);
            if(value.count<0 || value.capacity<value.count || value.capacity>1048576 ||
               (value.count && !value.data))return -1;
            if(!found)return 0;
            if(value.count && value.data[value.count-1]!=0)return -1;
            for(int index=0;index<value.count-1;++index)if(!value.data[index])return -1;
            result.found=true;result.characters=value.count?static_cast<unsigned>(value.count-1):0;
            if(result.characters>=capacity)return -2;
            if(value.count)memcpy(output,value.data,static_cast<std::size_t>(value.count)*sizeof(wchar_t));
            status=1;
        } __finally {if(value.data)release(value.data);}
    } __except(EXCEPTION_EXECUTE_HANDLER) {output[0]=0;result={};return -1;}
    return status;
}
}
