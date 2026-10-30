#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <atomic>
#include <cstdio>
#include <stdexcept>
#include "../native/GameThreadQueue.h"
using TwinKartNative::GameThreadQueue;
namespace {
HANDLE ready,pump,done;
HWND window=nullptr;
std::atomic<unsigned> executed{0},canceled{0},ordinary{0};
std::atomic<unsigned> inputSeen{0},inputDelivered{0};
DWORD inputThread=0;
bool Input(const MSG& message) {
    if(message.message!=WM_KEYDOWN || message.wParam!=VK_ESCAPE)return false;
    inputThread=GetCurrentThreadId();++inputSeen;return true;
}
DWORD ownerThread=0;
LRESULT CALLBACK WindowProc(HWND hwnd,UINT message,WPARAM w,LPARAM l) {
    if(message==WM_APP+77 && w==42 && l==123) ++ordinary;
    if(message==WM_KEYDOWN && w==VK_ESCAPE)++inputDelivered;
    if(message==WM_CLOSE) { DestroyWindow(hwnd); PostQuitMessage(0); return 0; }
    return DefWindowProcW(hwnd,message,w,l);
}
DWORD WINAPI Owner(void*) {
    ownerThread=GetCurrentThreadId();
    WNDCLASSW type{}; type.lpfnWndProc=WindowProc; type.hInstance=GetModuleHandleW(nullptr); type.lpszClassName=L"TwinKartQueueFixture";
    RegisterClassW(&type);
    window=CreateWindowExW(0,type.lpszClassName,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,type.hInstance,nullptr);
    SetEvent(ready); WaitForSingleObject(pump,5000);
    MSG message;
    while(GetMessageW(&message,nullptr,0,0)>0) { TranslateMessage(&message); DispatchMessageW(&message); }
    UnregisterClassW(type.lpszClassName,type.hInstance); return 0;
}
struct Context { DWORD thread; unsigned calls; bool nested; } contexts[GameThreadQueue::Capacity]{};
void Execute(void* raw) {
    auto& task=*static_cast<Context*>(raw); task.thread=GetCurrentThreadId(); ++task.calls;
    if(task.nested) { MSG message; PeekMessageW(&message,nullptr,0,0,PM_REMOVE); }
    if(++executed==GameThreadQueue::Capacity) SetEvent(done);
}
void Canceled(void*) { ++canceled; }
void Throw(void*) { throw std::runtime_error("synthetic task failure"); }
void Need(bool value,const char* text) { if(!value) throw std::runtime_error(text); }
}
int main() {
    GameThreadQueue queue;
    HANDLE owner=nullptr;
    try {
        ready=CreateEventW(nullptr,TRUE,FALSE,nullptr); pump=CreateEventW(nullptr,TRUE,FALSE,nullptr); done=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        owner=CreateThread(nullptr,0,Owner,nullptr,0,nullptr);
        Need(WaitForSingleObject(ready,5000)==WAIT_OBJECT_0 && window,"Fixture window failed");
        Need(!queue.Install(GetDesktopWindow(),GetModuleHandleW(nullptr)),"Foreign-process desktop accepted");
        Need(queue.InstallError()==ERROR_INVALID_WINDOW_HANDLE,"Foreign owner rejection lost diagnostic");
        Need(queue.Install(window,nullptr,Input),"Same-process null-module hook installation failed");
        Need(queue.InstallError()==ERROR_SUCCESS,"Successful installation retained old error");
        GameThreadQueue duplicate;
        Need(!duplicate.Install(window,nullptr) && duplicate.InstallError()==ERROR_ALREADY_EXISTS,"Duplicate dispatcher accepted");
        Need(queue.ThreadId()==ownerThread,"Wrong window owner thread");
        for(unsigned i=0;i<GameThreadQueue::Capacity;++i) { contexts[i].nested=i==0; Need(queue.Submit({Execute,&contexts[i],Canceled}),"Bounded task rejected"); }
        Need(!queue.Submit({Execute,&contexts[0],Canceled}),"Queue capacity not enforced");
        PostMessageW(window,WM_APP+77,42,123); SetEvent(pump);
        Need(WaitForSingleObject(done,5000)==WAIT_OBJECT_0,"Tasks did not drain after nested message pumping");
        for(const auto& task:contexts) Need(task.thread==ownerThread && task.calls==1,"Task ran on wrong thread or more than once");
        Need(queue.Submit({Throw,nullptr,Canceled}),"Throwing fixture rejected");
        for(int i=0;i<500 && (!canceled || !ordinary);++i) Sleep(10);
        Need(canceled==1 && ordinary==1,"Task exception escaped or ordinary message changed");
        PostMessageW(window,WM_KEYDOWN,VK_ESCAPE,1);
        for(int i=0;i<500 && !inputSeen;++i)Sleep(10);
        Need(inputSeen==1 && inputDelivered==0 && inputThread==ownerThread,"Owned Escape handler did not consume exactly once on the owner thread");
        queue.Stop(); Need(!queue.Submit({Execute,&contexts[0],Canceled}),"Task accepted after stop");
        PostMessageW(window,WM_CLOSE,0,0); Need(WaitForSingleObject(owner,5000)==WAIT_OBJECT_0,"Fixture owner did not stop");
        CloseHandle(owner); CloseHandle(ready); CloseHandle(pump); CloseHandle(done);
        puts("PASS local game-thread queue: same-process ownership, exact owner-thread execution, capacity, nested pumps, ordinary message preservation, exception containment and shutdown. No game started."); return 0;
    } catch(const std::exception& error) {
        queue.Stop(); if(window) PostMessageW(window,WM_CLOSE,0,0); SetEvent(pump);
        if(owner) { WaitForSingleObject(owner,5000); CloseHandle(owner); }
        fprintf(stderr,"%s\n",error.what()); return 1;
    }
}
