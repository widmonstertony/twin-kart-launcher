#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "DriftObjectIdentity.h"
#include "CameraControl.h"

// Independent build-specific adapter. Native object offsets are observations
// from the supported client; no client function bodies or assets are included.
namespace {
constexpr wchar_t ExpectedSha[] = L"5041d9f5d45adb4b1c932d8056a89f70db7fa98f93614ac30685cd1adeb9d9c2";
constexpr size_t DriveVtable = 0x51086c8, ViewSlot = 0x548, ViewFunction = 0x1539ce0;
using CameraView = void(__fastcall*)(void*, float, void*);
CameraView originalView = nullptr;
PVOID volatile* viewSlot = nullptr;
volatile LONG enabled = 0;
alignas(8) volatile LONG64 historyRevision = 1;
void InvalidateHistory() { InterlockedIncrement64(&historyRevision); }
volatile LONG calls = 0, applied = 0, rejected = 0;
volatile LONG lastThread = 0;
HANDLE logFile = INVALID_HANDLE_VALUE;
HANDLE toggleEvent = nullptr;
BYTE* image = nullptr;
struct Vec { float x, y, z; };
struct Quat { float x, y, z, w; };
struct View { Vec location; Vec rotation; float fov; };
struct Pose { Vec position; Quat rotation; };
using TwinKartCamera::Settings;
Settings settings{1, -30, 140, -10, 90, 0, 12, 8};
BY_HANDLE_FILE_INFORMATION lastControl{};
bool controlSeen=false;
volatile LONG headAnchored=0, seatFallback=0;
SRWLOCK settingsLock = SRWLOCK_INIT;
wchar_t controlPath[32768]{};
bool meshAudit = false;
bool cockpitHead = false;
void* auditedMesh = nullptr;
void* auditedAsset = nullptr;
int auditedIndex = -1;
TwinKartCamera::ObjectIdentity auditedIdentity{};
unsigned long long headName = 0;
bool headWasHidden = false, headHiddenByUs = false;
void Log(const char* event);
// PC preparation consumer indexes the same object array at RVA 0x1ac0dcf.
TwinKartCamera::ObjectSlot** ObjectChunks() { return *reinterpret_cast<TwinKartCamera::ObjectSlot***>(image + 0x69c28b0); }
int ObjectCount() { return *reinterpret_cast<int*>(image + 0x69c28c4); }
void ClearDriver() {
    auditedMesh = auditedAsset = nullptr; auditedIndex = -1;
    auditedIdentity = TwinKartCamera::ObjectIdentity{};
    headName = 0; headWasHidden = headHiddenByUs = false;
}
void RestoreDriver() {
    bool hadDriver=auditedMesh!=nullptr;
    __try {
        if (headHiddenByUs && TwinKartCamera::Live(auditedIdentity,ObjectChunks(),ObjectCount()) &&
            *reinterpret_cast<void**>(static_cast<BYTE*>(auditedMesh) + 0x4c0) == auditedAsset) {
            using Unhide = void(__fastcall*)(void*, unsigned long long);
            using IsHidden = bool(__fastcall*)(void*, unsigned long long);
            reinterpret_cast<Unhide>(image + 0x3e80390)(auditedMesh,headName);
            Log(reinterpret_cast<IsHidden>(image + 0x3e74d80)(auditedMesh,headName) ? "previous_driver_restore_failed" : "previous_driver_restored");
        } else if (headHiddenByUs) Log("previous_driver_expired_no_write");
    } __except (EXCEPTION_EXECUTE_HANDLER) { Log("previous_driver_unavailable_no_write"); }
    ClearDriver();
    if(hadDriver)InvalidateHistory();
}

bool DriverSignatures() {
    const struct { size_t rva; BYTE prefix[8]; } checks[] = {
        {0x2251540, {0x4c,0x8b,0xdc,0x48,0x81,0xec,0x88,0x00}},
        {0x3e6eee0, {0x48,0x8b,0x81,0xc0,0x04,0x00,0x00,0x48}},
        {0x3e6a200, {0x4c,0x8b,0x89,0xc0,0x04,0x00,0x00,0x4d}},
        {0x3bcd370, {0x48,0x89,0x54,0x24,0x10,0x53,0x48,0x83}},
        {0x248b950, {0x48,0x85,0xc9,0x74,0x2e,0x53,0x48,0x83}},
        {0x3e73710, {0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74}},
        {0x3e80390, {0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83}},
        {0x3e74d80, {0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83}},
        {0x2796c20, {0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b}},
        {0x3e46870, {0x40,0x53,0x48,0x83,0xec,0x60,0x48,0x8b}},
        {0x3e70900, {0x48,0x8b,0xc4,0x4c,0x89,0x40,0x18,0x55}}
    };
    for (const auto& check : checks) if (memcmp(image + check.rva, check.prefix, 8)) return false;
    return true;
}

// All engine calls in this diagnostic run inside GetCameraView on its thread.
// Bone names are discovered on the active driver's component. Only the head
// branch is suppressed, using the engine's visibility API with no physics change.
void UpdateDriver(void* component, bool cameraApplied) {
    if (!meshAudit && !cockpitHead) return;
    __try {
        static bool verified = false;
        if (!verified) {
            if (!DriverSignatures()) { meshAudit = false; cockpitHead = false; Log("driver_signatures_mismatch"); return; }
            verified = true;
        }
        BYTE* pawn = *reinterpret_cast<BYTE**>(static_cast<BYTE*>(component) + 0xf20);
        if (!pawn || (*reinterpret_cast<unsigned*>(pawn + 8) & 0x18000)) { RestoreDriver(); return; }
        using StaticClass = void*(__fastcall*)();
        using FindComponent = BYTE*(__fastcall*)(void*, void*);
        void* type = reinterpret_cast<StaticClass>(image + 0x2251540)();
        auto find = reinterpret_cast<FindComponent>((*reinterpret_cast<BYTE***>(pawn))[0x620 / 8]);
        BYTE* mesh = find(pawn, type);
        if (!mesh || *reinterpret_cast<BYTE**>(mesh) != image + 0x5109e38 ||
            *reinterpret_cast<BYTE**>(mesh + 0xa0) != pawn) { RestoreDriver(); return; }
        void* asset = *reinterpret_cast<void**>(mesh + 0x4c0);
        int identity = *reinterpret_cast<int*>(mesh + 0xc);
        TwinKartCamera::ObjectIdentity currentIdentity{};
        if (!TwinKartCamera::Capture(mesh,ObjectChunks(),ObjectCount(),&currentIdentity)) {
            // Use the client's FWeakObjectPtr assignment, observed at delegate
            // binding sites. Its own allocator initializes a zero serial safely.
            struct Weak { int index, serial; } weak{-1,0};
            using AssignWeak=void(__fastcall*)(Weak*,void*);
            reinterpret_cast<AssignWeak>(image+0x2796c20)(&weak,mesh);
            if (!TwinKartCamera::Capture(mesh,ObjectChunks(),ObjectCount(),&currentIdentity) ||
                currentIdentity.index!=weak.index || currentIdentity.serial!=weak.serial) { RestoreDriver(); return; }
        }
        using IsHidden = bool(__fastcall*)(void*, unsigned long long);
        using Hide = void(__fastcall*)(void*, unsigned long long, int);
        using Unhide = void(__fastcall*)(void*, unsigned long long);
        if (mesh != auditedMesh || asset != auditedAsset || identity != auditedIndex || currentIdentity.serial != auditedIdentity.serial) {
        RestoreDriver();
        using BoneCount = int(__fastcall*)(void*);
        using BoneName = void*(__fastcall*)(void*, unsigned long long*, int);
        struct String { wchar_t* data; int size, capacity; };
        using NameText = String*(__fastcall*)(String*, unsigned long long);
        using Free = void(__fastcall*)(void*);
        int count = reinterpret_cast<BoneCount>(image + 0x3e6eee0)(mesh);
        if (count < 1 || count > 512) return;
        auditedMesh = mesh;
        auditedAsset = asset; auditedIndex = identity; headName = 0; headHiddenByUs = false;
        auditedIdentity = currentIdentity;
        InvalidateHistory();
        for (int index = 0; index < count; ++index) {
            unsigned long long name = 0;
            reinterpret_cast<BoneName>(image + 0x3e6a200)(mesh, &name, index);
            String value{};
            reinterpret_cast<NameText>(image + 0x3bcd370)(&value, name);
            if (value.data && value.size > 1 && value.size <= 96 && value.capacity >= value.size) {
                char bone[128]{};
                bool safe = true;
                for (int n = 0; n < value.size - 1; ++n) {
                    wchar_t c = value.data[n];
                    if (!((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
                        (c >= L'0' && c <= L'9') || c == L'_' || c == L'-' || c == L' ' || c == L'.')) { safe = false; break; }
                    bone[n] = static_cast<char>(c);
                }
                if (safe && (!_stricmp(bone, "-Head") || !_stricmp(bone, "Head"))) headName = name;
                if (safe && meshAudit) {
                    char row[256]; DWORD written;
                    int size = sprintf_s(row, "{\"event\":\"driver_bone\",\"index\":%d,\"name\":\"%s\",\"id\":\"%llx\"}\r\n", index, bone, name);
                    if (size > 0) WriteFile(logFile, row, size, &written, nullptr);
                }
            }
            if (value.data) reinterpret_cast<Free>(image + 0x248b950)(value.data);
        }
        if (headName) headWasHidden = reinterpret_cast<IsHidden>(image + 0x3e74d80)(mesh, headName);
        Log(headName ? "driver_head_identified" : "driver_head_unavailable");
        }
        if (!cockpitHead || !headName) return;
        if (cameraApplied && !headWasHidden && !headHiddenByUs) {
            reinterpret_cast<Hide>(image + 0x3e73710)(mesh, headName, 0);
            headHiddenByUs = reinterpret_cast<IsHidden>(image + 0x3e74d80)(mesh, headName);
            Log(headHiddenByUs ? "driver_head_hidden" : "driver_head_hide_failed");
        } else if (!cameraApplied && headHiddenByUs) {
            reinterpret_cast<Unhide>(image + 0x3e80390)(mesh, headName);
            headHiddenByUs = reinterpret_cast<IsHidden>(image + 0x3e74d80)(mesh, headName);
            Log(headHiddenByUs ? "driver_head_restore_failed" : "driver_head_restored");
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { RestoreDriver(); meshAudit = false; cockpitHead = false; Log("mesh_adapter_failed"); }
}

void ReadSettings() {
    if (!controlPath[0]) return;
    HANDLE file = CreateFileW(controlPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    char bytes[128]{}; DWORD count = 0;
    bool ok = ReadFile(file, bytes, sizeof(bytes)-1, &count, nullptr) != FALSE;
    BY_HANDLE_FILE_INFORMATION info{};
    ok=ok && GetFileInformationByHandle(file,&info)!=FALSE && info.nFileSizeHigh==0 && info.nFileSizeLow<sizeof(bytes);
    CloseHandle(file);
    Settings candidate;
    if (!ok || !TwinKartCamera::ParseSettings(bytes,&candidate)) return;
    bool newFile=!controlSeen || info.nFileIndexHigh!=lastControl.nFileIndexHigh || info.nFileIndexLow!=lastControl.nFileIndexLow ||
        info.dwVolumeSerialNumber!=lastControl.dwVolumeSerialNumber || CompareFileTime(&info.ftLastWriteTime,&lastControl.ftLastWriteTime)!=0;
    AcquireSRWLockExclusive(&settingsLock);
    bool changed = memcmp(&settings, &candidate, sizeof(settings)) != 0;
    settings = candidate;
    ReleaseSRWLockExclusive(&settingsLock);
    if (changed || newFile) {
        LONG old=InterlockedExchange(&enabled, candidate.active);
        if(changed || old!=candidate.active)InvalidateHistory();
    }
    lastControl=info; controlSeen=true;
}

bool finite(float value) { return std::isfinite(value) != 0; }
bool valid(Vec value) { return finite(value.x) && finite(value.y) && finite(value.z); }
float square(float value) { return value * value; }
void Log(const char* event) {
    if (logFile == INVALID_HANDLE_VALUE) return;
    char row[384];
    int size = sprintf_s(row, "{\"event\":\"%s\",\"pid\":%lu,\"thread\":%ld,\"enabled\":%ld,\"calls\":%ld,\"applied\":%ld,\"rejected\":%ld,\"head_anchored\":%ld,\"seat_fallback\":%ld}\r\n",
        event, GetCurrentProcessId(), lastThread, enabled, calls, applied, rejected,headAnchored,seatFallback);
    DWORD written; if (size > 0) WriteFile(logFile, row, static_cast<DWORD>(size), &written, nullptr);
}

bool CheckClient() {
    wchar_t path[32768], allowed[32768];
    DWORD length = GetModuleFileNameW(nullptr, path, ARRAYSIZE(path));
    DWORD allowedLength = GetEnvironmentVariableW(L"TWIN_KART_ALLOWED_GAME", allowed, ARRAYSIZE(allowed));
    if (!length || length >= ARRAYSIZE(path) || !allowedLength || allowedLength >= ARRAYSIZE(allowed) || _wcsicmp(path, allowed)) return false;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    BCRYPT_ALG_HANDLE algorithm = nullptr; BCRYPT_HASH_HANDLE hash = nullptr;
    BYTE digest[32], buffer[65536]; bool ok = false;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0 &&
        BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) >= 0) {
        DWORD count = 0; bool readOk = true;
        for (;;) {
            if (!ReadFile(file, buffer, sizeof(buffer), &count, nullptr)) { readOk = false; break; }
            if (!count) break;
            if (BCryptHashData(hash, buffer, count, 0) < 0) { readOk = false; break; }
        }
        if (readOk && BCryptFinishHash(hash, digest, sizeof(digest), 0) >= 0) {
            wchar_t hex[65]; for (int n = 0; n < 32; ++n) swprintf_s(hex + n * 2, 65 - n * 2, L"%02x", digest[n]);
            ok = wcscmp(hex, ExpectedSha) == 0;
        }
    }
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    CloseHandle(file); return ok;
}

// The live camera obtains its pawn at +0xf20. The original camera calculator
// reads the pawn root at +0x130 and its world transform at +0x1d0/+0x1e0.
// Reads and writes occur inside the original GetCameraView call on its caller's
// thread, so no UObject method is invoked from our installation worker.
bool ReadPose(void* component, Pose* pose, View* before) {
    __try {
        BYTE* camera = static_cast<BYTE*>(component);
        if (*reinterpret_cast<BYTE**>(camera) != image + DriveVtable) return false;
        BYTE* pawn = *reinterpret_cast<BYTE**>(camera + 0xf20);
        if (!pawn || (*reinterpret_cast<unsigned*>(pawn + 8) & 0x18000)) return false;
        BYTE* root = *reinterpret_cast<BYTE**>(pawn + 0x130);
        if (!root) return false;
        pose->rotation = *reinterpret_cast<Quat*>(root + 0x1d0);
        pose->position = *reinterpret_cast<Vec*>(root + 0x1e0);
        Quat q = pose->rotation;
        float norm = square(q.x) + square(q.y) + square(q.z) + square(q.w);
        if (!valid(pose->position) || !finite(norm) || norm < 0.9f || norm > 1.1f ||
            !valid(before->location) || !finite(before->fov) || before->fov < 5 || before->fov > 175) return false;
        Vec delta{ before->location.x - pose->position.x, before->location.y - pose->position.y, before->location.z - pose->position.z };
        return square(delta.x) + square(delta.y) + square(delta.z) < square(3000.0f);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool ApplyView(void* component, void* output) {
    __try {
        View* view = static_cast<View*>(output); Pose pose;
        if (!ReadPose(component, &pose, view)) return false;
        AcquireSRWLockShared(&settingsLock); Settings current = settings; ReleaseSRWLockShared(&settingsLock);
        Quat q = pose.rotation;
        Vec forward{ 1 - 2 * (q.y*q.y + q.z*q.z), 2 * (q.x*q.y + q.w*q.z), 2 * (q.x*q.z - q.w*q.y) };
        Vec up{ 2 * (q.x*q.z + q.w*q.y), 2 * (q.y*q.z - q.w*q.x), 1 - 2 * (q.x*q.x + q.y*q.y) };
        constexpr float Degrees = 57.29577951308232f;
        Vec angle{ std::atan2(forward.z, std::sqrt(forward.x*forward.x + forward.y*forward.y)) * Degrees + current.pitch,
                   std::atan2(forward.y, forward.x) * Degrees, 0 };
        Vec location{ pose.position.x + forward.x * current.forward + up.x * current.height,
                      pose.position.y + forward.y * current.forward + up.y * current.height,
                      pose.position.z + forward.z * current.forward + up.z * current.height };
        bool atHead=false;
        BYTE* pawn=*reinterpret_cast<BYTE**>(static_cast<BYTE*>(component)+0xf20);
        if(current.headAnchor && headName && auditedMesh && TwinKartCamera::Live(auditedIdentity,ObjectChunks(),ObjectCount()) &&
            *reinterpret_cast<void**>(static_cast<BYTE*>(auditedMesh)+0xa0)==pawn &&
            *reinterpret_cast<void**>(static_cast<BYTE*>(auditedMesh)+0x4c0)==auditedAsset) {
            void** table=*reinterpret_cast<void***>(auditedMesh);
            if(table[0x430/8]==image+0x3e46870 && table[0x428/8]==image+0x3e70900) {
                using SocketLocation=Vec*(__fastcall*)(void*,Vec*,unsigned long long);
                Vec head{}; auto result=reinterpret_cast<SocketLocation>(table[0x430/8])(auditedMesh,&head,headName);
                Vec relative{head.x-pose.position.x,head.y-pose.position.y,head.z-pose.position.z};
                if(result==&head && valid(head) && square(relative.x)+square(relative.y)+square(relative.z)<square(500.0f)) {
                    location=Vec{head.x+forward.x*current.eyeForward+up.x*current.eyeHeight,
                        head.y+forward.y*current.eyeForward+up.y*current.eyeHeight,
                        head.z+forward.z*current.eyeForward+up.z*current.eyeHeight}; atHead=true;
                }
            }
        }
        if(atHead) InterlockedIncrement(&headAnchored); else InterlockedIncrement(&seatFallback);
        if (!valid(location) || !valid(angle)) return false;
        static bool previousHeadAnchor=false;
        if(atHead!=previousHeadAnchor)InvalidateHistory();
        previousHeadAnchor=atHead;
        view->location = location; view->rotation = angle; view->fov = static_cast<float>(current.fov); return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

void __fastcall ViewHook(void* component, float delta, void* output) {
    originalView(component, delta, output);
    InterlockedIncrement(&calls); InterlockedExchange(&lastThread, static_cast<LONG>(GetCurrentThreadId()));
    bool modified = false;
    if (enabled) {
        if (ApplyView(component, output)) { InterlockedIncrement(&applied); modified = true; }
        else InterlockedIncrement(&rejected);
    }
    UpdateDriver(component, modified);
    // Publish only a scalar; render threads never inspect native camera/pawn
    // objects. A newly available head anchor also changes the actual viewpoint.
    static bool wasModified=false;
    static void* lastComponent=nullptr;
    if(modified!=wasModified || (modified && component!=lastComponent))InvalidateHistory();
    wasModified=modified;lastComponent=component;
}

DWORD WINAPI Worker(void*) {
    wchar_t logPath[32768], initial[8]{};
    DWORD n = GetEnvironmentVariableW(L"TWIN_KART_CAMERA_LOG", logPath, ARRAYSIZE(logPath));
    if (!n || n >= ARRAYSIZE(logPath) || !CheckClient()) return 1;
    logFile = CreateFileW(logPath, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (logFile == INVALID_HANDLE_VALUE) return 2;
    image = reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr));
    BYTE signature[] = { 0x48,0x89,0x5c,0x24,0x20,0x56,0x48,0x83,0xec,0x60 };
    bool ready = false;
    for (int attempt = 0; attempt < 1200; ++attempt) {
        if (!memcmp(image + ViewFunction, signature, sizeof(signature)) &&
            *reinterpret_cast<void**>(image + DriveVtable + ViewSlot) == image + ViewFunction) { ready = true; break; }
        Sleep(50);
    }
    if (!ready) { Log("unsupported_runtime_signature"); CloseHandle(logFile); return 3; }
    originalView = reinterpret_cast<CameraView>(image + ViewFunction);
    viewSlot = reinterpret_cast<PVOID volatile*>(image + DriveVtable + ViewSlot);
    GetEnvironmentVariableW(L"TWIN_KART_FIRST_PERSON", initial, ARRAYSIZE(initial));
    enabled = initial[0] == L'1';
    GetEnvironmentVariableW(L"TWIN_KART_CAMERA_CONTROL", controlPath, ARRAYSIZE(controlPath));
    wchar_t auditFlag[8]{};
    GetEnvironmentVariableW(L"TWIN_KART_CAMERA_MESH_AUDIT", auditFlag, ARRAYSIZE(auditFlag));
    meshAudit = auditFlag[0] == L'1';
    GetEnvironmentVariableW(L"TWIN_KART_COCKPIT_HEAD", auditFlag, ARRAYSIZE(auditFlag));
    cockpitHead = auditFlag[0] == L'1';
    ReadSettings();
    DWORD previous;
    if (!VirtualProtect(const_cast<PVOID*>(viewSlot), sizeof(void*), PAGE_READWRITE, &previous)) { Log("slot_protection_failed"); return 4; }
    PVOID old = InterlockedCompareExchangePointer(viewSlot, reinterpret_cast<void*>(ViewHook), reinterpret_cast<void*>(originalView));
    DWORD ignored; VirtualProtect(const_cast<PVOID*>(viewSlot), sizeof(void*), previous, &ignored);
    if (old != reinterpret_cast<void*>(originalView)) { Log("slot_changed_before_install"); return 5; }
    Log("camera_hook_installed");
    wchar_t eventName[96];
    swprintf_s(eventName, L"Local\\TwinKartCameraToggle-%lu", GetCurrentProcessId());
    toggleEvent = CreateEventW(nullptr, FALSE, FALSE, eventName);
    bool wasDown = false;
    for (unsigned tick = 0;; ++tick) {
        if (tick % 20 == 0) ReadSettings();
        SHORT key = GetAsyncKeyState(VK_F6);
        bool down = (key & 0x8000) != 0;
        DWORD foregroundPid = 0;
        GetWindowThreadProcessId(GetForegroundWindow(), &foregroundPid);
        // The transition bit also catches taps shorter than the sampling period.
        bool requested = toggleEvent && WaitForSingleObject(toggleEvent, 0) == WAIT_OBJECT_0;
        if (requested || (foregroundPid == GetCurrentProcessId() && !wasDown && (down || (key & 1)))) {
            InterlockedExchange(&enabled, !enabled); InvalidateHistory(); Log("camera_toggled");
        }
        wasDown = down;
        if (tick % 200 == 0) Log("camera_sample");
        Sleep(10);
    }
}
}

extern "C" __declspec(dllexport) unsigned long long WINAPI TwinKartCameraHistoryRevision() {
    return static_cast<unsigned long long>(InterlockedCompareExchange64(&historyRevision,0,0));
}

BOOL WINAPI DllMain(HINSTANCE self, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(self);
        HANDLE worker = CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
        if (worker) CloseHandle(worker);
    }
    return TRUE;
}
