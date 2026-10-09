#include "native_locate.h"

#include <cstring>
#include <iterator>

namespace headsup
{
    namespace
    {
        constexpr uint8_t kIconBaseTable[]  = {0xA9, 0xA9, 0xA9, 0xA9, 0xA9, 0xA9};
        constexpr uint8_t kIconCountTable[] = {0x00, 0x01, 0x02, 0x00, 0x01, 0x02};
        constexpr uint32_t kIconTableGap    = 8;
        // Both tables are indexed by icon code, and the codes start at 200, so the operands point 200 bytes short.
        constexpr uint32_t kFirstIconCode    = 200;
        constexpr uint32_t kBaseReadOperand  = 0x1F9;
        constexpr uint32_t kCountReadOperand = 0x1FF;

        constexpr uint8_t kPrologue[] = {0x81, 0xEC, 0xC4, 0x06, 0x00, 0x00, 0x53, 0x55, 0x56, 0x57}; // sub esp,0x6C4; push x4
        constexpr uint32_t kHookOffset = 0x1D1;
        constexpr uint32_t kExitOffset = 0x6D7;
        constexpr uint8_t kExitBytes[] = {0x8D, 0x8C, 0x24, 0x84, 0x00, 0x00, 0x00}; // lea ecx,[esp+0x84]
        constexpr uint32_t kRoutineBytes = kExitOffset + sizeof(kExitBytes);

        constexpr uint8_t kCallOpcode  = 0xE8;
        constexpr uint32_t kCallLength = 5;
        // Each arrow's pushes up to its call: the target's reads m_AnkX (0xBC), m_AnkY (0xBE) and m_AnkNum (0xBA) and pushes
        // its gray, the candidate's reads m_SubAnkY (0xC2) and m_SubAnkX (0xC0) and pushes edi, the range color.
        constexpr uint8_t kTargetArrowLead[] = {0x66, 0x8B, 0x86, 0xBC, 0x00, 0x00, 0x00, 0x66, 0x85, 0xC0, 0x74, 0x32, 0x0F,
            0xBF, 0x96, 0xBE, 0x00, 0x00, 0x00, 0x6A, 0x00, 0x6A, 0x00, 0x6A, 0x00, 0x33, 0xC9, 0x8A, 0x8E, 0xBA, 0x00, 0x00,
            0x00, 0x68, 0x80, 0x80, 0x80, 0xE0, 0x0F, 0xBF, 0xC0, 0x8B, 0x4C, 0x8E, 0x78, 0x68, 0x00, 0x00, 0x80, 0x3F, 0x68,
            0x00, 0x00, 0x80, 0x3F, 0x52, 0x50, 0xE8};
        constexpr uint8_t kPickedArrowLead[] = {0x0F, 0xBF, 0x96, 0xC2, 0x00, 0x00, 0x00, 0x0F, 0xBF, 0x86, 0xC0, 0x00, 0x00,
            0x00, 0x6A, 0x00, 0x6A, 0x00, 0x6A, 0x00, 0x33, 0xC9, 0x8A, 0x8E, 0xBA, 0x00, 0x00, 0x00, 0x57, 0x68, 0x00, 0x00,
            0x80, 0x3F, 0x68, 0x00, 0x00, 0x80, 0x3F, 0x8B, 0x4C, 0x8E, 0x78, 0x52, 0x50, 0xE8};

        // add esp,0x14; ret 0x14: the wrapper entities' names are drawn through hands back as soon as the routine does.
        constexpr uint8_t kEntityCallerTail[] = {0x83, 0xC4, 0x14, 0xC2, 0x14, 0x00};

        bool Matches(const ImageSection& section, uint32_t offset, const uint8_t* bytes, uint32_t count)
        {
            return offset <= section.size && section.size - offset >= count &&
                   std::memcmp(section.bytes + offset, bytes, count) == 0;
        }

        uint32_t Read32(const uint8_t* at)
        {
            uint32_t value = 0;
            std::memcpy(&value, at, sizeof(value));
            return value;
        }

        std::vector<uint32_t> IconTables(const ImageSection& rdata)
        {
            std::vector<uint32_t> found;
            for (uint32_t offset = 0; offset + kIconTableGap + sizeof(kIconCountTable) <= rdata.size; ++offset)
                if (Matches(rdata, offset, kIconBaseTable, sizeof(kIconBaseTable)) &&
                    Matches(rdata, offset + kIconTableGap, kIconCountTable, sizeof(kIconCountTable)))
                    found.push_back(rdata.rva + offset);
            return found;
        }

        // Offsets into .text of every routine entry that reads this table pair the way the name routine does.
        void AddTableReaders(uint32_t moduleBase, uint32_t table, const ImageSection& text, std::vector<uint32_t>& entries)
        {
            const uint32_t baseOperand  = moduleBase + table - kFirstIconCode;
            const uint32_t countOperand = baseOperand + kIconTableGap;
            constexpr uint32_t kGap     = kCountReadOperand - kBaseReadOperand;
            for (uint32_t at = kBaseReadOperand; at + kGap + sizeof(uint32_t) <= text.size; ++at)
            {
                if (Read32(text.bytes + at) != baseOperand || Read32(text.bytes + at + kGap) != countOperand) continue;
                const uint32_t entry = at - kBaseReadOperand;
                if (text.size - entry >= kRoutineBytes) entries.push_back(entry);
            }
        }

        // The RVA of the call each copy of lead ends in.
        std::vector<uint32_t> CallsAfter(const uint8_t* lead, uint32_t bytes, const ImageSection& text)
        {
            std::vector<uint32_t> calls;
            for (uint32_t at = 0; at + bytes + kCallLength - 1 <= text.size; ++at)
                if (Matches(text, at, lead, bytes)) calls.push_back(text.rva + at + bytes - 1);
            return calls;
        }

        uint32_t CallTarget(uint32_t call, const ImageSection& text)
        {
            return call + kCallLength + Read32(text.bytes + (call - text.rva) + 1);
        }

        std::vector<uint32_t> ReturnsFromCallsTo(uint32_t target, const ImageSection& text)
        {
            std::vector<uint32_t> returns;
            for (uint32_t at = 0; at + kCallLength <= text.size; ++at)
            {
                if (text.bytes[at] != kCallOpcode) continue;
                const uint32_t returnRva = text.rva + at + kCallLength;
                if (returnRva + Read32(text.bytes + at + 1) == target) returns.push_back(returnRva);
            }
            return returns;
        }
    }

    const char* Describe(LocateProblem problem)
    {
        switch (problem)
        {
        case LocateProblem::None: return "found";
        case LocateProblem::NoIconTables: return "the client's icon tables were not found";
        case LocateProblem::NoTableReads: return "no routine reads the client's icon tables";
        case LocateProblem::AmbiguousTableReads: return "more than one routine reads the client's icon tables";
        case LocateProblem::UnexpectedPrologue: return "the name routine does not start as expected";
        case LocateProblem::PatchSiteChanged: return "the name routine was already changed, likely by another plugin";
        case LocateProblem::UnexpectedExit: return "the name routine does not end as expected";
        case LocateProblem::NoCallers: return "nothing calls the name routine";
        case LocateProblem::NoEntityCaller: return "the call that draws entities' names was not found";
        case LocateProblem::NoArrowDraws: return "the target window's arrow draws were not found";
        case LocateProblem::AmbiguousArrowDraws: return "the target window's arrow draws were found more than once";
        case LocateProblem::ArrowDrawsDiffer: return "the target window's two arrows are not drawn by the same function";
        }
        return "unknown problem";
    }

    NameRoutine LocateNameRoutine(uint32_t moduleBase, const ImageSection& text, const ImageSection& rdata)
    {
        NameRoutine result;
        const std::vector<uint32_t> tables = IconTables(rdata);
        if (tables.empty())
        {
            result.problem = LocateProblem::NoIconTables;
            return result;
        }
        std::vector<uint32_t> entries;
        for (const uint32_t table : tables)
            AddTableReaders(moduleBase, table, text, entries);
        if (entries.size() != 1)
        {
            result.problem = entries.empty() ? LocateProblem::NoTableReads : LocateProblem::AmbiguousTableReads;
            return result;
        }
        const uint32_t entry = entries.front();
        if (!Matches(text, entry, kPrologue, sizeof(kPrologue)))
            result.problem = LocateProblem::UnexpectedPrologue;
        else if (!Matches(text, entry + kHookOffset, kNameHookBytes, sizeof(kNameHookBytes)))
            result.problem = LocateProblem::PatchSiteChanged;
        else if (!Matches(text, entry + kExitOffset, kExitBytes, sizeof(kExitBytes)))
            result.problem = LocateProblem::UnexpectedExit;
        if (result.problem != LocateProblem::None) return result;

        result.entry         = text.rva + entry;
        result.hook          = result.entry + kHookOffset;
        result.resume        = result.hook + static_cast<uint32_t>(std::size(kNameHookBytes));
        result.exit          = result.entry + kExitOffset;
        result.callerReturns = ReturnsFromCallsTo(result.entry, text);
        if (result.callerReturns.empty())
        {
            result.problem = LocateProblem::NoCallers;
            return result;
        }
        uint32_t wrappers = 0;
        for (const uint32_t returnRva : result.callerReturns)
            if (Matches(text, returnRva - text.rva, kEntityCallerTail, sizeof(kEntityCallerTail)))
            {
                result.entityCaller = returnRva;
                ++wrappers;
            }
        if (wrappers != 1) result.problem = LocateProblem::NoEntityCaller;
        return result;
    }

    ArrowCalls LocateArrowCalls(const ImageSection& text)
    {
        ArrowCalls result;
        const std::vector<uint32_t> target = CallsAfter(kTargetArrowLead, sizeof(kTargetArrowLead), text);
        const std::vector<uint32_t> picked = CallsAfter(kPickedArrowLead, sizeof(kPickedArrowLead), text);
        if (target.empty() || picked.empty()) result.problem = LocateProblem::NoArrowDraws;
        else if (target.size() > 1 || picked.size() > 1) result.problem = LocateProblem::AmbiguousArrowDraws;
        else if (CallTarget(target.front(), text) != CallTarget(picked.front(), text)) result.problem = LocateProblem::ArrowDrawsDiffer;
        else result = ArrowCalls{LocateProblem::None, target.front(), picked.front(), CallTarget(target.front(), text)};
        return result;
    }
}
