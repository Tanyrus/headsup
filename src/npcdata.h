#pragma once

#include <cstdint>
#include <span>

namespace headsup
{
    // What an NPC's scripts do in a way the data refresh could not read, a bit each (tools/gen_npcs.py's UNREAD).
    enum NpcUnread : uint8_t
    {
        kNpcUnreadTrade = 1, // a trade its handler works out while it runs
        kNpcUnreadGives = 2,
        kNpcUnreadPops  = 4,
        kNpcUnreadShop  = 8,
    };

    // Where one of an NPC's lists sits in the pool of its kind.
    struct NpcSpan
    {
        uint16_t first;
        uint8_t count;
    };

    // One NPC with something to show, as live Phoenix loads it (data/phoenix_npcs.tsv). A step of a quest, mission or
    // battlefield adds to its lists only when live has that content.
    struct NpcRecord
    {
        uint32_t id;      // server ID
        NpcSpan trades;   // the items it takes in trade
        NpcSpan gives;    // the items it hands out, quest rewards included
        NpcSpan keyItems; // the key items it gives
        NpcSpan needs;    // the key items it checks for
        NpcSpan pops;     // the mobs it spawns, by server ID
        uint8_t unread;   // NpcUnread bits
    };

    struct KeyItemName
    {
        uint16_t id;
        const char* name;
    };

    // The record for this server ID; nullptr for an NPC the data has nothing to show of.
    const NpcRecord* NpcById(uint32_t serverId);

    // Its lists, each increasing: items and key items by the game's IDs.
    std::span<const uint16_t> TradesOf(const NpcRecord& npc);
    std::span<const uint16_t> GivesOf(const NpcRecord& npc);
    std::span<const uint16_t> KeyItemsOf(const NpcRecord& npc);
    std::span<const uint16_t> NeedsOf(const NpcRecord& npc);
    std::span<const uint32_t> PopsOf(const NpcRecord& npc);

    // A key item's name, its identifier in Phoenix's scripts in words; nullptr for one no NPC gives or checks for.
    const char* KeyItemNameOf(uint16_t keyItem);
}
