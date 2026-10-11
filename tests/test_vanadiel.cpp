#include "test.h"
#include "vanadiel.h"

#include <string>

using namespace headsup;

namespace
{
    constexpr double kDaySeconds = 3456.0; // Earth seconds in a Vana'diel day: 24 hours of 144 s (the facts)
    constexpr int64_t kDayMs     = 3456000;

    // The Earth time a quarter into the epoch's day whose place in the moon's cycle is daysmod: Phoenix's daysmod is
    // (day + 38) % 84 (the facts).
    int64_t OnCycleDay(int daysmod) { return kVanaEpochUnixMs + ((daysmod - 38 + 84) % 84) * kDayMs + kDayMs / 4; }
}

TEST(vanadiel_time_follows_phoenixs_clock)
{
    const VanaTime t = VanaAt(1791417600000LL); // 2026-10-08 00:00 UTC
    CHECK_EQ(t.day, 226159u);
    CHECK(t.year == 1514 && t.month == 3 && t.dayOfMonth == 20 && t.hour == 9 && t.minute == 0);
    CHECK_EQ(t.weekday, 7);
    CHECK(std::string(WeekdayName(t.weekday)) == "Darksday");
    CHECK(WeekdayElement(t.weekday) == Element::Dark);
    CHECK(t.moonPercent == 64 && t.moonDirection == MoonDirection::Waxing);
    const VanaTime later = VanaAt(1800000000000LL); // 2027-01-15 08:00 UTC
    CHECK(later.year == 1521 && later.month == 2 && later.dayOfMonth == 13 && later.hour == 17 && later.minute == 0);
    CHECK(std::string(WeekdayName(later.weekday)) == "Watersday");
    CHECK(later.moonPercent == 24 && later.moonDirection == MoonDirection::Waning);
}

TEST(the_epoch_is_886_01_01_firesday)
{
    const VanaTime t = VanaAt(kVanaEpochUnixMs);
    CHECK(t.year == 886 && t.month == 1 && t.dayOfMonth == 1 && t.hour == 0 && t.weekday == 0);
    CHECK(WeekdayElement(0) == Element::Fire && WeekdayElement(1) == Element::Earth &&
          WeekdayElement(4) == Element::Ice);
}

TEST(the_moon_is_named_as_phoenix_names_it)
{
    CHECK(std::string(MoonPhaseName(64, MoonDirection::Waxing)) == "Waxing Gibbous");
    CHECK(std::string(MoonPhaseName(24, MoonDirection::Waning)) == "Waning Crescent");
    CHECK(std::string(MoonPhaseName(90, MoonDirection::Waxing)) == "Full Moon");
    CHECK(std::string(MoonPhaseName(93, MoonDirection::Waning)) == "Waning Gibbous");
    CHECK(std::string(MoonPhaseName(41, MoonDirection::Waning)) == "Last Quarter");
    CHECK(std::string(MoonPhaseName(5, MoonDirection::Waxing)) == "New Moon");
    CHECK(std::string(MoonPhaseName(100, MoonDirection::None)) == "Full Moon");
    CHECK(std::string(MoonPhaseName(0, MoonDirection::None)) == "New Moon");
}

TEST(every_day_of_the_moons_cycle_has_the_name_phoenix_gives_it)
{
    // Each name's days of the cycle, from the facts' phase formula and common.lua's thresholds: the first row a
    // phase reaches, waning 94 Full, 61 Waning Gibbous, 41 Last Quarter, 11 Waning Crescent, then New; waxing 90
    // Full, 56 Waxing Gibbous, 39 First Quarter, 6 Waxing Crescent, then New; on days 0 and 42 neither.
    struct Phase
    {
        int first, last;
        const char* name;
    };
    constexpr Phase kPhases[] = {{0, 2, "Full Moon"}, {3, 16, "Waning Gibbous"}, {17, 24, "Last Quarter"},
        {25, 37, "Waning Crescent"}, {38, 44, "New Moon"}, {45, 58, "Waxing Crescent"}, {59, 65, "First Quarter"},
        {66, 79, "Waxing Gibbous"}, {80, 83, "Full Moon"}};
    int days = 0;
    for (const Phase& phase : kPhases)
        for (int daysmod = phase.first; daysmod <= phase.last; ++daysmod, ++days)
        {
            const VanaTime t = VanaAt(OnCycleDay(daysmod));
            CHECK_EQ(MoonPhaseName(t.moonPercent, t.moonDirection), phase.name);
        }
    CHECK_EQ(days, 84);
}

TEST(the_next_phase_from_an_exactly_full_or_new_moon)
{
    // Day 0 is full and day 42 new with neither direction; each name holds two more days (the table above).
    const double intoDay = kDaySeconds / 4.0;
    CHECK_EQ(SecondsToNextPhase(OnCycleDay(0)), 3.0 * kDaySeconds - intoDay);
    CHECK_EQ(SecondsToNextPhase(OnCycleDay(42)), 3.0 * kDaySeconds - intoDay);
}

TEST(earth_seconds_to_the_next_day_and_the_next_phase)
{
    CHECK_EQ(SecondsToNextDay(kVanaEpochUnixMs), kDaySeconds);
    // 09:00 leaves 15 Vana'diel hours (2160 s); the 64% waxing gibbous turns full eleven days later, at 90%.
    CHECK_EQ(SecondsToNextDay(1791417600000LL), 2160.0);
    CHECK_EQ(SecondsToNextPhase(1791417600000LL), 2160.0 + 10 * kDaySeconds);
}
