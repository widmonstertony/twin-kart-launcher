#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include "ClientBuildGuard.h"

// Read-only inventory through the installed client's IAssetRegistry. No Pak
// key extraction, resource export, map travel or world/gameplay mutation.
namespace {
constexpr char ClientSha[]="5041d9f5d45adb4b1c932d8056a89f70db7fa98f93614ac30685cd1adeb9d9c2";
BYTE* image=nullptr; HANDLE logFile=INVALID_HANDLE_VALUE;
struct NativeString { wchar_t* data; int count,capacity; };
struct NativeArray { BYTE* data; int count,capacity; } snapshot{};
// One bounded native copy is retained until this owned process exits. Its tag
// values contain client-owned reference-counted data; do not guess their ABI
// destructors or free just the array while leaving its elements undisposed.
void Log(const std::string& row) {
    DWORD written=0; std::string line=row+"\r\n";
    if(!WriteFile(logFile,line.data(),static_cast<DWORD>(line.size()),&written,nullptr) || written!=line.size()) throw std::runtime_error("inventory log write failed");
}
std::string Json(const std::string& value) {
    std::string text="\"";
    for(unsigned char ch:value) {
        if(ch=='"' || ch=='\\') { text+='\\'; text+=static_cast<char>(ch); }
        else if(ch<32) { char escaped[7]; sprintf_s(escaped,"\\u%04x",ch); text+=escaped; }
        else text+=static_cast<char>(ch);
    }
    return text+'"';
}
bool SignaturesReady() {
    __try {
        const BYTE registry[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83};
        const BYTE name[]={0x48,0x89,0x54,0x24,0x10,0x53,0x48,0x83};
        return !memcmp(image+0x3ba2f20,registry,8) && !memcmp(image+0x3bcd370,name,8);
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool RegistryReady(void** result) {
    __try {
        void* registry=*reinterpret_cast<void**>(image+0x699f298);
        if(!registry) return false;
        BYTE** table=*reinterpret_cast<BYTE***>(registry);
        if(reinterpret_cast<BYTE*>(table)<image+0x4ed0000 || reinterpret_cast<BYTE*>(table)>=image+0x8657000 ||
            table[0x60/8]<image || table[0x60/8]>=image+0x4ed0000 || table[0x248/8]<image || table[0x248/8]>=image+0x4ed0000) return false;
        using Loading=bool(__fastcall*)(void*);
        if(reinterpret_cast<Loading>(table[0x248/8])(registry)) return false;
        *result=registry; return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool ReadSnapshot(void* registry) {
    __try {
        using All=bool(__fastcall*)(void*,NativeArray*,bool);
        auto table=*reinterpret_cast<void***>(registry);
        bool found=reinterpret_cast<All>(table[0x60/8])(registry,&snapshot,true);
        return found && snapshot.data && snapshot.count>0 && snapshot.count<=snapshot.capacity && snapshot.capacity<=1000000;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool ReadName(unsigned long long name,NativeString* text) {
    __try {
        using NameText=NativeString*(__fastcall*)(NativeString*,unsigned long long);
        reinterpret_cast<NameText>(image+0x3bcd370)(text,name);
        return text->data && text->count>0 && text->count<=1024 && text->capacity>=text->count;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
std::string Name(unsigned long long name) {
    NativeString value{};
    if(!ReadName(name,&value)) throw std::runtime_error("invalid native name");
    int bytes=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data,value.count-1,nullptr,0,nullptr,nullptr);
    std::string text(bytes>0?bytes:0,'\0');
    if(bytes>0) WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data,value.count-1,&text[0],bytes,nullptr,nullptr);
    using Free=void(__fastcall*)(void*); reinterpret_cast<Free>(image+0x248b950)(value.data);
    if(bytes<=0) throw std::runtime_error("invalid native text");
    return text;
}
bool AssetNames(int index,unsigned long long* object,unsigned long long* type) {
    __try {
        BYTE* asset=snapshot.data+static_cast<size_t>(index)*0x60;
        *object=*reinterpret_cast<unsigned long long*>(asset);
        *type=*reinterpret_cast<unsigned long long*>(asset+0x20); return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
DWORD WINAPI Worker(void*) {
    wchar_t path[32768]; DWORD size=GetEnvironmentVariableW(L"TWIN_KART_ASSET_LOG",path,ARRAYSIZE(path));
    if(!size || size>=ARRAYSIZE(path) || !TwinKartClient::Allowed(ClientSha)) return 1;
    logFile=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(logFile==INVALID_HANDLE_VALUE) return 2;
    image=reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr));
    try {
        Log(std::string("{\"event\":\"asset_registry_begin\",\"client_sha256\":\"")+ClientSha+"\",\"source\":\"native IAssetRegistry\"}");
        void* registry=nullptr;
        for(int attempt=0;attempt<720 && !registry;++attempt) { if(SignaturesReady()) RegistryReady(&registry); if(!registry) Sleep(250); }
        if(!registry || !ReadSnapshot(registry)) throw std::runtime_error("native registry unavailable or incomplete");
        std::unordered_map<unsigned long long,std::string> classes;
        unsigned worlds=0,paths=0,vehicleAssets=0;
        for(int index=0;index<snapshot.count;++index) {
            unsigned long long object=0,type=0;
            if(!AssetNames(index,&object,&type)) throw std::runtime_error("invalid native asset record");
            if(!object || !type) continue;
            auto item=classes.find(type);
            if(item==classes.end()) item=classes.emplace(type,Name(type)).first;
            bool world=item->second=="World",ai=item->second.find("AI")!=std::string::npos && item->second.find("Path")!=std::string::npos;
            // Include mesh and native vehicle definitions for garage audits.
            // A localized name or granted item ID is not proof of a model.
            bool vehicle=item->second=="SkeletalMesh" || item->second.find("Kart")!=std::string::npos ||
                item->second.find("Character")!=std::string::npos;
            if(!world && !ai && !vehicle) continue;
            std::string objectPath=Name(object);
            if(objectPath.empty() || objectPath[0]!='/') throw std::runtime_error("unexpected native asset path");
            Log(std::string("{\"event\":\"native_asset\",\"class\":")+Json(item->second)+",\"object_path\":"+Json(objectPath)+"}");
            if(world) ++worlds; else if(ai) ++paths; else ++vehicleAssets;
        }
        if(!worlds) throw std::runtime_error("native snapshot did not contain world assets");
        char end[384];
        sprintf_s(end,"{\"event\":\"asset_registry_complete\",\"assets\":%d,\"worlds\":%u,\"ai_path_assets\":%u,\"vehicle_assets\":%u,\"retained_snapshot_bytes\":%llu,\"all_tracks_driven\":false}",
            snapshot.count,worlds,paths,vehicleAssets,static_cast<unsigned long long>(snapshot.capacity)*0x60);
        Log(end);
    } catch(const std::exception&) {
        const char failure[]="{\"event\":\"asset_registry_failed\",\"complete\":false}\r\n"; DWORD written; WriteFile(logFile,failure,sizeof(failure)-1,&written,nullptr);
    }
    CloseHandle(logFile); logFile=INVALID_HANDLE_VALUE; return 0;
}
}
BOOL WINAPI DllMain(HINSTANCE self,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH) { DisableThreadLibraryCalls(self); HANDLE worker=CreateThread(nullptr,0,Worker,nullptr,0,nullptr); if(worker) CloseHandle(worker); }
    return TRUE;
}
