#include "ph_timers.h"
#include "test.h"

using namespace headsup;

namespace
{
    constexpr uint16_t kEastRonfaure = 101;
    constexpr uint32_t kWorm         = 17191194; // a Carrion Worm, Bigmouth Billy's placeholder, on a 180 s respawn
    constexpr uint32_t kOtherWorm    = 17191195;
    constexpr uint32_t kLaTheineMob  = 17195258; // zone 102

    PhSighting Worm(bool alive, bool onScreen = true, uint32_t serverId = kWorm)
    {
        return PhSighting{serverId, 180, "Bigmouth Billy", alive, onScreen};
    }
}

TEST(a_placeholder_you_see_die_gets_its_respawn_timer)
{
    PhTimers t;
    t.Update(100.0, {Worm(true)});
    t.Update(101.0, {Worm(false)});
    const std::vector<TimerLine> lines = t.Lines(kEastRonfaure, 101.0);
    CHECK_EQ(lines.size(), 1u);
    CHECK(lines[0].text == "Bigmouth Billy [11A] 3:15" && !lines[0].up); // 15 s to its despawn, then its 180 s
    t.Update(110.0, {Worm(false)}); // its body still in view
    CHECK(t.Lines(kEastRonfaure, 210.5)[0].text == "Bigmouth Billy [11A] 1:26");
}

TEST(a_placeholder_you_did_not_see_die_gets_none)
{
    PhTimers offScreen;
    offScreen.Update(100.0, {Worm(true, false)});
    offScreen.Update(101.0, {Worm(false, false)});
    CHECK(offScreen.Lines(kEastRonfaure, 101.0).empty());

    PhTimers alreadyDead; // came into range as a corpse
    alreadyDead.Update(100.0, {Worm(false)});
    alreadyDead.Update(101.0, {Worm(false)});
    CHECK(alreadyDead.Lines(kEastRonfaure, 101.0).empty());

    PhTimers noRespawn; // a Dynamis statue's spawn: it never comes back on a timer
    noRespawn.Update(100.0, {PhSighting{kWorm, 0, "Bigmouth Billy", true, true}});
    noRespawn.Update(101.0, {PhSighting{kWorm, 0, "Bigmouth Billy", false, true}});
    CHECK(noRespawn.Lines(kEastRonfaure, 101.0).empty());

    PhTimers away; // left range alive and came back to its corpse
    away.Update(100.0, {Worm(true)});
    away.Update(101.0, {});
    away.Update(102.0, {Worm(false)});
    CHECK(away.Lines(kEastRonfaure, 102.0).empty());
}

TEST(a_timer_reads_up_when_due_then_goes)
{
    PhTimers t;
    t.Update(100.0, {Worm(true)});
    t.Update(101.0, {Worm(false)});
    t.Update(296.0, {});
    const std::vector<TimerLine> due = t.Lines(kEastRonfaure, 296.0);
    CHECK(due.size() == 1 && due[0].text == "Bigmouth Billy [11A] up" && due[0].up);
    t.Update(296.0 + kUpSeconds, {});
    CHECK_EQ(t.Lines(kEastRonfaure, 296.0 + kUpSeconds).size(), 1u);
    t.Update(296.0 + kUpSeconds + 1.0, {});
    CHECK(t.Lines(kEastRonfaure, 296.0 + kUpSeconds + 1.0).empty());
}

TEST(a_placeholder_seen_alive_again_loses_its_timer)
{
    PhTimers t;
    t.Update(100.0, {Worm(true)});
    t.Update(101.0, {Worm(false)});
    t.Update(250.0, {Worm(true)});
    CHECK(t.Lines(kEastRonfaure, 250.0).empty());
}

TEST(only_this_zones_timers_show_soonest_first)
{
    PhTimers t;
    t.Update(100.0, {Worm(true), Worm(true, true, kLaTheineMob)});
    t.Update(101.0, {Worm(true), Worm(false, true, kLaTheineMob)});
    t.Update(110.0, {Worm(false), Worm(true, true, kOtherWorm)});
    t.Update(111.0, {Worm(false, true, kOtherWorm)});
    const std::vector<TimerLine> lines = t.Lines(kEastRonfaure, 120.0);
    CHECK_EQ(lines.size(), 2u);
    CHECK(lines[0].text == "Bigmouth Billy [11A] 3:05" && lines[1].text == "Bigmouth Billy [11B] 3:06");
}

TEST(a_countdown_reads_minutes_and_seconds_or_hours)
{
    CHECK(Countdown(195.0) == "3:15");
    CHECK(Countdown(7.0) == "0:07");
    CHECK(Countdown(0.2) == "0:01"); // never 0:00 before it is due
    CHECK(Countdown(3909.0) == "1:05:09");
}
