#pragma once

#include "settings.h"
#include "tracker.h"

#include <cstdint>
#include <string>

namespace headsup
{
    // What a look adds to a mob's plate beyond its text: the glow around its name and the ornament above it, in the mob's
    // outline color unless the menu gives them their own.
    struct PlateStyle
    {
        bool glow          = false;
        uint32_t glowColor = 0;
        float glowStrength = 0.0f; // times the mockup's glow
        float glowSize     = 0.0f; // times the mockup's glow
        bool ornament      = false;
        uint32_t ornamentColor = 0;
    };

    // How one line of text is drawn: its font and the shadow under it.
    struct LineStyle
    {
        std::string font;
        bool bold            = false;
        uint32_t shadowColor = 0;
        float shadowStrength = 0.0f; // times the mockup's shadow
        bool operator==(const LineStyle&) const = default;
    };

    PlateStyle StyleFor(const ActorInfo& info, bool hasLevelLine, const Settings& settings);
    // Names and the ornament's diamond.
    LineStyle NameLineStyle(const Settings& settings);
    // The level line and the placeholder timers.
    LineStyle LevelLineStyle(const Settings& settings);
}
