#include "ph_timers.h"
#include "test.h"

using namespace headsup;

namespace
{
    constexpr uint16_t kEastRonfaure  = 101;
    constexpr uint32_t kWorm          = 17191194; // a Carrion Worm, Bigmouth Billy's placeholder, on a 180 s respawn
    constexpr uint32_t kBigmouthBilly = 17191196;
    constexpr uint32_t kOtherWorm     = 17191195;
    constexpr uint32_t kLaTheineMob   = 17195258; // zone 102
    constexpr uint32_t kWildRabbit    = 17190918; // no lottery
    // From the defeat message to the placeholder's return: its despawn, less the second the message waits, then its
    // respawn.
    constexpr double kWormSpan = kDeathToDespawnSeconds - kDefeatMessageSeconds + 180.0;

    PhSighting Worm(uint32_t serverId = kWorm) { return PhSighting{serverId, 180, "Bigmouth Billy"}; }
    PhSighting Billy() { return PhSighting{kBigmouthBilly, 180, "Bigmouth Billy"}; }
}

TEST(a_placeholder_is_seen_with_its_respawn_and_its_nms_name)
{
    const MobRecord* worm = MobById(kWorm);
    const auto seen       = SightingOf(worm);
    CHECK(seen.has_value());
    CHECK(seen->serverId == kWorm && seen->respawnSeconds == 180 && seen->nmName == "Bigmouth Billy");
    CHECK(!SightingOf(MobById(kWildRabbit)).has_value()); // no lottery
    CHECK(!SightingOf(nullptr).has_value());
    MobRecord orphan     = *worm;
    orphan.placeholderOf = 1; // an NM the data does not have
    CHECK(SightingOf(&orphan)->nmName == "NM");
}

TEST(a_lottery_nm_is_seen_with_its_placeholders_respawn_and_its_own_name)
{
    const auto seen = SightingOf(MobById(kBigmouthBilly));
    CHECK(seen.has_value());
    CHECK(seen->serverId == kBigmouthBilly && seen->respawnSeconds == 180 && seen->nmName == "Bigmouth Billy");
    MobRecord untimed          = *MobById(kBigmouthBilly);
    untimed.placeholderRespawn = 0; // its placeholders are on no timer
    CHECK(SightingOf(&untimed).has_value() && SightingOf(&untimed)->respawnSeconds == 0);
}

TEST(a_placeholders_defeat_starts_its_timer)
{
    PhTimers t;
    t.Died(101.0, Worm());
    const std::vector<TimerLine> lines = t.Lines(kEastRonfaure, 101.0);
    CHECK_EQ(lines.size(), 1u);
    CHECK(lines[0].text == "Bigmouth Billy [11A] 3:14" && !lines[0].up); // despawn 14 s after the message, then 180 s
    CHECK(t.Lines(kEastRonfaure, 210.5)[0].text == "Bigmouth Billy [11A] 1:25");
}

TEST(an_nms_defeat_starts_its_placeholders_timer_under_the_nms_id)
{
    PhTimers t;
    t.Died(101.0, Billy());
    CHECK(t.Lines(kEastRonfaure, 101.0)[0].text == "Bigmouth Billy [11C] 3:14");
    CHECK_EQ(*t.SecondsLeft(kBigmouthBilly, 101.0), kWormSpan);
    CHECK(!t.SecondsLeft(kWorm, 101.0).has_value());
}

TEST(a_death_with_no_respawn_starts_nothing)
{
    PhTimers t; // a Dynamis statue's spawn: it never comes back on a timer
    t.Died(101.0, PhSighting{kWorm, 0, "Bigmouth Billy"});
    CHECK(t.Lines(kEastRonfaure, 101.0).empty());
}

TEST(a_mob_seen_alive_again_loses_its_timer)
{
    PhTimers t;
    t.Died(101.0, Worm());
    t.Update(250.0, {kOtherWorm});
    CHECK_EQ(t.Lines(kEastRonfaure, 250.0).size(), 1u);
    t.Update(251.0, {kWorm});
    CHECK(t.Lines(kEastRonfaure, 251.0).empty());
}

TEST(a_later_defeat_starts_the_timer_again)
{
    PhTimers t;
    t.Died(101.0, Worm());
    t.Died(400.0, Worm());
    CHECK_EQ(*t.SecondsLeft(kWorm, 400.0), kWormSpan);
}

TEST(a_timer_reads_up_when_due_then_goes)
{
    PhTimers t;
    t.Died(101.0, Worm());
    const double due = 101.0 + kWormSpan;
    t.Update(due, {});
    const std::vector<TimerLine> lines = t.Lines(kEastRonfaure, due);
    CHECK(lines.size() == 1 && lines[0].text == "Bigmouth Billy [11A] up" && lines[0].up);
    t.Update(due + kUpSeconds, {});
    CHECK_EQ(t.Lines(kEastRonfaure, due + kUpSeconds).size(), 1u);
    t.Update(due + kUpSeconds + 1.0, {});
    CHECK(t.Lines(kEastRonfaure, due + kUpSeconds + 1.0).empty());
}

TEST(only_this_zones_timers_show_soonest_first)
{
    PhTimers t;
    t.Died(101.0, Worm(kLaTheineMob));
    t.Died(110.0, Worm());
    t.Died(111.0, Worm(kOtherWorm));
    const std::vector<TimerLine> lines = t.Lines(kEastRonfaure, 120.0);
    CHECK_EQ(lines.size(), 2u);
    CHECK(lines[0].text == "Bigmouth Billy [11A] 3:04" && lines[1].text == "Bigmouth Billy [11B] 3:05");
}

TEST(a_countdown_reads_minutes_and_seconds_or_hours)
{
    CHECK(Countdown(195.0) == "3:15");
    CHECK(Countdown(7.0) == "0:07");
    CHECK(Countdown(0.2) == "0:01"); // never 0:00 before it is due
    CHECK(Countdown(3909.0) == "1:05:09");
    CHECK(Countdown(299.0) == "4:59" && Countdown(2160.0) == "36:00" && Countdown(36720.0) == "10:12:00");
}

TEST(a_placeholders_seconds_left_count_down_to_zero)
{
    PhTimers timers;
    timers.Died(110.0, PhSighting{0x1001ABC, 300, "Some NM"});
    CHECK(!timers.SecondsLeft(0x1001DEF, 110.0).has_value());
    CHECK_EQ(*timers.SecondsLeft(0x1001ABC, 110.0), kDeathToDespawnSeconds - kDefeatMessageSeconds + 300.0);
    CHECK_EQ(*timers.SecondsLeft(0x1001ABC, 110.0 + kDeathToDespawnSeconds + 400.0), 0.0);
}
