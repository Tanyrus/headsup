#include "look.h"

namespace headsup
{
    PlateStyle StyleFor(const ActorInfo& info, bool hasLevelLine, const Settings& settings)
    {
        PlateStyle style;
        if (info.kind != EntityKind::Mob || !info.alive) return style;
        style.glow     = true;
        style.ornament = hasLevelLine;
        style.accent   = ToArgb(settings.color[CategoryIndex(info.category)]);
        return style;
    }
}
