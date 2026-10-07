#pragma once

#include "argb.h"
#include "screen_box.h"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace headsup
{
    // D3DPRIMITIVETYPE.
    enum PrimitiveType : uint32_t
    {
        kPointList = 1,
        kLineList,
        kLineStrip,
        kTriangleList,
        kTriangleStrip,
        kTriangleFan,
    };
    // Vertices a draw of primitiveCount primitives reads; 0 for other types.
    uint32_t VertexCount(uint32_t primitiveType, uint32_t primitiveCount);

    // XYZRHW vertices start with x, y, z and rhw; the diffuse color, when present, follows.
    constexpr uint32_t kPretransformedPositionBytes = 4 * sizeof(float);

    // The box of a pretransformed (XYZRHW) draw when it is placed in the 3D scene: every vertex depth strictly between 0
    // and 1, as the game draws its names. HUD text, such as the target bar's copy of a name, is drawn at depth 0 and is
    // rejected, as are draws of more than kMaxTextVertices vertices. depth is the farthest vertex's.
    constexpr uint32_t kMaxTextVertices = 256;
    bool WorldTextBox(const void* vertices, uint32_t stride, uint32_t count, ScreenBox& box, float& depth);

    // The box of a pretransformed quad (4 vertices) drawn in the game's UI layer, at depth 0.
    constexpr uint32_t kQuadVertices = 4;
    bool UiQuadBox(const void* vertices, uint32_t stride, ScreenBox& box);

    // Glyph quads larger than this (back-buffer pixels) are not letters: the game also draws screen-sized in-scene quads,
    // and on some frames one is credited to whichever entity is on the stack.
    constexpr float kMaxGlyphSize = 64.0f;
    bool LetterSized(const ScreenBox& glyph);

    // A name has at least this many glyph quads (a three-letter name; names also have a leading quad).
    constexpr size_t kMinGlyphs = 3;
    constexpr size_t kSizeFrames = 5;
    // The name among one entity's letter boxes: the largest run of letter-sized glyphs on the main text line with no gap
    // wider than two glyph heights. A letter of another name, credited to the entity by a stale stack pointer on some
    // frames, falls outside the run and is ignored. Invalid when the run has fewer than kMinGlyphs.
    ScreenBox NameplateFromGlyphs(std::vector<ScreenBox> glyphs);

    // One glyph draw of the game's names and the color it shows (white when the draw has none).
    struct GlyphDraw
    {
        ScreenBox box;
        uint32_t argb;
        uintptr_t texture; // the font's for letters; a name's icons use others
        float depth;
    };

    // The color the game shows for a letter: its vertex color times the texture stage's modulate scale (1, 2 or 4),
    // made opaque like our name's outline.
    uint32_t ShownColor(uint32_t diffuse, uint32_t modulateScale);
    // A name's color: the most common among the letters inside its box; white if none.
    uint32_t NameColor(const std::vector<GlyphDraw>& letters, const ScreenBox& plate);
    // A name's depth: the deepest of the letters inside its box, 0 without any. Other quads credited to the entity,
    // and letters of other names, can be deeper, and would put our nameplate behind its own head seen from above.
    float NameDepth(const std::vector<GlyphDraw>& letters, const ScreenBox& plate);

    // Whether a quad the game draws beside a name is one of its icons: on the name's line, up to eight letter heights to
    // its left, and sized like the name's icons (the strip is 1.5 letters tall, up to 2.7 wide, larger on the sub-target).
    bool BesideName(const ScreenBox& glyph, const ScreenBox& name);

    // Whether a quad the game draws at a depth in its scene belongs to a name: a letter, which also marks the image
    // names go to (namesImage), or anything else drawn into that image. The game also draws a quad over each character
    // into another image, at the character's depth; it is no part of a name and must be left alone.
    bool InNamesImage(bool letter, uintptr_t image, uintptr_t& namesImage);

    // What the game drew for names in one frame, from the glyphs it credited to each entity (by target index).
    struct FrameNames
    {
        uintptr_t font = 0; // the texture most glyphs use: the names' letters
        std::unordered_map<uint16_t, ScreenBox> plates; // each name's letters
        std::unordered_map<uint16_t, ScreenBox> wholes; // with the game's icons beside them (BesideName)
        std::unordered_map<uint16_t, uint32_t> colors;
        std::unordered_map<uint16_t, float> depths;
        std::unordered_map<uint16_t, uint32_t> glyphCounts;
        std::unordered_map<uint16_t, uint32_t> runs; // frames in a row each entity has had a name
        // Each name's letter heights over its last kSizeFrames frames in a row, and their median: the size its
        // nameplate follows, which a stray glyph stretching the name for a frame or two does not move.
        std::unordered_map<uint16_t, std::vector<float>> recentHeights;
        std::unordered_map<uint16_t, float> sizes;
        std::vector<ScreenBox> replacedPlates, keptPlates;
        float letterHeight = 0.0f; // the median name's; the last frame's when this one has none
    };
    // kept: the entities whose names stay the game's. last: the frame before, for the font, runs and letter height.
    FrameNames ReadFrameNames(const std::unordered_map<uint16_t, std::vector<GlyphDraw>>& glyphs,
        const std::unordered_set<uint16_t>& kept, const FrameNames& last);

    // Whether to hide a quad the game credits to an entity whose name HeadsUp replaces. A letter of the font (letter)
    // is hidden wherever it is, as names move between frames and the sub-target's is drawn larger, unless it is inside a
    // kept name (keptPlates, the previous frame's), where it was credited to the wrong entity. A quad in another texture
    // is hidden when it is sized like the name's icons, by letterHeight, the name's letters' height (0 before any).
    bool HideReplacedGlyph(const ScreenBox& glyph, bool letter, float letterHeight, const std::vector<ScreenBox>& keptPlates);

    // Whether to hide a letter the game credits to a kept name (ownPlate, its previous box) or to no one (nullptr): only
    // inside a name replaced in the previous frame (padded by two letter heights) and as tall as its letters, and never
    // inside its own.
    bool HideStrayLetter(const ScreenBox& glyph, const ScreenBox* ownPlate, const std::vector<ScreenBox>& replacedPlates);

    // Whether to block a glyph of the game's names: HideReplacedGlyph for an entity whose name HeadsUp replaces, sized
    // by its last name or the last frame's letters, else HideStrayLetter. ownerName: the owner's name in the last frame.
    bool HideGameGlyph(const ScreenBox& glyph, bool letter, bool replaced, const ScreenBox* ownerName, const FrameNames& last);
}
