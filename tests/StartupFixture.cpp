#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifdef TWIN_KART_STARTUP_DLL
BOOL WINAPI DllMain(HINSTANCE,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH) {
        wchar_t name[128]{};GetEnvironmentVariableW(L"TWIN_KART_TEST_READY_EVENT",name,ARRAYSIZE(name));
        HANDLE event=OpenEventW(EVENT_MODIFY_STATE,FALSE,name);
        if(event){SetEvent(event);CloseHandle(event);}
    }
    return TRUE;
}
#else
int wmain() {
    wchar_t path[32768]{};DWORD n=GetEnvironmentVariableW(L"TWIN_KART_TEST_MAIN_MARKER",path,ARRAYSIZE(path));
    if(!n || n>=ARRAYSIZE(path))return 2;
    HANDLE file=CreateFileW(path,GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return 3;
    const char marker[]="primary-resumed";DWORD written=0;
    bool ok=WriteFile(file,marker,sizeof(marker)-1,&written,nullptr) && written==sizeof(marker)-1;
    CloseHandle(file);return ok?0:4;
}
#endif
