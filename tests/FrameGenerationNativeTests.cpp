#define TWIN_KART_FRAME_GENERATION_FIXTURE 1
#include "../native/FrameGenerationD3D12.cpp"
#include "../native/NativeFrameMarkers.cpp"
#include "../native/NativeViewMath.h"
#include <thread>
#include <mutex>
#include <vector>
#include <stdexcept>
using namespace TwinKartGraphics;
void Need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct FixtureToken final:sl::FrameToken {unsigned value=0;operator uint32_t() const override{return value;}};
FixtureToken tokens[16];
DWORD simulation=0,rendering=0;
std::atomic<unsigned> sleeps{0},created{0},options{0};
std::mutex capturesLock;
std::vector<std::pair<unsigned,sl::PCLMarker>> captures;
std::vector<unsigned> constantsTokens;
std::vector<bool> resets;
sl::Result NewToken(sl::FrameToken*& token,const uint32_t* index) {
    Need(GetCurrentThreadId()==simulation && index && *index>=1 && *index<=5,"Provider token requested on wrong thread or without native simulation");
    auto& selected=tokens[*index];selected.value=*index;token=&selected;++created;return sl::Result::eOk;
}
sl::Result Sleep(const sl::FrameToken&) {
    Need(GetCurrentThreadId()==simulation,"Reflex pacing still executes on the presentation thread");++sleeps;return sl::Result::eOk;
}
sl::Result Marker(sl::PCLMarker stage,const sl::FrameToken& token) {
    bool sim=stage==sl::PCLMarker::eSimulationStart || stage==sl::PCLMarker::eSimulationEnd;
    Need(GetCurrentThreadId()==(sim?simulation:rendering),"Marker was replayed on a different thread");
    std::lock_guard<std::mutex> lock(capturesLock);captures.emplace_back(static_cast<unsigned>(token),stage);return sl::Result::eOk;
}
sl::Result Constants(const sl::Constants& values,const sl::FrameToken& token,const sl::ViewportHandle&) {
    Need(GetCurrentThreadId()==rendering,"Native camera constants left their render owner");
    bool infinite=static_cast<unsigned>(token)==1 || static_cast<unsigned>(token)==5;
    Need(infinite?(std::isinf(values.cameraFar) && values.cameraFar>0):values.cameraFar==10000,"Camera bound changed or used Streamline's invalid sentinel");
    constantsTokens.push_back(static_cast<unsigned>(token));resets.push_back(values.reset==sl::Boolean::eTrue);return sl::Result::eOk;
}
sl::Result Options(const sl::ViewportHandle&,const sl::DLSSGOptions&){++options;return sl::Result::eOk;}
namespace TwinKartGraphics {
struct FrameGenerationFixture {
    static void Prepare(FrameGeneration& generation) {
        auto& impl=*generation.impl;impl.thread=GetCurrentThreadId();impl.configured=true;impl.bound=true;
        impl.frameToken=NewToken;impl.sleep=Sleep;impl.marker=Marker;impl.constants=Constants;impl.setOptions=Options;
    }
};
}
void __fastcall Original(void*,unsigned long long){}
int main(){HANDLE ready=nullptr,done=nullptr,renderStarted=nullptr,simulationEnded=nullptr;try{
    simulation=GetCurrentThreadId();FrameGeneration generation;FrameGenerationFixture::Prepare(generation);
    Need(generation.AttachNativeMarkers(),"Original native sink registration failed");
    for(unsigned stage=0;stage<6;++stage)originals[stage]=Original;
    ready=CreateEventW(nullptr,FALSE,FALSE,nullptr);done=CreateEventW(nullptr,FALSE,FALSE,nullptr);Need(ready && done,"Fixture thread events");
    renderStarted=CreateEventW(nullptr,FALSE,FALSE,nullptr);simulationEnded=CreateEventW(nullptr,FALSE,FALSE,nullptr);Need(renderStarted && simulationEnded,"Overlapping native phase events");
    std::atomic<bool> threadOkay{true};constexpr unsigned long long base=0x100000000ull;
    std::thread worker([&]{try{
        rendering=GetCurrentThreadId();
        for(unsigned frame=0;frame<5;++frame) {
            Need(WaitForSingleObject(ready,10000)==WAIT_OBJECT_0,"Simulation did not submit frame");
            if(!frame)Need(generation.AdoptInitialRenderThread(),"Serialized initial render-owner handoff failed");
            float view[576]{};view[536]=640;view[537]=360;
            float projection[16]={1.125f,0,0,0,0,2,0,0,0,0,0,1,0,0,10,0};
            memcpy(view+32,projection,64);memcpy(view+144,projection,64);view[504]=view[509]=view[514]=view[519]=1;
            CameraFrame camera{};Need(DecodeNativeCamera(view,640,360,false,camera),"Infinite reversed-Z fixture decode failed");
            if(frame&1)camera.farPlane=10000;
            bool nested=(frame&1)!=0;
            auto id=base+frame;hooks[2](nullptr,id);
            if(frame==4){SetEvent(renderStarted);Need(WaitForSingleObject(simulationEnded,10000)==WAIT_OBJECT_0,"Actual simulation-end never reached overlapping render frame");}
            if(!nested)hooks[3](nullptr,id);
            Need(generation.BeginNativeFrame(id,camera)!=0,"Native frame accepted before actual PresentStart");
            hooks[4](nullptr,id);
            if(!frame) {
                auto invalid=camera;invalid.farPlane=sl::INVALID_FLOAT;
                Need(generation.BeginNativeFrame(id,invalid)!=0,"SL invalid far-plane sentinel was submitted");
                invalid=camera;invalid.farPlane=-std::numeric_limits<float>::infinity();
                Need(generation.BeginNativeFrame(id,invalid)!=0,"Negative infinite bound accepted");
                invalid=camera;invalid.farPlane=std::numeric_limits<float>::quiet_NaN();
                Need(generation.BeginNativeFrame(id,invalid)!=0,"NaN bound accepted");
                invalid=camera;invalid.invertedDepth=false;
                Need(generation.BeginNativeFrame(id,invalid)!=0,"Infinite bound without matching reversed depth accepted");
                invalid=camera;invalid.viewToClip[10]=0.5f;
                Need(generation.BeginNativeFrame(id,invalid)!=0,"Infinite bound paired with finite projection accepted");
            }
            Need(generation.BeginFrame(123,camera)!=0,"Present counter allocated a competing frame token");
            if(frame!=2) {
                Need(generation.BeginNativeFrame(id+99,camera)!=0,"Different native frame ID accepted");
                Need(generation.BeginNativeFrame(id,camera)==0,"Qualified native frame/camera token correlation failed");
            }
            hooks[5](nullptr,id);if(nested)hooks[3](nullptr,id);SetEvent(done);
        }
    }catch(...){threadOkay=false;SetEvent(done);}});
    for(unsigned frame=0;frame<5;++frame) {
        hooks[0](nullptr,base+frame);if(frame!=4)hooks[1](nullptr,base+frame);SetEvent(ready);
        if(frame==4){Need(WaitForSingleObject(renderStarted,10000)==WAIT_OBJECT_0,"Render thread never entered native overlap");hooks[1](nullptr,base+frame);SetEvent(simulationEnded);}
        Need(WaitForSingleObject(done,10000)==WAIT_OBJECT_0,"Render/present fixture timeout");
        if(!threadOkay)break;
    }
    worker.join();Need(threadOkay,"Original native/Streamline boundary fixture failed");
    Need(created==5 && sleeps==5 && captures.size()==30,"Missing native marker/token or duplicate presentation pacing");
    const sl::PCLMarker stages[]{sl::PCLMarker::eSimulationStart,sl::PCLMarker::eSimulationEnd,sl::PCLMarker::eRenderSubmitStart,
        sl::PCLMarker::eRenderSubmitEnd,sl::PCLMarker::ePresentStart,sl::PCLMarker::ePresentEnd};
    const unsigned nestedOrder[]{0,1,2,4,5,3};
    const unsigned overlapOrder[]{0,2,1,3,4,5};
    for(unsigned i=0;i<30;++i){unsigned frame=i/6,stage=i%6;unsigned expected=frame==4?overlapOrder[stage]:(frame&1)?nestedOrder[stage]:stage;
        Need(captures[i].first==frame+1 && captures[i].second==stages[expected],"Actual native linear/nested marker order or provider token changed");}
    Need(constantsTokens==std::vector<unsigned>({1,2,4,5}) && resets==std::vector<bool>({true,false,true,false}),"Skipped presentation did not reset history or used Present-count tokens");
    Need(!generation.AdoptInitialRenderThread(),"Initial handoff accepted an already used temporal token");
    Need(generation.AdoptDrainedRenderThread() && generation.SetActive(false)==0,"Serialized drained owner handoff failed");
    Need(generation.AdoptDrainedRenderThread() && options==1,"Repeated same-owner handoff changed provider options");
    generation.Shutdown();Need(options==2,"Serialized cross-thread cleanup did not disable generation");
    hooks[0](nullptr,base+5);Need(created==5,"Detached provider still consumed native simulation callback");
    CloseHandle(ready);CloseHandle(done);CloseHandle(renderStarted);CloseHandle(simulationEnded);ready=done=renderStarted=simulationEnded=nullptr;
    puts("PASS original native-to-Streamline boundary: actual simulation/render threads, 64-bit native IDs, provider-created tokens, simulation-only pacing, real-time six-stage forwarding, camera correlation, skipped-frame reset and detach/cleanup. Synthetic provider only; no game or NVIDIA runtime started.");return 0;
}catch(const std::exception& e){if(ready)CloseHandle(ready);if(done)CloseHandle(done);if(renderStarted)CloseHandle(renderStarted);if(simulationEnded)CloseHandle(simulationEnded);fprintf(stderr,"%s\n",e.what());return 1;}}
