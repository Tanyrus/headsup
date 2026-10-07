#pragma once

#include "generated/icon_ids.h"
#include "mobdata.h"

#include <cstdint>

namespace headsup
{

    // The longest row: a mob's aggro, link, sight or true sight, sound, scent, magic, ability and blood.
    constexpr int kMaxIcons = 8;

    // A row of icons, left to right: a mob's MobDB icons or a player's status icons.
    struct IconSet
    {
        Icon icons[kMaxIcons];
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
