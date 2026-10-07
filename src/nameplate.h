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
        uintptr_t texture; // the font's for letters; a name's icons use others
    };

    // The color the game shows for a letter: its vertex color times the texture stage's modulate scale (1, 2 or 4),
    // made opaque like our name's outline.
    uint32_t ShownColor(uint32_t diffuse, uint32_t modulateScale);

    // The game's color for a name: the most common color among the letters inside its nameplate box; white if none.
    uint32_t NameColor(const std::vector<GlyphDraw>& glyphs, const ScreenBox& plate);

    // Whose name a glyph belongs to, by the stack scan: an entity whose name HeadsUp replaced last frame, one whose name
    // it kept, or nobody.
    enum class GlyphOwner : uint8_t
    {
        None,
        Replaced,
        Kept,
    };

    // Whether to hide a glyph of the game's names. letter: drawn with the font's texture; anything else beside a name is
    // one of its icons. ownerPlate is the owner's previous name.
    // A letter counts only for names HeadsUp replaced in the previous frame (replacedPlates), and only if it is as tall
    // as that name's letters: hidden inside such a name (padded by two letter heights), or when the game credits it to
    // a Replaced owner whose name is within six letter heights (a stale stack pointer). A Kept owner's letter inside its
    // own name is never hidden.
    // An icon is hidden when its Replaced owner's name is beside it (BesideName).
    bool HideGlyph(const ScreenBox& glyph, bool letter, GlyphOwner owner, const ScreenBox* ownerPlate,
        const std::vector<ScreenBox>& replacedPlates);

    // Measured sizes of a nameplate's lines (0 for a line that is not shown).
    struct LineSizes
    {
        float nameWidth, nameHeight;
        float labelWidth, labelHeight;
        int iconCount;
        float iconSize;
        float cursorWidth  = 0.0f; // 0 without a target cursor
        float cursorHeight = 0.0f;
        float cursorTip    = 0.5f; // where across its width the cursor points, placed over the center
        int nameIconCount       = 0; // a player's icons, left of the name
        float nameIconSize      = 0.0f;
        bool centerNameAndIcons = false; // the name and its icons centered together, rather than the name alone
    };

    // Top-left corners of each line, centered on the nameplate. With a name: the name is centered on the game's name,
    // with the label just above it. Without: the label stacks 2 px above the game's name. The icon row, then the target
    // cursor, stack 2 px above whatever is below them. Icons are iconStep apart. A player's icons sit in a row against
    // the name's left edge, centered on it.
    struct NameplateLayout
    {
        float nameX, nameY;
        float labelX, labelY;
        float iconsX, iconsY, iconStep;
        float cursorX, cursorY;
        float nameIconsX, nameIconsY, nameIconStep;
    };
    NameplateLayout LayoutNameplate(const ScreenBox& plate, const LineSizes& sizes, bool showName);

    // The box of a pretransformed quad (4 vertices) drawn in the game's UI layer, at depth 0.
    constexpr uint32_t kQuadVertices = 4;
    bool UiQuadBox(const void* vertices, uint32_t stride, ScreenBox& box);

    // A mob whose name has our target cursor this frame: the game's cursor over that name is hidden.
    struct CursorName
    {
        uint16_t index;
        ScreenBox name; // back-buffer pixels
    };

    // The letters' box moved sideways so it is centered where the whole nameplate is: the game centers a name and the
    // icons beside it together over the entity, so this is where the entity is.
    ScreenBox CenteredOver(const ScreenBox& letters, const ScreenBox& whole);

    // Whether a quad the game draws beside a name is one of its icons: on the name's line, up to eight letter heights to
    // its left, and sized like the name (the game's icon strip is 1.5 letters tall and up to 2.7 wide).
    bool BesideName(const ScreenBox& glyph, const ScreenBox& name);

    // Whether a quad the game draws in its UI layer is its target cursor over this name (both in back-buffer pixels):
    // a cursor-sized quad centered on the name, with its bottom edge just above the name's top. The game centers it on
    // the name and the icons beside it together.
    bool IsGameCursor(const ScreenBox& quad, const ScreenBox& name);

    // How far up our target cursor floats at this time, in pixels: a slow bob of up to 15% of its height.
    float CursorBob(double seconds, float height);

    // How much to scale text and icons to follow the game's names: letterHeight / (screenHeight / 180), the typical
    // letter, clamped to 0.5x-2.5x. Not rounded, so sizes change smoothly.
    float DistanceScale(float letterHeight, float screenHeight);
    // The pixel height to draw text at when it is shown pixels tall: a size from 6 px in steps of 1.25x, which is then
    // scaled to the shown size. current, the size it is drawn at now, is kept while the shown size is no more than a
    // step below it or 5% above it, so text is redrawn only when its size has really changed.
    int RasterHeight(float pixels, int current);

}
