#pragma once
#include <atomic>
#include <cstdint>
namespace TwinKartGraphics {
// Thread-safe, expiring publication from the owned game thread. The render
// consumer reads no UObject or mutable world pointer across threads.
class RenderActivity {
    std::atomic<std::uint64_t> racingAt{0};
public:
    void Publish(bool racing,std::uint64_t now){racingAt.store(racing?now:0,std::memory_order_release);}
    bool Active(std::uint64_t now) const {
        auto stamp=racingAt.load(std::memory_order_acquire);
        return stamp && now>=stamp && now-stamp<=1000;
    }
};
}
