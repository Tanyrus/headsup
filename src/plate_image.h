#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
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

    constexpr uint8_t kInkCoverage = 64; // a quarter: the least a pixel of a glyph is covered to count as its ink

    // The rows and columns of a coverage holding ink, right and bottom exclusive.
    struct InkBox
    {
        int left = 0, top = 0, right = 0, bottom = 0;
        bool Empty() const { return right <= left || bottom <= top; }
    };
    // The box around every pixel covered at least threshold; empty when there is none.
    InkBox FindInk(const Coverage& coverage, uint8_t threshold);

    // Where a font's ink sits in the line GDI draws it in, in rows from the line's top: the top of its tallest
    // capitals and figures, its baseline, and the bottom of its descenders.
    struct LineInk
    {
        float top = 0.0f, baseline = 0.0f, bottom = 0.0f;
    };
    // A line's ink from the ink of glyphs drawn in it and of an H: nothing when either drew none.
    std::optional<LineInk> LineInkOf(const InkBox& glyphs, const InkBox& capital);

    // D3DCOLOR (ARGB), rows top to bottom, not premultiplied.
    struct Image
    {
        int width = 0;
        int height = 0;
        std::vector<uint32_t> argb;
    };

    // How far a blur of this CSS radius reaches past what it blurs: the margin it needs.
    int BlurMargin(float blur);

    // A text's soft shadow: its blur radius per pixel of the text's height, from the mockup's 22 px name.
    constexpr float kShadowPerHeight = 0.12f;

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
        float glowStrength   = 1.0f; // times the mockup's glow, which is solid wherever it would pass full
        float shadowStrength = 1.0f; // times the mockup's shadow, the same way
    };
    // The coverage needs a margin of BlurMargin of the wider of the glow and the shadow with its drop.
    Image Styled(const Coverage& coverage, const Layers& layers);
    // A hard edge grown radius pixels around a shape, for the cursor: the coverage needs a margin of radius.
    Image Outlined(const Coverage& coverage, int radius, uint32_t color, uint32_t outlineColor);

    // The ornament between the level line and the name, width by height inside a margin of BlurMargin(shadowBlur): a
    // hairline across its middle that fades in from both ends and lightens toward its center, under a diamond as tall as
    // the ornament, in the lighter color with a dark shadow.
    Image Ornament(int width, int height, float shadowBlur, float shadowStrength, uint32_t color, uint32_t shadowColor);

    // The bar's background as one column of pixels, stretched across the screen: a vertical gradient through top,
    // middle (55% down) and bottom over height rows, whose last row is bottomLine; then one row of underLine (none when
    // 0), then below fading to clear over belowRows rows.
    Image StripColumn(int height, int belowRows, uint32_t top, uint32_t middle, uint32_t bottom, uint32_t bottomLine,
        uint32_t underLine, uint32_t below);

    struct BoxStyle
    {
        float radius     = 0.0f;
        uint32_t top     = 0, bottom = 0; // the fill, top to bottom
        uint32_t border  = 0;             // 1 px along the inside of the edge; 0 for none
        uint32_t outer   = 0;             // a second 1 px border outerGap pixels outside the box; 0 for none
        int outerGap     = 0;
    };
    // How far past the box its outer border reaches: its gap and its own pixel, or nothing without one.
    int OuterPad(const BoxStyle& style);
    // A diamond size pixels tall and wide.
    Image Diamond(int size, uint32_t color);
    // Gil's coin: a disc size pixels across with a 1 px rim at 60% of its color's brightness.
    Image Coin(int size, uint32_t color);
    // A five-pointed star with a point straight up, in the circle size pixels across.
    Image Star(int size, uint32_t color);

    // A size in whole pixels, at least one: what text and images are drawn at.
    int WholePixels(float size);
    // A power of two, which every Direct3D 8 card accepts.
    int TextureSide(int pixels);
    // What a device that reports no largest texture is taken to make.
    constexpr int kFallbackTextureSide = 2048;
    // The longest side of a text's texture, from the device's largest width and height (0 for unknown): the smaller,
    // as a power of two, and no more than the 32-bit client can hold a screenful of.
    int TextTextureSide(uint32_t maxWidth, uint32_t maxHeight);
    // Puts a width by height image of D3DCOLORs in the top-left corner of a sideX by sideY texture whose rows start
    // pitch bytes apart, and clears the rest, so filtering at the image's edge blends with nothing.
    void CopyToCorner(const void* image, int width, int height, void* rows, int pitch, int sideX, int sideY);

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
