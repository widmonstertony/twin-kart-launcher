// Included inside DriftRaceBridge's private namespace, after the existing
// exact-build scene primitives. No CDO, original map or global mesh is edited.
bool CurrentHideRepairWorld() {
    if(!hideRepairedSceneEnabled || hideArenaCapture || !hideCaptureArmed || GetCurrentThreadId()!=queue.ThreadId() ||
       !HideMaterialIdentityLive(hideRepairedScene.world))return false;
    auto world=static_cast<BYTE*>(hideRepairedScene.world.object);wchar_t name[96]{};
    return !*reinterpret_cast<void**>(world+0x38) && TwinKartLocalRace::objectiveWorld.Matches(world,
        *reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0),*reinterpret_cast<int*>(image+0x69c28c4)) &&
        ModeNameText(*reinterpret_cast<unsigned long long*>(world+0x18),name,ARRAYSIZE(name)) && !wcscmp(name,L"HideNSeekTestLevel");
}
bool HideRepairActorOwned(BYTE* actor) {
    using GetWorld=BYTE*(__fastcall*)(void*);
    return CurrentHideRepairWorld() && TwinKartLocalRace::Live(actor) && !(*reinterpret_cast<unsigned*>(actor+8)&0x18030) &&
        !(actor[0x5c]&0x10) && reinterpret_cast<GetWorld>((*reinterpret_cast<void***>(actor))[0x160/8])(actor)==hideRepairedScene.world.object;
}
BYTE* HideRepairComponent(BYTE* actor,unsigned getter) {
    if(!HideRepairActorOwned(actor))return nullptr;
    using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
    auto type=reinterpret_cast<Type>(image+getter)();
    auto component=reinterpret_cast<Find>(image+0x3cb3550)(actor,type);
    return TwinKartLocalRace::Live(component) && TwinKartRace::ObjectiveAssetIsA(image,component,getter) &&
        *reinterpret_cast<void**>(component+0xa0)==actor?component:nullptr;
}
void ReleaseHideRepairedScene() {
    if(CurrentHideRepairWorld()) {
        // Restore only our still-current instance overrides. A later caller's
        // choice is never overwritten, and expired/recycled serials are skipped.
        for(auto& floor:hideRepairedScene.floors)if(HideMaterialIdentityLive(floor.actor) &&
            HideMaterialIdentityLive(floor.component) && HideMaterialIdentityLive(floor.mesh) &&
            HideRepairActorOwned(static_cast<BYTE*>(floor.actor.object)) &&
            *reinterpret_cast<void**>(static_cast<BYTE*>(floor.component.object)+0xa0)==floor.actor.object &&
            *reinterpret_cast<void**>(static_cast<BYTE*>(floor.component.object)+0x4c8)==floor.mesh.object) {
            auto component=static_cast<BYTE*>(floor.component.object);
            for(unsigned slot=0;slot<32;++slot)if(floor.changed&(1u<<slot)) {
                void* overrides[32]{};unsigned count=0;
                if(TwinKartVisual::ReadMaterialOverrides(component,overrides,count) &&
                   overrides[slot]==hideRepairedScene.assets[3].object &&
                   (!floor.previous[slot] || HideMaterialIdentityLive(floor.previousIdentity[slot]))) {
                    auto method=(*reinterpret_cast<BYTE***>(component))[0x570/8];
                    if(method>=image && method<image+0x4ed0000)
                        reinterpret_cast<void(__fastcall*)(void*,int,void*)>(method)(component,slot,floor.previous[slot]);
                }
            }
        }
        for(auto& start:hideRepairedScene.starts)if(HideMaterialIdentityLive(start.actor) &&
            HideRepairActorOwned(static_cast<BYTE*>(start.actor.object))) {
            auto& tag=*reinterpret_cast<unsigned long long*>(static_cast<BYTE*>(start.actor.object)+0x248);
            if(tag==hideRepairedScene.mutedTag)tag=start.tag;
        }
        for(unsigned i=hideRepairedScene.actorCount;i>0;--i) {
            auto id=hideRepairedScene.actors[i-1];
            if(HideMaterialIdentityLive(id) && HideRepairActorOwned(static_cast<BYTE*>(id.object)))
                reinterpret_cast<bool(__fastcall*)(void*,bool,bool)>(image+0x3cac200)(id.object,false,true);
        }
    }
    hideRepairedScene=HideRepairedSceneState{};
}
void RollbackHideRepairScene(void*) {ReleaseHideRepairedScene();}
TwinKartRace::RoadSurfaceResult ReadHideRepairedSurface(const TwinKartRace::ArenaPoint& start,
    const TwinKartRace::ArenaPoint& end,TwinKartRace::RoadSurface& outputSurface,void* raw) {
    using Result=TwinKartRace::RoadSurfaceResult;
    if(!CurrentHideRepairWorld() || raw!=hideRepairedScene.world.object || !TwinKartRace::Finite(start) ||
       !TwinKartRace::Finite(end) || start.z<=end.z || std::fabs(start.x-end.x)>.01f || std::fabs(start.y-end.y)>.01f)return Result::Invalid;
    hideRepairedScene.groundDiagnostic=1;++hideRepairedScene.groundQueries;
    auto channels=hideRepairedScene.groundTypes;auto& channelCount=hideRepairedScene.groundTypeCount;
    using Channel=unsigned char(__fastcall*)(int);
    if(!channelCount)for(int query=0;query<32;++query) {
        auto channel=reinterpret_cast<Channel>(image+0x3f159a0)(query);
        if(channel==0 || channel==1 || channel==17) {
            if(channelCount>=3)return Result::Invalid;channels[channelCount++]=static_cast<unsigned char>(query);
        }
    }
    if(channelCount!=3){hideRepairedScene.groundDiagnostic=2;return Result::Invalid;}
    void* ignored[8]{};int ignoredCount=0;TwinKartRace::QueryArray types{channels,channelCount,channelCount};
    for(unsigned attempt=0;attempt<9;++attempt) {
        struct alignas(16) Hit {unsigned char bytes[0x88]{};std::uint64_t guard=0xe840f3d1296b57a0ull;} hit;
        TwinKartRace::QueryArray exclusions{ignored,ignoredCount,ignoredCount};TwinKartRace::QueryColor color{};
        auto trace=reinterpret_cast<TwinKartRace::ObjectTrace>(image+0x3ff5990);
        bool found=trace(raw,&start,&end,&types,false,&exclusions,0,hit.bytes,false,&color,&color,0.f);
        // Some retained test meshes have a complex triangle floor but no
        // simple query hull. Ask the ORIGINAL collision system for that actual
        // geometry; neither a missing hit nor a prop top becomes invented road.
        if(!found && hit.guard==0xe840f3d1296b57a0ull)found=trace(raw,&start,&end,&types,true,&exclusions,0,hit.bytes,false,&color,&color,0.f);
        hideRepairedScene.groundDiagnostic=3;
        if(!CurrentHideRepairWorld() || hit.guard!=0xe840f3d1296b57a0ull)return Result::Invalid;
        if(!found){hideRepairedScene.groundDiagnostic=4;return Result::Miss;}
        TwinKartRace::HitWeakIdentity weakActor{},weakComponent{};
        memcpy(&weakActor,hit.bytes+0x68,sizeof(weakActor));memcpy(&weakComponent,hit.bytes+0x70,sizeof(weakComponent));
        auto resolve=reinterpret_cast<TwinKartRace::ResolveHitWeak>(image+0x2799770);
        auto actor=static_cast<BYTE*>(resolve(&weakActor)),component=static_cast<BYTE*>(resolve(&weakComponent));
        auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);int count=*reinterpret_cast<int*>(image+0x69c28c4);
        hideRepairedScene.groundDiagnostic=5;
        if(!TwinKartCamera::Live({actor,weakActor.index,weakActor.serial},chunks,count) ||
           !TwinKartCamera::Live({component,weakComponent.index,weakComponent.serial},chunks,count) ||
           !HideRepairActorOwned(actor) || *reinterpret_cast<void**>(component+0xa0)!=actor)return Result::Invalid;
        bool floor=false;
        for(auto& saved:hideRepairedScene.floors)if(saved.actor.object==actor && saved.actor.serial==weakActor.serial &&
            saved.component.object==component && saved.component.serial==weakComponent.serial && HideMaterialIdentityLive(saved.mesh) &&
            *reinterpret_cast<void**>(component+0x4c8)==saved.mesh.object)floor=true;
        if(floor) {
            TwinKartRace::RoadSurface surface{};memcpy(&surface.point,hit.bytes+0xc,sizeof(surface.point));
            memcpy(&surface.normal,hit.bytes+0x30,sizeof(surface.normal));surface.normalZ=surface.normal.z;
            surface.penetrating=(hit.bytes[0x5d]&2)!=0;
            float norm=surface.normal.x*surface.normal.x+surface.normal.y*surface.normal.y+surface.normal.z*surface.normal.z;
            hideRepairedScene.groundDiagnostic=6;
            if(!(hit.bytes[0x5d]&1) || !TwinKartRace::Finite(surface.point) || !TwinKartRace::Finite(surface.normal) ||
               !std::isfinite(norm) || (!surface.penetrating && std::fabs(norm-1)>.03f) ||
               std::fabs(surface.point.x-start.x)>2 || std::fabs(surface.point.y-start.y)>2 ||
               surface.point.z>start.z+2 || surface.point.z<end.z-2)return Result::Invalid;
            hideRepairedScene.groundDiagnostic=7;outputSurface=surface;return Result::Hit;
        }
        hideRepairedScene.groundDiagnostic=8;if(ignoredCount>=8)return Result::Invalid;
        for(int i=0;i<ignoredCount;++i)if(ignored[i]==actor)return Result::Invalid;
        ignored[ignoredCount++]=actor; // Skip a prop, never declare its top a road.
    }
    return Result::Invalid;
}
bool HideRepairGround(const TwinKartRace::ArenaPoint& sample,TwinKartRace::ArenaPoint& contact,void*) {
    TwinKartRace::RoadSurface surface{};
    if(ReadHideRepairedSurface({sample.x,sample.y,sample.z+4000},{sample.x,sample.y,sample.z-4000},surface,
        hideRepairedScene.world.object)!=TwinKartRace::RoadSurfaceResult::Hit || surface.penetrating || surface.normalZ<=.70f)return false;
    contact=surface.point;return true;
}
BYTE* LoadHideRepairAsset(unsigned index,const wchar_t* path,unsigned getter) {
    if(!CurrentHideRepairWorld() || index>=ARRAYSIZE(hideRepairedScene.assets))return nullptr;
    auto& id=hideRepairedScene.assets[index];
    if(HideMaterialIdentityLive(id))return static_cast<BYTE*>(id.object);
    TwinKartRace::ObjectiveSoftReference reference{};
    reinterpret_cast<void(__fastcall*)(void*,const wchar_t*,int)>(image+0x2589130)(&reference.path.name,path,1);
    auto asset=reinterpret_cast<BYTE*(__fastcall*)(void*)>(image+0xa42300)(&reference);
    if(!CurrentHideRepairWorld() || !TwinKartLocalRace::Live(asset) ||
       !TwinKartRace::ObjectiveAssetIsA(image,asset,getter) || !TwinKartLocalRace::CaptureFlagObject(asset,id))return nullptr;
    return asset;
}
bool ApplyHideRepairActor(const TwinKartObjective::HideRepairPlacement& item,void*) {
    using Kind=TwinKartObjective::HideRepairActor;using Type=BYTE*(__fastcall*)();
    // The exact client enum table at 0x5ce9930 maps PhysicsOnly to 2 and
    // QueryAndPhysics to 3. Vehicle sweeps must see these obstacles too.
    constexpr unsigned char queryAndPhysics=3,staticMobility=0,movableMobility=2;
    if(!CurrentHideRepairWorld() || hideRepairedScene.actorCount>=ARRAYSIZE(hideRepairedScene.actors))return false;
    RuntimeEvent("native_hide_repaired_test_scene_spawn_begin",hideRepairedScene.actorCount,static_cast<int>(item.kind));
    unsigned getter=item.kind==Kind::Start?0x450c7b0:item.kind==Kind::Sun?0x4429610:item.kind==Kind::Ambient?0x4539310:0x454fa90;
    if(item.kind==Kind::Sky && !HideMaterialIdentityLive(hideRepairedScene.assets[5]))return false;
    auto type=item.kind==Kind::Sky?static_cast<BYTE*>(hideRepairedScene.assets[5].object):reinterpret_cast<Type>(image+getter)();
    if(!TwinKartLocalRace::Live(type))return false;
    auto begin=reinterpret_cast<TwinKartObjective::BeginObjectiveActor>(image+0x3f37eb0);
    auto finish=reinterpret_cast<TwinKartObjective::FinishObjectiveActor>(image+0x3f41030);
    auto actor=static_cast<BYTE*>(begin(hideRepairedScene.world.object,type,&item.transform,1,nullptr));
    if(!HideRepairActorOwned(actor))return false;
    auto& retained=hideRepairedScene.actors[hideRepairedScene.actorCount];
    if(!TwinKartLocalRace::CaptureFlagObject(actor,retained)) {
        reinterpret_cast<bool(__fastcall*)(void*,bool,bool)>(image+0x3cac200)(actor,false,true);return false;
    }
    ++hideRepairedScene.actorCount; // Own even an unfinished actor for rollback.
    if(item.kind==Kind::Start) {
        if(item.slot>=16)return false;
        reinterpret_cast<void(__fastcall*)(void*,const wchar_t*,int)>(image+0x2589130)(actor+0x248,item.slot<8?L"HiderStart":L"SeekerStart",1);
    } else if(item.kind==Kind::Wall || item.kind==Kind::Crate || item.kind==Kind::Tree) {
        auto component=HideRepairComponent(actor,0x454d3d0);if(!component)return false;
        // Reflected native SceneComponent::SetMobility and StaticMeshComponent
        // SetStaticMesh preserve registration, bounds, body and render updates.
        auto table=*reinterpret_cast<BYTE***>(component);
        if(table[0x500/8]<image || table[0x500/8]>=image+0x4ed0000 || table[0x928/8]<image ||
           table[0x928/8]>=image+0x4ed0000 || table[0x638/8]<image || table[0x638/8]>=image+0x4ed0000)return false;
        reinterpret_cast<void(__fastcall*)(void*,unsigned char)>(table[0x500/8])(component,movableMobility);
        auto& meshIdentity=hideRepairedScene.assets[item.kind==Kind::Wall?0:item.kind==Kind::Crate?1:2];
        if(!HideMaterialIdentityLive(meshIdentity))return false;auto mesh=meshIdentity.object;
        if(!reinterpret_cast<bool(__fastcall*)(void*,void*)>(table[0x928/8])(component,mesh) ||
           !HideMaterialIdentityLive(retained) || !HideRepairActorOwned(actor) || *reinterpret_cast<void**>(component+0x4c8)!=mesh)return false;
        reinterpret_cast<void(__fastcall*)(void*,unsigned char)>(table[0x638/8])(component,queryAndPhysics);
        if(item.kind==Kind::Wall) {
            if(!HideMaterialIdentityLive(hideRepairedScene.assets[4]))return false;
            auto set=(*reinterpret_cast<BYTE***>(component))[0x570/8];
            if(set<image || set>=image+0x4ed0000)return false;
            reinterpret_cast<void(__fastcall*)(void*,int,void*)>(set)(component,0,hideRepairedScene.assets[4].object);
            // Assign the mesh while editable, then register the wall as a
            // static obstacle, like the original arena geometry. A movable
            // component can instead create a kinematic PhysX actor.
            reinterpret_cast<void(__fastcall*)(void*,unsigned char)>(table[0x500/8])(component,staticMobility);
        }
    } else if(item.kind==Kind::Sun || item.kind==Kind::Ambient) {
        auto component=HideRepairComponent(actor,item.kind==Kind::Sun?0x44296e0:0x4537460);if(!component)return false;
        auto mobility=(*reinterpret_cast<BYTE***>(component))[0x500/8];
        if(mobility<image || mobility>=image+0x4ed0000)return false;
        reinterpret_cast<void(__fastcall*)(void*,unsigned char)>(mobility)(component,2);
        reinterpret_cast<void(__fastcall*)(void*,float)>(image+(item.kind==Kind::Sun?0x3e2b410:0x3e7a6d0))
            (component,item.kind==Kind::Sun?4.f:1.1f);
    }
    if(!CurrentHideRepairWorld() || !HideMaterialIdentityLive(retained) || finish(actor,&item.transform)!=actor ||
       !HideRepairActorOwned(actor))return false;
    if(item.kind==Kind::Wall) {
        auto component=HideRepairComponent(actor,0x454d3d0);
        auto table=component?*reinterpret_cast<BYTE***>(component):nullptr;
        if(table && table[0x638/8]==image+0x41dfa60 && table[0x6d8/8]==image+0x41cf890) {
            // Deferred construction can reset settings set before Finish.
            // Check the effective flag on the registered body and restore the
            // authored QueryAndPhysics policy only on our actual wall.
            using GetCollision=unsigned char(__fastcall*)(void*);
            auto collision=reinterpret_cast<GetCollision>(image+0x4156570);
            if(component[0x14f]!=staticMobility)return false;
            RuntimeEvent("native_hide_wall_static_mobility",item.slot,1);
            if(collision(component+0x2e8)!=queryAndPhysics)
                reinterpret_cast<void(__fastcall*)(void*,unsigned char)>(table[0x638/8])(component,queryAndPhysics);
            if(!HideMaterialIdentityLive(retained) || !HideRepairActorOwned(actor))return false;
            RuntimeEvent("native_hide_wall_effective_collision",item.slot,collision(component+0x2e8));
            ChaserBodyLease lease;
            lease.body=reinterpret_cast<BYTE*(__fastcall*)(void*,unsigned long long,bool)>(table[0x6d8/8])(component,0,true);
            if(lease.body)lease.actorHandle=*reinterpret_cast<void**>(lease.body+0x120);
            const auto geometry=lease.actorHandle?ChaserBodyGeometry(lease):TwinKartRace::BodyGeometrySnapshot{};
            RuntimeEvent("native_hide_wall_simple_physics_shapes",item.slot,geometry.valid?static_cast<int>(geometry.supported):0);
        }
    }
    return true;
}
bool PrepareHideRepairedSceneImpl(BYTE* world,BYTE* mode) {
    if(!hideRepairedSceneEnabled || hideArenaCapture)return false;
    if(hideRepairedScene.ready)return CurrentHideRepairWorld() && hideRepairedScene.world.object==world;
    if(hideRepairedScene.attempted || GetCurrentThreadId()!=queue.ThreadId() || GetTickCount64()<hideRepairedScene.next)return false;
    hideRepairedScene.next=GetTickCount64()+1000;
    if(!TwinKartLocalRace::CaptureFlagObject(world,hideRepairedScene.world) || !CurrentHideRepairWorld() || !HideRepairActorOwned(mode))return false;
    auto stage=[](int value) {if(hideRepairedScene.stage!=value){hideRepairedScene.stage=value;RuntimeEvent("native_hide_repaired_test_scene_preparation_stage",-1,value);}};
    stage(1);
    auto chunks=*reinterpret_cast<TwinKartCamera::ObjectSlot***>(image+0x69c28b0);int count=*reinterpret_cast<int*>(image+0x69c28c4);
    auto name=reinterpret_cast<void(__fastcall*)(void*,const wchar_t*,int)>(image+0x2589130);
    name(&hideRepairedScene.mutedTag,L"TwinKart_Replaced_Test_Start",1);
    bool first=true;hideRepairedScene.floorCount=hideRepairedScene.startCount=0;unsigned originalAreas=0;
    for(int index=0;index<count;++index) {
        auto chunk=chunks[index/65536];if(!chunk || chunk[index%65536].flags&0x30000000)continue;
        auto actor=static_cast<BYTE*>(chunk[index%65536].object);
        // Classify first: most UObjects are not Actors and have no Actor world slot.
        if(!TwinKartLocalRace::Live(actor) || *reinterpret_cast<unsigned*>(actor+8)&0x18030)continue;
        bool start=TwinKartRace::ObjectiveAssetIsA(image,actor,0x450c7b0),area=TwinKartRace::ObjectiveAssetIsA(image,actor,0x10f3610);
        bool meshActor=TwinKartRace::ObjectiveAssetIsA(image,actor,0x454fa90);
        if((!start && !area && !meshActor) || !HideRepairActorOwned(actor))continue;
        if(area)++originalAreas;
        if(start) {
            auto tag=*reinterpret_cast<unsigned long long*>(actor+0x248);wchar_t tagText[128]{};
            if(!ModeNameText(tag,tagText,ARRAYSIZE(tagText)))return false;
            if(TwinKartObjective::HideRepairStartRole(tagText)) {
                if(hideRepairedScene.startCount>=ARRAYSIZE(hideRepairedScene.starts))return false;
                auto& saved=hideRepairedScene.starts[hideRepairedScene.startCount++];saved.tag=tag;
                if(!TwinKartLocalRace::CaptureFlagObject(actor,saved.actor))return false;
            }
        }
        if(!meshActor)continue;
        auto component=HideRepairComponent(actor,0x454d3d0);auto mesh=component?*reinterpret_cast<BYTE**>(component+0x4c8):nullptr;
        if(!ExactHideMeshPath(mesh,L"/Game/Dev/HideNSeek/Mesh_HideNSeekRoad",L"Mesh_HideNSeekRoad"))continue;
        if(hideRepairedScene.floorCount>=ARRAYSIZE(hideRepairedScene.floors))return false;
        auto& floor=hideRepairedScene.floors[hideRepairedScene.floorCount++];
        if(!TwinKartLocalRace::CaptureFlagObject(actor,floor.actor) || !TwinKartLocalRace::CaptureFlagObject(component,floor.component) ||
           !TwinKartLocalRace::CaptureFlagObject(mesh,floor.mesh))return false;
        TwinKartRace::ArenaPoint center{},extent{};
        reinterpret_cast<void(__fastcall*)(void*,bool,TwinKartRace::ArenaPoint*,TwinKartRace::ArenaPoint*,bool)>(image+0x3cb2830)(actor,true,&center,&extent,false);
        if(!TwinKartRace::Finite(center) || !TwinKartRace::Finite(extent) || extent.x<1 || extent.y<1 || extent.z<0 ||
           extent.x>60000 || extent.y>60000 || extent.z>4000)return false;
        TwinKartRace::ArenaPoint a{center.x-extent.x,center.y-extent.y,center.z-extent.z},b{center.x+extent.x,center.y+extent.y,center.z+extent.z};
        if(first){hideRepairedScene.low=a;hideRepairedScene.high=b;first=false;}
        else for(int axis=0;axis<3;++axis) {
            auto lo=axis==0?&hideRepairedScene.low.x:axis==1?&hideRepairedScene.low.y:&hideRepairedScene.low.z;
            auto hi=axis==0?&hideRepairedScene.high.x:axis==1?&hideRepairedScene.high.y:&hideRepairedScene.high.z;
            float av=axis==0?a.x:axis==1?a.y:a.z,bv=axis==0?b.x:axis==1?b.y:b.z;
            *lo=(std::min)(*lo,av);*hi=(std::max)(*hi,bv);
        }
    }
    int census=static_cast<int>(hideRepairedScene.floorCount+hideRepairedScene.startCount*16+originalAreas*2048);
    if(census!=hideRepairedScene.census) {
        hideRepairedScene.census=census;
        RuntimeEvent("native_hide_repaired_test_scene_floor_count",-1,hideRepairedScene.floorCount);
        RuntimeEvent("native_hide_repaired_test_scene_previous_start_count",-1,hideRepairedScene.startCount);
        RuntimeEvent("native_hide_repaired_test_scene_previous_area_count",-1,originalAreas);
        if(!first){RuntimeEvent("native_hide_repaired_test_scene_floor_width_cm",-1,static_cast<int>(hideRepairedScene.high.x-hideRepairedScene.low.x));
            RuntimeEvent("native_hide_repaired_test_scene_floor_depth_cm",-1,static_cast<int>(hideRepairedScene.high.y-hideRepairedScene.low.y));
            RuntimeEvent("native_hide_repaired_test_scene_floor_low_z",-1,static_cast<int>(hideRepairedScene.low.z));
            RuntimeEvent("native_hide_repaired_test_scene_floor_high_z",-1,static_cast<int>(hideRepairedScene.high.z));}
    }
    if(first || !originalAreas || !hideRepairedScene.startCount)return false;
    stage(2);
    TwinKartObjective::HideRepairPlan plan;
    if(!TwinKartObjective::BuildHideRepairPlan(hideRepairedScene.low,hideRepairedScene.high,HideRepairGround,nullptr,plan)) {
        RuntimeEvent("native_hide_repaired_test_scene_ground_diagnostic",-1,hideRepairedScene.groundDiagnostic);
        RuntimeEvent("native_hide_repaired_test_scene_ground_query_count",-1,hideRepairedScene.groundQueries);
        RuntimeEvent("native_hide_repaired_test_scene_supported_layout_unavailable",-1,0);return false;
    }
    // All six exact paths were found in authenticated installed PC directories.
    stage(3);
    if(!LoadHideRepairAsset(0,L"/Engine/BasicShapes/Cube.Cube",0x454a5e0) ||
       !LoadHideRepairAsset(1,L"/Game/ART/Env/Theme_Village/Asset/Mesh/BC_mesh_marketbox01.BC_mesh_marketbox01",0x454a5e0) ||
       !LoadHideRepairAsset(2,L"/Game/ART/Env/Theme_Desert/Asset/Mesh/Des_mesh_tree003.Des_mesh_tree003",0x454a5e0) ||
       !LoadHideRepairAsset(3,L"/Game/ART/Env/Theme_Village/Asset/Material/Vil_mat_asphalt005_road_inst.Vil_mat_asphalt005_road_inst",0x44d98e0) ||
       !LoadHideRepairAsset(4,L"/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial",0x44d98e0) ||
       !LoadHideRepairAsset(5,L"/Engine/EngineSky/BP_Sky_Sphere.BP_Sky_Sphere_C",0x44030f0))return false;
    for(auto& asset:hideRepairedScene.assets)if(!HideMaterialIdentityLive(asset))return false;
    hideRepairedScene.attempted=true;
    stage(4);
    if(!TwinKartObjective::ApplyHideRepairPlan(plan,ApplyHideRepairActor,RollbackHideRepairScene,nullptr)) {
        hideRepairedScene.attempted=true;RuntimeEvent("native_hide_repaired_test_scene_rolled_back",-1,0);return false;
    }
    stage(5);
    for(auto& saved:hideRepairedScene.starts)if(saved.actor.object) {
        if(!HideMaterialIdentityLive(saved.actor) || !HideRepairActorOwned(static_cast<BYTE*>(saved.actor.object)) ||
           *reinterpret_cast<unsigned long long*>(static_cast<BYTE*>(saved.actor.object)+0x248)!=saved.tag) {
            ReleaseHideRepairedScene();hideRepairedScene.attempted=true;return false;
        }
        *reinterpret_cast<unsigned long long*>(static_cast<BYTE*>(saved.actor.object)+0x248)=hideRepairedScene.mutedTag;
    }
    stage(6);
    for(auto& floor:hideRepairedScene.floors)if(floor.component.object) {
        auto component=static_cast<BYTE*>(floor.component.object);unsigned countOverrides=0;
        TwinKartVisual::MaterialArray<const TwinKartVisual::StaticMaterial> materials{};
        if(HideMaterialIdentityLive(floor.mesh))memcpy(&materials,static_cast<BYTE*>(floor.mesh.object)+0x88,sizeof(materials));
        unsigned slots=materials.count>0?static_cast<unsigned>(materials.count):0;
        void* overrides[32]{};
        if(!HideMaterialIdentityLive(floor.component) || !HideMaterialIdentityLive(floor.mesh) ||
           !materials.data || !slots || slots>32 || materials.capacity<materials.count || materials.capacity>128 ||
           !TwinKartVisual::ReadMaterialOverrides(component,overrides,countOverrides)) {
            RuntimeEvent("native_hide_repaired_test_scene_material_header_rejected",-1,static_cast<int>(slots));
            ReleaseHideRepairedScene();hideRepairedScene.attempted=true;return false;
        }
        // This expressly selected repair authors the floor appearance even
        // when the prototype's checker material is a valid asset. Preserve
        // every previous instance override and restore only our own writes.
        for(unsigned slot=0;slot<slots;++slot) {
            if(overrides[slot] && !TwinKartLocalRace::CaptureFlagObject(static_cast<BYTE*>(overrides[slot]),floor.previousIdentity[slot])) {
                ReleaseHideRepairedScene();hideRepairedScene.attempted=true;return false;
            }
            floor.previous[slot]=overrides[slot];floor.changed|=1u<<slot;
            auto method=(*reinterpret_cast<BYTE***>(component))[0x570/8];
            if(method<image || method>=image+0x4ed0000) {ReleaseHideRepairedScene();hideRepairedScene.attempted=true;return false;}
            reinterpret_cast<void(__fastcall*)(void*,int,void*)>(method)(component,slot,hideRepairedScene.assets[3].object);
            void* after[32]{};unsigned afterCount=0;
            if(!HideMaterialIdentityLive(floor.component) || !HideMaterialIdentityLive(hideRepairedScene.assets[3]) ||
               !TwinKartVisual::ReadMaterialOverrides(component,after,afterCount) || after[slot]!=hideRepairedScene.assets[3].object) {
                ReleaseHideRepairedScene();hideRepairedScene.attempted=true;return false;
            }
        }
    }
    stage(7);
    // Native Hide's lazy gameplay components use its own component registry,
    // unlike the engine-owned visual components on our static mesh actors.
    using Type=void*(__fastcall*)();using Find=BYTE*(__fastcall*)(void*,void*);
    auto spawnType=reinterpret_cast<Type>(image+0x10f46e0)();
    auto spawner=reinterpret_cast<Find>((*reinterpret_cast<void***>(mode))[0x620/8])(mode,spawnType);
    if(!TwinKartLocalRace::OwnedComponent(spawner,mode,spawnType)) {
        RuntimeEvent("native_hide_repaired_test_scene_spawn_registry_missing",-1,0);
        ReleaseHideRepairedScene();hideRepairedScene.attempted=true;return false;
    }
    reinterpret_cast<void(__fastcall*)(void*)>(image+0x10ed040)(spawner);
    int hiders=*reinterpret_cast<int*>(spawner+0xd8),seekers=*reinterpret_cast<int*>(spawner+0xe8);
    if(hiders!=8 || seekers!=8){RuntimeEvent("native_hide_repaired_test_scene_spawn_registry_count",hiders,seekers);ReleaseHideRepairedScene();hideRepairedScene.attempted=true;return false;}
    hideRepairedScene.layout=plan;
    hideRepairedScene.ready=true;
    stage(8);
    RuntimeEvent("native_hide_repaired_test_scene_actor_count",-1,hideRepairedScene.actorCount);
    RuntimeEvent("native_hide_repaired_test_scene_width_cm",-1,static_cast<int>(plan.high.x-plan.low.x));
    RuntimeEvent("native_hide_repaired_test_scene_depth_cm",-1,static_cast<int>(plan.high.y-plan.low.y));
    RuntimeEvent("native_hide_repaired_test_scene_hider_starts",-1,hiders);
    RuntimeEvent("native_hide_repaired_test_scene_seeker_starts",-1,seekers);
    RuntimeEvent("native_hide_repaired_test_scene_original_prop_areas",-1,originalAreas);
    RuntimeEvent("native_hide_repaired_test_scene_prepared",-1,1);
    return true;
}
int RecordHideRepairFault(EXCEPTION_POINTERS* fault) {
    auto address=reinterpret_cast<BYTE*>(fault->ExceptionRecord->ExceptionAddress);
    RuntimeEvent("native_hide_repaired_test_scene_fault_code",hideRepairedScene.actorCount,static_cast<int>(fault->ExceptionRecord->ExceptionCode));
    RuntimeEvent("native_hide_repaired_test_scene_fault_rva",hideRepairedScene.actorCount,
        address>=image && address<image+0x8657000?static_cast<int>(address-image):0);
    return EXCEPTION_EXECUTE_HANDLER;
}
bool PrepareHideRepairedScene(BYTE* world,BYTE* mode) {
    // The native rendering activity observer catches SEH. Catch it here first
    // so an engine callback fault cannot silently strand a partial scene.
    __try {return PrepareHideRepairedSceneImpl(world,mode);}
    __except(RecordHideRepairFault(GetExceptionInformation())) {
        __try {ReleaseHideRepairedScene();}
        __except(EXCEPTION_EXECUTE_HANDLER) {hideRepairedScene=HideRepairedSceneState{};RuntimeEvent("native_hide_repaired_test_scene_cleanup_fault",-1,0);}
        hideRepairedScene.attempted=true;RuntimeEvent("native_hide_repaired_test_scene_fault_rolled_back",-1,1);return false;
    }
}
