#pragma once
#include "RaceControl.h"
#include "NativeRacePreparation.h"
#include "NativeReturnRoute.h"
#include "NativeObjectiveArena.h"

namespace TwinKartObjective {
// The retained original development world has a separately gated authored
// repair. Diagnostic capture and normal lobby entry have distinct commands;
// neither is an ordinary circuit alias or proof of full gameplay acceptance.
// No caller can supply another world, class, options or console command.
inline constexpr const wchar_t* HideWorldPackage=L"/Game/Dev/HideNSeek/HideNSeekTestLevel";
inline constexpr const wchar_t* HideWorldObject=L"/Game/Dev/HideNSeek/HideNSeekTestLevel.HideNSeekTestLevel";
inline bool ParseHideCapture(const char* text,TwinKartRace::Request* output) {
    if(!text || !output || strnlen_s(text,257)>256 || strncmp(text,"TKHIDE_CAPTURE1 ",15))return false;
    TwinKartRace::Request request;char extra=0;
    if(sscanf_s(text,"TKHIDE_CAPTURE1 %32s %c",request.identity,33u,&extra,1u)!=1)return false;
    strcpy_s(request.track,"HideNSeekTestLevel");
    if(!TwinKartRace::Valid(request))return false;
    request.mode=109;request.ai=0;request.laps=0;*output=request;return true;
}
inline bool ParseHideBotCapture(const char* text,TwinKartRace::Request* output) {
    constexpr char prefix[]="TKHIDE_AI_CAPTURE1 ";
    if(!text || !output || strnlen_s(text,257)>256 || strncmp(text,prefix,sizeof(prefix)-1))return false;
    TwinKartRace::Request request;char extra=0;
    if(sscanf_s(text,"TKHIDE_AI_CAPTURE1 %32s %c",request.identity,33u,&extra,1u)!=1)return false;
    strcpy_s(request.track,"HideNSeekTestLevel");
    if(!TwinKartRace::Valid(request))return false;
    request.mode=109;request.ai=7;request.laps=0;*output=request;return true;
}
// Normal original-lobby entry uses its own exact command and gate. It cannot
// supply an engine path, arbitrary world, player count or command line.
inline bool ParseHideRepairedStart(const char* text,TwinKartRace::Request* output) {
    constexpr char prefix[]="TKHIDE_REPAIRED1 ";
    if(!text || !output || strnlen_s(text,257)>256 || strncmp(text,prefix,sizeof(prefix)-1))return false;
    TwinKartRace::Request request;char extra=0;
    if(sscanf_s(text,"TKHIDE_REPAIRED1 %32s %c",request.identity,33u,&extra,1u)!=1)return false;
    strcpy_s(request.track,"HideNSeekTestLevel");if(!TwinKartRace::Valid(request))return false;
    request.mode=109;request.ai=7;request.laps=0;*output=request;return true;
}
inline bool ParseHideArenaCapture(const char* text,TwinKartRace::Request* output) {
    constexpr char prefix[]="TKHIDE_ARENA_CAPTURE1 ";
    if(!text || !output || strnlen_s(text,257)>256 || strncmp(text,prefix,sizeof(prefix)-1))return false;
    TwinKartRace::Request request;char extra=0;
    if(sscanf_s(text,"TKHIDE_ARENA_CAPTURE1 %32s %c",request.identity,33u,&extra,1u)!=1)return false;
    strcpy_s(request.track,"Flag_F01");if(!TwinKartRace::Valid(request))return false;
    request.mode=109;request.ai=7;request.laps=0;*output=request;return true;
}
inline bool HideCaptureOptions(const TwinKartRace::Request& request,wchar_t* output,size_t capacity) {
    if(request.mode!=109 || (request.ai!=0 && request.ai!=7) || request.laps || request.profile[0] || request.custom[0] ||
       (strcmp(request.track,"HideNSeekTestLevel") && (strcmp(request.track,"Flag_F01") || request.ai!=7)) || !output)return false;
    auto syntax=request;syntax.mode=1;syntax.ai=7;syntax.laps=1;
    if(!TwinKartRace::Valid(syntax))return false;
    const wchar_t* options=request.ai==7?L"game=/Script/KartDrift.HideNSeekGameMode?players=8?ais=0":
        L"game=/Script/KartDrift.HideNSeekGameMode?players=1?ais=0";
    return capacity>wcslen(options) && wcscpy_s(output,capacity,options)==0;
}
using OpenLevel=void(__fastcall*)(void*,unsigned long long,bool,TwinKartRace::PreparationString*);
using ReleaseString=void(__fastcall*)(void*);
// Original FUniqueNetIdRepl script-struct metadata gives size 0x28/alignment 8.
// Its empty local value contains no online identity or remote authentication.
struct alignas(8) EmptyLocalPlayerId {unsigned char bytes[0x28]{};};
static_assert(sizeof(EmptyLocalPlayerId)==0x28,"Original empty local ID ABI");
using InitializeLocalPlayer=TwinKartRace::ReturnString*(__fastcall*)(void*,TwinKartRace::ReturnString*,void*,
    const EmptyLocalPlayerId*,const TwinKartRace::ReturnString*,const TwinKartRace::ReturnString*);
using OriginalControllerInitialization=void(__fastcall*)(void*,void*);
inline bool InitializeHideController(void* mode,void* controller,int slot,InitializeLocalPlayer initialize,
    OriginalControllerInitialization finish,ReleaseString release) {
    if(!mode || !controller || slot<1 || slot>7 || !initialize || !finish || !release)return false;
    wchar_t text[64]{};if(swprintf_s(text,L"?Name=TwinAI%d",slot)<=0)return false;
    TwinKartRace::ReturnString options{text,static_cast<int>(wcslen(text)+1),static_cast<int>(wcslen(text)+1)},portal{},error{};
    EmptyLocalPlayerId id{};auto result=initialize(mode,&error,controller,&id,&options,&portal);
    bool valid=result==&error && TwinKartRace::ReturnStringShape(error) && error.count<=1;
    if(error.data)release(error.data);
    if(!valid)return false;
    finish(mode,controller);return true;
}
inline bool SubmitHide(void* world,unsigned long long name,const TwinKartRace::Request& request,
    TwinKartRace::CopyPreparationString copy,OpenLevel open,ReleaseString release) {
    wchar_t text[256]{};
    if(!world || !name || !copy || !open || !release || !HideCaptureOptions(request,text,ARRAYSIZE(text)))return false;
    TwinKartRace::PreparationString source{text,static_cast<int>(wcslen(text)+1),static_cast<int>(wcslen(text)+1)},owned{};
    auto result=copy(&owned,&source);
    if(result!=&owned || !owned.data || owned.data==source.data || owned.count!=source.count || owned.capacity<owned.count) {
        if(owned.data && owned.data!=source.data)release(owned.data);
        return false;
    }
    // OpenLevel's FString Options is passed BY VALUE, via an x64 indirect
    // argument. The original callee frees the transferred buffer at 3f508f1.
    // Never pass a borrowed stack view or destroy it again after the call.
    open(world,name,true,&owned);return true;
}
}
