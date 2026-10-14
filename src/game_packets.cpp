#include "game_packets.h"

#include "argb.h"
#include "bytes.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <iterator>

namespace headsup
{
    namespace
    {
        constexpr uint32_t kZoneInWeather = 0x68, kZoneInPlayer = 0x04, kWeatherNumber = 0x08;
        constexpr uint32_t kFirstOwner = 0x1D, kRegionStride = 4;
        constexpr uint32_t kSanDoriaShare = 0x86, kBastokShare = 0x87, kWindurstShare = 0x88, kBeastmenShare = 0x94;
        constexpr unsigned kWholePercent = 100;
        const char* const kNationNames[] = {"", "San d'Oria", "Bastok", "Windurst", "Beastmen"};
        constexpr uint32_t kNationColors[] = {0xFF8D8D8D, 0xFFE05A5A, 0xFF5B8DEF, kBarGreen, 0xFFB38AD8};
        constexpr uint32_t kInspectId = 0x04, kInspectIndex = 0x08, kInspectMode = 0x0A, kInspectLinkshell = 0x10;
        constexpr uint32_t kInspectJobs = 0x22, kInspectLevels = 0x24, kInspectMasterLevel = 0x27;
        constexpr uint32_t kInspectGeneralLeast = 0x29; // through the master flags
        constexpr uint8_t kGeneralMode          = 0x01;
        constexpr size_t kLinkshellBytes        = 16;
        // Phoenix's ItemLockFlg::Linkshell on the item, and where its Exdata::Linkshell keeps the packed name.
        constexpr uint32_t kWornLinkshellFlags  = 0x13;
        constexpr size_t kItemLinkshell = 0x09, kItemLinkshellBytes = 15;
        constexpr uint32_t kInspectName = 0x80, kInspectTitle = 0x90, kInspectMessageBytes = 0x94;
        constexpr size_t kNameBytes             = 16;
        constexpr uint32_t kEntityIndex = 0x08, kEntitySends = 0x0A, kEntityFlags = 0x21;
        constexpr uint8_t kSendsFlags = 0x04, kSendsDespawn = 0x20; // UPDATE_HP and UPDATE_DESPAWN
        constexpr uint32_t kUntargetableFlag = 0x800;                // EntityFlags::Untargetable
        constexpr unsigned kCodeBits = 6, kLetters = 26, kDigits = 10, kEndCode = 63, kByteBits = 8;
        constexpr size_t kMostLinkshellLetters = 20;
    }

    std::optional<Weather> ParseZoneInWeather(const uint8_t* data, uint32_t size)
    {
        if (data == nullptr || size < kZoneInWeather + sizeof(uint16_t)) return std::nullopt;
        return WeatherFromId(ReadAt<uint16_t>(data, kZoneInWeather));
    }

    std::optional<uint32_t> ParseZoneInPlayer(const uint8_t* data, uint32_t size)
    {
        if (data == nullptr || size < kZoneInPlayer + sizeof(uint32_t)) return std::nullopt;
        return ReadAt<uint32_t>(data, kZoneInPlayer);
    }

    std::optional<Weather> ParseWeather(const uint8_t* data, uint32_t size)
    {
        if (data == nullptr || size < kWeatherNumber + sizeof(uint16_t)) return std::nullopt;
        return WeatherFromId(ReadAt<uint16_t>(data, kWeatherNumber));
    }

    std::optional<RegionOwners> ParseConquest(const uint8_t* data, uint32_t size)
    {
        if (data == nullptr || size <= kFirstOwner + kRegionStride * (kConquestRegionCount - 1)) return std::nullopt;
        RegionOwners owners{};
        for (int r = 0; r < kConquestRegionCount; ++r)
        {
            const uint8_t owner = data[kFirstOwner + kRegionStride * static_cast<uint32_t>(r)];
            owners[r] = owner <= static_cast<uint8_t>(Nation::Beastmen) ? static_cast<Nation>(owner) : Nation::None;
        }
        return owners;
    }

    std::optional<RegionShares> ParseRegionShares(const uint8_t* data, uint32_t size)
    {
        if (data == nullptr || size <= kBeastmenShare) return std::nullopt;
        RegionShares shares{{{Nation::SanDoria, data[kSanDoriaShare]}, {Nation::Bastok, data[kBastokShare]},
            {Nation::Windurst, data[kWindurstShare]}, {Nation::Beastmen, data[kBeastmenShare]}}};
        unsigned total = 0;
        for (const RegionShare& share : shares)
            total += share.percent;
        if (total > kWholePercent) return std::nullopt;
        std::ranges::stable_sort(shares, std::ranges::greater{}, &RegionShare::percent);
        return shares;
    }

    const char* NationName(Nation nation)
    {
        return kNationNames[std::min<size_t>(static_cast<size_t>(nation), std::size(kNationNames) - 1)];
    }

    uint32_t NationColor(Nation nation)
    {
        return kNationColors[std::min<size_t>(static_cast<size_t>(nation), std::size(kNationColors) - 1)];
    }

    std::string LinkshellName(const uint8_t* packed, size_t bytes)
    {
        std::string name;
        if (packed == nullptr) return name;
        for (size_t i = 0; i < kMostLinkshellLetters && (i + 1) * kCodeBits <= bytes * kByteBits; ++i)
        {
            unsigned code = 0;
            for (unsigned b = 0; b < kCodeBits; ++b)
            {
                const size_t bit = i * kCodeBits + b; // from each byte's top bit down
                code = code << 1 | ((packed[bit / kByteBits] >> (kByteBits - 1 - bit % kByteBits)) & 1u);
            }
            if (code == kEndCode) break;
            if (code == 0)
            {
                // A name 2 mod 4 letters long ends on four 1 bits, so a 60 ('7') reads before the zeros; Phoenix's
                // DecodeStringLinkshell drops the letter before a 0 too.
                if (!name.empty()) name.pop_back();
                break;
            }
            if (code <= kLetters)
                name += static_cast<char>('a' + code - 1);
            else if (code <= 2 * kLetters)
                name += static_cast<char>('A' + code - kLetters - 1);
            else if (code <= 2 * kLetters + kDigits)
                name += static_cast<char>('0' + code - 2 * kLetters - 1);
        }
        return name;
    }

    std::optional<InspectGeneral> ParseInspectGeneral(const uint8_t* data, uint32_t size)
    {
        if (data == nullptr || size < kInspectGeneralLeast || data[kInspectMode] != kGeneralMode) return std::nullopt;
        return InspectGeneral{ReadAt<uint32_t>(data, kInspectId), ReadAt<uint16_t>(data, kInspectIndex),
            LinkshellName(data + kInspectLinkshell, kLinkshellBytes), data[kInspectJobs], data[kInspectJobs + 1],
            data[kInspectLevels], data[kInspectLevels + 1], data[kInspectMasterLevel]};
    }

    std::optional<EntityUpdate> ParseEntityUpdate(const uint8_t* data, uint32_t size)
    {
        if (data == nullptr || size < kEntityFlags + sizeof(uint32_t)) return std::nullopt;
        const uint8_t sends = data[kEntitySends];
        EntityUpdate update{ReadAt<uint16_t>(data, kEntityIndex), (sends & kSendsDespawn) != 0, std::nullopt};
        if ((sends & kSendsFlags) != 0) update.untargetable = (ReadAt<uint32_t>(data, kEntityFlags) & kUntargetableFlag) != 0;
        return update;
    }

    std::optional<InspectMessage> ParseInspectMessage(const uint8_t* data, uint32_t size)
    {
        if (data == nullptr || size < kInspectMessageBytes) return std::nullopt;
        const auto* name = reinterpret_cast<const char*>(data + kInspectName);
        return InspectMessage{std::string(name, strnlen(name, kNameBytes)), ReadAt<uint32_t>(data, kInspectTitle)};
    }

    std::string WornLinkshellName(uint32_t itemFlags, const uint8_t* extra, size_t extraBytes)
    {
        if (extra == nullptr || itemFlags != kWornLinkshellFlags || extraBytes < kItemLinkshell + kItemLinkshellBytes) return {};
        return LinkshellName(extra + kItemLinkshell, kItemLinkshellBytes);
    }
}
