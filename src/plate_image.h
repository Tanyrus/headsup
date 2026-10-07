#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace headsup
{
    constexpr int kBytesPerPixel = 4; // A8R8G8B8 textures and GDI's 32-bit bitmaps

    // Antialiased text or a filled shape, one byte per pixel, rows top to bottom.
    struct Coverage
    {
        int width = 0;
        int height = 0;
        std::vector<uint8_t> alpha;
    };

    // D3DCOLOR (ARGB), rows top to bottom, not premultiplied.
    struct Image
    {
        int width = 0;
        int height = 0;
        std::vector<uint32_t> argb;
    };

    // The coverage needs a margin of radius empty pixels for its outline to fit.
    Image Outlined(const Coverage& coverage, int radius, uint32_t color, uint32_t outlineColor);

    // A power of two, which every Direct3D 8 card accepts.
    int TextureSide(int pixels);

    // A point of a closed outline in the unit box, x to the right and y down.
    struct ShapePoint
    {
        float x, y;
    };

    struct Shape
    {
        const ShapePoint* points;
        size_t count;
        float aspect; // height over width
    };

    const Shape& ArrowShape();
    // Phoenix's feather icon (third_party/phoenix-feather).
    const Shape& FeatherShape();

    // Where the shape points: the x of its lowest point, from 0 to 1 across its width.
    float ShapeTip(const Shape& shape);

    Coverage ShapeCoverage(const Shape& shape, int pixelHeight, int margin);
}
