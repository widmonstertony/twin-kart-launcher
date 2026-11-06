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
inline constexpr unsigned ChaserPhaseComponentGetter=0x2250f20,ChaserGameNameRva=0x68b9e50;
inline unsigned ScenePhaseComponentGetter(int mode,bool ownedFactory,bool ownedChaser=false) {
    // Factory has its own original phase component. Its local forty-seat
    // lease is independent of the ordinary race lease and must be present
    // before its driving/render gates are considered.
    return mode==108 && ownedChaser?ChaserPhaseComponentGetter:
        mode==100 && ownedFactory?0x2431330:mode==104?0x22ac6a0:RacePhaseComponentGetter;
}
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
