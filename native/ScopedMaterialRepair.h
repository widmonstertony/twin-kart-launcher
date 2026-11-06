#pragma once
#include <cstdint>
namespace TwinKartVisual {
struct MaterialSlot {std::uint64_t name=0;void* material=nullptr;void* overrideValue=nullptr;bool verifiedLoaderFallback=false;};
struct MaterialRepairEntry {unsigned index=0;std::uint64_t name=0;void* replacement=nullptr;};
struct MaterialRepairPlan {MaterialRepairEntry entries[32]{};unsigned count=0;};
enum class MaterialPlanStatus {Invalid,Unchanged,Ready};
inline bool UniqueMaterialSlots(const MaterialSlot* slots,unsigned count) {
    if(!slots || !count || count>32)return false;
    for(unsigned i=0;i<count;++i) {
        if(!slots[i].name)return false;
        for(unsigned j=0;j<i;++j)if(slots[i].name==slots[j].name)return false;
    }
    return true;
}
// Match original slot identities, never array order, material-name prefixes or
// geometric similarity. Preserve healthy defaults and every explicit override;
// only a caller-verified loader fallback can be treated as a missing default.
inline MaterialPlanStatus PlanMissingMaterials(const MaterialSlot* target,unsigned targetCount,
    const MaterialSlot* source,unsigned sourceCount,MaterialRepairPlan& output) {
    output={};
    if(!UniqueMaterialSlots(target,targetCount) || !UniqueMaterialSlots(source,sourceCount))return MaterialPlanStatus::Invalid;
    MaterialRepairPlan plan;
    for(unsigned i=0;i<targetCount;++i) {
        if((target[i].material && !target[i].verifiedLoaderFallback) || target[i].overrideValue)continue;
        const MaterialSlot* found=nullptr;
        for(unsigned j=0;j<sourceCount;++j)if(target[i].name==source[j].name)found=&source[j];
        if(!found || !found->material || found->verifiedLoaderFallback)return MaterialPlanStatus::Invalid;
        plan.entries[plan.count++]={i,target[i].name,found->material};
    }
    output=plan;return plan.count?MaterialPlanStatus::Ready:MaterialPlanStatus::Unchanged;
}
struct MaterialRepairKey {
    void* component=nullptr;std::uint64_t componentSerial=0;
    void* mesh=nullptr;std::uint64_t meshSerial=0;
};
using ReadRepairSlot=bool(*)(const MaterialRepairKey&,unsigned,bool,MaterialSlot&,void*);
// The native writer must compare the actual current override with expected,
// recheck owner/world/thread/serials and use the original component setter.
using WriteRepairSlot=bool(*)(const MaterialRepairKey&,unsigned,void*,void*,bool,void*);
class MaterialRepairLease {
    MaterialRepairKey key{};MaterialRepairPlan plan{};std::uint32_t pending=0;
public:
    void Forget(){key={};plan={};pending=0;}
    bool Active()const{return pending!=0;}
    const MaterialRepairKey& Key()const{return key;}
    bool Healthy(ReadRepairSlot read,void* context)const {
        if(!Active() || !read)return false;
        for(unsigned i=0;i<plan.count;++i)if(pending&(std::uint32_t{1}<<i)) {
            MaterialSlot slot;
            if(!read(key,plan.entries[i].index,true,slot,context) || slot.name!=plan.entries[i].name ||
               slot.overrideValue!=plan.entries[i].replacement)return false;
        }
        return true;
    }
    bool Restore(ReadRepairSlot read,WriteRepairSlot write,void* context) {
        if(!read || !write)return !Active();
        for(unsigned i=0;i<plan.count;++i)if(pending&(std::uint32_t{1}<<i)) {
            const auto& entry=plan.entries[i];MaterialSlot slot;
            // On mesh replacement only clear an override we still own. The
            // caller retains component/world/owner serial checks in this mode.
            if(!read(key,entry.index,false,slot,context))continue;
            if(slot.overrideValue!=entry.replacement ||
               write(key,entry.index,entry.replacement,nullptr,false,context))pending&=~(std::uint32_t{1}<<i);
        }
        if(!pending)Forget();return !Active();
    }
    bool Apply(const MaterialRepairKey& target,const MaterialRepairPlan& desired,
               ReadRepairSlot read,WriteRepairSlot write,void* context) {
        if(Active() || !target.component || !target.componentSerial || !target.mesh || !target.meshSerial ||
           !read || !write || !desired.count || desired.count>32)return false;
        // Validate the entire snapshot before any component mutation.
        for(unsigned i=0;i<desired.count;++i) {
            const auto& entry=desired.entries[i];MaterialSlot slot;
            if(entry.index>=32 || !entry.name || !entry.replacement ||
               !read(target,entry.index,true,slot,context) || slot.name!=entry.name ||
               (slot.material && !slot.verifiedLoaderFallback) || slot.overrideValue)return false;
            for(unsigned j=0;j<i;++j)if(desired.entries[j].index==entry.index)return false;
        }
        key=target;plan=desired;
        for(unsigned i=0;i<plan.count;++i) {
            const auto& entry=plan.entries[i];MaterialSlot current;
            if(!read(key,entry.index,true,current,context) || current.name!=entry.name ||
               (current.material && !current.verifiedLoaderFallback) || current.overrideValue) {
                Restore(read,write,context);return false;
            }
            pending|=std::uint32_t{1}<<i;
            if(!write(key,entry.index,nullptr,entry.replacement,true,context)) {
                Restore(read,write,context);return false;
            }
        }
        if(!Healthy(read,context)){Restore(read,write,context);return false;}
        return true;
    }
};
}
