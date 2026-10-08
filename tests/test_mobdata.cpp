#include "mobdata.h"
#include "test.h"

#include <string>

using namespace headsup;

// Expected values are read from data/phoenix_mobs.tsv (zone 103 is Valkurm Dunes).

TEST(lookup_by_server_id)
{
    const MobRecord* m = FindMob(17199648, "Goblin Bounty Hunter");
    CHECK(m != nullptr);
    CHECK_EQ(m->minLevel, 17);
    CHECK_EQ(m->maxLevel, 20);
    CHECK_EQ(m->flags, kMobAggressive | kMobLink);
    CHECK_EQ(m->respawn, 300u);
    CHECK_EQ(m->detects, kDetectSight);
}

TEST(passive_and_notorious_flags)
{
    const MobRecord* snipper = FindMob(17199322, "Snipper");
    CHECK(snipper != nullptr);
    CHECK_EQ(snipper->flags, 0);
    CHECK_EQ(snipper->detects, kDetectHearing); // crabs detect by sound
    const MobRecord* monk = FindMob(17199603, "Beach Monk");
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
        CHECK(IsAggressive(MobRecord{1, "Mob", 1, 1, row.flags, 0, 0, 0, 0}) == row.aggressive);
}

TEST(a_different_name_at_the_id_is_not_a_match)
{
    CHECK(FindMob(17199648, "Snipper") == nullptr);
    CHECK(FindMob(17199648, "") == nullptr);
}

TEST(names_match_without_punctuation_or_case)
{
    CHECK(FindMob(17199577, "Goblin's Dragonfly") != nullptr); // Phoenix: "Goblins Dragonfly"
    CHECK(SameMobName("Do'Bho Venomtail", "DoBho Venomtail"));
    CHECK(SameMobName("goblin bounty hunter", "Goblin Bounty Hunter"));
    CHECK(!SameMobName("Goblin Bounty Hunter", "Goblin Bounty Hunters"));
    CHECK(!SameMobName("Bat", ""));
}

TEST(a_record_named_after_phoenixs_template_key_is_the_clients_mob)
{
    // Outer Horutoto Ruins' bats: Phoenix's Stink_Bats_OHR template has no display name, so the data holds "Stink Bats
    // OHR"; the client, and a Windows tester's /hu debug, show "Stink Bats".
    const MobRecord* bats = FindMob(17571844, "Stink Bats");
    CHECK(bats != nullptr);
    CHECK_EQ(bats->minLevel, 15);
    CHECK_EQ(bats->maxLevel, 18);
}

TEST(only_whole_tag_words_after_the_clients_name_match)
{
    // Every tag seen against the client's own names: zones and areas, jobs, eras, weapons.
    CHECK(TaggedMobName("Lost Soul war ENS", "Lost Soul"));
    CHECK(TaggedMobName("Death Jacket CN RFS", "Death Jacket"));
    CHECK(TaggedMobName("Tapanas Minion past", "Tapana's Minion"));
    CHECK(!TaggedMobName("Locus Tomb Worm past", "Locus Dire Bat")); // another mob at that ID, in King Ranperre's Tomb
    CHECK(!TaggedMobName("Chaser Bat past", "Stink Bats"));        // as many letters, then a tag: still another mob
    CHECK(!TaggedMobName("Bats past", "Bat"));                     // a longer word, not a tag
    CHECK(!TaggedMobName("Iron CraniumV1", "Iron Cranium"));       // glued on: not a whole word
    CHECK(!TaggedMobName("Stink Bats", "Stink Bats OHR"));         // the client's name is never the longer
    CHECK(!TaggedMobName("Stink Bats", "Stink Bats"));             // the same name is SameMobName's
    CHECK(!TaggedMobName("Lost Soul war ENS", ""));
}

TEST(a_starter_mobs_level_mod)
{
    const MobRecord* rabbit = FindMob(17190918, "Wild Rabbit"); // East Ronfaure; its spawn script sets -2
    CHECK(rabbit != nullptr);
    CHECK_EQ(rabbit->expLevelMod, -2);
    CHECK_EQ(FindMob(17199648, "Goblin Bounty Hunter")->expLevelMod, 0);
}

TEST(a_placeholder_knows_its_nm)
{
    // East Ronfaure's Carrion Worms either side of Bigmouth Billy (its script's phList).
    CHECK_EQ(FindMob(17191194, "Carrion Worm")->placeholderOf, 17191196u);
    CHECK_EQ(FindMob(17191195, "Carrion Worm")->placeholderOf, 17191196u);
    CHECK_EQ(FindMob(17191196, "Bigmouth Billy")->placeholderOf, 0u);
}

TEST(a_record_by_id_alone_is_found_whatever_its_name)
{
    const MobRecord* billy = MobById(17191196);
    CHECK(billy != nullptr && std::string(billy->name) == "Bigmouth Billy");
    CHECK(MobById(1) == nullptr);
}

TEST(unknown_ids_are_not_found)
{
    CHECK(FindMob(1, "Snipper") == nullptr);
    CHECK(FindMob(0xFFFFFFFF, "Snipper") == nullptr);
}

