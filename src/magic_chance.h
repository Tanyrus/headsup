#pragma once

#include "mobdata.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace headsup
{
    // What a rank badge says: the rank itself, how much more often magic lands than at rank 0, or how often it lands.
    enum class RankDisplay : uint8_t
    {
        Rank,
        IncreasedChance,
        TotalChance,
    };
    constexpr int kRankDisplayCount = 3;

    // The skills the badges' spells are cast with, and the stats they read.
    enum class MagicSkill : uint8_t
    {
        Divine,
        Enfeebling,
        Elemental,
        Dark,
        Singing,
    };
    constexpr int kMagicSkillCount = 5;
    enum class CasterStat : uint8_t
    {
        Int,
        Mnd,
        Chr,
    };
    constexpr int kCasterStatCount = 3;

    // A skill neither of your jobs has, as Caster::skill holds it.
    constexpr int kNoSkill = -1;

    // What you bring to the magic hit rate, as the game knows it.
    struct Caster
    {
        int level = 0; // main job level, as synced
        // The server's, with gear and merits, by MagicSkill; 0 for one your job has and never raised.
        std::array<int, kMagicSkillCount> skill = [] {
            std::array<int, kMagicSkillCount> none;
            none.fill(kNoSkill);
            return none;
        }();
        int windInstrumentSkill = 0;            // while a wind instrument is equipped, else 0: songs add half of it
        int stat[kCasterStatCount] = {};        // with gear and effects
        int macc = 0;                           // magic accuracy of your equipment
        int elementMacc[kElementCount] = {};    // each element's, with each elemental staff bonus at ten a point
        bool elementalSeal = false;
        bool darkSeal = false;
    };

    // Phoenix's skill IDs (data/enums/skill_type.yaml), the order of Ashita's combat skills.
    constexpr int kCombatSkillCount = 48;
    // Your equipment slots, Ashita's main (0) to back (15); the ranged slot holds an instrument.
    constexpr int kEquipSlotCount = 16, kRangedSlot = 2;
    // The game's own numbers for a Caster, as the plugin reads them.
    struct CasterReading
    {
        int level = 0;
        uint16_t combatSkills[kCombatSkillCount] = {}; // as the server sends them, the top bit a cap flag
        int stat[kCasterStatCount] = {};               // with gear and effects
        uint16_t equipped[kEquipSlotCount] = {};       // item IDs, 0 for none
        std::span<const int16_t> buffs;                // status effect IDs
    };
    Caster CasterOf(const CasterReading& reading);

    // The spell a badge stands for: an unresisted tier I nuke for an element, the effect landing for a status.
    struct MagicSpell
    {
        MagicSkill skill;
        CasterStat stat; // the target's same stat is held against it
        int bonusMacc;
        Element element;
        std::optional<StatusRank> status; // none for a nuke
        int allowedResists = 0;           // a status's: resists it can take and still land
    };

    // The nuke an element badge stands for: Elemental Magic for Fire to Water, Banish for Light, Bio for Dark.
    MagicSpell NukeOf(Element element);
    // The spell a status badge stands for: the first of its spells your skills can cast, else its first. Sleep, the
    // badge for both sleeps, is dark sleep's.
    MagicSpell StatusSpellOf(StatusRank status, const Caster* caster);

    // The mob at the level its chances are figured for.
    struct MagicTarget
    {
        int level;
        const MobStats& stats;
        const MobResistances& resistances;
    };

    // Phoenix's magic hit rate at a rank (scripts/combat/basic/magic_hit_rate.lua, every zone level corrected): the
    // chance a nuke lands unresisted, or that a status lands past its resist trait; nullopt when your skills cannot
    // cast the spell.
    std::optional<double> ChanceOf(const MagicSpell& spell, const Caster& caster, const MagicTarget& target, int rank);
    // What a rank multiplies magic evasion by.
    double EvasionMultiplierOf(int rank);
    // The mob's magic evasion its rank takes away against the spell, a point a percent of hit rate (negative when it
    // resists).
    int EvasionLost(const MagicSpell& spell, const MagicTarget& target, int rank);

    // A rank badge's number: the rank, or "+5%" more often than at rank 0, or "90%" in all. Your accuracy counts with
    // countAccuracy and is needed for the total; each step falls back on the one before when what it needs is missing.
    std::string RankNumberText(RankDisplay display, bool countAccuracy, int rank, const MagicSpell& spell,
        const Caster* caster, const std::optional<MagicTarget>& target);
}
