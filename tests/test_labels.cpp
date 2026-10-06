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
    CHECK_EQ(l.argb, ConArgb(Con::EvenMatch));
}

TEST(single_level_shows_one_value)
{
    const MobRecord m = Mob(21, 21);
    const Label l     = MakeLabel(&m, nullptr, 20);
    CHECK(Text(l) == "Lv 21 T");
    CHECK_EQ(l.argb, 0xFFFFFF59u);
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
    CHECK_EQ(l.argb, 0xFF73B3FFu);
    CHECK(Text(MakeLabel(nullptr, &check, 20)) == "Lv 22 DC");
}

TEST(unknown_levels_show_question_marks)
{
    const MobRecord scripted = Mob(0, 0);
    CHECK(Text(MakeLabel(&scripted, nullptr, 20)) == "Lv ? ??");
    CHECK(Text(MakeLabel(nullptr, nullptr, 20)) == "Lv ? ??");
    CHECK_EQ(MakeLabel(nullptr, nullptr, 20).argb, ConArgb(Con::TooWeak));
}

TEST(unknown_player_level_shows_levels_without_a_con)
{
    const MobRecord m = Mob(17, 20);
    CHECK(Text(MakeLabel(&m, nullptr, 0)) == "Lv 17-20 ??");
}

TEST(con_colors)
{
    CHECK_EQ(ConArgb(Con::TooWeak), 0xFF999999u);
    CHECK_EQ(ConArgb(Con::IncrediblyEasyPrey), 0xFF66FF66u);
    CHECK_EQ(ConArgb(Con::EasyPrey), 0xFF66FF66u);
    CHECK_EQ(ConArgb(Con::DecentChallenge), 0xFF73B3FFu);
    CHECK_EQ(ConArgb(Con::EvenMatch), 0xFFFFFFFFu);
    CHECK_EQ(ConArgb(Con::Tough), 0xFFFFFF59u);
    CHECK_EQ(ConArgb(Con::VeryTough), 0xFFFF5959u);
    CHECK_EQ(ConArgb(Con::IncrediblyTough), 0xFFFF5959u);
    CHECK_EQ(ConArgb(static_cast<Con>(9)), 0xFF999999u);
}

TEST(longest_label_fits)
{
    const MobRecord m = Mob(100, 150);
    CHECK(Text(MakeLabel(&m, nullptr, 1)) == "Lv 100-150 IT");
    const MobRecord wide = Mob(1, 150);
    CHECK(Text(MakeLabel(&wide, nullptr, 75)) == "Lv 1-150 TW-IT");
}
