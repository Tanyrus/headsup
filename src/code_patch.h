#pragma once

#include "function_ref.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace headsup
{
    // A few bytes of the client's code that HeadsUp writes its own over while it runs.
    struct CodePatch
    {
        static constexpr uint32_t kMaxBytes = 6; // the longest: a five-byte instruction and a nop
        uint8_t* at    = nullptr;
        uint32_t bytes = 0;
        uint8_t game[kMaxBytes] = {}; // as Apply found them
        uint8_t ours[kMaxBytes] = {};
    };

    // A call or jump: its opcode, then where it goes, counted from the instruction after it.
    constexpr uint8_t kCallOpcode = 0xE8, kJumpOpcode = 0xE9;
    constexpr uint32_t kBranchLength = 5;
    // The operand of a call or jump at `address` in the client that lands on `to`.
    uint32_t BranchOperand(uint32_t address, uint32_t to);
    // Sends the call or jump at `branch`, which is at `address` in the client, to `to` by changing its operand alone.
    CodePatch Retarget(uint8_t* branch, uint32_t address, uint32_t to);
    // An opcode and its 32-bit operand at `at`, then nops up to `bytes`, from kBranchLength to kMaxBytes.
    CodePatch Instruction(uint8_t* at, uint32_t bytes, uint8_t opcode, uint32_t operand);

    // Changes the client's code; the plugin's writer unprotects the bytes around the copy and flushes the instruction
    // cache.
    using CodeWriter = FunctionRef<bool(uint8_t* at, const uint8_t* bytes, size_t count)>;

    enum class Applied : uint8_t
    {
        All,
        None,
        Partly, // putting the game's bytes back was refused too, so some of ours are still there
    };
    // Keeps the game's bytes and writes ours over them, all or none: when one cannot be written, the ones before it get
    // the game's bytes back, last first.
    Applied Apply(std::span<CodePatch> patches, CodeWriter write);

    enum class Restored : uint8_t
    {
        All,
        ChangedSince, // another tool wrote over ours, and its bytes were left as they are
        NotWritten,
    };
    // Puts the game's bytes back wherever ours still are. Every patch is tried, and the last that was not says why.
    Restored Restore(std::span<const CodePatch> patches, CodeWriter write);
}
