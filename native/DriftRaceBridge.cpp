#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_set>
#include <stdexcept>
#include "ClientBuildGuard.h"
#include "GameThreadQueue.h"
#include "DriftObjectIdentity.h"
#include "RaceControl.h"

// Original game-thread bridge for exact-build native race preparation work.
// Track lookup and an opt-in native local-host candidate. A travel request is
// not evidence of AI driving, a completed race or native settlement.
namespace {
constexpr char ClientSha[]="5041d9f5d45adb4b1c932d8056a89f70db7fa98f93614ac30685cd1adeb9d9c2";
BYTE* image=nullptr; HINSTANCE module=nullptr; HANDLE output=INVALID_HANDLE_VALUE;
TwinKartNative::GameThreadQueue queue;
struct NativeString { const wchar_t* data; int count,capacity; };
struct Query { std::wstring alias; wchar_t level[512]{}; int status=-1; DWORD executedThread=0; HANDLE complete=nullptr; };
struct StartQuery { TwinKartRace::Request request; wchar_t track[97]{},options[512]{}; int status=-1; DWORD executedThread=0; HANDLE complete=nullptr; };
struct NativeMode { unsigned char mode; unsigned char padding[7]; NativeString profile; };
static_assert(sizeof(NativeMode)==24,"Supported native game mode layout");
// All object access remains inside this game's window thread. No object pointer
// survives a queued task or a level transition.
BYTE* Traveler() {
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);
    int count=*reinterpret_cast<int*>(image+0x69c28c4);
    if(!chunks || count<1 || count>2000000) return nullptr;
    BYTE* found=nullptr;
    for(int index=0;index<count;++index) {
        auto chunk=chunks[index/65536]; if(!chunk) continue;
        auto& slot=chunk[index%65536]; auto object=static_cast<BYTE*>(slot.object);
        if(!object || (slot.flags&0x30000000) || *reinterpret_cast<void**>(object)!=image+0x52618c0 ||
           (*reinterpret_cast<unsigned*>(object+8)&0x18030) || *reinterpret_cast<int*>(object+0xc)!=index) continue;
        if(found) return nullptr; // Never pick an ambiguous subsystem instance.
        found=object;
    }
    return found;
}
// 0=model not ready; 1=native travel submitted; 2=definition absent;
// -1=native fault; -2=unsupported transform; no value means race acceptance.
int NativeStart(const wchar_t* track,const wchar_t* options,int mode) {
    __try {
        BYTE* traveler=Traveler();
        using Model=void*(__fastcall*)(void*);
        if(!traveler || !reinterpret_cast<Model>(image+0x1ba3cf0)(traveler)) return 0;
        if(wcsstr(track,L"_Mirror") || wcsstr(track,L"_rvs")) return -2;
        NativeString alias{track,static_cast<int>(wcslen(track)+1),static_cast<int>(wcslen(track)+1)};
        alignas(16) BYTE definition[0x160]{};
        using Get=void*(__fastcall*)(void*,const NativeString*);
        using Destroy=void(__fastcall*)(void*);
        if(reinterpret_cast<Get>(image+0x1472ce0)(definition,&alias)!=definition) return -1;
        const auto level=*reinterpret_cast<NativeString*>(definition);
        if(!level.data || level.count<=1 || level.count>512 || level.capacity<level.count || level.data[level.count-1]) {
            reinterpret_cast<Destroy>(image+0x1620210)(definition); return 2;
        }
        unsigned long long levelName=0;
        using Name=void*(__fastcall*)(void*,const wchar_t*,int);
        reinterpret_cast<Name>(image+0x2589130)(&levelName,level.data,1);
        // Retain the client's character/kart setup and its native allocation,
        // reference-count and preview rules instead of cloning vendor structs.
        using Prepare=void(__fastcall*)(void*,const void*);
        reinterpret_cast<Prepare>(image+0x1bba570)(traveler,definition);
        reinterpret_cast<Destroy>(image+0x1620210)(definition);
        void* details=*reinterpret_cast<void**>(traveler+0x360);
        if(!details) return -1;
        NativeMode selected{}; selected.mode=static_cast<unsigned char>(mode);
        NativeString owned{};
        using Copy=void*(__fastcall*)(NativeString*,const NativeString*);
        reinterpret_cast<Copy>(image+0x9120e0)(&owned,&alias);
        using Initialize=bool(__fastcall*)(void*,const NativeMode*,NativeString*,void*,const void*,bool);
        if(!reinterpret_cast<Initialize>(image+0x1bba140)(traveler,&selected,&owned,details,traveler+0x368,true)) return -1;
        NativeString optionText{options,static_cast<int>(wcslen(options)+1),static_cast<int>(wcslen(options)+1)};
        using Open=void(__fastcall*)(const void*,unsigned long long,bool,const NativeString*);
        reinterpret_cast<Open>(image+0x3f50670)(traveler,levelName,true,&optionText);
        return 1;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return -1; }
}
void ExecuteStart(void* raw) {
    auto& value=*static_cast<StartQuery*>(raw); value.executedThread=GetCurrentThreadId();
    value.status=NativeStart(value.track,value.options,value.request.mode); SetEvent(value.complete);
}
void CancelStart(void* raw) { auto& value=*static_cast<StartQuery*>(raw); value.status=-1; SetEvent(value.complete); }
// 0=not ready, 1=valid native name, 2=empty native definition, -1=fault/cancel.
int NativeTrack(const wchar_t* name,wchar_t* level) {
    __try {
        using Root=void*(__fastcall*)();
        if(!reinterpret_cast<Root>(image+0x1449110)()) return 0;
        size_t length=wcslen(name); NativeString alias{name,static_cast<int>(length+1),static_cast<int>(length+1)};
        alignas(16) BYTE record[0x160]{};
        using Get=void*(__fastcall*)(void*,const NativeString*);
        using Destroy=void(__fastcall*)(void*);
        void* returned=reinterpret_cast<Get>(image+0x1472ce0)(record,&alias);
        int result=-1;
        if(returned==record) {
            auto value=reinterpret_cast<const NativeString*>(record);
            if(value->count==0) result=2;
            else if(value->data && value->count>0 && value->count<=512 && value->capacity>=value->count && value->data[value->count-1]==0) {
                memcpy(level,value->data,static_cast<size_t>(value->count)*sizeof(wchar_t)); result=value->count>1?1:2;
            }
        }
        // Native copy constructor/default initialization and destructor use the
        // client's allocator/reference-count rules. No shallow struct clone.
        reinterpret_cast<Destroy>(image+0x1620210)(record);
        return result;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return -1; }
}
void Execute(void* raw) {
    auto& value=*static_cast<Query*>(raw); value.executedThread=GetCurrentThreadId();
    value.status=NativeTrack(value.alias.c_str(),value.level); SetEvent(value.complete);
}
void Cancel(void* raw) { auto& value=*static_cast<Query*>(raw); value.status=-1; SetEvent(value.complete); }
bool SignaturesReady() {
    __try {
        const BYTE get[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74};
        const BYTE destroy[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74};
        const BYTE initialize[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c};
        const BYTE prepare[]={0x48,0x89,0x5c,0x24,0x18,0x55,0x56,0x57};
        const BYTE name[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83};
        const BYTE open[]={0x48,0x89,0x54,0x24,0x10,0x55,0x53,0x56};
        return !memcmp(image+0x1472ce0,get,8) && !memcmp(image+0x1620210,destroy,8) && !memcmp(image+0x1bba140,initialize,8) &&
            !memcmp(image+0x1bba570,prepare,8) && !memcmp(image+0x2589130,name,8) && !memcmp(image+0x3f50670,open,8);
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
BOOL CALLBACK FindWindow(HWND window,LPARAM raw) {
    DWORD process=0; GetWindowThreadProcessId(window,&process);
    if(process!=GetCurrentProcessId()) return TRUE;
    wchar_t name[64]{}; GetClassNameW(window,name,ARRAYSIZE(name));
    if(wcscmp(name,L"UnrealWindow")) return TRUE;
    *reinterpret_cast<HWND*>(raw)=window; return FALSE;
}
std::string Utf8(const wchar_t* value) {
    int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value,-1,nullptr,0,nullptr,nullptr);
    if(size<=0) throw std::runtime_error("Invalid native text");
    std::string result(static_cast<size_t>(size),'\0');
    if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value,-1,&result[0],size,nullptr,nullptr)) throw std::runtime_error("Native text conversion failed");
    result.pop_back(); return result;
}
std::string Json(const std::string& value) {
    std::string result="\"";
    for(unsigned char ch:value) {
        if(ch=='"' || ch=='\\') { result+='\\'; result+=static_cast<char>(ch); }
        else if(ch<32) { char code[7]; sprintf_s(code,"\\u%04x",ch); result+=code; }
        else result+=static_cast<char>(ch);
    }
    return result+'"';
}
void Log(const std::string& line) {
    std::string bytes=line+"\r\n"; DWORD written=0;
    if(!WriteFile(output,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr) || written!=bytes.size()) throw std::runtime_error("Bridge log write failed");
}
std::vector<std::string> Queries(const wchar_t* path) {
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if(file==INVALID_HANDLE_VALUE) throw std::runtime_error("Track query file unavailable");
    LARGE_INTEGER size{}; bool valid=GetFileSizeEx(file,&size) && size.QuadPart>0 && size.QuadPart<=65536;
    std::string bytes(valid?static_cast<size_t>(size.QuadPart):0,'\0'); DWORD read=0;
    bool ok=valid && ReadFile(file,&bytes[0],static_cast<DWORD>(bytes.size()),&read,nullptr) && read==bytes.size(); CloseHandle(file);
    if(!ok) throw std::runtime_error("Invalid track query input");
    std::vector<std::string> values; std::unordered_set<std::string> seen;
    size_t start=0;
    while(start<bytes.size()) {
        size_t end=bytes.find('\n',start); if(end==std::string::npos) end=bytes.size();
        std::string value=bytes.substr(start,end-start); if(!value.empty() && value.back()=='\r') value.pop_back(); start=end+1;
        if(value.empty()) continue;
        if(value.size()>96 || !((value[0]>='A' && value[0]<='Z') || (value[0]>='a' && value[0]<='z'))) throw std::runtime_error("Invalid track alias");
        for(char ch:value) if(!((ch>='a' && ch<='z') || (ch>='A' && ch<='Z') || (ch>='0' && ch<='9') || ch=='_')) throw std::runtime_error("Invalid track alias");
        if(seen.insert(value).second) values.push_back(value);
        if(values.size()>1000) throw std::runtime_error("Track query limit");
    }
    if(values.empty()) throw std::runtime_error("Empty track query input");
    return values;
}
void WatchControl(const wchar_t* path) {
    char previous[33]{};
    for(;;) {
        Sleep(250);
        HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);
        if(file==INVALID_HANDLE_VALUE) continue;
        LARGE_INTEGER size{}; char bytes[257]{}; DWORD read=0;
        bool valid=GetFileSizeEx(file,&size) && size.QuadPart>0 && size.QuadPart<=256 &&
            ReadFile(file,bytes,static_cast<DWORD>(size.QuadPart),&read,nullptr) && read==size.QuadPart;
        CloseHandle(file);
        StartQuery value;
        if(!valid || memchr(bytes,0,read) || !TwinKartRace::Parse(bytes,&value.request) || !strcmp(previous,value.request.identity)) continue;
        strcpy_s(previous,value.request.identity);
        size_t length=strlen(value.request.track);
        for(size_t i=0;i<length;++i) value.track[i]=value.request.track[i];
        if(!TwinKartRace::Options(value.request,value.options,ARRAYSIZE(value.options))) continue;
        value.complete=CreateEventW(nullptr,TRUE,FALSE,nullptr); if(!value.complete) throw std::runtime_error("Race event unavailable");
        for(int attempt=0;attempt<30;++attempt) {
            value.status=-1; ResetEvent(value.complete);
            if(!queue.Submit({ExecuteStart,&value,CancelStart}) || WaitForSingleObject(value.complete,5000)!=WAIT_OBJECT_0) { queue.Stop(); break; }
            if(value.status!=0) break;
            Sleep(1000);
        }
        if(value.executedThread!=queue.ThreadId()) { queue.Stop(); CloseHandle(value.complete); throw std::runtime_error("Race task not dispatched"); }
        CloseHandle(value.complete);
        Log(std::string("{\"event\":\"native_local_host_request\",\"request_id\":")+Json(value.request.identity)+",\"track_id\":"+Json(value.request.track)+
            ",\"mode\":"+std::to_string(value.request.mode)+",\"ai\":"+std::to_string(value.request.ai)+",\"status\":"+std::to_string(value.status)+
            ",\"travel_requested\":"+(value.status==1?"true":"false")+",\"race_verified\":false,\"game_thread\":"+std::to_string(value.executedThread)+"}");
    }
}
DWORD WINAPI Worker(void*) {
    wchar_t path[32768]{},input[32768]{},control[32768]{};
    DWORD a=GetEnvironmentVariableW(L"TWIN_KART_RACE_BRIDGE_LOG",path,ARRAYSIZE(path));
    DWORD b=GetEnvironmentVariableW(L"TWIN_KART_TRACK_QUERIES",input,ARRAYSIZE(input));
    DWORD c=GetEnvironmentVariableW(L"TWIN_KART_RACE_CONTROL",control,ARRAYSIZE(control));
    if(!a || a>=ARRAYSIZE(path) || b>=ARRAYSIZE(input) || c>=ARRAYSIZE(control) || (!b && !c) || !TwinKartClient::Allowed(ClientSha)) return 1;
    output=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,0,nullptr);
    if(output==INVALID_HANDLE_VALUE) return 2;
    image=reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr));
    try {
        auto aliases=b?Queries(input):std::vector<std::string>{}; HWND window=nullptr;
        for(int attempt=0;attempt<480 && !window;++attempt) { if(SignaturesReady()) EnumWindows(FindWindow,reinterpret_cast<LPARAM>(&window)); if(!window) Sleep(250); }
        if(!window || !queue.Install(window,module)) throw std::runtime_error("Owned game-thread message hook unavailable");
        Log(std::string("{\"event\":\"native_race_bridge_ready\",\"client_sha256\":\"")+ClientSha+"\",\"can_start_race\":false,\"experimental_local_host\":"+(c?"true":"false")+",\"game_thread\":"+std::to_string(queue.ThreadId())+"}");
        unsigned found=0,empty=0; bool initialized=false;
        for(const auto& alias:aliases) {
            Query value; value.alias.assign(alias.begin(),alias.end()); value.complete=CreateEventW(nullptr,TRUE,FALSE,nullptr);
            if(!value.complete) throw std::runtime_error("Query event failed");
            bool success=false;
            for(int attempt=0;attempt<(initialized?1:90);++attempt) {
                value.status=-1; ResetEvent(value.complete);
                if(!queue.Submit({Execute,&value,Cancel}) || WaitForSingleObject(value.complete,5000)!=WAIT_OBJECT_0) { queue.Stop(); break; }
                if(value.status==1 || initialized || value.status<0) { success=value.status>=1; break; }
                if(attempt==0) Log(std::string("{\"event\":\"native_track_lookup_waiting\",\"status\":")+std::to_string(value.status)+",\"game_thread\":"+std::to_string(value.executedThread)+"}");
                Sleep(1000);
            }
            if(!success || value.executedThread!=queue.ThreadId()) { queue.Stop(); CloseHandle(value.complete); throw std::runtime_error("Native track definition unavailable"); }
            initialized=true; CloseHandle(value.complete);
            Log(std::string("{\"event\":\"native_track_definition\",\"track_id\":")+Json(alias)+",\"status\":"+std::to_string(value.status)+",\"native_level_name\":"+Json(Utf8(value.level))+",\"game_thread\":"+std::to_string(value.executedThread)+"}");
            if(value.status==1) ++found; else ++empty;
        }
        Log(std::string("{\"event\":\"native_track_queries_complete\",\"found\":")+std::to_string(found)+",\"empty\":"+std::to_string(empty)+",\"race_started\":false}");
        if(c) WatchControl(control);
    } catch(const std::exception&) {
        const char line[]="{\"event\":\"native_race_bridge_failed\",\"can_start_race\":false}\r\n"; DWORD written; WriteFile(output,line,sizeof(line)-1,&written,nullptr);
    }
    queue.Stop(); CloseHandle(output); output=INVALID_HANDLE_VALUE; return 0;
}
}
BOOL WINAPI DllMain(HINSTANCE self,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH) { module=self; DisableThreadLibraryCalls(self); HANDLE worker=CreateThread(nullptr,0,Worker,nullptr,0,nullptr); if(worker) CloseHandle(worker); }
    return TRUE;
}
