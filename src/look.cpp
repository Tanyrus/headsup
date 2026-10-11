#include "look.h"

namespace headsup
{
    namespace
    {
        constexpr float kPercent = 100.0f;
    }

    PlateStyle StyleFor(const ActorInfo& info, bool hasLevelLine, const Settings& settings)
    {
        PlateStyle style;
        if (info.kind != EntityKind::Mob || !info.alive) return style;
        const uint32_t outline = ToArgb(settings.color[CategoryIndex(info.category)]);
        style.glow          = settings.nameGlow && !(settings.glowOffWhenFighting && info.claimedByParty);
        style.glowColor     = settings.ownGlowColor ? ToArgb(settings.glowColor) : outline;
        style.glowStrength  = static_cast<float>(settings.glowStrength) / kPercent;
        style.glowSize      = static_cast<float>(settings.glowSize) / kPercent;
        style.ornament      = hasLevelLine && settings.showOrnament;
        style.ornamentColor = settings.ownOrnamentColor ? ToArgb(settings.ornamentColor) : outline;
        return style;
    }

    LineStyle NameLineStyle(const Settings& settings)
    {
        return LineStyle{settings.fontName, settings.fontBold, ToArgb(settings.textOutline),
            static_cast<float>(settings.nameShadow) / kPercent};
    }

    LineStyle LevelLineStyle(const Settings& settings)
    {
        return LineStyle{settings.labelFontName, settings.labelFontBold, ToArgb(settings.labelShadowColor),
            static_cast<float>(settings.labelShadow) / kPercent};
    }

    LineStyle BadgeLineStyle(const Settings& settings)
    {
        return LineStyle{settings.badgeFontName, true, ToArgb(settings.labelShadowColor), 0.0f, true};
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
