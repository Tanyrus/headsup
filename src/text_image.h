#pragma once

#include <cstdint>
#include <vector>

namespace headsup
{
    constexpr int kBytesPerPixel = 4; // A8R8G8B8 textures and GDI's 32-bit bitmaps

    // Antialiased coverage, of GDI's text or a filled shape: one byte per pixel, rows top to bottom.
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

    // The coverage in color over an outline in outlineColor: the coverage grown by radius pixels. It needs a margin of
    // radius empty pixels for the outline to fit.
    Image Outlined(const Coverage& coverage, int radius, uint32_t color, uint32_t outlineColor);

    // The texture side that holds this many pixels: the next power of two, which every Direct3D 8 card accepts.
    int TextureSide(int pixels);
}
