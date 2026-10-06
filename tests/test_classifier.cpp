#include "classifier.h"
#include "test.h"

using namespace aggroglow;

namespace
{
    MobRecord Mob(bool aggro, uint8_t minLevel, uint8_t maxLevel)
    {
        return MobRecord{103, 0, "Test Mob", aggro, false, minLevel, maxLevel};
    }

    const PlayerState kLevel75{75, false, ConTable::Era};
}

TEST(missing_mob_is_unknown)
{
    CHECK(Classify(nullptr, kLevel75) == Category::Unknown);
}

TEST(passive_mob_wont_attack)
{
    const MobRecord m = Mob(false, 70, 75);
    CHECK(Classify(&m, kLevel75) == Category::WontAttack);
}

TEST(aggressive_even_match_will_attack)
{
    const MobRecord m = Mob(true, 75, 75);
    CHECK(Classify(&m, kLevel75) == Category::WillAttack);
}

TEST(aggressive_too_weak_wont_attack)
{
    const MobRecord m = Mob(true, 50, 55);
    CHECK(Classify(&m, kLevel75) == Category::WontAttack);
}

TEST(highest_level_decides)
{
    const MobRecord m = Mob(true, 50, 56);
    CHECK(Classify(&m, kLevel75) == Category::WillAttack);
}

TEST(too_weak_mobs_still_attack_a_sitting_player)
{
    const MobRecord m = Mob(true, 50, 55);
    CHECK(Classify(&m, PlayerState{75, true, ConTable::Era}) == Category::WillAttack);
}

TEST(unknown_player_level_assumes_attack)
{
    const MobRecord m = Mob(true, 1, 1);
    CHECK(Classify(&m, PlayerState{0, false, ConTable::Era}) == Category::WillAttack);
}

TEST(modern_table_counts_incredibly_easy_prey)
{
    const MobRecord m = Mob(true, 56, 56);
    CHECK(Classify(&m, PlayerState{78, false, ConTable::Modern}) == Category::WillAttack);
    CHECK(Classify(&m, PlayerState{78, false, ConTable::Era}) == Category::WontAttack);
}

TEST(sitting_statuses_match_phoenix)
{
    CHECK(IsSittingStatus(33));
    CHECK(IsSittingStatus(47));
    CHECK(IsSittingStatus(63));
    CHECK(IsSittingStatus(73));
    CHECK(!IsSittingStatus(0));
    CHECK(!IsSittingStatus(1));
    CHECK(!IsSittingStatus(62));
    CHECK(!IsSittingStatus(74));
}

TEST(unknown_mob_level_assumes_attack)
{
    // MobDB records "level unknown" as 0/0 (BCNM arenas, Salvage, Nyzul, some field mobs).
    const MobRecord m = Mob(true, 0, 0);
    CHECK(Classify(&m, kLevel75) == Category::WillAttack);
}
