#include "mobdata.h"
#include "test.h"

#include <algorithm>
#include <string>

using namespace headsup;

// Expected values are read from data/phoenix_mobs.tsv (zone 103 is Valkurm Dunes).

TEST(lookup_by_server_id)
{
    const MobRecord* m = MobById(17199648);
    CHECK(m != nullptr);
    CHECK_EQ(m->minLevel, 17);
    CHECK_EQ(m->maxLevel, 20);
    CHECK_EQ(m->flags, kMobAggressive | kMobLink);
    CHECK_EQ(m->respawn, 300u);
    CHECK_EQ(m->detects, kDetectSight);
}

TEST(passive_and_notorious_flags)
{
    const MobRecord* snipper = MobById(17199322);
    CHECK(snipper != nullptr);
    CHECK_EQ(snipper->flags, 0);
    CHECK_EQ(snipper->detects, kDetectHearing); // crabs detect by sound
    const MobRecord* monk = MobById(17199603);
    CHECK(monk != nullptr);
    CHECK_EQ(monk->flags, kMobAggressive | kMobNotorious);
}

TEST(a_mob_aggros_unless_no_aggro_or_a_follower)
{
    struct Row
    {
        uint8_t flags;
        bool aggressive;
    };
    for (const Row& row : {Row{kMobAggressive, true}, Row{kMobAlwaysAggro, true}, Row{0, false},
             Row{kMobAggressive | kMobNoAggro, false}, Row{kMobAlwaysAggro | kMobNoAggro, false}, Row{kMobLink, false},
             Row{kMobAggressive | kMobFollows, false}})
        CHECK(IsAggressive(MobRecord{1, "Mob", 1, 1, row.flags, 0, 0, 0, 0, 0, 0}) == row.aggressive);
}

TEST(a_starter_mobs_level_mod)
{
    const MobRecord* rabbit = MobById(17190918); // East Ronfaure; its spawn script sets -2
    CHECK(rabbit != nullptr);
    CHECK_EQ(rabbit->expLevelMod, -2);
    CHECK_EQ(MobById(17199648)->expLevelMod, 0);
}

TEST(a_placeholder_knows_its_nm)
{
    // East Ronfaure's Carrion Worms either side of Bigmouth Billy (its script's phList).
    CHECK_EQ(MobById(17191194)->placeholderOf, 17191196u);
    CHECK_EQ(MobById(17191195)->placeholderOf, 17191196u);
    CHECK_EQ(MobById(17191196)->placeholderOf, 0u);
}

TEST(a_record_keeps_phoenixs_own_name_for_the_mob)
{
    const MobRecord* billy = MobById(17191196);
    CHECK(billy != nullptr && std::string(billy->name) == "Bigmouth Billy");
    // Outer Horutoto Ruins' bats: Phoenix's Stink_Bats_OHR template has no display name, so the data holds its key
    // where the client shows "Stink Bats".
    const MobRecord* bats = MobById(17571844);
    CHECK(bats != nullptr && std::string(bats->name) == "Stink Bats OHR");
    CHECK_EQ(bats->minLevel, 15);
    CHECK_EQ(bats->maxLevel, 18);
}

TEST(unknown_ids_are_not_found)
{
    CHECK(MobById(1) == nullptr);
    CHECK(MobById(0xFFFFFFFF) == nullptr);
}

TEST(a_mob_without_resistances_of_its_own_is_neutral)
{
    MobRecord mob{1, "Mob", 1, 1, 0, 0, 0, 0, 0, 0, 0};
    const MobResistances& neutral = ResistancesOf(mob);
    CHECK_EQ(neutral.immunity, 0u);
    CHECK(std::ranges::all_of(neutral.sdt, [](int16_t v) { return v == 0; }));
    CHECK(std::ranges::all_of(neutral.elementRanks, [](int8_t v) { return v == 0; }));
    CHECK(std::ranges::all_of(neutral.statusRanks, [](int8_t v) { return v == 0; }));
    mob.resistances = 0xFFFF; // past the table
    CHECK(&ResistancesOf(mob) == &neutral);
}

TEST(a_goblin_is_weak_to_light_as_phoenixs_data_says)
{
    const MobRecord* hunter = MobById(17199648);
    CHECK(hunter != nullptr);
    CHECK(ResistancesOf(*hunter).elementRanks[static_cast<int>(Element::Light)] <= -3);
}

// Drop lists as Phoenix's data/zones/<zone>/mobs.yaml and its modules give them (its sql/mob_droplist.sql keeps only
// the numbered lists scripts hand out), item IDs from its sql/item_basic.sql.
namespace
{
    constexpr uint16_t kMolybdenumOre = 647, kIronOre = 643, kMythrilOre = 644, kGoldOre = 737;
    constexpr uint16_t kBatWing = 922, kFiendBlood = 924, kDragonTalon = 903;
    constexpr uint16_t kCommon = 150, kVeryCommon = 240, kAlways = 1000; // per mille: data/enums/drop_rate.yaml
    constexpr uint16_t kWholeRoll = 10000, kTalonShare = 4500; // a one_of's shares are basis points
    constexpr uint32_t kBigmouthBilly = 17191196, kJormungand = 16797969, kValkurmDoman = 17199567;
    constexpr uint32_t kNorthGustabergFledermaus = 17211463;

    bool Has(std::span<const MobDrop> drops, uint16_t item, uint16_t rate, DropKind kind, uint8_t group = 0)
    {
        return std::ranges::any_of(drops, [&](const MobDrop& d) {
            return d.item == item && d.rate == rate && d.kind == kind && d.group == group;
        });
    }
}

TEST(bigmouth_billy_drops_ore_and_has_gold_ore_to_steal)
{
    const MobRecord* billy = MobById(kBigmouthBilly);
    CHECK(billy != nullptr);
    const std::span<const MobDrop> drops = DropsOf(*billy);
    CHECK(Has(drops, kIronOre, kVeryCommon, DropKind::Drop));
    CHECK(Has(drops, kMythrilOre, kVeryCommon, DropKind::Drop));
    CHECK(Has(drops, kGoldOre, 0, DropKind::Steal)); // Steal takes no rate from the YAML
    CHECK_EQ(std::ranges::count(drops, DropKind::Steal, &MobDrop::kind), 1);
    CHECK(std::ranges::none_of(drops, [](const MobDrop& d) { return d.group != 0; }));
}

TEST(a_mob_without_loot_has_an_empty_list)
{
    const MobRecord* doman = MobById(kValkurmDoman);
    CHECK(doman != nullptr && std::string(doman->name) == "Doman");
    CHECK(DropsOf(*doman).empty());
    CHECK_EQ(DropGroupOf(*doman, 1).rate, 0);

    MobRecord mob{1, "Mob", 1, 1, 0, 0, 0, 0, 0, 0, 0};
    CHECK(DropsOf(mob).empty());
    mob.drops = 0xFFFF; // past the table
    CHECK(DropsOf(mob).empty());
    CHECK(&DropGroupOf(mob, 1) == &DropGroupOf(*doman, 1));
}

TEST(jormungand_rolls_its_groups_and_picks_by_weight)
{
    const MobRecord* jormungand = MobById(kJormungand);
    CHECK(jormungand != nullptr);
    const std::span<const MobDrop> drops = DropsOf(*jormungand);
    CHECK_EQ(std::ranges::count(drops, kMolybdenumOre, &MobDrop::item), 4); // four rolls, each always
    CHECK(Has(drops, kMolybdenumOre, kAlways, DropKind::Drop));
    // Its first one_of: talon and blood 45% each, meat 10%, the group rolled at very common.
    CHECK(Has(drops, kDragonTalon, kTalonShare, DropKind::Drop, 1));
    CHECK_EQ(DropGroupOf(*jormungand, 1).rate, kVeryCommon);
    CHECK_EQ(DropGroupOf(*jormungand, 1).weight, kWholeRoll);
    // Its third names nine materials evenly, a weight of 1 each.
    constexpr uint16_t kEvenMaterials = 9;
    CHECK_EQ(DropGroupOf(*jormungand, 3).weight, kEvenMaterials);
    CHECK_EQ(DropGroupOf(*jormungand, 4).rate, 0);
    CHECK_EQ(DropGroupOf(*jormungand, 0).weight, 0);
    // Every group's weight is its members' rates, and the ungrouped entries come first.
    for (uint8_t group = 1; group <= 3; ++group)
    {
        int weight = 0;
        for (const MobDrop& d : drops)
            weight += d.group == group ? d.rate : 0;
        CHECK_EQ(weight, DropGroupOf(*jormungand, group).weight);
    }
    CHECK(std::ranges::is_sorted(drops, {}, &MobDrop::group));
}

TEST(a_yaml_module_takes_a_drop_out)
{
    // North Gustaberg's Fledermaus: the era module mob_droplist_removal drops its Vial of Fiend Blood.
    const MobRecord* bat = MobById(kNorthGustabergFledermaus);
    CHECK(bat != nullptr && std::string(bat->name) == "Fledermaus");
    CHECK(Has(DropsOf(*bat), kBatWing, kCommon, DropKind::Drop));
    CHECK_EQ(std::ranges::count(DropsOf(*bat), kFiendBlood, &MobDrop::item), 0);
}

TEST(an_nm_counts_its_placeholders_and_any_other_mob_has_none)
{
    CHECK_EQ(MobById(17191196)->placeholders, 2); // Bigmouth Billy: the two Carrion Worms above
    CHECK_EQ(MobById(17191194)->placeholders, 0);
    CHECK_EQ(MobById(17199648)->placeholders, 0);
}
