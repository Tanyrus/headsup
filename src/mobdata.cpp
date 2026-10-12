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

#include "generated/mobdata.inc"

        const MobDropList& DropListOf(const MobRecord& mob)
        {
            return mob.drops < std::size(kMobDropLists) ? kMobDropLists[mob.drops] : kMobDropLists[0];
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

}
