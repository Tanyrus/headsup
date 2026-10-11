#include "code_patch.h"

#include <algorithm>
#include <cstring>

namespace headsup
{
    namespace
    {
        constexpr uint8_t kNop = 0x90;
    }

    uint32_t BranchOperand(uint32_t address, uint32_t to) { return to - (address + kBranchLength); }

    CodePatch Retarget(uint8_t* branch, uint32_t address, uint32_t to)
    {
        const uint32_t operand = BranchOperand(address, to);
        CodePatch patch{branch + 1, sizeof(operand)};
        std::memcpy(patch.ours, &operand, sizeof(operand));
        return patch;
    }

    CodePatch Instruction(uint8_t* at, uint32_t bytes, uint8_t opcode, uint32_t operand)
    {
        CodePatch patch{at, bytes};
        patch.ours[0] = opcode;
        std::memcpy(patch.ours + 1, &operand, sizeof(operand));
        std::fill(patch.ours + 1 + sizeof(operand), patch.ours + bytes, kNop);
        return patch;
    }

    Applied Apply(std::span<CodePatch> patches, CodeWriter write)
    {
        for (size_t i = 0; i < patches.size(); ++i)
        {
            std::memcpy(patches[i].game, patches[i].at, patches[i].bytes);
            if (write(patches[i].at, patches[i].ours, patches[i].bytes)) continue;
            Applied result = Applied::None;
            while (i-- > 0)
                if (!write(patches[i].at, patches[i].game, patches[i].bytes)) result = Applied::Partly;
            return result;
        }
        return Applied::All;
    }

    Restored Restore(std::span<const CodePatch> patches, CodeWriter write)
    {
        Restored result = Restored::All;
        for (const CodePatch& patch : patches)
        {
            if (std::memcmp(patch.at, patch.ours, patch.bytes) != 0)
                result = Restored::ChangedSince;
            else if (!write(patch.at, patch.game, patch.bytes))
                result = Restored::NotWritten;
        }
        return result;
    }
}
