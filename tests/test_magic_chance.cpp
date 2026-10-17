#include "magic_chance.h"
#include "test.h"

#include <cmath>

using namespace headsup;

namespace
{
    bool Near(std::optional<double> got, double want) { return got && std::abs(*got - want) < 1e-9; }

    // A level 29 Clipper of Qufim Island as Phoenix's data has it: magic evasion 83 (rank C+ at 29), no evasion mods.
    MobStats ClipperStats()
    {
        MobStats stats{};
        stats.value[static_cast<int>(Stat::Int)]          = 25;
        stats.value[static_cast<int>(Stat::Mnd)]          = 25;
        stats.value[static_cast<int>(Stat::Chr)]          = 25;
        stats.value[static_cast<int>(Stat::MagicEvasion)] = 83;
        return stats;
    }

    // A level 30 caster with Elemental and Enfeebling at 93 and the mob's own INT and MND, nothing worn.
    Caster LevelThirty()
    {
        Caster caster;
        caster.level                                          = 30;
        caster.skill[static_cast<int>(MagicSkill::Elemental)]  = 93;
        caster.skill[static_cast<int>(MagicSkill::Enfeebling)] = 93;
        caster.stat[static_cast<int>(CasterStat::Int)]          = 25;
        caster.stat[static_cast<int>(CasterStat::Mnd)]          = 25;
        return caster;
    }
}

TEST(a_weakness_takes_off_magic_evasion_and_lands_more_unresisted_nukes)
{
    const MobStats stats = ClipperStats();
    const MobResistances res{};
    const MagicTarget clipper{29, stats, res};
    const MagicSpell blizzard = NukeOf(Element::Ice);
    // Rank -3: floor(83 * 0.95) = 78; 93 - 78 + 25 = 40, no correction for a lower mob: (50 + 40) / 100.
    CHECK(Near(ChanceOf(blizzard, LevelThirty(), clipper, -3), 0.90));
    // Rank 0: 93 - 83 + 25 = 35.
    CHECK(Near(ChanceOf(blizzard, LevelThirty(), clipper, 0), 0.85));
    // Rank 2: floor(83 * 1.049) = 87; 93 - 87 + 25 = 31.
    CHECK(Near(ChanceOf(blizzard, LevelThirty(), clipper, 2), 0.81));
    CHECK_EQ(EvasionLost(blizzard, clipper, -3), 5);
    CHECK_EQ(EvasionLost(blizzard, clipper, 2), -4);
    CHECK_EQ(EvasionLost(blizzard, clipper, 0), 0);
}

TEST(below_fifty_percent_a_point_counts_half_and_each_level_above_you_takes_four)
{
    const MobStats stats = ClipperStats();
    const MobResistances res{};
    const MagicTarget clipper{29, stats, res};
    Caster caster                                         = LevelThirty();
    caster.level                                          = 25;
    caster.skill[static_cast<int>(MagicSkill::Elemental)] = 70;
    caster.stat[static_cast<int>(CasterStat::Int)]         = 10;
    // INT 15 below: -10 + (-15 + 10) / 2 = -12.5, so 70 - 12.5 floors to 57; 57 - 83 + 25 = -1 halves to floor(-0.5),
    // -1; four levels below the mob take 16 more: (50 - 17) / 100.
    CHECK(Near(ChanceOf(NukeOf(Element::Fire), caster, clipper, 0), 0.33));
}

TEST(the_hit_rate_stays_between_five_and_ninety_five_percent)
{
    const MobStats stats = ClipperStats();
    const MobResistances res{};
    const MagicTarget clipper{29, stats, res};
    Caster caster        = LevelThirty();
    caster.elementalSeal = true; // 256 more
    CHECK(Near(ChanceOf(NukeOf(Element::Ice), caster, clipper, 0), 0.95));
    caster       = LevelThirty();
    caster.level = 1; // 28 levels below: 112 off
    CHECK(Near(ChanceOf(NukeOf(Element::Ice), caster, clipper, 0), 0.05));
}

TEST(at_rank_ten_the_rate_is_its_floor_and_at_eleven_nothing_lands)
{
    const MobStats stats = ClipperStats();
    const MobResistances res{};
    const MagicTarget clipper{29, stats, res};
    CHECK(Near(ChanceOf(NukeOf(Element::Ice), LevelThirty(), clipper, 10), 0.05));
    CHECK(Near(ChanceOf(NukeOf(Element::Ice), LevelThirty(), clipper, 11), 0.0));
    CHECK(Near(ChanceOf(StatusSpellOf(StatusRank::Bind, nullptr), LevelThirty(), clipper, 11), 0.0));
}

TEST(a_status_lands_through_its_allowed_resists_and_past_its_resist_trait)
{
    const MobStats stats = ClipperStats();
    MobResistances res{};
    const MagicTarget clipper{29, stats, res};
    // Paralyze: MND, -10 accuracy, ice. 93 - 10 = 83; 83 - 83 + 25 = 25: 0.75 a roll, and it lands through one resist:
    // 1 - 0.25^2.
    CHECK(Near(ChanceOf(StatusSpellOf(StatusRank::Paralyze, nullptr), LevelThirty(), clipper, 0), 1 - 0.25 * 0.25));
    // Bind lands through two: 93 - 83 + 25 = 35, 1 - 0.15^3.
    CHECK(Near(ChanceOf(StatusSpellOf(StatusRank::Bind, nullptr), LevelThirty(), clipper, 0), 1 - 0.15 * 0.15 * 0.15));
    // A resist trait of 10 resists 15 in 100 first.
    res.statusResist[static_cast<int>(StatusRank::Bind)] = 10;
    CHECK(Near(ChanceOf(StatusSpellOf(StatusRank::Bind, nullptr), LevelThirty(), clipper, 0),
        0.85 * (1 - 0.15 * 0.15 * 0.15)));
}

TEST(a_status_reads_its_spell_s_element_and_its_own_magic_evasion)
{
    const MobStats stats = ClipperStats();
    MobResistances res{};
    res.elementMeva[static_cast<int>(Element::Wind)]   = 10; // Silence is wind
    res.statusMeva[static_cast<int>(StatusRank::Silence)] = 5;
    res.elementMeva[static_cast<int>(Element::Ice)]    = 40; // not Silence's
    const MagicTarget clipper{29, stats, res};
    const MagicSpell silence = StatusSpellOf(StatusRank::Silence, nullptr);
    // 83 + 10 + 5 = 98 at rank -2: floor(98 * 0.96019) = 94, 4 lost; 93 - 94 + 25 = 24, 1 - 0.26^2.
    CHECK_EQ(EvasionLost(silence, clipper, -2), 4);
    CHECK(Near(ChanceOf(silence, LevelThirty(), clipper, -2), 1 - 0.26 * 0.26));
}

TEST(a_status_badge_takes_the_first_spell_your_skills_can_cast)
{
    Caster bard                                         = LevelThirty();
    bard.skill[static_cast<int>(MagicSkill::Enfeebling)] = kNoSkill;
    bard.skill[static_cast<int>(MagicSkill::Elemental)]  = kNoSkill;
    bard.skill[static_cast<int>(MagicSkill::Singing)]    = 90;
    const MagicSpell lullaby                             = StatusSpellOf(StatusRank::LightSleep, &bard);
    CHECK(lullaby.skill == MagicSkill::Singing && lullaby.stat == CasterStat::Chr && lullaby.element == Element::Light);
    Caster whm                                     = LevelThirty();
    whm.skill[static_cast<int>(MagicSkill::Divine)] = 90;
    CHECK(StatusSpellOf(StatusRank::LightSleep, &whm).skill == MagicSkill::Divine); // Repose
    CHECK(StatusSpellOf(StatusRank::LightSleep, nullptr).skill == MagicSkill::Singing);
    CHECK(StatusSpellOf(StatusRank::DarkSleep, &bard).skill == MagicSkill::Enfeebling); // Sleep, which a bard lacks
    const MagicSpell stun = StatusSpellOf(StatusRank::Stun, nullptr);
    CHECK(stun.skill == MagicSkill::Dark && stun.bonusMacc == 200 && stun.allowedResists == 2);
    CHECK(NukeOf(Element::Light).skill == MagicSkill::Divine && NukeOf(Element::Dark).skill == MagicSkill::Dark);
}

TEST(songs_add_half_a_wind_instrument_s_skill)
{
    const MobStats stats = ClipperStats();
    const MobResistances res{};
    const MagicTarget clipper{29, stats, res};
    Caster bard                                      = LevelThirty();
    bard.skill[static_cast<int>(MagicSkill::Singing)] = 60;
    bard.windInstrumentSkill                         = 45;
    bard.stat[static_cast<int>(CasterStat::Chr)]      = 25;
    // 60 + floor(45 / 2) = 82; 82 - 83 + 25 = 24: 1 - 0.26^2.
    CHECK(Near(ChanceOf(StatusSpellOf(StatusRank::LightSleep, &bard), bard, clipper, 0), 1 - 0.26 * 0.26));
}

TEST(a_spell_your_skills_cannot_cast_has_no_chance)
{
    const MobStats stats = ClipperStats();
    const MobResistances res{};
    const MagicTarget clipper{29, stats, res};
    CHECK(!ChanceOf(NukeOf(Element::Light), LevelThirty(), clipper, 0)); // no Divine Magic
    // A skill your job has but never raised still casts, with no accuracy from it: 0 - 83 + 25 = -58 halves to -29.
    Caster unskilled                                         = LevelThirty();
    unskilled.skill[static_cast<int>(MagicSkill::Elemental)] = 0;
    CHECK(Near(ChanceOf(NukeOf(Element::Fire), unskilled, clipper, 0), 0.21));
}

TEST(gear_element_accuracy_and_the_seals_count_for_their_spells)
{
    const MobStats stats = ClipperStats();
    const MobResistances res{};
    const MagicTarget clipper{29, stats, res};
    Caster caster                                    = LevelThirty();
    caster.skill[static_cast<int>(MagicSkill::Elemental)] = 60;
    caster.macc                                      = 3;
    caster.elementMacc[static_cast<int>(Element::Ice)] = 20;
    // 60 + 3 + 20 = 83: 83 - 83 + 25 = 25.
    CHECK(Near(ChanceOf(NukeOf(Element::Ice), caster, clipper, 0), 0.75));
    // The ice staff's accuracy does nothing for fire: 63 - 83 + 25 = 5.
    CHECK(Near(ChanceOf(NukeOf(Element::Fire), caster, clipper, 0), 0.55));
    // Elemental Seal is not for dark magic, Dark Seal is.
    caster.skill[static_cast<int>(MagicSkill::Dark)] = 60;
    caster.elementalSeal                             = true;
    CHECK(Near(ChanceOf(NukeOf(Element::Dark), caster, clipper, 0), 0.55));
    caster.darkSeal = true;
    CHECK(Near(ChanceOf(NukeOf(Element::Dark), caster, clipper, 0), 0.95));
}

TEST(the_badge_number_by_display_and_its_fallbacks)
{
    const MobStats stats = ClipperStats();
    const MobResistances res{};
    const MagicTarget clipper{29, stats, res};
    const Caster caster      = LevelThirty();
    const MagicSpell blizzard = NukeOf(Element::Ice);
    const MagicSpell water    = NukeOf(Element::Water);
    CHECK_EQ(RankNumberText(RankDisplay::Rank, true, -3, blizzard, &caster, clipper), "-3");
    CHECK_EQ(RankNumberText(RankDisplay::IncreasedChance, false, -3, blizzard, &caster, clipper), "+5%");
    CHECK_EQ(RankNumberText(RankDisplay::IncreasedChance, true, -3, blizzard, &caster, clipper), "+5%"); // 90 less 85
    CHECK_EQ(RankNumberText(RankDisplay::IncreasedChance, true, 2, water, &caster, clipper), "-4%");      // 81 less 85
    CHECK_EQ(RankNumberText(RankDisplay::TotalChance, true, -3, blizzard, &caster, clipper), "90%");
    CHECK_EQ(RankNumberText(RankDisplay::TotalChance, false, -3, blizzard, &caster, clipper), "90%");
    // Near the cap your own accuracy leaves little to gain.
    Caster sealed        = caster;
    sealed.elementalSeal = true;
    CHECK_EQ(RankNumberText(RankDisplay::IncreasedChance, true, -3, blizzard, &sealed, clipper), "0%");
    CHECK_EQ(RankNumberText(RankDisplay::IncreasedChance, false, -3, blizzard, &sealed, clipper), "+5%");
    // Without the spell's skill or your data the total is the mob's own; without the mob's stats, the rank.
    const MagicSpell banish = NukeOf(Element::Light);
    CHECK_EQ(RankNumberText(RankDisplay::TotalChance, true, -3, banish, &caster, clipper), "+5%");
    CHECK_EQ(RankNumberText(RankDisplay::TotalChance, true, -3, blizzard, nullptr, clipper), "+5%");
    CHECK_EQ(RankNumberText(RankDisplay::IncreasedChance, true, -3, blizzard, &caster, std::nullopt), "-3");
    CHECK_EQ(RankNumberText(RankDisplay::TotalChance, true, 4, blizzard, &caster, std::nullopt), "4");
}

TEST(your_caster_from_the_game_s_numbers)
{
    CasterReading reading;
    reading.level                                     = 30;
    reading.combatSkills[36]                          = 0x8000 | 93; // Elemental Magic, capped
    reading.combatSkills[35]                          = 80;          // Enfeebling Magic
    reading.combatSkills[37]                          = 0;           // Dark Magic, never raised
    reading.combatSkills[40]                          = 0x8000;      // Singing, which neither job has
    reading.combatSkills[42]                          = 0x8000 | 45; // Wind Instrument
    reading.stat[static_cast<int>(CasterStat::Int)]   = 31;
    reading.equipped[0]                               = 17547; // Ice Staff: ice staff bonus 2, wind -2
    const int16_t buffs[]                             = {79, 255, 345}; // Elemental Seal and Dark Seal
    reading.buffs                                     = buffs;
    const Caster caster                               = CasterOf(reading);
    CHECK_EQ(caster.level, 30);
    CHECK_EQ(caster.skill[static_cast<int>(MagicSkill::Elemental)], 93);
    CHECK_EQ(caster.skill[static_cast<int>(MagicSkill::Enfeebling)], 80);
    CHECK_EQ(caster.skill[static_cast<int>(MagicSkill::Dark)], 0);
    CHECK_EQ(caster.skill[static_cast<int>(MagicSkill::Singing)], kNoSkill);
    CHECK_EQ(caster.stat[static_cast<int>(CasterStat::Int)], 31);
    CHECK_EQ(caster.elementMacc[static_cast<int>(Element::Ice)], 20);
    CHECK_EQ(caster.elementMacc[static_cast<int>(Element::Wind)], -20);
    CHECK_EQ(caster.macc, 0);
    CHECK(caster.elementalSeal && caster.darkSeal);
    CHECK_EQ(caster.windInstrumentSkill, 0); // no instrument in the ranged slot
    reading.equipped[kRangedSlot] = 17344; // a Cornette, a wind instrument
    reading.buffs                 = {};
    const Caster bard             = CasterOf(reading);
    CHECK_EQ(bard.windInstrumentSkill, 45);
    CHECK(!bard.elementalSeal && !bard.darkSeal);
}
