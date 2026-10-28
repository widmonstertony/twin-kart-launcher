#include "../native/DriftObjectIdentity.h"
#include <cstdio>
#include <stdexcept>
#include <memory>
void Need(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main() {
    try {
        using namespace TwinKartCamera;
        BYTE object[32]{}; *reinterpret_cast<int*>(object + 12) = 65536;
        std::unique_ptr<ObjectSlot[]> chunk(new ObjectSlot[65536]{});
        ObjectSlot* chunks[2]{nullptr,chunk.get()};
        chunk[0].object = object; chunk[0].serial = 7;
        ObjectIdentity identity{};
        Need(Capture(object,chunks,65537,&identity) && Live(identity,chunks,65537),"Live second-chunk identity rejected");
        chunk[0].serial = 8; Need(!Live(identity,chunks,65537),"Recycled address/index accepted");
        chunk[0].serial = 7; chunk[0].object = nullptr; Need(!Live(identity,chunks,65537),"Freed slot accepted");
        chunk[0].object = object; chunk[0].flags = 0x20000000; Need(!Live(identity,chunks,65537),"Pending-kill slot accepted");
        chunk[0].flags = 0x10000000; Need(!Live(identity,chunks,65537),"Unreachable slot accepted");
        chunk[0].flags = 0; *reinterpret_cast<unsigned*>(object + 8) = 0x8000;
        Need(!Live(identity,chunks,65537),"Destruction-started object accepted");
        *reinterpret_cast<unsigned*>(object + 8) = 0; chunk[0].serial = 0;
        Need(!Capture(object,chunks,65537,&identity),"Unversioned object retained for later writes");
        chunk[0].serial = 7;
        Need(!Capture(object,chunks,65536,&identity) && !Capture(object,nullptr,65537,&identity),"Array bounds or null chunks ignored");
        Need(Capture(object,chunks,65537,&identity),"Fixture restoration failed");
        *reinterpret_cast<int*>(object + 12) = 1; Need(!Live(identity,chunks,65537),"Changed object index accepted");
        puts("PASS camera object lifetime: chunk boundaries, serial/address reuse, freed/pending/unreachable/destructing objects, unversioned objects and bounds. No game started.");
        return 0;
    } catch (const std::exception& error) { fprintf(stderr,"%s\n",error.what()); return 1; }
}
