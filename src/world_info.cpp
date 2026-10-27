#include "world_info.h"

#include "vanadiel.h"

#include <cmath>

namespace headsup
{
    namespace
    {
#include "generated/world.inc"

        constexpr double kMsPerSecond   = 1000.0;
        constexpr int kHoursPerDay      = 24;
        constexpr double kVanaHour      = kEarthSecondsPerVanaDay / kHoursPerDay; // in Earth seconds
        // Phoenix's rse::get_race, 1 to 8 by the game's race numbers, and get_location.
        constexpr const char* kRseRaces[] = {"Hume Male", "Hume Female", "Elvaan Male", "Elvaan Female", "Tarutaru Male",
            "Tarutaru Female", "Mithra", "Galka"};
        constexpr const char* kRsePlaces[] = {"Ordelle's Caves", "Gusgen Mines", "Maze of Shakhrami"};
        constexpr const char* kElementNames[kElementCount] = {"Fire", "Ice", "Wind", "Earth", "Lightning", "Water", "Light",
            "Dark"};

        // Earth seconds from a to b going forward round a cycle.
        double Ahead(double a, double b, double every) { return std::fmod(std::fmod(b - a, every) + every, every); }
    }

    std::span<const TravelRoute> TravelRoutes() { return kTravelRoutes; }

    std::span<const GuildShop> GuildShops() { return kGuildShops; }

    Sailing NextSailing(const TravelRoute& route, int64_t unixMs)
    {
        const double every = route.every;
        const double into  = std::fmod(static_cast<double>(unixMs - kVanaEpochUnixMs) / kMsPerSecond, every);
        Sailing best;
        for (int i = 0; i < route.count; ++i)
        {
            const Departure& d = route.departures[i];
            const double leaveIn = Ahead(into, d.leave, every);
            // Open from its door opening until it leaves: nearer the door's opening behind than the departure.
            const bool boarding = Ahead(d.board, into, every) < Ahead(d.board, d.leave, every);
            if (i == 0 || leaveIn < best.leaveIn)
                best = Sailing{boarding ? 0.0 : Ahead(into, d.board, every), leaveIn, boarding};
        }
        return best;
    }

    GuildHours GuildAt(const GuildShop& shop, int64_t unixMs)
    {
        const VanaTime now   = VanaAt(unixMs);
        const bool holiday   = now.weekday == shop.holiday;
        const double opensAt = SecondsToDay(unixMs, now.day) + shop.opens * kVanaHour;
        if (!holiday && now.hour >= shop.opens && now.hour < shop.closes)
            return GuildHours{true, false, SecondsToDay(unixMs, now.day) + shop.closes * kVanaHour};
        // The first opening still to come on a day that is not its holiday.
        for (int ahead = 0; ahead <= kVanaDaysPerWeek; ++ahead)
        {
            const double at = opensAt + ahead * kEarthSecondsPerVanaDay;
            if (at > 0.0 && (now.weekday + ahead) % kVanaDaysPerWeek != shop.holiday) return GuildHours{false, holiday, at};
        }
        return GuildHours{false, holiday, 0.0};
    }

    Rse RseAt(int64_t unixMs, int weeksAhead)
    {
        const uint32_t week = VanaAt(unixMs).day / kVanaDaysPerWeek + static_cast<uint32_t>(weeksAhead);
        return Rse{kRseRaces[week % std::size(kRseRaces)], kRsePlaces[week % std::size(kRsePlaces)],
            SecondsToDay(unixMs, (week + 1) * kVanaDaysPerWeek)};
    }

    const char* ElementName(Element element) { return kElementNames[static_cast<int>(element) % kElementCount]; }

    std::optional<ElementWeather> NextElementWeather(const ZoneRecord& zone, Element element, uint32_t today)
    {
        for (uint32_t day = today; day < today + kWeatherDays; ++day)
        {
            const std::optional<WeatherOdds> odds = WeatherOn(zone, day);
            if (!odds) return std::nullopt;
            ElementWeather found{day};
            for (const WeatherChance& chance : Chances(*odds))
            {
                if (WeatherElement(chance.weather) != element) continue;
                if (found.percent == 0) found.weather = chance.weather; // likeliest first
                found.percent += chance.percent;
            }
            if (found.percent > 0) return found;
        }
        return std::nullopt;
    }

    int ZonesWithElement(Element element, uint32_t day)
    {
        int count = 0;
        for (uint16_t id = 1; const ZoneRecord* zone = ZoneById(id); ++id)
            if (const std::optional<WeatherOdds> odds = WeatherOn(*zone, day))
                count += WeatherElement(odds->normal) == element || WeatherElement(odds->common) == element ||
                         WeatherElement(odds->rare) == element;
        return count;
    }
}
