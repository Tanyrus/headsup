#pragma once

#include <cstdint>
#include <span>
#include <vector>

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

    // What a link is (tools/gen_npcs.py's LINK_KINDS): a quest of the quest log, a mission, a battlefield, or a quest
    // the server keeps out of the log.
    enum NpcLinkKind : uint8_t
    {
        kLinkQuest,
        kLinkMission,
        kLinkBattlefield,
        kLinkHidden,
    };

    // A quest, mission or battlefield live has that an NPC takes part in.
    struct NpcLink
    {
        uint8_t kind; // NpcLinkKind
        uint8_t log;  // a quest's or mission's log
        const char* name; // its identifier in Phoenix's scripts in words, without the game's punctuation
    };

    // Where one of an NPC's lists sits in the pool of its kind.
    struct NpcSpan
    {
        uint16_t first;
        uint8_t count;
    };

    // One item of a shop's stock. rank is where xi.shop.nation stocks it from: 3 always, 2 while the shop's nation is
    // 1st or 2nd in conquest, 1 while it is 1st and only to its citizens; 0 for a shop that is not a nation's.
    struct NpcStockItem
    {
        uint16_t item;
        uint8_t rank;
        uint32_t price; // before fame
    };

    struct NpcShop
    {
        NpcSpan stock;
        bool fame; // its prices move with fame, from 10% off to 10% more
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
        NpcSpan links;    // the quests, missions and battlefields live has that it takes part in
        NpcSpan purposes; // what the shared systems it calls make it, in words
        uint16_t shop;    // its shop in the table ShopOf reads; 0 for none
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

    // Its links in the data's order: quests by log, missions by log, battlefields, then hidden quests.
    std::vector<const NpcLink*> LinksOf(const NpcRecord& npc);
    // The name of a quest's or mission's log ("Jeuno", "CoP"); nullptr for a battlefield or hidden quest.
    const char* LogNameOf(const NpcLink& link);

    // What the shared systems it calls make it ("Logging point", "Home point", "Teleports to Mineshaft"), each once.
    std::vector<const char*> PurposesOf(const NpcRecord& npc);

    // Its shop; nullptr when it sells nothing the data could read.
    const NpcShop* ShopOf(const NpcRecord& npc);
    // A shop's stock in its script's order.
    std::span<const NpcStockItem> StockOf(const NpcShop& shop);

    // A key item's name, its identifier in Phoenix's scripts in words; nullptr for one no NPC gives or checks for.
    const char* KeyItemNameOf(uint16_t keyItem);
}
