#include "zones.h"

#include <iterator>

namespace headsup
{
    namespace
    {
#include "generated/zones.inc"

        // Phoenix's REGION_TYPE, 0 to 45 (zone.h), as players spell the regions.
        constexpr std::string_view kRegionNames[] = {"Ronfaure", "Zulkheim", "Norvallen", "Gustaberg", "Derfland",
            "Sarutabaruta", "Kolshushu", "Aragoneu", "Fauregandi", "Valdeaunia", "Qufim Island", "Li'Telor", "Kuzotz",
            "Vollbow", "Elshimo Lowlands", "Elshimo Uplands", "Tu'Lia", "Movalpolos", "Tavnazian Archipelago",
            "San d'Oria", "Bastok", "Windurst", "Jeuno", "Dynamis", "Tavnazian Marquisate", "Promyvion", "Lumoria",
            "Limbus", "West Aht Urhgan", "Mamool Ja Savagelands", "Halvung", "Arrapago", "Alzadaal", "Ronfaure Front",
            "Norvallen Front", "Gustaberg Front", "Derfland Front", "Sarutabaruta Front", "Aragoneu Front",
            "Fauregandi Front", "Valdeaunia Front", "Abyssea", "The Threshold", "Abdhaljs", "Adoulin Islands",
            "East Ulbuka"};
        static_assert(std::size(kRegionNames) == kRegionCount);
    }

    // gen_zones.py writes each zone at its id.
    const ZoneRecord* ZoneById(uint16_t id) { return id < std::size(kZones) ? &kZones[id] : nullptr; }

    const ZoneRecord* KnownZone(uint16_t id) { return id != 0 ? ZoneById(id) : nullptr; }

    std::string ZoneTitle(uint16_t id)
    {
        if (const ZoneRecord* zone = KnownZone(id)) return zone->name;
        return id != 0 ? "Zone " + std::to_string(id) : "-";
    }

    std::string_view ZoneRegion(const ZoneRecord* zone)
    {
        return zone != nullptr ? RegionName(zone->region) : std::string_view();
    }

    std::string_view RegionName(uint8_t region)
    {
        return region < std::size(kRegionNames) ? kRegionNames[region] : std::string_view{};
    }

    std::optional<uint16_t> PackedWeather(const ZoneRecord& zone, uint32_t vanaDay)
    {
        if (zone.calendar >= std::size(kWeatherCalendars)) return std::nullopt;
        return kWeatherOdds[kWeatherCalendars[zone.calendar][vanaDay % static_cast<uint32_t>(kWeatherDays)]];
    }
}
