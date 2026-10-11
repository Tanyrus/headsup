#include "bar_signet.h"
#include "player_status.h"
#include "test.h"
#include "vanadiel.h"
#include "zones.h"

#include <array>

using namespace headsup;

namespace
{
    constexpr int64_t kNow = 1791527400000; // 2026-10-09 06:30 UTC

    // A timer running out seconds after kNow, as the game holds it: 60ths of a second from XIUI's VANA_BASE, which
    // passed 32 bits long before kNow.
    uint32_t TimerFor(double seconds)
    {
        const int64_t ticks = (kNow - kVanaEpochUnixMs) / 1000 * 60;
        return static_cast<uint32_t>(ticks + static_cast<int64_t>(seconds * 60));
    }

    struct Slots
    {
        std::array<int16_t, kStatusIconSlots> buffs{}, icons{};
        std::array<uint32_t, kStatusIconSlots> timers{};
        Slots()
        {
            buffs.fill(-1);
            icons.fill(-1);
        }
        std::optional<SignetState> Read() const { return SignetFrom(buffs.data(), icons.data(), timers.data(), kNow); }
    };
}

TEST(a_status_timer_counts_sixtieths_of_a_second_from_the_vana_diel_epoch_and_wraps_at_32_bits)
{
    CHECK_EQ(*StatusSecondsLeft(TimerFor(7200.0), kNow), 7200.0);
    CHECK_EQ(*StatusSecondsLeft(TimerFor(7200.0), kNow + 1000), 7199.0);
    CHECK_EQ(*StatusSecondsLeft(TimerFor(-1.0), kNow), 0.0); // ran out a second ago
    CHECK(!StatusSecondsLeft(kNoExpiry, kNow).has_value());
}

TEST(signet_or_sanction_is_found_in_the_buffs_and_timed_from_the_icon_slot_that_holds_it)
{
    Slots s;
    CHECK(!s.Read().has_value());
    s.buffs[0] = s.icons[0] = 40; // Protect
    s.buffs[1] = s.icons[1] = kSignetEffect;
    s.timers[1] = TimerFor(3 * 3600.0);
    CHECK(s.Read() == (SignetState{Influence::Signet, 3 * 3600.0}));
    s.buffs[1] = s.icons[1] = kSanctionEffect;
    CHECK(s.Read()->which == Influence::Sanction);
}

TEST(a_signet_whose_timer_cannot_be_read_shows_without_a_time)
{
    Slots s;
    s.buffs[0] = kSignetEffect;
    CHECK(s.Read() == (SignetState{Influence::Signet, std::nullopt})); // no icon slot holds it
    s.icons[3]  = kSignetEffect;
    s.timers[3] = TimerFor(30 * 3600.0); // longer than any Signet lasts: a misread
    CHECK(!s.Read()->secondsLeft.has_value());
    CHECK(SignetFrom(s.buffs.data(), nullptr, nullptr, kNow)->which == Influence::Signet);
    CHECK(!SignetFrom(nullptr, s.icons.data(), s.timers.data(), kNow).has_value());
}

TEST(each_works_only_in_the_regions_phoenix_names)
{
    CHECK(WorksIn(Influence::Signet, 1) && WorksIn(Influence::Signet, 22));   // Zulkheim, Jeuno
    CHECK(!WorksIn(Influence::Signet, 23) && !WorksIn(Influence::Signet, 28)); // Dynamis, West Aht Urhgan
    CHECK(WorksIn(Influence::Sanction, 28) && WorksIn(Influence::Sanction, 32));
    CHECK(!WorksIn(Influence::Sanction, 27) && !WorksIn(Influence::Sanction, 33));
    CHECK(!WorksIn(Influence::Signet, kUnknownRegion) && !WorksIn(Influence::Sanction, kUnknownRegion));
}
