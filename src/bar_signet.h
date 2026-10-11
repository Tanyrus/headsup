#pragma once

#include "bar_content.h"

#include <cstdint>
#include <optional>

namespace headsup
{
    constexpr int16_t kSignetEffect = 253, kSanctionEffect = 256; // Phoenix's status_effects.yaml
    constexpr uint32_t kNoExpiry            = 0x7FFFFFFF;    // a status timer that never runs out
    constexpr int64_t kStatusTicksPerSecond = 60;
    constexpr double kLongestInfluence      = 24.0 * 3600.0; // seconds: longer than Phoenix gives either
    constexpr double kInfluenceWarning      = 600.0;         // seconds left the bar shows in amber

    // Phoenix bestows one at a time.
    enum class Influence : uint8_t
    {
        Signet,
        Sanction,
    };

    struct SignetState
    {
        Influence which = Influence::Signet;
        std::optional<double> secondsLeft; // nothing when its timer could not be read
        bool operator==(const SignetState&) const = default;
    };

    // A status timer as the game keeps it: 60ths of a second from the Vana'diel epoch's Earth instant, wrapping at 32
    // bits (XIUI's vanatime.lua). The seconds left at unixMs, 0 once run out; nothing for one that never runs out.
    std::optional<double> StatusSecondsLeft(uint32_t raw, int64_t unixMs);
    // Signet or Sanction, whichever is first in the game's buffs, timed from the status icon slot that holds it: the
    // icons and timers share slots. Each array holds kStatusIconSlots and may be null; a time over kLongestInfluence
    // is not trusted.
    std::optional<SignetState> SignetFrom(const int16_t* buffs, const int16_t* icons, const uint32_t* timers,
        int64_t unixMs);
    // Where Phoenix lets each work (experience_points.lua): Signet from Ronfaure to Jeuno, Sanction from West Aht
    // Urhgan to Alzadaal.
    bool WorksIn(Influence which, uint8_t region);
    const char* InfluenceName(Influence which);

    WidgetContent SignetWidget(const std::optional<SignetState>& signet);
    PopupContent SignetPopup(const std::optional<SignetState>& signet, uint16_t zone);
}
