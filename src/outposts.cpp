#include "outposts.h"

#include "bytes.h"

#include <charconv>
#include <cstdio>

namespace headsup
{
    namespace
    {
        constexpr uint16_t kFirstGuardEvent = 32758, kLastGuardEvent = 32763; // Windurst, Bastok, San d'Oria: two each
        constexpr uint32_t kNumbersAt = 0x08, kEventAt = 0x2C;
        constexpr size_t kMaskNumber  = 4;
        constexpr int kFirstRunBit    = 5;
        // The cities' regions: an overseer there tells every run (Jeuno's, the next, does not).
        constexpr uint8_t kFirstCityRegion = 19, kLastCityRegion = 21;
        constexpr std::array<int32_t, 8> kDelivery{16, 0, 0, 0, 1, 0, 0, 255}; // 'you have brought us supplies!'

        size_t SlotOf(Nation nation) { return static_cast<size_t>(nation) - static_cast<size_t>(Nation::SanDoria); }
        bool IsNation(Nation n) { return n == Nation::SanDoria || n == Nation::Bastok || n == Nation::Windurst; }
        uint32_t BitOf(uint8_t region) { return 1u << (region + kFirstRunBit); }
    }

    std::optional<GuardEvent> ParseGuardEvent(const uint8_t* data, uint32_t size)
    {
        if (data == nullptr || size < kEventAt + sizeof(uint16_t)) return std::nullopt;
        const auto event = ReadAt<uint16_t>(data, kEventAt);
        if (event < kFirstGuardEvent || event > kLastGuardEvent) return std::nullopt;
        constexpr Nation kByPair[] = {Nation::Windurst, Nation::Bastok, Nation::SanDoria};
        GuardEvent e{kByPair[(event - kFirstGuardEvent) / 2], {}};
        for (size_t i = 0; i < e.numbers.size(); ++i)
            e.numbers[i] = ReadAt<int32_t>(data, kNumbersAt + 4 * i);
        return e;
    }

    bool OutpostRuns::Heard(const GuardEvent& event, uint8_t region)
    {
        if (!IsNation(event.nation)) return false;
        Known& known      = m_Nations[SlotOf(event.nation)];
        const Known was   = known;
        if (region >= kFirstCityRegion && region <= kLastCityRegion)
        {
            known.told = ~0u;
            known.ran  = static_cast<uint32_t>(event.numbers[kMaskNumber]);
        }
        else if (region < kConquestRegionCount && event.numbers == kDelivery)
        {
            known.told |= BitOf(region);
            known.ran |= BitOf(region);
        }
        return known != was;
    }

    std::optional<bool> OutpostRuns::Ran(Nation nation, uint8_t region) const
    {
        if (!IsNation(nation) || region >= kConquestRegionCount) return std::nullopt;
        const Known& known = m_Nations[SlotOf(nation)];
        if ((known.told & BitOf(region)) == 0) return std::nullopt;
        return (known.ran & BitOf(region)) != 0;
    }

    std::string OutpostLines(uint32_t character, const OutpostRuns& runs)
    {
        std::string text;
        for (size_t n = 0; n < runs.m_Nations.size(); ++n)
        {
            const OutpostRuns::Known& known = runs.m_Nations[n];
            if (known.told == 0) continue;
            char line[64];
            std::snprintf(line, sizeof(line), "%u %zu %x %x\n", character, n + static_cast<size_t>(Nation::SanDoria),
                known.told, known.ran);
            text += line;
        }
        return text;
    }

    bool ReadOutpostLine(std::string_view line, std::map<uint32_t, OutpostRuns>& into)
    {
        uint32_t values[4] = {};
        const int bases[4] = {10, 10, 16, 16};
        const char* at = line.data();
        const char* end = line.data() + line.size();
        for (int i = 0; i < 4; ++i)
        {
            while (at < end && *at == ' ')
                ++at;
            const auto [next, problem] = std::from_chars(at, end, values[i], bases[i]);
            if (problem != std::errc{}) return false;
            at = next;
        }
        const auto nation = static_cast<Nation>(values[1]);
        if (at != end || !IsNation(nation)) return false;
        into[values[0]].m_Nations[SlotOf(nation)] = OutpostRuns::Known{values[2], values[3]};
        return true;
    }

    std::string OutpostFileText(const OutpostFile& file)
    {
        std::string text;
        for (const auto& [character, runs] : file)
            text += OutpostLines(character, runs);
        return text;
    }

    OutpostFile OutpostFileFrom(std::string_view text)
    {
        OutpostFile file;
        while (!text.empty())
        {
            const size_t end = text.find('\n');
            ReadOutpostLine(text.substr(0, end), file);
            text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);
        }
        return file;
    }
}
