#include "classifier.h"
#include "test.h"

using namespace headsup;

namespace
{
    MobRecord Mob(uint8_t flags, uint8_t minLevel, uint8_t maxLevel, int16_t levelMod = 0)
    {
        return MobRecord{17199648, "Test Mob", minLevel, maxLevel, flags, 300, 0, levelMod};
    }

    const PlayerState kLevel75{75, false};
}

TEST(missing_mob_is_unknown)
{
    CHECK(Classify(nullptr, 0, kLevel75) == Category::Unknown);
}

TEST(passive_mob_wont_attack)
{
    const MobRecord m = Mob(0, 70, 75);
    CHECK(Classify(&m, 0, kLevel75) == Category::WontAttack);
}

TEST(aggressive_even_match_will_attack)
{
    const MobRecord m = Mob(kMobAggressive, 75, 75);
    CHECK(Classify(&m, 0, kLevel75) == Category::WillAttack);
}

TEST(aggressive_too_weak_wont_attack)
{
    const MobRecord m = Mob(kMobAggressive, 50, 55);
    CHECK(Classify(&m, 0, kLevel75) == Category::WontAttack);
}

TEST(highest_level_decides)
{
    const MobRecord m = Mob(kMobAggressive, 50, 56);
    CHECK(Classify(&m, 0, kLevel75) == Category::WillAttack);
}

TEST(too_weak_mobs_still_attack_a_sitting_player)
{
    const MobRecord m = Mob(kMobAggressive, 50, 55);
    CHECK(Classify(&m, 0, PlayerState{75, true}) == Category::WillAttack);
}

TEST(unknown_player_level_assumes_attack)
{
    const MobRecord m = Mob(kMobAggressive, 1, 1);
    CHECK(Classify(&m, 0, PlayerState{0, false}) == Category::WillAttack);
}

TEST(unknown_mob_level_assumes_attack)
{
    // 0/0: a script sets the level when the mob spawns.
    const MobRecord m = Mob(kMobAggressive, 0, 0);
    CHECK(Classify(&m, 0, kLevel75) == Category::WillAttack);
}

TEST(a_check_gives_a_script_level_mob_its_level)
{
    const MobRecord m = Mob(kMobAggressive, 0, 0);
    CHECK(Classify(&m, 55, kLevel75) == Category::WontAttack);
}

TEST(always_aggro_ignores_too_weak)
{
    const MobRecord m = Mob(kMobAlwaysAggro, 50, 55);
    CHECK(Classify(&m, 0, kLevel75) == Category::WillAttack);
}

TEST(no_aggro_overrides_aggressive)
{
    const MobRecord noAggro = Mob(kMobAggressive | kMobAlwaysAggro | kMobNoAggro, 75, 75);
    CHECK(Classify(&noAggro, 0, kLevel75) == Category::WontAttack);
}

TEST(followers_never_attack)
{
    // Phoenix's tapMobAggro lets a mob with the Follow roam flag follow instead, even an always-aggro one.
    const MobRecord m = Mob(kMobAggressive | kMobAlwaysAggro | kMobFollows, 75, 75);
    CHECK(Classify(&m, 0, kLevel75) == Category::WontAttack);
}

TEST(the_level_mod_counts_for_too_weak)
{
    // At 75, level 56 gives 15 exp (Easy Prey) and 55 none.
    const MobRecord lowered = Mob(kMobAggressive, 50, 56, -1);
    CHECK(Classify(&lowered, 0, kLevel75) == Category::WontAttack);
    const MobRecord raised = Mob(kMobAggressive, 50, 55, 1);
    CHECK(Classify(&raised, 0, kLevel75) == Category::WillAttack);
}

TEST(a_checked_level_already_includes_the_mod)
{
    const MobRecord m = Mob(kMobAggressive, 50, 56, -1);
    CHECK(Classify(&m, 56, kLevel75) == Category::WillAttack);
}

TEST(examined_level_replaces_the_range)
{
    const MobRecord m = Mob(kMobAggressive, 50, 56);
    CHECK(Classify(&m, 55, kLevel75) == Category::WontAttack);
    CHECK(Classify(&m, 56, kLevel75) == Category::WillAttack);
}

TEST(notorious_mobs_get_their_own_categories)
{
    const MobRecord attacks = Mob(kMobAggressive | kMobNotorious, 75, 75);
    CHECK(Classify(&attacks, 0, kLevel75) == Category::NmWillAttack);
    const MobRecord passive = Mob(kMobNotorious, 75, 75);
    CHECK(Classify(&passive, 0, kLevel75) == Category::NmWontAttack);
    const MobRecord tooWeak = Mob(kMobAggressive | kMobNotorious, 50, 55);
    CHECK(Classify(&tooWeak, 0, kLevel75) == Category::NmWontAttack);
}
