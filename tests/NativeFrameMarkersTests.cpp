#include "../native/NativeFrameMarkers.cpp"
#include "../native/NativeFrameTimeline.h"
#include <stdexcept>
#include <thread>
#include <mutex>
#include <vector>
using namespace TwinKartGraphics;
void Need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int object=0;
std::atomic<unsigned> nativeCalls[6]{};
template<unsigned Stage> void __fastcall Original(void* self,unsigned long long id) {
    Need(self==&object && id<3,"Native method arguments changed");++nativeCalls[Stage];
}
struct FixtureSink:NativeFrameMarkerSink {
    std::mutex lock;std::vector<unsigned> sequence;bool fail=false;
    void Observe(FrameStage stage,unsigned long long frame) override {
        std::lock_guard<std::mutex> hold(lock);
        if(fail)throw std::runtime_error("Original fixture exception");
        sequence.push_back(static_cast<unsigned>(frame*6)+static_cast<unsigned>(stage));
    }
};
int main(){Method* storage=nullptr;try{
    storage=static_cast<Method*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));Need(storage!=nullptr,"Fixture interface allocation");
    Method expected[6]{Original<0>,Original<1>,Original<2>,Original<3>,Original<4>,Original<5>};Method* slots[6]{};
    for(unsigned i=0;i<6;++i){storage[i]=expected[i];slots[i]=storage+i;}
    DWORD protection=0;Need(VirtualProtect(storage,4096,PAGE_READONLY,&protection)!=FALSE,"Fixture interface protection");
    Method incorrect[6];memcpy(incorrect,expected,sizeof(expected));incorrect[3]=expected[4];
    Need(!Install(slots,incorrect),"Foreign native slot accepted");for(unsigned i=0;i<6;++i)Need(storage[i]==expected[i],"Rejected install partially patched native interface");
    Need(Install(slots,expected),"Exact native marker interface replacement failed");
    auto selected=std::make_shared<FixtureSink>();auto other=std::make_shared<FixtureSink>();
    Need(BindNativeFrameMarkerSink(selected) && !BindNativeFrameMarkerSink(other),"Multiple live native sinks accepted");
    RemoveNativeFrameMarkerSink(other);unsigned long long native=99;Need(!CurrentNativeRenderFrame(native),"Unobserved render frame fabricated");
    for(unsigned frame=0;frame<2;++frame) {
        storage[0](&object,frame);storage[1](&object,frame);
        Need(!CurrentNativeRenderFrame(native),"Simulation thread acquired another thread's render frame");
        bool okay=false;
        std::thread render([&]{
            for(unsigned stage=2;stage<6;++stage)storage[stage](&object,frame);
            okay=CurrentNativeRenderFrame(native) && native==frame;
        });render.join();Need(okay,"Actual render/present callback lost native frame0/thread identity");
    }
    Need(selected->sequence.size()==12,"Native simulation/render/present callbacks missing");
    for(unsigned i=0;i<12;++i)Need(selected->sequence[i]==i,"Native stage or frame order changed");
    selected->fail=true;storage[0](&object,2);Need(nativeCalls[0]==3,"Sink exception prevented original native method");
    RemoveNativeFrameMarkerSink(selected);storage[1](&object,2);Need(nativeCalls[1]==3 && selected->sequence.size()==12,"Detached sink still consumed native callback");
    NativeFrameTimeline timeline;
    Need(!timeline.Advance(FrameStage::SimulationEnd,0,100),"Missing native simulation start fabricated");
    for(unsigned stage=0;stage<5;++stage) {
        auto entry=timeline.Advance(static_cast<FrameStage>(stage),0,100+stage);Need(entry!=nullptr,"Complete native marker order rejected");
        if(!stage)entry->token=&object;
        timeline.Complete(0,static_cast<FrameStage>(stage),true);
    }
    Need(timeline.ForPresent(0,106)!=nullptr && !timeline.ForPresent(1,106),"Native frame0 correlation or mismatched ID rejection failed");
    Need(!timeline.Advance(FrameStage::PresentStart,0,106),"Duplicate native marker accepted");
    Need(timeline.Advance(FrameStage::PresentEnd,0,107)!=nullptr,"Native present end rejected");timeline.Complete(0,FrameStage::PresentEnd,true);
    Need(!timeline.ForPresent(0,108) && !timeline.ForPresent(0,2000),"Already-presented or stale token reused");
    Need(!timeline.Advance(FrameStage::SimulationStart,0,109),"Nonmonotonic simulation ID accepted");
    auto failed=timeline.Advance(FrameStage::SimulationStart,1,110);Need(failed!=nullptr,"Next native frame rejected");failed->token=&object;
    timeline.Complete(1,FrameStage::SimulationStart,false);Need(!timeline.Advance(FrameStage::SimulationEnd,1,111),"Provider failure still advanced frame");
    Need(!timeline.Advance(static_cast<FrameStage>(64),100,120),"Invalid stage accepted");
    VirtualFree(storage,0,MEM_RELEASE);storage=nullptr;
    puts("PASS actual original marker interface hooks: six native stages, unchanged original methods, frame0/native IDs, simulation versus render threads, exact-slot rejection, sink lifetime/exception isolation, provider failure, duplicate/stale/out-of-order rejection. No game or NVIDIA runtime started.");return 0;
}catch(const std::exception& e){if(storage)VirtualFree(storage,0,MEM_RELEASE);fprintf(stderr,"%s\n",e.what());return 1;}}
