#include "bar_signet.h"

#include "player_status.h" // kStatusIconSlots
#include "vanadiel.h"      // kVanaEpochUnixMs
#include "zones.h"

#include <string>
#include <string_view>

namespace headsup
{
    namespace
    {
        // Phoenix's REGION_TYPE.
        constexpr uint8_t kJeuno = 22, kWestAhtUrhgan = 28, kAlzadaal = 32;

        std::optional<Influence> InfluenceOf(int16_t effect)
        {
            if (effect == kSignetEffect) return Influence::Signet;
            if (effect == kSanctionEffect) return Influence::Sanction;
            return std::nullopt;
        }
    }

    std::optional<double> StatusSecondsLeft(uint32_t raw, int64_t unixMs)
    {
        if (raw == kNoExpiry) return std::nullopt;
        const int64_t now = (unixMs - kVanaEpochUnixMs) * kStatusTicksPerSecond / kMsPerSecond;
        const auto left   = static_cast<int32_t>(raw - static_cast<uint32_t>(now));
        return left > 0 ? static_cast<double>(left) / kStatusTicksPerSecond : 0.0;
    }

    std::optional<SignetState> SignetFrom(const int16_t* buffs, const int16_t* icons, const uint32_t* timers,
        int64_t unixMs)
    {
        if (buffs == nullptr) return std::nullopt;
        for (int slot = 0; slot < kStatusIconSlots; ++slot)
        {
            const std::optional<Influence> which = InfluenceOf(buffs[slot]);
            if (!which) continue;
            SignetState state{*which, std::nullopt};
            for (int i = 0; icons != nullptr && timers != nullptr && i < kStatusIconSlots; ++i)
                if (icons[i] == buffs[slot])
                {
                    state.secondsLeft = StatusSecondsLeft(timers[i], unixMs);
                    break;
                }
            if (state.secondsLeft && *state.secondsLeft > kLongestInfluence) state.secondsLeft.reset();
            return state;
        }
        return std::nullopt;
    }

    bool WorksIn(Influence which, uint8_t region)
    {
        if (which == Influence::Signet) return region <= kJeuno;
        return region >= kWestAhtUrhgan && region <= kAlzadaal;
    }

    const char* InfluenceName(Influence which) { return which == Influence::Signet ? "Signet" : "Sanction"; }

    WidgetContent SignetWidget(const std::optional<SignetState>& signet)
    {
        WidgetContent w;
        if (!signet) return w; // hidden while neither is on
        w.shown = true;
        w.title = ListOf(Text(InfluenceName(signet->which)));
        if (!signet->secondsLeft)
            w.subtitle = ListOf(Text("-", Tone::Dim));
        else if (*signet->secondsLeft < kInfluenceWarning)
            w.subtitle = ListOf(Colored(SpanText(*signet->secondsLeft), kWarningColor));
        else
            w.subtitle = ListOf(Text(SpanText(*signet->secondsLeft), Tone::Dim));
        w.hasPopup = true;
        return w;
    }

    PopupContent SignetPopup(const std::optional<SignetState>& signet, uint16_t zone)
    {
        PopupContent popup;
        if (!signet) return popup;
        const bool isSignet = signet->which == Influence::Signet;
        popup.title         = ListOf(Text(InfluenceName(signet->which)));
        PopupSection rows;
        rows.rows.push_back(Labeled("Time left",
            signet->secondsLeft ? Text(SpanText(*signet->secondsLeft)) : Text("-", Tone::Dim)));
        // From enemies that give EXP; Sanction's bonus is the Regen, Refresh or food time chosen at the guard.
        rows.rows.push_back(Labeled("Gives", Text(isSignet ? "conquest points, crystals"
                                                           : "Imperial Standing, your chosen bonus")));
        const ZoneRecord* record = KnownZone(zone);
        if (const std::string_view region = ZoneRegion(record); !region.empty())
            rows.rows.push_back(LineOf("Here: " + std::string(region) +
                                       (WorksIn(signet->which, record->region) ? ", where it works"
                                                                               : ", where it does not work")));
        popup.sections = ListOf(std::move(rows));
        return popup;
    }
}
