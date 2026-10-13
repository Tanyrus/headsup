#include "bar_server_time.h"
#include "test.h"

using namespace headsup;

namespace
{
    constexpr int64_t kFriday1530Jst = 1791527400000; // 2026-10-09 06:30 UTC
    constexpr int64_t kSunday2300Jst = 1791727200000; // 2026-10-11 14:00 UTC
    constexpr int64_t kMonday0000Jst = 1791730800000; // 2026-10-11 15:00 UTC
    constexpr double kHour = 3600.0, kDay = 86400.0;
}

TEST(japan_time_is_nine_hours_ahead_of_utc_with_weeks_from_monday)
{
    CHECK(JstAt(kFriday1530Jst) == (JstTime{4, 15, 30}));
    CHECK(JstAt(kSunday2300Jst) == (JstTime{6, 23, 0}));
    CHECK(JstAt(kMonday0000Jst) == (JstTime{0, 0, 0}));
    CHECK(JstAt(0) == (JstTime{3, 9, 0})); // 1970-01-01 09:00 JST, a Thursday
    CHECK_EQ(JstWeekdayName(4), "Friday");
}

TEST(the_daily_reset_is_the_next_midnight_in_japan)
{
    CHECK_EQ(SecondsToDailyReset(kFriday1530Jst), 8.5 * kHour);
    CHECK_EQ(SecondsToDailyReset(kSunday2300Jst), kHour);
    CHECK_EQ(SecondsToDailyReset(kMonday0000Jst), kDay); // at midnight itself, the next one
}

TEST(the_tally_is_the_next_monday_midnight_in_japan)
{
    CHECK_EQ(SecondsToTally(kFriday1530Jst), 2 * kDay + 8.5 * kHour);
    CHECK_EQ(SecondsToTally(kSunday2300Jst), kHour);
    CHECK_EQ(SecondsToTally(kMonday0000Jst), 7 * kDay); // the tally just ran: the next is a week away
    CHECK_EQ(SecondsToTally(kMonday0000Jst + 10 * 3600000), 6 * kDay + 14 * kHour);
}

TEST(a_zones_time_carries_its_own_date)
{
    const ZoneTime pacific = ZoneAt(ClockZone::UsPacific, kFriday1530Jst); // Thursday 23:30 there
    CHECK(pacific.year == 2026 && pacific.month == 10 && pacific.day == 8);
    const ZoneTime japan = ZoneAt(ClockZone::Japan, kFriday1530Jst);
    CHECK(japan.year == 2026 && japan.month == 10 && japan.day == 9);
    constexpr int64_t kNewYearsEve = 1798747200000; // 2026-12-31 20:00 UTC: 05:00 on 1 January in Japan
    const ZoneTime first = ZoneAt(ClockZone::Japan, kNewYearsEve), last = ZoneAt(ClockZone::UsHawaii, kNewYearsEve);
    CHECK(first.year == 2027 && first.month == 1 && first.day == 1);
    CHECK(last.year == 2026 && last.month == 12 && last.day == 31);
    constexpr int64_t kLeapDay = 1835438400000; // 2028-02-29 12:00 UTC
    const ZoneTime leap = ZoneAt(ClockZone::UsHawaii, kLeapDay);
    CHECK(leap.year == 2028 && leap.month == 2 && leap.day == 29 && leap.hour == 2);
}

TEST(a_clock_zone_follows_its_daylight_saving_rule)
{
    // 2026: the US changes on 8 March and 1 November, Europe on 29 March and 25 October, Sydney on 5 April and
    // 4 October.
    constexpr int64_t kMarch7 = 1772884800000;  // 2026-03-07 12:00 UTC
    constexpr int64_t kMarch9 = 1773057600000;  // 2026-03-09 12:00 UTC
    CHECK(ZoneAt(ClockZone::UsEastern, kMarch7) == (ZoneTime{5, 7, 0, "EST"}));
    CHECK(ZoneAt(ClockZone::UsEastern, kMarch9) == (ZoneTime{0, 8, 0, "EDT"}));
    CHECK(ZoneAt(ClockZone::UsArizona, kMarch9) == (ZoneTime{0, 5, 0, "MST"})); // no daylight saving
    CHECK(ZoneAt(ClockZone::UkIreland, kMarch9) == (ZoneTime{0, 12, 0, "GMT"}));  // Europe changes later
    CHECK(ZoneAt(ClockZone::AustraliaEastern, kMarch9) == (ZoneTime{0, 23, 0, "AEDT"})); // Sydney's summer
    CHECK(ZoneAt(ClockZone::CentralEurope, kFriday1530Jst) == (ZoneTime{4, 8, 30, "CEST"}));
    CHECK(ZoneAt(ClockZone::AustraliaEastern, kFriday1530Jst) == (ZoneTime{4, 17, 30, "AEDT"})); // from 4 October
    CHECK(ZoneAt(ClockZone::Japan, kFriday1530Jst) == (ZoneTime{4, 15, 30, "JST"}));
    // The changes themselves: Europe's at 01:00 UTC on 29 March, the US's at 02:00 local on 1 November.
    constexpr int64_t kEuropeChange = 1774746000000; // 2026-03-29 01:00 UTC
    CHECK(ZoneAt(ClockZone::UkIreland, kEuropeChange - 1).abbreviation == std::string_view("GMT"));
    CHECK(ZoneAt(ClockZone::UkIreland, kEuropeChange) == (ZoneTime{6, 2, 0, "BST"}));
    constexpr int64_t kUsFallBack = 1793512800000; // 2026-11-01 06:00 UTC, 02:00 EDT
    CHECK(ZoneAt(ClockZone::UsEastern, kUsFallBack - 1) == (ZoneTime{6, 1, 59, "EDT"}));
    CHECK(ZoneAt(ClockZone::UsEastern, kUsFallBack) == (ZoneTime{6, 1, 0, "EST"}));
}
