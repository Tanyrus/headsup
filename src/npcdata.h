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

    // What an act is (tools/gen_npcs.py's ACT_KINDS): the player clicks or trades, or the zone shows or hides the ???.
    enum NpcActKind : uint8_t
    {
        kActClick,
        kActTrade,
        kActShows,
        kActHides,
    };

    // A condition of an act (gen_npcs.WHEN_KINDS): when it happens, or what it needs.
    enum NpcWhen : uint8_t
    {
        kWhenHour,      // a from the hour, b to the hour, both in
        kWhenWeather,   // a the weather (Phoenix's xi.weather)
        kWhenJob,       // a the main job
        kWhenDay,       // a the weekday, Firesday 0
        kWhenNation,    // a the player's nation
        kWhenKeyItem,   // a a key item held
        kWhenNoKeyItem, // a a key item not held
        kWhenKeyItems,  // the key items it checks for held
        kWhenQuest,     // a quest under way
        kWhenLink,      // a the quest, mission or battlefield (LinkAt)
        kWhenStep,      // a its step
        kWhenPrimed,    // the ??? itself readied (a tree honeyed, the drawer the server picked)
        kWhenNmKilled,  // the NM a step asks for just killed
        kWhenRoll,      // a lucky roll
        kWhenMob,       // name the mob whose death it follows
        kWhenZoneStart, // the zone starting up
    };

    struct NpcWhenEntry
    {
        uint8_t kind; // NpcWhen
        uint16_t a;
        uint16_t b;
        const char* name;
    };

    struct NpcActItem
    {
        uint16_t item;
        uint16_t qty;
    };

    // A message an act says: its words (NpcText) and the parameters that fill their <item>, <keyitem> and <number>.
    struct NpcSay
    {
        uint16_t text;
        NpcSpan params;
    };

    // A message's words as the zone's IDs.lua gives them, the game's own, with <item> and the like for parameters.
    struct NpcText
    {
        uint16_t zone;
        const char* words;
    };

    // One distinct thing the refresh's probe saw a ??? do on the server (data/phoenix_npc_acts.tsv).
    struct NpcAct
    {
        uint8_t kind;   // NpcActKind
        NpcSpan items;  // a trade's items
        NpcSpan when;   // its conditions, none when it happens whenever
        NpcSpan says;
        NpcSpan pops;   // mobs, by server ID
        NpcSpan gives;  // items
        NpcSpan keyItems;
        bool event; // it starts a cutscene
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
        NpcSpan acts;     // what the probe saw it do, in kNpcActs
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

    // What the refresh's probe saw it do on the server, in the order it found them.
    std::span<const NpcAct> ActsOf(const NpcRecord& npc);
    std::span<const NpcActItem> ItemsOf(const NpcAct& act);
    std::span<const NpcWhenEntry> WhenOf(const NpcAct& act);
    std::span<const NpcSay> SaysOf(const NpcAct& act);
    std::span<const uint32_t> PopsOf(const NpcAct& act);
    std::span<const uint16_t> GivesOf(const NpcAct& act);
    std::span<const uint16_t> KeyItemsOf(const NpcAct& act);
    std::span<const uint32_t> ParamsOf(const NpcSay& say);
    // A message's words; nullptr for a text past the table.
    const char* WordsOf(const NpcSay& say);
    // A live link by its place in the table a kWhenLink condition names; nullptr past it.
    const NpcLink* LinkAt(uint16_t index);

    // Its shop; nullptr when it sells nothing the data could read.
    const NpcShop* ShopOf(const NpcRecord& npc);
    // A shop's stock in its script's order.
    std::span<const NpcStockItem> StockOf(const NpcShop& shop);

    // A key item's name, its identifier in Phoenix's scripts in words; nullptr for one no NPC gives or checks for.
    const char* KeyItemNameOf(uint16_t keyItem);
}
