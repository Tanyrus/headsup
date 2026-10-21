#include "game_font.h"
#include "icons.h"
#include "test.h"

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <string>
#include <vector>

using namespace headsup;

namespace
{
    MobRecord Mob(uint8_t flags, uint16_t detects)
    {
        return MobRecord{17199648, "Goblin Bounty Hunter", 17, 20, flags, 300, detects, 0, 0, 0, 0};
    }

    std::vector<Icon> List(const IconSet& set)
    {
        return std::vector<Icon>(set.icons, set.icons + set.count);
    }
}

TEST(no_data_no_icons)
{
    CHECK_EQ(IconsFor(nullptr, kAllDetection).count, 0);
}

TEST(aggressive_and_passive_mobs)
{
    const MobRecord aggressive = Mob(kMobAggressive, 0);
    CHECK(List(IconsFor(&aggressive, kAllDetection)) == std::vector<Icon>{Icon::AggroNQ});
    const MobRecord always = Mob(kMobAlwaysAggro, 0);
    CHECK(List(IconsFor(&always, kAllDetection)) == std::vector<Icon>{Icon::AggroNQ});
    const MobRecord passive = Mob(0, 0);
    CHECK(List(IconsFor(&passive, kAllDetection)) == std::vector<Icon>{Icon::PassiveNQ});
}

TEST(no_aggro_mobs_are_passive)
{
    // The aggro icon is the mob's nature: it never aggros at all.
    const MobRecord noAggro = Mob(kMobAggressive | kMobNoAggro, 0);
    CHECK(List(IconsFor(&noAggro, kAllDetection)) == std::vector<Icon>{Icon::PassiveNQ});
}

TEST(notorious_monsters_get_hq_icons)
{
    const MobRecord aggressive = Mob(kMobAggressive | kMobNotorious, 0);
    CHECK(List(IconsFor(&aggressive, kAllDetection)) == std::vector<Icon>{Icon::AggroHQ});
    const MobRecord passive = Mob(kMobNotorious, 0);
    CHECK(List(IconsFor(&passive, kAllDetection)) == std::vector<Icon>{Icon::PassiveHQ});
}

TEST(every_icon_in_xiui_order)
{
    const MobRecord m = Mob(kMobAggressive | kMobLink,
        kDetectSight | kDetectHearing | kDetectScent | kDetectMagic | kDetectAbility | kDetectLowHp);
    CHECK(List(IconsFor(&m, kAllDetection)) == (std::vector<Icon>{Icon::AggroNQ, Icon::Link, Icon::Sight, Icon::Sound, Icon::Scent,
                                    Icon::Magic, Icon::Ability, Icon::Blood}));
}

TEST(true_detection_replaces_sight)
{
    const MobRecord sight = Mob(kMobTrueDetection, kDetectSight);
    CHECK(List(IconsFor(&sight, kAllDetection)) == (std::vector<Icon>{Icon::PassiveNQ, Icon::TrueSight}));
    const MobRecord sound = Mob(kMobTrueDetection, kDetectHearing); // true sound: the TrueSight icon and Sound
    CHECK(List(IconsFor(&sound, kAllDetection)) == (std::vector<Icon>{Icon::PassiveNQ, Icon::TrueSight, Icon::Sound}));
}

TEST(every_icon_a_mobs_row_can_hold_has_a_popup_tag)
{
    const uint16_t detections[] = {
        kDetectSight, kDetectHearing, kDetectLowHp, kDetectMagic, kDetectAbility, kDetectScent};
    for (int flags = 0; flags < 256; ++flags)
        for (int subset = 0; subset < 1 << std::size(detections); ++subset)
        {
            uint16_t detects = 0;
            for (size_t bit = 0; bit < std::size(detections); ++bit)
                if (subset & (1 << bit)) detects |= detections[bit];
            const MobRecord mob = Mob(static_cast<uint8_t>(flags), detects);
            const IconSet icons = IconsFor(&mob, kAllDetection);
            for (int i = 0; i < icons.count; ++i)
                CHECK(std::string(IconTag(icons.icons[i])) != "");
        }
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

namespace
{
    MobResistances Neutral() { return MobResistances{}; }

    // Only the clearest weaknesses and resistances, so each test sees just what it sets up. Statuses keep their defaults:
    // no weaknesses, resistances from rank 4.
    constexpr BadgeRules kStrict{.elementWeakRank = -3, .elementResistRank = 4};

    BadgeRules Statuses(int weakRank, int resistRank)
    {
        BadgeRules rules = kStrict;
        for (StatusBadgeRule& status : rules.status)
            status = StatusBadgeRule{true, weakRank, resistRank};
        return rules;
    }

    std::vector<InfoIcon> Info(const MobResistances& r, const BadgeRules& rules = kStrict)
    {
        const InfoIconSet set = InfoIconsFor(&r, rules);
        return std::vector<InfoIcon>(set.icons, set.icons + set.count);
    }

    int16_t& SdtOf(MobResistances& r, Sdt type) { return r.sdt[static_cast<int>(type)]; }
    int8_t& ElementRankOf(MobResistances& r, Element e) { return r.elementRanks[static_cast<int>(e)]; }
    int8_t& RankOf(MobResistances& r, StatusRank s) { return r.statusRanks[static_cast<int>(s)]; }
}

TEST(no_data_or_neutral_data_shows_no_badges)
{
    CHECK_EQ(InfoIconsFor(nullptr, BadgeRules{}).count, 0);
    CHECK(Info(Neutral()).empty());
    BadgeRules all        = Statuses(-1, 1);
    all.elementWeakRank   = -1;
    all.elementResistRank = 1;
    CHECK(Info(Neutral(), all).empty()); // rank 0 is neutral
}

TEST(damage_weaknesses_come_first_largest_first_then_resistances_strongest_first)
{
    MobResistances r = Neutral();
    SdtOf(r, Sdt::Piercing) = 2500;
    SdtOf(r, Sdt::Impact)   = 5000;
    SdtOf(r, Sdt::Slashing) = -2500;
    SdtOf(r, Sdt::H2H)      = -10000;
    SdtOf(r, Sdt::Fire)     = 49; // rounds to 0%: nothing
    CHECK(Info(r) == (std::vector<InfoIcon>{{Icon::Impact, BadgeKind::Weak, 50}, {Icon::Piercing, BadgeKind::Weak, 25},
                         {Icon::H2H, BadgeKind::Resist, 100}, {Icon::Slashing, BadgeKind::Resist, 25}}));
}

TEST(elements_show_by_rank_when_weak_or_resisted)
{
    MobResistances r = Neutral();
    for (int8_t& rank : r.elementRanks)
        rank = -2; // the common rank: nothing
    ElementRankOf(r, Element::Light) = -3;
    ElementRankOf(r, Element::Fire)  = -3;
    ElementRankOf(r, Element::Dark)  = 6;
    ElementRankOf(r, Element::Ice)   = 11;
    ElementRankOf(r, Element::Water) = 3;
    SdtOf(r, Sdt::Slashing)          = -2500;
    CHECK(Info(r) == (std::vector<InfoIcon>{{Icon::Fire, BadgeKind::WeakRank, -3}, {Icon::Light, BadgeKind::WeakRank, -3},
                         {Icon::Slashing, BadgeKind::Resist, 25}, {Icon::Ice, BadgeKind::ResistRank, 11},
                         {Icon::Dark, BadgeKind::ResistRank, 6}}));
}

TEST(by_default_every_element_weakness_shows_weakest_first)
{
    MobResistances r = Neutral();
    const int8_t vulture[kElementCount] = {-2, -3, 0, 0, -2, -2, -2, -2}; // fire, ice, wind, earth, lightning, water, light, dark
    std::copy(std::begin(vulture), std::end(vulture), r.elementRanks);
    ElementRankOf(r, Element::Dark) = 4;
    CHECK(Info(r, BadgeRules{}) == (std::vector<InfoIcon>{{Icon::Ice, BadgeKind::WeakRank, -3}, {Icon::Fire, BadgeKind::WeakRank, -2},
                                      {Icon::Lightning, BadgeKind::WeakRank, -2}, {Icon::Water, BadgeKind::WeakRank, -2},
                                      {Icon::Light, BadgeKind::WeakRank, -2}, {Icon::Dark, BadgeKind::ResistRank, 4}}));
}

TEST(the_element_cutoffs_choose_how_weak_or_resistant_shows)
{
    MobResistances r                = Neutral();
    ElementRankOf(r, Element::Fire) = -1;
    ElementRankOf(r, Element::Ice)  = 1;
    ElementRankOf(r, Element::Wind) = 3;
    CHECK(Info(r, {.elementWeakRank = -1, .elementResistRank = 1}) ==
          (std::vector<InfoIcon>{{Icon::Fire, BadgeKind::WeakRank, -1}, {Icon::Wind, BadgeKind::ResistRank, 3},
              {Icon::Ice, BadgeKind::ResistRank, 1}}));
    CHECK(Info(r, {.elementWeakRank = -2, .elementResistRank = 3}) == (std::vector<InfoIcon>{{Icon::Wind, BadgeKind::ResistRank, 3}}));
}

TEST(an_elements_sdt_wins_over_its_rank)
{
    MobResistances r = Neutral();
    SdtOf(r, Sdt::Fire)             = 2500;
    ElementRankOf(r, Element::Fire) = 6;
    CHECK(Info(r) == (std::vector<InfoIcon>{{Icon::Fire, BadgeKind::Weak, 25}}));
}

TEST(status_ranks_from_4_up_highest_first_and_none_for_an_immunity)
{
    MobResistances r = Neutral();
    RankOf(r, StatusRank::Paralyze) = 4;
    RankOf(r, StatusRank::Gravity)  = 9;
    RankOf(r, StatusRank::Bind)     = 3; // below 4: nothing
    RankOf(r, StatusRank::Silence)  = 11;
    r.immunity                      = kImmuneSilence;
    CHECK(Info(r) == (std::vector<InfoIcon>{{Icon::Gravity, BadgeKind::StatusRank, 9}, {Icon::Paralyze, BadgeKind::StatusRank, 4},
                         {Icon::Silence, BadgeKind::Immune, 0}}));
}

TEST(light_and_dark_sleep_merge_into_one_sleep_when_they_agree)
{
    MobResistances r = Neutral();
    RankOf(r, StatusRank::LightSleep) = 6;
    RankOf(r, StatusRank::DarkSleep)  = 6;
    CHECK(Info(r) == (std::vector<InfoIcon>{{Icon::Sleep, BadgeKind::StatusRank, 6}}));
    RankOf(r, StatusRank::DarkSleep) = 5;
    CHECK(Info(r) == (std::vector<InfoIcon>{{Icon::LightSleep, BadgeKind::StatusRank, 6}, {Icon::DarkSleep, BadgeKind::StatusRank, 5}}));
    RankOf(r, StatusRank::DarkSleep) = 6;
    r.immunity                       = kImmuneLightSleep; // immune to one: the other keeps its own rank
    CHECK(Info(r) == (std::vector<InfoIcon>{{Icon::DarkSleep, BadgeKind::StatusRank, 6}, {Icon::LightSleep, BadgeKind::Immune, 0}}));
    r.immunity = kImmuneLightSleep | kImmuneDarkSleep;
    CHECK(Info(r) == (std::vector<InfoIcon>{{Icon::Sleep, BadgeKind::Immune, 0}}));
}

TEST(status_weaknesses_show_when_turned_on_weakest_first_before_the_resistances)
{
    MobResistances r                = Neutral();
    RankOf(r, StatusRank::Paralyze) = -3;
    RankOf(r, StatusRank::Bind)     = -3;
    RankOf(r, StatusRank::Poison)   = -2;
    RankOf(r, StatusRank::Blind)    = -1;
    RankOf(r, StatusRank::Gravity)  = 6;
    RankOf(r, StatusRank::Silence)  = -3;
    r.immunity                      = kImmuneSilence; // immune: no rank
    CHECK(Info(r) == (std::vector<InfoIcon>{{Icon::Gravity, BadgeKind::StatusRank, 6}, {Icon::Silence, BadgeKind::Immune, 0}}));
    CHECK(Info(r, Statuses(-2, 4)) ==
          (std::vector<InfoIcon>{{Icon::Paralyze, BadgeKind::StatusWeak, -3}, {Icon::Bind, BadgeKind::StatusWeak, -3},
              {Icon::Poison, BadgeKind::StatusWeak, -2}, {Icon::Gravity, BadgeKind::StatusRank, 6}, {Icon::Silence, BadgeKind::Immune, 0}}));
    CHECK(Info(r, Statuses(-3, 7)) ==
          (std::vector<InfoIcon>{{Icon::Paralyze, BadgeKind::StatusWeak, -3}, {Icon::Bind, BadgeKind::StatusWeak, -3},
              {Icon::Silence, BadgeKind::Immune, 0}}));
}

TEST(sleeps_that_agree_on_a_weakness_merge_too)
{
    MobResistances r                  = Neutral();
    RankOf(r, StatusRank::LightSleep) = -2;
    RankOf(r, StatusRank::DarkSleep)  = -2;
    CHECK(Info(r, Statuses(-1, 4)) == (std::vector<InfoIcon>{{Icon::Sleep, BadgeKind::StatusWeak, -2}}));
    BadgeRules rules = Statuses(-1, 4);
    rules.status[static_cast<int>(StatusRank::DarkSleep)].weakRank = -3; // dark sleep's cutoff leaves only light sleep
    CHECK(Info(r, rules) == (std::vector<InfoIcon>{{Icon::LightSleep, BadgeKind::StatusWeak, -2}}));
}

TEST(each_status_has_its_own_switch_and_cutoffs)
{
    MobResistances r                = Neutral();
    RankOf(r, StatusRank::Paralyze) = 5;
    RankOf(r, StatusRank::Bind)     = 5;
    RankOf(r, StatusRank::Silence)  = -2;
    RankOf(r, StatusRank::Stun)     = 5;
    BadgeRules rules                                            = kStrict;
    rules.status[static_cast<int>(StatusRank::Bind)].show       = false;
    rules.status[static_cast<int>(StatusRank::Silence)].weakRank = -2;
    rules.status[static_cast<int>(StatusRank::Stun)].resistRank = 6;
    CHECK(Info(r, rules) == (std::vector<InfoIcon>{{Icon::Silence, BadgeKind::StatusWeak, -2}, {Icon::Paralyze, BadgeKind::StatusRank, 5}}));
}

TEST(either_cutoff_can_be_off)
{
    MobResistances r                = Neutral();
    ElementRankOf(r, Element::Fire) = -3;
    ElementRankOf(r, Element::Ice)  = 11;
    CHECK(Info(r, {.elementWeakRank = kNoWeakRank}) == (std::vector<InfoIcon>{{Icon::Ice, BadgeKind::ResistRank, 11}}));
    CHECK(Info(r, {.elementResistRank = kNoResistRank}) == (std::vector<InfoIcon>{{Icon::Fire, BadgeKind::WeakRank, -3}}));
    MobResistances s                = Neutral();
    RankOf(s, StatusRank::Paralyze) = 11;
    RankOf(s, StatusRank::Bind)     = -3;
    CHECK(Info(s, Statuses(-1, kNoResistRank)) == (std::vector<InfoIcon>{{Icon::Bind, BadgeKind::StatusWeak, -3}}));
    CHECK(Info(s, Statuses(kNoWeakRank, 1)) == (std::vector<InfoIcon>{{Icon::Paralyze, BadgeKind::StatusRank, 11}}));
}

TEST(the_element_limit_keeps_the_ranks_that_move_magic_evasion_most)
{
    MobResistances r                     = Neutral();
    ElementRankOf(r, Element::Fire)      = -2; // magic evasion x0.96019
    ElementRankOf(r, Element::Ice)       = 4;  // x1.126
    ElementRankOf(r, Element::Wind)      = -3; // x0.95
    ElementRankOf(r, Element::Earth)     = -1; // x0.98
    ElementRankOf(r, Element::Lightning) = 11; // x2.35
    const auto limit = [](int most) { return BadgeRules{.elementWeakRank = -1, .elementResistRank = 1, .maxElements = most}; };
    CHECK(Info(r, limit(2)) == (std::vector<InfoIcon>{{Icon::Lightning, BadgeKind::ResistRank, 11}, {Icon::Ice, BadgeKind::ResistRank, 4}}));
    CHECK(Info(r, limit(4)) == (std::vector<InfoIcon>{{Icon::Wind, BadgeKind::WeakRank, -3}, {Icon::Fire, BadgeKind::WeakRank, -2},
                                   {Icon::Lightning, BadgeKind::ResistRank, 11}, {Icon::Ice, BadgeKind::ResistRank, 4}}));
    CHECK(Info(r, limit(0)).empty());
    CHECK_EQ(Info(r, limit(kElementCount)).size(), 5u);
}

TEST(immunities_in_mobdbs_order_and_the_ones_without_icons_left_out)
{
    MobResistances r = Neutral();
    r.immunity = kImmunePetrify | kImmuneGravity | kImmuneElegy | kImmuneStun | kImmuneTerror | kImmuneAddle | kImmuneCurse;
    CHECK(Info(r) == (std::vector<InfoIcon>{{Icon::Gravity, BadgeKind::Immune, 0}, {Icon::Stun, BadgeKind::Immune, 0},
                         {Icon::Elegy, BadgeKind::Immune, 0}, {Icon::Petrify, BadgeKind::Immune, 0}}));
}

TEST(a_mob_with_everything_fits_the_set)
{
    MobResistances r = Neutral();
    for (int16_t& sdt : r.sdt)
        sdt = 2500;
    r.immunity = 0xFFFFFFFFu;
    const std::vector<InfoIcon> all = Info(r);
    CHECK_EQ(all.size(), 12u + 12u); // 12 damage types; Sleep and the 11 other immunities with icons
    CHECK(all.size() <= static_cast<size_t>(kMaxInfoIcons));
}

TEST(rank_badges_read_as_the_chance_their_rank_moves)
{
    // A level 29 Clipper of Qufim Island: magic evasion 83 (rank C+, Phoenix's sql/skill_caps.sql), no evasion mods.
    MobStats stats{};
    stats.value[static_cast<int>(Stat::MagicEvasion)] = 83;
    const MobResistances res                          = Neutral();
    const RankNumbers mobs{RankDisplay::IncreasedChance, true, nullptr, MagicTarget{29, stats, res}};
    CHECK(BadgeText({Icon::Piercing, BadgeKind::Weak, 25}, mobs) == "+25%"); // damage taken stays a damage percent
    CHECK(BadgeText({Icon::Slashing, BadgeKind::Resist, 50}, mobs) == "-50%");
    CHECK(BadgeText({Icon::Light, BadgeKind::WeakRank, -3}, mobs) == "+5%");       // 83 - floor(83 * 0.95)
    CHECK(BadgeText({Icon::Dark, BadgeKind::ResistRank, 4}, mobs) == "-10%");      // 83 - floor(83 * 1.126)
    CHECK(BadgeText({Icon::Paralyze, BadgeKind::StatusWeak, -2}, mobs) == "+4%");  // 83 - floor(83 * 0.96019)
    CHECK(BadgeText({Icon::Sleep, BadgeKind::StatusRank, 11}, mobs) == "-90%");    // no further than 95% to 5%
    CHECK(BadgeText({Icon::Bind, BadgeKind::Immune, 0}, mobs) == "\xC3\x97");
}

TEST(rank_badges_can_read_as_ranks_and_do_without_the_mob_s_stats)
{
    const RankNumbers ranks{RankDisplay::Rank, true, nullptr, std::nullopt};
    CHECK(BadgeText({Icon::Piercing, BadgeKind::Weak, 25}, ranks) == "+25%");
    CHECK(BadgeText({Icon::Light, BadgeKind::WeakRank, -3}, ranks) == "-3");
    CHECK(BadgeText({Icon::Dark, BadgeKind::ResistRank, 6}, ranks) == "6");
    CHECK(BadgeText({Icon::Paralyze, BadgeKind::StatusWeak, -2}, ranks) == "-2");
    CHECK(BadgeText({Icon::Sleep, BadgeKind::StatusRank, 11}, ranks) == "11");
    const RankNumbers unknown{RankDisplay::TotalChance, true, nullptr, std::nullopt};
    CHECK(BadgeText({Icon::Light, BadgeKind::WeakRank, -3}, unknown) == "-3");
}

TEST(a_mob_s_chances_are_figured_at_its_checked_level_or_the_top_of_its_range)
{
    // Goblin Bounty Hunter, Valkurm Dunes, 17 to 20, no level mod; magic evasion 58 at 20 and 52 at 18 (rank C+,
    // Phoenix's sql/skill_caps.sql).
    const MobRecord& hunter = *MobById(17199648);
    const std::optional<MagicTarget> top = MagicTargetOf(hunter, 0);
    CHECK(top && top->level == 20 && top->stats.value[static_cast<int>(Stat::MagicEvasion)] == 58);
    const std::optional<MagicTarget> checked = MagicTargetOf(hunter, 18);
    CHECK(checked && checked->level == 18 && checked->stats.value[static_cast<int>(Stat::MagicEvasion)] == 52);
    CHECK(!MagicTargetOf(hunter, 30)); // a level its data has no stats for
}

TEST(detection_icons_can_be_filtered_by_kind)
{
    const MobRecord m = Mob(kMobAggressive | kMobLink, kDetectSight | kDetectHearing);
    CHECK(List(IconsFor(&m, kAllDetection)) == (std::vector<Icon>{Icon::AggroNQ, Icon::Link, Icon::Sight, Icon::Sound}));
    CHECK(List(IconsFor(&m, DetectionBit(DetectionIcon::Link) | DetectionBit(DetectionIcon::Sound))) ==
          (std::vector<Icon>{Icon::Link, Icon::Sound}));
    CHECK_EQ(IconsFor(&m, 0).count, 0);
    CHECK(DetectionOf(Icon::AggroHQ) == DetectionIcon::Aggro);
    CHECK(DetectionOf(Icon::PassiveNQ) == DetectionIcon::Aggro);
    CHECK(DetectionOf(Icon::TrueSight) == DetectionIcon::TrueSight);
}

TEST(the_badge_groups_split_the_info_icons_in_order)
{
    MobResistances r = Neutral();
    SdtOf(r, Sdt::Impact)          = 5000;  // +50%
    SdtOf(r, Sdt::Slashing)        = -2500; // -25%
    RankOf(r, StatusRank::Silence) = 6;
    r.immunity                     = kImmuneStun;
    const auto group = [&](const MobResistances& mob, const BadgeRules& rules, BadgeGroup g) {
        const InfoIconSet set = InfoIconsOf(&mob, rules, g);
        return std::vector<InfoIcon>(set.icons, set.icons + set.count);
    };
    CHECK(group(r, BadgeRules{}, BadgeGroup::Weak) == (std::vector<InfoIcon>{{Icon::Impact, BadgeKind::Weak, 50}}));
    CHECK(group(r, BadgeRules{}, BadgeGroup::Resist) == (std::vector<InfoIcon>{{Icon::Slashing, BadgeKind::Resist, 25}}));
    CHECK(group(r, BadgeRules{}, BadgeGroup::Status) == (std::vector<InfoIcon>{{Icon::Silence, BadgeKind::StatusRank, 6}}));
    CHECK(group(r, BadgeRules{}, BadgeGroup::Immune) == (std::vector<InfoIcon>{{Icon::Stun, BadgeKind::Immune, 0}}));
    CHECK_EQ(InfoIconsOf(nullptr, BadgeRules{}, BadgeGroup::Weak).count, 0);

    // The ranks and the second entries: each group holds an SDT or weakness first and its rank or resistance after.
    MobResistances s = Neutral();
    SdtOf(s, Sdt::Piercing)             = 2500;
    ElementRankOf(s, Element::Fire)     = -3;
    SdtOf(s, Sdt::H2H)                  = -1000;
    ElementRankOf(s, Element::Ice)      = 11;
    RankOf(s, StatusRank::Paralyze)     = -3;
    RankOf(s, StatusRank::Gravity)      = 6;
    s.immunity                          = kImmuneBind | kImmuneStun;
    const BadgeRules rules              = Statuses(-2, 4);
    CHECK(group(s, rules, BadgeGroup::Weak) ==
          (std::vector<InfoIcon>{{Icon::Piercing, BadgeKind::Weak, 25}, {Icon::Fire, BadgeKind::WeakRank, -3}}));
    CHECK(group(s, rules, BadgeGroup::Resist) ==
          (std::vector<InfoIcon>{{Icon::H2H, BadgeKind::Resist, 10}, {Icon::Ice, BadgeKind::ResistRank, 11}}));
    CHECK(group(s, rules, BadgeGroup::Status) ==
          (std::vector<InfoIcon>{{Icon::Paralyze, BadgeKind::StatusWeak, -3}, {Icon::Gravity, BadgeKind::StatusRank, 6}}));
    CHECK(group(s, rules, BadgeGroup::Immune) ==
          (std::vector<InfoIcon>{{Icon::Bind, BadgeKind::Immune, 0}, {Icon::Stun, BadgeKind::Immune, 0}}));
}

TEST(each_icon_a_mobs_row_holds_belongs_to_its_own_filter_kind)
{
    struct Pair
    {
        Icon icon;
        DetectionIcon kind;
        MobRecord mob;
    };
    const Pair pairs[] = {
        {Icon::AggroNQ, DetectionIcon::Aggro, Mob(kMobAggressive, 0)},
        {Icon::AggroHQ, DetectionIcon::Aggro, Mob(kMobAggressive | kMobNotorious, 0)},
        {Icon::PassiveNQ, DetectionIcon::Aggro, Mob(0, 0)},
        {Icon::PassiveHQ, DetectionIcon::Aggro, Mob(kMobNotorious, 0)},
        {Icon::Link, DetectionIcon::Link, Mob(kMobLink, 0)},
        {Icon::Sight, DetectionIcon::Sight, Mob(0, kDetectSight)},
        {Icon::TrueSight, DetectionIcon::TrueSight, Mob(kMobTrueDetection, kDetectSight)},
        {Icon::Sound, DetectionIcon::Sound, Mob(0, kDetectHearing)},
        {Icon::Scent, DetectionIcon::Scent, Mob(0, kDetectScent)},
        {Icon::Magic, DetectionIcon::Magic, Mob(0, kDetectMagic)},
        {Icon::Ability, DetectionIcon::Ability, Mob(0, kDetectAbility)},
        {Icon::Blood, DetectionIcon::Blood, Mob(0, kDetectLowHp)},
    };
    for (const Pair& p : pairs)
    {
        CHECK(DetectionOf(p.icon) == p.kind);
        const auto holds = [&](uint32_t shown) {
            const std::vector<Icon> row = List(IconsFor(&p.mob, shown));
            return std::ranges::find(row, p.icon) != row.end();
        };
        CHECK(holds(DetectionBit(p.kind)));
        CHECK(!holds(kAllDetection & ~DetectionBit(p.kind)));
    }
}

TEST(a_grey_icon_is_painted_as_the_game_paints_a_names_pearl)
{
    // The game's colors stand at half strength, 0x80 for the texture as it is: a name's pearl is its texture times twice
    // the linkshell's bytes, as the server sends them (4-2-0 is 79, 47, 15). Greys 50, 100 and 200 and a clear 100.
    const unsigned char greys[] = {50, 50, 50, 255, 100, 100, 100, 255, 200, 200, 200, 255, 100, 100, 100, 0};
    const IconBitmap icon{greys, 2, 2};
    const std::vector<unsigned char> painted = PaintedIcon(icon, 0xFF4F2F0Fu);
    const std::vector<unsigned char> expected = {6, 18, 31, 255, 12, 37, 62, 255, 24, 74, 124, 255, 12, 37, 62, 0};
    CHECK(painted == expected);
    // A white shell doubles the pearl's light, held to full; the color's own alpha counts for nothing.
    const std::vector<unsigned char> white = PaintedIcon(icon, 0x00FFFFFFu);
    const std::vector<unsigned char> doubled = {100, 100, 100, 255, 200, 200, 200, 255, 255, 255, 255, 255, 200, 200, 200, 0};
    CHECK(white == doubled);
}

namespace
{
    // A 32 x 32 picture as ReadGamePearl gives one: clear black, with what paint puts at each pixel. The game's alpha
    // stands at half: 119 is opaque.
    template <typename Paint>
    ColorPicture GamePicture(Paint paint)
    {
        ColorPicture picture{"menu    ustatshd", 32, 32, std::vector<uint8_t>(32 * 32 * 4, 0)};
        for (int y = 0; y < 32; ++y)
            for (int x = 0; x < 32; ++x)
                paint(x, y, picture.bgra.data() + (y * 32 + x) * 4);
        return picture;
    }
    void Put(uint8_t* pixel, uint8_t b, uint8_t g, uint8_t r, uint8_t a)
    {
        pixel[0] = b, pixel[1] = g, pixel[2] = r, pixel[3] = a;
    }
    // A shape of 64 x 64, as XIUI's pearl stands among the built-in icons.
    constexpr int kShapeSide = 64;
    struct PearlShape
    {
        std::vector<uint8_t> bgra = std::vector<uint8_t>(kShapeSide * kShapeSide * 4, 0);
        IconBitmap Bitmap() const { return IconBitmap{bgra.data(), kShapeSide, kShapeSide}; }
    };
    template <typename Paint>
    PearlShape ShapeOf(Paint paint)
    {
        PearlShape shape;
        for (int y = 0; y < kShapeSide; ++y)
            for (int x = 0; x < kShapeSide; ++x)
                paint(x, y, shape.bgra.data() + (y * kShapeSide + x) * 4);
        return shape;
    }
    const uint8_t* At(const std::vector<unsigned char>& bgra, int x, int y) { return bgra.data() + (y * kShapeSide + x) * 4; }
    bool Near(const uint8_t* pixel, int b, int g, int r, int slack)
    {
        return std::abs(pixel[0] - b) <= slack && std::abs(pixel[1] - g) <= slack && std::abs(pixel[2] - r) <= slack;
    }
    const auto kFlatGrey = [](int, int, uint8_t* pixel) { Put(pixel, 120, 120, 120, 255); };
}

TEST(the_game_lit_pearl_lays_the_games_visible_square_over_the_shapes)
{
    // A 16 x 16 opaque square from (8, 8), its top left quarter one color and the rest another, on a flat shape: four
    // shape pixels a texel, so shape (8, 8) is texel (10, 10) and (56, 56) texel (22, 22), six texels from where they meet.
    const ColorPicture game = GamePicture([](int x, int y, uint8_t* pixel) {
        if (x < 8 || y < 8 || x >= 24 || y >= 24) return;
        if (x < 16 && y < 16) Put(pixel, 40, 80, 120, 119);
        else Put(pixel, 90, 60, 30, 119);
    });
    const PearlShape shape                    = ShapeOf(kFlatGrey);
    const std::vector<unsigned char> lit = GameLitPearl(game, shape.Bitmap());
    CHECK_EQ(lit.size(), shape.bgra.size());
    CHECK(Near(At(lit, 8, 8), 40, 80, 120, 1));
    CHECK(Near(At(lit, 56, 56), 90, 60, 30, 1));
    CHECK(GameLitPearl(GamePicture([](int, int, uint8_t*) {}), shape.Bitmap()).empty());
}

TEST(the_game_lit_pearl_has_the_shapes_alpha_and_the_games_color_out_to_its_edge)
{
    // The game's disc of radius 10 in clear black, under a shape whose disc reaches further: every pixel the shape
    // shows has the game's color, never the black of its clear pixels, and the shape's own alpha.
    const ColorPicture game = GamePicture([](int x, int y, uint8_t* pixel) {
        const float dx = static_cast<float>(x) + 0.5f - 16.0f, dy = static_cast<float>(y) + 0.5f - 16.0f;
        if (dx * dx + dy * dy <= 100.0f) Put(pixel, 200, 150, 100, 119);
    });
    const PearlShape shape = ShapeOf([](int x, int y, uint8_t* pixel) {
        const float dx = static_cast<float>(x) + 0.5f - 32.0f, dy = static_cast<float>(y) + 0.5f - 32.0f;
        Put(pixel, 120, 120, 120, dx * dx + dy * dy <= 31.0f * 31.0f ? 255 : dx * dx + dy * dy <= 32.0f * 32.0f ? 128 : 0);
    });
    const std::vector<unsigned char> lit = GameLitPearl(game, shape.Bitmap());
    bool alphas = true, colored = true;
    for (int y = 0; y < kShapeSide; ++y)
        for (int x = 0; x < kShapeSide; ++x)
        {
            alphas = alphas && At(lit, x, y)[3] == At(shape.bgra, x, y)[3];
            if (At(lit, x, y)[3] > 0) colored = colored && Near(At(lit, x, y), 200, 150, 100, 2);
        }
    CHECK(alphas);
    CHECK(colored);
}

TEST(the_game_lit_pearl_keeps_the_shapes_outline_but_not_its_broad_light)
{
    // The shape: grey 100 over its top half, 200 under it (XIUI's bright bottom), inside a dark ring two pixels wide.
    // The ring stays dark, at 20 over its neighborhood's 50 or so; the halves come out alike, as the game's flat color
    // has them.
    const ColorPicture game = GamePicture([](int, int, uint8_t* pixel) { Put(pixel, 120, 120, 120, 119); });
    const PearlShape shape       = ShapeOf([](int x, int y, uint8_t* pixel) {
        const bool ring = x < 2 || y < 2 || x >= kShapeSide - 2 || y >= kShapeSide - 2;
        const uint8_t v = ring ? 20 : y < kShapeSide / 2 ? 100 : 200;
        Put(pixel, v, v, v, 255);
    });
    const std::vector<unsigned char> lit = GameLitPearl(game, shape.Bitmap());
    const int top = At(lit, 32, 12)[1], bottom = At(lit, 32, 52)[1], ring = At(lit, 32, 0)[1];
    CHECK(std::abs(top - bottom) <= 6);
    CHECK(ring * 2 < top);
}

TEST(the_game_lit_pearl_smooths_the_games_block_noise)
{
    // Texels alternating 100 and 150, as DXT's four colors a block leave the game's sheen, come out their mean.
    const ColorPicture game = GamePicture([](int x, int y, uint8_t* pixel) {
        const uint8_t v = (x + y) % 2 == 0 ? 100 : 150;
        Put(pixel, v, v, v, 119);
    });
    const PearlShape shape                    = ShapeOf(kFlatGrey);
    const std::vector<unsigned char> lit = GameLitPearl(game, shape.Bitmap());
    bool even = true;
    for (int y = 4; y < kShapeSide - 4; ++y)
        for (int x = 4; x < kShapeSide - 4; ++x)
            even = even && Near(At(lit, x, y), 125, 125, 125, 3);
    CHECK(even);
}
