#pragma once

#include "mobdata.h"

#include <cstdint>

namespace headsup
{
    // The MobDB icons XIUI shows (third_party/mobdb-icons).
    enum class Icon : uint8_t
    {
        AggroNQ,
        AggroHQ,
        PassiveNQ,
        PassiveHQ,
        Link,
        Sight,
        TrueSight,
        Sound,
        Scent,
        Magic,
        Ability, // JA.png: job abilities and weapon skills
        Blood,   // low HP
    };
    constexpr int kIconCount = 12;

    // A mob's icons, left to right.
    struct IconSet
    {
        Icon icons[9];
        int count = 0;
    };

    // XIUI's order: aggressive or passive (HQ for notorious monsters), link, then detection. The aggro icon is the
    // mob's nature (it aggros at all), not whether it would attack this player. Empty without data.
    IconSet IconsFor(const MobRecord* mob);

    // An icon's pixels, rows top to bottom, each B, G, R, A as D3DFMT_A8R8G8B8 keeps it in memory.
    struct IconBitmap
    {
        const unsigned char* bgra;
        uint32_t width;
        uint32_t height;
    };

    const IconBitmap& IconImage(Icon icon);
}
