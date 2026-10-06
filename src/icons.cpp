#include "icons.h"

#include <iterator>

namespace aggroglow
{
    namespace
    {
#include "generated/icons.inc"

        // The width and height are the first fields of the IHDR chunk, after the signature and the chunk's length and type.
        constexpr size_t kPngWidthOffset  = 16;
        constexpr size_t kPngHeightOffset = 20;

        uint32_t BigEndian32(const unsigned char* bytes)
        {
            return uint32_t{bytes[0]} << 24 | uint32_t{bytes[1]} << 16 | uint32_t{bytes[2]} << 8 | uint32_t{bytes[3]};
        }

        template <size_t N>
        IconPng Png(const unsigned char (&data)[N])
        {
            return IconPng{data, N, BigEndian32(data + kPngWidthOffset), BigEndian32(data + kPngHeightOffset)};
        }

        const IconPng kIconPngs[] = {
            Png(kPngAggroNQ),
            Png(kPngAggroHQ),
            Png(kPngPassiveNQ),
            Png(kPngPassiveHQ),
            Png(kPngLink),
            Png(kPngSight),
            Png(kPngTrueSight),
            Png(kPngSound),
            Png(kPngScent),
            Png(kPngMagic),
            Png(kPngJA),
            Png(kPngBlood),
        };
        static_assert(std::size(kIconPngs) == kIconCount, "one PNG per Icon, in Icon order");
    }

    IconSet IconsFor(const MobRecord* mob)
    {
        IconSet set;
        if (mob == nullptr) return set;
        auto add = [&](Icon icon) { set.icons[set.count++] = icon; };

        const bool notorious  = (mob->flags & kMobNotorious) != 0;
        const bool aggressive = (mob->flags & (kMobAggressive | kMobAlwaysAggro)) != 0 &&
                                (mob->flags & (kMobNoAggro | kMobNeutral)) == 0;
        if (aggressive)
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

    const IconPng& IconImage(Icon icon)
    {
        const auto i = static_cast<unsigned>(icon);
        return kIconPngs[i < kIconCount ? i : 0];
    }
}
