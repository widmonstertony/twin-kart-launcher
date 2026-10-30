#pragma once
#include <windows.h>

namespace TwinKartNative {
struct ThreadTask {
    void (*execute)(void*);
    void* context;
    void (*cancel)(void*);
};
// Local message hook only: never installs a global hook. An optional owned
// input handler may consume a message; without it all input is preserved. A
// task is run on the verified owner thread of a window in this same process.
class GameThreadQueue {
public:
    static constexpr unsigned Capacity=32;
private:
    SRWLOCK tasksLock=SRWLOCK_INIT;
    ThreadTask tasks[Capacity]{};
    unsigned head=0,count=0;
    HHOOK hook=nullptr;
    HWND window=nullptr;
    DWORD thread=0;
    DWORD installError=ERROR_SUCCESS;
    bool accepting=false;
    bool (*inputHandler)(const MSG&)=nullptr;
    void ExecuteOne();
    static LRESULT CALLBACK MessageHook(int,WPARAM,LPARAM);
public:
    GameThreadQueue()=default;
    GameThreadQueue(const GameThreadQueue&)=delete;
    GameThreadQueue& operator=(const GameThreadQueue&)=delete;
    bool Install(HWND ownedWindow,HINSTANCE callbackModule,bool (*input)(const MSG&)=nullptr);
    bool Submit(ThreadTask task);
    void Stop();
    DWORD ThreadId() const { return thread; }
    DWORD InstallError() const { return installError; }
};
}
