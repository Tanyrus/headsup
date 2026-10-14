#pragma once

#include "icons.h"
#include "settings.h"

#include <cstdint>
#include <string>

namespace headsup
{
    // How one line of text is drawn: its font and the shadow under it.
    struct LineStyle
    {
        std::string font;
        uint8_t style        = 0; // FontStyle bits
        uint32_t shadowColor = 0;
        float shadowStrength = 0.0f; // times the mockup's shadow
        bool outline         = false; // a crisp edge in shadowColor instead of the soft shadow
        bool operator==(const LineStyle&) const = default;
    };

    // Badges' numbers, which sit on their icons.
    LineStyle BadgeLineStyle(const Settings& settings);

    // The color a badge's text takes: weaknesses, resistances, status ranks and immunities each have their own.
    uint32_t BadgeColor(BadgeKind kind, const Settings& settings);
}
