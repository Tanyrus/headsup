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
    // A death the player had earned the timer of while it lived.
    void Fought(PhTimers& timers, double at, const PhSighting& mob)
    {
        timers.Earned(mob.serverId);
        timers.Died(at, mob);
    }
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
    Fought(t, 101.0, Worm());
    const std::vector<TimerLine> lines = t.Lines(kEastRonfaure, 101.0);
    CHECK_EQ(lines.size(), 1u);
    CHECK(lines[0].text == "Bigmouth Billy [11A] 3:14" && !lines[0].up); // despawn 14 s after the message, then 180 s
    CHECK(t.Lines(kEastRonfaure, 210.5)[0].text == "Bigmouth Billy [11A] 1:25");
}

TEST(an_nms_defeat_starts_its_placeholders_timer_under_the_nms_id)
{
    PhTimers t;
    Fought(t, 101.0, Billy());
    CHECK(t.Lines(kEastRonfaure, 101.0)[0].text == "Bigmouth Billy [11C] 3:14");
    CHECK_EQ(*t.SecondsLeft(kBigmouthBilly, 101.0), kWormSpan);
    CHECK(!t.SecondsLeft(kWorm, 101.0).has_value());
}

TEST(a_death_with_no_respawn_starts_nothing)
{
    PhTimers t; // a Dynamis statue's spawn: it never comes back on a timer
    Fought(t, 101.0, PhSighting{kWorm, 0, "Bigmouth Billy"});
    CHECK(t.Lines(kEastRonfaure, 101.0).empty());
}

TEST(a_mob_seen_alive_again_loses_its_timer)
{
    PhTimers t;
    Fought(t, 101.0, Worm());
    t.Update(250.0, {kOtherWorm});
    CHECK_EQ(t.Lines(kEastRonfaure, 250.0).size(), 1u);
    t.Update(251.0, {kWorm});
    CHECK(t.Lines(kEastRonfaure, 251.0).empty());
}

TEST(a_later_defeat_starts_the_timer_again)
{
    PhTimers t;
    Fought(t, 101.0, Worm());
    Fought(t, 400.0, Worm());
    CHECK_EQ(*t.SecondsLeft(kWorm, 400.0), kWormSpan);
}

TEST(a_timer_reads_up_when_due_then_goes)
{
    PhTimers t;
    Fought(t, 101.0, Worm());
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
    Fought(t, 101.0, Worm(kLaTheineMob));
    Fought(t, 110.0, Worm());
    Fought(t, 111.0, Worm(kOtherWorm));
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
    Fought(timers, 110.0, PhSighting{0x1001ABC, 300, "Some NM"});
    CHECK(!timers.SecondsLeft(0x1001DEF, 110.0).has_value());
    CHECK_EQ(*timers.SecondsLeft(0x1001ABC, 110.0), kDeathToDespawnSeconds - kDefeatMessageSeconds + 300.0);
    CHECK_EQ(*timers.SecondsLeft(0x1001ABC, 110.0 + kDeathToDespawnSeconds + 400.0), 0.0);
}

TEST(a_death_starts_a_timer_only_once_it_was_earned_while_the_mob_lived)
{
    PhTimers t;
    t.Died(101.0, Worm()); // in range of the defeat message, and nothing more
    CHECK(t.Lines(kEastRonfaure, 101.0).empty());
    t.Earned(kWorm);
    t.Died(400.0, Worm());
    CHECK_EQ(*t.SecondsLeft(kWorm, 400.0), kWormSpan);
    // Earned once is for that life: its next death starts nothing unless earned again.
    t.Update(700.0, {kWorm});
    t.Died(800.0, Worm());
    CHECK(!t.SecondsLeft(kWorm, 800.0).has_value());
    // A zone change forgets what was earned there.
    t.Earned(kOtherWorm);
    t.Zoned();
    t.Died(900.0, Worm(kOtherWorm));
    CHECK(!t.SecondsLeft(kOtherWorm, 900.0).has_value());
}

TEST(a_timer_is_earned_by_targeting_or_by_being_active_with_it_on_your_screen)
{
    CHECK(EarnsTimer(true, false, false)); // targeting counts even standing still
    CHECK(EarnsTimer(false, true, true));
    CHECK(!EarnsTimer(false, true, false)); // away from the keyboard
    // In range and active, but not seen: a party member's claim off your screen earns you nothing (the user, 2026-10-26).
    CHECK(!EarnsTimer(false, false, true));
}

TEST(you_are_active_until_you_have_not_moved_or_turned_for_two_and_a_half_minutes)
{
    Activity a;
    CHECK(!a.Active(10.0)); // never seen moving
    a.Pose(10.0, WorldPoint{1.0f, 2.0f, 3.0f}, 0.5f);
    a.Pose(20.0, WorldPoint{1.0f, 2.0f, 3.0f}, 0.5f); // the first pose is where you stand, not a move
    CHECK(!a.Active(20.0));
    a.Pose(30.0, WorldPoint{1.5f, 2.0f, 3.0f}, 0.5f);
    CHECK(a.Active(30.0) && a.Active(30.0 + kIdleSeconds));
    CHECK(!a.Active(30.0 + kIdleSeconds + 1.0));
    a.Pose(400.0, WorldPoint{1.5f, 2.0f, 3.0f}, 1.0f); // turning on the spot counts
    CHECK(a.Active(400.0));
}
