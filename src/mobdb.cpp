#include "mobdb.h"

#include <algorithm>
#include <iterator>

namespace aggroglow
{
    namespace
    {
#include "generated/mobdb_data.inc"

        struct IndexKey
        {
            uint16_t zone;
            uint16_t index;
        };

        struct NameKey
        {
            uint16_t zone;
            std::string_view name;
        };
    }

    const MobRecord* FindMob(uint16_t zone, uint16_t targetIndex, std::string_view name)
    {
        const auto spawn = std::lower_bound(std::begin(kByIndex), std::end(kByIndex), IndexKey{zone, targetIndex},
            [](const MobRecord& r, const IndexKey& k) { return r.zone != k.zone ? r.zone < k.zone : r.index < k.index; });
        if (spawn != std::end(kByIndex) && spawn->zone == zone && spawn->index == targetIndex && name == spawn->name)
            return &*spawn;

        const auto named = std::lower_bound(std::begin(kByName), std::end(kByName), NameKey{zone, name},
            [](const MobRecord& r, const NameKey& k) { return r.zone != k.zone ? r.zone < k.zone : std::string_view(r.name) < k.name; });
        if (named != std::end(kByName) && named->zone == zone && name == named->name)
            return &*named;
        return nullptr;
    }

    size_t MobRecordCount()
    {
        return std::size(kByIndex) + std::size(kByName);
    }
}
