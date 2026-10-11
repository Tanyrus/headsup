#include "weather.h"

#include <algorithm>
#include <utility>

namespace headsup
{
    namespace
    {
        const char* const kNames[kWeatherCount] = {"Clear", "Sunshine", "Clouds", "Fog", "Hot Spell", "Heat Wave",
            "Rain", "Squall", "Dust Storm", "Sand Storm", "Wind", "Gales", "Snow", "Blizzards", "Thunder",
            "Thunderstorms", "Auroras", "Stellar Glare", "Gloom", "Darkness"};
        // From Hot Spell on, the weathers pair up by element, the single one first (zoneutils.cpp GetWeatherElement).
        constexpr Element kPairElements[] = {Element::Fire, Element::Water, Element::Earth, Element::Wind, Element::Ice,
            Element::Lightning, Element::Light, Element::Dark};
        constexpr int kFirstElemental = static_cast<int>(Weather::HotSpell);
        constexpr uint16_t kFiveBits  = 0x1F;
        constexpr int kCommonShift = 5, kNormalShift = 10;

        Weather Decode(uint32_t bits) { return WeatherFromId(bits & kFiveBits).value_or(Weather::None); }
    }

    std::optional<Weather> WeatherFromId(uint32_t id)
    {
        if (id >= static_cast<uint32_t>(kWeatherCount)) return std::nullopt;
        return static_cast<Weather>(id);
    }

    const char* WeatherName(Weather weather)
    {
        return kNames[std::clamp(static_cast<int>(weather), 0, kWeatherCount - 1)];
    }

    std::optional<Element> WeatherElement(Weather weather)
    {
        const int id = static_cast<int>(weather);
        if (id < kFirstElemental || id >= kWeatherCount) return std::nullopt;
        return kPairElements[(id - kFirstElemental) / 2];
    }

    bool IsDoubleWeather(Weather weather)
    {
        const int id = static_cast<int>(weather);
        return id >= kFirstElemental && id < kWeatherCount && (id - kFirstElemental) % 2 == 1;
    }

    std::optional<WeatherOdds> WeatherOn(const ZoneRecord& zone, uint32_t vanaDay)
    {
        const std::optional<uint16_t> packed = PackedWeather(zone, vanaDay);
        if (!packed) return std::nullopt;
        return WeatherOdds{Decode(*packed >> kNormalShift), Decode(*packed >> kCommonShift), Decode(*packed)};
    }

    WeatherChances Chances(const WeatherOdds& odds)
    {
        WeatherChances out;
        for (const WeatherChance& c : {WeatherChance{odds.normal, kNormalChance},
                 WeatherChance{odds.common, kCommonChance}, WeatherChance{odds.rare, kRareChance}})
        {
            WeatherChance* const end  = out.chances + out.count;
            WeatherChance* const same = std::ranges::find(out.chances, end, c.weather, &WeatherChance::weather);
            if (same != end)
                same->percent += c.percent;
            else
                out.chances[out.count++] = c;
        }
        // An insertion sort: stable, and no buffer to allocate as std::stable_sort's.
        for (int i = 1; i < out.count; ++i)
            for (int j = i; j > 0 && out.chances[j - 1].percent < out.chances[j].percent; --j)
                std::swap(out.chances[j - 1], out.chances[j]);
        return out;
    }
}
