#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include "../native/NativeChaserMode.h"
#include "../native/NativeChaserConfig.h"
namespace O=TwinKartObjective;
namespace R=TwinKartRace;
namespace {
int originalCalls=0,retainedCalls=0,createCalls=0;
int nativeValue=1,retainedValue=2,callbackValue=3;
void* nativeResult=nullptr;
void Need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void* __fastcall Native(const R::PreparationMode*){++originalCalls;return nativeResult;}
void* Retained(void* context){Need(context==&retainedValue,"Retained definition context changed");++retainedCalls;return context;}
struct ClassFixture {std::array<unsigned char,0x118> header{};void* stored=nullptr;};
static_assert(offsetof(ClassFixture,stored)==0x118,"Original PC class-default offset");
bool publishDefault=true;
void* __fastcall Create(void* raw){++createCalls;if(publishDefault)static_cast<ClassFixture*>(raw)->stored=&retainedValue;return &callbackValue;}
unsigned nameReads=0;bool nameValid=true;
bool ClassName(std::uint64_t name,wchar_t* out,std::size_t capacity) {
    ++nameReads;Need(name==0x1122334455667788ull && capacity==192,"Native class-path FName ABI changed");
    if(!nameValid)return false;
    return wcscpy_s(out,capacity,L"/Script/KartDrift.ChaserGameMode")==0;
}
struct ConnectionFixture {unsigned calls=0,slots=0;int fail=-1;};
bool Connect(const O::ChaserConnectionSeat& seat,void* raw) {
    auto& fixture=*static_cast<ConnectionFixture*>(raw);++fixture.calls;
    Need(seat.ai && seat.slot>0 && (seat.connection==0 || seat.connection==3),"Native connection setter reached a human, existing connection or disconnected seat");
    if(seat.slot==fixture.fail)return false;
    fixture.slots|=1u<<seat.slot;return true;
}
int nativeNetworkMode=0;unsigned nativeModeCalls=0,worldChecks=0;
int __fastcall NativeMode(void*){++nativeModeCalls;return nativeNetworkMode;}
bool OwnsWorld(void* world,void* context){++worldChecks;return world==context;}
bool FaultWorld(void*,void*){RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);return true;}
bool transitionNative=true,transitionOwned=true,transitionReady=false;unsigned transitionCalls=0,transitionChecks=0;
bool __fastcall NativeTransition(void*){++transitionCalls;return transitionNative;}
bool LoadingReady(void*,bool& owned,void*){++transitionChecks;owned=transitionOwned;return transitionReady;}
bool LoadingFault(void*,bool& owned,void* context){owned=context!=nullptr;RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);return true;}
}
static unsigned configLoads=0,configFallbacks=0;static void* configNative=nullptr;
static void* __fastcall ConfigLoad(void*){++configLoads;return configNative;}
static void* ConfigDefault(void* reference,void* context){++configFallbacks;return reference==&nativeValue?context:nullptr;}
int main(){try {
    Need(O::ChaserCaptureTaggers(L"7")==7 && O::ChaserCaptureTaggers(L"1")==1 &&
         !O::ChaserCaptureTaggers(nullptr) && !O::ChaserCaptureTaggers(L"") && !O::ChaserCaptureTaggers(L"8") &&
         !O::ChaserCaptureTaggers(L"7 ") && !O::ChaserCaptureTaggers(L"07"),"Capture role count syntax escaped its bound");
    {
        ClassFixture type;alignas(8) std::array<unsigned char,0xb8> instance{};
        *reinterpret_cast<unsigned*>(instance.data()+8)=0x40;
        *reinterpret_cast<void**>(instance.data()+0x10)=&type;
        auto before=instance;
        Need(O::ConfigureAuthoredChaserCapture(instance.data(),&type,7) &&
             *reinterpret_cast<int*>(instance.data()+0x40)==7,"Owned capture configuration was not applied");
        for(size_t i=0;i<instance.size();++i)if(i<0x40 || i>=0x44)
            Need(instance[i]==before[i],"Capture changed timers, inventories or another native field");
        type.stored=instance.data();before=instance;
        Need(!O::ConfigureAuthoredChaserCapture(instance.data(),&type,1) && instance==before,"Native class default was modified");
        type.stored=nullptr;instance[0x44]=1;before=instance;
        Need(!O::ConfigureAuthoredChaserCapture(instance.data(),&type,1) && instance==before,"Flexible native configuration was overwritten");
        instance[0x44]=0;instance[8]|=0x10;before=instance;
        Need(!O::ConfigureAuthoredChaserCapture(instance.data(),&type,1) && instance==before,"Default/archetype capture accepted");
        instance[8]=0;before=instance;
        Need(!O::ConfigureAuthoredChaserCapture(instance.data(),&type,1) && instance==before,"Nontransient existing config was overwritten");
        Need(!O::ConfigureAuthoredChaserCapture(nullptr,&type,7) &&
             !O::ConfigureAuthoredChaserCapture(instance.data(),&type,8),"Invalid capture configuration accepted");
        puts("PASS isolated Chaser branch fixture: exact opt-in syntax, only authored transient initial population changed, native defaults/timers/inventory/results preserved. No original match executed.");
    }
    Need(O::ResolveChaserConfigAsset(&nativeValue,true,true,ConfigLoad,ConfigDefault,&retainedValue)==&retainedValue &&
        configLoads==1 && configFallbacks==1,"Missing owned Chaser config did not reach its qualified original default");
    configNative=&callbackValue;
    Need(O::ResolveChaserConfigAsset(&nativeValue,true,true,ConfigLoad,ConfigDefault,&retainedValue)==&callbackValue &&
        configLoads==2 && configFallbacks==1,"Existing config was overwritten");configNative=nullptr;
    Need(!O::ResolveChaserConfigAsset(&nativeValue,false,true,ConfigLoad,ConfigDefault,&retainedValue) &&
        !O::ResolveChaserConfigAsset(&nativeValue,true,false,ConfigLoad,ConfigDefault,&retainedValue) &&
        !O::ResolveChaserConfigAsset(nullptr,true,true,ConfigLoad,ConfigDefault,&retainedValue) && configLoads==5 && configFallbacks==1,
        "Disabled/foreign-thread/null config entered fallback");
    Need(!O::ResolveChaserConfigAsset(&nativeValue,true,true,nullptr,ConfigDefault,&retainedValue) && configLoads==5,
        "Absent native config loader accepted");
    puts("PASS Chaser config source boundary: native load once, existing asset preservation, exact qualified fallback responsibility, feature/thread/null isolation. Synthetic callbacks only; no original serialization executed.");
    R::PreparationMode request{};request.mode=108;
    auto resolve=[&](bool enabled=true,bool owner=true){return O::ResolveRetainedChaser(&request,enabled,owner,Native,Retained,&retainedValue);};
    Need(resolve()==&retainedValue && originalCalls==1 && retainedCalls==1,"Missing original Chaser definition did not reach the retained class boundary");
    nativeResult=&nativeValue;
    Need(resolve()==&nativeValue && originalCalls==2 && retainedCalls==1,"An existing native definition was overwritten");
    nativeResult=nullptr;
    Need(!resolve(false) && !resolve(true,false) && originalCalls==4 && retainedCalls==1,"Disabled/foreign-thread lookup entered compatibility logic");
    for(int mode:{1,4,100,104,105,109,116,118}) {
        request.mode=static_cast<unsigned char>(mode);Need(!resolve(),"Another original mode was mapped to Chaser");
    }
    request.mode=108;
    wchar_t explicitProfile[]=L"original_profile";
    request.profile={explicitProfile,static_cast<int>(wcslen(explicitProfile)+1),static_cast<int>(wcslen(explicitProfile)+1)};
    Need(!resolve() && retainedCalls==1,"An explicit original profile silently fell back to class defaults");
    wchar_t empty[]=L"";request.profile={empty,1,1};
    Need(resolve()==&retainedValue && retainedCalls==2,"An original empty FString profile was rejected");
    wchar_t unterminated[]{L'x'};request.profile={unterminated,1,1};
    Need(!resolve(),"Malformed native profile reached retained construction");
    request.profile={nullptr,2,2};Need(!resolve(),"Missing native profile buffer reached retained construction");
    request.profile={nullptr,-1,0};Need(!resolve(),"Negative native profile count reached retained construction");
    request.profile={};
    Need(!O::ResolveRetainedChaser(nullptr,true,true,Native,Retained,&retainedValue),"Null mode reached retained construction");
    int beforeOriginal=originalCalls;
    Need(!O::ResolveRetainedChaser(&request,true,true,nullptr,Retained,&retainedValue) && originalCalls==beforeOriginal,"Absent native forwarding target was accepted");
    Need(!O::ResolveRetainedChaser(&request,true,true,Native,nullptr,nullptr),"Absent retained reader was accepted");
    O::ChaserModeDefinition definition;definition.asset=&retainedValue;definition.kind=2;
    definition.teamSize=16;definition.teamCount=1;
    wcscpy_s(definition.gameClass,L"/Script/KartDrift.ChaserGameMode");wcscpy_s(definition.travelName,L"Chaser");
    Need(O::ValidChaserDefinition(definition),"Original Chaser descriptor was rejected");
    definition.teamSize=8;Need(!O::ValidChaserDefinition(definition),"An eight-seat circuit definition was accepted as Chaser");definition.teamSize=16;
    definition.teamCount=2;Need(!O::ValidChaserDefinition(definition),"Race teams were confused with native dynamic Chaser roles");definition.teamCount=1;
    definition.kind=1;Need(!O::ValidChaserDefinition(definition),"Wrong native mode kind was accepted");definition.kind=2;
    wcscpy_s(definition.gameClass,L"/Script/KartDrift.ItemSingleGameMode");Need(!O::ValidChaserDefinition(definition),"An item game class was accepted as Chaser");
    wcscpy_s(definition.gameClass,L"/Script/KartDrift.ChaserGameMode");wcscpy_s(definition.travelName,L"FlagSquad");
    Need(!O::ValidChaserDefinition(definition),"A renamed Flag definition was accepted as Chaser");
    ClassFixture type;
    Need(O::OriginalClassDefault(&type,Create)==&retainedValue && type.stored==&retainedValue && createCalls==1,
         "Class default reader returned the callback temporary instead of the engine-owned object");
    Need(O::OriginalClassDefault(&type,Create)==&retainedValue && createCalls==1,"Existing class default was constructed twice");
    type.stored=nullptr;publishDefault=false;
    Need(!O::OriginalClassDefault(&type,Create) && createCalls==2,"Unstored transient callback return escaped as a class default");
    Need(!O::OriginalClassDefault(&type,nullptr) && !O::OriginalClassDefault(nullptr,Create) && createCalls==2,
         "Missing class/create entry caused an invalid native call");
    std::array<unsigned char,0x258> nativeAsset{};
    auto write=[&](unsigned offset,auto value){memcpy(nativeAsset.data()+offset,&value,sizeof(value));};
    wchar_t travel[]=L"Chaser";
    TwinKartEquipment::String travelValue{travel,7,7};
    type.stored=nativeAsset.data();write(8,0x30u);write(0x10,static_cast<void*>(&type));
    write(0x40,0x1122334455667788ull);write(0x230,2);write(0x238,travelValue);write(0x248,16);write(0x24c,1);
    auto bytesBefore=nativeAsset;O::ChaserModeDefinition copied;
    Need(O::ReadOriginalChaserDefault(&type,nativeAsset.data(),ClassName,copied) && O::ValidChaserDefinition(copied) && nameReads==1,
         "Exact original class-default descriptor did not reach the production reader");
    auto rejected=[&] {
        O::ChaserModeDefinition previous;previous.asset=&callbackValue;
        Need(!O::ReadOriginalChaserDefault(&type,nativeAsset.data(),ClassName,previous) && previous.asset==&callbackValue,
             "Rejected native class default published a partial definition");
    };
    write(8,0x20u);rejected();write(8,0x18030u);rejected();write(8,0x30u);
    write(0x10,static_cast<void*>(&callbackValue));rejected();write(0x10,static_cast<void*>(&type));
    type.stored=&callbackValue;rejected();type.stored=nativeAsset.data();
    write(0x248,8);rejected();write(0x248,16);
    write(0x24c,2);rejected();write(0x24c,1);
    travelValue.count=65;write(0x238,travelValue);rejected();travelValue.count=7;write(0x238,travelValue);
    nameValid=false;rejected();nameValid=true;
    Need(nativeAsset==bytesBefore,"The original class-default metadata was mutated while reading it");
    int pawns[16]{},states[16]{};O::ChaserConnectionSeat seats[16];
    bool spawnAi=true;
    Need(O::ChaserSpawnSlot(1,15,0,spawnAi) && !spawnAi && O::ChaserSpawnSlot(1,15,15,spawnAi) && spawnAi &&
         O::ChaserSpawnSlot(1,7,7,spawnAi) && spawnAi,"Original Chaser local human/bot slot classification failed");
    for(auto invalidSlot:{-1,16,100}) {
        spawnAi=false;Need(!O::ChaserSpawnSlot(1,15,invalidSlot,spawnAi) && !spawnAi,"Invalid Chaser slot changed native spawn classification");
    }
    Need(!O::ChaserSpawnSlot(2,14,1,spawnAi) && !O::ChaserSpawnSlot(1,0,0,spawnAi) &&
         !O::ChaserSpawnSlot(1,16,1,spawnAi),"Unsupported Chaser population was accepted");
    for(int i=0;i<16;++i)seats[i]={&pawns[i],&states[i],i,i!=0,i==0?1u:0u};
    ConnectionFixture connections;unsigned completed=999,pending=999;
    Need(O::PrepareChaserConnections(seats,16,Connect,&connections,completed) && completed==15 && connections.calls==15 && connections.slots==65534,
         "Sixteen-seat native roster did not notify exactly its new standalone bots");
    connections={};seats[1].connection=1;seats[2].connection=2;
    Need(O::PrepareChaserConnections(seats,16,Connect,&connections,completed) && completed==13 && connections.slots==65528,
         "Connected/disconnected native seats were changed during initial role preparation");
    seats[1].connection=seats[2].connection=0;seats[15].slot=14;connections={};
    Need(!O::PrepareChaserConnections(seats,16,Connect,&connections,completed) && !connections.calls && completed==0,
         "Malformed late roster entry caused partial native connection notifications");seats[15].slot=15;
    auto invalid=[&] {
        connections={};pending=123;
        Need(!O::ChaserConnectionPlan(seats,16,pending) && pending==123 &&
             !O::PrepareChaserConnections(seats,16,Connect,&connections,completed) && !connections.calls,
             "Invalid roster produced a partial plan or emitted native callbacks");
    };
    seats[1].state=seats[2].state;invalid();seats[1].state=&states[1];
    seats[1].pawn=seats[2].pawn;invalid();seats[1].pawn=&pawns[1];
    seats[0].connection=0;invalid();seats[0].connection=1;
    seats[0].ai=true;invalid();seats[0].ai=false;
    seats[1].ai=false;invalid();seats[1].ai=true;
    seats[1].connection=4;invalid();seats[1].connection=0;
    connections={};seats[1].connection=3;
    Need(O::PrepareChaserConnections(seats,16,Connect,&connections,completed) && completed==15 && connections.slots==65534,
         "Original AIController status was rejected during standalone pregame preparation");
    seats[1].connection=0;
    for(unsigned status=0;status<8;++status)
        Need(O::ChaserCoreConnection(status)==(status==0 || status==1 || status==3),"Chaser core accepted a disconnected/unknown state");
    Need(!O::ChaserConnectionPlan(nullptr,16,pending) && !O::ChaserConnectionPlan(seats,1,pending) &&
         !O::ChaserConnectionPlan(seats,17,pending),"Invalid native population bound was accepted");
    connections={};connections.fail=3;
    Need(!O::PrepareChaserConnections(seats,16,Connect,&connections,completed) && connections.calls==3 && completed==2 && connections.slots==6,
         "Native callback rejection was ignored or later seats were still modified");
    // Rebuild a fresh observed roster after partial native success. Earlier
    // successful callbacks must not prevent remaining seats being retried.
    seats[1].connection=seats[2].connection=1;seats[4].connection=2;connections={};
    Need(O::PrepareChaserConnections(seats,16,Connect,&connections,completed) && completed==12 &&
         connections.calls==12 && connections.slots==(65534u & ~6u & ~(1u<<4)),
         "Partial Chaser connection failure blocked retry or reconnected completed/disconnected native seats");
    for(auto& seat:seats)seat.connection=1;
    Need(O::PrepareChaserConnections(seats,16,nullptr,nullptr,completed) && !completed,
         "Already-connected native roster required redundant notifications");
    seats[1].connection=0;Need(!O::PrepareChaserConnections(seats,16,nullptr,nullptr,completed),"Missing native setter accepted an unprepared bot");
    auto authority=[&](void* world,bool enabled=true,bool thread=true){return O::ResolveChaserRoleAuthority(world,enabled,thread,NativeMode,OwnsWorld,&nativeValue);};
    Need(authority(&nativeValue)==1 && nativeModeCalls==1 && worldChecks==1,
         "Owned standalone Chaser did not enter the original role authority branch");
    Need(authority(&retainedValue)==0 && nativeModeCalls==2 && worldChecks==2,"Another world received Chaser authority compatibility");
    Need(authority(&nativeValue,false)==0 && authority(&nativeValue,true,false)==0 && authority(nullptr)==0 &&
         nativeModeCalls==5 && worldChecks==2,"Disabled/foreign-thread/null-world queries entered ownership or skipped native forwarding");
    for(int mode:{1,2,3,7,-1}) {
        nativeNetworkMode=mode;unsigned before=nativeModeCalls;
        Need(authority(&nativeValue)==mode && nativeModeCalls==before+1 && worldChecks==2,
             "A native server/client/unknown network mode was changed or ownership logic ran unnecessarily");
    }
    nativeNetworkMode=0;
    Need(O::ResolveChaserRoleAuthority(&nativeValue,true,true,NativeMode,FaultWorld,nullptr)==0,
         "A faulty ownership check escaped or falsely activated native authority");
    Need(O::ResolveChaserRoleAuthority(&nativeValue,true,true,nullptr,OwnsWorld,&nativeValue)==-1 &&
         O::ResolveChaserRoleAuthority(&nativeValue,true,true,NativeMode,nullptr,nullptr)==0,
         "Missing native/ownership interfaces falsely established Chaser authority");
    puts("PASS retained Chaser mode boundary: original lookup forwarded once, exact missing-default/owner/feature scope, profile isolation, sixteen-seat native descriptor and engine-owned lazy class-default lifetime. Synthetic callbacks only; no game or original class constructor executed.");
    puts("PASS Chaser connection preparation: whole-roster validation, local human preserved, only new bots notified, disconnected/duplicate/malformed seats rejected, native callback failure stops later writes. Native random selection and role construction remain external and untested.");
    puts("PASS Chaser role authority adapter: native forwarding once, standalone owned-world gate, server/client preservation, disabled/thread/null/foreign/fault isolation. No native role/collision/infection callback executed.");
    unsigned eventsOriginal=nativeModeCalls,eventsOwned=worldChecks;
    auto events=[&](void* world,bool enabled=true,bool thread=true){return O::ResolveChaserRoleEvents(world,enabled,thread,NativeMode,OwnsWorld,&nativeValue);};
    Need(events(&nativeValue)==3 && nativeModeCalls==eventsOriginal+1 && worldChecks==eventsOwned+1 &&
         authority(&nativeValue)==1 && nativeModeCalls==eventsOriginal+2,
         "Standalone original client-event and authority queries did not retain distinct native branches");
    Need(events(&retainedValue)==0 && events(&nativeValue,false)==0 && events(&nativeValue,true,false)==0 && events(nullptr)==0,
         "Original role-event subscription compatibility escaped feature/thread/world scope");
    for(int mode:{1,2,3,-1,7}) {
        nativeNetworkMode=mode;auto checksBefore=worldChecks,originalBefore=nativeModeCalls;
        Need(events(&nativeValue)==mode && worldChecks==checksBefore && nativeModeCalls==originalBefore+1,
             "Existing native client/server/unknown event mode was changed or forwarded more than once");
    }
    nativeNetworkMode=0;
    Need(O::ResolveChaserRoleEvents(&nativeValue,true,true,NativeMode,FaultWorld,nullptr)==0 &&
         O::ResolveChaserRoleEvents(&nativeValue,true,true,nullptr,OwnsWorld,&nativeValue)==-1 &&
         O::ResolveChaserRoleEvents(&nativeValue,true,true,NativeMode,nullptr,nullptr)==0,
         "Faulty/missing role-event ownership fabricated an original collision subscription");
    puts("PASS original Chaser movement-event gate: independent client-event/authority branches, original query forwarded once, exact standalone feature/thread/world scope and native client/server/fault isolation. Synthetic callbacks only; no original collision/infection executed.");
    auto loading=[&](void* rule,bool enabled=true,bool thread=true){return O::ResolveChaserLoadingReady(rule,enabled,thread,
        NativeTransition,LoadingReady,nullptr);};
    Need(!loading(&nativeValue) && transitionCalls==1 && transitionChecks==1,
         "Owned native Loading completion ignored unprepared local navigation");
    transitionReady=true;Need(loading(&nativeValue) && transitionCalls==2 && transitionChecks==2,
         "Prepared navigation did not release original native Loading completion");
    transitionNative=false;unsigned checks=transitionChecks;
    Need(!loading(&nativeValue) && transitionChecks==checks,"Navigation readiness bypassed original loading/streaming completion");
    transitionNative=true;transitionReady=false;
    Need(loading(&nativeValue,false) && loading(&nativeValue,true,false) && loading(nullptr) && transitionChecks==checks,
         "Disabled/thread/null transition was intercepted or skipped original forwarding");
    transitionOwned=false;Need(loading(&retainedValue),"Other Chaser rules or foreign mode/world were held by local navigation");
    Need(O::ResolveChaserLoadingReady(&nativeValue,true,true,NativeTransition,LoadingFault,nullptr),
         "Unqualified readiness fault changed a foreign original transition result");
    Need(!O::ResolveChaserLoadingReady(&nativeValue,true,true,NativeTransition,LoadingFault,&nativeValue),
         "Qualified readiness fault released an unprepared original Loading rule");
    Need(O::ResolveChaserLoadingReady(&nativeValue,true,true,NativeTransition,nullptr,nullptr) &&
         !O::ResolveChaserLoadingReady(&nativeValue,true,true,nullptr,LoadingReady,nullptr),
         "Missing readiness/native interfaces fabricated a loading completion");
    puts("PASS Chaser pregame readiness: native forwarding once, native-incomplete preservation, exact owned-loading hold/release, disabled/thread/null/foreign isolation and qualified-fault containment. Synthetic callbacks only; no native phase or timer executed.");
    return 0;
}catch(const std::exception& e){fprintf(stderr,"%s\n",e.what());return 1;}}
