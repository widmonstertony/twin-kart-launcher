#define TWIN_KART_FRAME_GENERATION_FIXTURE 1
#include "../native/FrameGenerationD3D12.cpp"
#include "../native/NativeFrameMarkers.cpp"
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
    Need(GetCurrentThreadId()==simulation && index && *index>=1 && *index<=4,"Provider token requested on wrong thread or without native simulation");
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
int main(){HANDLE ready=nullptr,done=nullptr;try{
    simulation=GetCurrentThreadId();FrameGeneration generation;FrameGenerationFixture::Prepare(generation);
    Need(generation.AttachNativeMarkers(),"Original native sink registration failed");
    for(unsigned stage=0;stage<6;++stage)originals[stage]=Original;
    ready=CreateEventW(nullptr,FALSE,FALSE,nullptr);done=CreateEventW(nullptr,FALSE,FALSE,nullptr);Need(ready && done,"Fixture thread events");
    std::atomic<bool> threadOkay{true};constexpr unsigned long long base=0x100000000ull;
    std::thread worker([&]{try{
        rendering=GetCurrentThreadId();
        for(unsigned frame=0;frame<4;++frame) {
            Need(WaitForSingleObject(ready,10000)==WAIT_OBJECT_0,"Simulation did not submit frame");
            if(!frame)Need(generation.AdoptInitialRenderThread(),"Serialized initial render-owner handoff failed");
            CameraFrame camera{};camera.nearPlane=1;camera.farPlane=10000;camera.fieldOfView=1;camera.aspect=16.0f/9;
            camera.motionScaleX=camera.motionScaleY=1;
            auto id=base+frame;hooks[2](nullptr,id);hooks[3](nullptr,id);
            Need(generation.BeginNativeFrame(id,camera)!=0,"Native frame accepted before actual PresentStart");
            hooks[4](nullptr,id);
            Need(generation.BeginFrame(123,camera)!=0,"Present counter allocated a competing frame token");
            if(frame!=2) {
                Need(generation.BeginNativeFrame(id+99,camera)!=0,"Different native frame ID accepted");
                Need(generation.BeginNativeFrame(id,camera)==0,"Qualified native frame/camera token correlation failed");
            }
            hooks[5](nullptr,id);SetEvent(done);
        }
    }catch(...){threadOkay=false;SetEvent(done);}});
    for(unsigned frame=0;frame<4;++frame) {
        hooks[0](nullptr,base+frame);hooks[1](nullptr,base+frame);SetEvent(ready);
        Need(WaitForSingleObject(done,10000)==WAIT_OBJECT_0,"Render/present fixture timeout");
        if(!threadOkay)break;
    }
    worker.join();Need(threadOkay,"Original native/Streamline boundary fixture failed");
    Need(created==4 && sleeps==4 && captures.size()==24,"Missing native marker/token or duplicate presentation pacing");
    const sl::PCLMarker stages[]{sl::PCLMarker::eSimulationStart,sl::PCLMarker::eSimulationEnd,sl::PCLMarker::eRenderSubmitStart,
        sl::PCLMarker::eRenderSubmitEnd,sl::PCLMarker::ePresentStart,sl::PCLMarker::ePresentEnd};
    for(unsigned i=0;i<24;++i)Need(captures[i].first==i/6+1 && captures[i].second==stages[i%6],"Native frame token/stage order changed");
    Need(constantsTokens==std::vector<unsigned>({1,2,4}) && resets==std::vector<bool>({true,false,true}),"Skipped presentation did not reset history or used Present-count tokens");
    generation.Shutdown();Need(options==1,"Serialized cross-thread cleanup did not disable generation");
    hooks[0](nullptr,base+4);Need(created==4,"Detached provider still consumed native simulation callback");
    CloseHandle(ready);CloseHandle(done);ready=done=nullptr;
    puts("PASS original native-to-Streamline boundary: actual simulation/render threads, 64-bit native IDs, provider-created tokens, simulation-only pacing, real-time six-stage forwarding, camera correlation, skipped-frame reset and detach/cleanup. Synthetic provider only; no game or NVIDIA runtime started.");return 0;
}catch(const std::exception& e){if(ready)CloseHandle(ready);if(done)CloseHandle(done);fprintf(stderr,"%s\n",e.what());return 1;}}
