#define WIN32_LEAN_AND_MEAN
#include "GameThreadQueue.h"
#include <atomic>

namespace TwinKartNative {
namespace {
SRWLOCK dispatchLock=SRWLOCK_INIT;
std::atomic<GameThreadQueue*> installed{nullptr};
thread_local bool dispatching=false;
void Cancel(ThreadTask task) noexcept { if(task.cancel) { try { task.cancel(task.context); } catch(...) {} } }
}
bool GameThreadQueue::Install(HWND ownedWindow,HINSTANCE callbackModule,bool (*input)(const MSG&)) {
    (void)callbackModule; // The callback stays in this process; no DLL injection by USER32.
    installError=ERROR_SUCCESS;
    DWORD process=0; DWORD owner=GetWindowThreadProcessId(ownedWindow,&process);
    if(!ownedWindow || !owner || process!=GetCurrentProcessId()) { installError=ERROR_INVALID_WINDOW_HANDLE; return false; }
    AcquireSRWLockExclusive(&dispatchLock);
    if(installed.load() || hook) { installError=ERROR_ALREADY_EXISTS; ReleaseSRWLockExclusive(&dispatchLock); return false; }
    window=ownedWindow; thread=owner;
    inputHandler=input;
    installed.store(this,std::memory_order_release);
    // MSDN's same-process thread contract uses a null hMod. The DLL is already
    // loaded and pinned by the owned session, including early-loader startup.
    SetLastError(ERROR_SUCCESS);
    hook=SetWindowsHookExW(WH_GETMESSAGE,MessageHook,nullptr,owner);
    if(!hook)installError=GetLastError();
    AcquireSRWLockExclusive(&tasksLock); accepting=hook!=nullptr; ReleaseSRWLockExclusive(&tasksLock);
    if(!hook) { installed.store(nullptr); window=nullptr; thread=0; }
    bool result=hook!=nullptr; ReleaseSRWLockExclusive(&dispatchLock); return result;
}
bool GameThreadQueue::Submit(ThreadTask task) {
    if(!task.execute) return false;
    AcquireSRWLockExclusive(&tasksLock);
    bool accepted=accepting && count<Capacity;
    if(accepted) { tasks[(head+count)%Capacity]=task; ++count; }
    HWND target=window;
    ReleaseSRWLockExclusive(&tasksLock);
    if(accepted) PostMessageW(target,WM_NULL,0,0);
    return accepted;
}
void GameThreadQueue::ExecuteOne() {
    if(GetCurrentThreadId()!=thread) return;
    ThreadTask task{};
    AcquireSRWLockExclusive(&tasksLock);
    if(accepting && count) { task=tasks[head]; tasks[head]=ThreadTask{}; head=(head+1)%Capacity; --count; }
    ReleaseSRWLockExclusive(&tasksLock);
    if(task.execute) { try { task.execute(task.context); } catch(...) { Cancel(task); } }
    AcquireSRWLockShared(&tasksLock);
    bool remaining=accepting && count; HWND target=window;
    ReleaseSRWLockShared(&tasksLock);
    if(remaining) PostMessageW(target,WM_NULL,0,0);
}
LRESULT CALLBACK GameThreadQueue::MessageHook(int code,WPARAM parameter,LPARAM message) {
    if(code==HC_ACTION && parameter==PM_REMOVE && !dispatching) {
        AcquireSRWLockShared(&dispatchLock);
        GameThreadQueue* queue=installed.load(std::memory_order_acquire);
        if(queue) {
            dispatching=true;
            if(message && queue->inputHandler) {
                auto& msg=*reinterpret_cast<MSG*>(message);
                // Never let an observer consume another window's messages.
                if(msg.hwnd==queue->window)try {
                    if(queue->inputHandler(msg)){msg.message=WM_NULL;msg.wParam=0;msg.lParam=0;}
                }catch(...){}
            }
            queue->ExecuteOne();dispatching=false;
        }
        ReleaseSRWLockShared(&dispatchLock);
    }
    return CallNextHookEx(nullptr,code,parameter,message);
}
void GameThreadQueue::Stop() {
    // Stop is called by the module's control/worker thread, never by a task.
    // Waiting for dispatch rundown keeps in-flight contexts valid for callers.
    AcquireSRWLockExclusive(&dispatchLock);
    if(installed.load()==this) installed.store(nullptr,std::memory_order_release);
    AcquireSRWLockExclusive(&tasksLock);
    accepting=false; window=nullptr;
    ThreadTask canceled[Capacity]{}; unsigned pending=count;
    for(unsigned i=0;i<pending;++i) canceled[i]=tasks[(head+i)%Capacity];
    head=count=0;
    ReleaseSRWLockExclusive(&tasksLock);
    HHOOK previousHook=hook; hook=nullptr;
    if(previousHook) UnhookWindowsHookEx(previousHook);
    ReleaseSRWLockExclusive(&dispatchLock);
    for(unsigned i=0;i<pending;++i) Cancel(canceled[i]);
}
}
