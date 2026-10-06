#include "labels.h"
#include "test.h"

#include <string>

using namespace aggroglow;

namespace
{
    MobRecord Mob(uint8_t minLevel, uint8_t maxLevel)
    {
        return MobRecord{17199648, 103, "Goblin Bounty Hunter", minLevel, maxLevel, kMobAggressive, 300, 0};
    }

    std::string Text(const Label& l)
    {
        return l.text;
    }
}

// Player level 20 in the era table: 17 is Easy Prey, 18-19 Decent Challenge, 20 Even Match, 21 Tough.

TEST(range_shows_both_ends_colored_by_the_harder)
{
    const MobRecord m = Mob(17, 20);
    const Label l     = MakeLabel(&m, nullptr, 20);
    CHECK(Text(l) == "Lv 17-20 EP-EM");
    CHECK(l.shade == LabelShade::EvenMatch);
}

TEST(single_level_shows_one_value)
{
    const MobRecord m = Mob(21, 21);
    const Label l     = MakeLabel(&m, nullptr, 20);
    CHECK(Text(l) == "Lv 21 T");
    CHECK(l.shade == LabelShade::Tough);
}

TEST(range_with_one_con_shows_it_once)
{
    const MobRecord m = Mob(18, 19);
    CHECK(Text(MakeLabel(&m, nullptr, 20)) == "Lv 18-19 DC");
}

TEST(examined_spawn_shows_the_server_values)
{
    const MobRecord m       = Mob(17, 20);
    const CheckResult check = {22, Con::DecentChallenge};
    const Label l           = MakeLabel(&m, &check, 20);
    CHECK(Text(l) == "Lv 22 DC");
    CHECK(l.shade == LabelShade::DecentChallenge);
    CHECK(Text(MakeLabel(nullptr, &check, 20)) == "Lv 22 DC");
}

TEST(unknown_levels_show_question_marks)
{
    const MobRecord scripted = Mob(0, 0);
    CHECK(Text(MakeLabel(&scripted, nullptr, 20)) == "Lv ? ??");
    CHECK(Text(MakeLabel(nullptr, nullptr, 20)) == "Lv ? ??");
    CHECK(MakeLabel(nullptr, nullptr, 20).shade == LabelShade::Unknown);
    const MobRecord ranged = Mob(17, 20);
    CHECK(MakeLabel(&ranged, nullptr, 0).shade == LabelShade::Unknown); // player level not known yet
}

TEST(similar_cons_share_a_color)
{
    const LabelShade expected[kConCount] = {LabelShade::TooWeak, LabelShade::EasyPrey, LabelShade::EasyPrey,
        LabelShade::DecentChallenge, LabelShade::EvenMatch, LabelShade::Tough, LabelShade::VeryTough, LabelShade::VeryTough};
    for (int c = 0; c < kConCount; ++c)
        CHECK(ShadeFor(static_cast<Con>(c)) == expected[c]);
}

TEST(unknown_player_level_shows_levels_without_a_con)
{
    const MobRecord m = Mob(17, 20);
    CHECK(Text(MakeLabel(&m, nullptr, 0)) == "Lv 17-20 ??");
}

TEST(longest_label_fits)
{
    const MobRecord m = Mob(100, 150);
    CHECK(Text(MakeLabel(&m, nullptr, 1)) == "Lv 100-150 IT");
    const MobRecord wide = Mob(1, 150);
    CHECK(Text(MakeLabel(&wide, nullptr, 75)) == "Lv 1-150 TW-IT");
}
