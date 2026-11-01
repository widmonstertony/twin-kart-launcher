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
struct Components {BYTE* state=nullptr;BYTE* path=nullptr;BYTE* behavior=nullptr;BYTE* movement=nullptr;bool prepared=false;BYTE* kart=nullptr;BYTE* character=nullptr;BYTE* replication=nullptr;BYTE* progress=nullptr;BYTE* slots=nullptr;};
std::unordered_map<BYTE*,Components> components;
std::unordered_map<void*,BYTE*> worlds;
BYTE *world=nullptr,*otherWorld=nullptr,*major=nullptr,*phase=nullptr,*rule=nullptr,*foreignPhase=nullptr,*foreignRule=nullptr;
BYTE *aiType=nullptr,*flagAiType=nullptr,*stateType=nullptr,*pathType=nullptr,*speedType=nullptr,*itemType=nullptr,*movementType=nullptr;
BYTE *speedTeamType=nullptr,*itemTeamType=nullptr;
BYTE *phaseType=nullptr,*gameState=nullptr,*manager=nullptr,*foreignManager=nullptr;
BYTE *playerWaitType=nullptr,*playerWait=nullptr;
BYTE *majorType=nullptr;
BYTE* progressType=nullptr;
std::vector<std::array<int,2>> observedRanks,observedTimes;
void ObserveFinish(const char* name,int slot,int value) {
    if(!strcmp(name,"native_local_race_finish_rank"))observedRanks.push_back({slot,value});
    if(!strcmp(name,"native_local_race_finish_time_ms"))observedTimes.push_back({slot,value});
}
BYTE* flagTypes[5]{};
BYTE *flagGameModeType=nullptr,*flagConfigType=nullptr;
BYTE *contextType=nullptr,*flagContextType=nullptr;
BYTE *itemSlotType=nullptr,*itemStackType=nullptr,*cubeStacks[2]{};
void* __fastcall ItemSlotType(){return itemSlotType;}void* __fastcall ItemStackType(){return itemStackType;}
int cubeObserved[4]{-1,-1,-1,-1};unsigned cubeNativeCalls=0;
void Check(bool value);
void ObserveCube(const char* name,int slot,int value) {
    Check(slot==1);
    const char* names[]={"native_local_cube_pickup_designated_first","native_local_cube_pickup_designated_second","native_local_cube_pickup_items_before","native_local_cube_pickup_items_after"};
    for(int i=0;i<4;++i)if(!strcmp(name,names[i]))cubeObserved[i]=value;
}
void __fastcall NativeCubeOverlap(void*,void*,void*) {
    ++cubeNativeCalls;for(auto stack:cubeStacks)if(stack)*reinterpret_cast<int*>(stack+0xc8)=1;
}
void* __fastcall ContextType(){return contextType;}
void* __fastcall FlagContextType(){return flagContextType;}
unsigned contextConstructCalls=0;BYTE *seenContextType=nullptr,*seenContextOwner=nullptr;
void* __fastcall ContextConstruct(void* raw) {
    ++contextConstructCalls;auto values=static_cast<BYTE**>(raw);
    seenContextType=values?values[0]:nullptr;seenContextOwner=values?values[1]:nullptr;
    return seenContextType;
}
BYTE* flagZoneType=nullptr;
alignas(8) BYTE flagNotification[0x30]{};
int originalNetMode=0;
void* __fastcall FlagGameModeType(){return flagGameModeType;}
void* __fastcall FlagConfigType(){return flagConfigType;}
void* __fastcall FlagZoneType(){return flagZoneType;}
int __fastcall NativeNetMode(void*){return originalNetMode;}
BYTE* __fastcall NativeFlagConfig(void*){return flagNotification;}
std::unordered_map<BYTE*,std::array<BYTE*,5>> flagComponents;
unsigned flagCreates=0,flagRegisters=0;
unsigned flagCommandCalls=0;
bool delayPlayerWait=false;
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
bool __fastcall RoadQueryFixture(void* queryWorld,void* raw,const TwinKartRace::ArenaPoint* start,const TwinKartRace::ArenaPoint* end,const void* mask,const void* parameters) {
    Check(queryWorld!=nullptr && parameters==&flagCommandCalls && *static_cast<const unsigned*>(mask)==0x20000 && start->z>end->z);
    auto hit=static_cast<unsigned char*>(raw);memset(hit,0xa5,0x88);
    TwinKartRace::ArenaPoint point{start->x,start->y,123};memcpy(hit+0xc,&point,sizeof(point));float normal=1;memcpy(hit+0x38,&normal,4);hit[0x5d]=1;return true;
}
void __fastcall FlagCommand(void*,const void* previous){Check(reinterpret_cast<uintptr_t>(previous)%16==0);++flagCommandCalls;}
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
unsigned contextActorWorldCalls=0;
BYTE* __fastcall ContextActorWorld(void* object){++contextActorWorldCalls;return World(object);}
void* __fastcall AiType(){return aiType;}void* __fastcall StateType(){return stateType;}
void* __fastcall FlagAiType(){return flagAiType;}
void* __fastcall FlagNotifyType(){return flagTypes[0];}
void* __fastcall FlagHolderType(){return flagTypes[1];}
void* __fastcall FlagDropType(){return flagTypes[2];}
void* __fastcall FlagReceiveType(){return flagTypes[3];}
void* __fastcall FlagCarryType(){return flagTypes[4];}
void* __fastcall PathType(){return pathType;}void* __fastcall SpeedType(){return speedType;}
void* __fastcall ItemType(){return itemType;}void* __fastcall MovementType(){return movementType;}
void* __fastcall SpeedTeamType(){return speedTeamType;}void* __fastcall ItemTeamType(){return itemTeamType;}
void* __fastcall PhaseType(){return phaseType;}
void* __fastcall PlayerWaitType(){return playerWaitType;}
void* __fastcall MajorType(){return majorType;}
void* __fastcall ProgressType(){return progressType;}
unsigned originalSkillReadinessCalls=0,baseSkillReadinessCalls=0;
bool __fastcall OriginalSkillReadiness(void*,void*){++originalSkillReadinessCalls;return false;}
bool __fastcall SharedSkillReadiness(void*,void* index){++baseSkillReadinessCalls;static_cast<BYTE*>(index)[4]=0;return true;}
void __fastcall FlagSkillPhaseName(void* output,const wchar_t* name,int){
    *static_cast<unsigned long long*>(output)=!wcscmp(name,L"Game")?101:!wcscmp(name,L"Overtime")?102:!wcscmp(name,L"LastSpurt")?103:0;
}
wchar_t cubeFixtureName[96]=L"Cube_Desert_I06",cubeFixtureStorage[96]{};
unsigned cubeLevelCalls=0,cubeAssignments=0;
TwinKartRace::ReturnString* __fastcall CubeCurrentName(TwinKartRace::ReturnString* output,void*,bool) {
    ++cubeLevelCalls;wcscpy_s(cubeFixtureStorage,cubeFixtureName);int n=static_cast<int>(wcslen(cubeFixtureStorage)+1);
    *output={cubeFixtureStorage,n,96};return output;
}
void* __fastcall CubeAssignName(TwinKartRace::ReturnString* target,const TwinKartRace::ReturnString* source) {
    ++cubeAssignments;wcscpy_s(cubeFixtureStorage,source->data);*target={cubeFixtureStorage,source->count,96};return target;
}
void* __fastcall PresetType(){return presetType;}void* __fastcall KartType(){return kartType;}void* __fastcall CharacterType(){return characterType;}
void* __fastcall AccountType(){return accountType;}void* __fastcall ReplicationType(){return replicationType;}
BYTE* __fastcall Find(void* raw,void* type) {
    auto fc=flagComponents.find(static_cast<BYTE*>(raw));
    if(fc!=flagComponents.end())for(unsigned i=0;i<5;++i)if(type==flagTypes[i])return fc->second[i];
    if(raw==gameState && type==phaseType)return phase;
    if(raw==gameState && type==playerWaitType)return delayPlayerWait?nullptr:playerWait;
    if(raw==gameState && type==majorType)return major;
    auto found=components.find(static_cast<BYTE*>(raw));if(found==components.end())return nullptr;auto& c=found->second;
    if(type==stateType)return c.state;if(type==pathType)return c.path;if(type==movementType)return c.movement;
    if(type==progressType)return c.progress;
    if(type==itemSlotType)return c.slots;
    if(type==kartType)return c.kart;if(type==characterType)return c.character;
    if(type==replicationType)return c.replication;
    if((type==speedType || type==itemType || type==speedTeamType || type==itemTeamType) && c.behavior && *reinterpret_cast<void**>(c.behavior+0x10)==type)return c.behavior;
    return nullptr;
}
void __fastcall FlagParameters(void* raw,void* type){memset(raw,0,0x40);*static_cast<void**>(raw)=type;}
void* __fastcall FlagConstruct(void* raw){
    auto params=static_cast<BYTE**>(raw);++flagCreates;
    auto result=Object(0x300,params[0],nullptr,world);*reinterpret_cast<void**>(result+0x20)=params[1];return result;
}
void __fastcall FlagRegister(void* raw){
    auto object=static_cast<BYTE*>(raw),pawn=*reinterpret_cast<BYTE**>(object+0x20);
    for(unsigned i=0;i<5;++i)if(*reinterpret_cast<void**>(object+0x10)==flagTypes[i]) {
        Check(!flagComponents[pawn][i]);
        if(i==1)Check(object[0x130]==1);
        *reinterpret_cast<void**>(object+0xa0)=pawn;flagComponents[pawn][i]=object;++flagRegisters;return;
    }
    ++errors;
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
    *reinterpret_cast<BYTE**>(world+0x118)=gameState;
    playerWaitType=Object(0x40);playerWait=Object(0xd8,playerWaitType,gameState,world);
    majorType=Object(0x40);*reinterpret_cast<void**>(major+0x10)=majorType;*reinterpret_cast<void**>(major+0xa0)=gameState;
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
    Stub(0x2381ed0,reinterpret_cast<void*>(MajorType));
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
    std::string eventCatalog="TKAI1\n";
    for(int id=1;id<=9;++id)eventCatalog+="C Character:"+std::to_string(id)+" Costume:"+std::to_string(id)+" - - - -\nK Kart:"+std::to_string(id)+" - - - - - - - - -\n";
    auto available=[](const std::wstring&,void*)->int{return 1;};
    Need(R::aiRoster.Load(eventCatalog.data(),eventCatalog.size()) && R::aiRoster.Prepare(7,ownerPreset,available,nullptr),"Event roster fixture preparation");
    const wchar_t* roles[8]{};for(auto& role:roles)role=L"Character:2";
    Need(R::aiRoster.ApplyCharacterRoles(roles,available,nullptr),"Registered event character roster");
    R::nativeCharacterRules=true;unsigned beforeFixed=equipmentCopies;
    R::OnUnknown(major,pawns[0],0,false);
    Need(equipmentCopies==beforeFixed+17 && !wcscmp(observedCharacter.data,L"Character:2") && !wcscmp(observedKart.data,L"SelectedKart"),
        "Native fixed-character rules were replaced by the player's ordinary character preset or disabled kart customization");
    R::nativeCharacterRules=false;
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
    Need(forceBeginCalls==1 && R::localStartRequested,"Circuit local readiness did not use the native idempotent PlayerWait method");
    forceBeginCalls=0;playerWait[0xc8]=playerWait[0xc9]=0;
    R::Arm(104);Need(R::raceWorld.Bind({world,*reinterpret_cast<int*>(world+0xc),*reinterpret_cast<int*>(world+0xc)+1},chunks,static_cast<int>(objects.size())),"Flag local scope fixture");
    *reinterpret_cast<int*>(major+0x13c)=2;
    unsigned flagRegistrations=registrations,flagSpeedFactories=speedFactories,flagItemFactories=itemFactories;
    for(auto& type:flagTypes)type=Object(0x40);
    Stub(0x23a1840,reinterpret_cast<void*>(FlagNotifyType));Stub(0x22ac760,reinterpret_cast<void*>(FlagHolderType));
    Stub(0x22a7030,reinterpret_cast<void*>(FlagDropType));Stub(0x22ac830,reinterpret_cast<void*>(FlagReceiveType));
    Stub(0x22a6400,reinterpret_cast<void*>(FlagCarryType));Stub(0x2771dd0,reinterpret_cast<void*>(FlagParameters));
    Stub(0x3de4900,reinterpret_cast<void*>(FlagRegister));R::construct=FlagConstruct;
    delayPlayerWait=true;
    for(int slot=1;slot<8;++slot) {
        auto pawn=pawns[slot];*reinterpret_cast<void**>(pawn+0x10)=flagAiType;
        auto& c=components[pawn];c.path=nullptr;c.behavior=nullptr;*reinterpret_cast<void**>(c.movement+0xf08)=nullptr;
        R::OnUnknown(major,pawn,slot,false);
        Need(!c.path && !c.behavior && *reinterpret_cast<void**>(c.movement+0xf08)==nullptr,"Flag AI was polluted with circuit FollowTG/behavior");
        Need(*reinterpret_cast<BYTE*>(c.state+0x321)==1 && *reinterpret_cast<int*>(c.state+0x16c)==1+slot%2,"Flag AI classification/team assignment failed");
        Need(forceBeginCalls==0,"Flag player-wait force ran before its component was available");
    }
    Need(!R::RetryLocalStartWhenReady(otherWorld) && !R::RetryLocalStartWhenReady(world) && forceBeginCalls==0,"Flag delayed readiness escaped its world or bypassed missing PlayerWait");
    delayPlayerWait=false;
    Need(R::RetryLocalStartWhenReady(world) && forceBeginCalls==1,"Flag delayed readiness did not retry after native component creation");
    Need(!R::RetryLocalStartWhenReady(world) && forceBeginCalls==1,"Flag delayed readiness repeated a native start");
    R::OnUnknown(major,pawns[7],7,false);
    Need(flagCreates==28 && flagRegisters==28 && !errors,"Flag components missing, duplicated or initialized with wrong holder policy");
    Need(forceBeginCalls==1 && R::localStartRequested && *reinterpret_cast<BYTE*>(playerWait+0xc9)==1,"Flag readiness did not use the original idempotent PlayerWait transition");
    Need(registrations==flagRegistrations,"Flag AI called ordinary circuit registration");
    flagGameModeType=Object(0x40);flagConfigType=Object(0x40);
    Stub(0x22a77b0,reinterpret_cast<void*>(FlagGameModeType));Stub(0x22a6580,reinterpret_cast<void*>(FlagConfigType));
    *reinterpret_cast<void**>(gameState+0x10)=flagGameModeType;R::flagNetMode=NativeNetMode;
    Need(R::LocalFlagPhysicsNetMode(world)==0,"Local projectile selection ran outside its movement initialization scope");
    R::flagPhysicsScope=phase;
    Need(R::LocalFlagPhysicsNetMode(world)==3 && R::LocalFlagPhysicsNetMode(otherWorld)==0,"Local projectile selection escaped its Flag world");
    originalNetMode=2;Need(R::LocalFlagPhysicsNetMode(world)==2,"Flag physics altered a non-standalone world");originalNetMode=0;
    R::aiSkillNetMode=NativeNetMode;
    Need(R::LocalAiSkillNetMode(world)==1 && R::LocalAiSkillNetMode(otherWorld)==0,"Native AI skill timer authority did not stay inside its owned standalone race");
    originalNetMode=2;Need(R::LocalAiSkillNetMode(world)==2,"AI skill timer changed a listen-server mode");originalNetMode=0;
    int wrongThreadSkillMode=-1;std::thread skillThread([&]{wrongThreadSkillMode=R::LocalAiSkillNetMode(world);});skillThread.join();
    Need(wrongThreadSkillMode==0,"AI skill timer authority escaped the game thread");
    R::aiSkillCanUse=OriginalSkillReadiness;R::baseSkillCanUse=SharedSkillReadiness;
    Stub(0x22ac6a0,reinterpret_cast<void*>(PhaseType));Stub(0x2589130,reinterpret_cast<void*>(FlagSkillPhaseName));
    auto flagSkillPhase=Object(0x40,nullptr,nullptr,world);
    *reinterpret_cast<BYTE**>(phase+0x110)=flagSkillPhase;
    BYTE skillIndex[8]{};skillIndex[4]=1;
    R::flagSkillsEnabled=false;
    Need(!R::OnAiCanUseSkill(phase,skillIndex) && originalSkillReadinessCalls==1 && !baseSkillReadinessCalls,"Ordinary Flag readiness was replaced");
    R::flagSkillsEnabled=true;*reinterpret_cast<unsigned long long*>(flagSkillPhase+0x28)=101;
    Need(R::OnAiCanUseSkill(phase,skillIndex) && baseSkillReadinessCalls==1 && !skillIndex[4],"Skill Flag did not use the native shared cooldown/readiness contract during Game");
    *reinterpret_cast<unsigned long long*>(flagSkillPhase+0x28)=102;
    Need(R::OnAiCanUseSkill(phase,skillIndex) && baseSkillReadinessCalls==2,"Skill Flag overtime was blocked by absent circuit race timers");
    *reinterpret_cast<unsigned long long*>(flagSkillPhase+0x28)=999;skillIndex[4]=1;
    Need(!R::OnAiCanUseSkill(phase,skillIndex) && !skillIndex[4] && baseSkillReadinessCalls==2,"Skill Flag permitted skill readiness outside gameplay");
    Need(!R::OnAiCanUseSkill(foreignPhase,skillIndex) && originalSkillReadinessCalls==2,"Skill Flag readiness escaped its bound world");
    R::armedMode=113;Need(!R::OnAiCanUseSkill(phase,skillIndex) && originalSkillReadinessCalls==3,"Circuit AI readiness was replaced");
    R::armedMode=104;R::flagSkillsEnabled=false;*reinterpret_cast<BYTE**>(phase+0x110)=nullptr;
    R::currentLevelName=CubeCurrentName;Stub(0x9118f0,reinterpret_cast<void*>(CubeAssignName));
    TwinKartRace::ReturnString pathName{};R::armedMode=116;wcscpy_s(R::cubeBaseTrack,L"Desert_I06");
    Need(R::CubePathLevelName(&pathName,world,true)==&pathName && !wcscmp(pathName.data,L"Desert_I06") && cubeAssignments==1,"Cube AI path did not retain its explicit base-track resource identity");
    Need(R::CubePathLevelName(&pathName,otherWorld,true)==&pathName && !wcscmp(pathName.data,L"Cube_Desert_I06") && cubeAssignments==1,"Cube AI path alias escaped its serial-bound world");
    wcscpy_s(cubeFixtureName,L"Cube_Forest_I05");R::CubePathLevelName(&pathName,world,true);
    Need(!wcscmp(pathName.data,L"Cube_Forest_I05") && cubeAssignments==1,"Cube path mapping silently normalized another world");
    wcscpy_s(cubeFixtureName,L"Cube_Desert_I06");R::armedMode=4;R::CubePathLevelName(&pathName,world,true);
    Need(!wcscmp(pathName.data,L"Cube_Desert_I06") && cubeAssignments==1 && cubeLevelCalls==4,"Cube path mapping changed ordinary gameplay or failed native forwarding");
    R::armedMode=104;R::cubeBaseTrack[0]=0;
    R::flagPhysicsScope=nullptr;
    flagZoneType=Object(0x40);Stub(0x22add30,reinterpret_cast<void*>(FlagZoneType));
    auto goal=Object(0x290,flagZoneType,nullptr,world);R::flagZoneNetMode=NativeNetMode;
    Need(R::LocalFlagZoneInitialization(goal)==1,"Standalone goal did not request original authority registration");
    worlds[goal]=otherWorld;Need(R::LocalFlagZoneInitialization(goal)==0,"Goal initialization escaped its local Flag world");
    worlds[goal]=world;*reinterpret_cast<void**>(goal+0x10)=aiType;
    Need(R::LocalFlagZoneInitialization(goal)==0,"Goal registration changed a different actor class");
    auto configuration=Object(0xb8,flagConfigType);
    *reinterpret_cast<void**>(flagNotification+0x20)=configuration;R::flagConfig=NativeFlagConfig;
    Need(R::LocalFlagConfig(phase)==flagNotification && configuration[0x30]==1,"Standalone Flag authority overlap did not use native per-world config");
    configuration[0x30]=0;*reinterpret_cast<unsigned*>(configuration+8)=0x10;
    R::LocalFlagConfig(phase);Need(configuration[0x30]==0,"Flag adapter changed class default configuration");
    *reinterpret_cast<unsigned*>(configuration+8)=0;R::LocalFlagConfig(foreignPhase);
    Need(configuration[0x30]==0,"Flag authority config escaped local world");
    Stub(0x191dc50,reinterpret_cast<void*>(FlagCommand));
    BYTE* flagMotion=Object(0x1f0,nullptr,pawns[1],world);
    R::MirrorFlagCommand(flagMotion);R::MirrorFlagCommand(flagMotion);
    Need(flagCommandCalls==1,"Unchanged local flag command repeated replication consumer");
    *reinterpret_cast<int*>(flagMotion+0x180)=1;R::MirrorFlagCommand(flagMotion);
    Need(flagCommandCalls==2,"Changed native flag command was dropped");
    worlds[flagMotion]=otherWorld;*reinterpret_cast<int*>(flagMotion+0x180)=2;R::MirrorFlagCommand(flagMotion);
    Need(flagCommandCalls==2,"Foreign arena received a local flag command");
    worlds[flagMotion]=world;++objectSlots[*reinterpret_cast<int*>(flagMotion+0xc)].serial;
    R::MirrorFlagCommand(flagMotion);Need(flagCommandCalls==3,"Recycled flag identity reused old command state");
    TwinKartRace::ArenaDriver arena;
    auto forward=arena.Step({0,0,0},0,{10000,0,0},0.25f,1);
    Need(forward.throttle==1 && forward.steer==0,"Arena approach does not accelerate along native heading");
    auto turn=arena.Step({100,0,0},0,{100,10000,0},0.25f,1);
    Need(turn.steer>0 && turn.throttle>0 && turn.throttle<1,"Arena turn did not slow and steer toward target");
    bool backed=false;for(int i=0;i<16;++i)if(arena.Step({100,0,0},0,{10000,0,0},0.25f,1).throttle<0)backed=true;
    Need(backed,"Arena collision recovery did not use reverse controls");
    Need(arena.Step({100,0,0},0,{100,0,0},0.25f,1).throttle==0,"Arena target arrival retained throttle");
    arena.Reset();arena.Step({0,0,0},0,{1000,0,0},0.25f,1);
    Need(arena.Step({-300,0,0},0,{1000,0,0},0.25f,1).throttle>0,"Forward speed feedback kept braking into reverse away from target");
    arena.Reset();arena.Step({0,0,0},0,{1000,0,0},0.05f,1);
    auto gradualBrake=arena.Step({40,0,0},0,{1000,0,0},0.05f,1);
    Need(gradualBrake.throttle<0 && gradualBrake.throttle>-0.3f,"Short-segment speed feedback still uses abrupt reverse braking");
    arena.Reset();
    for(int i=0;i<100;++i)Need(arena.Step({i*5.f,0,0},0,{10000,0,0},0.05f,1).throttle>0,
        "Higher-frequency slow forward motion was mistaken for a stalled kart");
    TwinKartRace::FlagGroundNavigation ground;
    TwinKartRace::ArenaPoint roadContact{};
    Need(TwinKartRace::QueryRoad(world,RoadQueryFixture,&flagCommandCalls,{10,20,1000},{10,20,-1000},roadContact) && roadContact.z==123 && !errors,
        "Native complete 0x88-byte road-hit ABI or query parameters were truncated");
    Need(ground.Initialize({0,0,0},{2400,2400,600}),"Ground graph fixture initialization");
    TwinKartRace::ArenaPoint sample{};
    while(ground.Next(sample)) {
        int x=static_cast<int>(sample.x/400),y=static_cast<int>(sample.y/400);
        float z=y>=2 && y<=4?x*100.f:x==6?600.f:0.f;
        ground.Sample(z,!(x==3 && y==3));
    }
    std::vector<TwinKartRace::ArenaPoint> route;
    Need(ground.Path({0,1200,0},{2400,1200,600},route) && route.size()>3,"Ground graph did not find sampled ramp around a collision gap");
    for(size_t i=1;i<route.size();++i)Need(std::fabs(route[i].z-route[i-1].z)<=TwinKartRace::ArenaDistance(route[i],route[i-1])*0.25f+15,"Ground graph jumped a cliff");
    Need(!ground.Path({0,1200,0},{2400,1200,2500},route),"Ground graph silently snapped a missing upper surface to a lower road");
    Need(ground.Initialize({0,0,0},{1600,1600,400}),"Bank/ramp fixture initialization");
    while(ground.Next(sample)) {
        int x=static_cast<int>(sample.x/400),y=static_cast<int>(sample.y/400);
        ground.Sample(y==4?x*100.f:x>=2?400.f:0.f,true);
    }
    Need(ground.Path({0,0,0},{1600,0,400},route),"Gentle bank detour was lost");
    bool usedRamp=false;for(const auto& point:route)if(point.y>=1200)usedRamp=true;
    Need(usedRamp,"Arena path took the steep bank instead of its connected gentle ramp");
    style=nullptr;R::OnResultInit(phase,&resultClass,&playerClass,&pointClass,&provider,&style);
    Need(style==nullptr && speedFactories==flagSpeedFactories && itemFactories==flagItemFactories,"Flag settlement borrowed a speed/item result factory");
    for(int slot=1;slot<8;++slot)*reinterpret_cast<void**>(pawns[slot]+0x10)=aiType;
    unsigned beforeShortRoom=forceBeginCalls;
    R::Arm(102);Need(R::raceWorld.Bind({world,*reinterpret_cast<int*>(world+0xc),*reinterpret_cast<int*>(world+0xc)+1},chunks,static_cast<int>(objects.size())),"Short local room scope fixture");
    *reinterpret_cast<int*>(major+0x138)=3;playerWait[0xc8]=playerWait[0xc9]=0;
    Need(!R::ForceLocalStartWhenReady(major,1,true) && !R::ForceLocalStartWhenReady(major,2,false) &&
        !R::ForceLocalStartWhenReady(major,3,true) && !R::ForceLocalStartWhenReady(major,4,true) &&
        !R::RetryLocalStartWhenReady(otherWorld) && forceBeginCalls==beforeShortRoom,
        "Local three-AI room bypassed a missing driver, invalid seat or world");
    Need(R::ForceLocalStartWhenReady(major,2,true) && forceBeginCalls==beforeShortRoom+1,
        "Local readiness incorrectly required seven AI in a three-AI duo room");
    Need(!R::RetryLocalStartWhenReady(world) && forceBeginCalls==beforeShortRoom+1,
        "Local short-room readiness submitted duplicate native start notices");
    puts("PASS team lifecycle: duo/squad speed/item, balanced human+AI teams, native team behavior and result factories, foreign-world isolation.");
    puts("PASS production equipment registration scope: current account preset/body -> human kart/character/physics hooks, preserved native channel and fallback, unchanged AI/foreign-world defaults, ambiguous/expired preset rejection, thread ownership and registration-lifetime cleanup. Synthetic interfaces only.");
    malformed=false;progressType=Object(0x40);Stub(0x23cc170,reinterpret_cast<void*>(ProgressType));
    for(int slot=0;slot<8;++slot) {
        auto progress=Object(0x240,progressType,pawns[slot],world);components[pawns[slot]].progress=progress;
        *reinterpret_cast<int*>(progress+0x204)=slot;
        *reinterpret_cast<float*>(progress+0x20c)=slot?20.5f+slot:0;
    }
    R::event=ObserveFinish;R::eventDiagnosticsInstalled=true;R::armedMode=116;
    unsigned beforeFinishQueries=queries;R::RecordNativeFinishers(phase);
    Need(observedRanks.size()==8 && observedTimes.size()==8 && observedRanks[7][0]==7 && observedRanks[7][1]==7 && observedTimes[7][1]==27500 && queries==beforeFinishQueries,"Native result diagnostics changed rank/time or called a pawn-list method on an unrelated result component");
    R::RecordNativeFinishers(foreignPhase);Need(observedRanks.size()==8 && queries==beforeFinishQueries,"Native result diagnostics escaped the owned world");
    *reinterpret_cast<int*>(components[pawns[7]].progress+0x204)=99;R::RecordNativeFinishers(phase);
    Need(observedRanks.size()==15 && *reinterpret_cast<int*>(components[pawns[7]].progress+0x204)==99,"Native result diagnostics accepted invalid ranks or rewrote gameplay");
    auto retiredState=components[pawns[6]].state;objectSlots[*reinterpret_cast<int*>(retiredState+0xc)].flags=0x30000000;
    worlds[components[pawns[5]].state]=otherWorld;R::RecordNativeFinishers(phase);
    Need(observedRanks.size()==20 && queries==beforeFinishQueries && frees==queries,"Native result diagnostics accepted a retired/foreign state or allocated a native pawn list");
    worlds[components[pawns[5]].state]=world;objectSlots[*reinterpret_cast<int*>(retiredState+0xc)].flags=0;
    contextType=Object(0x40);flagContextType=Object(0x40);
    Stub(0x22d8460,reinterpret_cast<void*>(ContextType));Stub(0x22acef0,reinterpret_cast<void*>(FlagContextType));
    auto savedConstruct=R::construct;R::construct=ContextConstruct;
    alignas(8) BYTE contextParameters[0x40]{};auto parameterValues=reinterpret_cast<BYTE**>(contextParameters);
    parameterValues[0]=contextType;parameterValues[1]=pawns[1];contextParameters[0x30]=0xa5;
    auto contextPawnTable=pawnTable;contextPawnTable[0x160/8]=reinterpret_cast<void*>(ContextActorWorld);
    *reinterpret_cast<void**>(pawns[1])=contextPawnTable.data();
    R::armedMode=104;*reinterpret_cast<void**>(pawns[1]+0x10)=flagAiType;
    Need(R::OnItemContextConstruct(contextParameters)==flagContextType && seenContextOwner==pawns[1] && parameterValues[0]==contextType && contextParameters[0x30]==0xa5 && contextConstructCalls==1 && contextActorWorldCalls==1,"Flag AI did not use the actor-world ABI/select native context before construction or rewrote caller parameters");
    worlds[pawns[1]]=otherWorld;R::OnItemContextConstruct(contextParameters);
    Need(seenContextType==contextType && contextConstructCalls==2,"Flag context selection escaped its owned world");worlds[pawns[1]]=world;
    R::armedMode=116;R::OnItemContextConstruct(contextParameters);
    Need(seenContextType==contextType && contextConstructCalls==3,"Flag context selection changed a circuit context");
    R::armedMode=104;parameterValues[0]=flagContextType;R::OnItemContextConstruct(contextParameters);
    Need(seenContextType==flagContextType && contextConstructCalls==4,"Existing special context did not forward once");
    parameterValues[0]=contextType;*reinterpret_cast<void**>(pawns[1]+0x10)=aiType;R::OnItemContextConstruct(contextParameters);
    Need(seenContextType==contextType && contextConstructCalls==5,"Flag context selection changed a different pawn class");
    *reinterpret_cast<void**>(pawns[1])=pawnTable.data();
    R::construct=savedConstruct;
    itemSlotType=Object(0x40);itemStackType=Object(0x40);
    Stub(0x22ef370,reinterpret_cast<void*>(ItemSlotType));Stub(0x22ef0d0,reinterpret_cast<void*>(ItemStackType));
    auto slots=Object(0x200,itemSlotType,pawns[1],world);components[pawns[1]].slots=slots;
    struct ItemArray {BYTE** data;int count,capacity;};BYTE* chunkObjects[2]{pawns[1],pawns[1]};
    for(int i=0;i<2;++i){cubeStacks[i]=Object(0x180,itemStackType,pawns[1],world);*reinterpret_cast<ItemArray*>(cubeStacks[i]+0xc0)={chunkObjects,0,2};}
    *reinterpret_cast<ItemArray*>(slots+0x170)={cubeStacks,2,2};
    auto cubeActor=Object(0x550,nullptr,nullptr,world);cubeActor[0x228]=3;cubeActor[0x2c0]=6;
    auto itemContext=Object(0x200,nullptr,pawns[1],world);
    R::event=ObserveCube;R::armedMode=116;R::cubeAuthOverlap=NativeCubeOverlap;
    R::OnCubeAuthOverlap(cubeActor,pawns[1],itemContext);
    Need(cubeNativeCalls==1 && cubeObserved[0]==3 && cubeObserved[1]==6 && cubeObserved[2]==0 && cubeObserved[3]==2,"Cube observer lost native double-item grant values or forwarding");
    for(auto& value:cubeObserved)value=-1;worlds[cubeActor]=otherWorld;R::OnCubeAuthOverlap(cubeActor,pawns[1],itemContext);
    Need(cubeNativeCalls==2 && cubeObserved[0]==-1,"Cube observer escaped its actor world");worlds[cubeActor]=world;
    R::armedMode=4;R::OnCubeAuthOverlap(cubeActor,pawns[1],itemContext);
    Need(cubeNativeCalls==3 && cubeObserved[0]==-1,"Cube observer changed ordinary item pickup");
    R::armedMode=116;*reinterpret_cast<ItemArray*>(slots+0x170)={reinterpret_cast<BYTE**>(1),2,2};
    R::OnCubeAuthOverlap(cubeActor,pawns[1],itemContext);
    Need(cubeNativeCalls==4 && cubeObserved[0]==-1,"Malformed native item array leaked diagnostics or suppressed forwarding");
    R::eventDiagnosticsInstalled=false;R::event=nullptr;
    R::armed=false;before=queries;R::OnActivationBegin(rule);Need(queries==before && !errors,"Unarmed native forwarding changed preparation");
    R::raceWorld.Reset();VirtualFree(fixtureImage,0,MEM_RELEASE);fixtureImage=nullptr;
    puts("PASS production local AI preparation and settlement boundary with original synthetic native interfaces: seven speed/item AI, client-first and async-spline ordering, native forwarding, world isolation, temporary-array cleanup, mode-aware missing-style fallback and preserved result/provider ownership. No game code, driving or fabricated results executed.");return 0;
}catch(const std::exception& e){if(fixtureImage)VirtualFree(fixtureImage,0,MEM_RELEASE);fprintf(stderr,"%s\n",e.what());return 1;}}
