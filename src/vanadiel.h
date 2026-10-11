#pragma once

#include "generated/mobflags.h" // Element

#include <cstdint>

namespace headsup
{
    // Phoenix's clock (earth_time.h, vanadiel_clock.h): Vana'diel began at Unix 1009810800, as 886/01/01 00:00, a
    // Firesday, and runs 25 times faster than Earth.
    constexpr int64_t kVanaEpochUnixMs = 1009810800000LL;
    constexpr int kVanaDaysPerWeek     = 8;

    enum class MoonDirection : uint8_t
    {
        None, // exactly full or new
        Waning,
        Waxing,
    };

    struct VanaTime
    {
        uint32_t day = 0; // days since the epoch, as Phoenix's count_days has them
        int year = 0, month = 0, dayOfMonth = 0, hour = 0, minute = 0;
        int weekday = 0; // 0 Firesday to 7 Darksday
        int moonPercent = 0;
        MoonDirection moonDirection = MoonDirection::None;
    };

    VanaTime VanaAt(int64_t unixMs);
    const char* WeekdayName(int weekday);
    Element WeekdayElement(int weekday);
    // Phoenix's phase names (scripts/utils/common.lua with the names /time prints).
    const char* MoonPhaseName(int percent, MoonDirection direction);
    // Earth seconds until the next Vana'diel midnight.
    double SecondsToNextDay(int64_t unixMs);
    // Earth seconds until the moon's phase name next changes.
    double SecondsToNextPhase(int64_t unixMs);
}
