#include "npcdata.h"

#include <algorithm>
#include <iterator>

namespace headsup
{
    namespace
    {
#include "generated/npcdata.inc"

        template <typename T, size_t N>
        std::span<const T> SpanOf(const T (&pool)[N], const NpcSpan& span)
        {
            return span.first + span.count <= N ? std::span<const T>(pool).subspan(span.first, span.count)
                                                : std::span<const T>();
        }
    }

    const NpcRecord* NpcById(uint32_t serverId)
    {
        const auto it = std::lower_bound(std::begin(kNpcs), std::end(kNpcs), serverId,
            [](const NpcRecord& r, uint32_t id) { return r.id < id; });
        return it == std::end(kNpcs) || it->id != serverId ? nullptr : &*it;
    }

    std::span<const uint16_t> TradesOf(const NpcRecord& npc) { return SpanOf(kNpcItems, npc.trades); }

    std::span<const uint16_t> GivesOf(const NpcRecord& npc) { return SpanOf(kNpcItems, npc.gives); }

    std::span<const uint16_t> KeyItemsOf(const NpcRecord& npc) { return SpanOf(kNpcKeyItems, npc.keyItems); }

    std::span<const uint16_t> NeedsOf(const NpcRecord& npc) { return SpanOf(kNpcKeyItems, npc.needs); }

    std::span<const uint32_t> PopsOf(const NpcRecord& npc) { return SpanOf(kNpcMobs, npc.pops); }

    const char* KeyItemNameOf(uint16_t keyItem)
    {
        const auto it = std::lower_bound(std::begin(kKeyItemNames), std::end(kKeyItemNames), keyItem,
            [](const KeyItemName& k, uint16_t id) { return k.id < id; });
        return it == std::end(kKeyItemNames) || it->id != keyItem ? nullptr : it->name;
    }
}
