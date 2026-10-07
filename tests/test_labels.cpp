#include "labels.h"
#include "test.h"

#include <string>

using namespace headsup;

namespace
{
    MobRecord Mob(uint8_t minLevel, uint8_t maxLevel, int16_t levelMod = 0)
    {
        return MobRecord{17199648, "Goblin Bounty Hunter", minLevel, maxLevel, kMobAggressive, 300, 0, levelMod};
    }

    std::string Text(const Label& l)
    {
        return l.text;
    }
}

// Player level 20 in Phoenix's era table: 17 is Easy Prey, 18-19 Decent Challenge, 20 Even Match, 21 Tough.

TEST(the_level_mod_moves_the_con_but_not_the_level_shown)
{
    const MobRecord m = Mob(19, 21, -2); // cons as 17-19
    const Label l     = MakeLabel(&m, nullptr, 20);
    CHECK(Text(l) == "Lv 19-21 EP-DC");
    CHECK(l.shade == LabelShade::DecentChallenge);
}

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

TEST(a_checked_spawn_shows_the_server_values)
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
}

TEST(the_rarer_cons_share_a_neighbors_color)
{
    // The menu has a color per shade: these two cons take their neighbor's, every other con has its own.
    CHECK(ShadeFor(Con::IncrediblyEasyPrey) == LabelShade::EasyPrey);
    CHECK(ShadeFor(Con::IncrediblyTough) == LabelShade::VeryTough);
    CHECK(ShadeFor(Con::DecentChallenge) == LabelShade::DecentChallenge);
    CHECK(ShadeFor(static_cast<Con>(kConCount)) == LabelShade::Unknown);
}

TEST(unknown_player_level_shows_levels_without_a_con)
{
    const MobRecord m = Mob(17, 20);
    const Label l     = MakeLabel(&m, nullptr, 0);
    CHECK_EQ(Text(l), "Lv 17-20 ??");
    CHECK(l.shade == LabelShade::Unknown);
}

TEST(the_longest_label_fits)
{
    // Three-digit levels at both ends and two two-letter cons: at 99, a level 102 is Very Tough and 150 Incredibly
    // Tough (era table rows +3 and +15).
    const MobRecord m = Mob(102, 150);
    CHECK_EQ(Text(MakeLabel(&m, nullptr, 99)), "Lv 102-150 VT-IT");
}
