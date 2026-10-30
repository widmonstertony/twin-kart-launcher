#pragma once
#include <cstdint>
namespace TwinKartGraphics {
// Exact-build interface facts independently observed in property metadata,
// native phase construction and static FName initialization. A transition
// rule is contained by a UPhase; it is never the CurrentPhase object itself.
inline constexpr unsigned RacePhaseComponentGetter=0x23cc5f0;
inline constexpr unsigned PhaseObjectGetter=0x23dcd00;
inline constexpr unsigned CurrentPhaseOffset=0x110,PhaseNameOffset=0x28;
inline constexpr unsigned BeginRaceNameRva=0x68ba7f0,RetireCountdownNameRva=0x68ba800;
inline bool ClassifyNativeRacePhase(const void* actualClass,const void* phaseClass,
    std::uint64_t currentName,std::uint64_t beginRaceName,std::uint64_t retireName,bool paused) {
    if(paused || !actualClass || !phaseClass || actualClass!=phaseClass ||
       !currentName || !beginRaceName || !retireName || beginRaceName==retireName)return false;
    return currentName==beginRaceName || currentName==retireName;
}
inline bool ClassifyNativeDrivingPhase(const void* actualClass,const void* phaseClass,
    std::uint64_t currentName,std::uint64_t beginRaceName,bool paused) {
    return !paused && actualClass && phaseClass && actualClass==phaseClass &&
        currentName && beginRaceName && currentName==beginRaceName;
}
}
