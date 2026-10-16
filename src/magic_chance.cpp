#include "magic_chance.h"

#include "icons.h"

#include <algorithm>
#include <cmath>
#include <span>

namespace headsup
{
    namespace
    {
        // Phoenix's scripts/combat/basic/magic_hit_rate.lua: magic evasion is multiplied by its rank's.
        constexpr double kEvasionMultiplier[kMaxRank - kMinRank + 1] = {0.95, 0.96019, 0.98, 1, 1.023, 1.049, 1.0905,
            1.126, 1.2075, 1.3475, 1.70065, 2.141, 2.2, 2.275, 2.35};
        constexpr int kFlooredRank   = 10; // and above: the hit rate is left at its floor
        constexpr int kResistedRank  = 11; // and above: a status always resists, a nuke never lands whole
        constexpr int kEvenRate      = 25; // equal accuracy and evasion land 75%: 50 and this
        constexpr int kBaseRate      = 50;
        constexpr int kPerLevel      = 4; // hit rate a mob's level above yours takes away
        constexpr double kMinRate    = 0.05;
        constexpr double kMaxRate    = 0.95;
        constexpr int kSealAccuracy  = 256;
        constexpr int kMaxResists    = 3; // a mob's resist tiers
        // scripts/data/status_effect_tables.lua's isTargetResistant: a resist trait's power has 5 added, and none
        // counts until it passes that.
        constexpr int kTraitFloor   = 5;
        constexpr double kPercentScale   = 100;

        // scripts/globals/spells/damage_spell.lua and enfeebling_spell.lua, enfeebling_song.lua's songs: each spell's
        // stat, bonus accuracy and element; status_effect_tables.lua's allowed resist state for its effect.
        constexpr MagicSpell kElementalNuke{MagicSkill::Elemental, CasterStat::Int, 0, Element::Fire, std::nullopt, 0};
        constexpr MagicSpell kBanish{MagicSkill::Divine, CasterStat::Mnd, 0, Element::Light, std::nullopt, 0};
        constexpr MagicSpell kBio{MagicSkill::Dark, CasterStat::Int, 0, Element::Dark, std::nullopt, 0};
        constexpr MagicSpell kParalyze{MagicSkill::Enfeebling, CasterStat::Mnd, -10, Element::Ice, StatusRank::Paralyze, 1};
        constexpr MagicSpell kBind{MagicSkill::Enfeebling, CasterStat::Int, 0, Element::Ice, StatusRank::Bind, 2};
        constexpr MagicSpell kSilence{MagicSkill::Enfeebling, CasterStat::Mnd, 0, Element::Wind, StatusRank::Silence, 1};
        constexpr MagicSpell kSlow{MagicSkill::Enfeebling, CasterStat::Mnd, 10, Element::Earth, StatusRank::Slow, 1};
        constexpr MagicSpell kPoison{MagicSkill::Enfeebling, CasterStat::Int, 0, Element::Water, StatusRank::Poison, 1};
        constexpr MagicSpell kFoeLullaby{MagicSkill::Singing, CasterStat::Chr, 0, Element::Light, StatusRank::LightSleep, 1};
        constexpr MagicSpell kRepose{MagicSkill::Divine, CasterStat::Mnd, 0, Element::Light, StatusRank::LightSleep, 1};
        constexpr MagicSpell kSleep{MagicSkill::Enfeebling, CasterStat::Int, 0, Element::Dark, StatusRank::DarkSleep, 1};
        constexpr MagicSpell kBlind{MagicSkill::Enfeebling, CasterStat::Int, 0, Element::Dark, StatusRank::Blind, 1};
        constexpr MagicSpell kStun{MagicSkill::Dark, CasterStat::Int, 200, Element::Lightning, StatusRank::Stun, 2};
        constexpr MagicSpell kGravity{MagicSkill::Enfeebling, CasterStat::Int, 0, Element::Wind, StatusRank::Gravity, 1};

        constexpr Stat kStatOf[kCasterStatCount] = {Stat::Int, Stat::Mnd, Stat::Chr};
        // Each MagicSkill's ID in Phoenix's skill types, and the instrument songs add half of.
        constexpr int kSkillIds[kMagicSkillCount] = {32, 35, 36, 37, 40};
        constexpr int kWindInstrument             = 42;
        constexpr uint16_t kSkillValue            = 0x7FFF; // the rest is the cap flag
        // charutils::BuildingCharSkillsTable: a skill neither job has is sent as 0 with the cap flag, one never
        // raised as a plain 0.
        constexpr uint16_t kJobLacksSkill = 0x8000;
        constexpr int kStaffAccuracy              = 10;     // a staff bonus point's
        constexpr int16_t kElementalSealEffect = 79, kDarkSealEffect = 345; // Phoenix's data/status_effects.yaml

        int Index(auto value) { return static_cast<int>(value); }

        std::span<const MagicSpell> SpellsOf(StatusRank status)
        {
            static constexpr MagicSpell kLightSleeps[] = {kFoeLullaby, kRepose};
            switch (status)
            {
            case StatusRank::Paralyze: return {&kParalyze, 1};
            case StatusRank::Bind: return {&kBind, 1};
            case StatusRank::Silence: return {&kSilence, 1};
            case StatusRank::Slow: return {&kSlow, 1};
            case StatusRank::Poison: return {&kPoison, 1};
            case StatusRank::LightSleep: return kLightSleeps;
            case StatusRank::DarkSleep: return {&kSleep, 1};
            case StatusRank::Blind: return {&kBlind, 1};
            case StatusRank::Stun: return {&kStun, 1};
            case StatusRank::Gravity: return {&kGravity, 1};
            }
            return {&kParalyze, 1};
        }

        // magicAccuracyFromStatDifference: a point a point within 10, then half, then a quarter, held to 30.
        double AccuracyFromStats(int difference)
        {
            double accuracy = difference;
            if (difference <= -31) accuracy = -20 + (difference + 30) / 4.0;
            else if (difference <= -11) accuracy = -10 + (difference + 10) / 2.0;
            else if (difference >= 31) accuracy = 20 + (difference - 30) / 4.0;
            else if (difference >= 11) accuracy = 10 + (difference - 10) / 2.0;
            return std::clamp(accuracy, -30.0, 30.0);
        }

        int MagicEvasion(const MagicSpell& spell, const MagicTarget& target)
        {
            int evasion = target.stats.value[Index(Stat::MagicEvasion)] + target.resistances.elementMeva[Index(spell.element)];
            if (spell.status) evasion += target.resistances.statusMeva[Index(*spell.status)];
            return evasion;
        }

        bool CanCast(const MagicSpell& spell, const Caster& caster) { return caster.skill[Index(spell.skill)] != kNoSkill; }

        std::string Percent(double chance) { return std::to_string(static_cast<int>(std::lround(chance * kPercentScale))) + "%"; }
    }

    Caster CasterOf(const CasterReading& reading)
    {
        Caster caster;
        caster.level = reading.level;
        for (int i = 0; i < kMagicSkillCount; ++i)
        {
            const uint16_t raw = reading.combatSkills[kSkillIds[i]];
            caster.skill[i]    = raw == kJobLacksSkill ? kNoSkill : raw & kSkillValue;
        }
        std::ranges::copy(reading.stat, caster.stat);
        for (const uint16_t id : reading.equipped)
        {
            const ItemMagic* item = id != 0 ? ItemMagicOf(id) : nullptr;
            if (item == nullptr) continue;
            caster.macc += item->macc;
            for (int e = 0; e < kElementCount; ++e)
                caster.elementMacc[e] += item->elementMacc[e] + item->staffBonus[e] * kStaffAccuracy;
        }
        const uint16_t ranged = reading.equipped[kRangedSlot];
        const ItemMagic* instrument = ranged != 0 ? ItemMagicOf(ranged) : nullptr;
        if (instrument != nullptr && instrument->skill == kWindInstrument)
            caster.windInstrumentSkill = reading.combatSkills[kWindInstrument] & kSkillValue;
        caster.elementalSeal = std::ranges::find(reading.buffs, kElementalSealEffect) != reading.buffs.end();
        caster.darkSeal      = std::ranges::find(reading.buffs, kDarkSealEffect) != reading.buffs.end();
        return caster;
    }

    double EvasionMultiplierOf(int rank) { return kEvasionMultiplier[std::clamp(rank, kMinRank, kMaxRank) - kMinRank]; }

    MagicSpell NukeOf(Element element)
    {
        if (element == Element::Light) return kBanish;
        if (element == Element::Dark) return kBio;
        MagicSpell nuke = kElementalNuke;
        nuke.element    = element;
        return nuke;
    }

    MagicSpell StatusSpellOf(StatusRank status, const Caster* caster)
    {
        const std::span<const MagicSpell> spells = SpellsOf(status);
        if (caster != nullptr)
            for (const MagicSpell& spell : spells)
                if (CanCast(spell, *caster)) return spell;
        return spells.front();
    }

    std::optional<double> ChanceOf(const MagicSpell& spell, const Caster& caster, const MagicTarget& target, int rank)
    {
        if (!CanCast(spell, caster)) return std::nullopt;
        if (rank >= kResistedRank) return 0.0;
        double rate = kMinRate;
        if (rank < kFlooredRank)
        {
            const int stat     = Index(spell.stat);
            double accuracy    = caster.skill[Index(spell.skill)] + caster.macc + caster.elementMacc[Index(spell.element)] +
                              spell.bonusMacc +
                              AccuracyFromStats(caster.stat[stat] - target.stats.value[Index(kStatOf[stat])]);
            if (spell.skill == MagicSkill::Singing) accuracy += caster.windInstrumentSkill / 2;
            if (caster.elementalSeal && spell.skill != MagicSkill::Dark && spell.skill != MagicSkill::Divine)
                accuracy += kSealAccuracy;
            if (caster.darkSeal && spell.skill == MagicSkill::Dark) accuracy += kSealAccuracy;
            const int evasion = static_cast<int>(std::floor(MagicEvasion(spell, target) * EvasionMultiplierOf(rank)));
            int hit           = static_cast<int>(std::floor(accuracy)) - evasion + kEvenRate;
            if (hit < 0) hit = static_cast<int>(std::floor(hit / 2.0));
            hit -= std::clamp(target.level - caster.level, 0, static_cast<int>(kPercentScale)) * kPerLevel;
            rate = std::clamp((kBaseRate + hit) / kPercentScale, kMinRate, kMaxRate);
        }
        if (!spell.status) return rate;
        const double resisted = spell.allowedResists >= kMaxResists ? 0.0 : std::pow(1 - rate, spell.allowedResists + 1);
        const int trait       = target.resistances.statusResist[Index(*spell.status)] + kTraitFloor;
        const double unmoved  = trait > kTraitFloor ? 1 - std::min(trait / kPercentScale, 1.0) : 1.0;
        return unmoved * (1 - resisted);
    }

    int EvasionLost(const MagicSpell& spell, const MagicTarget& target, int rank)
    {
        const int evasion = MagicEvasion(spell, target);
        return evasion - static_cast<int>(std::floor(evasion * EvasionMultiplierOf(rank)));
    }

    std::string RankNumberText(RankDisplay display, bool countAccuracy, int rank, const MagicSpell& spell,
        const Caster* caster, const std::optional<MagicTarget>& target)
    {
        if (display == RankDisplay::Rank || !target) return std::to_string(rank);
        const std::optional<double> chance = caster != nullptr ? ChanceOf(spell, *caster, *target, rank) : std::nullopt;
        if (display == RankDisplay::TotalChance && chance) return Percent(*chance);
        if (countAccuracy && chance)
            return PercentText(static_cast<int>(std::lround((*chance - *ChanceOf(spell, *caster, *target, 0)) * kPercentScale)));
        const int widest = static_cast<int>(std::lround((kMaxRate - kMinRate) * kPercentScale));
        return PercentText(std::clamp(EvasionLost(spell, *target, rank), -widest, widest));
    }
}
