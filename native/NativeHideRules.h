#pragma once
#include <cstdint>
#include <cmath>
#include <cwchar>
#include "FlagArenaDriver.h"
namespace TwinKartObjective {
struct HideRoles {int hiders=0,seekers=0;};
// Observed FHiderAndSeekerCount (two Int32 fields), within the native
// TMap<Int32,FHiderAndSeekerCount> sparse element. Hash links are not roles.
struct HideRoleEntry {int players;HideRoles roles;int hashNext,hashIndex;};
static_assert(sizeof(HideRoleEntry)==20,"Exact original role-map element");
struct HideRoleMap {
    const HideRoleEntry* data;int count,capacity;
    std::uint32_t inlineBits[4];const std::uint32_t* secondaryBits;
    int bitCount,bitCapacity,firstFree,freeCount;
    std::uint64_t inlineHash;const void* secondaryHash;int hashSize,pad;
};
static_assert(sizeof(HideRoleMap)==80,"Exact supported PC sparse-map header");
struct HidePhaseEntry {std::uint64_t name;void* phase;int hashNext,hashIndex;};
struct HidePhaseMap {
    const HidePhaseEntry* data;int count,capacity;
    std::uint32_t inlineBits[4];const std::uint32_t* secondaryBits;
    int bitCount,bitCapacity,firstFree,freeCount;
    std::uint64_t inlineHash;const void* secondaryHash;int hashSize,pad;
};
static_assert(sizeof(HidePhaseEntry)==24 && sizeof(HidePhaseMap)==80,"Original phase-map observation ABI");
inline bool HidePhaseInitializationReady(const HidePhaseMap& map,std::uint64_t setup,std::uint64_t grant,
                                        void*& setupPhase,void*& grantPhase) {
    if(!setup || !grant || setup==grant || !map.data || map.count<2 || map.count>64 || map.capacity<map.count ||
       map.capacity>128 || map.secondaryBits || map.bitCount!=map.count || map.bitCapacity<map.count ||
       map.bitCapacity>128 || map.freeCount<0 || map.freeCount>map.count)return false;
    void* first=nullptr;void* second=nullptr;int occupied=0;
    for(int index=0;index<map.count;++index)if(map.inlineBits[index/32]&(1u<<(index%32))) {
        ++occupied;const auto& row=map.data[index];
        if(row.name==setup){if(first || !row.phase)return false;first=row.phase;}
        if(row.name==grant){if(second || !row.phase)return false;second=row.phase;}
    }
    if(occupied!=map.count-map.freeCount || !first || !second || first==second)return false;
    setupPhase=first;grantPhase=second;return true;
}
inline bool ReadHideRoles(const HideRoleMap& map,int players,HideRoles& output) {
    if(players<1 || players>16 || !map.data || map.count<1 || map.count>128 || map.capacity<map.count || map.capacity>1024 ||
       map.bitCount!=map.count || map.bitCapacity<map.bitCount || map.bitCapacity>128 || map.secondaryBits ||
       map.freeCount<0 || map.freeCount>map.count)return false;
    bool found=false;HideRoles value{};int occupied=0;unsigned seen=0;
    for(int index=0;index<map.count;++index) {
        if(!(map.inlineBits[index/32]&(std::uint32_t{1}<<(index%32))))continue;
        ++occupied;const auto& row=map.data[index];
        if(row.players<1 || row.players>16 || (seen&(1u<<row.players)) || row.roles.hiders<0 || row.roles.seekers<0 ||
           row.roles.hiders>16 || row.roles.seekers>16 || row.roles.hiders+row.roles.seekers!=row.players)return false;
        seen|=1u<<row.players;if(row.players==players){value=row.roles;found=true;}
    }
    if(!found || occupied!=map.count-map.freeCount)return false;
    output=value;return true;
}
inline bool CompetitiveHideRoles(const HideRoles& roles,int players) {
    return players>=2 && players<=16 && roles.hiders>0 && roles.hiders<=16 && roles.seekers>0 && roles.seekers<=16 && roles.hiders+roles.seekers==players;
}
inline void* HideSessionClass(bool ownedRequest,bool templateObject,void* current,void* base,void* local) {
    // A caller-specified or nonstandard session is never overwritten.
    return ownedRequest && !templateObject && current && current==base && local && local!=base?local:current;
}
inline int HidePreparationSeconds(float configured,float target,float elapsed) {
    if(!std::isfinite(configured) || configured<1.f || configured>300.f || target!=configured ||
       !std::isfinite(elapsed) || elapsed<0.f || elapsed>=target)return 0;
    return static_cast<int>(std::ceil(target-elapsed));
}
// Keep the camera on the near side of an observed obstruction. The native
// desired rotation and FOV stay untouched, including when the ray is clear.
inline bool ClipHideCamera(const TwinKartRace::ArenaPoint& pivot,TwinKartRace::ArenaPoint& eye,float hitTime) {
    using namespace TwinKartRace;
    if(!Finite(pivot) || !Finite(eye) || !std::isfinite(hitTime) || hitTime<=0.f || hitTime>1.f)return false;
    const ArenaPoint arm{eye.x-pivot.x,eye.y-pivot.y,eye.z-pivot.z};
    const float length=std::sqrt(arm.x*arm.x+arm.y*arm.y+arm.z*arm.z);
    if(!std::isfinite(length) || length<40.f || length>2400.f)return false;
    const float travel=(std::max)(0.f,hitTime-24.f/length);
    eye={pivot.x+travel*arm.x,pivot.y+travel*arm.y,pivot.z+travel*arm.z};return true;
}
// Fit a real disguise's bounds in the local view without changing its mesh.
// Prefer the original camera side. If that entire side is blocked, try
// across and to either side of the prop, including a steep overhead view.
inline constexpr unsigned HideDisguiseViewCount=7;
// The retained client provides these four real disguise blueprints.
inline int HideDisguiseKind(const wchar_t* className) {
    if(!className)return 0;
    constexpr const wchar_t* classes[]{L"BP_PalmTreeProp_C",L"BP_FishStockProp_C",L"BP_SignProp_C",L"BP_CornProp_C"};
    for(int i=0;i<4;++i)if(std::wcscmp(classes[i],className)==0)return i+1;
    return 0;
}
// A tall disguise can need more camera distance than the market has room
// for. Keep its visible trunk in frame using the real child actor's origin,
// and fit the full model for smaller props.
inline bool FitHideDisguiseTarget(int kind,const TwinKartRace::ArenaPoint& pawn,const TwinKartRace::ArenaPoint& child,
    TwinKartRace::ArenaPoint& center,TwinKartRace::ArenaPoint& extent) {
    using namespace TwinKartRace;
    if(!Finite(pawn) || !Finite(child) || !Finite(center) || !Finite(extent) ||
       ArenaDistance(pawn,child)>450.f || std::fabs(pawn.z-child.z)>600.f ||
       extent.x<=0 || extent.y<=0 || extent.z<=0)return false;
    if(kind==1 && (extent.x>450.f || extent.y>450.f || extent.z>550.f ||
       ArenaDistance(center,pawn)>=1000.f || std::fabs(center.z-pawn.z)>=1800.f)) {
        center={child.x,child.y,child.z+280.f};
        extent={(std::min)(extent.x,260.f),(std::min)(extent.y,260.f),(std::min)(extent.z,400.f)};
    }
    return ArenaDistance(center,pawn)<1000.f && std::fabs(center.z-pawn.z)<1800.f;
}
inline bool FrameHideDisguise(const TwinKartRace::ArenaPoint& center,const TwinKartRace::ArenaPoint& extent,
    const TwinKartRace::ArenaPoint& stockEye,float stockYaw,float fov,unsigned attempt,
    TwinKartRace::ArenaPoint& eye,TwinKartRace::ArenaPoint& rotation) {
    using namespace TwinKartRace;
    if(!Finite(center) || !Finite(extent) || !Finite(stockEye) || !std::isfinite(stockYaw) ||
       !std::isfinite(fov) || fov<35.f || fov>130.f || extent.x<=0.f || extent.y<=0.f || extent.z<=0.f ||
       extent.x>1000.f || extent.y>1000.f || extent.z>1600.f || attempt>=HideDisguiseViewCount)return false;
    constexpr float rad=.01745329252f;
    float x=stockEye.x-center.x,y=stockEye.y-center.y;
    float side=std::sqrt(x*x+y*y);
    if(!std::isfinite(side))return false;
    if(side<20.f || side>2600.f){x=-std::cos(stockYaw*rad);y=-std::sin(stockYaw*rad);}
    else{x/=side;y/=side;}
    const float radius=std::sqrt(extent.x*extent.x+extent.y*extent.y+extent.z*extent.z);
    if(radius<12.f)return false;
    // Use a conservative portrait-safe angle so the prop does not fill the
    // view, including when the player is using a relatively narrow FOV.
    const float distance=(std::max)(360.f,radius/(std::max)(.22f,std::sin(fov*rad*.28f)));
    if(!std::isfinite(distance) || distance>3000.f)return false;
    switch(attempt){
    case 3:x=-x;y=-y;break;
    case 4:{float old=x;x=-y;y=old;break;}
    case 5:{float old=x;x=y;y=-old;break;}
    }
    constexpr float horizontal[]{.91f,.66f,.36f,.56f,.56f,.56f,.12f};
    constexpr float vertical[]{.42f,.75f,.93f,.83f,.83f,.83f,.993f};
    eye={center.x+x*distance*horizontal[attempt],center.y+y*distance*horizontal[attempt],
         center.z+distance*vertical[attempt]};
    rotation={-std::atan2(vertical[attempt],horizontal[attempt])/rad,std::atan2(-y,-x)/rad,0.f};
    return Finite(eye) && Finite(rotation);
}
// Owned captures of the retained prototype do not schedule Config.Game. This
// compatibility clock begins only at the actual original search callback;
// loading, role selection and the escape period cannot consume this budget.
// It requests the existing end condition, never a winner or a score.
class HideSearchClock {
    const void* owner=nullptr;
    unsigned long long generation=0;
    double elapsed=0,limit=0;
    bool active=false;
public:
    bool Begin(const void* rules,unsigned long long serial,double seconds) {
        if(!rules || !serial || !std::isfinite(seconds) || seconds<=0 || seconds>3600)return false;
        if(active && owner==rules && generation==serial)return false;
        owner=rules;generation=serial;elapsed=0;limit=seconds;active=true;return true;
    }
    void Stop(){owner=nullptr;generation=0;elapsed=limit=0;active=false;}
    int RemainingSeconds() const {return active && elapsed<limit?static_cast<int>(std::ceil(limit-elapsed)):0;}
    bool Advance(const void* rules,unsigned long long serial,double delta) {
        if(!active)return false;
        if(rules!=owner || serial!=generation){Stop();return false;}
        if(!std::isfinite(delta) || delta<=0 || delta>5)return false;
        elapsed+=delta;if(elapsed<limit)return false;
        active=false;return true;
    }
};
struct HideSeat {std::uintptr_t controller=0,pawn=0;};
inline bool HideSeatSet(const HideSeat* seats,int count,bool requirePawns) {
    if(!seats || count!=8)return false;
    for(int i=0;i<count;++i) {
        if(!seats[i].controller || (requirePawns && !seats[i].pawn))return false;
        for(int j=0;j<i;++j)
            if(seats[j].controller==seats[i].controller || (seats[i].pawn && seats[j].pawn==seats[i].pawn))return false;
    }
    return true;
}
// Retire only a previous role pawn that no controller retained. Validate the
// entire replacement roster first; incomplete spawn must preserve every pawn.
inline bool RetiredHidePawn(const HideSeat* before,const HideSeat* after,int count,int index) {
    if(index<0 || index>=count || !HideSeatSet(before,count,false) || !HideSeatSet(after,count,true) || !before[index].pawn)return false;
    for(int i=0;i<count;++i) {
        bool found=false;
        for(int j=0;j<count;++j)if(before[i].controller==after[j].controller)found=true;
        if(!found || after[i].pawn==before[index].pawn)return false;
    }
    return true;
}
inline bool UncontrolledHidePawn(std::uintptr_t pawn,const HideSeat* actual,int count) {
    if(!pawn || !HideSeatSet(actual,count,false))return false;
    for(int i=0;i<count;++i)if(actual[i].pawn==pawn)return false;
    return true;
}
inline int HideDisguiseChoice(std::uint32_t entropy,int count,int capacity) {
    return count>0 && count<=32 && capacity>=count && capacity<=256?static_cast<int>(entropy%static_cast<unsigned>(count)):-1;
}
// Pinned APlayerController.Player boundary. An unconnected local bot has no
// UPlayer; an actual local player has one. Callers validate native identity,
// world and possession before borrowing this field.
inline bool HideControllerHasPlayer(const void* controller) {
    return controller && *reinterpret_cast<void* const*>(static_cast<const unsigned char*>(controller)+0x298)!=nullptr;
}
inline int HideInitialDisguiseChoice(std::uint32_t entropy,int count,int capacity,const void* controller,
    bool searchActive,bool selectedInfo,bool childActor) {
    if(!controller)return -1;
    bool human=HideControllerHasPlayer(controller);
    // Preserve a chosen prop even while its original child is still loading.
    // A human gets the native selection UI's entire preparation phase first.
    if(selectedInfo || childActor || (human && !searchActive))return -1;
    return HideDisguiseChoice(human?0u:entropy,count,capacity);
}
struct HideObservation {TwinKartRace::ArenaPoint position{};bool hider=false,valid=false;};
// A patrol is not abandoned just because its travel takes four seconds. Real
// motion, including a road detour away from the direct goal, renews progress.
// A bounded overall lifetime still recovers loops; this never moves a pawn.
class HidePatrolProgress {
    TwinKartRace::ArenaPoint goal{},milestone{};
    std::uint64_t began=0,advanced=0;
    bool known=false;
public:
    void Reset(){*this=HidePatrolProgress{};}
    bool Renew(const TwinKartRace::ArenaPoint& position,const TwinKartRace::ArenaPoint& target,std::uint64_t now) {
        if(!TwinKartRace::Finite(position) || !TwinKartRace::Finite(target))return true;
        if(!known || now<began || now<advanced || TwinKartRace::ArenaDistance(goal,target)>50 ||
           std::fabs(goal.z-target.z)>50) {
            known=true;goal=target;milestone=position;began=advanced=now;
        }
        if(TwinKartRace::ArenaDistance(position,target)<200)return true;
        if(TwinKartRace::ArenaDistance(position,milestone)>=120){milestone=position;advanced=now;}
        return now-advanced>=6000 || now-began>=30000;
    }
};
inline bool HideContactChannels(int hider,int seeker) {
    // Only the original Pawn object channel on a real role pawn. Never
    // reconfigure responses to floor/wall object channels. Original Seekers
    // also use Pawn, so role instances intentionally overlap one another.
    return hider==2 && seeker>0 && seeker<32 && seeker!=17;
}
// Original local decision policy. The caller supplies real native positions
// and sampled, reachable road candidates. It does not supply a win or score.
inline float HideSafety(const TwinKartRace::ArenaPoint& candidate,const HideObservation* seats,int count) {
    if(!TwinKartRace::Finite(candidate) || !seats || count!=8)return -1;
    float nearest=1.e20f;
    for(int i=0;i<count;++i)if(seats[i].valid && !seats[i].hider && TwinKartRace::Finite(seats[i].position))
        nearest=(std::min)(nearest,TwinKartRace::ArenaDistance(candidate,seats[i].position));
    return nearest;
}
inline int HideMovingTarget(const HideObservation* seats,const HideObservation* previous,int count,int seeker) {
    if(!seats || !previous || count!=8 || seeker<0 || seeker>=count || !seats[seeker].valid || seats[seeker].hider ||
       !TwinKartRace::Finite(seats[seeker].position))return -1;
    int found=-1;float best=3600;
    for(int i=0;i<count;++i)if(seats[i].valid && previous[i].valid && seats[i].hider && previous[i].hider &&
       TwinKartRace::Finite(seats[i].position) && TwinKartRace::Finite(previous[i].position)) {
        float d=TwinKartRace::ArenaDistance(seats[i].position,seats[seeker].position);
        // Motion or very close inspection reveals a disguise. A stationary
        // distant prop is not automatically treated as a known opponent.
        if(d<best && (d<250 || TwinKartRace::ArenaDistance(seats[i].position,previous[i].position)>8)) {
            found=i;best=d;
        }
    }
    return found;
}
}
