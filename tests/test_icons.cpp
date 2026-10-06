#include "icons.h"
#include "test.h"

#include <cstring>
#include <vector>

using namespace aggroglow;

namespace
{
    MobRecord Mob(uint8_t flags, uint16_t detects)
    {
        return MobRecord{17199648, 103, "Goblin Bounty Hunter", 17, 20, flags, 300, detects};
    }

    std::vector<Icon> List(const IconSet& set)
    {
        return std::vector<Icon>(set.icons, set.icons + set.count);
    }
}

TEST(no_data_no_icons)
{
    CHECK_EQ(IconsFor(nullptr).count, 0);
}

TEST(aggressive_and_passive_mobs)
{
    const MobRecord aggressive = Mob(kMobAggressive, 0);
    CHECK(List(IconsFor(&aggressive)) == std::vector<Icon>{Icon::AggroNQ});
    const MobRecord always = Mob(kMobAlwaysAggro, 0);
    CHECK(List(IconsFor(&always)) == std::vector<Icon>{Icon::AggroNQ});
    const MobRecord passive = Mob(0, 0);
    CHECK(List(IconsFor(&passive)) == std::vector<Icon>{Icon::PassiveNQ});
}

TEST(no_aggro_or_neutral_mobs_are_passive)
{
    // The aggro icon is the mob's nature: it never aggros at all.
    const MobRecord noAggro = Mob(kMobAggressive | kMobNoAggro, 0);
    CHECK(List(IconsFor(&noAggro)) == std::vector<Icon>{Icon::PassiveNQ});
    const MobRecord neutral = Mob(kMobAggressive | kMobNeutral, 0);
    CHECK(List(IconsFor(&neutral)) == std::vector<Icon>{Icon::PassiveNQ});
}

TEST(notorious_monsters_get_hq_icons)
{
    const MobRecord aggressive = Mob(kMobAggressive | kMobNotorious, 0);
    CHECK(List(IconsFor(&aggressive)) == std::vector<Icon>{Icon::AggroHQ});
    const MobRecord passive = Mob(kMobNotorious, 0);
    CHECK(List(IconsFor(&passive)) == std::vector<Icon>{Icon::PassiveHQ});
}

TEST(every_icon_in_xiui_order)
{
    const MobRecord m = Mob(kMobAggressive | kMobLink,
        kDetectSight | kDetectHearing | kDetectScent | kDetectMagic | kDetectAbility | kDetectLowHp);
    CHECK(List(IconsFor(&m)) == (std::vector<Icon>{Icon::AggroNQ, Icon::Link, Icon::Sight, Icon::Sound, Icon::Scent,
                                    Icon::Magic, Icon::Ability, Icon::Blood}));
}

TEST(true_detection_replaces_sight)
{
    const MobRecord sight = Mob(kMobTrueDetection, kDetectSight);
    CHECK(List(IconsFor(&sight)) == (std::vector<Icon>{Icon::PassiveNQ, Icon::TrueSight}));
    const MobRecord sound = Mob(kMobTrueDetection, kDetectHearing); // true sound: the TrueSight icon and Sound
    CHECK(List(IconsFor(&sound)) == (std::vector<Icon>{Icon::PassiveNQ, Icon::TrueSight, Icon::Sound}));
}

TEST(every_icon_has_an_embedded_png)
{
    for (int i = 0; i < kIconCount; ++i)
    {
        const IconPng& png = IconImage(static_cast<Icon>(i));
        CHECK(png.size > 8);
        CHECK(std::memcmp(png.data, "\x89PNG\r\n\x1a\n", 8) == 0);
    }
}
