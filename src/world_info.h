#pragma once

#include "generated/mobflags.h" // Element
#include "weather.h"
#include "zones.h"

#include <cstdint>
#include <optional>
#include <span>

namespace headsup
{
    // A ship's runs on one route (Phoenix's zone data, tools/phoenix/world.py): each opens its door and leaves at these
    // Earth seconds into the route's cycle, which starts at the Vana'diel epoch.
    constexpr int kMostDepartures = 2;
    struct Departure
    {
        uint16_t board, leave;
    };
    struct TravelRoute
    {
        const char* group; // Airships, Boats, Manaclipper, Barge
        const char* name;  // "Jeuno to San d'Oria"
        uint16_t every;
        uint8_t count;
        Departure departures[kMostDepartures];
    };
    // A guild shop: open from opens to closes, Vana'diel hours, but on its holiday (0 Firesday to 7 Darksday).
    struct GuildShop
    {
        const char* guild; // Woodworking, ..., Tenshodo, Linkshell, Chips
        const char* shops; // its merchants: "Wahraga, Gathweeda"
        const char* zone;
        uint8_t opens, closes, holiday;
    };
    std::span<const TravelRoute> TravelRoutes();
    std::span<const GuildShop> GuildShops();

    // The route's next departure: Earth seconds until it leaves, and until its door opens or, while open, 0.
    struct Sailing
    {
        double boardIn = 0.0, leaveIn = 0.0;
        bool boarding  = false;
    };
    Sailing NextSailing(const TravelRoute& route, int64_t unixMs);

    // Open or closed now, and Earth seconds until that changes.
    struct GuildHours
    {
        bool open = false, holiday = false;
        double changeIn = 0.0;
    };
    GuildHours GuildAt(const GuildShop& shop, int64_t unixMs);

    // Phoenix's Race Specific Equipment (vana_time.h rse): a race and a place for each Vana'diel week, changing when the
    // week does.
    struct Rse
    {
        const char* race;
        const char* place;
        double endsIn = 0.0;
    };
    Rse RseAt(int64_t unixMs, int weeksAhead = 0);

    const char* ElementName(Element element);
    // The next day from today, today included, when the zone can roll a weather of the element: its likeliest weather
    // of that element and the element's chance that day. nullopt for a zone without a calendar or one that never rolls it.
    struct ElementWeather
    {
        uint32_t day = 0;
        Weather weather = Weather::None;
        int percent     = 0;
    };
    std::optional<ElementWeather> NextElementWeather(const ZoneRecord& zone, Element element, uint32_t today);
    // How many zones can roll a weather of the element on the day.
    int ZonesWithElement(Element element, uint32_t day);
}
