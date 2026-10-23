#pragma once

#include "game_font.h"

#include <cstdint>
#include <span>
#include <string>

namespace headsup
{
    // The game's eight Window Types (Config > Windows > Shared), each a skin of its own: ROM/0/14.DAT for Type 1 to
    // ROM/0/21.DAT for Type 8.
    constexpr int kWindowTypes = 8;
    std::string WindowSkinFile(int type);

    // A skin's pictures: the background the game tiles every 128 pixels, the strip its top and bottom edges are cut
    // from, the one its left and right edges are, and its four corners; and the background's brightness at the top
    // and at the bottom of a window (1 is the picture as it is), which the game shades it with.
    struct WindowSkin
    {
        ColorPicture back, across, down, corner;
        float shadeTop = 1.0f, shadeBottom = 1.0f;
    };
    // A skin out of its whole DAT.
    Read<WindowSkin> ReadWindowSkin(std::span<const uint8_t> dat);
}
