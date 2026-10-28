#pragma once
#include <windows.h>

namespace TwinKartNative {
struct ThreadTask {
    void (*execute)(void*);
    void* context;
    void (*cancel)(void*);
};
// Local message hook only: never installs a global hook or changes input. A
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
    bool accepting=false;
    void ExecuteOne();
    static LRESULT CALLBACK MessageHook(int,WPARAM,LPARAM);
public:
    GameThreadQueue()=default;
    GameThreadQueue(const GameThreadQueue&)=delete;
    GameThreadQueue& operator=(const GameThreadQueue&)=delete;
    bool Install(HWND ownedWindow,HINSTANCE callbackModule);
    bool Submit(ThreadTask task);
    void Stop();
    DWORD ThreadId() const { return thread; }
};
}
