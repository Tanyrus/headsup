#include "look.h"

namespace headsup
{
    LineStyle BadgeLineStyle(const Settings& settings)
    {
        return LineStyle{settings.badgeFontName, true, ToArgb(LineDesign{}.shadowColor.custom), 0.0f, true};
    }

    uint32_t BadgeColor(BadgeKind kind, const Settings& settings)
    {
        switch (kind)
        {
        case BadgeKind::Weak:
        case BadgeKind::WeakRank:
        case BadgeKind::StatusWeak: return ToArgb(settings.weakBadgeColor);
        case BadgeKind::Resist:
        case BadgeKind::ResistRank: return ToArgb(settings.resistBadgeColor);
        case BadgeKind::StatusRank: return ToArgb(settings.rankBadgeColor);
        case BadgeKind::Immune: break;
        }
        return ToArgb(settings.immuneBadgeColor);
    }
}
