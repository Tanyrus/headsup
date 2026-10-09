#pragma once

#include "settings.h"
#include "tracker.h"

#include <cstdint>

namespace headsup
{
    // What a look adds to a plate beyond its text: the glow around a mob's name and the ornament above it, both in the
    // mob's outline color.
    struct PlateStyle
    {
        bool glow       = false;
        bool ornament   = false;
        uint32_t accent = 0;
    };

    PlateStyle StyleFor(const ActorInfo& info, bool hasLevelLine, const Settings& settings);
}
