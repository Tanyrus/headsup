#include "vanadiel.h"

#include <algorithm>
#include <cstring>

namespace headsup
{
    namespace
    {
        constexpr int64_t kVanaSpeed  = 25;
        constexpr int64_t kVanaDayMs  = 86400000LL, kVanaHourMs = 3600000LL, kVanaMinuteMs = 60000LL; // Vana'diel ms
        constexpr double kMsPerSecond = 1000.0;
        constexpr double kEarthSecondsPerVanaDay = static_cast<double>(kVanaDayMs) / kVanaSpeed / kMsPerSecond;
        constexpr int kStartYear = 886, kDaysPerYear = 360, kDaysPerMonth = 30, kMonthsPerYear = 12, kHoursPerDay = 24;
        constexpr int kMinutesPerHour = 60;
        // Phoenix's get_phase: the cycle is 84 days, and day 0 of the epoch is day 38 of it (886 years plus 26 days).
        constexpr int kMoonCycle = 84, kMoonHalf = 42, kMoonOffset = 38;
        constexpr const char* kWeekdays[kVanaDaysPerWeek] = {"Firesday", "Earthsday", "Watersday", "Windsday", "Iceday",
            "Lightningday", "Lightsday", "Darksday"};
        constexpr Element kWeekdayElements[kVanaDaysPerWeek] = {Element::Fire, Element::Earth, Element::Water,
            Element::Wind, Element::Ice, Element::Lightning, Element::Light, Element::Dark};

        int64_t VanaMs(int64_t unixMs) { return std::max<int64_t>(0, unixMs - kVanaEpochUnixMs) * kVanaSpeed; }

        void MoonOn(uint32_t day, int& percent, MoonDirection& direction)
        {
            const int daysmod = static_cast<int>((static_cast<int64_t>(day) + kMoonOffset) % kMoonCycle);
            const double half = kMoonHalf;
            percent   = daysmod >= kMoonHalf ? static_cast<int>(100.0 * ((daysmod - kMoonHalf) / half) + 0.5)
                                             : static_cast<int>(100.0 * (1.0 - daysmod / half) + 0.5);
            direction = daysmod == 0 || daysmod == kMoonHalf ? MoonDirection::None
                        : daysmod < kMoonHalf                ? MoonDirection::Waning
                                                             : MoonDirection::Waxing;
        }
    }

    VanaTime VanaAt(int64_t unixMs)
    {
        const int64_t vana = VanaMs(unixMs);
        const int64_t days = vana / kVanaDayMs;
        VanaTime t;
        t.day        = static_cast<uint32_t>(days);
        t.year       = kStartYear + static_cast<int>(days / kDaysPerYear);
        t.month      = static_cast<int>((days / kDaysPerMonth) % kMonthsPerYear) + 1;
        t.dayOfMonth = static_cast<int>(days % kDaysPerMonth) + 1;
        t.hour       = static_cast<int>((vana / kVanaHourMs) % kHoursPerDay);
        t.minute     = static_cast<int>((vana / kVanaMinuteMs) % kMinutesPerHour);
        t.weekday    = static_cast<int>(days % kVanaDaysPerWeek);
        MoonOn(t.day, t.moonPercent, t.moonDirection);
        return t;
    }

    const char* WeekdayName(int weekday) { return kWeekdays[std::clamp(weekday, 0, kVanaDaysPerWeek - 1)]; }

    Element WeekdayElement(int weekday) { return kWeekdayElements[std::clamp(weekday, 0, kVanaDaysPerWeek - 1)]; }

    const char* MoonPhaseName(int percent, MoonDirection direction)
    {
        switch (direction)
        {
        case MoonDirection::None: return percent >= 100 ? "Full Moon" : "New Moon";
        case MoonDirection::Waning:
            return percent >= 94 ? "Full Moon" : percent >= 61 ? "Waning Gibbous" : percent >= 41 ? "Last Quarter"
                 : percent >= 11 ? "Waning Crescent" : "New Moon";
        case MoonDirection::Waxing:
            return percent >= 90 ? "Full Moon" : percent >= 56 ? "Waxing Gibbous" : percent >= 39 ? "First Quarter"
                 : percent >= 6 ? "Waxing Crescent" : "New Moon";
        }
        return "New Moon";
    }

    double SecondsToNextDay(int64_t unixMs)
    {
        const int64_t intoDay = VanaMs(unixMs) % kVanaDayMs;
        return static_cast<double>(kVanaDayMs - intoDay) / static_cast<double>(kVanaSpeed) / kMsPerSecond;
    }

    double SecondsToNextPhase(int64_t unixMs)
    {
        const VanaTime now = VanaAt(unixMs);
        const char* name   = MoonPhaseName(now.moonPercent, now.moonDirection);
        for (int ahead = 1; ahead <= kMoonCycle; ++ahead)
        {
            int percent = 0;
            MoonDirection direction{};
            MoonOn(now.day + static_cast<uint32_t>(ahead), percent, direction);
            if (std::strcmp(MoonPhaseName(percent, direction), name) != 0)
                return SecondsToNextDay(unixMs) + (ahead - 1) * kEarthSecondsPerVanaDay;
        }
        return 0.0;
    }
}
