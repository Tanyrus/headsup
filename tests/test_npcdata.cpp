#include "npcdata.h"
#include "test.h"

#include <string>
#include <utility>
#include <vector>

using namespace headsup;

// Expected values are read from Phoenix's scripts: zones/Carpenters_Landing (zone 2), zones/Bibiki_Bay/npcs/qm_shen.lua
// and enum/key_item.codegen.lua.

namespace
{
    template <typename T>
    std::vector<T> Listed(std::span<const T> values)
    {
        return std::vector<T>(values.begin(), values.end());
    }
}

TEST(a_qm_names_the_mushrooms_it_takes_and_the_nm_they_pop)
{
    const NpcRecord* qm = NpcById(16785729); // qm_mycophile: Sleepshroom, Woozyshroom and Danceshroom pop Mycophile
    CHECK(qm != nullptr);
    if (qm == nullptr) return;
    CHECK(Listed(TradesOf(*qm)) == (std::vector<uint16_t>{4373, 4374, 4375}));
    CHECK(Listed(PopsOf(*qm)) == (std::vector<uint32_t>{16785722}));
    CHECK(GivesOf(*qm).empty() && KeyItemsOf(*qm).empty() && NeedsOf(*qm).empty());
    CHECK_EQ(qm->unread, 0);
}

TEST(the_key_items_a_qm_gives_and_checks_for_have_their_names)
{
    const NpcRecord* qm = NpcById(16785781); // qm_para: with the Antique Amulet it pops Para, then gives the medallion
    CHECK(qm != nullptr);
    if (qm == nullptr) return;
    CHECK(Listed(KeyItemsOf(*qm)) == (std::vector<uint16_t>{681}));
    CHECK(Listed(NeedsOf(*qm)) == (std::vector<uint16_t>{680}));
    CHECK(Listed(PopsOf(*qm)) == (std::vector<uint32_t>{16785716}));
    CHECK(std::string(KeyItemNameOf(681)) == "Cathedral Medallion");
    CHECK(std::string(KeyItemNameOf(680)) == "Antique Amulet");
    CHECK(std::string(KeyItemNameOf(8)) == "Airship Pass");
    CHECK(KeyItemNameOf(5) == nullptr); // no NPC gives or checks for it
}

TEST(what_a_script_works_out_while_it_runs_is_marked_unread)
{
    const NpcRecord* qm = NpcById(16793970); // Bibiki Bay's qm_shen looks Shen up by name
    CHECK(qm != nullptr);
    if (qm == nullptr) return;
    CHECK(Listed(TradesOf(*qm)) == (std::vector<uint16_t>{1823})); // Shrimp Lantern
    CHECK(PopsOf(*qm).empty());
    CHECK_EQ(qm->unread, kNpcUnreadPops);
}

TEST(an_npc_with_nothing_to_show_has_no_record)
{
    CHECK(NpcById(16781329) == nullptr); // a blank placeholder of zone 1
    CHECK(NpcById(0) == nullptr);
    CHECK(NpcById(0xFFFFFFFF) == nullptr);
}

TEST(a_shop_has_its_stock_and_says_whether_fame_moves_its_prices)
{
    // Maera, aboard the ship to Selbina, sells at fixed prices; Southern San d'Oria's Lusiane sells by xi.shop.nation,
    // which prices by fame and stocks some items only while San d'Oria places well in conquest.
    const NpcRecord* maera = NpcById(17678362);
    CHECK(maera != nullptr && ShopOf(*maera) != nullptr);
    if (maera == nullptr || ShopOf(*maera) == nullptr) return;
    std::vector<std::pair<uint16_t, uint32_t>> stock;
    for (const NpcStockItem& entry : StockOf(*ShopOf(*maera)))
        stock.emplace_back(entry.item, entry.price);
    CHECK(stock == (std::vector<std::pair<uint16_t, uint32_t>>{{4112, 910}, {4128, 4832}, {4148, 316}, {4150, 2595}, {4151, 800}}));
    CHECK(!ShopOf(*maera)->fame);
    const NpcRecord* lusiane = NpcById(17719350);
    CHECK(lusiane != nullptr && ShopOf(*lusiane) != nullptr);
    if (lusiane == nullptr || ShopOf(*lusiane) == nullptr) return;
    std::vector<int> ranks;
    for (const NpcStockItem& entry : StockOf(*ShopOf(*lusiane)))
        ranks.push_back(entry.rank);
    CHECK(ranks == (std::vector<int>{2, 3, 1, 2, 3, 3, 3}));
    CHECK(ShopOf(*lusiane)->fame);
    const NpcRecord* qm = NpcById(16785729);
    CHECK(qm != nullptr && ShopOf(*qm) == nullptr);
}

TEST(an_npcs_links_are_the_quests_and_missions_live_has_with_their_logs)
{
    // The Gate of the Gods (Grand Palace of Hu'Xzoi's _iya) takes part in Jeuno's Apocalypse Nigh and CoP's Garden of
    // Antiquity and A Fate Decided (quests/jeuno, missions/cop).
    const NpcRecord* gate = NpcById(16916828);
    CHECK(gate != nullptr);
    if (gate == nullptr) return;
    std::vector<std::string> links;
    for (const NpcLink* link : LinksOf(*gate))
        links.push_back(std::string(link->name) + " / " + (LogNameOf(*link) != nullptr ? LogNameOf(*link) : "-"));
    CHECK(links == (std::vector<std::string>{"Apocalypse Nigh / Jeuno", "Garden of Antiquity / CoP", "A Fate Decided / CoP"}));
    // The Shrouded Maw's Memento Circle: three battlefields, which have no log.
    const NpcRecord* circle = NpcById(16818249);
    CHECK(circle != nullptr && LinksOf(*circle).size() == 3);
    if (circle != nullptr && !LinksOf(*circle).empty())
        CHECK(LinksOf(*circle)[0]->kind == kLinkBattlefield && LogNameOf(*LinksOf(*circle)[0]) == nullptr);
    // Southern San d'Oria [S]'s Mystic Retriever is only in an RoV mission, which live has off.
    CHECK(NpcById(17105705) == nullptr);
}

TEST(what_an_npcs_systems_make_it_is_in_words)
{
    // Carpenters' Landing's Logging Point (xi.helm, LOGGING), Oldton Movalpolos' Twinkbrix (teleport.to MINESHAFT)
    // and Southern San d'Oria's Alivatand (crafting's guild points, LEATHERCRAFT).
    const auto purposes = [](uint32_t id) {
        std::vector<std::string> words;
        if (const NpcRecord* npc = NpcById(id))
            for (const char* purpose : PurposesOf(*npc))
                words.emplace_back(purpose);
        return words;
    };
    CHECK(purposes(16785774) == (std::vector<std::string>{"Logging point"}));
    CHECK(purposes(16822535) == (std::vector<std::string>{"Teleports to Mineshaft"}));
    CHECK(purposes(17719548) == (std::vector<std::string>{"Leathercraft guild points"}));
    CHECK(NpcById(16974393) == nullptr); // Aht Urhgan's Najaaj only walks a path
}
