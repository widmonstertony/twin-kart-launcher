#pragma once
#include <cstdint>
namespace TwinKartObjective {
// Presentation of an already-decided native result, never a win condition.
// Values come from this client's ERaceTeamType: None=5 is not a result.
struct PrototypeSettlement {
    std::uintptr_t world=0;int index=-1,serial=0;unsigned winner=5;
    bool Valid()const{return world && index>=0 && serial>0 && winner<=4;}
    bool SameWorld(const PrototypeSettlement& other)const {
        return world==other.world && index==other.index && serial==other.serial;
    }
};
class PrototypeSettlementMailbox {
    PrototypeSettlement current{};bool available=false,shown=false;
public:
    // The owning adapter supplies synchronization and native ownership checks.
    bool Publish(const PrototypeSettlement& value) {
        if(!value.Valid())return false;
        if(current.SameWorld(value))return false;
        current=value;available=true;shown=false;return true;
    }
    bool Take(PrototypeSettlement& value) {
        if(!available || shown)return false;
        value=current;available=false;shown=true;return true;
    }
    bool Restore(const PrototypeSettlement& value) {
        if(!value.Valid() || !shown || available || !current.SameWorld(value) || current.winner!=value.winner)return false;
        available=true;shown=false;return true;
    }
    void Reset(){current={};available=shown=false;}
};
inline const wchar_t* PrototypeWinnerText(unsigned winner) {
    switch(winner) {
    case 0:return L"\u4e2a\u4eba";
    case 1:return L"\u7ea2\u961f";
    case 2:return L"\u84dd\u961f";
    case 3:return L"\u7eff\u961f";
    case 4:return L"\u9ec4\u961f";
    default:return nullptr;
    }
}
// The game thread observes the current owned world. The worker consumes only
// copied identities and grants; it never follows a UObject pointer itself.
struct PrototypePresentationWorld {
    std::uintptr_t world=0;int index=-1,serial=0;unsigned mode=0;
    bool Valid()const{return world && index>=0 && serial>0 && (mode==108 || mode==109);}
    bool Same(const PrototypePresentationWorld& other)const {
        return world==other.world && index==other.index && serial==other.serial && mode==other.mode;
    }
};
struct PrototypePresentationGrant {
    PrototypePresentationWorld world{};std::uint64_t generation=0;
    bool Same(const PrototypePresentationGrant& other)const {
        return generation==other.generation && world.Same(other.world);
    }
};
enum class PrototypeReturnOutcome {Idle,Pending,Submitted,Stale,Failed};
// The owning adapter synchronizes this bounded state with its existing lock.
// Only one return may be queued. Its acknowledgment can survive native cleanup
// invalidating the presentation grant during that very return operation.
class PrototypePresentationState {
    PrototypePresentationWorld observed{};
    std::uint64_t generation=1,nextRequest=0,returnRequest=0;
    PrototypePresentationGrant returning{};
    PrototypeReturnOutcome outcome=PrototypeReturnOutcome::Idle;
public:
    std::uint64_t Generation()const{return generation;}
    bool Observe(PrototypePresentationWorld current) {
        if(!current.Valid())current={};
        if(observed.Same(current))return false;
        observed=current;++generation;return true;
    }
    void Reset(){observed={};++generation;}
    PrototypePresentationGrant Qualify(const PrototypePresentationWorld& value)const {
        return value.Valid() && observed.Same(value)?PrototypePresentationGrant{value,generation}:PrototypePresentationGrant{};
    }
    bool Current(const PrototypePresentationGrant& grant)const {
        return grant.generation==generation && grant.world.Valid() && observed.Same(grant.world);
    }
    bool BeginReturn(const PrototypePresentationGrant& grant,std::uint64_t& request) {
        if(!Current(grant) || outcome==PrototypeReturnOutcome::Pending)return false;
        returning=grant;returnRequest=++nextRequest;outcome=PrototypeReturnOutcome::Pending;request=returnRequest;return true;
    }
    bool CompleteReturn(const PrototypePresentationGrant& grant,std::uint64_t request,PrototypeReturnOutcome result) {
        if(outcome!=PrototypeReturnOutcome::Pending || request!=returnRequest || !returning.Same(grant) ||
           (result!=PrototypeReturnOutcome::Submitted && result!=PrototypeReturnOutcome::Stale && result!=PrototypeReturnOutcome::Failed))return false;
        outcome=result==PrototypeReturnOutcome::Failed && !Current(grant)?PrototypeReturnOutcome::Stale:result;return true;
    }
    PrototypeReturnOutcome ReturnOutcome(const PrototypePresentationGrant& grant)const {
        if(returning.Same(grant) && outcome!=PrototypeReturnOutcome::Idle)return outcome;
        return Current(grant)?PrototypeReturnOutcome::Idle:PrototypeReturnOutcome::Stale;
    }
};
struct HideMatchSummary {
    std::uintptr_t world=0;int index=-1,serial=0,rounds=0;
    unsigned char outcomes[16]{}; // 1=original time limit, 2=original no-hiders condition
    bool SameWorld(const HideMatchSummary& other)const{return world==other.world && index==other.index && serial==other.serial;}
    bool Valid()const {
        if(!world || index<0 || serial<=0 || rounds<1 || rounds>16)return false;
        for(int i=0;i<rounds;++i)if(outcomes[i]!=1 && outcomes[i]!=2)return false;
        return true;
    }
};
// Read-only journal of ORIGINAL search-end conditions. The main FProgram's
// actual terminal key and round counter, not our clock/wall time, admit UI.
// There is no invented aggregate winner when native rounds swap roles.
class HideSettlementJournal {
    HideMatchSummary current{};bool finished=false;
public:
    void Reset(){current={};finished=false;}
    bool Record(std::uintptr_t world,int index,int serial,int rounds,int round,bool nativeTimeout,bool nativeNoHiders) {
        if(!world || index<0 || serial<=0 || rounds<1 || rounds>16 || round<0 || round>=rounds ||
           (!nativeTimeout && !nativeNoHiders) || finished)return false;
        HideMatchSummary identity{world,index,serial,rounds};
        if(current.world && (!current.SameWorld(identity) || current.rounds!=rounds))return false;
        if(!current.world)current=identity;
        if(current.outcomes[round])return false;
        // The no-hiders condition remains decisive if it coincides with timeout.
        current.outcomes[round]=nativeNoHiders?2:1;return true;
    }
    bool Finish(std::uintptr_t world,int index,int serial,unsigned nativeKey,int nativeRound,int nativeRounds,HideMatchSummary& result) {
        HideMatchSummary identity{world,index,serial,nativeRounds};
        if(finished || nativeKey!=0x80000001u || !current.SameWorld(identity) || nativeRounds!=current.rounds ||
           nativeRound!=nativeRounds || !current.Valid())return false;
        result=current;finished=true;return true;
    }
};
}
