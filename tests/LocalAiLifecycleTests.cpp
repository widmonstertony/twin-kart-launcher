#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <array>
#include <vector>
#include <memory>
#include <unordered_map>
#include <stdexcept>
#include <cstdio>
#include <thread>
#include "../native/LocalRaceRuntime.h"
namespace R=TwinKartLocalRace;
namespace {
BYTE* fixtureImage=nullptr;
std::array<TwinKartCamera::ObjectSlot,512> objectSlots{};
TwinKartCamera::ObjectSlot* chunks[]{objectSlots.data()};
std::vector<std::unique_ptr<BYTE[]>> objects;
std::array<void*,200> pawnTable{};
struct Components {BYTE* state=nullptr;BYTE* path=nullptr;BYTE* behavior=nullptr;BYTE* movement=nullptr;bool prepared=false;BYTE* kart=nullptr;BYTE* character=nullptr;BYTE* replication=nullptr;};
std::unordered_map<BYTE*,Components> components;
std::unordered_map<void*,BYTE*> worlds;
BYTE *world=nullptr,*otherWorld=nullptr,*major=nullptr,*phase=nullptr,*rule=nullptr,*foreignPhase=nullptr,*foreignRule=nullptr;
BYTE *aiType=nullptr,*flagAiType=nullptr,*stateType=nullptr,*pathType=nullptr,*speedType=nullptr,*itemType=nullptr,*movementType=nullptr;
BYTE *speedTeamType=nullptr,*itemTeamType=nullptr;
BYTE *phaseType=nullptr,*gameState=nullptr,*manager=nullptr,*foreignManager=nullptr;
BYTE *playerWaitType=nullptr,*playerWait=nullptr;
BYTE *presetType=nullptr,*kartType=nullptr,*characterType=nullptr;
BYTE *accountType=nullptr,*replicationType=nullptr;
bool exerciseEquipment=false;unsigned equipmentCopies=0,equipmentComparisons=0;
TwinKartEquipment::String observedKart{},observedCharacter{};
TwinKartEquipment::Body selectedBody{},defaultBody{},observedBody{};
unsigned bodyReads=0,bodyCalls=0;int originalChannel=17;const void* observedChannel=nullptr;
std::vector<BYTE*> pawns;
unsigned registrations=0,bindings=0,queries=0,frees=0,managerCalls=0,activationCalls=0,errors=0,duplicateComponents=0;
unsigned autoCalls=0,splineCalls=0;
unsigned forceBeginCalls=0;
unsigned resultCalls=0,speedFactories=0,itemFactories=0;
int speedStyle=1,itemStyle=4,spitemSoloStyle=105,spitemTeamStyle=106;
void *resultProvider=nullptr,*resultStyle=nullptr;
const void *seenResultClass=nullptr,*seenPlayerClass=nullptr,*seenPointClass=nullptr;
bool malformed=false,requireReady=true;
void Check(bool value){if(!value)++errors;}
void Need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
BYTE* Object(size_t size,BYTE* type=nullptr,BYTE* owner=nullptr,BYTE* inWorld=nullptr) {
    Need(objects.size()<objectSlots.size(),"Fixture object registry full");
    auto memory=std::make_unique<BYTE[]>(size);auto object=memory.get();
    int index=static_cast<int>(objects.size());*reinterpret_cast<int*>(object+0xc)=index;
    *reinterpret_cast<void**>(object+0x10)=type;
    if(owner)*reinterpret_cast<void**>(object+0xa0)=owner;
    *reinterpret_cast<void**>(object)=pawnTable.data();
    objectSlots[index].object=object;objectSlots[index].serial=index+1;objects.push_back(std::move(memory));
    if(inWorld)worlds[object]=inWorld;
    *reinterpret_cast<int*>(fixtureImage+0x69c28c4)=static_cast<int>(objects.size());return object;
}
BYTE* __fastcall World(void* object){auto found=worlds.find(object);return found==worlds.end()?nullptr:found->second;}
void* __fastcall AiType(){return aiType;}void* __fastcall StateType(){return stateType;}
void* __fastcall FlagAiType(){return flagAiType;}
void* __fastcall PathType(){return pathType;}void* __fastcall SpeedType(){return speedType;}
void* __fastcall ItemType(){return itemType;}void* __fastcall MovementType(){return movementType;}
void* __fastcall SpeedTeamType(){return speedTeamType;}void* __fastcall ItemTeamType(){return itemTeamType;}
void* __fastcall PhaseType(){return phaseType;}
void* __fastcall PlayerWaitType(){return playerWaitType;}
void* __fastcall PresetType(){return presetType;}void* __fastcall KartType(){return kartType;}void* __fastcall CharacterType(){return characterType;}
void* __fastcall AccountType(){return accountType;}void* __fastcall ReplicationType(){return replicationType;}
BYTE* __fastcall Find(void* raw,void* type) {
    if(raw==gameState && type==phaseType)return phase;
    if(raw==gameState && type==playerWaitType)return playerWait;
    auto found=components.find(static_cast<BYTE*>(raw));if(found==components.end())return nullptr;auto& c=found->second;
    if(type==stateType)return c.state;if(type==pathType)return c.path;if(type==movementType)return c.movement;
    if(type==kartType)return c.kart;if(type==characterType)return c.character;
    if(type==replicationType)return c.replication;
    if((type==speedType || type==itemType || type==speedTeamType || type==itemTeamType) && c.behavior && *reinterpret_cast<void**>(c.behavior+0x10)==type)return c.behavior;
    return nullptr;
}
void __fastcall Register(void* raw) {
    auto pawn=static_cast<BYTE*>(raw)-0x320;auto& c=components[pawn];Check(c.prepared);++registrations;
    if(!R::Live(c.path))c.path=Object(0x150,pathType,pawn,world);else Check(*reinterpret_cast<void**>(c.path+0xa0)==pawn);
    if(!c.movement)c.movement=Object(0x1000,movementType,pawn,world);else Check(*reinterpret_cast<void**>(c.movement+0xa0)==pawn);
    bool speed=*reinterpret_cast<BYTE*>(pawn+0x328)!=0;
    bool team=*reinterpret_cast<int*>(major+0x13c)>0;
    if(!R::Live(c.behavior))c.behavior=Object(0x200,speed?(team?speedTeamType:speedType):(team?itemTeamType:itemType),pawn,world);else Check(*reinterpret_cast<void**>(c.behavior+0xa0)==pawn);
}
void __fastcall Bind(void* raw,void* path) {++bindings;*reinterpret_cast<void**>(static_cast<BYTE*>(raw)+0xf08)=path;}
void* __fastcall EquipmentCopy(void* target,const void* source){++equipmentCopies;memcpy(target,source,sizeof(TwinKartEquipment::String));return target;}
bool __fastcall EquipmentEqual(const void*,const void*){++equipmentComparisons;return false;}
void* __fastcall CurrentBody(void*,void* out){++bodyReads;memcpy(out,&selectedBody,sizeof(selectedBody));return out;}
void __fastcall BodySetter(void*,const void* body,const void* channel){++bodyCalls;observedBody=*static_cast<const TwinKartEquipment::Body*>(body);observedChannel=channel;}
void __fastcall AddUnknown(void* rawRule,void* raw,int slot,bool) {
    auto& c=components[static_cast<BYTE*>(raw)];c.prepared=true;
    if(rawRule==major && *reinterpret_cast<int*>(major+0x13c)>0)*reinterpret_cast<int*>(c.state+0x16c)=slot?2:1;
    if(exerciseEquipment && c.kart && c.character) {
        alignas(8) BYTE kart[0xb8]{},character[0x60]{};
        R::OnKartDescriptorEqual(c.kart+0x10b0,kart);R::OnCharacterDescriptorEqual(c.character+0x1020,character);
        if(c.replication)R::OnKartBody(c.replication,&defaultBody,&originalChannel);
        observedKart=*reinterpret_cast<TwinKartEquipment::String*>(kart);observedCharacter=*reinterpret_cast<TwinKartEquipment::String*>(character);
    }
}
void __fastcall AutoDrive(void* raw,bool enabled) {
    ++autoCalls;auto movement=static_cast<BYTE*>(raw);*reinterpret_cast<BYTE*>(movement+0xedd)=enabled?1:0;
    if(!enabled)*reinterpret_cast<void**>(movement+0xf08)=nullptr;
}
struct NativeArray {BYTE** data;int count,capacity;};
void __fastcall GetPawns(void*,NativeArray* output) {
    ++queries;output->count=malformed?9:static_cast<int>(pawns.size());output->capacity=output->count;
    output->data=static_cast<BYTE**>(HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,output->count*sizeof(void*)));
    Check(output->data!=nullptr);if(output->data && !malformed)memcpy(output->data,pawns.data(),pawns.size()*sizeof(void*));
}
void __fastcall Free(void* memory){++frees;Check(HeapFree(GetProcessHeap(),0,memory)!=FALSE);}
void __fastcall ForceBegin(void* raw){++forceBeginCalls;*reinterpret_cast<BYTE*>(static_cast<BYTE*>(raw)+0xc9)=1;}
void Ready() {
    if(!requireReady)return;
    Check(pawns.size()==8);
    for(size_t i=1;i<pawns.size();++i) {
        auto& c=components[pawns[i]];Check(c.prepared && c.path && c.behavior && c.movement);
        Check(c.path && *reinterpret_cast<void**>(c.movement+0xf08)==c.path);
        Check(*reinterpret_cast<BYTE*>(c.state+0x321)==1);
        Check(c.behavior && *reinterpret_cast<void**>(c.behavior+0x10)==(R::armedMode==1?speedType:itemType));
    }
}
void __fastcall Manager(void*){++managerCalls;Ready();}
void __fastcall Activate(void*){++activationCalls;Check(managerCalls>0);Ready();}
void __fastcall Spline(void*){++splineCalls;Check(managerCalls>0);Ready();}
void* __fastcall SpeedStyle(const void*,void** output){++speedFactories;*output=&speedStyle;return output;}
void* __fastcall ItemStyle(const void*,void** output){++itemFactories;*output=&itemStyle;return output;}
void* __fastcall SpitemSoloStyle(const void*,void** output){*output=&spitemSoloStyle;return output;}
void* __fastcall SpitemTeamStyle(const void*,void** output){*output=&spitemTeamStyle;return output;}
void __fastcall Result(void*,const void* resultClass,const void* playerClass,const void* pointClass,void** provider,void** styler) {
    ++resultCalls;resultProvider=provider?*provider:nullptr;resultStyle=styler?*styler:nullptr;
    seenResultClass=resultClass;seenPlayerClass=playerClass;seenPointClass=pointClass;
}
void Stub(unsigned rva,void* target) {
    auto page=reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(fixtureImage+rva)&~uintptr_t(4095));
    Need(VirtualAlloc(page,4096,MEM_COMMIT,PAGE_READWRITE)==page,"Fixture API page allocation");
    DWORD old=0;Need(VirtualProtect(page,4096,PAGE_READWRITE,&old)!=FALSE,"Fixture shared-page write protection");
    BYTE jump[14]{0xff,0x25,0,0,0,0};memcpy(jump+6,&target,8);memcpy(fixtureImage+rva,jump,sizeof(jump));
    DWORD previous=0;Need(VirtualProtect(page,4096,PAGE_EXECUTE_READ,&previous)!=FALSE,"Fixture API protection");
    FlushInstructionCache(GetCurrentProcess(),fixtureImage+rva,sizeof(jump));
}
}
int main(){try{
    fixtureImage=static_cast<BYTE*>(VirtualAlloc(nullptr,0x6a00000,MEM_RESERVE,PAGE_NOACCESS));Need(fixtureImage!=nullptr,"Fixture interface address space");
    auto globals=fixtureImage+0x69c2000;Need(VirtualAlloc(globals,4096,MEM_COMMIT,PAGE_READWRITE)==globals,"Fixture registry globals");
    *reinterpret_cast<TwinKartCamera::ObjectSlot***>(fixtureImage+0x69c28b0)=chunks;
    pawnTable[0x620/8]=reinterpret_cast<void*>(Find);
    pawnTable[0x160/8]=reinterpret_cast<void*>(World);
    world=Object(0x200);otherWorld=Object(0x200);
    aiType=Object(0x40);flagAiType=Object(0x40);stateType=Object(0x40);pathType=Object(0x40);speedType=Object(0x40);itemType=Object(0x40);movementType=Object(0x40);
    major=Object(0x200,nullptr,nullptr,world);*reinterpret_cast<void**>(major)=fixtureImage+0x51dd410;
    *reinterpret_cast<int*>(major+0x130)=1;*reinterpret_cast<int*>(major+0x138)=7;
    phaseType=Object(0x40);gameState=Object(0x200,nullptr,nullptr,world);
    *reinterpret_cast<BYTE**>(world+0x120)=gameState;
    playerWaitType=Object(0x40);playerWait=Object(0xd8,playerWaitType,gameState,world);
    manager=Object(0x200,nullptr,gameState,world);foreignManager=Object(0x200,nullptr,gameState,otherWorld);
    phase=Object(0x200,phaseType,gameState,world);rule=Object(0x80);*reinterpret_cast<void**>(rule+0x28)=phase;
    foreignPhase=Object(0x200,nullptr,nullptr,otherWorld);foreignRule=Object(0x80);*reinterpret_cast<void**>(foreignRule+0x28)=foreignPhase;
    for(int i=0;i<8;++i) {
        BYTE* pawn=Object(0x400,i?aiType:nullptr,nullptr,world);pawns.push_back(pawn);
        auto& c=components[pawn];c.state=Object(0x400,stateType,pawn,world);c.movement=Object(0x1000,movementType,pawn,world);
        *reinterpret_cast<int*>(c.state+0x168)=i;
    }
    Stub(0x3ddf4b0,reinterpret_cast<void*>(World));Stub(0x23bd880,reinterpret_cast<void*>(AiType));Stub(0x2413310,reinterpret_cast<void*>(FlagAiType));Stub(0x23afeb0,reinterpret_cast<void*>(StateType));
    Stub(0x22b1e10,reinterpret_cast<void*>(PathType));Stub(0x2218050,reinterpret_cast<void*>(SpeedType));Stub(0x220fed0,reinterpret_cast<void*>(ItemType));
    Stub(0x22f9030,reinterpret_cast<void*>(MovementType));Stub(0x1973d10,reinterpret_cast<void*>(Register));Stub(0x15d1100,reinterpret_cast<void*>(Bind));
    Stub(0x1971550,reinterpret_cast<void*>(GetPawns));Stub(0x248b950,reinterpret_cast<void*>(Free));
    Stub(TwinKartGraphics::RacePhaseComponentGetter,reinterpret_cast<void*>(PhaseType));
    Stub(0x23b4a90,reinterpret_cast<void*>(PlayerWaitType));Stub(0x1638220,reinterpret_cast<void*>(ForceBegin));
    Stub(0x168da80,reinterpret_cast<void*>(SpeedStyle));Stub(0x168db20,reinterpret_cast<void*>(ItemStyle));
    R::image=fixtureImage;R::owner=GetCurrentThreadId();R::addUnknown=AddUnknown;R::managerBegin=Manager;R::activationBegin=Activate;R::splineBegin=Spline;R::autoDrive=AutoDrive;R::Arm(1);
    Need(!R::Live(reinterpret_cast<BYTE*>(1)),"Expired native component address escaped liveness guard");
    int index=*reinterpret_cast<int*>(world+0xc);Need(R::raceWorld.Bind({world,index,index+1},chunks,static_cast<int>(objects.size())),"Native local scope fixture");
    for(int i=0;i<8;++i)R::OnUnknown(major,pawns[i],i,false);
    Need(registrations==7 && bindings==7 && !components[pawns[0]].path,"Client-before-server setup, seven AI or human exclusion failed");
    auto first=components[pawns[1]].movement;auto firstDriver=components[pawns[1]].path;
    R::OnAutoDrive(first,true);Need(autoCalls==1 && *reinterpret_cast<BYTE*>(first+0xedd)==1 && *reinterpret_cast<void**>(first+0xf08)==firstDriver,"AI native drive-enable request was swallowed");
    R::OnAutoDrive(first,false);Need(autoCalls==1 && *reinterpret_cast<BYTE*>(first+0xedd)==0 && *reinterpret_cast<void**>(first+0xf08)==firstDriver,"Native disable destroyed AI path or retained stale control flag");
    R::OnManagerBegin(rule);R::OnActivationBegin(rule);
    Need(managerCalls==1 && activationCalls==1 && registrations==7 && queries==2 && frees==2 && !errors && !duplicateComponents,"Native AI pre-phase preparation/forwarding failed");
    auto movement=components[pawns[7]].movement;
    components[pawns[7]].path=nullptr;components[pawns[7]].behavior=nullptr;*reinterpret_cast<void**>(movement+0xf08)=nullptr;
    R::OnSplineBegin(manager);
    Need(registrations==8 && splineCalls==1 && queries==3 && frees==3 && !errors && components[pawns[7]].movement==movement,
        "Async native initial spline consumer ran before late AI preparation");
    components[pawns[7]].path=nullptr;components[pawns[7]].behavior=nullptr;*reinterpret_cast<void**>(movement+0xf08)=nullptr;
    R::OnActivationBegin(rule);
    Need(components[pawns[7]].movement==movement && registrations==9 && activationCalls==2 && !errors,"Late AI setup missed native activation or duplicated movement");
    for(size_t i=1;i<pawns.size();++i) {auto& c=components[pawns[i]];c.path=nullptr;c.behavior=nullptr;*reinterpret_cast<void**>(c.movement+0xf08)=nullptr;}
    R::armedMode=4;R::OnManagerBegin(rule);R::OnActivationBegin(rule);
    Need(registrations==16 && !errors,"Native solo item behavior preparation failed");
    unsigned before=queries;requireReady=false;R::OnManagerBegin(foreignRule);Need(queries==before,"AI preparation escaped local world");
    R::OnSplineBegin(foreignManager);Need(queries==before && splineCalls==2,"Async AI preparation escaped local world or skipped native forwarding");
    malformed=true;R::OnManagerBegin(rule);Need(frees==queries,"Malformed native array was leaked");
    // Exercise the production settlement boundary with owned synthetic
    // factories. The game remains the sole source of result objects/data;
    // only a missing local solo presentation style may be supplied.
    R::resultInit=Result;int providerStorage=7,resultClass=8,playerClass=9,pointClass=10;
    void* provider=&providerStorage;void* style=nullptr;R::armedMode=1;
    R::OnResultInit(phase,&resultClass,&playerClass,&pointClass,&provider,&style);
    Need(resultCalls==1 && speedFactories==1 && itemFactories==0 && resultStyle==&speedStyle && resultProvider==&providerStorage &&
        seenResultClass==&resultClass && seenPlayerClass==&playerClass && seenPointClass==&pointClass,"Local speed settlement altered native provider/result interfaces");
    R::armedMode=4;style=nullptr;R::OnResultInit(phase,&resultClass,&playerClass,&pointClass,&provider,&style);
    Need(resultCalls==2 && itemFactories==1 && resultStyle==&itemStyle && resultProvider==&providerStorage,"Local item settlement failed mode-aware style fallback");
    style=&speedStyle;R::OnResultInit(phase,&resultClass,&playerClass,&pointClass,&provider,&style);
    Need(resultCalls==3 && itemFactories==1 && resultStyle==&speedStyle,"Existing native settlement style was replaced");
    style=nullptr;R::OnResultInit(foreignPhase,&resultClass,&playerClass,&pointClass,&provider,&style);
    Need(resultCalls==4 && itemFactories==1 && resultStyle==nullptr,"Settlement fallback escaped bound local world");
    R::OnResultInit(phase,&resultClass,&playerClass,&pointClass,&provider,nullptr);
    Need(resultCalls==5 && itemFactories==1 && resultProvider==&providerStorage,"Absent native style storage was dereferenced or provider replaced");
    auto& stale=components[pawns[2]];auto oldPath=stale.path;auto oldBehavior=stale.behavior;auto oldMovement=stale.movement;
    *reinterpret_cast<unsigned*>(oldPath+8)|=0x10000;*reinterpret_cast<unsigned*>(oldBehavior+8)|=0x10000;
    unsigned oldRegistrations=registrations;R::EnsureAiDriver(pawns[2],2);
    Need(registrations==oldRegistrations+1 && stale.path!=oldPath && stale.behavior!=oldBehavior && stale.movement==oldMovement &&
        *reinterpret_cast<void**>(oldMovement+0xf08)==stale.path && !errors,"Pending-destruction native AI components blocked recovery or retained dead driver");
    // Exercise actual production registration -> preset scan -> descriptor
    // hooks, independently of proprietary code. Only one local human receives
    // the selected equipment; defaults continue through both native compares.
    presetType=Object(0x40);kartType=Object(0x40);characterType=Object(0x40);
    accountType=Object(0x40);replicationType=Object(0x40);
    auto account=Object(0x1900,accountType);auto preset=Object(0x148,presetType);
    // Observed PC layout: UObject Outer is the account; +28 is its embedded
    // interface pointer and deliberately must not pass UObject liveness.
    *reinterpret_cast<void**>(preset+0x20)=account;
    *reinterpret_cast<void**>(preset+0x28)=account+0x28;
    using NativeString=TwinKartEquipment::String;
    auto text=[](const wchar_t* value)->NativeString{int n=static_cast<int>(wcslen(value)+1);return {value,n,n};};
    *reinterpret_cast<NativeString*>(account+0x988)=text(L"FixtureEncodedBody");
    float mass=1,defaultMass=2,force=321;memcpy(selectedBody.bytes,&mass,4);memcpy(selectedBody.bytes+4,&force,4);memcpy(defaultBody.bytes,&defaultMass,4);
    struct Pair {NativeString key,value;int next=-1,hash=0;};static_assert(sizeof(Pair)==40,"Fixture native sparse pair");
    Pair equipped[]{{text(L"Character"),text(L"SelectedCharacter")},{text(L"Costume"),text(L"SelectedCostume")},{text(L"Kart"),text(L"SelectedKart")}};
    auto map=preset+0x98;*reinterpret_cast<Pair**>(map)=equipped;
    *reinterpret_cast<int*>(map+8)=3;*reinterpret_cast<int*>(map+12)=3;*reinterpret_cast<unsigned*>(map+0x10)=7;
    *reinterpret_cast<int*>(map+0x28)=3;*reinterpret_cast<int*>(map+0x2c)=128;
    for(unsigned slot=0;slot<2;++slot){auto& c=components[pawns[slot]];c.kart=Object(0x1200,kartType,pawns[slot],world);c.character=Object(0x1100,characterType,pawns[slot],world);c.replication=Object(0x400,replicationType,pawns[slot],world);}
    Stub(0x220fc90,reinterpret_cast<void*>(PresetType));Stub(0x22fef20,reinterpret_cast<void*>(KartType));Stub(0x2251540,reinterpret_cast<void*>(CharacterType));
    Stub(0x923880,reinterpret_cast<void*>(EquipmentCopy));R::kartEqual=EquipmentEqual;R::characterEqual=EquipmentEqual;
    Stub(0x22ff320,reinterpret_cast<void*>(AccountType));Stub(0x22ff4a0,reinterpret_cast<void*>(ReplicationType));Stub(0x1b4cfd0,reinterpret_cast<void*>(CurrentBody));R::setBody=BodySetter;
    TwinKartEquipment::Preset ownerPreset;BYTE* readAccount=nullptr;
    Need(R::ReadEquipmentPreset(ownerPreset,&readAccount) && readAccount==account && !wcscmp(ownerPreset.kart[0].data,L"SelectedKart"),
        "Observed PC Outer/account layout rejected the selected equipment");
    *reinterpret_cast<void**>(preset+0x20)=world;readAccount=nullptr;
    Need(!R::ReadEquipmentPreset(ownerPreset,&readAccount) && !readAccount,"Non-account Outer accepted as selected equipment owner");
    *reinterpret_cast<void**>(preset+0x20)=account+0x28;
    Need(!R::ReadEquipmentPreset(ownerPreset),"Embedded account interface accepted as a UObject owner");
    *reinterpret_cast<void**>(preset+0x20)=account;
    exerciseEquipment=true;R::OnUnknown(major,pawns[0],0,false);
    Need(equipmentCopies==17 && equipmentComparisons==2 && !wcscmp(observedKart.data,L"SelectedKart") &&
        !wcscmp(observedCharacter.data,L"SelectedCharacter") && !R::equipmentReady && !R::equipmentPawn,"Human equipment missed native descriptor boundary or outlived registration");
    Need(bodyReads==1 && bodyCalls==1 && !memcmp(&observedBody,&selectedBody,sizeof(selectedBody)) && observedChannel==&originalChannel && !R::equipmentBodyReady,
        "Current account tuning missed human body initialization, replaced native channel or escaped registration lifetime");
    R::OnUnknown(major,pawns[1],1,false);Need(equipmentCopies==17 && equipmentComparisons==4 && !observedKart.count,"AI default equipment was overwritten");
    Need(bodyReads==1 && !memcmp(&observedBody,&defaultBody,sizeof(defaultBody)),"AI default physics was overwritten with human upgrades");
    R::OnUnknown(foreignRule,pawns[0],0,false);Need(equipmentCopies==17 && !observedKart.count,"Equipment escaped the local world scope");
    auto duplicatePreset=Object(0x148,presetType);*reinterpret_cast<void**>(duplicatePreset+0x20)=account;*reinterpret_cast<void**>(duplicatePreset+0x28)=account+0x28;
    R::OnUnknown(major,pawns[0],0,false);Need(equipmentCopies==17 && !observedKart.count,"Ambiguous local preset was selected silently");
    *reinterpret_cast<unsigned*>(duplicatePreset+8)=0x10000;
    R::OnUnknown(major,pawns[0],0,false);Need(equipmentCopies==34,"Expired alternate preset blocked the valid current account");
    auto savedEncoded=*reinterpret_cast<NativeString*>(account+0x988);*reinterpret_cast<NativeString*>(account+0x988)={};
    auto readsBefore=bodyReads;R::OnUnknown(major,pawns[0],0,false);
    Need(bodyReads==readsBefore && !memcmp(&observedBody,&defaultBody,sizeof(defaultBody)),"Missing account physics input was claimed as decoded or invalidated native fallback");
    *reinterpret_cast<NativeString*>(account+0x988)=savedEncoded;
    R::equipmentPawn=pawns[0];R::equipmentReady=R::ReadEquipmentPreset(R::equipmentPreset);
    std::thread wrongOwner([&]{alignas(8) BYTE desired[0xb8]{};R::OnKartDescriptorEqual(components[pawns[0]].kart+0x10b0,desired);});wrongOwner.join();
    Need(equipmentCopies==51,"Non-game thread applied borrowed account equipment");
    R::equipmentPawn=nullptr;R::equipmentReady=false;exerciseEquipment=false;
    speedTeamType=Object(0x40);itemTeamType=Object(0x40);
    Stub(0x2218390,reinterpret_cast<void*>(SpeedTeamType));Stub(0x22174e0,reinterpret_cast<void*>(ItemTeamType));
    Stub(0x168da30,reinterpret_cast<void*>(SpeedStyle));Stub(0x168dad0,reinterpret_cast<void*>(ItemStyle));
    Stub(0x168dbc0,reinterpret_cast<void*>(SpitemSoloStyle));Stub(0x168db70,reinterpret_cast<void*>(SpitemTeamStyle));
    for(int mode:{2,3,5,6,106,107,102,103}) {
        R::armedMode=mode;int teams=TwinKartRace::TeamCount(mode,8);
        *reinterpret_cast<int*>(major+0x13c)=teams;
        for(int slot=0;slot<8;++slot) {
            auto& c=components[pawns[slot]];c.behavior=nullptr;
            R::OnUnknown(major,pawns[slot],slot,false);
            Need(*reinterpret_cast<int*>(c.state+0x16c)==1+slot%teams,"Native all-AI-opposition fallback was not balanced");
            if(slot)Need(c.behavior && *reinterpret_cast<void**>(c.behavior+0x10)==(TwinKartRace::SpeedMode(mode)?speedTeamType:itemTeamType),"Team race selected solo AI behavior");
        }
        style=nullptr;R::OnResultInit(phase,&resultClass,&playerClass,&pointClass,&provider,&style);
        Need(style==(TwinKartRace::SpitemMode(mode)?&spitemTeamStyle:TwinKartRace::SpeedMode(mode)?&speedStyle:&itemStyle) && resultProvider==&providerStorage,"Team settlement factory or provider ownership failed");
        *reinterpret_cast<int*>(components[pawns[1]].state+0x16c)=99;
        R::OnUnknown(foreignRule,pawns[1],1,false);
        Need(*reinterpret_cast<int*>(components[pawns[1]].state+0x16c)==99,"Team assignment escaped local world");
    }
    *reinterpret_cast<int*>(major+0x13c)=0;
    R::armedMode=105;
    for(int slot=1;slot<8;++slot) {
        auto& c=components[pawns[slot]];c.behavior=nullptr;R::OnUnknown(major,pawns[slot],slot,false);
        Need(c.behavior && *reinterpret_cast<void**>(c.behavior+0x10)==itemType,"Spitem solo AI behavior missing");
    }
    style=nullptr;R::OnResultInit(phase,&resultClass,&playerClass,&pointClass,&provider,&style);
    Need(style==&spitemSoloStyle && resultProvider==&providerStorage,"Spitem solo used ordinary item settlement");
    // Flag uses its distinct original pawn/rules/result path. It must receive
    // balanced native PawnState metadata without creating a circuit FollowTG
    // or borrowing a speed/item result factory.
    R::Arm(104);Need(R::raceWorld.Bind({world,*reinterpret_cast<int*>(world+0xc),*reinterpret_cast<int*>(world+0xc)+1},chunks,static_cast<int>(objects.size())),"Flag local scope fixture");
    *reinterpret_cast<int*>(major+0x13c)=2;
    unsigned flagRegistrations=registrations,flagSpeedFactories=speedFactories,flagItemFactories=itemFactories;
    for(int slot=1;slot<8;++slot) {
        auto pawn=pawns[slot];*reinterpret_cast<void**>(pawn+0x10)=flagAiType;
        auto& c=components[pawn];c.path=nullptr;c.behavior=nullptr;*reinterpret_cast<void**>(c.movement+0xf08)=nullptr;
        R::OnUnknown(major,pawn,slot,false);
        Need(!c.path && !c.behavior && *reinterpret_cast<void**>(c.movement+0xf08)==nullptr,"Flag AI was polluted with circuit FollowTG/behavior");
        Need(*reinterpret_cast<BYTE*>(c.state+0x321)==1 && *reinterpret_cast<int*>(c.state+0x16c)==1+slot%2,"Flag AI classification/team assignment failed");
        Need(forceBeginCalls==(slot==7?1u:0u),"Flag player-wait force ran before all seven original AI were prepared");
    }
    R::OnUnknown(major,pawns[7],7,false);
    Need(forceBeginCalls==1 && R::flagStartRequested && *reinterpret_cast<BYTE*>(playerWait+0xc9)==1,"Flag readiness did not use the original idempotent PlayerWait transition");
    Need(registrations==flagRegistrations,"Flag AI called ordinary circuit registration");
    style=nullptr;R::OnResultInit(phase,&resultClass,&playerClass,&pointClass,&provider,&style);
    Need(style==nullptr && speedFactories==flagSpeedFactories && itemFactories==flagItemFactories,"Flag settlement borrowed a speed/item result factory");
    for(int slot=1;slot<8;++slot)*reinterpret_cast<void**>(pawns[slot]+0x10)=aiType;
    puts("PASS team lifecycle: duo/squad speed/item, balanced human+AI teams, native team behavior and result factories, foreign-world isolation.");
    puts("PASS production equipment registration scope: current account preset/body -> human kart/character/physics hooks, preserved native channel and fallback, unchanged AI/foreign-world defaults, ambiguous/expired preset rejection, thread ownership and registration-lifetime cleanup. Synthetic interfaces only.");
    R::armed=false;before=queries;R::OnActivationBegin(rule);Need(queries==before && !errors,"Unarmed native forwarding changed preparation");
    R::raceWorld.Reset();VirtualFree(fixtureImage,0,MEM_RELEASE);fixtureImage=nullptr;
    puts("PASS production local AI preparation and settlement boundary with original synthetic native interfaces: seven speed/item AI, client-first and async-spline ordering, native forwarding, world isolation, temporary-array cleanup, mode-aware missing-style fallback and preserved result/provider ownership. No game code, driving or fabricated results executed.");return 0;
}catch(const std::exception& e){if(fixtureImage)VirtualFree(fixtureImage,0,MEM_RELEASE);fprintf(stderr,"%s\n",e.what());return 1;}}
