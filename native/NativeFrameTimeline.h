#pragma once
#include <cstdint>
#include "FrameGenerationD3D12.h"
namespace TwinKartGraphics {
// CPU correlation only, externally serialized. Tokens come from the provider,
// and are never synthesized by this ledger or inferred from Present counts.
class NativeFrameTimeline {
public:
    struct Ticket {
        std::uint64_t nativeId=0,time=0;
        unsigned index=0,seen=0,completed=0;
        void* token=nullptr;
        bool live=false,failed=false;
    };
private:
    Ticket tickets[16]{};
    std::uint64_t lastNative=0;
    unsigned nextIndex=0;
    bool haveLast=false;
public:
    Ticket* Advance(FrameStage stage,std::uint64_t id,std::uint64_t now) {
        if(static_cast<unsigned>(stage)>5)return nullptr;
        const unsigned bit=1u<<static_cast<unsigned>(stage);
        // Simulation and render submission are separate native threads. A
        // render-start callback may precede simulation-end for the same ID;
        // rejecting that real callback loses the whole ticket. Preserve the
        // observed order and require each phase's own start before its end.
        // Present is nested inside rendering. ForPresent still requires an
        // actually completed simulation-end before any frame can interpolate.
        constexpr unsigned prerequisites[]{0,1,1,5,5,21};
        const unsigned required=prerequisites[static_cast<unsigned>(stage)];
        auto& ticket=tickets[id%16];
        if(stage==FrameStage::SimulationStart) {
            if(haveLast && id<=lastNative)return nullptr;
            if(ticket.live && ticket.completed!=63 && now>=ticket.time && now-ticket.time<=1000)return nullptr;
            ticket=Ticket{};ticket.nativeId=id;ticket.time=now;ticket.index=++nextIndex;ticket.live=true;
            lastNative=id;haveLast=true;
        }
        if(!ticket.live || ticket.nativeId!=id || ticket.failed || now<ticket.time || now-ticket.time>1000 ||
           (ticket.seen&bit) || (ticket.completed&required)!=required)return nullptr;
        ticket.seen|=bit;return &ticket;
    }
    void Complete(std::uint64_t id,FrameStage stage,bool success) {
        auto& ticket=tickets[id%16];if(!ticket.live || ticket.nativeId!=id)return;
        if(success)ticket.completed|=1u<<static_cast<unsigned>(stage);else ticket.failed=true;
    }
    Ticket* ForPresent(std::uint64_t id,std::uint64_t now) {
        auto& ticket=tickets[id%16];
        return ticket.live && ticket.nativeId==id && !ticket.failed && ticket.token &&
            (ticket.completed&23)==23 && !(ticket.seen&32) && now>=ticket.time && now-ticket.time<=1000?&ticket:nullptr;
    }
    Ticket Inspect(std::uint64_t id) const {
        const auto& ticket=tickets[id%16];
        return ticket.live && ticket.nativeId==id?ticket:Ticket{};
    }
};
}
