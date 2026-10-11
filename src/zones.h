#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace headsup
{
    constexpr int kWeatherDays       = 2160; // a zone's weather calendar repeats after this many Vana'diel days
    constexpr uint8_t kUnknownRegion = 255;
    constexpr uint8_t kNoCalendar    = 255;

    struct ZoneRecord
    {
        const char* name;  // as players know it: "King Ranperre's Tomb"
        uint8_t region;    // Phoenix's REGION_TYPE, kUnknownRegion for none
        uint8_t calendar;  // its weather calendar, kNoCalendar for none
    };

    const ZoneRecord* ZoneById(uint16_t id);
    // Zone 0 is in the data as "unknown"; the bar reads it, and an id past the data, as no zone it knows.
    const ZoneRecord* KnownZone(uint16_t id);
    // The zone's name, "Zone 999" for one past the data, "-" for zone 0.
    std::string ZoneTitle(uint16_t id);
    // Its region's name; empty for none or an unknown region.
    std::string_view ZoneRegion(const ZoneRecord* zone);
    // Phoenix's REGION_TYPE spelled for players: "Zulkheim", "Tu'Lia"; empty when unknown.
    std::string_view RegionName(uint8_t region);
    // The zone's three weathers on a day of its calendar's kWeatherDays cycle, packed normal << 10 | common << 5 |
    // rare; nullopt without a calendar.
    std::optional<uint16_t> PackedWeather(const ZoneRecord& zone, uint32_t vanaDay);
}
