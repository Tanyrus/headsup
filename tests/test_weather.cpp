#include "test.h"
#include "weather.h"
#include "zones.h"

#include <string>
#include <vector>

using namespace headsup;

TEST(weather_names_elements_and_doubles)
{
    CHECK(std::string(WeatherName(Weather::HotSpell)) == "Hot Spell" &&
          std::string(WeatherName(Weather::StellarGlare)) == "Stellar Glare");
    CHECK(std::string(WeatherName(Weather::None)) == "Clear");
    CHECK(WeatherElement(Weather::Rain) == Element::Water &&
          WeatherElement(Weather::Thunderstorms) == Element::Lightning);
    CHECK(!WeatherElement(Weather::Fog).has_value() && !WeatherElement(Weather::Sunshine).has_value());
    CHECK(IsDoubleWeather(Weather::HeatWave) && IsDoubleWeather(Weather::Darkness) && !IsDoubleWeather(Weather::Gloom));
    CHECK(WeatherFromId(14) == Weather::Thunder && !WeatherFromId(20).has_value());
}

TEST(values_past_the_last_weather_are_neither_elemental_nor_double)
{
    const auto past = static_cast<Weather>(kWeatherCount + 1);
    CHECK(!IsDoubleWeather(past) && !WeatherElement(past).has_value());
}

TEST(a_zones_day_reads_its_calendar_and_blank_days_carry_the_last)
{
    // Valkurm Dunes, day 0: 0x0508, normal sunshine, common and rare dust storm; day 1 is blank.
    const ZoneRecord& dunes = *ZoneById(103);
    const WeatherOdds day0{Weather::Sunshine, Weather::DustStorm, Weather::DustStorm};
    CHECK(WeatherOn(dunes, 0) == day0);
    CHECK(WeatherOn(dunes, 1) == day0);
    CHECK(WeatherOn(dunes, kWeatherDays) == day0); // the cycle wraps
    CHECK(!WeatherOn(ZoneRecord{"Nowhere", kUnknownRegion, kNoCalendar}, 0).has_value());
}

TEST(a_calendar_with_one_set_day_keeps_it_to_the_end_of_its_cycle)
{
    // Apollyon's calendar sets only day 0, 0x4E73: darkness as the normal, common and rare weather.
    const ZoneRecord& apollyon = *ZoneById(38);
    const WeatherOdds dark{Weather::Darkness, Weather::Darkness, Weather::Darkness};
    CHECK(WeatherOn(apollyon, 0) == dark);
    CHECK(WeatherOn(apollyon, kWeatherDays - 1) == dark);
}

namespace
{
    std::vector<WeatherChance> Listed(const WeatherChances& chances) { return {chances.begin(), chances.end()}; }
}

TEST(chances_add_up_alike_weathers_likeliest_first)
{
    CHECK(Listed(Chances({Weather::Sunshine, Weather::DustStorm, Weather::DustStorm})) ==
          (std::vector<WeatherChance>{{Weather::Sunshine, 50}, {Weather::DustStorm, 50}}));
    CHECK(Listed(Chances({Weather::Rain, Weather::Clouds, Weather::Squall})) ==
          (std::vector<WeatherChance>{{Weather::Rain, 50}, {Weather::Clouds, 35}, {Weather::Squall, 15}}));
    CHECK(Listed(Chances({Weather::Rain, Weather::Clouds, Weather::Rain})) ==
          (std::vector<WeatherChance>{{Weather::Rain, 65}, {Weather::Clouds, 35}}));
    CHECK(Listed(Chances({Weather::Fog, Weather::Fog, Weather::Fog})) ==
          (std::vector<WeatherChance>{{Weather::Fog, 100}}));
}
