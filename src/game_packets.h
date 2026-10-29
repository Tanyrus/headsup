#pragma once

#include "guild_points.h"

#include "weather.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace headsup
{
    constexpr uint16_t kZoneInPacket         = 0x00A; // server to client: entering a zone
    constexpr uint16_t kWeatherPacket        = 0x057; // server to client: the zone's weather changed
    constexpr uint16_t kSkillsPacket         = 0x062; // server to client: your skills, the crafts' ranks among them
    constexpr uint16_t kConquestPacket       = 0x05E; // server to client: the conquest standings
    // Client to server: GP_CLI_COMMAND_REQCONQUEST, which the game sends when Region Info opens. Phoenix answers with
    // a kConquestPacket and a 0x071 (0x05a_reqconquest.cpp).
    constexpr uint16_t kConquestRequestPacket = 0x05A;
    // Its bytes as Ashita's AddOutgoingPacket takes them, header and all: the id in the first word's low 9 bits, the
    // size in 4-byte words (1: the header alone) in its high 7, then the sync word, which the game fills in.
    constexpr std::array<uint8_t, 4> kConquestRequest = {0x5A, 0x02, 0x00, 0x00};
    constexpr uint16_t kInspectGeneralPacket = 0x0C9; // server to client: a check's equipment or general part
    constexpr uint16_t kInspectMessagePacket = 0x0CA; // server to client: a check's name, title and bazaar message

    // Offsets count Ashita's 4-byte header. Phoenix's s2c 0x00A: WeatherNumber at 0x68.
    std::optional<Weather> ParseZoneInWeather(const uint8_t* data, uint32_t size);
    // Phoenix's s2c 0x00A: your server id (UniqueNo) at 0x04.
    std::optional<uint32_t> ParseZoneInPlayer(const uint8_t* data, uint32_t size);
    // Phoenix's s2c 0x057: WeatherNumber at 0x08.
    std::optional<Weather> ParseWeather(const uint8_t* data, uint32_t size);
    // Your rank in each craft, by guild id (0 Amateur on), from the skills packet.
    using CraftRanks = std::array<uint8_t, kCraftGuilds>;
    std::optional<CraftRanks> ParseCraftRanks(const uint8_t* data, uint32_t size);

    enum class Nation : uint8_t
    {
        None, // no one, or neutral
        SanDoria,
        Bastok,
        Windurst,
        Beastmen,
    };
    constexpr int kConquestRegionCount = 19; // Ronfaure (0) to the Tavnazian Archipelago (18), REGION_TYPE's numbers
    using RegionOwners                 = std::array<Nation, kConquestRegionCount>;
    // Phoenix's s2c 0x05E: region i's owner byte at 0x1D + 4i, 0 none, 1 San d'Oria, 2 Bastok, 3 Windurst, 4 beastmen.
    std::optional<RegionOwners> ParseConquest(const uint8_t* data, uint32_t size);
    // Phoenix's s2c 0x05E past the owners (0x05e_conquest.h): each contender's share of the influence in the region
    // you are in, in whole percents rounded down: San d'Oria at 0x86, Bastok at 0x87, Windurst at 0x88, the beastmen
    // at 0x94. The packet does not name the region. At the tally a region goes to whoever is ahead of all three
    // others (updateWeekConquest).
    struct RegionShare
    {
        Nation nation   = Nation::None;
        uint8_t percent = 0;
        bool operator==(const RegionShare&) const = default;
    };
    constexpr int kContenderCount = 4; // the three nations and the beastmen
    // The largest share first, equal shares in Nation's order.
    using RegionShares = std::array<RegionShare, kContenderCount>;
    // Nothing for a short packet, or for shares of more than 100 in all, which a layout other than Phoenix's would
    // give.
    std::optional<RegionShares> ParseRegionShares(const uint8_t* data, uint32_t size);
    constexpr int kContenderNations = 4; // San d'Oria to the beastmen, as kAllNations has them
    constexpr uint8_t NationBit(Nation nation) { return static_cast<uint8_t>(1u << (static_cast<int>(nation) - 1)); }
    const char* NationName(Nation nation); // "San d'Oria", ...; "" for none
    uint32_t NationColor(Nation nation);

    // Phoenix's s2c 0x0C9 general (0x0c9_equip_inspect_general.h): UniqNo at 0x04, ActIndex at 0x08, OptionFlag 0x01
    // at 0x0A, the linkshell's name at 0x10, jobs at 0x22, levels at 0x24, the master level at 0x27, flags at 0x28.
    // The jobs and levels are 0 for an anonymous player.
    struct InspectGeneral
    {
        uint32_t serverId = 0;
        uint16_t index    = 0;
        std::string linkshell;
        uint8_t mainJob = 0, subJob = 0, mainLevel = 0, subLevel = 0, masterLevel = 0;
        bool operator==(const InspectGeneral&) const = default;
    };
    std::optional<InspectGeneral> ParseInspectGeneral(const uint8_t* data, uint32_t size);

    // Phoenix's s2c 0x0CA (0x0ca_inspect_message.h): the name at 0x80, 16 bytes; the title number at 0x90.
    struct InspectMessage
    {
        std::string name;
        uint32_t title = 0;
        bool operator==(const InspectMessage&) const = default;
    };
    std::optional<InspectMessage> ParseInspectMessage(const uint8_t* data, uint32_t size);

    constexpr uint16_t kEntityUpdatePacket = 0x00E; // server to client: an NPC's or a mob's update
    struct EntityUpdate
    {
        uint16_t index; // entity target index
        bool despawn;
        std::optional<bool> untargetable; // when the update carries the entity's flags
        std::optional<bool> calledForHelp;
    };
    // From Phoenix's src/map/packets/entity_update.cpp: a mob or an NPC set untargetable, as a Chigoe is until it
    // engages, has the flags' Untargetable bit (the packet's TargetOffFlag) in every update that carries them, and a
    // mob whose claimer called for help the CallForHelp bit.
    std::optional<EntityUpdate> ParseEntityUpdate(const uint8_t* data, uint32_t size);

    // A linkshell's name as Phoenix packs it (EncodeStringLinkshell): 6-bit codes from the first byte's top bits,
    // a-z 1 to 26, A-Z 27 to 52, 0-9 53 to 62, ended by 63 or by four 1 bits and zeros, 20 at most.
    std::string LinkshellName(const uint8_t* packed, size_t bytes);
    // The name of a linkshell you wear, from its item as the game keeps it (Ashita's item_t), in the bytes a check of
    // you carries. Empty for any other item.
    std::string WornLinkshellName(uint32_t itemFlags, const uint8_t* extra, size_t extraBytes);
    // A container's slots as the client keeps them (Ashita's items_t): items sit from slot 1 to the capacity the game
    // reports, and the last slot to look in is never past the array's end.
    constexpr uint32_t kContainerSlots = 81;
    constexpr uint32_t LastItemSlot(uint32_t capacity)
    {
        return capacity < kContainerSlots ? capacity : kContainerSlots - 1;
    }
}
