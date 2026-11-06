#include <cstdio>
#include <stdexcept>
#include "../native/NativeStaticMaterials.h"
namespace V=TwinKartVisual;
void Need(bool value,const char* text){if(!value)throw std::runtime_error(text);}
struct Scene {
    V::MaterialSlot target[3]{};bool owner=true,originalMesh=true;
    unsigned writes=0;int fail=-1;bool failAfterMutation=false;
    void* changeSecondDefault=nullptr;
};
bool Read(const V::MaterialRepairKey&,unsigned index,bool same,V::MaterialSlot& out,void* raw) {
    auto& scene=*static_cast<Scene*>(raw);
    if(!scene.owner || (same && !scene.originalMesh) || index>=3)return false;
    out=scene.target[index];return true;
}
bool Write(const V::MaterialRepairKey& key,unsigned index,void* before,void* next,bool same,void* raw) {
    auto& scene=*static_cast<Scene*>(raw);V::MaterialSlot out;
    if(!Read(key,index,same,out,raw) || out.overrideValue!=before)return false;
    ++scene.writes;bool fail=same && scene.fail==static_cast<int>(index);
    if(!fail || scene.failAfterMutation)scene.target[index].overrideValue=next;
    if(same && index==0 && scene.changeSecondDefault)scene.target[1].material=scene.changeSecondDefault;
    return !fail;
}
int main(){try {
    int component=0,mesh=0,leaf=0,bark=0,healthy=0,foreign=0;
    alignas(16) unsigned char nativeMesh[0xa0]{},nativeComponent[0x4e0]{};
    V::StaticMaterial nativeRows[2]{};nativeRows[0].material=&bark;nativeRows[0].slot=20;
    nativeRows[1].material=&leaf;nativeRows[1].slot=10;
    V::MaterialArray<const V::StaticMaterial> table{nativeRows,2,2};
    memcpy(nativeMesh+0x88,&table,sizeof(table));V::MaterialSlot copied[32]{};unsigned copiedCount=0;
    Need(V::ReadStaticMaterials(nativeMesh,copied,copiedCount) && copiedCount==2 &&
         copied[1].name==10 && copied[1].material==&leaf,"Original static material offset/48-byte stride changed");
    table.capacity=1;memcpy(nativeMesh+0x88,&table,sizeof(table));copied[0].name=999;
    Need(!V::ReadStaticMaterials(nativeMesh,copied,copiedCount) && copiedCount==0 && copied[0].name==999,
         "Malformed native table published a partial material snapshot");
    void* nativeOverrides[2]{nullptr,&foreign};V::MaterialArray<void*> overrides{nativeOverrides,2,2};
    memcpy(nativeComponent+0x478,&overrides,sizeof(overrides));void* copiedOverrides[32]{};
    Need(V::ReadMaterialOverrides(nativeComponent,copiedOverrides,copiedCount) && copiedCount==2 &&
         copiedOverrides[1]==&foreign && !copiedOverrides[2],"Original override layout or trailing null slots changed");
    overrides.count=33;overrides.capacity=33;memcpy(nativeComponent+0x478,&overrides,sizeof(overrides));
    Need(!V::ReadMaterialOverrides(nativeComponent,copiedOverrides,copiedCount) && !copiedCount,
         "Oversized native override array accepted");
    V::MaterialRepairKey key{&component,12,&mesh,34};
    V::MaterialSlot source[3]{{20,&bark,nullptr},{10,&leaf,nullptr},{30,&healthy,nullptr}};
    Scene scene{{{10,nullptr,nullptr},{20,nullptr,nullptr},{30,&healthy,nullptr}}};
    V::MaterialRepairPlan plan;
    Need(V::PlanMissingMaterials(scene.target,3,source,3,plan)==V::MaterialPlanStatus::Ready && plan.count==2 &&
         plan.entries[0].replacement==&leaf && plan.entries[1].replacement==&bark,
         "Slot identity mapping used source array order or overwrote a valid default");
    auto duplicate=source[1];source[1].name=20;
    Need(V::PlanMissingMaterials(scene.target,3,source,3,plan)==V::MaterialPlanStatus::Invalid && !plan.count,
        "Ambiguous original material slot identity accepted");source[1]=duplicate;
    source[0].material=nullptr;
    Need(V::PlanMissingMaterials(scene.target,3,source,3,plan)==V::MaterialPlanStatus::Invalid && !plan.count,
        "Partial source materials produced a partly published plan");source[0].material=&bark;
    V::PlanMissingMaterials(scene.target,3,source,3,plan);
    V::MaterialRepairLease lease;
    scene.target[1].overrideValue=&foreign;
    Need(!lease.Apply(key,plan,Read,Write,&scene) && scene.writes==0,"Changed snapshot mutated an earlier valid slot");
    scene.target[1].overrideValue=nullptr;
    Need(lease.Apply(key,plan,Read,Write,&scene) && lease.Healthy(Read,&scene) && scene.writes==2,
         "Missing native materials did not receive verified original replacements");
    Need(!lease.Apply(key,plan,Read,Write,&scene),"An active lease was overwritten");
    scene.target[1].overrideValue=&foreign;scene.originalMesh=false;
    Need(!lease.Healthy(Read,&scene),"A replaced native mesh kept stale healthy status");
    Need(lease.Restore(Read,Write,&scene) && !lease.Active() && !scene.target[0].overrideValue &&
         scene.target[1].overrideValue==&foreign,"Mesh-change cleanup overwrote a foreign override");
    scene.originalMesh=true;scene.target[1].overrideValue=nullptr;scene.fail=1;
    Need(!lease.Apply(key,plan,Read,Write,&scene) && !lease.Active() && !scene.target[0].overrideValue &&
         !scene.target[1].overrideValue,"Second native-setter failure left the first material applied");
    scene.failAfterMutation=true;
    Need(!lease.Apply(key,plan,Read,Write,&scene) && !lease.Active() && !scene.target[0].overrideValue &&
         !scene.target[1].overrideValue,"A setter that changed state before failing escaped rollback");
    scene.fail=-1;scene.changeSecondDefault=&healthy;
    Need(!lease.Apply(key,plan,Read,Write,&scene) && !lease.Active() && !scene.target[0].overrideValue &&
         !scene.target[1].overrideValue && scene.target[1].material==&healthy,
         "Native callback changed a later default but the old plan still overwrote it");
    scene.changeSecondDefault=nullptr;scene.target[1].material=nullptr;
    scene.fail=-1;Need(lease.Apply(key,plan,Read,Write,&scene),"Lifetime cleanup fixture initialization");
    unsigned before=scene.writes;scene.owner=false;
    Need(!lease.Restore(Read,Write,&scene) && lease.Active() && scene.writes==before,"Expired/foreign component was written during cleanup");
    lease.Forget();Need(!lease.Active(),"Destroyed-world lease remained active");
    scene.owner=true;scene.target[0].overrideValue=&foreign;scene.target[1].overrideValue=nullptr;
    Need(V::PlanMissingMaterials(scene.target,3,source,3,plan)==V::MaterialPlanStatus::Ready && plan.count==1 &&
         plan.entries[0].index==1,"Explicit native override was not preserved in a new plan");
    Need(!V::UniqueMaterialSlots(nullptr,1) && !V::UniqueMaterialSlots(source,33),"Unbounded native arrays accepted");
    scene.target[0]={10,&healthy,nullptr,true};scene.target[1]={20,&healthy,nullptr,false};
    Need(V::PlanMissingMaterials(scene.target,3,source,3,plan)==V::MaterialPlanStatus::Ready && plan.count==1 &&
         plan.entries[0].index==0,"A verified loader fallback was not distinguished from a healthy original default");
    Need(lease.Apply(key,plan,Read,Write,&scene) && lease.Restore(Read,Write,&scene) && scene.target[0].material==&healthy,
         "Fallback repair changed the original mesh's material table");
    scene.target[0].overrideValue=&foreign;
    Need(V::PlanMissingMaterials(scene.target,3,source,3,plan)==V::MaterialPlanStatus::Unchanged,
         "A verified loader fallback caused an explicit override to be overwritten");
    puts("PASS scoped missing-material repair: exact slot identity, preserved defaults/overrides, atomic plan validation, native setter failure rollback, mesh change, foreign override and expired-owner cleanup. Synthetic interfaces only; no native model or rendering acceptance.");return 0;
}catch(const std::exception& e){fprintf(stderr,"%s\n",e.what());return 1;}}
