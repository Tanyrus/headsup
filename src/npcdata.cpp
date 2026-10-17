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

    std::vector<const NpcLink*> LinksOf(const NpcRecord& npc)
    {
        std::vector<const NpcLink*> links;
        for (const uint16_t index : SpanOf(kNpcLinkIds, npc.links))
            if (index < std::size(kNpcLinks)) links.push_back(&kNpcLinks[index]);
        return links;
    }

    const char* LogNameOf(const NpcLink& link)
    {
        if (link.kind == kLinkQuest) return link.log < std::size(kQuestLogNames) ? kQuestLogNames[link.log] : nullptr;
        if (link.kind == kLinkMission)
            return link.log < std::size(kMissionLogNames) ? kMissionLogNames[link.log] : nullptr;
        return nullptr;
    }

    std::vector<const char*> PurposesOf(const NpcRecord& npc)
    {
        std::vector<const char*> purposes;
        for (const uint16_t index : SpanOf(kNpcPurposeIds, npc.purposes))
            if (index < std::size(kNpcPurposes)) purposes.push_back(kNpcPurposes[index]);
        return purposes;
    }

    std::span<const NpcAct> ActsOf(const NpcRecord& npc) { return SpanOf(kNpcActs, npc.acts); }

    std::span<const NpcActItem> ItemsOf(const NpcAct& act) { return SpanOf(kNpcActItems, act.items); }

    std::span<const NpcWhenEntry> WhenOf(const NpcAct& act) { return SpanOf(kNpcWhen, act.when); }

    std::span<const NpcSay> SaysOf(const NpcAct& act) { return SpanOf(kNpcSays, act.says); }

    std::span<const uint32_t> PopsOf(const NpcAct& act) { return SpanOf(kNpcMobs, act.pops); }

    std::span<const uint16_t> GivesOf(const NpcAct& act) { return SpanOf(kNpcItems, act.gives); }

    std::span<const uint16_t> KeyItemsOf(const NpcAct& act) { return SpanOf(kNpcKeyItems, act.keyItems); }

    std::span<const uint32_t> ParamsOf(const NpcSay& say) { return SpanOf(kNpcSayParams, say.params); }

    const char* WordsOf(const NpcSay& say)
    {
        return say.text > 0 && say.text < std::size(kNpcTexts) ? kNpcTexts[say.text].words : nullptr;
    }

    const NpcLink* LinkAt(uint16_t index)
    {
        return index > 0 && index < std::size(kNpcLinks) ? &kNpcLinks[index] : nullptr;
    }

    const NpcShop* ShopOf(const NpcRecord& npc)
    {
        return npc.shop > 0 && npc.shop < std::size(kNpcShops) ? &kNpcShops[npc.shop] : nullptr;
    }

    std::span<const NpcStockItem> StockOf(const NpcShop& shop) { return SpanOf(kNpcStock, shop.stock); }

    const char* KeyItemNameOf(uint16_t keyItem)
    {
        const auto it = std::lower_bound(std::begin(kKeyItemNames), std::end(kKeyItemNames), keyItem,
            [](const KeyItemName& k, uint16_t id) { return k.id < id; });
        return it == std::end(kKeyItemNames) || it->id != keyItem ? nullptr : it->name;
    }
}
