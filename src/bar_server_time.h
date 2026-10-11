#pragma once

#include "bar_content.h"

#include <cstdint>
#include <string_view>

namespace headsup
{
    // Phoenix keeps Japan's time (earth_time.h's jst namespace): UTC+9, without daylight saving.
    constexpr int64_t kJstOffsetMs = 9 * kMsPerHour;

    struct JstTime
    {
        int weekday = 0; // 0 Monday to 6 Sunday, as Phoenix's get_game_weekday counts
        int hour = 0, minute = 0;
        bool operator==(const JstTime&) const = default;
    };
    JstTime JstAt(int64_t unixMs);
    const char* JstWeekdayName(int weekday);
    // Earth seconds to the next JST midnight, Phoenix's daily reset: more than 0, at most a day.
    double SecondsToDailyReset(int64_t unixMs);
    // Earth seconds to the next Monday 00:00 JST, Phoenix's conquest tally (get_next_game_week): more than 0, at most
    // a week.
    double SecondsToTally(int64_t unixMs);

    // The zones the Server time widget can show its clock in, with their daylight saving rules as of 2026.
    enum class ClockZone : uint8_t
    {
        Japan, // the server's, which the resets go by
        Utc,
        UsEastern,
        UsCentral,
        UsMountain,
        UsArizona,
        UsPacific,
        UsAlaska,
        UsHawaii,
        UkIreland,
        CentralEurope,
        EasternEurope,
        AustraliaEastern,
    };
    constexpr int kClockZoneCount = static_cast<int>(ClockZone::AustraliaEastern) + 1;
    // As the menu lists it: "Japan (JST)".
    const char* ClockZoneName(ClockZone zone);

    struct ZoneTime
    {
        int weekday = 0; // 0 Monday to 6 Sunday
        int hour = 0, minute = 0;
        const char* abbreviation = ""; // "JST", "EDT", "AEST"
        int second               = 0;  // left out of ==
        bool operator==(const ZoneTime& o) const
        {
            return weekday == o.weekday && hour == o.hour && minute == o.minute &&
                   std::string_view(abbreviation) == o.abbreviation;
        }
    };
    ZoneTime ZoneAt(ClockZone zone, int64_t unixMs);

    // The clock in zone; the popup gives the time in Japan too when zone is another, and the resets, which go by it.
    WidgetContent ServerTimeWidget(int64_t unixMs, ClockZone zone = ClockZone::Japan, bool twentyFour = true,
        bool seconds = false);
    PopupContent ServerTimePopup(int64_t unixMs, ClockZone zone = ClockZone::Japan);
}
