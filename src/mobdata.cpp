#include "mobdata.h"

#include <algorithm>
#include <cctype>
#include <iterator>

namespace aggroglow
{
    namespace
    {
#include "generated/mobdata.inc"

        // Moves i to the next letter or digit; false when there is none.
        bool NextAlnum(std::string_view s, size_t& i)
        {
            while (i < s.size() && !std::isalnum(static_cast<unsigned char>(s[i])))
                ++i;
            return i < s.size();
        }
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

    const MobRecord* FindMob(uint32_t serverId, std::string_view displayName)
    {
        const auto it = std::lower_bound(std::begin(kMobs), std::end(kMobs), serverId,
            [](const MobRecord& r, uint32_t id) { return r.id < id; });
        if (it == std::end(kMobs) || it->id != serverId || !SameMobName(it->name, displayName)) return nullptr;
        return &*it;
    }

    size_t MobRecordCount()
    {
        return std::size(kMobs);
    }
}
