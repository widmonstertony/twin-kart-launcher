#include "../native/RaceControl.h"
#include "../native/NativeTrackModes.h"
#include "../native/NativeReturnRoute.h"
#include "../native/NativeRacePreparation.h"
#include "../native/NativeModeBinding.h"
#include "../native/NativeEventMenu.h"
#include "../native/NativeCubeWorld.h"
#include "../native/NativeFactoryPopulation.h"
#include "../native/NativeObjectiveCapture.h"
#include "../native/NativeHideRules.h"
#include <stdexcept>
#include <fstream>
#include <iterator>
void NeedAt(bool value,int line) {
    if(!value){fprintf(stderr,"Race request contract failed at line %d\n",line);throw std::runtime_error("Race request contract failed");}
}
#define Need(value) NeedAt((value),__LINE__)
int hideCopies=0,hideOpens=0,hideReleases=0;
void* __fastcall CopyHide(TwinKartRace::PreparationString* out,const TwinKartRace::PreparationString* in) {
    ++hideCopies;out->count=out->capacity=in->count;
    out->data=static_cast<wchar_t*>(malloc(static_cast<size_t>(in->count)*sizeof(wchar_t)));
    memcpy(out->data,in->data,static_cast<size_t>(in->count)*sizeof(wchar_t));return out;
}
void __fastcall ReleaseHide(void* memory){++hideReleases;free(memory);}
void __fastcall OpenHide(void* world,unsigned long long name,bool absolute,TwinKartRace::PreparationString* owned) {
    Need(world && name==42 && absolute && owned && owned->data && wcsstr(owned->data,L"HideNSeekGameMode"));
    ++hideOpens;ReleaseHide(owned->data);
}
void* __fastcall BorrowHide(TwinKartRace::PreparationString* out,const TwinKartRace::PreparationString* in){*out=*in;return out;}
int hideLogins=0,hidePosts=0,hideLoginFrees=0;bool hideLoginFailure=false;
TwinKartRace::ReturnString* __fastcall InitHidePlayer(void* mode,TwinKartRace::ReturnString* error,void* controller,
    const TwinKartObjective::EmptyLocalPlayerId* id,const TwinKartRace::ReturnString* options,const TwinKartRace::ReturnString* portal) {
    Need(mode && controller && id && options && !wcscmp(options->data,L"?Name=TwinAI1") && portal && !portal->count);
    for(auto byte:id->bytes)Need(byte==0);
    ++hideLogins;const wchar_t* text=hideLoginFailure?L"Rejected":L"";error->count=error->capacity=static_cast<int>(wcslen(text)+1);
    error->data=static_cast<wchar_t*>(malloc(error->count*sizeof(wchar_t)));memcpy(error->data,text,error->count*sizeof(wchar_t));return error;
}
void __fastcall PostHidePlayer(void* mode,void* controller){Need(mode && controller);++hidePosts;}
void __fastcall FreeHideLogin(void* text){++hideLoginFrees;free(text);}
void ObjectiveCaptureChecks() {
    using namespace TwinKartObjective;
    HideRoleEntry entries[]={{1,{1,0},-1,0},{8,{6,2},-1,0},{999,{-1,-1},-1,0}};
    HideRoleMap map{};map.data=entries;map.count=3;map.capacity=4;map.inlineBits[0]=3;
    map.bitCount=3;map.bitCapacity=128;map.firstFree=2;map.freeCount=1;HideRoles roles{99,99};
    Need(ReadHideRoles(map,1,roles) && roles.hiders==1 && !roles.seekers && !CompetitiveHideRoles(roles,1));
    Need(ReadHideRoles(map,8,roles) && roles.hiders==6 && roles.seekers==2 && CompetitiveHideRoles(roles,8));
    Need(!ReadHideRoles(map,7,roles) && roles.hiders==6 && roles.seekers==2);
    entries[1].players=1;Need(!ReadHideRoles(map,1,roles));entries[1].players=8;
    entries[1].roles.seekers=3;Need(!ReadHideRoles(map,8,roles));entries[1].roles.seekers=2;
    map.freeCount=0;Need(!ReadHideRoles(map,8,roles));map.freeCount=1;
    map.secondaryBits=map.inlineBits;Need(!ReadHideRoles(map,8,roles));map.secondaryBits=nullptr;
    map.bitCount=2;Need(!ReadHideRoles(map,8,roles));map.bitCount=3;
    Need(!ReadHideRoles(map,17,roles));
    int setupObject=0,grantObject=0,sentinel=0;
    HidePhaseEntry phaseEntries[]{{11,&setupObject,-1,0},{999,nullptr,-1,0},{22,&grantObject,-1,0}};
    HidePhaseMap phaseMap{};phaseMap.data=phaseEntries;phaseMap.count=3;phaseMap.capacity=3;
    phaseMap.inlineBits[0]=5;phaseMap.bitCount=3;phaseMap.bitCapacity=128;phaseMap.freeCount=1;
    void* setupPhase=&sentinel;void* grantPhase=&sentinel;
    Need(HidePhaseInitializationReady(phaseMap,11,22,setupPhase,grantPhase) && setupPhase==&setupObject && grantPhase==&grantObject);
    setupPhase=grantPhase=&sentinel;phaseEntries[2].phase=nullptr;
    Need(!HidePhaseInitializationReady(phaseMap,11,22,setupPhase,grantPhase) && setupPhase==&sentinel && grantPhase==&sentinel);
    phaseEntries[2].phase=&grantObject;phaseMap.inlineBits[0]=1;
    Need(!HidePhaseInitializationReady(phaseMap,11,22,setupPhase,grantPhase));phaseMap.inlineBits[0]=5;
    phaseEntries[2].name=11;Need(!HidePhaseInitializationReady(phaseMap,11,22,setupPhase,grantPhase));phaseEntries[2].name=22;
    phaseMap.count=0;Need(!HidePhaseInitializationReady(phaseMap,11,22,setupPhase,grantPhase));
    Need(HidePreparationSeconds(45,45,0)==45 && HidePreparationSeconds(45,45,44.5f)==1);
    Need(HidePreparationSeconds(45,45,45)==0 && HidePreparationSeconds(45,90,0)==0 &&
        HidePreparationSeconds(45,45,-1)==0 && HidePreparationSeconds(0,0,0)==0);
    TwinKartRace::ArenaPoint eye{600,0,150};
    Need(ClipHideCamera({0,0,150},eye,0.5f) && std::fabs(eye.x-276.f)<0.01f && eye.z==150.f);
    Need(!ClipHideCamera({0,0,150},eye,0.f) && std::fabs(eye.x-276.f)<0.01f);
    eye={600,0,150};
    Need(ClipHideCamera({0,0,150},eye,0.02f) && eye.x==0.f);
    const TwinKartRace::ArenaPoint fishCenter{1200,500,180},fishExtent{170,120,95};
    TwinKartRace::ArenaPoint lowEye{},highEye{},look{};
    Need(FrameHideDisguise(fishCenter,fishExtent,{900,500,250},0,90,0,lowEye,look) &&
        lowEye.x<fishCenter.x-250.f && std::fabs(look.y)<0.01f && look.x<0.f);
    Need(FrameHideDisguise(fishCenter,fishExtent,{900,500,250},0,90,2,highEye,look) &&
        highEye.z>lowEye.z+150.f && highEye.x>lowEye.x && look.x<-60.f);
    TwinKartRace::ArenaPoint alternate{};
    Need(FrameHideDisguise(fishCenter,fishExtent,{900,500,250},0,90,3,alternate,look) && alternate.x>fishCenter.x);
    Need(FrameHideDisguise(fishCenter,fishExtent,{900,500,250},0,90,4,alternate,look) && alternate.y<fishCenter.y);
    Need(FrameHideDisguise(fishCenter,fishExtent,{900,500,250},0,90,5,alternate,look) && alternate.y>fishCenter.y);
    Need(FrameHideDisguise(fishCenter,fishExtent,{-6000,500,250},0,90,6,alternate,look) &&
        alternate.z>highEye.z && look.x<-80.f);
    Need(!FrameHideDisguise(fishCenter,fishExtent,{900,500,250},0,90,HideDisguiseViewCount,alternate,look));
    Need(!FrameHideDisguise(fishCenter,{170,120,0},{900,500,250},0,90,0,lowEye,look));
    Need(FrameHideDisguise(fishCenter,{0.5f,120,90},{900,500,250},0,90,0,lowEye,look));
    Need(HideDisguiseKind(L"BP_PalmTreeProp_C")==1 && HideDisguiseKind(L"BP_FishStockProp_C")==2 &&
        HideDisguiseKind(L"BP_SignProp_C")==3 && HideDisguiseKind(L"BP_CornProp_C")==4 &&
        HideDisguiseKind(L"BP_CornProp_C_1")==0 && HideDisguiseKind(nullptr)==0);
    TwinKartRace::ArenaPoint palmCenter{0,0,2000},palmExtent{700,600,1600};
    Need(FitHideDisguiseTarget(1,{0,0,70},{20,10,25},palmCenter,palmExtent) && palmCenter.x==20 &&
        palmCenter.z==305 && palmExtent.x==260 && palmExtent.z==400);
    auto stallCenter=fishCenter,stallExtent=fishExtent;
    Need(FitHideDisguiseTarget(2,{1200,500,80},{1200,500,50},stallCenter,stallExtent) &&
        stallCenter.z==fishCenter.z && stallExtent.z==fishExtent.z);
    Need(!FitHideDisguiseTarget(1,{0,0,70},{900,10,25},palmCenter,palmExtent));
    HideSearchClock clock;int clockRules=0,otherRules=0;
    Need(!clock.Begin(nullptr,1,180) && !clock.Begin(&clockRules,0,180));
    Need(!clock.Begin(&clockRules,1,0) && !clock.Begin(&clockRules,1,3601));
    Need(!clock.Begin(&clockRules,1,NAN) && !clock.Begin(&clockRules,1,INFINITY));
    Need(clock.Begin(&clockRules,1,180));
    Need(!clock.Advance(&clockRules,1,0) && !clock.Advance(&clockRules,1,-1));
    Need(!clock.Advance(&clockRules,1,6) && !clock.Advance(&clockRules,1,NAN));
    for(int tick=0;tick<179;++tick)Need(!clock.Advance(&clockRules,1,1));
    Need(!clock.Begin(&clockRules,1,180));
    Need(clock.Advance(&clockRules,1,1) && !clock.Advance(&clockRules,1,1));
    Need(clock.Begin(&clockRules,2,1) && !clock.Advance(&otherRules,2,1));
    Need(!clock.Advance(&clockRules,2,1));
    Need(clock.Begin(&clockRules,3,1) && !clock.Advance(&clockRules,4,1));
    Need(!clock.Advance(&clockRules,3,1));
    Need(clock.Begin(&clockRules,4,1));clock.Stop();Need(!clock.Advance(&clockRules,4,1));
    HideSeat oldSeats[8]{},newSeats[8]{};
    for(unsigned i=0;i<8;++i){oldSeats[i]={i+1,i+100};newSeats[7-i]={i+1,i+200};}
    for(int i=0;i<8;++i)Need(RetiredHidePawn(oldSeats,newSeats,8,i));
    newSeats[0].pawn=oldSeats[1].pawn;Need(!RetiredHidePawn(oldSeats,newSeats,8,1));newSeats[0].pawn=207;
    newSeats[1].pawn=0;Need(!RetiredHidePawn(oldSeats,newSeats,8,0));newSeats[1].pawn=206;
    newSeats[1].pawn=newSeats[0].pawn;Need(!RetiredHidePawn(oldSeats,newSeats,8,0));newSeats[1].pawn=206;
    newSeats[1].controller=999;Need(!RetiredHidePawn(oldSeats,newSeats,8,0));newSeats[1].controller=7;
    oldSeats[0].pawn=0;Need(!RetiredHidePawn(oldSeats,newSeats,8,0));
    Need(!RetiredHidePawn(oldSeats,newSeats,7,1) && !RetiredHidePawn(oldSeats,newSeats,8,8));
    Need(UncontrolledHidePawn(100,newSeats,8));newSeats[0].pawn=100;
    Need(!UncontrolledHidePawn(100,newSeats,8) && !UncontrolledHidePawn(0,newSeats,8));
    Need(!UncontrolledHidePawn(100,nullptr,8) && !UncontrolledHidePawn(100,newSeats,7));
    Need(HideDisguiseChoice(123,1,4)==0 && HideDisguiseChoice(123,4,4)==3);
    Need(HideDisguiseChoice(0xffffffff,32,32)==31);
    Need(HideDisguiseChoice(1,0,4)==-1 && HideDisguiseChoice(1,33,64)==-1);
    Need(HideDisguiseChoice(1,4,3)==-1 && HideDisguiseChoice(1,4,257)==-1);
    // Exercise the same native controller field consumed by the bridge. An
    // inverted adapter previously let bots wait and stole the human's choice.
    struct ControllerFixture {unsigned char padding[0x298]{};void* player=nullptr;};
    static_assert(offsetof(ControllerFixture,player)==0x298,"Pinned UPlayer field");
    int localPlayer=0;ControllerFixture humanController{},botController{};humanController.player=&localPlayer;
    Need(HideControllerHasPlayer(&humanController) && !HideControllerHasPlayer(&botController));
    Need(!HideControllerHasPlayer(nullptr));
    Need(HideInitialDisguiseChoice(123,4,4,&botController,false,false,false)==3);
    Need(HideInitialDisguiseChoice(123,4,4,&humanController,true,false,false)==0);
    Need(HideInitialDisguiseChoice(123,4,4,&humanController,false,false,false)==-1);
    Need(HideInitialDisguiseChoice(123,4,4,nullptr,true,false,false)==-1);
    for(const void* controller:{static_cast<const void*>(&humanController),static_cast<const void*>(&botController)}) {
        for(bool searching:{false,true}) {
            Need(HideInitialDisguiseChoice(123,4,4,controller,searching,true,false)==-1);
            Need(HideInitialDisguiseChoice(123,4,4,controller,searching,false,true)==-1);
            Need(HideInitialDisguiseChoice(123,4,4,controller,searching,true,true)==-1);
        }
    }
    Need(HideInitialDisguiseChoice(123,4,3,&humanController,true,false,false)==-1);
    humanController.player=nullptr;botController.player=&localPlayer;
    Need(HideInitialDisguiseChoice(123,4,4,&humanController,false,false,false)==3);
    Need(HideInitialDisguiseChoice(123,4,4,&botController,false,false,false)==-1);
    HideObservation observed[8]{},previousObserved[8]{};
    Need(HideContactChannels(2,2) && HideContactChannels(2,6) && HideContactChannels(2,18));
    Need(!HideContactChannels(0,6) && !HideContactChannels(1,6) && !HideContactChannels(2,0) &&
        !HideContactChannels(2,17) && !HideContactChannels(2,32));
    observed[0]={{0,0,0},false,true};observed[1]={{1000,0,0},true,true};
    previousObserved[0]=observed[0];previousObserved[1]=observed[1];
    Need(HideMovingTarget(observed,previousObserved,8,0)==-1);
    previousObserved[1].position.x=980;Need(HideMovingTarget(observed,previousObserved,8,0)==1);
    previousObserved[1]=observed[1];observed[1].position.x=200;Need(HideMovingTarget(observed,previousObserved,8,0)==1);
    observed[1].position.x=4000;Need(HideMovingTarget(observed,previousObserved,8,0)==-1);
    observed[1].valid=false;Need(HideMovingTarget(observed,previousObserved,8,0)==-1);
    Need(HideMovingTarget(nullptr,previousObserved,8,0)==-1 && HideMovingTarget(observed,previousObserved,7,0)==-1);
    Need(HideMovingTarget(observed,previousObserved,8,8)==-1 && HideMovingTarget(observed,previousObserved,8,1)==-1);
    Need(HideSafety({1000,0,0},observed,8)==1000 && HideSafety({2000,0,0},observed,8)==2000);
    Need(HideSafety({},nullptr,8)==-1 && HideSafety({},observed,7)==-1);
    HidePatrolProgress patrol;
    TwinKartRace::ArenaPoint farGoal{7000,0,0};
    Need(!patrol.Renew({},farGoal,1000));
    for(unsigned seconds=1;seconds<=12;++seconds)
        Need(!patrol.Renew({seconds*500.f,0,0},farGoal,1000+seconds*1000));
    Need(patrol.Renew({6900,0,0},farGoal,14000));
    patrol.Reset();Need(!patrol.Renew({},farGoal,1000));
    Need(!patrol.Renew({},farGoal,6999) && patrol.Renew({},farGoal,7000));
    patrol.Reset();Need(!patrol.Renew({},farGoal,1000));
    // A genuine obstacle detour may initially increase straight-line distance.
    for(unsigned seconds=1;seconds<30;++seconds)
        Need(!patrol.Renew({-static_cast<float>(seconds)*150.f,0,0},farGoal,1000+seconds*1000));
    Need(patrol.Renew({-4500,0,0},farGoal,31000));
    Need(!patrol.Renew({-4500,0,0},{0,6000,0},31001));
    Need(!patrol.Renew({-4500,0,0},{0,6000,0},100));
    patrol.Reset();Need(!patrol.Renew({},farGoal,1000));
    for(unsigned seconds=1;seconds<=20;++seconds)
        Need(!patrol.Renew({seconds*50.f,0,0},farGoal,1000+seconds*1000));
    Need(patrol.Renew({NAN,0,0},farGoal,22000));
    int base=0,local=0,custom=0;
    Need(HideSessionClass(true,false,&base,&base,&local)==&local);
    Need(HideSessionClass(false,false,&base,&base,&local)==&base);
    Need(HideSessionClass(true,true,&base,&base,&local)==&base);
    Need(HideSessionClass(true,false,&custom,&base,&local)==&custom);
    Need(!HideSessionClass(true,false,nullptr,&base,&local));
    Need(HideSessionClass(true,false,&base,&base,nullptr)==&base);
    Need(InitializeHideController(&base,&local,1,InitHidePlayer,PostHidePlayer,FreeHideLogin));
    Need(hideLogins==1 && hidePosts==1 && hideLoginFrees==1);
    hideLoginFailure=true;Need(!InitializeHideController(&base,&local,1,InitHidePlayer,PostHidePlayer,FreeHideLogin));
    Need(hideLogins==2 && hidePosts==1 && hideLoginFrees==2);
    Need(!InitializeHideController(&base,&local,0,InitHidePlayer,PostHidePlayer,FreeHideLogin));
    Need(!InitializeHideController(&base,&local,8,InitHidePlayer,PostHidePlayer,FreeHideLogin));
    Need(hideLogins==2 && hidePosts==1 && hideLoginFrees==2);
    TwinKartRace::Request request;wchar_t options[256]{};
    Need(TwinKartObjective::ParseHideCapture("TKHIDE_CAPTURE1 0123456789abcdef0123456789abcdef\n",&request));
    Need(request.mode==109 && !request.ai && !request.laps && !strcmp(request.track,"HideNSeekTestLevel"));
    Need(TwinKartObjective::HideCaptureOptions(request,options,ARRAYSIZE(options)) && wcsstr(options,L"HideNSeekGameMode?players=1?ais=0"));
    Need(TwinKartObjective::SubmitHide(&request,42,request,CopyHide,OpenHide,ReleaseHide));
    Need(hideCopies==1 && hideOpens==1 && hideReleases==1);
    Need(!TwinKartObjective::SubmitHide(&request,42,request,BorrowHide,OpenHide,ReleaseHide));
    Need(!TwinKartObjective::SubmitHide(nullptr,42,request,CopyHide,OpenHide,ReleaseHide));
    Need(!TwinKartObjective::SubmitHide(&request,0,request,CopyHide,OpenHide,ReleaseHide));
    Need(hideCopies==1 && hideOpens==1 && hideReleases==1);
    Need(!TwinKartRace::Valid(request) && !TwinKartRace::Options(request,options,ARRAYSIZE(options)));
    request.ai=6;Need(!TwinKartObjective::HideCaptureOptions(request,options,ARRAYSIZE(options)));request.ai=0;
    strcpy_s(request.track,"OtherWorld");Need(!TwinKartObjective::HideCaptureOptions(request,options,ARRAYSIZE(options)));
    for(auto invalid:{"TKHIDE_CAPTURE1 bad","TKHIDE_CAPTURE1 0123456789abcdef0123456789abcdef OtherWorld",
        "TKHIDE_CAPTURE1 0123456789abcdef0123456789abcdef ?game=Other","TKHIDE_CAPTURE1 0123456789abcdef0123456789abcdef trailing"})
        Need(!TwinKartObjective::ParseHideCapture(invalid,&request));
    Need(ParseHideBotCapture("TKHIDE_AI_CAPTURE1 0123456789abcdef0123456789abcdef\n",&request));
    Need(request.mode==109 && request.ai==7 && !request.laps && HideCaptureOptions(request,options,ARRAYSIZE(options)) &&
        wcsstr(options,L"?players=8?ais=0") && !TwinKartRace::Valid(request));
    Need(!ParseHideBotCapture("TKHIDE_AI_CAPTURE1 0123456789abcdef0123456789abcdef 39",&request));
    Need(!ParseHideBotCapture("TKHIDE_AI_CAPTURE1 bad",&request));
    Need(ParseHideArenaCapture("TKHIDE_ARENA_CAPTURE1 0123456789abcdef0123456789abcdef\n",&request));
    Need(request.mode==109 && request.ai==7 && !strcmp(request.track,"Flag_F01") &&
        HideCaptureOptions(request,options,ARRAYSIZE(options)) && !TwinKartRace::Valid(request));
    Need(!ParseHideArenaCapture("TKHIDE_ARENA_CAPTURE1 0123456789abcdef0123456789abcdef Flag_F02",&request));
    Need(!ParseHideArenaCapture("TKHIDE_ARENA_CAPTURE1 bad",&request));
    request.ai=0;Need(!HideCaptureOptions(request,options,ARRAYSIZE(options)));
    int flagClass=0,hideClass=0,otherClass=0;
    Need(ObjectiveArenaModeClass(true,true,true,L"Flag_F01",&flagClass,&flagClass,&hideClass)==&hideClass);
    Need(ObjectiveArenaModeClass(true,true,true,HideArenaPackage,&flagClass,&flagClass,&hideClass)==&hideClass);
    Need(ObjectiveArenaModeClass(false,true,true,L"Flag_F01",&flagClass,&flagClass,&hideClass)==&flagClass);
    Need(ObjectiveArenaModeClass(true,false,true,L"Flag_F01",&flagClass,&flagClass,&hideClass)==&flagClass);
    Need(ObjectiveArenaModeClass(true,true,false,L"Flag_F01",&flagClass,&flagClass,&hideClass)==&flagClass);
    Need(ObjectiveArenaModeClass(true,true,true,L"Flag_F02",&flagClass,&flagClass,&hideClass)==&flagClass);
    Need(ObjectiveArenaModeClass(true,true,true,L"Flag_F01",&otherClass,&flagClass,&hideClass)==&otherClass);
    Need(ObjectiveArenaModeClass(true,true,true,L"Flag_F01",&flagClass,&flagClass,nullptr)==&flagClass);
    TwinKartRace::ArenaPoint anchors[]{{-5000,0,50},{5000,0,50}};
    auto ground=+[](const TwinKartRace::ArenaPoint& sample,TwinKartRace::ArenaPoint& output,void* blocked)->bool {
        if(blocked && sample.x>0)return false;output={sample.x,sample.y,25};return true;
    };
    ObjectiveArenaPlan plan;
    Need(BuildObjectiveArenaPlan(anchors,2,ground,nullptr,plan));
    for(int a=0;a<16;++a) {
        Need(plan.starts[a].translation[2]==185 && plan.starts[a].scale[0]==1);
        for(int b=0;b<a;++b)Need(std::hypot(plan.starts[a].translation[0]-plan.starts[b].translation[0],
            plan.starts[a].translation[1]-plan.starts[b].translation[1])>=300);
    }
    ObjectiveArenaPlan before=plan;
    Need(!BuildObjectiveArenaPlan(anchors,2,ground,&plan,plan) && !memcmp(&before,&plan,sizeof(plan)));
    TwinKartRace::ArenaPoint distant[]{{0,-58000,50},{0,0,50}};
    Need(BuildObjectiveArenaPlan(distant,2,ground,nullptr,plan));
    for(const auto& start:plan.starts) {
        Need(start.translation[1]>=-32000 && start.translation[1]<=-26000);
        for(const auto& other:plan.starts)Need(std::hypot(start.translation[0]-other.translation[0],
            start.translation[1]-other.translation[1])<7000);
    }
    auto gap=+[](const TwinKartRace::ArenaPoint& sample,TwinKartRace::ArenaPoint& output,void*)->bool {
        output={sample.x,sample.y,std::fabs(sample.y+29000)<100?1000.f:25.f};return true;
    };
    before=plan;
    Need(!BuildObjectiveArenaPlan(distant,2,gap,nullptr,plan) && !memcmp(&before,&plan,sizeof(plan)));
    Need(!BuildObjectiveArenaPlan(anchors,1,ground,nullptr,plan));
    anchors[1].x=-4500;Need(!BuildObjectiveArenaPlan(anchors,2,ground,nullptr,plan));
    auto begin=+[](void* world,void* type,const ObjectiveTransform* transform,unsigned char collision,void* owner)->void* {
        if(world!=type || !transform || collision!=1 || owner)return nullptr;
        ++*static_cast<int*>(world);return world;
    };
    auto finish=+[](void* actor,const ObjectiveTransform* transform)->void* {
        if(!transform)return nullptr;++*static_cast<int*>(actor);return actor;
    };
    int nativeCalls=0;
    Need(SpawnObjectiveActor(&nativeCalls,&nativeCalls,plan.starts[0],begin,finish)==&nativeCalls && nativeCalls==2);
    auto invalid=plan.starts[0];memset(invalid.rotation,0,sizeof(invalid.rotation));
    Need(!SpawnObjectiveActor(&nativeCalls,&nativeCalls,invalid,begin,finish) && nativeCalls==2);
    Need(!SpawnObjectiveActor(nullptr,&nativeCalls,plan.starts[0],begin,finish) && nativeCalls==2);
}
static wchar_t ownedRoute[512];
static int routeCopies=0;
static unsigned nativePreparations=0;
bool InspectFixture(void* asset,TwinKartRace::ModeDefinition& out,void*) {
    if(!asset)return false;out=*static_cast<TwinKartRace::ModeDefinition*>(asset);return true;
}
void ModeBindingChecks() {
    using namespace TwinKartRace;
    wchar_t cubeObject[256]{};
    auto cube=CubeWorldForTrack(116,L"Desert_I06");Need(cube && CubeWorldObjectPath(*cube,cubeObject,256));
    Need(!wcscmp(cubeObject,L"/Game/ART/Env/Stage_ModeLab/CubeCube/Cube_Desert_I06/Cube_Desert_I06.Cube_Desert_I06"));
    Need(CubeWorldForTrack(118,L"Desert_I06")==cube && !CubeWorldObjectPath(*cube,cubeObject,12));
    Need(!CubeWorldForTrack(4,L"Desert_I06") && !CubeWorldForTrack(117,L"Desert_I06") && !CubeWorldForTrack(116,L"Village_I04"));
    Need(!CubeWorldForTrack(116,L"Cube_Desert_I06") && !CubeWorldForTrack(116,L"Desert_I06_Mirror"));
    Need(EventTrackSupports(1,116,L"Village_I05") && EventTrackSupports(1,118,L"Village_I05"));
    Need(EventTrackSupports(1,116,L"World_I11") && EventTrackSupports(1,118,L"World_I11") && !EventTrackSupports(1,4,L"World_I11"));
    Need(!EventTrackSupports(1,4,L"Village_I05") && !EventTrackSupports(1,116,L"Desert_I06") && !EventTrackSupports(2,116,L"Village_I04"));
    Need(NativeCharacterRules("s062_speed_solo_dao") && NativeCharacterRules("item_squad_miso_tiera"));
    Need(!NativeCharacterRules("speed_solo_ghost") && !NativeCharacterRules("") && !NativeCharacterRules(nullptr));
    struct ExpectedMenuMode {int mode;const wchar_t* profile;const wchar_t* id;unsigned char noParty;};
    // Pin the published event identities separately from the empty-profile
    // local modes. The same mode number can legitimately occur in both sets.
    const ExpectedMenuMode eventRows[]{
        {113,L"speed_solo_ghost",L"TwinKart_speed_solo_ghost",1},
        {113,L"speed_solo_derek",L"TwinKart_speed_solo_derek",1},
        {115,L"speed_squad_skill",L"TwinKart_speed_squad_skill",0},
        {112,L"item_squad_miso_tiera",L"TwinKart_item_squad_miso_tiera",0},
        {110,L"item_solo_brodi",L"TwinKart_item_solo_brodi",1},
        {113,L"s062_speed_solo_dao",L"TwinKart_s062_speed_solo_dao",1},
        {112,L"s062_item_squad_runner_attacker",L"TwinKart_s062_item_squad_runner_attacker",0},
        {105,L"s062_spitem_solo_skill",L"TwinKart_s062_spitem_solo_skill",1},
        {107,L"s062_spitem_squad_skill",L"TwinKart_s062_spitem_squad_skill",0},
        {104,L"s062_flag_skill",L"TwinKart_s062_flag_skill",0},
        {116,L"cubecubeitem_solo",L"TwinKart_cubecubeitem_solo",1},
        {118,L"cubecubeitem_squad",L"TwinKart_cubecubeitem_squad",0}
    };
    const ExpectedMenuMode localRows[]{
        {101,L"",L"TwinKart_LocalMode_101",1},{102,L"",L"TwinKart_LocalMode_102",0},
        {103,L"",L"TwinKart_LocalMode_103",0},{104,L"",L"TwinKart_LocalMode_104",0},
        {105,L"",L"TwinKart_LocalMode_105",1},{106,L"",L"TwinKart_LocalMode_106",0},
        {107,L"",L"TwinKart_LocalMode_107",0},{110,L"",L"TwinKart_LocalMode_110",1},
        {111,L"",L"TwinKart_LocalMode_111",0},{112,L"",L"TwinKart_LocalMode_112",0},
        {113,L"",L"TwinKart_LocalMode_113",1},{114,L"",L"TwinKart_LocalMode_114",0},
        {115,L"",L"TwinKart_LocalMode_115",0},{116,L"",L"TwinKart_LocalMode_116",1},
        {118,L"",L"TwinKart_LocalMode_118",0}
    };
    const ExpectedMenuMode prototypeRows[]{
        {100,L"",L"TwinKart_FactoryRun",0},{108,L"",L"TwinKart_Chaser",0},
        {109,L"",L"TwinKart_Hide_Repaired",1}
    };
    auto checkMenuRow=[](const EventMenuRow& row,int index,const ExpectedMenuMode& expected) {
        Need(row.active==1 && row.index==index+1 && row.sort==index+1 && row.noParty==expected.noParty);
        Need(row.modes.count==1 && row.modes.capacity==1 && row.modes.data);
        const auto& mode=row.modes.data[0];Need(mode.mode==expected.mode);
        Need(mode.profile.data && mode.profile.count==static_cast<int>(wcslen(expected.profile)+1) &&
            mode.profile.capacity==mode.profile.count && !wcscmp(mode.profile.data,expected.profile));
        Need(row.id.data && row.id.count==static_cast<int>(wcslen(expected.id)+1) &&
            row.id.capacity==row.id.count && !wcscmp(row.id.data,expected.id));
        Need(!row.hours.count && !row.legacyModes.count && !wcscmp(row.start.data,L"2000/1/1 0:00:00") &&
            !wcscmp(row.end.data,L"2099/12/31 23:59:59"));
    };
    auto checkBaseMenu=[&](const EventMenuCopySource& value) {
        int index=0;
        for(size_t i=0;i<FeaturedLocalModeCount;++i){checkMenuRow(value.source.rows.data[index],index,localRows[i]);++index;}
        for(const auto& expected:eventRows){checkMenuRow(value.source.rows.data[index],index,expected);++index;}
        for(size_t i=FeaturedLocalModeCount;i<ARRAYSIZE(localRows);++i){checkMenuRow(value.source.rows.data[index],index,localRows[i]);++index;}
        Need(index==27);
    };
    EventMenuCopySource menu;Need(!menu.Build(nullptr));Need(menu.Build(&menu));
    Need(menu.source.rows.count==27 && menu.source.rows.capacity==27 && menu.source.baseVtable==&menu);
    checkBaseMenu(menu);
    EventMenuCopySource optionalMenu;
    for(unsigned gates=0;gates<8;++gates) {
        bool factory=(gates&1)!=0,chaser=(gates&2)!=0,hide=(gates&4)!=0;
        Need(optionalMenu.Build(&optionalMenu,factory,chaser,hide));
        int expectedCount=27+(factory?1:0)+(chaser?1:0)+(hide?1:0);
        Need(optionalMenu.source.rows.count==expectedCount && optionalMenu.source.rows.capacity==expectedCount &&
            optionalMenu.source.baseVtable==&optionalMenu);
        checkBaseMenu(optionalMenu);
        int index=27;
        for(unsigned prototype=0;prototype<3;++prototype)if(gates&(1u<<prototype)) {
            checkMenuRow(optionalMenu.source.rows.data[index],index,prototypeRows[prototype]);++index;
        }
        Need(index==expectedCount && menu.source.rows.count==27);checkBaseMenu(menu);
    }
    Need(optionalMenu.Build(&optionalMenu) && optionalMenu.source.rows.count==27);checkBaseMenu(optionalMenu);
    Request request{};strcpy_s(request.identity,"0123456789abcdef0123456789abcdef");strcpy_s(request.track,"Pirate_R01");
    request.mode=110;request.ai=7;request.laps=3;strcpy_s(request.profile,"item_solo_brodi");
    ModeDefinition definition{};definition.asset=&definition;definition.kind=0;definition.teamSize=1;definition.teamCount=8;
    wcscpy_s(definition.gameClass,L"/Script/KartDrift.SkillSingleGameMode");wcscpy_s(definition.travelName,L"ItemSingle");
    wchar_t profile[]=L"item_solo_brodi",fallback[]=L"ItemLabSolo";
    ModeMapEntry entries[]={{{fallback,12,12},&definition,0,0},{{profile,16,16},&definition,0,0}};
    ModeDefinition resolved{};wchar_t options[512]{};
    Need(ResolveModeBinding(request,entries,2,InspectFixture,nullptr,resolved));
    Need(ModeOptions(request,resolved,options,512) && wcsstr(options,L"SkillSingleGameMode?gamemode=ItemSingle") && wcsstr(options,L"?gameprofile=item_solo_brodi"));
    Need(wcsstr(options,L"?racemode=110?gameprofile=item_solo_brodi"));
    Need(wcsstr(options,L"?game.useskill=true?game.skill.activesp=80p") && ValidCustomOptions("?game.skill.activesp=80p?game.useskill=true"));
    strcpy_s(request.custom,"?game.useskill=false");Need(!ModeOptions(request,resolved,options,512));
    strcpy_s(request.custom,"?game.useskill=true");Need(ModeOptions(request,resolved,options,512));request.custom[0]=0;
    // Native room controls must affect gameplay and cannot bypass mandatory
    // event rules by spelling the same setting in the UI namespace.
    strcpy_s(request.custom,"?item.UseSkill=false");Need(!ModeOptions(request,resolved,options,512));
    strcpy_s(request.custom,"?item.ActiveSkillCoolTimePercentage=20?item.UseSkill=true");
    Need(ModeOptions(request,resolved,options,512) && wcsstr(options,L"?game.skill.activeinitialsp=80p"));
    strcpy_s(request.custom,"?item.ActiveSkillCoolTimePercentage=50");Need(!ModeOptions(request,resolved,options,512));
    Request converted=request;request.profile[0]=0;
    strcpy_s(request.custom,"?item.ActiveSkillCoolTime=false?item.SkillPercentage=false");
    Need(NormalizeUiOptions(request,converted) && strstr(converted.custom,"?game.skill.activesp=1?game.skill.activeinitialsp=0") && strstr(converted.custom,"?game.skill.percentage100=true"));
    strcpy_s(request.custom,"?item.ActiveSkillCoolTimePercentage=101");Need(!NormalizeUiOptions(request,converted));
    strcpy_s(request.custom,"?item.UseSkill=maybe");Need(!NormalizeUiOptions(request,converted));
    request.mode=113;strcpy_s(request.custom,"?speed.Collision=false?speed.Draft=false?speed.TeamBooster=true");
    Need(NormalizeUiOptions(request,converted) && strstr(converted.custom,"?game.ignorealways=true") && strstr(converted.custom,"?game.draft=false") && strstr(converted.custom,"?game.teambooster=true"));
    strcpy_s(request.custom,"?speed.Collision=false?game.ignorealways=false");Need(!NormalizeUiOptions(request,converted));
    request.mode=110;strcpy_s(request.profile,"item_solo_brodi");request.custom[0]=0;
    Need(!ValidCustomOptions("?game.command=quit") && !ValidCustomOptions("?game.skill.exe=bad"));
    auto hitRun=EventProfile("s062_item_squad_runner_attacker");Need(hitRun && !wcscmp(EventCharacter(*hitRun,0,8),L"Character:100000043") && !wcscmp(EventCharacter(*hitRun,7,8),L"Character:100000027"));
    strcpy_s(request.profile,"unknown");Need(!ResolveModeBinding(request,entries,2,InspectFixture,nullptr,resolved));
    request.profile[0]=0;Need(ResolveModeBinding(request,entries,2,InspectFixture,nullptr,resolved));
    Need(ModeOptions(request,resolved,options,512) && !wcsstr(options,L"?gameprofile="));
    entries[1]=entries[0];Need(!ResolveModeBinding(request,entries,2,InspectFixture,nullptr,resolved));
    Need(!ResolveModeBinding(request,entries,513,InspectFixture,nullptr,resolved));
    definition.teamSize=2;Need(!ResolveModeBinding(request,entries,1,InspectFixture,nullptr,resolved));definition.teamSize=1;
    definition.teamCount=4;Need(!ResolveModeBinding(request,entries,1,InspectFixture,nullptr,resolved));definition.teamCount=8;
    wcscpy_s(definition.travelName,L"SpeedSingle");Need(!ResolveModeBinding(request,entries,1,InspectFixture,nullptr,resolved));
    wcscpy_s(definition.travelName,L"ItemSingle");
    wcscpy_s(definition.gameClass,L"/Script/KartDrift.SkillSingleGameMode?listen");Need(!ResolveModeBinding(request,entries,1,InspectFixture,nullptr,resolved));
    request.mode=112;definition.teamSize=4;definition.teamCount=2;
    wcscpy_s(definition.gameClass,L"/Script/KartDrift.SkillTeamGameMode");wcscpy_s(definition.travelName,L"ItemTeam");
    Need(ModeOptions(request,definition,options,512) && wcsstr(options,L"SkillTeamGameMode?gamemode=ItemTeam?team=2?teamsize=4"));
    Need(!ModeOptions(request,definition,options,8));
    request.mode=116;CanonicalModeProfile(request);Need(!strcmp(request.profile,"cubecubeitem_solo"));
    request.mode=118;request.profile[0]=0;CanonicalModeProfile(request);Need(!strcmp(request.profile,"cubecubeitem_squad"));
    request.mode=110;strcpy_s(request.profile,"s062_item_solo_brodi");CanonicalModeProfile(request);Need(!strcmp(request.profile,"item_solo_brodi"));
    request.mode=113;strcpy_s(request.profile,"s062_item_solo_brodi");CanonicalModeProfile(request);Need(!strcmp(request.profile,"s062_item_solo_brodi"));
}
void* __fastcall CopyPreparation(TwinKartRace::PreparationString* out,const TwinKartRace::PreparationString* in) {
    *out={new wchar_t[in->count],in->count,in->count};std::wmemcpy(out->data,in->data,in->count);return out;
}
bool __fastcall InitializePreparation(void* traveler,const TwinKartRace::PreparationMode* mode,TwinKartRace::PreparationString* track,void* details,const TwinKartRace::PreparationHints* hints,bool random) {
    Need(traveler==&nativePreparations && mode->mode==4 && !wcscmp(mode->profile.data,L"item_solo_test") &&
        !wcscmp(track->data,L"Pirate_R01") && !details && !hints->count && !hints->data && !random);
    ++nativePreparations;delete[] track->data;return true;
}
void* __fastcall AssignRoute(TwinKartRace::ReturnString* out,const TwinKartRace::ReturnString* in) {
    ++routeCopies;std::wmemcpy(ownedRoute,in->data,in->count);*out={ownedRoute,in->count,512};return out;
}
int main(int argc,char** argv) {
    ObjectiveCaptureChecks();
    ModeBindingChecks();
    TwinKartRace::Request factory;
    Need(TwinKartFactory::ParseCapture("TKFACTORY_CAPTURE1 0123456789abcdef0123456789abcdef Factory_I01\n",&factory));
    Need(factory.mode==100 && factory.ai==36 && factory.laps==5 && !TwinKartRace::Valid(factory));
    wchar_t factoryOptions[512]{};
    Need(TwinKartFactory::CaptureOptions(factory,factoryOptions,512) && wcsstr(factoryOptions,L"?players=4") &&
        wcsstr(factoryOptions,L"?ais=36") && wcsstr(factoryOptions,L"TobyRushGameMode"));
    Need(!TwinKartFactory::CaptureOptions(factory,factoryOptions,8));
    Need(TwinKartFactory::ParseStart("TKFACTORY1 0123456789abcdef0123456789abcdef Factory_I01\n",&factory));
    Need(factory.mode==100 && factory.ai==36 && factory.laps==5 && TwinKartFactory::CaptureOptions(factory,factoryOptions,512));
    for(auto bad:{"TKFACTORY1 bad Factory_I01","TKFACTORY1 0123456789abcdef0123456789abcdef Other_I01",
        "TKFACTORY1 0123456789abcdef0123456789abcdef Factory_I01 39",
        "TKFACTORY1 0123456789abcdef0123456789abcdef Factory_I01?game=Other"})Need(!TwinKartFactory::ParseStart(bad,&factory));
    for(const char* bad:{"TKFACTORY_CAPTURE1 bad Factory_I01","TKFACTORY_CAPTURE1 0123456789abcdef0123456789abcdef Factory_I01?game=Other",
        "TKFACTORY_CAPTURE1 0123456789abcdef0123456789abcdef Factory_I01 39","TKRACE1 0123456789abcdef0123456789abcdef Factory_I01 100 36 5"})
        Need(!TwinKartFactory::ParseCapture(bad,&factory));
    factory.ai=39;Need(!TwinKartFactory::CaptureOptions(factory,factoryOptions,512));
    wchar_t lobbyText[]=L"/Game/Test/Lobby";
    TwinKartRace::ReturnString lobby{lobbyText,static_cast<int>(sizeof(lobbyText)/sizeof(wchar_t)),static_cast<int>(sizeof(lobbyText)/sizeof(wchar_t))},route{};
    Need(TwinKartRace::PrepareReturnRoute(route,lobby,false,AssignRoute)==-1 && routeCopies==0);
    Need(TwinKartRace::PrepareReturnRoute(route,lobby,true,AssignRoute)==2 && routeCopies==1 && route.data!=lobby.data);
    Need(TwinKartRace::PrepareReturnRoute(route,lobby,true,AssignRoute)==1 && routeCopies==1);
    lobbyText[1]=L'X';Need(route.data[1]==L'G'); // Native-owned copy survives source changes/travel.
    TwinKartRace::ReturnString empty{},broken{lobbyText,600,600};
    Need(TwinKartRace::PrepareReturnRoute(empty,broken,true,AssignRoute)==-1 && !empty.data && routeCopies==1);
    broken={lobbyText,4,2};Need(!TwinKartRace::HasReturnRoute(broken));
    Need(TwinKartRace::PrepareReturnRoute(empty,empty,true,AssignRoute)==-1 && routeCopies==1);
    // Native byte-enum array, independent of track name conventions. Check
    // malformed records leave the caller's existing evidence unchanged.
    Need(TwinKartRace::DevelopmentWorldPath(L"/Game/Dev/HideNSeek/HideNSeekTestLevel.HideNSeekTestLevel"));
    Need(TwinKartRace::DevelopmentWorldPath(L"World'/Game/Dev/HideNSeek/HideNSeekTestLevel.HideNSeekTestLevel'"));
    Need(TwinKartRace::DevelopmentWorldPath(L"/game/dev/OtherMap.OtherMap"));
    Need(!TwinKartRace::DevelopmentWorldPath(L"/Game/ART/Env/Stage_ModeLab/FlagMode/Flag_F01/Flag_F01.Flag_F01"));
    Need(!TwinKartRace::DevelopmentWorldPath(L"/Game/Device/World.World") && !TwinKartRace::DevelopmentWorldPath(nullptr));
    unsigned char definition[0x160]{};unsigned char types[]={1,2};
    struct Array {const unsigned char* data;int count,capacity;} array{types,2,2};
    memcpy(definition+0x18,&array,sizeof(array));unsigned mask=99;
    Need(TwinKartRace::ReadNativeTrackModeMask(definition,sizeof(definition),mask) && mask==6);
    Need(TwinKartRace::NativeTrackSupports(mask,1) && TwinKartRace::NativeTrackSupports(mask,4));
    types[0]=0;Need(TwinKartRace::ReadNativeTrackModeMask(definition,sizeof(definition),mask));
    Need(TwinKartRace::NativeTrackSupports(mask,1) && !TwinKartRace::NativeTrackSupports(mask,4));
    types[0]=3;types[1]=4;Need(TwinKartRace::ReadNativeTrackModeMask(definition,sizeof(definition),mask));
    Need(!TwinKartRace::NativeTrackSupports(mask,1) && !TwinKartRace::NativeTrackSupports(mask,4));
    types[1]=5;mask=99;Need(!TwinKartRace::ReadNativeTrackModeMask(definition,sizeof(definition),mask) && mask==99);
    array.count=3;memcpy(definition+0x18,&array,sizeof(array));
    Need(!TwinKartRace::ReadNativeTrackModeMask(definition,sizeof(definition),mask));
    array={nullptr,1,1};memcpy(definition+0x18,&array,sizeof(array));
    Need(!TwinKartRace::ReadNativeTrackModeMask(definition,sizeof(definition),mask));
    array={nullptr,0,0};memcpy(definition+0x18,&array,sizeof(array));
    Need(TwinKartRace::ReadNativeTrackModeMask(definition,sizeof(definition),mask) && mask==0);
    Need(!TwinKartRace::ReadNativeTrackModeMask(definition,0x27,mask) && !TwinKartRace::NativeTrackSupports(0xffffffff,1));
    TwinKartRace::Request request; const char* valid="TKRACE1 0123456789abcdef0123456789abcdef Pirate_R01 1 7 3\n";
    Need(TwinKartRace::Parse(valid,&request)); wchar_t options[512]{};
    Need(TwinKartRace::Options(request,options,512) && wcsstr(options,L"SpeedSingleGameMode") && wcsstr(options,L"players=1?observers=0?ais=7?lap=3"));
    request.mode=4; Need(TwinKartRace::Options(request,options,512) && wcsstr(options,L"ItemSingleGameMode"));
    request.ai=8; Need(!TwinKartRace::Options(request,options,512));
    Need(!TwinKartRace::Parse("TKRACE1 bad Pirate_R01 1 7 3",&request));
    Need(!TwinKartRace::Parse("TKRACE1 0123456789abcdef0123456789abcdef Pirate_R01?listen 1 7 3",&request));
    Need(TwinKartRace::Parse("TKRACE1 0123456789abcdef0123456789abcdef Pirate_R01 2 7 3",&request));
    Need(TwinKartRace::Options(request,options,512) && wcsstr(options,L"SpeedTeamGameMode") && wcsstr(options,L"team=4?teamsize=2"));
    request.mode=6;Need(TwinKartRace::Options(request,options,512) && wcsstr(options,L"ItemTeamGameMode") && wcsstr(options,L"team=2?teamsize=4"));
    for(int mode=1;mode<=6;++mode) {
        Need(TwinKartRace::NativeTrackSupports(6,mode));
        Need(!TwinKartRace::NativeTrackSupports(mode<=3?2:4,mode));
        request.mode=mode;request.ai=7;Need(TwinKartRace::Valid(request));
        int counts[5]{};
        for(int slot=0;slot<8;++slot) {int team=TwinKartRace::SlotTeam(mode,slot,8);Need(team>=0 && team<=4);++counts[team];}
        int size=TwinKartRace::TeamSize(mode);
        if(size==1)Need(counts[0]==8);
        else for(int team=1;team<=8/size;++team)Need(counts[team]==size);
        Need(TwinKartRace::SlotTeam(mode,-1,8)==-1 && TwinKartRace::SlotTeam(mode,8,8)==-1);
    }
    request.mode=3;request.ai=3;Need(!TwinKartRace::Valid(request));
    request.mode=2;request.ai=2;Need(!TwinKartRace::Valid(request));
    request.mode=2;request.ai=3;Need(TwinKartRace::Valid(request));
    request.mode=104;request.ai=7;Need(TwinKartRace::Valid(request));
    Need(TwinKartRace::Options(request,options,512) && wcsstr(options,L"FlagGameMode?gamemode=FlagSquad?team=2?teamsize=4?players=1?observers=0?ais=7") && !wcsstr(options,L"?lap="));
    Need(TwinKartRace::NativeTrackSupports(16,104) && !TwinKartRace::NativeTrackSupports(6,104));
    Need(TwinKartRace::SlotTeam(104,0,8)==1 && TwinKartRace::SlotTeam(104,1,8)==2 && TwinKartRace::SlotTeam(104,7,8)==2);
    request.ai=3;Need(!TwinKartRace::Valid(request));
    for(int mode=105;mode<=107;++mode) {
        request.mode=mode;request.ai=7;
        Need(TwinKartRace::Valid(request) && TwinKartRace::Options(request,options,512));
        Need(wcsstr(options,mode==105?L"SpitemSingleGameMode":L"SpitemTeamGameMode"));
        Need(TwinKartRace::NativeTrackSupports(8,mode) && !TwinKartRace::NativeTrackSupports(6,mode) && !TwinKartRace::NativeTrackSupports(16,mode));
        Need(TwinKartRace::BehaviorGetter(mode)==(mode==105?0x220fed0u:0x22174e0u));
        Need(TwinKartRace::ResultFactory(mode)==(mode==105?0x168dbc0u:0x168db70u));
        request.ai=3;Need(TwinKartRace::Valid(request)==(mode!=107));
    }
    for(int mode=101;mode<=103;++mode) {
        request.mode=mode;request.ai=7;Need(TwinKartRace::Valid(request));
        Need(TwinKartRace::Options(request,options,512) && wcsstr(options,L"game=/Script/KartDrift.ModeLabGameMode?gamemode=InfinityBooster?"));
        Need(TwinKartRace::NativeTrackSupports(4,mode) && !TwinKartRace::NativeTrackSupports(2,mode) && !TwinKartRace::NativeTrackSupports(16,mode));
        Need(TwinKartRace::SpeedMode(mode) && TwinKartRace::BehaviorGetter(mode)==(mode==101?0x2218050u:0x2218390u));
        request.ai=3;Need(TwinKartRace::Valid(request)==(mode!=103));
    }
    for(int mode:{110,111,112,113,114,115,116,118}) {
        request.mode=mode;request.ai=7;Need(TwinKartRace::Valid(request) && TwinKartRace::ModeName(mode));
        Need(TwinKartRace::NativeTrackSupports(TwinKartRace::SpeedMode(mode)?4:2,mode));
        Need(TwinKartRace::TeamSize(mode)==((mode==116 || mode==110 || mode==113)?1:(mode==111 || mode==114)?2:4));
    }
    for(int mode:{100,109,117,119,120,121,122}) {
        request.mode=mode;request.ai=7;Need(!TwinKartRace::Valid(request) && !TwinKartRace::ModeName(mode));
    }
    request.mode=108;request.ai=7;strcpy_s(request.track,"Flag_F01");
    Need(TwinKartRace::Valid(request) && TwinKartRace::Options(request,options,512) && wcsstr(options,L"ChaserGameMode") &&
        wcsstr(options,L"?team=1?teamsize=16") && !wcsstr(options,L"?lap="));
    strcpy_s(request.track,"Flag_F02");Need(!TwinKartRace::Valid(request));strcpy_s(request.track,"Pirate_R01");
    Need(!TwinKartRace::Parse("TKRACE1 0123456789abcdef0123456789abcdef Pirate_R01 1 7 3 extra",&request));
    Need(!TwinKartRace::Parse("TKRACE1 0123456789abcdef0123456789abcdef Pirate_R01 1 0 3",&request));
    Need(TwinKartRace::Parse("TKRACE2 0123456789abcdef0123456789abcdef Pirate_R01 4 7 3 item_solo_test ?item.IgnoreFaraway=false?item.UseSkill=true?item.ActiveSkillCoolTime=false?item.CustomItem=Invalid",&request));
    Need(TwinKartRace::Options(request,options,512) && wcsstr(options,L"?item.UseSkill=true") && wcsstr(options,L"?item.ActiveSkillCoolTime=false"));
    Need(TwinKartRace::PrepareNativeRace(&nativePreparations,request,CopyPreparation,InitializePreparation) && nativePreparations==1);
    Need(!TwinKartRace::PrepareNativeRace(nullptr,request,CopyPreparation,InitializePreparation) && nativePreparations==1);
    for(const char* bad:{"?game=OtherGameMode","?item.UseSkill=1?ITEM.UseSkill=0","?item.UseSkill=","?item.CustomItem=foo/bar","?item.CustomItem=x;quit","?item.CustomItem=x extra","?players=99"}) {
        char text[1025]{};sprintf_s(text,"TKRACE2 0123456789abcdef0123456789abcdef Pirate_R01 4 7 3 - %s",bad);
        Need(!TwinKartRace::Parse(text,&request));
    }
    Need(TwinKartRace::Parse("TKRACE2 0123456789abcdef0123456789abcdef Pirate_R01 4 7 3 - -",&request) && !request.profile[0] && !request.custom[0]);
    Need(!TwinKartRace::Parse("TKRACE2 0123456789abcdef0123456789abcdef Pirate_R01 4 7 3 bad/profile -",&request));
    if(argc==2) {
        std::ifstream file(argv[1],std::ios::binary); std::string text((std::istreambuf_iterator<char>(file)),{});
        Need(file.good() || file.eof()); Need(TwinKartRace::Parse(text.c_str(),&request) && request.mode==4 && request.ai==7 && request.laps==3 && !strcmp(request.track,"Pirate_R01"));
    }
    puts("PASS native race request: bounded syntax, supported modes, player/AI separation and no command injection.");
}
