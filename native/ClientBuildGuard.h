#pragma once
#include <windows.h>
#include <bcrypt.h>
#include <cstdio>
#include <cstring>
namespace TwinKartClient {
inline bool FileHasHash(const wchar_t* path,const char* expectedSha) {
    if(!path || !expectedSha || strlen(expectedSha)!=64) return false;
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,nullptr);
    if(file==INVALID_HANDLE_VALUE) return false;
    BCRYPT_ALG_HANDLE algorithm=nullptr; BCRYPT_HASH_HANDLE hash=nullptr; bool ok=false;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0 && BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0) {
        BYTE bytes[65536],digest[32]; DWORD read=0; bool valid=true;
        for(;;) {
            if(!ReadFile(file,bytes,sizeof(bytes),&read,nullptr)) { valid=false; break; }
            if(!read) break;
            if(BCryptHashData(hash,bytes,read,0)<0) { valid=false; break; }
        }
        if(valid && BCryptFinishHash(hash,digest,32,0)>=0) {
            char text[65]; for(int index=0;index<32;++index) sprintf_s(text+index*2,65-index*2,"%02x",digest[index]);
            ok=!strcmp(text,expectedSha);
        }
    }
    if(hash) BCryptDestroyHash(hash); if(algorithm) BCryptCloseAlgorithmProvider(algorithm,0); CloseHandle(file); return ok;
}
inline bool Allowed(const char* expectedSha) {
    wchar_t expected[32768],actual[32768];
    DWORD count=GetEnvironmentVariableW(L"TWIN_KART_ALLOWED_GAME",expected,ARRAYSIZE(expected));
    DWORD length=GetModuleFileNameW(nullptr,actual,ARRAYSIZE(actual));
    return count && count<ARRAYSIZE(expected) && length && length<ARRAYSIZE(actual) && !_wcsicmp(expected,actual) && FileHasHash(actual,expectedSha);
}
}
