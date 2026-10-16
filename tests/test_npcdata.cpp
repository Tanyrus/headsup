#include "npcdata.h"
#include "test.h"

#include <string>
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
