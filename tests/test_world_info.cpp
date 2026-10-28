#include "world_info.h"
#include "vanadiel.h"
#include "test.h"

#include <cstring>
#include <map>

using namespace headsup;

namespace
{
    constexpr double kVanaHour = 144.0, kVanaDay = 3456.0; // in Earth seconds
    // The start of a Vana'diel day, plus Earth seconds.
    int64_t At(uint32_t day, double seconds = 0.0)
    {
        return kVanaEpochUnixMs + static_cast<int64_t>((day * kVanaDay + seconds) * 1000.0);
    }
    const TravelRoute* Route(const char* name)
    {
        for (const TravelRoute& route : TravelRoutes())
            if (std::strcmp(route.name, name) == 0) return &route;
        return nullptr;
    }
}

TEST(a_ship_is_boarding_from_its_door_opening_until_it_leaves)
{
    const TravelRoute airship{"Airships", "Jeuno to San d'Oria", 864, 1, {{28, 182}}};
    // 100 s into a cycle: its door opened 72 s ago and it leaves in 82.
    const Sailing aboard = NextSailing(airship, At(0, 864.0 + 100.0));
    CHECK(aboard.boarding && aboard.boardIn == 0.0 && aboard.leaveIn == 82.0);
    // 200 s in it is gone, and the next one opens at 28 s into the next cycle.
    const Sailing gone = NextSailing(airship, At(0, 864.0 + 200.0));
    CHECK(!gone.boarding && gone.boardIn == 864.0 - 200.0 + 28.0 && gone.leaveIn == 864.0 - 200.0 + 182.0);
}

TEST(a_route_of_two_runs_sails_with_the_sooner)
{
    const TravelRoute barge{"Barge", "Central to South Landing", 3456, 2, {{678, 752}, {2790, 2864}}};
    CHECK_EQ(NextSailing(barge, At(0, 1000.0)).leaveIn, 2864.0 - 1000.0);
    CHECK_EQ(NextSailing(barge, At(0, 3000.0)).leaveIn, 3456.0 - 3000.0 + 752.0);
}

TEST(phoenixs_selbina_ferry_leaves_at_midnight_eight_and_sixteen)
{
    // Phoenix's run departs 2 Earth seconds into each 8-hour cycle (data/zones/selbina/zone.yaml).
    const TravelRoute* ferry = Route("Selbina to Mhaura");
    CHECK(ferry != nullptr);
    for (const double hour : {0.0, 8.0, 16.0})
        CHECK_EQ(NextSailing(*ferry, At(5, hour * kVanaHour)).leaveIn, 2.0);
    CHECK(Route("Jeuno to San d'Oria") != nullptr && Route("Bibiki Bay tour") != nullptr);
}

TEST(a_guild_is_open_in_its_hours_but_on_its_holiday)
{
    const GuildShop woodworking{"Woodworking", "Chaupire", "Northern San d'Oria", 6, 21, 0}; // Firesday off
    // Day 9 is an Earthsday: open at 10:00 until 21:00.
    const GuildHours ten = GuildAt(woodworking, At(9, 10 * kVanaHour));
    CHECK(ten.open && !ten.holiday && ten.changeIn == 11 * kVanaHour);
    // At 22:00 it opens at 06:00 the next day.
    const GuildHours late = GuildAt(woodworking, At(9, 22 * kVanaHour));
    CHECK(!late.open && !late.holiday && late.changeIn == 8 * kVanaHour);
    // On Firesday (day 16) it stays shut and opens at 06:00 on Earthsday.
    const GuildHours holiday = GuildAt(woodworking, At(16, 10 * kVanaHour));
    CHECK(!holiday.open && holiday.holiday && holiday.changeIn == 20 * kVanaHour);
    // Late on Darksday (day 15) the holiday comes next, so it opens the day after.
    CHECK_EQ(GuildAt(woodworking, At(15, 22 * kVanaHour)).changeIn, (2 + 24 + 6) * kVanaHour);
    // Before it opens on an open day: at 06:00 that day.
    CHECK_EQ(GuildAt(woodworking, At(9, 2 * kVanaHour)).changeIn, 4 * kVanaHour);
    CHECK(!GuildShops().empty());
}

TEST(the_rse_takes_a_race_and_a_place_each_vanadiel_week)
{
    // Phoenix's rse: the race is the week's number of the eight, the place of the three.
    const Rse first = RseAt(At(3));
    CHECK(std::strcmp(first.race, "Hume Male") == 0 && std::strcmp(first.place, "Ordelle's Caves") == 0);
    CHECK_EQ(first.endsIn, 5 * kVanaDay);
    const Rse fifth = RseAt(At(5 * 8 + 3));
    CHECK(std::strcmp(fifth.race, "Tarutaru Female") == 0 && std::strcmp(fifth.place, "Maze of Shakhrami") == 0);
    // A week ahead, and when that one ends.
    const Rse next = RseAt(At(5 * 8 + 3), 1);
    CHECK(std::strcmp(next.race, "Mithra") == 0 && std::strcmp(next.place, "Ordelle's Caves") == 0);
    CHECK_EQ(next.endsIn, (5 + 8) * kVanaDay);
}

TEST(the_next_full_and_new_moons_are_the_days_of_100_and_0_percent)
{
    const uint32_t full = NextMoonDay(0, true), fresh = NextMoonDay(0, false);
    CHECK(VanaAt(At(full)).moonPercent == 100 && VanaAt(At(fresh)).moonPercent == 0);
    CHECK(full == 46 && fresh == 4);
    // From a full moon, the next one is a cycle on.
    CHECK_EQ(NextMoonDay(full, true), full + 84);
    CHECK_EQ(SecondsToDay(At(3, 100.0), 4), kVanaDay - 100.0);
}

TEST(the_next_day_a_zone_can_roll_an_elements_weather)
{
    const ZoneRecord* zone = ZoneById(79); // Caedarva Mire: Water, Lightning and Dark weathers, and no others
    CHECK(zone != nullptr);
    const auto has = [&](uint32_t day, Element element) {
        const WeatherOdds odds = *WeatherOn(*zone, day);
        return WeatherElement(odds.normal) == element || WeatherElement(odds.common) == element ||
               WeatherElement(odds.rare) == element;
    };
    CHECK(NextElementWeather(*zone, Element::Water, 0).has_value() && !NextElementWeather(*zone, Element::Fire, 0));
    for (int e = 0; e < kElementCount; ++e)
    {
        const auto element = static_cast<Element>(e);
        const std::optional<ElementWeather> next = NextElementWeather(*zone, element, 0);
        if (!next)
        {
            // None in the whole cycle.
            for (uint32_t day = 0; day < kWeatherDays; ++day)
                CHECK(!has(day, element));
            continue;
        }
        // No day before it has one, and that day its weathers of the element add up to the chance.
        for (uint32_t day = 0; day < next->day; ++day)
        {
            const WeatherOdds odds = *WeatherOn(*zone, day);
            for (const Weather w : {odds.normal, odds.common, odds.rare})
                CHECK(WeatherElement(w) != element);
        }
        const WeatherOdds odds = *WeatherOn(*zone, next->day);
        int percent = 0;
        for (const auto [w, chance] : {std::pair{odds.normal, kNormalChance}, {odds.common, kCommonChance}, {odds.rare, kRareChance}})
            if (WeatherElement(w) == element) percent += chance;
        CHECK(next->percent == percent && WeatherElement(next->weather) == element);
    }
    CHECK(!NextElementWeather(ZoneRecord{"None", kUnknownRegion, kNoCalendar}, Element::Fire, 0));
}

TEST(the_zones_that_can_roll_an_element_come_likeliest_first_with_their_chance)
{
    for (const uint32_t day : {0u, 777u})
    {
        // Each zone's chance of Ice that day: its three weathers' shares added up.
        std::map<uint16_t, int> expected;
        for (uint16_t id = 1; const ZoneRecord* zone = ZoneById(id); ++id)
            if (const std::optional<WeatherOdds> odds = WeatherOn(*zone, day))
            {
                const int percent = (WeatherElement(odds->normal) == Element::Ice ? kNormalChance : 0) +
                                    (WeatherElement(odds->common) == Element::Ice ? kCommonChance : 0) +
                                    (WeatherElement(odds->rare) == Element::Ice ? kRareChance : 0);
                if (percent > 0) expected[id] = percent;
            }
        const std::vector<ZoneRoll> rolls = ZonesRolling(Element::Ice, day);
        CHECK_EQ(rolls.size(), expected.size());
        for (size_t i = 0; i < rolls.size(); ++i)
        {
            CHECK_EQ(rolls[i].percent, expected[rolls[i].zone]);
            // Its weathers of the element, likeliest first, add up to it.
            int sum = 0;
            for (size_t w = 0; w < rolls[i].weathers.size(); ++w)
            {
                CHECK(WeatherElement(rolls[i].weathers[w].weather) == Element::Ice);
                CHECK(w == 0 || rolls[i].weathers[w - 1].percent >= rolls[i].weathers[w].percent);
                sum += rolls[i].weathers[w].percent;
            }
            CHECK_EQ(sum, rolls[i].percent);
            if (i > 0) CHECK(rolls[i - 1].percent > rolls[i].percent ||
                             (rolls[i - 1].percent == rolls[i].percent && rolls[i - 1].zone < rolls[i].zone));
        }
    }
    CHECK_EQ(std::strcmp(ElementName(Element::Lightning), "Lightning"), 0);
}

TEST(the_high_quality_turn_ins_are_phoenixs_recipes_high_qualities)
{
    // Hellfire Axe and Sword are what Inferno Axe and Sword make at high quality (sql/synth_recipes.sql).
    CHECK(HighQualityTurnIn(16713) && HighQualityTurnIn(16928));
    CHECK(!HighQualityTurnIn(16709) && !HighQualityTurnIn(16594) && !HighQualityTurnIn(0));
}
