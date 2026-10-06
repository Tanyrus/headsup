#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace aggroglow
{
    struct MobRecord
    {
        uint16_t zone;
        uint16_t index; // entity target index for spawn records, 0 for name records
        const char* name;
        bool aggro;
        bool notorious;
        uint8_t minLevel;
        uint8_t maxLevel;
    };

    // The spawn record for (zone, targetIndex) when its name matches `name`, otherwise the name record for
    // (zone, name); nullptr when MobDB has neither.
    const MobRecord* FindMob(uint16_t zone, uint16_t targetIndex, std::string_view name);

    size_t MobRecordCount();
}
