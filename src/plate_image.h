#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
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

    // How far a blur of this CSS radius reaches past what it blurs: the margin it needs.
    int BlurMargin(float blur);

    constexpr int kNoMarkColumn = std::numeric_limits<int>::max();

    // Text as the look draws it, back to front: a glow (the letters blurred, at part strength), a dark shadow (the letters
    // blurred, once in place and once dropped), then the letters. Blurs are CSS radii, twice the Gaussian's sigma; 0 is none.
    struct Layers
    {
        float shadowBlur     = 0.0f;
        int shadowDrop       = 0; // pixels down
        float glowBlur       = 0.0f;
        uint32_t color       = 0;
        uint32_t shadowColor = 0;
        uint32_t glowColor   = 0;
        int markX            = kNoMarkColumn; // from this column on, the letters take markColor
        uint32_t markColor   = 0;
    };
    // The coverage needs a margin of BlurMargin of the wider of the glow and the shadow with its drop.
    Image Styled(const Coverage& coverage, const Layers& layers);
    // A hard edge grown radius pixels around a shape, for the cursor: the coverage needs a margin of radius.
    Image Outlined(const Coverage& coverage, int radius, uint32_t color, uint32_t outlineColor);

    // The ornament between the level line and the name, width by height inside a margin of BlurMargin(shadowBlur): a
    // hairline across its middle that fades in from both ends and lightens toward its center, under a diamond as tall as
    // the ornament, in the lighter color with a dark shadow.
    Image Ornament(int width, int height, float shadowBlur, uint32_t color, uint32_t shadowColor);

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
