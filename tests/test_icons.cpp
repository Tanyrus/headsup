#include "icons.h"
#include "test.h"

#include <vector>

using namespace headsup;

namespace
{
    MobRecord Mob(uint8_t flags, uint16_t detects)
    {
        return MobRecord{17199648, "Goblin Bounty Hunter", 17, 20, flags, 300, detects, 0};
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

TEST(no_aggro_mobs_are_passive)
{
    // The aggro icon is the mob's nature: it never aggros at all.
    const MobRecord noAggro = Mob(kMobAggressive | kMobNoAggro, 0);
    CHECK(List(IconsFor(&noAggro)) == std::vector<Icon>{Icon::PassiveNQ});
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

TEST(every_icon_fills_a_square_so_all_show_the_same_size)
{
    // gen_icons.py cuts each image to its visible pixels: XIUI's player icons carry margins of 3 to 8 px of their 64.
    for (int i = 0; i < kIconCount; ++i)
    {
        const IconBitmap& icon = IconImage(static_cast<Icon>(i));
        CHECK_EQ(icon.width, icon.height);
        CHECK(icon.width > 0 && icon.width <= 64);
        bool left = false, right = false, top = false, bottom = false;
        for (uint32_t y = 0; y < icon.height; ++y)
        {
            for (uint32_t x = 0; x < icon.width; ++x)
            {
                if (icon.bgra[(y * icon.width + x) * 4 + 3] == 0) continue;
                left |= x == 0, right |= x == icon.width - 1, top |= y == 0, bottom |= y == icon.height - 1;
            }
        }
        CHECK((left && right) || (top && bottom));
    }
}
