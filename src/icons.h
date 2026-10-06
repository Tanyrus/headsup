#pragma once

#include "mobdata.h"

#include <cstdint>

namespace aggroglow
{
    // The MobDB icons XIUI shows (third_party/mobdb-icons), in tools/gen_icons.py's order.
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

    struct IconPng
    {
        const unsigned char* data;
        uint32_t size;
    };

    // The embedded PNG for an icon.
    const IconPng& IconImage(Icon icon);
}
