#include "mobdata.h"
#include "test.h"

using namespace aggroglow;

// Expected values are read from data/phoenix_mobs.tsv (zone 103 is Valkurm Dunes).

TEST(lookup_by_server_id)
{
    const MobRecord* m = FindMob(17199648, "Goblin Bounty Hunter");
    CHECK(m != nullptr);
    CHECK_EQ(m->zone, 103);
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

TEST(unknown_ids_are_not_found)
{
    CHECK(FindMob(1, "Snipper") == nullptr);
    CHECK(FindMob(0xFFFFFFFF, "Snipper") == nullptr);
}

TEST(all_data_is_compiled_in)
{
    CHECK(MobRecordCount() > 60000);
}
