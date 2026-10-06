#pragma once

#include <cstdint>
#include <vector>

namespace aggroglow
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
    };

    // Vertices a draw of primitiveCount primitives reads, by D3DPRIMITIVETYPE (1 point list ... 6 triangle fan);
    // 0 for other types.
    uint32_t VertexCount(uint32_t primitiveType, uint32_t primitiveCount);

    // The box of a pretransformed (XYZRHW) draw when it is text placed in the 3D scene: every vertex depth strictly
    // between 0 and 1, as the game draws nameplates. HUD text, such as the target bar's copy of a mob's name, is drawn
    // at depth 0 and is rejected, as are draws of more than kMaxTextVertices vertices.
    constexpr uint32_t kMaxTextVertices = 256;
    bool WorldTextBox(const void* vertices, uint32_t stride, uint32_t count, ScreenBox& box);

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
    // been drawn for kStableFrames frames in a row, and the nameplate lies fully inside the screen.
    bool LabelVisible(const ScreenBox* plate, uint32_t meshDraws, uint32_t plateFramesInRow, float screenWidth, float screenHeight);

    // Top-left corner of a width x height label centered above a box, gap pixels above its top.
    void PlaceAbove(const ScreenBox& box, float width, float height, float gap, float& x, float& y);
}
