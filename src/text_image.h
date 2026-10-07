#pragma once

#include <cstdint>
#include <vector>

namespace headsup
{
    // Antialiased text as GDI draws it: one coverage byte per pixel, rows top to bottom.
    struct Coverage
    {
        int width = 0;
        int height = 0;
        std::vector<uint8_t> alpha;
    };

    // Pixels as D3DCOLOR (ARGB), rows top to bottom, not premultiplied.
    struct Image
    {
        int width = 0;
        int height = 0;
        std::vector<uint32_t> argb;
    };

    // The text in color over an outline in outlineColor: the text's coverage grown by radius pixels. The coverage needs
    // a margin of radius empty pixels for the outline to fit.
    Image OutlinedText(const Coverage& text, int radius, uint32_t color, uint32_t outlineColor);

    // The texture side that holds this many pixels: the next power of two, which every Direct3D 8 card accepts.
    int TextureSide(int pixels);
}
