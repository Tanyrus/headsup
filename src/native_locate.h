#pragma once

#include <cstdint>
#include <vector>

namespace headsup
{
    // A section of the client's image as loaded: the code exists only in memory, as FFXiMain.dll's .text is unpacked
    // at load.
    struct ImageSection
    {
        uint32_t rva;
        const uint8_t* bytes;
        uint32_t size;
    };

    // The game's name routine overwritten at hook, a whole number of instructions the gate runs again on its way to
    // resume.
    constexpr uint8_t kNameHookBytes[] = {0x33, 0xD2, 0x33, 0xED, 0x33, 0xF6}; // xor edx,edx; xor ebp,ebp; xor esi,esi

    enum class LocateProblem : uint8_t
    {
        None,
        NoIconTables,
        NoTableReads,
        AmbiguousTableReads,
        UnexpectedPrologue,
        PatchSiteChanged,
        UnexpectedExit,
        NoCallers,
        NoEntityCaller,
    };
    const char* Describe(LocateProblem problem);

    // RVAs.
    struct NameRoutine
    {
        LocateProblem problem = LocateProblem::None;
        uint32_t entry = 0, hook = 0, resume = 0, exit = 0;
        std::vector<uint32_t> callerReturns;
        uint32_t entityCaller = 0; // the one of those that entities' names come back to
    };

    // Found by the two icon tables it reads (from .rdata, which is in the file) rather than by its own bytes, so it
    // does not depend on where the client was loaded or on a committed copy of the game's code.
    NameRoutine LocateNameRoutine(uint32_t moduleBase, const ImageSection& text, const ImageSection& rdata);
}
