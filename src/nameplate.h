#pragma once

#include <cstdint>
#include <vector>

namespace headsup
{
    // An axis-aligned screen rectangle in pixels.
    struct ScreenBox
    {
        float minX = 0.0f;
        float minY = 0.0f;
        float maxX = 0.0f;
        float maxY = 0.0f;
        bool valid = false;

        void Add(float x, float y);
        void Add(const ScreenBox& other);
        ScreenBox Scaled(float scaleX, float scaleY) const;
        float Width() const { return maxX - minX; }
        float Height() const { return maxY - minY; }
        float CenterX() const { return (minX + maxX) * 0.5f; }
        float CenterY() const { return (minY + maxY) * 0.5f; }
    };

    constexpr uint32_t kWhite = 0xFFFFFFFF;
    // XYZRHW vertices start with x, y, z and rhw; the diffuse color, when present, follows.
    constexpr uint32_t kPretransformedPositionBytes = 4 * sizeof(float);

    // Vertices a draw of primitiveCount primitives reads, by D3DPRIMITIVETYPE (1 point list ... 6 triangle fan);
    // 0 for other types.
    uint32_t VertexCount(uint32_t primitiveType, uint32_t primitiveCount);

    // The box of a pretransformed (XYZRHW) draw when it is text placed in the 3D scene: every vertex depth strictly
    // between 0 and 1, as the game draws nameplates. HUD text, such as the target bar's copy of a mob's name, is drawn
    // at depth 0 and is rejected, as are draws of more than kMaxTextVertices vertices. depth is the farthest vertex's.
    constexpr uint32_t kMaxTextVertices = 256;
    bool WorldTextBox(const void* vertices, uint32_t stride, uint32_t count, ScreenBox& box, float& depth);

    // Glyph quads larger than this (back-buffer pixels) are not letters: the game also draws screen-sized in-scene quads,
    // and on some frames one is attributed to whichever mob is on the stack.
    constexpr float kMaxGlyphSize = 64.0f;
    // A nameplate has at least this many glyph quads (a three-letter name; names also have a leading quad).
    constexpr size_t kMinGlyphs = 3;

    // The nameplate among one mob's glyph boxes: the largest run of letter-sized glyphs on the main text line with no gap
    // wider than two glyph heights. A glyph of another name, or a large quad, attributed to the mob by a stale stack
    // pointer on some frames, falls outside the run and is ignored. Invalid when the run has fewer than kMinGlyphs.
    ScreenBox NameplateFromGlyphs(std::vector<ScreenBox> glyphs);

    // Frames in a row a nameplate must be drawn before its label shows, so a one-frame pop never flashes a label.
    constexpr uint32_t kStableFrames = 2;

    // Whether the camera can see the mob well enough to label it: the game drew its body this frame, its nameplate has
    // been drawn for kStableFrames frames in a row, and some of the nameplate is on screen.
    bool LabelVisible(const ScreenBox* plate, uint32_t meshDraws, uint32_t plateFramesInRow, float screenWidth, float screenHeight);

    // One nameplate glyph draw and the color the game shows it in (white when the draw has none).
    struct GlyphDraw
    {
        ScreenBox box;
        uint32_t argb;
    };

    // The color the game shows for a letter: its vertex color times the texture stage's modulate scale (1, 2 or 4),
    // made opaque like our name's outline.
    uint32_t ShownColor(uint32_t diffuse, uint32_t modulateScale);

    // The game's color for a name: the most common color among the letters inside its nameplate box; white if none.
    uint32_t NameColor(const std::vector<GlyphDraw>& glyphs, const ScreenBox& plate);

    // Who drew a glyph, by the stack scan.
    enum class GlyphOwner : uint8_t
    {
        None,
        Mob,
        Other, // a player or NPC
    };

    // Replace mode: whether to hide a glyph of a mob's name. Only names HeadsUp replaced in the previous frame count
    // (replacedPlates), and only glyphs as tall as that name's letters. Hidden inside such a name (padded by two letter
    // heights), or when the game credits it to a mob whose replaced name is within six letter heights (a stale stack
    // pointer). ownerPlate is the owner's previous name: for a mob, only if it was replaced. A player's or NPC's glyph
    // inside its own name is never hidden.
    bool HideGlyph(const ScreenBox& glyph, GlyphOwner owner, const ScreenBox* ownerPlate,
        const std::vector<ScreenBox>& replacedPlates);

    // Measured sizes of a nameplate's lines (0 for a line that is not shown).
    struct LineSizes
    {
        float nameWidth, nameHeight;
        float labelWidth, labelHeight;
        int iconCount;
        float iconSize;
    };

    // Top-left corners of each line, centered on the nameplate. With a name: the name is centered on the game's name,
    // the label 1 px above it and the icon row 2 px above that. Without: the label and icons stack 2 px above the game's
    // name. Icons are iconStep apart.
    struct NameplateLayout
    {
        float nameX, nameY;
        float labelX, labelY;
        float iconsX, iconsY, iconStep;
    };
    NameplateLayout LayoutNameplate(const ScreenBox& plate, const LineSizes& sizes, bool showName);

    // How much to scale text and icons to follow the game's names: letterHeight / (screenHeight / 180), the typical
    // letter, clamped to 0.5x-2.5x. Not rounded, so sizes change smoothly.
    float DistanceScale(float letterHeight, float screenHeight);
    // The pixel height to draw text at when it is shown pixels tall: a size from 6 px in steps of 1.25x, which is then
    // scaled to the shown size. current, the size it is drawn at now, is kept while the shown size is no more than a
    // step below it or 5% above it, so text is redrawn only when its size has really changed.
    int RasterHeight(float pixels, int current);

}
