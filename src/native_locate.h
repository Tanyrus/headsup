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
        NoArrowDraws,
        AmbiguousArrowDraws,
        ArrowDrawsDiffer,
        NoPointerMapping,
        AmbiguousPointerMapping,
        PointerMappingDiffers,
        NoPointerShow,
        AmbiguousPointerShow,
        PointerShowChanged,
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

    // RVAs of the calls at the end of the target window's draw that draw its arrows, each a five-byte call: over the
    // target at (m_AnkX, m_AnkY), and over the candidate while a sub-target is picked at (m_SubAnkX, m_SubAnkY). Both go
    // to one shape draw, a thiscall that takes x, y, scale x, scale y, color and three zeros and pops them.
    struct ArrowCalls
    {
        LocateProblem problem = LocateProblem::None;
        uint32_t target = 0, picked = 0, draw = 0;
    };
    constexpr uint32_t kArrowArgumentBytes = 32;

    ArrowCalls LocateArrowCalls(const ImageSection& text);

    // Where the game guesses its window's client area as GetWindowRect less two SM_CXFRAME across and two SM_CYFRAME
    // and SM_CYCAPTION down (the borders zeroed only in window mode 3): when it reads a mouse message's position, when
    // it moves Windows' pointer to its own, and when the pointer at its area's edge pans the camera. Each is a call to
    // GetWindowRect through the import table and a load of GetSystemMetrics from it into a register, six bytes each.
    struct WindowGuess
    {
        uint32_t windowRectCall = 0, metricsLoad = 0; // RVAs
        uint8_t movImmediate = 0;                     // the opcode of mov reg, imm32 into the register metricsLoad fills
    };
    struct PointerMapping
    {
        LocateProblem problem = LocateProblem::None;
        std::vector<WindowGuess> guesses;
    };
    PointerMapping LocatePointerMapping(const ImageSection& text);

    // The game's mouse controller and its routine that shows or hides the pointer (a thiscall taking 1 to show and
    // popping it), found through the game's own call to it on a mouse move. The controller is a global whose address
    // the code holds; its byte at kPointerShownOffset is 1 while the pointer shows.
    struct PointerShow
    {
        LocateProblem problem   = LocateProblem::None;
        uint32_t controllerGlobal = 0; // the global's address in the running client, as the code holds it
        uint32_t show             = 0; // RVA
    };
    constexpr uint32_t kPointerShownOffset = 0x4E;
    PointerShow LocatePointerShow(const ImageSection& text);
}
