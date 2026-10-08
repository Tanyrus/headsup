#include "mobdata.h"

#include <algorithm>
#include <cctype>
#include <iterator>

namespace headsup
{
    namespace
    {
#include "generated/mobdata.inc"

        bool NextAlnum(std::string_view s, size_t& i)
        {
            while (i < s.size() && !std::isalnum(static_cast<unsigned char>(s[i])))
                ++i;
            return i < s.size();
        }
    }

    bool IsAggressive(const MobRecord& mob)
    {
        return (mob.flags & (kMobAggressive | kMobAlwaysAggro)) != 0 && (mob.flags & (kMobNoAggro | kMobFollows)) == 0;
    }

    bool SameMobName(std::string_view a, std::string_view b)
    {
        size_t i = 0, j = 0;
        for (;;)
        {
            const bool moreA = NextAlnum(a, i);
            const bool moreB = NextAlnum(b, j);
            if (!moreA || !moreB) return moreA == moreB;
            if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[j]))) return false;
            ++i;
            ++j;
        }
    }

    bool TaggedMobName(std::string_view recorded, std::string_view shown)
    {
        size_t i = 0, j = 0;
        while (NextAlnum(shown, j))
        {
            if (!NextAlnum(recorded, i) || std::tolower(static_cast<unsigned char>(recorded[i])) !=
                                               std::tolower(static_cast<unsigned char>(shown[j])))
                return false;
            ++i;
            ++j;
        }
        // The client's name is used up mid-record: a tag must follow, after a break between words.
        return i < recorded.size() && !std::isalnum(static_cast<unsigned char>(recorded[i])) && NextAlnum(recorded, i);
    }

    const MobRecord* MobById(uint32_t serverId)
    {
        const auto it = std::lower_bound(std::begin(kMobs), std::end(kMobs), serverId,
            [](const MobRecord& r, uint32_t id) { return r.id < id; });
        return it == std::end(kMobs) || it->id != serverId ? nullptr : &*it;
    }

    const MobRecord* FindMob(uint32_t serverId, std::string_view displayName)
    {
        const MobRecord* mob = MobById(serverId);
        return mob != nullptr && (SameMobName(mob->name, displayName) || TaggedMobName(mob->name, displayName)) ? mob : nullptr;
    }

}
