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
void Check(bool value);
void CheckForwardRecovery() {
    using D=R::ForwardRecovery;
    Check(R::RecoverForwardDecision(true,true,false,0.f)==D::Press);
    Check(R::RecoverForwardDecision(true,true,false,1.f)==D::None);
    Check(R::RecoverForwardDecision(true,true,false,-1.f)==D::None);
    Check(R::RecoverForwardDecision(false,true,false,0.f)==D::None);
    Check(R::RecoverForwardDecision(true,false,true,1.f)==D::Release);
    Check(R::RecoverForwardDecision(false,true,true,1.f)==D::Release);
    Check(R::RecoverForwardDecision(true,false,false,1.f)==D::None);
    for(int mode:{101,102,103})Check(R::NeedsInfiniteBooster(mode,true,true,0,0));
    for(int mode:{1,4,104,108,109,112})Check(!R::NeedsInfiniteBooster(mode,true,true,0,0));
    Check(!R::NeedsInfiniteBooster(103,false,true,0,0));
    Check(!R::NeedsInfiniteBooster(103,true,false,0,0));
    Check(!R::NeedsInfiniteBooster(103,true,true,-1,0));
    Check(!R::NeedsInfiniteBooster(103,true,true,1,0));
    Check(!R::NeedsInfiniteBooster(103,true,true,0,4));
    Check(R::ForwardControlChordAllowed(103,true,false));
    Check(!R::ForwardControlChordAllowed(103,false,true));
    Check(!R::ForwardControlChordAllowed(103,true,true));
    Check(R::ForwardControlChordAllowed(1,true,false));
    Check(R::ForwardControlChordAllowed(107,true,false));
    Check(R::ForwardControlChordAllowed(112,true,false));
    Check(!R::ForwardControlChordAllowed(108,true,false));
    Check(R::ForwardControlChordAllowed(1,false,false));
}
BYTE* fixtureImage=nullptr;
std::array<TwinKartCamera::ObjectSlot,512> objectSlots{};
TwinKartCamera::ObjectSlot* chunks[]{objectSlots.data()};
std::vector<std::unique_ptr<BYTE[]>> objects;
std::array<void*,200> pawnTable{};
struct Components {BYTE* state=nullptr;BYTE* path=nullptr;BYTE* behavior=nullptr;BYTE* movement=nullptr;bool prepared=false;BYTE* kart=nullptr;BYTE* character=nullptr;BYTE* replication=nullptr;BYTE* progress=nullptr;BYTE* slots=nullptr;BYTE* animation=nullptr;BYTE* manager=nullptr;BYTE* simulation=nullptr;BYTE* kartEvents=nullptr;};
std::unordered_map<BYTE*,Components> components;
std::unordered_map<void*,BYTE*> worlds;
BYTE *world=nullptr,*otherWorld=nullptr,*major=nullptr,*phase=nullptr,*rule=nullptr,*foreignPhase=nullptr,*foreignRule=nullptr;
BYTE *aiType=nullptr,*flagAiType=nullptr,*stateType=nullptr,*pathType=nullptr,*speedType=nullptr,*itemType=nullptr,*movementType=nullptr;
BYTE *speedTeamType=nullptr,*itemTeamType=nullptr;
BYTE *phaseType=nullptr,*gameState=nullptr,*manager=nullptr,*foreignManager=nullptr;
BYTE *drivingPhaseType=nullptr,*movementInterfaceType=nullptr;
unsigned humanMovementEnables=0;
bool humanBodySim=false,humanBodyExists=false,humanTick=true;
bool __fastcall HumanSimulating(void*,unsigned long long name){Check(name==0);return humanBodySim;}
void* __fastcall HumanRootBody(void* root,unsigned long long name,bool welded){Check(name==0 && welded);return humanBodyExists?root:nullptr;}
bool __fastcall HumanTickEnabled(void*){return humanTick;}
void __fastcall HumanEnableTick(void*,bool enable){Check(enable);humanTick=true;}
void* __fastcall DrivingPhaseType(){return drivingPhaseType;}
void* __fastcall MovementInterfaceType(){return movementInterfaceType;}
BYTE* __fastcall MovementInterfaceQuery(void* raw,void* type) {
    Check(type==movementInterfaceType);return static_cast<BYTE*>(raw)+0x140;
}
void __fastcall EnableHumanMovement(void* api,bool enabled) {
    Check(enabled);++humanMovementEnables;*reinterpret_cast<int*>(static_cast<BYTE*>(api)+0xc0)=1;humanBodySim=true;
}
BYTE *playerWaitType=nullptr,*playerWait=nullptr;
BYTE *majorType=nullptr;
BYTE *factoryModeType=nullptr,*factoryRobotType=nullptr,*factoryRacerType=nullptr;
BYTE* factoryAnimationType=nullptr;
BYTE* factorySimulationType=nullptr;
BYTE* factoryKartEventType=nullptr;
void* __fastcall FactoryKartEventType(){return factoryKartEventType;}
unsigned factoryRemoteTickPauses=0;
BYTE *factoryNativeItemType=nullptr,*factoryUseStack=nullptr;
unsigned factoryUseCalls=0,factoryConsumed=0,factoryUseNotices=0;
unsigned factoryOverlapCalls=0;bool factoryOriginalOverlap=false;
bool __fastcall FactoryOriginalOverlap(void*){++factoryOverlapCalls;return factoryOriginalOverlap;}
struct FixturePrimaryId {unsigned long long type,name;};
unsigned long long resourceName=901,resourceType=902,resourceRoundTrip=901;
bool resourceAllocatedEmpty=false,resourceLoadFailure=false,resourceResolveFailure=false;
void* resourceLoaded=nullptr;unsigned resourceLoadCalls=0;
void* __fastcall ResourcePathName(void* out,const wchar_t*,int){*static_cast<unsigned long long*>(out)=901;return out;}
FixturePrimaryId* __fastcall ResourceIdentity(void*,FixturePrimaryId* out,const TwinKartRace::AiAssetPath*){*out={resourceType,resourceName};return out;}
TwinKartEquipment::String* __fastcall ResourceNameText(TwinKartEquipment::String* out,unsigned long long) {
    const wchar_t id[]=L"Kart:499000001";auto text=static_cast<wchar_t*>(HeapAlloc(GetProcessHeap(),0,sizeof(id)));
    memcpy(text,id,sizeof(id));*out={text,static_cast<int>(ARRAYSIZE(id)),static_cast<int>(ARRAYSIZE(id))};return out;
}
TwinKartRace::AiAssetPath* __fastcall ResourceRoundTrip(void*,TwinKartRace::AiAssetPath* out,unsigned long long,const TwinKartEquipment::String*) {
    *out={resourceRoundTrip,{}};
    if(resourceAllocatedEmpty) {
        auto empty=static_cast<wchar_t*>(HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(wchar_t)));
        out->subpath={empty,1,1};
    }
    return out;
}
void* __fastcall ResourceStringCopy(void* raw,const void* source) {
    auto out=static_cast<TwinKartEquipment::String*>(raw);auto in=static_cast<const TwinKartEquipment::String*>(source);
    *out={};if(in->count) {
        auto data=static_cast<wchar_t*>(HeapAlloc(GetProcessHeap(),0,static_cast<size_t>(in->count)*sizeof(wchar_t)));
        memcpy(data,in->data,static_cast<size_t>(in->count)*sizeof(wchar_t));*out={data,in->count,in->count};
    }
    return out;
}
void* __fastcall ResourceSynchronousLoad(TwinKartRace::ObjectiveSoftReference* reference) {
    ++resourceLoadCalls;Check(reference && reference->index==-1 && !reference->serial && !reference->tag && reference->path.name==901);
    Check(reference->path.subpath.count==1 && reference->path.subpath.data && !reference->path.subpath.data[0]);
    return resourceLoadFailure?nullptr:resourceLoaded;
}
void* __fastcall ResourceResolve(const TwinKartEquipment::String* identifier) {
    Check(identifier && !wcscmp(identifier->data,L"Kart:499000001"));return resourceResolveFailure?nullptr:resourceLoaded;
}
void* __fastcall FactoryNativeItemType(){return factoryNativeItemType;}
void __fastcall FactoryNativeItemUse(void*) {
    ++factoryUseCalls;if(factoryUseStack && *reinterpret_cast<int*>(factoryUseStack+0xc8)>0)--*reinterpret_cast<int*>(factoryUseStack+0xc8);
}
void ObserveFactoryItem(const char* name,int slot,int value) {
    if(!strcmp(name,"native_factory_cooperative_item_consumed")){Check(slot==1 && value==1);factoryConsumed+=value;}
    if(!strcmp(name,"native_factory_cooperative_item_use_observed")){Check(slot==1 && value==1);++factoryUseNotices;}
}
void* __fastcall FactorySimulationType(){return factorySimulationType;}
bool __fastcall FactoryTickEnabled(void* raw){return static_cast<BYTE*>(raw)[0x3b]!=0;}
void __fastcall FactorySetTick(void* raw,bool enabled){Check(!enabled);++factoryRemoteTickPauses;static_cast<BYTE*>(raw)[0x3b]=enabled?1:0;}
BYTE *factoryStateType=nullptr,*factoryManagerType=nullptr;
BYTE* factorySequenceType=nullptr;
BYTE *factoryIntroRuleType=nullptr,*factoryMajorSequenceType=nullptr,*factoryMajorSequence=nullptr,*factoryIntroMovie=nullptr;
void* __fastcall FactoryIntroRuleType(){return factoryIntroRuleType;}
void* __fastcall FactoryMajorSequenceType(){return factoryMajorSequenceType;}
unsigned factoryIntroCalls=0,factoryIntroSequenceCalls=0,factoryUnhideCalls=0;
bool factoryIntroMakesCall=true;
void __fastcall FactoryIntroSequenceFixture(void*,void*,void*){++factoryIntroSequenceCalls;}
void __fastcall FactorySetHidden(void* raw,bool hidden){++factoryUnhideCalls;auto actor=static_cast<BYTE*>(raw);actor[0x58]=hidden?(actor[0x58]|0x20):(actor[0x58]&~0x20);}
void* __fastcall FactorySequenceType(){return factorySequenceType;}
unsigned factoryPodiumCalls=0;void* factoryPodiumArguments[4]{};
void __fastcall FactoryPodiumFixture(void* component,void* sequence,const void* environment,void* widget) {
    ++factoryPodiumCalls;factoryPodiumArguments[0]=component;factoryPodiumArguments[1]=sequence;
    factoryPodiumArguments[2]=const_cast<void*>(environment);factoryPodiumArguments[3]=widget;
}
void* __fastcall FactoryModeType(){return factoryModeType;}
void* __fastcall FactoryRobotType(){return factoryRobotType;}
void* __fastcall FactoryRacerType(){return factoryRacerType;}
void* __fastcall FactoryAnimationType(){return factoryAnimationType;}
void* __fastcall FactoryStateType(){return factoryStateType;}
void* __fastcall FactoryManagerType(){return factoryManagerType;}
unsigned factoryFallbacks=0;
void* __fastcall FactoryFallback(void* value){++factoryFallbacks;return value;}
BYTE* progressType=nullptr;
std::vector<std::array<int,2>> observedRanks,observedTimes;
void ObserveFinish(const char* name,int slot,int value) {
    if(!strcmp(name,"native_local_race_finish_rank"))observedRanks.push_back({slot,value});
    if(!strcmp(name,"native_local_race_finish_time_ms"))observedTimes.push_back({slot,value});
}
BYTE* flagTypes[5]{};
BYTE *flagGameModeType=nullptr,*flagConfigType=nullptr;
BYTE *contextType=nullptr,*flagContextType=nullptr;
BYTE *chaserContextType=nullptr,*chaserContextModeType=nullptr;
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
void* __fastcall ChaserContextType(){return chaserContextType;}
void* __fastcall ChaserContextModeType(){return chaserContextModeType;}
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
    TwinKartRace::ArenaPoint point{start->x,start->y,123},normal{0,0,1};memcpy(hit+0xc,&point,sizeof(point));memcpy(hit+0x30,&normal,sizeof(normal));hit[0x5d]=1;return true;
}
unsigned objectTraceCalls=0;bool objectTraceBlocked=false;
bool __fastcall ObjectTraceFixture(void* queryWorld,const TwinKartRace::ArenaPoint* start,const TwinKartRace::ArenaPoint* end,
    const TwinKartRace::QueryArray* types,bool complex,const TwinKartRace::QueryArray* ignored,int draw,void* hit,bool ignoreSelf,
    const TwinKartRace::QueryColor* color,const TwinKartRace::QueryColor* hitColor,float duration) {
    ++objectTraceCalls;Check(queryWorld && start && end && types && ignored && !complex && !draw && !ignoreSelf && duration==0);
    Check(types->count==2 && types->capacity==2 && static_cast<const unsigned char*>(types->data)[0]==7);
    Check(ignored->count==1 && ignored->capacity==1 && static_cast<void* const*>(ignored->data)[0]==queryWorld);
    Check(reinterpret_cast<std::uintptr_t>(hit)%16==0 && color && hitColor && color->a==1 && hitColor->a==1);
    static_cast<unsigned char*>(hit)[0x87]=0xa5;return objectTraceBlocked;
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
void* rejectComponentWorldActor=nullptr;
BYTE* __fastcall ComponentWorldFixture(void* object){return object==rejectComponentWorldActor?nullptr:World(object);}
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
    if(type==factoryAnimationType)return c.animation;
    if(type==factoryManagerType)return c.manager;
    if(type==factorySimulationType)return c.simulation;
    if(type==factoryKartEventType)return c.kartEvents;
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
unsigned factoryComponentCreates=0,factoryComponentRegisters=0,factoryItemModeCalls=0;
bool chaserCoreRegistration=false;int chaserCoreExpireAfter=-1;BYTE* chaserCoreExpirePawn=nullptr;
void* __fastcall FactoryComponentConstruct(void* raw) {
    auto params=static_cast<BYTE**>(raw);++factoryComponentCreates;
    if(chaserCoreRegistration)Check(components[params[1]].prepared);
    auto result=Object(0x1200,params[0],nullptr,world);*reinterpret_cast<void**>(result+0x20)=params[1];return result;
}
void __fastcall FactoryComponentRegister(void* raw) {
    auto value=static_cast<BYTE*>(raw);auto pawn=*reinterpret_cast<BYTE**>(value+0xa0);auto type=*reinterpret_cast<void**>(value+0x10);
    if(!pawn)pawn=*reinterpret_cast<BYTE**>(value+0x20);
    auto& c=components[pawn];BYTE** destination=type==pathType?&c.path:type==movementType?&c.movement:type==factoryAnimationType?&c.animation:type==factoryManagerType?&c.manager:type==itemTeamType?&c.behavior:type==factorySimulationType?&c.simulation:type==factoryKartEventType?&c.kartEvents:nullptr;
    if(destination && *destination==value)return;
    Check(destination && !*destination);if(destination){*destination=value;*reinterpret_cast<void**>(value+0xa0)=pawn;++factoryComponentRegisters;}
    if(chaserCoreRegistration && type==movementType)value[0xed8]=0;
    if(chaserCoreRegistration && chaserCoreExpireAfter==static_cast<int>(factoryComponentRegisters) && chaserCoreExpirePawn)
        ++objectSlots[*reinterpret_cast<int*>(chaserCoreExpirePawn+0xc)].serial;
}
void __fastcall FactoryConfigureItemMode(void* raw){static_cast<BYTE*>(raw)[0xed8]=1;++factoryItemModeCalls;}
unsigned factoryAiStateSets=0;
void __fastcall FactorySetAiState(void* raw){static_cast<BYTE*>(raw)[0x321]=1;++factoryAiStateSets;}
unsigned factoryDedicatedRegistrations=0;
void __fastcall FactoryDedicatedRegistration(void* raw) {
    ++factoryDedicatedRegistrations;auto pawn=static_cast<BYTE*>(raw)-0x320;
    for(auto type:{pathType,movementType,factoryAnimationType}) {
        alignas(8) BYTE parameters[0x40]{};auto values=reinterpret_cast<BYTE**>(parameters);values[0]=type;values[1]=pawn;
        auto component=R::OnFactoryConstruct(parameters);FactoryComponentRegister(component);
        if(type==movementType)FactoryConfigureItemMode(component);
    }
}
bool factorySeenAndroid=false;
unsigned spawnFixtureCalls=0,chaserClassifierCalls=0;
bool __fastcall FactorySpawnFixture(void*,void*,int,void*,void**,bool* isAi){++spawnFixtureCalls;factorySeenAndroid=*isAi;return true;}
void ChaserClassifierFixture(void*,void*,int slot,bool* isAi){++chaserClassifierCalls;if(isAi)*isAi=slot>0;}
unsigned factoryManagerForwards=0,factoryManagerReadyBeforeForward=0;
void __fastcall FactoryManagerFixture(void* raw) {
    ++factoryManagerForwards;auto phaseValue=*reinterpret_cast<BYTE**>(static_cast<BYTE*>(raw)+0x28);auto state=*reinterpret_cast<BYTE**>(phaseValue+0xa0);
    if(Find(state,factoryManagerType))++factoryManagerReadyBeforeForward;
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
bool __fastcall EquipmentAlreadyEqual(const void*,const void*){++equipmentComparisons;return true;}
std::wstring queriedSkillCharacter;unsigned skillQueries=0;
const TwinKartEquipment::String* expectedSkillArgument=nullptr;
unsigned skillOwnedBlocks=0;bool skillQueryResult=true;
void* __fastcall SkillArgumentCopy(void* raw,const void* source) {
    auto target=static_cast<TwinKartEquipment::String*>(raw);
    auto value=static_cast<const TwinKartEquipment::String*>(source);
    Need(target==expectedSkillArgument,"Skill hook replaced the native by-value argument object");
    if(target->data) {
        Need(HeapValidate(GetProcessHeap(),0,target->data)!=FALSE,"Skill argument was not engine-owned storage");
        Need(HeapFree(GetProcessHeap(),0,const_cast<wchar_t*>(target->data))!=FALSE,"Old native skill temporary cleanup");--skillOwnedBlocks;
    }
    auto next=static_cast<wchar_t*>(HeapAlloc(GetProcessHeap(),0,value->count*sizeof(wchar_t)));
    Need(next!=nullptr,"Owned skill-copy allocation");++skillOwnedBlocks;
    memcpy(next,value->data,value->count*sizeof(wchar_t));*target={next,value->count,value->count};return target;
}
void SetSkillArgument(TwinKartEquipment::String& target,const wchar_t* value) {
    Need(!target.data,"Consumed skill temporary reused without reconstruction");
    int count=static_cast<int>(wcslen(value)+1);auto copy=static_cast<wchar_t*>(HeapAlloc(GetProcessHeap(),0,count*sizeof(wchar_t)));
    Need(copy!=nullptr,"Native caller skill temporary allocation");++skillOwnedBlocks;memcpy(copy,value,count*sizeof(wchar_t));target={copy,count,count};
    expectedSkillArgument=&target;
}
bool __fastcall SkillQuery(void*,const TwinKartEquipment::String* character,unsigned char,void*) {
    Need(character==expectedSkillArgument && character->data && HeapValidate(GetProcessHeap(),0,character->data)!=FALSE,
        "Native consuming skill query received borrowed roster storage");
    ++skillQueries;queriedSkillCharacter=character->data;
    Need(HeapFree(GetProcessHeap(),0,const_cast<wchar_t*>(character->data))!=FALSE,"Native skill callee could not release its by-value argument");
    --skillOwnedBlocks;*const_cast<TwinKartEquipment::String*>(character)={};return skillQueryResult;
}
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
void __fastcall FactoryIntroFixture(void*) {
    ++factoryIntroCalls;
    for(size_t i=0;i<pawns.size() && i<4;++i)pawns[i][0x58]|=0x20;
    if(factoryIntroMakesCall && !pawns.empty())R::OnFactoryIntroSequence(factoryMajorSequence,factoryIntroMovie,pawns[0]);
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
    CheckForwardRecovery();
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
    R::eventSkillRoster=R::aiRoster;
    Need(R::eventSkillRoster.ApplyCharacterRoles(roles,available,nullptr),"Registered event skill roster");
    R::nativeCharacterRules=true;unsigned beforeFixed=equipmentCopies;
    R::OnUnknown(major,pawns[0],0,false);
    Need(equipmentCopies==beforeFixed+17 && !wcscmp(observedCharacter.data,L"SelectedCharacter") && !wcscmp(observedKart.data,L"SelectedKart"),
        "Event skill roles replaced the player's selected appearance or kart customization");
    R::characterSkillQuery=SkillQuery;auto skillComponent=Object(0x200,nullptr,pawns[0],world);
    Stub(0x923880,reinterpret_cast<void*>(SkillArgumentCopy));
    TwinKartEquipment::String visualCharacter{};SetSkillArgument(visualCharacter,L"SelectedCharacter");
    Need(R::OnCharacterSkillQuery(skillComponent,&visualCharacter,0,nullptr) && skillQueries==1 && queriedSkillCharacter==L"Character:2",
        "Prescribed event skill failed independently of the visible player character");
    Need(!skillOwnedBlocks,"Event skill query leaked its owned FString");
    TwinKartEquipment::Preset retainedRole;Need(R::eventSkillRoster.GetCharacter(0,retainedRole) && !wcscmp(retainedRole.character[0].data,L"Character:2"),
        "Native query released the retained role roster");
    SetSkillArgument(visualCharacter,L"SelectedCharacter");skillQueryResult=false;
    Need(!R::OnCharacterSkillQuery(skillComponent,&visualCharacter,0,nullptr) && !skillOwnedBlocks,
        "Failed native skill query did not consume its owned temporary exactly once");skillQueryResult=true;
    Stub(0x923880,reinterpret_cast<void*>(EquipmentCopy));
    std::set<std::wstring> eventVisualCharacters;R::aiRosterEnabled=true;
    for(int seat=1;seat<=7;++seat) {
        auto& c=components[pawns[seat]];c.kart=Object(0x1200,kartType,pawns[seat],world);c.character=Object(0x1100,characterType,pawns[seat],world);
        TwinKartEquipment::Preset selected;Need(R::aiRoster.Get(seat,selected),"Random appearance missing");
        std::wstring expected=selected.character[0].data;R::OnUnknown(major,pawns[seat],seat,false);
        Need(observedCharacter.data && expected==observedCharacter.data && eventVisualCharacters.insert(expected).second,
            "Event team role collapsed the individually randomized race/result appearance");
    }
    R::aiRosterEnabled=false;
    SetSkillArgument(visualCharacter,L"SelectedCharacter");
    R::OnCharacterSkillQuery(foreignPhase,&visualCharacter,0,nullptr);
    Need(skillQueries==3 && queriedSkillCharacter==L"SelectedCharacter" && !skillOwnedBlocks,"Event skill role escaped the owned race world");
    R::nativeCharacterRules=false;
    SetSkillArgument(visualCharacter,L"SelectedCharacter");
    R::OnCharacterSkillQuery(skillComponent,&visualCharacter,0,nullptr);
    Need(skillQueries==4 && queriedSkillCharacter==L"SelectedCharacter" && !skillOwnedBlocks,"Event skill role leaked into an ordinary race");
    // Recover only the missing native local-human enable operation at the
    // actual BeginRace transition, once. Never force throttle or unfreeze a
    // native item reaction; preserve an existing delegate and foreign worlds.
    malformed=false;
    drivingPhaseType=Object(0x40);movementInterfaceType=Object(0x40);
    Stub(TwinKartGraphics::PhaseObjectGetter,reinterpret_cast<void*>(DrivingPhaseType));
    Stub(0x239be10,reinterpret_cast<void*>(MovementInterfaceType));Stub(0x277d780,reinterpret_cast<void*>(MovementInterfaceQuery));
    Stub(0x15be550,reinterpret_cast<void*>(EnableHumanMovement));
    auto phaseGlobals=fixtureImage+0x68ba000;
    Need(VirtualAlloc(phaseGlobals,4096,MEM_COMMIT,PAGE_READWRITE)==phaseGlobals,"Phase name fixture allocation");
    *reinterpret_cast<unsigned long long*>(fixtureImage+TwinKartGraphics::BeginRaceNameRva)=123;
    auto nativeMovementTable=fixtureImage+0x5118000;
    Need(VirtualAlloc(nativeMovementTable,4096,MEM_COMMIT,PAGE_READWRITE)==nativeMovementTable,"Movement interface fixture allocation");
    *reinterpret_cast<void**>(fixtureImage+0x5118b80+0x1f8)=fixtureImage+0x15be550;
    auto humanMovement=components[pawns[0]].movement;
    *reinterpret_cast<void**>(humanMovement+0x140)=fixtureImage+0x5118b80;
    auto drivingPhase=Object(0x40,drivingPhaseType);*reinterpret_cast<unsigned long long*>(drivingPhase+0x28)=122;
    *reinterpret_cast<void**>(phase+TwinKartGraphics::CurrentPhaseOffset)=drivingPhase;
    R::PrepareHumanStartMovement(phase);Need(!humanMovementEnables,"Human movement enabled before BeginRace");
    *reinterpret_cast<unsigned long long*>(drivingPhase+0x28)=123;
    R::PrepareHumanStartMovement(phase);R::PrepareHumanStartMovement(phase);
    Need(humanMovementEnables==1 && R::humanStartMovement.object==humanMovement,"Missing human movement not recovered exactly once");
    R::humanStartMovement={};R::PrepareHumanStartMovement(phase);
    Need(humanMovementEnables==1,"An already enabled native movement was initialized twice");
    R::humanStartMovement={};*reinterpret_cast<int*>(humanMovement+0x200)=0;
    auto humanState=components[pawns[0]].state;humanState[0x321]=1;
    R::PrepareHumanStartMovement(phase);Need(humanMovementEnables==1,"An AI received the human start recovery");humanState[0x321]=0;
    auto duplicateHuman=components[pawns[1]].state;auto previousAi=duplicateHuman[0x321];duplicateHuman[0x321]=0;
    *reinterpret_cast<int*>(duplicateHuman+0x168)=0;R::PrepareHumanStartMovement(phase);
    Need(humanMovementEnables==1,"Ambiguous human identity accepted");
    duplicateHuman[0x321]=previousAi;*reinterpret_cast<int*>(duplicateHuman+0x168)=1;
    R::PrepareHumanStartMovement(foreignPhase);Need(humanMovementEnables==1,"Human recovery escaped the local race world");
    {
        auto rootTable=fixtureImage+0x5116000;
        Need(VirtualAlloc(rootTable,8192,MEM_COMMIT,PAGE_READWRITE)==rootTable,"Kart root vtable fixture allocation");
        auto setRootVirtual=[](size_t slot,unsigned rva,void* method){
            Stub(rva,method);*reinterpret_cast<void**>(fixtureImage+0x5116878+slot)=fixtureImage+rva;
        };
        setRootVirtual(0x408,0x41d8f70,reinterpret_cast<void*>(HumanSimulating));
        setRootVirtual(0x6d8,0x428fcd0,reinterpret_cast<void*>(HumanRootBody));
        *reinterpret_cast<void**>(fixtureImage+0x5116878+0x5a8)=fixtureImage+0x42994f0;
        Stub(0x3de02a0,reinterpret_cast<void*>(HumanTickEnabled));Stub(0x3de6fb0,reinterpret_cast<void*>(HumanEnableTick));
        *reinterpret_cast<void**>(fixtureImage+0x5118590+0x358)=fixtureImage+0x3de6fb0;
        *reinterpret_cast<void**>(fixtureImage+0x5118590+0x368)=fixtureImage+0x3de02a0;
        auto root=Object(0x1100,kartType,pawns[0],world),motion=Object(0x410,stateType,pawns[0],world);
        auto savedMovementTable=*reinterpret_cast<void**>(humanMovement);
        auto savedRoot=*reinterpret_cast<BYTE**>(pawns[0]+0x130);
        auto savedUpdated=*reinterpret_cast<BYTE**>(humanMovement+0xb8);
        *reinterpret_cast<void**>(humanMovement)=fixtureImage+0x5118590;
        *reinterpret_cast<void**>(root)=fixtureImage+0x5116878;
        *reinterpret_cast<BYTE**>(root+0x4c0)=Object(0x40);
        *reinterpret_cast<BYTE**>(pawns[0]+0x130)=*reinterpret_cast<BYTE**>(humanMovement+0xb8)=root;
        *reinterpret_cast<int*>(humanMovement+0x200)=3;
        R::humanBodyRepairTried=false;R::humanBodyCheckUntil=1000;humanBodySim=false;humanTick=false;
        R::RecoverHumanDriveBody(humanMovement,pawns[0],motion,500);
        Need(humanMovementEnables==1 && !humanTick,"Missing physics body should not be treated as ready");
        humanBodyExists=true;motion[0x359]=1;
        R::RecoverHumanDriveBody(humanMovement,pawns[0],motion,500);
        Need(humanMovementEnables==1 && !humanTick,"Native freeze overridden");motion[0x359]=0;
        R::RecoverHumanDriveBody(humanMovement,pawns[0],motion,1001);
        Need(humanMovementEnables==1 && !humanTick,"Late start recovery after driving begins");
        R::RecoverHumanDriveBody(humanMovement,pawns[0],motion,500);
        Need(humanMovementEnables==2 && humanTick && humanBodySim,"Dormant loaded player body not rearmed through native methods");
        humanBodySim=false;R::RecoverHumanDriveBody(humanMovement,pawns[0],motion,600);
        Need(humanMovementEnables==2 && !humanBodySim,"Start recovery ran more than once");
        *reinterpret_cast<void**>(humanMovement)=savedMovementTable;
        *reinterpret_cast<BYTE**>(pawns[0]+0x130)=savedRoot;
        *reinterpret_cast<BYTE**>(humanMovement+0xb8)=savedUpdated;
    }
    *reinterpret_cast<void**>(phase+TwinKartGraphics::CurrentPhaseOffset)=nullptr;R::humanStartMovement={};malformed=true;
    // The objective lease must cover both the actor and component world and
    // is inactive by default. Ordinary race/Factory leases remain separate.
    auto scopePawn=pawns[0];auto scopeKart=components[scopePawn].kart;
    TwinKartCamera::ObjectIdentity objectiveIdentity{};
    Need(TwinKartCamera::Capture(otherWorld,chunks,512,&objectiveIdentity) &&
        R::objectiveWorld.Bind(objectiveIdentity,chunks,512),"Objective lease fixture");
    R::equipmentPawn=scopePawn;R::equipmentReady=true;
    rejectComponentWorldActor=scopePawn;
    Stub(0x3ddf4b0,reinterpret_cast<void*>(ComponentWorldFixture));
    worlds[scopePawn]=otherWorld;worlds[scopeKart]=otherWorld;
    Need(!R::OwnEquipmentDescriptor(scopeKart+0x10b0,0x22fef20,0x10b0),"Objective appearance became active without temporary scope");
    R::objectiveEquipmentScope=true;
    Need(R::OwnEquipmentDescriptor(scopeKart+0x10b0,0x22fef20,0x10b0),"Owned objective appearance rejected");
    auto scopeCharacter=components[scopePawn].character;worlds[scopeCharacter]=otherWorld;
    alignas(8) BYTE objectiveKartDesired[0xb8]{},objectiveCharacterDesired[0x78]{};
    R::kartEqual=R::characterEqual=EquipmentAlreadyEqual;
    unsigned compareBeforeRecovery=equipmentComparisons;
    Need(!R::OnKartDescriptorEqual(scopeKart+0x10b0,objectiveKartDesired) &&
        !R::OnCharacterDescriptorEqual(scopeCharacter+0x1020,objectiveCharacterDesired) &&
        equipmentComparisons==compareBeforeRecovery+2,
        "Equal objective descriptors skipped original model rebuild with missing kart/character meshes or forwarded twice");
    auto loadedKartMesh=Object(0x40),loadedCharacterMesh=Object(0x40);
    *reinterpret_cast<void**>(scopeKart+0x4c0)=loadedKartMesh;*reinterpret_cast<void**>(scopeCharacter+0x4c0)=loadedCharacterMesh;
    Need(R::OnKartDescriptorEqual(scopeKart+0x10b0,objectiveKartDesired) &&
        R::OnCharacterDescriptorEqual(scopeCharacter+0x1020,objectiveCharacterDesired),
        "Valid loaded objective meshes caused redundant native model rebuilds");
    *reinterpret_cast<unsigned*>(loadedKartMesh+8)=0x10000;
    Need(!R::OnKartDescriptorEqual(scopeKart+0x10b0,objectiveKartDesired),"Expired skeletal mesh satisfied objective recovery");
    *reinterpret_cast<unsigned*>(loadedKartMesh+8)=0;
    bool previousInstalled=R::installed;R::installed=true;
    Need(R::ObjectiveAppearanceMeshMask(scopePawn)==3,"Loaded kart and character mesh assets were not independently detected");
    *reinterpret_cast<void**>(scopeCharacter+0x4c0)=nullptr;
    Need(R::ObjectiveAppearanceMeshMask(scopePawn)==1,"Kart-only model was accepted as a complete loaded appearance");
    scopePawn[0x5c]|=0x10;
    Need(R::ObjectiveAppearanceMeshMask(scopePawn)==-1 && R::PrepareObjectiveAppearance(scopePawn,0)==-1,
        "A destroying role pawn received model readiness or appearance preparation");
    scopePawn[0x5c]&=~0x10;
    worlds[scopeCharacter]=world;
    Need(R::ObjectiveAppearanceMeshMask(scopePawn)==-1 && R::OnCharacterDescriptorEqual(scopeCharacter+0x1020,objectiveCharacterDesired),
        "Objective recovery or readiness escaped the character component's owned world");
    worlds[scopeCharacter]=otherWorld;
    std::thread wrongMeshOwner([&]{Need(R::ObjectiveAppearanceMeshMask(scopePawn)==-1,"Non-game-thread objective model readiness");});wrongMeshOwner.join();
    R::objectiveEquipmentScope=false;
    Need(R::OnKartDescriptorEqual(scopeKart+0x10b0,objectiveKartDesired),"Objective model recovery changed ordinary native equality");
    R::objectiveEquipmentScope=true;R::installed=previousInstalled;
    R::kartEqual=R::characterEqual=EquipmentEqual;
    *reinterpret_cast<void**>(scopeKart+0x4c0)=nullptr;worlds[scopeCharacter]=world;
    worlds[scopeKart]=world;
    worlds[scopePawn]=otherWorld;
    // Even an otherwise valid circuit component cannot escape the active
    // objective appearance call's stricter actor/component world scope.
    Need(!R::OwnEquipmentDescriptor(scopeKart+0x10b0,0x22fef20,0x10b0),"Foreign component accepted under objective lease");
    worlds[scopeKart]=otherWorld;worlds[scopePawn]=world;
    Need(!R::OwnEquipmentDescriptor(scopeKart+0x10b0,0x22fef20,0x10b0),"Foreign actor accepted under objective lease");
    worlds[scopeKart]=world;worlds[scopePawn]=world;
    TwinKartCamera::ObjectIdentity circuitIdentity{};
    Need(TwinKartCamera::Capture(world,chunks,512,&circuitIdentity) && R::raceWorld.Bind(circuitIdentity,chunks,512),"Circuit lease restoration");
    R::objectiveWorld.Reset();R::objectiveEquipmentScope=false;
    rejectComponentWorldActor=nullptr;Stub(0x3ddf4b0,reinterpret_cast<void*>(World));
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
    Need(!ground.Initialize({-15000,-123000,0},{-9000,-61000,0},300) &&
        ground.InitializeBounded({-15000,-123000,0},{-9000,-61000,0}) && ground.Count()<=20000,
        "Formal arena dimensions repeatedly rejected the development-map grid or exceeded its fixed memory budget");
    Need(!ground.InitializeBounded({0,0,0},{500000,500000,0}) && !ground.Started(),
        "Unbounded arena escaped the road-sampling memory/axis limits");
    arena.Reset();auto backing=arena.Step({0,0,0},0,{-1000,0,0},0.05f,1);
    Need(backing.reverse && backing.throttle<0 && TwinKartRace::NativeKartThrottle(backing)==-1.f,
        "Explicit kart reversing lost the original reverse-key sentinel");
    Need(TwinKartRace::NativeKartThrottle({-0.25f,0,false})==-0.25f &&
        TwinKartRace::NativeKartThrottle({0.35f,0,false})==0.35f,"Analog brake/acceleration was converted into a reverse-key command");
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
    Need(ground.Initialize({0,0,0},{1200,1200,0}),"Horizontal wall fixture");
    while(ground.Next(sample))ground.Sample(0,true);
    Need(ground.Path({0,400,0},{1200,400,0},route),"Open flat graph path");
    unsigned blocked=0;
    for(size_t i=0;i<ground.AdjacencyCount();++i) {
        int a=0,b=0;TwinKartRace::ArenaPoint first{},second{};
        if(ground.AdjacencyAt(i,a,b,first,second) && ((first.x<=400 && second.x>=800) || (second.x<=400 && first.x>=800))) {
            Need(ground.BlockTransition(a,b),"Real horizontal obstacle did not block both directions");++blocked;
        }
    }
    Need(blocked>0 && !ground.Path({0,400,0},{1200,400,0},route) && !ground.Path({1200,400,0},{0,400,0},route),
        "Graph crossed a sampled wall or smoothed through its blocked diagonal");
    Need(ground.RegionAt({0,400,0})>=0 && ground.RegionAt({0,400,0})!=ground.RegionAt({1200,400,0}),
        "Goal selection treated disconnected wall regions as reachable");
    auto wrongGoalAnchor=[](const TwinKartRace::ArenaPoint&,const TwinKartRace::ArenaPoint& to,void*)->bool{return to.x>=800;};
    TwinKartRace::ArenaPoint rejectedGoal{99,98,97};
    Need(!ground.ReachableSampleAt(5,ground.RegionAt({0,400,0}),rejectedGoal,wrongGoalAnchor,nullptr) && rejectedGoal.x==99,
        "Goal selection accepted a sampled cell whose real collision-validated anchor belonged to another region");
    Need(!ground.BlockTransition(-1,0) && !ground.BlockTransition(0,0) && !ground.BlockTransition(0,15),"Invalid graph edge accepted");
    Need(ground.Initialize({0,0,0},{1200,1200,0}),"Blocked-edge reset fixture");
    while(ground.Next(sample))ground.Sample(0,true);
    Need(ground.RegionAt({0,400,0})==ground.RegionAt({1200,400,0}) && ground.RegionAtSample(9999)==-1,
        "Graph reset retained stale disconnected regions or accepted an invalid sample");
    auto anchor=[](const TwinKartRace::ArenaPoint& from,const TwinKartRace::ArenaPoint& to,void*)->bool{return from.x>=200 || to.x>=400;};
    Need(ground.Path({0,400,0},{1200,400,0},route,anchor,nullptr) && route.front().x>=400,"Path chose an occluded nearest start anchor");
    auto closed=[](const TwinKartRace::ArenaPoint&,const TwinKartRace::ArenaPoint&,void*)->bool{return false;};
    Need(!ground.Path({0,400,0},{1200,400,0},route,closed,nullptr),"Path invented an anchor when all start/goal connections were blocked");
    unsigned char queryTypes[]{7,9};void* ignoredActor[]{world};
    TwinKartRace::QueryArray typeInput{queryTypes,2,2},ignoreInput{ignoredActor,1,1};
    Need(TwinKartRace::QueryLineClear(world,ObjectTraceFixture,typeInput,ignoreInput,{0,0,80},{100,0,80}) && objectTraceCalls==1 && !errors,
        "Original object trace boundary lost borrowed arrays, ignore actors, ABI extent or no-debug settings");
    objectTraceBlocked=true;Need(!TwinKartRace::QueryLineClear(world,ObjectTraceFixture,typeInput,ignoreInput,{0,0,80},{100,0,80}) && objectTraceCalls==2,
        "Blocking original trace was called clear");
    typeInput.count=0;Need(!TwinKartRace::QueryLineClear(world,ObjectTraceFixture,typeInput,ignoreInput,{0,0,80},{100,0,80}) && objectTraceCalls==2,
        "Invalid object trace inputs reached the native boundary");
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
    chaserContextType=Object(0x40);chaserContextModeType=Object(0x40);
    Stub(0x2250d20,reinterpret_cast<void*>(ChaserContextType));Stub(0x22507c0,reinterpret_cast<void*>(ChaserContextModeType));
    auto savedContextWorldMode=*reinterpret_cast<BYTE**>(world+0x118);
    auto savedContextObjective=R::objectiveWorld;
    auto contextMode=Object(0x500,chaserContextModeType,nullptr,world);
    *reinterpret_cast<BYTE**>(world+0x118)=contextMode;
    R::armedMode=108;R::chaserItemContextEnabled=false;*reinterpret_cast<void**>(pawns[1]+0x10)=flagAiType;
    auto contextCheck=[&](BYTE* expected,const char* message) {
        std::array<BYTE,0x40> originalParameters{};memcpy(originalParameters.data(),contextParameters,0x40);
        unsigned countBefore=contextConstructCalls;BYTE originalSpeedMode=pawns[1][0x328];
        Need(R::OnItemContextConstruct(contextParameters)==expected && seenContextType==expected &&
             contextConstructCalls==countBefore+1 && !memcmp(originalParameters.data(),contextParameters,0x40) &&
             pawns[1][0x328]==originalSpeedMode,message);
    };
    contextCheck(contextType,"Disabled Chaser compatibility selected a new native item context");
    R::chaserItemContextEnabled=true;R::objectiveWorld.Reset();
    contextCheck(contextType,"Chaser context selected before original objective-world ownership was established");
    TwinKartCamera::ObjectIdentity contextWorldIdentity;
    auto contextChunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(R::image+0x69c28b0);
    int contextObjects=*reinterpret_cast<int*>(R::image+0x69c28c4);
    Need(TwinKartCamera::Capture(world,contextChunks,contextObjects,&contextWorldIdentity) &&
         R::objectiveWorld.Bind(contextWorldIdentity,contextChunks,contextObjects),"Chaser context fixture objective world did not bind");
    contextCheck(chaserContextType,"Original Chaser SpeedSingleAiPawn did not construct its dedicated original context exactly once");
    Need(!R::Eligible(major) && !R::BindWorld(major) && !R::Local(major),
         "Eight-seat Chaser entered circuit AI registration or result ownership through an existing race lease");
    Need(!R::InActorRaceWorld(pawns[1]) && !R::InRaceWorld(components[pawns[1]].state),
         "Chaser inherited actor/component circuit overrides through an older same-world lease");
    worlds[pawns[1]]=otherWorld;contextCheck(contextType,"Chaser context selection escaped its leased original world");worlds[pawns[1]]=world;
    *reinterpret_cast<void**>(contextMode+0x10)=aiType;contextCheck(contextType,"Another native game mode inherited Chaser item context");
    *reinterpret_cast<void**>(contextMode+0x10)=chaserContextModeType;
    parameterValues[0]=chaserContextType;contextCheck(chaserContextType,"An existing dedicated item context was replaced or constructed twice");parameterValues[0]=contextType;
    *reinterpret_cast<void**>(pawns[1]+0x10)=aiType;contextCheck(contextType,"Chaser context override changed an unrelated pawn class");
    *reinterpret_cast<void**>(pawns[1]+0x10)=flagAiType;
    auto originalContextThread=R::owner;R::owner=originalContextThread+1;
    contextCheck(contextType,"Foreign owner thread selected Chaser item context");R::owner=originalContextThread;
    // Exercise the actual production core adapter with original registration
    // callbacks, missing pieces, native serial expiry and scoped recovery.
    factoryAnimationType=Object(0x40);factoryKartEventType=Object(0x40);
    Stub(0x23077d0,reinterpret_cast<void*>(FactoryAnimationType));
    Stub(0x22f6900,reinterpret_cast<void*>(FactoryKartEventType));
    Stub(0x23b02c0,reinterpret_cast<void*>(FactorySetAiState));
    Stub(0x2771dd0,reinterpret_cast<void*>(FlagParameters));
    Stub(0x3de4900,reinterpret_cast<void*>(FactoryComponentRegister));
    R::construct=FactoryComponentConstruct;chaserCoreRegistration=true;
    auto corePawn=Object(0x329,flagAiType,nullptr,world);components[corePawn].prepared=true;
    auto& core=components[corePawn];core.state=Object(0x400,stateType,corePawn,world);
    core.replication=Object(0x400,replicationType,corePawn,world);
    *reinterpret_cast<int*>(core.state+0x168)=15;corePawn[0x328]=0xa7;core.state[0x338]=3;
    auto coreBeforeCreates=factoryComponentCreates,coreBeforeSets=factoryAiStateSets;
    auto coreBeforeRegisters=factoryComponentRegisters;
    Need(R::PrepareChaserAiCore(corePawn,15) && core.movement && core.animation && core.kartEvents &&
         factoryComponentCreates==coreBeforeCreates+3 && factoryAiStateSets==coreBeforeSets+1 && core.state[0x321] &&
         !core.path && !core.simulation && !core.movement[0xed8] && corePawn[0x328]==0xa7 && core.state[0x338]==3,
         "Original Chaser core did not supply only missing native movement/event/animation or overwrote simulation/params/interface storage");
    Need(R::PrepareChaserAiCore(corePawn,15) && factoryComponentCreates==coreBeforeCreates+3 &&
         factoryAiStateSets==coreBeforeSets+1,"Repeated Chaser readiness duplicated components or native AI setter");
    auto coreNoWrites=[&](const char* message) {
        unsigned creates=factoryComponentCreates,sets=factoryAiStateSets;
        Need(!R::PrepareChaserAiCore(corePawn,15) && creates==factoryComponentCreates && sets==factoryAiStateSets,message);
    };
    Need(!R::PrepareChaserAiCore(corePawn,0) && !R::PrepareChaserAiCore(corePawn,16) &&
         !R::PrepareChaserAiCore(corePawn,14),"Human, out-of-range or mismatched Chaser seat entered preparation");
    core.state[0x338]=2;coreNoWrites("Intentionally disconnected Chaser bot was prepared");core.state[0x338]=0;
    core.state[0x338]=4;coreNoWrites("Unknown Chaser connection was prepared");core.state[0x338]=0;
    R::chaserItemContextEnabled=false;coreNoWrites("Disabled Chaser entered native core setup");R::chaserItemContextEnabled=true;
    R::armedMode=104;coreNoWrites("Flag mode entered Chaser native core setup");R::armedMode=108;
    R::owner=originalContextThread+1;coreNoWrites("Foreign thread entered Chaser native core setup");R::owner=originalContextThread;
    worlds[corePawn]=otherWorld;coreNoWrites("Foreign actor world entered Chaser native core setup");worlds[corePawn]=world;
    worlds[core.state]=otherWorld;coreNoWrites("Foreign state world entered Chaser native core setup");worlds[core.state]=world;
    auto savedCoreReplication=core.replication;core.replication=nullptr;
    coreNoWrites("Chaser movement was registered before its native replication prerequisite");core.replication=savedCoreReplication;
    worlds[core.replication]=otherWorld;coreNoWrites("Foreign replication was consumed by Chaser native core setup");worlds[core.replication]=world;
    *reinterpret_cast<void**>(corePawn+0x10)=aiType;coreNoWrites("Circuit pawn entered Chaser native core setup");*reinterpret_cast<void**>(corePawn+0x10)=flagAiType;
    // A model/respawn callback can recycle the pawn during registration.
    // Its old lease must reject the result before constructing more pieces.
    *reinterpret_cast<unsigned*>(core.animation+8)|=0x10000;core.animation=nullptr;
    chaserCoreExpireAfter=static_cast<int>(factoryComponentRegisters)+1;chaserCoreExpirePawn=corePawn;
    Need(!R::PrepareChaserAiCore(corePawn,15),"Expired Chaser pawn serial was accepted after native registration callback");
    chaserCoreExpireAfter=-1;chaserCoreExpirePawn=nullptr;
    Need(R::PrepareChaserAiCore(corePawn,15) && corePawn[0x328]==0xa7,
         "Fresh Chaser lease could not recover after callback lifetime transition");
    auto coreClient=Object(0x329,flagAiType,nullptr,world);auto& clientCore=components[coreClient];
    clientCore.state=Object(0x400,stateType,coreClient,world);*reinterpret_cast<int*>(clientCore.state+0x168)=15;
    clientCore.replication=Object(0x400,replicationType,coreClient,world);
    auto coreRuleTable=*reinterpret_cast<void**>(major),coreRuleOwner=*reinterpret_cast<void**>(major+0xa0);
    int coreOriginalAiCount=*reinterpret_cast<int*>(major+0x138);
    *reinterpret_cast<BYTE**>(major)=R::image+0x51dd410;*reinterpret_cast<BYTE**>(major+0xa0)=contextMode;
    *reinterpret_cast<int*>(major+0x138)=15;
    R::OnUnknown(major,coreClient,15,false);
    Need(clientCore.prepared && clientCore.movement && clientCore.animation && clientCore.kartEvents && clientCore.state[0x321] && !errors,
         "Production AddUnknownPlayer did not forward native client setup before Chaser component preparation");
    *reinterpret_cast<void**>(major)=coreRuleTable;*reinterpret_cast<void**>(major+0xa0)=coreRuleOwner;
    *reinterpret_cast<int*>(major+0x138)=coreOriginalAiCount;
    Stub(0x15c3790,reinterpret_cast<void*>(FactoryConfigureItemMode));
    auto modeCallsBefore=factoryItemModeCalls;clientCore.state[0x338]=1;
    Stub(0x3ddf4b0,reinterpret_cast<void*>(ComponentWorldFixture));rejectComponentWorldActor=coreClient;
    Need(R::PrepareChaserMovementMode(coreClient,clientCore.movement,15) && clientCore.movement[0xed8] &&
         factoryItemModeCalls==modeCallsBefore+1 && clientCore.state[0x338]==1,
         "Connected Chaser bot did not receive the original item-mode operation");
    Need(R::PrepareChaserMovementMode(coreClient,clientCore.movement,15) && factoryItemModeCalls==modeCallsBefore+1,
         "Existing Chaser item mode was configured twice");
    clientCore.movement[0xed8]=0;
    for(unsigned status:{0u,2u,3u,4u}) {
        clientCore.state[0x338]=static_cast<BYTE>(status);
        Need(!R::PrepareChaserMovementMode(coreClient,clientCore.movement,15) && !clientCore.movement[0xed8] &&
             factoryItemModeCalls==modeCallsBefore+1,"Disconnected/unconnected Chaser bot entered movement-mode configuration");
    }
    clientCore.state[0x338]=1;
    Need(!R::PrepareChaserMovementMode(coreClient,clientCore.movement,0) &&
         !R::PrepareChaserMovementMode(coreClient,clientCore.movement,14) && factoryItemModeCalls==modeCallsBefore+1,
         "Human or wrong Chaser slot entered movement-mode configuration");
    worlds[clientCore.movement]=otherWorld;
    Need(!R::PrepareChaserMovementMode(coreClient,clientCore.movement,15) && factoryItemModeCalls==modeCallsBefore+1,
         "Foreign movement world received Chaser item parameters");worlds[clientCore.movement]=world;
    R::owner=originalContextThread+1;
    Need(!R::PrepareChaserMovementMode(coreClient,clientCore.movement,15) && factoryItemModeCalls==modeCallsBefore+1,
         "Foreign thread configured Chaser item mode");R::owner=originalContextThread;
    factoryItemModeCalls=modeCallsBefore;
    rejectComponentWorldActor=nullptr;Stub(0x3ddf4b0,reinterpret_cast<void*>(World));
    puts("PASS production Chaser movement mode: original item-mode API once for a current connected bot, preserved connection, existing-mode reuse, wrong-slot/world/thread and disconnected/unconnected rejection. Synthetic callbacks only; no actual driving accepted.");
    factoryComponentCreates=coreBeforeCreates;factoryComponentRegisters=coreBeforeRegisters;factoryAiStateSets=coreBeforeSets;
    chaserCoreRegistration=false;R::construct=ContextConstruct;
    puts("PASS production Chaser AI core: post-client original component registration/reuse, native AI setter, no circuit/remote driver, unchanged parameter readiness/interface byte, all scope guards and registration-time serial expiry/retry. Synthetic callbacks only; no native movement accepted.");
    R::chaserItemContextEnabled=false;R::objectiveWorld=savedContextObjective;R::armedMode=104;
    *reinterpret_cast<BYTE**>(world+0x118)=savedContextWorldMode;*reinterpret_cast<void**>(pawns[1]+0x10)=aiType;
    puts("PASS production Chaser item-context selection: original speed-AI pawn retained, dedicated context selected only before native construction in enabled owned Chaser world, original fields/parameters preserved, one forwarding call, existing/foreign/thread isolation and circuit-registration exclusion. Synthetic native interfaces only; no item use accepted.");
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
    // Exercise the real Factory production boundary independently of 1+7.
    // No game, native rule body or result is executed by these fixtures.
    factoryModeType=Object(0x40);factoryRobotType=Object(0x40);factoryRacerType=Object(0x40);factoryAnimationType=Object(0x40);
    Stub(0x24345d0,reinterpret_cast<void*>(FactoryModeType));
    Stub(0x2433330,reinterpret_cast<void*>(FactoryRobotType));
    Stub(0x2431240,reinterpret_cast<void*>(FactoryRacerType));
    Stub(0x22b1e10,reinterpret_cast<void*>(PathType));Stub(0x22f9030,reinterpret_cast<void*>(MovementType));
    // Animation is absent on the robot fixture; the census must not invent it.
    Stub(0x23077d0,reinterpret_cast<void*>(FactoryAnimationType));
    factorySimulationType=Object(0x40);Stub(0x2402870,reinterpret_cast<void*>(FactorySimulationType));
    factoryKartEventType=Object(0x40);Stub(0x22f6900,reinterpret_cast<void*>(FactoryKartEventType));
    Stub(0x3de02a0,reinterpret_cast<void*>(FactoryTickEnabled));Stub(0x3de6fb0,reinterpret_cast<void*>(FactorySetTick));
    pawnTable[0x358/8]=fixtureImage+0x3de6fb0;pawnTable[0x368/8]=fixtureImage+0x3de02a0;
    R::factoryCompatEnabled=true;R::factoryWorld.Reset();R::factoryPopulation.Reset();
    *reinterpret_cast<void**>(gameState+0x10)=factoryModeType;
    *reinterpret_cast<int*>(major+0x130)=4;*reinterpret_cast<int*>(major+0x138)=36;
    Need(R::BindFactorySpawn(major,gameState,39),"Factory slot39 failed its independent 4/36 world binding");
    R::raceWorld.Reset();
    R::LocalRaceWorld factoryExitLease;
    Need(R::CaptureOwnedWorld(world,factoryExitLease) && factoryExitLease.Matches(world,chunks,static_cast<int>(objects.size())),
        "Factory exit confirmation captured an empty ordinary race lease");
    Need(!R::CaptureOwnedWorld(otherWorld,factoryExitLease) && factoryExitLease.Matches(world,chunks,static_cast<int>(objects.size())),
        "Foreign world replaced an already captured Factory exit lease");
    auto savedExitOwner=R::owner;R::owner=0;
    Need(!R::CaptureOwnedWorld(world,factoryExitLease),"Factory exit lease escaped its native owner thread");R::owner=savedExitOwner;
    R::raceWorld.Reset();auto savedFactorySpawn=R::spawn;R::spawn=FactorySpawnFixture;bool factoryIsAi=false;void* ignoredPawn=nullptr;
    Need(R::OnSpawn(major,gameState,39,nullptr,&ignoredPawn,&factoryIsAi) && factoryIsAi && factorySeenAndroid && R::factorySpawnSlot==-1,"Factory slot39 did not select its original robot spawn branch");
    factoryIsAi=true;
    Need(R::OnSpawn(major,gameState,3,nullptr,&ignoredPawn,&factoryIsAi) && !factoryIsAi && !factorySeenAndroid,"Factory converted a cooperative racer into an Android");R::spawn=savedFactorySpawn;
    Need(!R::BindFactorySpawn(major,gameState,40) && !R::BindFactorySpawn(major,gameState,-1),"Factory admitted an invalid high/negative slot");
    *reinterpret_cast<int*>(major+0x138)=7;Need(!R::BindFactorySpawn(major,gameState,1),"Factory borrowed the seven-AI population");
    *reinterpret_cast<int*>(major+0x138)=36;
    auto factoryRobot=Object(0x400,factoryRobotType,nullptr,world);
    auto& factoryComponents=components[factoryRobot];
    factoryComponents.path=Object(0x180,pathType,factoryRobot,world);
    factoryComponents.movement=Object(0x1000,movementType,factoryRobot,world);
    auto factoryOriginalConstruct=R::construct;R::construct=FactoryFallback;R::factorySpawnSlot=39;
    alignas(8) BYTE factoryParams[0x40]{};auto factoryValues=reinterpret_cast<BYTE**>(factoryParams);
    factoryValues[0]=pathType;factoryValues[1]=factoryRobot;
    Need(R::OnFactoryConstruct(factoryParams)==factoryComponents.path && !factoryFallbacks,"Factory failed to reuse its own native FollowTG component");
    factoryValues[0]=movementType;
    Need(R::OnFactoryConstruct(factoryParams)==factoryComponents.movement && !factoryFallbacks,"Factory duplicated native robot movement");
    R::factorySpawnSlot=3;
    Need(R::OnFactoryConstruct(factoryParams)==factoryParams && factoryFallbacks==1,"Robot component reuse changed a cooperative racer");
    R::factorySpawnSlot=39;worlds[factoryRobot]=otherWorld;
    Need(R::OnFactoryConstruct(factoryParams)==factoryParams && factoryFallbacks==2,"Factory component reuse escaped its world");
    worlds[factoryRobot]=world;
    auto factoryOriginalOwner=R::owner;R::owner=0;
    Need(R::OnFactoryConstruct(factoryParams)==factoryParams && factoryFallbacks==3,"Factory reused components off its owner thread");R::owner=factoryOriginalOwner;
    R::ObserveFactoryPawn(major,factoryRobot,39);
    Need(R::factoryPopulation.Seen()==(std::uint64_t{1}<<39) && !R::factoryPopulation.NativePopulationPresent(chunks,static_cast<int>(objects.size())),"Factory census truncated slot39 or certified a partial population");
    auto factorySeen=R::factoryPopulation.Seen();R::ObserveFactoryPawn(major,factoryRobot,0);
    Need(R::factoryPopulation.Seen()==factorySeen,"Factory counted a robot as a racer");
    auto cooperative=Object(0x400,factoryRacerType,nullptr,world);
    components[cooperative].simulation=Object(0x450,factorySimulationType,cooperative,world);
    components[cooperative].simulation[0x3b]=1;
    R::construct=FactoryComponentConstruct;Stub(0x3de4900,reinterpret_cast<void*>(FactoryComponentRegister));
    Stub(0x15c3790,reinterpret_cast<void*>(FactoryConfigureItemMode));
    Need(R::PrepareFactoryCooperativeDriver(cooperative,1) && factoryComponentCreates==4 && factoryComponentRegisters==4 && factoryItemModeCalls==1,
        "Factory cooperative racer did not receive original generic driving components");
    Need(factoryRemoteTickPauses==1 && !components[cooperative].simulation[0x3b],"Factory left remote smoothing in control of the cooperative AI rigid body");
    Need(components[cooperative].behavior && *reinterpret_cast<void**>(components[cooperative].behavior+0x10)==itemTeamType,
        "Factory cooperative racer lacks the original team-aware item AI");
    Need(R::PrepareFactoryCooperativeDriver(cooperative,1) && factoryComponentCreates==4 && factoryComponentRegisters==4 && factoryItemModeCalls==1,
        "Factory duplicated a cooperative driver's native components");
    Need(factoryRemoteTickPauses==1,"Factory needlessly repeated a disabled remote smoothing tick update");
    Need(*reinterpret_cast<void**>(cooperative+0x10)==factoryRacerType &&
        *reinterpret_cast<void**>(components[cooperative].movement+0xf08)==components[cooperative].path,
        "Factory changed the cooperative racer class or lost its native FollowTG binding");
    Need(!R::PrepareFactoryCooperativeDriver(cooperative,0) && !R::PrepareFactoryCooperativeDriver(factoryRobot,4) && factoryComponentCreates==4,
        "Factory cooperative adapter touched the human or Android branch");
    factoryStateType=Object(0x40);factoryManagerType=Object(0x40);
    Stub(0x2432080,reinterpret_cast<void*>(FactoryStateType));Stub(0x2431330,reinterpret_cast<void*>(PhaseType));
    Stub(0x2216dd0,reinterpret_cast<void*>(FactoryManagerType));
    auto factoryState=Object(0x400,factoryStateType,nullptr,world);
    *reinterpret_cast<void**>(phase+0xa0)=factoryState;*reinterpret_cast<void**>(world+0x120)=factoryState;
    R::factoryManagerBegin=FactoryManagerFixture;
    R::OnFactoryManagerBegin(rule);
    Need(factoryManagerForwards==1 && factoryManagerReadyBeforeForward==1 && factoryComponentCreates==5 && factoryComponentRegisters==5,
        "Factory native scenario initialization ran before the original AI manager existed");
    R::OnFactoryManagerBegin(rule);
    Need(factoryManagerForwards==2 && factoryManagerReadyBeforeForward==2 && factoryComponentCreates==5,
        "Factory duplicated its original state-owned AI manager");
    worlds[phase]=otherWorld;R::OnFactoryManagerBegin(rule);worlds[phase]=world;
    Need(factoryManagerForwards==3 && factoryComponentCreates==5,"Factory manager preparation escaped its phase world or suppressed native forwarding");
    *reinterpret_cast<void**>(phase+0xa0)=gameState;*reinterpret_cast<void**>(world+0x120)=gameState;
    factorySequenceType=Object(0x40);Stub(0x2294250,reinterpret_cast<void*>(FactorySequenceType));
    *reinterpret_cast<void**>(world+0x120)=factoryState;
    auto factorySequenceComponent=Object(0x200,factorySequenceType,factoryState,world);
    auto factorySequence=Object(0x40);int factoryEnvironment=73,factoryWidget=82;
    R::factoryCustomPodium=FactoryPodiumFixture;
    R::OnFactoryPodium(factorySequenceComponent,nullptr,&factoryEnvironment,&factoryWidget);
    Need(!factoryPodiumCalls,"Factory forwarded an absent losing cinematic to the null-dereferencing native routine");
    R::OnFactoryPodium(factorySequenceComponent,factorySequence,&factoryEnvironment,&factoryWidget);
    Need(factoryPodiumCalls==1 && factoryPodiumArguments[0]==factorySequenceComponent && factoryPodiumArguments[1]==factorySequence &&
        factoryPodiumArguments[2]==&factoryEnvironment && factoryPodiumArguments[3]==&factoryWidget,
        "Factory altered a valid native sequence/environment/widget call");
    worlds[factorySequenceComponent]=otherWorld;R::OnFactoryPodium(factorySequenceComponent,nullptr,&factoryEnvironment,&factoryWidget);
    Need(factoryPodiumCalls==2,"Factory cinematic guard escaped its component world");worlds[factorySequenceComponent]=world;
    *reinterpret_cast<void**>(factorySequenceComponent+0xa0)=gameState;
    R::OnFactoryPodium(factorySequenceComponent,nullptr,&factoryEnvironment,&factoryWidget);
    Need(factoryPodiumCalls==3,"Factory cinematic guard accepted a component owned by a different actor");
    *reinterpret_cast<void**>(factorySequenceComponent+0xa0)=factoryState;
    auto factoryPodiumOwner=R::owner;R::owner=0;R::OnFactoryPodium(factorySequenceComponent,nullptr,&factoryEnvironment,&factoryWidget);R::owner=factoryPodiumOwner;
    Need(factoryPodiumCalls==4,"Factory cinematic guard changed another thread's call");
    R::factoryCompatEnabled=false;R::OnFactoryPodium(factorySequenceComponent,nullptr,&factoryEnvironment,&factoryWidget);R::factoryCompatEnabled=true;
    Need(factoryPodiumCalls==5,"Disabled Factory adapter suppressed native forwarding");
    factoryIntroRuleType=Object(0x40);factoryMajorSequenceType=Object(0x40);
    Stub(0x2432c70,reinterpret_cast<void*>(FactoryIntroRuleType));Stub(0x2385f60,reinterpret_cast<void*>(FactoryMajorSequenceType));
    Stub(0x3cc28b0,reinterpret_cast<void*>(FactorySetHidden));pawnTable[0x308/8]=fixtureImage+0x3cc28b0;
    auto factoryIntroRule=Object(0x60,factoryIntroRuleType);*reinterpret_cast<void**>(factoryIntroRule+0x28)=phase;
    *reinterpret_cast<void**>(phase+0xa0)=factoryState;
    factoryMajorSequence=Object(0x200,factoryMajorSequenceType,factoryState,world);
    auto ordinaryPawns=pawns;pawns.clear();
    for(int i=0;i<TwinKartFactory::PopulationCount;++i) {
        auto pawn=Object(0x400,i<4?factoryRacerType:factoryRobotType,nullptr,world);
        auto state=Object(0x400,stateType,pawn,world);*reinterpret_cast<int*>(state+0x168)=i;components[pawn].state=state;
        pawn[0x58]|=0x20;pawns.push_back(pawn);
    }
    R::factoryIntroBegin=FactoryIntroFixture;R::factoryIntroSequence=FactoryIntroSequenceFixture;
    R::OnFactoryIntroBegin(factoryIntroRule);
    Need(factoryIntroCalls==1 && !factoryIntroSequenceCalls && factoryUnhideCalls==4 && !R::factoryIntroScope,
        "Missing Factory intro did not restore exactly four racers or leaked its scope");
    for(int i=0;i<40;++i)Need(bool(pawns[i][0x58]&0x20)==(i>=4),"Factory intro restoration changed Android visibility or left a racer hidden");
    factoryIntroMovie=Object(0x40);R::OnFactoryIntroBegin(factoryIntroRule);
    Need(factoryIntroSequenceCalls==1 && factoryUnhideCalls==4,"Factory interrupted an available native intro");
    factoryIntroMovie=nullptr;factoryIntroMakesCall=false;R::OnFactoryIntroBegin(factoryIntroRule);
    Need(factoryIntroSequenceCalls==1 && factoryUnhideCalls==8,"Factory left racers hidden when native intro could not select a local pawn");
    factoryIntroMakesCall=true;auto savedRacerSlot=*reinterpret_cast<int*>(components[pawns[3]].state+0x168);
    *reinterpret_cast<int*>(components[pawns[3]].state+0x168)=2;R::OnFactoryIntroBegin(factoryIntroRule);
    Need(factoryUnhideCalls==8,"Factory partially changed visibility before rejecting duplicate racer slots");
    *reinterpret_cast<int*>(components[pawns[3]].state+0x168)=savedRacerSlot;
    worlds[phase]=otherWorld;R::OnFactoryIntroBegin(factoryIntroRule);worlds[phase]=world;
    Need(factoryUnhideCalls==8 && factoryIntroSequenceCalls==2,"Factory intro restoration escaped its phase world or suppressed forwarding");
    pawns=ordinaryPawns;*reinterpret_cast<void**>(phase+0xa0)=gameState;
    *reinterpret_cast<void**>(world+0x120)=gameState;
    Stub(0x19ea8a0,reinterpret_cast<void*>(FactoryDedicatedRegistration));
    auto robotState=Object(0x400,stateType,factoryRobot,world);components[factoryRobot].state=robotState;
    *reinterpret_cast<int*>(robotState+0x168)=39;
    Stub(0x23b02c0,reinterpret_cast<void*>(FactorySetAiState));
    Need(R::PrepareFactoryControlledPawn(factoryRobot,39) && robotState[0x321] && factoryAiStateSets==1,
        "Factory Android retained remote-human control semantics");
    auto cooperativeState=Object(0x400,stateType,cooperative,world);components[cooperative].state=cooperativeState;
    *reinterpret_cast<int*>(cooperativeState+0x168)=1;*reinterpret_cast<int*>(cooperativeState+0x16c)=1;
    Need(R::PrepareFactoryControlledPawn(cooperative,1) && cooperativeState[0x321] &&
        *reinterpret_cast<void**>(cooperative+0x10)==factoryRacerType && *reinterpret_cast<int*>(cooperativeState+0x16c)==1,
        "Factory AI control converted its cooperative racer class/team");
    cooperativeState[0x321]=0;Need(!R::PrepareFactoryControlledPawn(cooperative,0) && !cooperativeState[0x321],"Factory overwrote human control state");
    worlds[cooperative]=otherWorld;Need(!R::PrepareFactoryControlledPawn(cooperative,1) && !cooperativeState[0x321],"Factory AI state setter escaped its world");worlds[cooperative]=world;
    factoryNativeItemType=Object(0x40);Stub(0x22d7240,reinterpret_cast<void*>(FactoryNativeItemType));
    auto factoryNativeItem=Object(0x200,factoryNativeItemType,cooperative,world);
    R::factoryCanOverlapCube=FactoryOriginalOverlap;
    Need(R::OnFactoryCanOverlapCube(factoryNativeItem) && factoryOverlapCalls==1 && !cooperative[0x300],
        "Local cooperative racer could not collect cubes without impersonating the native AI pawn class");
    factoryNativeItem[0x120]=1;Need(!R::OnFactoryCanOverlapCube(factoryNativeItem),"Factory overrode the native explicit ignore-cube policy");factoryNativeItem[0x120]=0;
    *reinterpret_cast<int*>(cooperativeState+0x168)=0;Need(!R::OnFactoryCanOverlapCube(factoryNativeItem),"Factory changed the human's original failed pickup predicate");*reinterpret_cast<int*>(cooperativeState+0x168)=1;
    worlds[factoryNativeItem]=otherWorld;Need(!R::OnFactoryCanOverlapCube(factoryNativeItem),"Factory cube eligibility escaped its item-component world");worlds[factoryNativeItem]=world;
    factoryOriginalOverlap=true;Need(R::OnFactoryCanOverlapCube(nullptr),"Factory suppressed an original successful pickup predicate");factoryOriginalOverlap=false;
    auto factorySlots=Object(0x200,itemSlotType,cooperative,world);components[cooperative].slots=factorySlots;
    factoryUseStack=Object(0x100,itemStackType,factorySlots,world);BYTE* factoryStacks[]={factoryUseStack};BYTE* factoryChunks[]={cooperative,cooperative};
    *reinterpret_cast<ItemArray*>(factorySlots+0x170)={factoryStacks,1,1};*reinterpret_cast<ItemArray*>(factoryUseStack+0xc0)={factoryChunks,2,2};
    R::event=ObserveFactoryItem;R::factoryItemUse=FactoryNativeItemUse;R::factoryItemUseObserved=0;
    R::OnFactoryItemUse(factoryNativeItem);R::OnFactoryItemUse(factoryNativeItem);
    Need(factoryUseCalls==2 && factoryConsumed==2 && factoryUseNotices==1,"Factory observer did not forward/count original item consumption exactly once per call");
    worlds[factoryNativeItem]=otherWorld;R::OnFactoryItemUse(factoryNativeItem);worlds[factoryNativeItem]=world;
    Need(factoryUseCalls==3 && factoryConsumed==2,"Factory item diagnostics escaped the native item component world");
    *reinterpret_cast<int*>(cooperativeState+0x168)=0;R::OnFactoryItemUse(factoryNativeItem);*reinterpret_cast<int*>(cooperativeState+0x168)=1;
    Need(factoryUseCalls==4 && factoryUseNotices==1,"Factory labeled the human's item use as an AI decision");
    *reinterpret_cast<ItemArray*>(factorySlots+0x170)={nullptr,1,1};R::OnFactoryItemUse(factoryNativeItem);
    Need(factoryUseCalls==5 && factoryConsumed==2,"Malformed item array changed forwarding or fabricated Factory consumption");
    R::event=nullptr;
    auto beforeRobotCreates=factoryComponentCreates,beforeRobotRegisters=factoryComponentRegisters;
    bool robotPrepared=R::PrepareFactoryRobotDriver(factoryRobot,39);
    if(!robotPrepared || factoryDedicatedRegistrations!=1 || factoryComponentCreates!=beforeRobotCreates+3 || factoryComponentRegisters!=beforeRobotRegisters+3)
        fprintf(stderr,"Factory recovery details: prepared=%d native=%u creates=%u/%u registers=%u/%u\n",robotPrepared?1:0,factoryDedicatedRegistrations,factoryComponentCreates,beforeRobotCreates,factoryComponentRegisters,beforeRobotRegisters);
    Need(robotPrepared && factoryDedicatedRegistrations==1 &&
        factoryComponentCreates==beforeRobotCreates+3 && factoryComponentRegisters==beforeRobotRegisters+3,
        "Factory robot recovery used ordinary registration or duplicated surviving FollowTG/movement");
    Need(R::PrepareFactoryRobotDriver(factoryRobot,39) && factoryDedicatedRegistrations==1,
        "Factory repeated its dedicated registration after all components were ready");
    Need(components[factoryRobot].simulation && components[factoryRobot].kartEvents && !components[factoryRobot].simulation[0x3b],
        "Factory Android lacks the original growth/event support or has competing network movement enabled");
    auto robotMovement=components[factoryRobot].movement;auto robotPath=components[factoryRobot].path;
    R::OnAutoDrive(robotMovement,false);
    Need(*reinterpret_cast<void**>(robotMovement+0xf08)==robotPath && robotMovement[0xedd]==0,
        "Factory native disable branch destroyed the original FollowTG component");
    R::OnAutoDrive(robotMovement,true);
    Need(*reinterpret_cast<void**>(robotMovement+0xf08)==robotPath && robotMovement[0xedd]==1,
        "Factory suppressed a legitimate native enable request");
    worlds[factoryRobot]=otherWorld;R::OnAutoDrive(robotMovement,false);worlds[factoryRobot]=world;
    Need(!*reinterpret_cast<void**>(robotMovement+0xf08),"Factory FollowTG preservation escaped its actor world");
    TwinKartFactory::Population completeFactory;
    for(int i=0;i<TwinKartFactory::PopulationCount;++i) {
        auto object=Object(0x40);TwinKartCamera::ObjectIdentity id{};
        Need(TwinKartCamera::Capture(object,chunks,static_cast<int>(objects.size()),&id),"Factory census identity fixture");
        Need(completeFactory.Observe(i,TwinKartFactory::SlotRole(i,4,36),id,chunks,static_cast<int>(objects.size()),true),"Factory census rejected a distinct native identity");
        if(i==39) {
            Need(completeFactory.NativePopulationPresent(chunks,static_cast<int>(objects.size())),"Factory forty-pawn census still used eight/32-bit capacity");
            R::factoryPopulation=completeFactory;R::factoryRosterReady=true;R::factoryStartRequested=false;R::factoryCooperativeMask=0;
            playerWait[0xc8]=playerWait[0xc9]=0;unsigned beforeFactoryStart=forceBeginCalls;
            Need(!R::ForceFactoryStartWhenReady(major) && forceBeginCalls==beforeFactoryStart,"Factory bypassed missing cooperative drivers");
            R::factoryCooperativeMask=14;
            Need(R::ForceFactoryStartWhenReady(major) && forceBeginCalls==beforeFactoryStart+1 && playerWait[0xc9],"Factory did not use original local readiness after forty native members");
            Need(R::ForceFactoryStartWhenReady(major) && forceBeginCalls==beforeFactoryStart+1,"Factory submitted duplicate readiness notices");
            ++objectSlots[id.index].serial;
            Need(!completeFactory.NativePopulationPresent(chunks,static_cast<int>(objects.size())),"Factory census accepted a recycled native slot");
            Need(!completeFactory.Observe(39,TwinKartFactory::Role::Android,{object,id.index,id.serial+1},chunks,static_cast<int>(objects.size()),true),"Factory silently replaced an expired member identity");
            R::factoryStartRequested=false;Need(!R::ForceFactoryStartWhenReady(major) && forceBeginCalls==beforeFactoryStart+1,"Factory admitted readiness with an expired native member");
        }
    }
    // Exercise the native inverse resource lookup ABI with independently
    // authored fixture functions, never a vendor asset or native body.
    Stub(0x2589130,reinterpret_cast<void*>(ResourcePathName));Stub(0x3bcd370,reinterpret_cast<void*>(ResourceNameText));
    Stub(0x1444fb0,reinterpret_cast<void*>(ResourceRoundTrip));Stub(0x1000,reinterpret_cast<void*>(ResourceIdentity));
    pawnTable[0x2f8/8]=fixtureImage+0x1000;auto resourceManager=Object(0x200);
    TwinKartRace::AiAssetLookup resourceLookup{fixtureImage,resourceManager,902};wchar_t resourceId[97]=L"unchanged";unsigned beforeResourceFrees=frees;
    Need(TwinKartRace::RegisteredIdentityForPath(resourceLookup,L"/Game/Test/OwnedNpc.OwnedNpc",resourceId,ARRAYSIZE(resourceId)) &&
        !wcscmp(resourceId,L"Kart:499000001") && frees==beforeResourceFrees+1,"Native asset identity was not round-tripped or its original text buffer leaked");
    wcscpy_s(resourceId,L"unchanged");resourceRoundTrip=903;
    Need(!TwinKartRace::RegisteredIdentityForPath(resourceLookup,L"/Game/Test/OwnedNpc.OwnedNpc",resourceId,ARRAYSIZE(resourceId)) &&
        !wcscmp(resourceId,L"unchanged") && frees==beforeResourceFrees+2,"Wrong NPC resource path was accepted or partially replaced the selected identity");
    resourceRoundTrip=901;resourceType=999;
    Need(!TwinKartRace::RegisteredIdentityForPath(resourceLookup,L"/Game/Test/OwnedNpc.OwnedNpc",resourceId,ARRAYSIZE(resourceId)) &&
        frees==beforeResourceFrees+2,"Foreign primary asset type reached NPC identity assignment");resourceType=902;
    // The explicit objective dependency operation must load only a registered
    // path, preserve caller identifiers and free both independent path strings.
    resourceLoaded=resourceManager;resourceAllocatedEmpty=true;
    Stub(0x923880,reinterpret_cast<void*>(ResourceStringCopy));Stub(0xa42300,reinterpret_cast<void*>(ResourceSynchronousLoad));
    Stub(0x146c720,reinterpret_cast<void*>(ResourceResolve));
    const wchar_t assetId[]=L"Kart:499000001";TwinKartEquipment::String assetIdentifier{assetId,static_cast<int>(ARRAYSIZE(assetId)),static_cast<int>(ARRAYSIZE(assetId))};
    auto dependencyFrees=frees;
    Need(TwinKartRace::LoadObjectiveRegisteredAsset(resourceLookup,assetIdentifier) && resourceLoadCalls==1 && frees==dependencyFrees+2 && !errors,
        "Objective dependency load lost soft-reference ABI, native identity verification or owned string cleanup");
    resourceLoadFailure=true;
    Need(!TwinKartRace::LoadObjectiveRegisteredAsset(resourceLookup,assetIdentifier) && resourceLoadCalls==2 && frees==dependencyFrees+4,
        "Failed objective dependency load was accepted or leaked path ownership");resourceLoadFailure=false;
    resourceResolveFailure=true;
    Need(!TwinKartRace::LoadObjectiveRegisteredAsset(resourceLookup,assetIdentifier) && resourceLoadCalls==3 && frees==dependencyFrees+6,
        "Loaded object without the matching original asset identity was accepted");resourceResolveFailure=false;
    resourceRoundTrip=0;
    Need(!TwinKartRace::LoadObjectiveRegisteredAsset(resourceLookup,assetIdentifier) && resourceLoadCalls==3 && frees==dependencyFrees+7,
        "Unregistered dependency reached synchronous loading or leaked its path");resourceRoundTrip=901;resourceAllocatedEmpty=false;
    auto member=reinterpret_cast<TwinKartRace::ObjectiveSoftReference*>(resourceManager+0x40);*member={};
    const wchar_t emptySubpath[]=L"";member->path.name=901;member->path.subpath={emptySubpath,1,1};
    auto memberFrees=frees;
    Need(TwinKartRace::LoadObjectiveSoftMember(fixtureImage,resourceManager,0x40,true) && resourceLoadCalls==4 && frees==memberFrees &&
        member->path.subpath.data==emptySubpath,"Borrowed native model property was freed, replaced or not loaded");
    member->path.name=0;
    Need(TwinKartRace::LoadObjectiveSoftMember(fixtureImage,resourceManager,0x40,false) &&
        !TwinKartRace::LoadObjectiveSoftMember(fixtureImage,resourceManager,0x40,true) && resourceLoadCalls==4,
        "Empty optional model property caused a load or a missing required mesh was accepted");
    member->path.name=901;member->path.subpath.count=10000;
    Need(!TwinKartRace::LoadObjectiveSoftMember(fixtureImage,resourceManager,0x40,true) && resourceLoadCalls==4 && frees==memberFrees,
        "Malformed model property reached the native loader or borrowed storage cleanup");
    TwinKartEquipment::Preset objectivePreset{};
    objectivePreset.kart[0]=objectivePreset.character[0]=objectivePreset.character[1]=assetIdentifier;
    const wchar_t motionId[]=L"FinishMotion:700000001";
    for(unsigned field=2;field<6;++field)objectivePreset.character[field]={motionId,static_cast<int>(ARRAYSIZE(motionId)),static_cast<int>(ARRAYSIZE(motionId))};
    auto preservedObjectivePreset=objectivePreset;auto beforePresetLoads=resourceLoadCalls;
    resourceAllocatedEmpty=true;TwinKartRace::ObjectiveModelFailure assetFailure;
    Need(TwinKartRace::LoadObjectivePresetAssets(resourceLookup,objectivePreset,&assetFailure) &&
        resourceLoadCalls==beforePresetLoads+3 && !memcmp(&preservedObjectivePreset,&objectivePreset,sizeof(objectivePreset)) && !errors,
        "Native motion IDs blocked the human model or preparation changed original animation descriptors");
    resourceLoadFailure=true;
    Need(!TwinKartRace::LoadObjectivePresetAssets(resourceLookup,objectivePreset,&assetFailure) &&
        assetFailure.group==0 && assetFailure.field==0 && assetFailure.required,
        "A missing body was accepted after separating optional finishing motions");
    resourceLoadFailure=false;resourceAllocatedEmpty=false;
    R::construct=factoryOriginalConstruct;R::Arm(1);R::factoryCompatEnabled=false;
    Need(!R::factoryPopulation.Seen() && R::factorySpawnSlot==-1,"Ordinary travel retained Factory population state");
    puts("PASS production Factory boundary: distinct 4/36 roles, slot39/40 bounds, native component reuse/forwarding, owner-thread/world isolation and forty serial identities. No Factory gameplay acceptance.");
    R::armed=false;before=queries;R::OnActivationBegin(rule);Need(queries==before && !errors,"Unarmed native forwarding changed preparation");
    auto savedSpawn=R::spawn;auto savedClassifier=R::chaserSpawnClassifier;int savedMode=R::armedMode;
    R::spawn=FactorySpawnFixture;R::chaserSpawnClassifier=ChaserClassifierFixture;R::armedMode=108;
    unsigned beforeSpawn=spawnFixtureCalls;bool selectedAi=false;
    Need(R::OnSpawn(nullptr,nullptr,7,nullptr,nullptr,&selectedAi) && selectedAi && factorySeenAndroid &&
         chaserClassifierCalls==1 && spawnFixtureCalls==beforeSpawn+1,
         "Chaser classification did not precede exactly one original native spawn");
    selectedAi=true;beforeSpawn=spawnFixtureCalls;
    Need(R::OnSpawn(nullptr,nullptr,0,nullptr,nullptr,&selectedAi) && !selectedAi && !factorySeenAndroid &&
         chaserClassifierCalls==2 && spawnFixtureCalls==beforeSpawn+1,"Local Chaser human was classified as a bot");
    R::armedMode=4;selectedAi=true;beforeSpawn=spawnFixtureCalls;
    Need(R::OnSpawn(nullptr,nullptr,7,nullptr,nullptr,&selectedAi) && selectedAi && factorySeenAndroid &&
         chaserClassifierCalls==2 && spawnFixtureCalls==beforeSpawn+1,"Chaser classifier changed an ordinary-mode spawn");
    R::chaserSpawnClassifier=nullptr;R::armedMode=108;selectedAi=false;beforeSpawn=spawnFixtureCalls;
    Need(R::OnSpawn(nullptr,nullptr,7,nullptr,nullptr,&selectedAi) && !selectedAi && !factorySeenAndroid &&
         chaserClassifierCalls==2 && spawnFixtureCalls==beforeSpawn+1,"An absent Chaser classifier changed native spawning");
    R::spawn=savedSpawn;R::chaserSpawnClassifier=savedClassifier;R::armedMode=savedMode;
    R::raceWorld.Reset();VirtualFree(fixtureImage,0,MEM_RELEASE);fixtureImage=nullptr;
    puts("PASS production local AI preparation and settlement boundary with original synthetic native interfaces: seven speed/item AI, client-first and async-spline ordering, native forwarding, world isolation, temporary-array cleanup, mode-aware missing-style fallback and preserved result/provider ownership. No game code, driving or fabricated results executed.");return 0;
}catch(const std::exception& e){if(fixtureImage)VirtualFree(fixtureImage,0,MEM_RELEASE);fprintf(stderr,"%s\n",e.what());return 1;}}
