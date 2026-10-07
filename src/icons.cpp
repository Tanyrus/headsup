#include "icons.h"

#include <iterator>

namespace headsup
{
    namespace
    {
#include "generated/icons.inc"
    }

    IconSet IconsFor(const MobRecord* mob)
    {
        IconSet set;
        if (mob == nullptr) return set;
        auto add = [&](Icon icon) { set.icons[set.count++] = icon; };

        const bool notorious = (mob->flags & kMobNotorious) != 0;
        if (IsAggressive(*mob))
            add(notorious ? Icon::AggroHQ : Icon::AggroNQ);
        else
            add(notorious ? Icon::PassiveHQ : Icon::PassiveNQ);
        if (mob->flags & kMobLink) add(Icon::Link);

        const bool trueDetection = (mob->flags & kMobTrueDetection) != 0;
        if ((mob->detects & kDetectSight) && !trueDetection) add(Icon::Sight);
        if (trueDetection) add(Icon::TrueSight);
        if (mob->detects & kDetectHearing) add(Icon::Sound);
        if (mob->detects & kDetectScent) add(Icon::Scent);
        if (mob->detects & kDetectMagic) add(Icon::Magic);
        if (mob->detects & kDetectAbility) add(Icon::Ability);
        if (mob->detects & kDetectLowHp) add(Icon::Blood);
        return set;
    }

    const IconBitmap& IconImage(Icon icon)
    {
        return kIcons[static_cast<size_t>(icon)];
    }
}
