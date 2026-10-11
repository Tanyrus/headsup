#pragma once

#include "generated/mobflags.h" // Element
#include "zones.h"

#include <cstdint>
#include <optional>

namespace headsup
{
    // Phoenix's xi::Weather (data/enums/weather.yaml).
    enum class Weather : uint8_t
    {
        None, Sunshine, Clouds, Fog, HotSpell, HeatWave, Rain, Squall, DustStorm, SandStorm, Wind, Gales, Snow,
        Blizzards, Thunder, Thunderstorms, Auroras, StellarGlare, Gloom, Darkness,
    };
    constexpr int kWeatherCount = static_cast<int>(Weather::Darkness) + 1;
    // Phoenix rolls the day's weather from three: the normal one half the time, the common 35%, the rare 15%, and
    // rolls again every 3 to 30 Earth minutes.
    constexpr int kNormalChance = 50, kCommonChance = 35, kRareChance = 15;
    constexpr int kDayWeathers  = 3;

    struct WeatherOdds
    {
        Weather normal = Weather::None, common = Weather::None, rare = Weather::None;
        bool operator==(const WeatherOdds&) const = default;
    };
    struct WeatherChance
    {
        Weather weather = Weather::None;
        int percent     = 0;
        bool operator==(const WeatherChance&) const = default;
    };

    std::optional<Weather> WeatherFromId(uint32_t id);
    const char* WeatherName(Weather weather);
    std::optional<Element> WeatherElement(Weather weather);
    bool IsDoubleWeather(Weather weather);
    // The day's three weathers in a zone, as Phoenix's entryForDay reads its calendar; nullopt for a zone without one.
    std::optional<WeatherOdds> WeatherOn(const ZoneRecord& zone, uint32_t vanaDay);
    struct WeatherChances
    {
        WeatherChance chances[kDayWeathers] = {};
        int count = 0;
        const WeatherChance* begin() const { return chances; }
        const WeatherChance* end() const { return chances + count; }
    };
    // Each distinct weather once with its chances added up, likeliest first (normal before common before rare on a
    // tie).
    WeatherChances Chances(const WeatherOdds& odds);
}
