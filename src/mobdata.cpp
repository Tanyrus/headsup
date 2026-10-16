#include "mobdata.h"

#include <algorithm>
#include <iterator>

namespace headsup
{
    namespace
    {
        // Where one drop list's entries sit in kMobDrops and its groups in kMobDropGroups.
        struct MobDropList
        {
            uint32_t first;
            uint16_t count;
            uint16_t firstGroup;
            uint8_t groups;
        };

        // Where one list's entries sit in its table.
        struct MobSpan
        {
            uint32_t first;
            uint16_t count;
        };

#include "generated/mobdata.inc"
#include "generated/item_magic.inc"
#include "generated/mob_info.inc"

        static_assert(std::size(kMobInfoOf) == std::size(kMobs), "mob_info.inc must name a row for each mob");

        constexpr MobInfo kNoInfo{0, 0, 0, 0, kNoWindow, kNoWindow, 0, 0, 0, "", 0, 0, 0, 0, 0};

        const MobDropList& DropListOf(const MobRecord& mob)
        {
            return mob.drops < std::size(kMobDropLists) ? kMobDropLists[mob.drops] : kMobDropLists[0];
        }

        template <typename T, size_t N, size_t M>
        std::span<const T> SpanOf(const T (&entries)[N], const MobSpan (&spans)[M], uint16_t index)
        {
            const MobSpan& span = index < M ? spans[index] : spans[0];
            return std::span<const T>(entries).subspan(span.first, span.count);
        }
    }

    bool IsAggressive(const MobRecord& mob)
    {
        return (mob.flags & (kMobAggressive | kMobAlwaysAggro)) != 0 && (mob.flags & (kMobNoAggro | kMobFollows)) == 0;
    }

    const MobRecord* MobById(uint32_t serverId)
    {
        const auto it = std::lower_bound(std::begin(kMobs), std::end(kMobs), serverId,
            [](const MobRecord& r, uint32_t id) { return r.id < id; });
        return it == std::end(kMobs) || it->id != serverId ? nullptr : &*it;
    }

    const MobResistances& ResistancesOf(const MobRecord& mob)
    {
        return mob.resistances < std::size(kMobResistances) ? kMobResistances[mob.resistances] : kMobResistances[0];
    }

    const MobStats* StatsAt(const MobRecord& mob, int level)
    {
        if (mob.stats == 0 || mob.stats >= std::size(kMobStatRuns) || level < mob.minLevel || level > mob.maxLevel)
            return nullptr;
        return &kMobStats[kMobStatRuns[mob.stats] + static_cast<uint32_t>(level - mob.minLevel)];
    }

    const ItemMagic* ItemMagicOf(uint16_t itemId)
    {
        const auto it = std::lower_bound(std::begin(kItemMagic), std::end(kItemMagic), itemId,
            [](const ItemMagic& item, uint16_t id) { return item.id < id; });
        return it == std::end(kItemMagic) || it->id != itemId ? nullptr : &*it;
    }

    std::span<const MobDrop> DropsOf(const MobRecord& mob)
    {
        const MobDropList& list = DropListOf(mob);
        return std::span(kMobDrops).subspan(list.first, list.count);
    }

    const MobDropGroup& DropGroupOf(const MobRecord& mob, uint8_t group)
    {
        const MobDropList& list = DropListOf(mob);
        return group >= 1 && group <= list.groups ? kMobDropGroups[list.firstGroup + group - 1] : kMobDropGroups[0];
    }

    const MobInfo& InfoOf(const MobRecord& mob)
    {
        const MobRecord* record = MobById(mob.id);
        return record != nullptr ? kMobInfo[kMobInfoOf[record - std::begin(kMobs)]] : kNoInfo;
    }

    const MobCombat* CombatAt(const MobRecord& mob, int level)
    {
        const MobInfo& info = InfoOf(mob);
        if (info.combat == 0 || info.combat >= std::size(kMobCombatRuns) || level < mob.minLevel || level > mob.maxLevel)
            return nullptr;
        return &kMobCombat[kMobCombatRuns[info.combat] + static_cast<uint32_t>(level - mob.minLevel)];
    }

    const MobDefenses& DefensesOf(const MobRecord& mob)
    {
        const uint16_t index = InfoOf(mob).defenses;
        return index < std::size(kMobDefenses) ? kMobDefenses[index] : kMobDefenses[0];
    }

    std::span<const MobSkill> SkillsOf(const MobRecord& mob) { return SpanOf(kMobSkills, kMobSkillLists, InfoOf(mob).skills); }

    std::span<const MobSpell> SpellsOf(const MobRecord& mob) { return SpanOf(kMobSpells, kMobSpellLists, InfoOf(mob).spells); }

    std::span<const MobRegime> RegimesOf(const MobRecord& mob)
    {
        return SpanOf(kMobRegimes, kMobRegimeRuns, InfoOf(mob).regimes);
    }

    const Regime* RegimeById(uint16_t id)
    {
        const auto it = std::lower_bound(std::begin(kRegimes), std::end(kRegimes), id,
            [](const Regime& r, uint16_t want) { return r.id < want; });
        return it == std::end(kRegimes) || it->id != id ? nullptr : &*it;
    }

}
