#pragma once

#include "argb.h"
#include "screen_box.h"

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace headsup
{
    // D3DPRIMITIVETYPE's values: this file is built without the Direct3D headers.
    enum PrimitiveType : uint32_t
    {
        kPointList = 1,
        kLineList,
        kLineStrip,
        kTriangleList,
        kTriangleStrip,
        kTriangleFan,
    };
    // Every type counts: the caller reads this many vertices from the draw.
    uint32_t VertexCount(uint32_t primitiveType, uint32_t primitiveCount);

    // x, y, z and rhw; the diffuse color, when present, follows.
    constexpr uint32_t kPretransformedPositionBytes = 4 * sizeof(float);

    // The game draws its names inside the scene (0 < z < 1), and HUD text, such as the target bar's copy of a name, at 0.
    constexpr uint32_t kMaxTextVertices = 256;
    bool WorldTextBox(const void* vertices, uint32_t stride, uint32_t count, ScreenBox& box, float& depth);

    constexpr uint32_t kQuadVertices = 4;
    bool UiQuadBox(const void* vertices, uint32_t stride, ScreenBox& box);

    // The largest run of letter-sized glyphs on the main text line: a letter of another name, credited to the entity by
    // a stale stack pointer on some frames, falls outside it.
    ScreenBox NameplateFromGlyphs(std::vector<ScreenBox> glyphs);

    struct GlyphDraw
    {
        ScreenBox box;
        uint32_t argb;
        uintptr_t texture;
        float depth;
    };

    // Opaque, like our name's outline.
    uint32_t ShownColor(uint32_t diffuse, uint32_t modulateScale);
    uint32_t NameColor(const std::vector<GlyphDraw>& letters, const ScreenBox& plate);
    // Only letters inside the box count: other quads credited to the entity can be deeper, and would put our nameplate
    // behind its own head seen from above.
    float NameDepth(const std::vector<GlyphDraw>& letters, const ScreenBox& plate);

    bool BesideName(const ScreenBox& glyph, const ScreenBox& name);

    // A letter marks the image names go to. The game also draws a quad over each character into another image, at the
    // character's depth, and blocking it makes the character vanish.
    bool InNamesImage(bool letter, uintptr_t image, uintptr_t& namesImage);

    struct FrameNames
    {
        uintptr_t font = 0;
        std::unordered_map<uint16_t, ScreenBox> plates;
        std::unordered_map<uint16_t, ScreenBox> wholes;
        std::unordered_map<uint16_t, uint32_t> colors;
        std::unordered_map<uint16_t, float> depths;
        std::unordered_map<uint16_t, uint32_t> glyphCounts;
        std::unordered_map<uint16_t, uint32_t> framesInRow;
        std::unordered_map<uint16_t, std::vector<float>> recentHeights;
        // The median of recentHeights, so a stray glyph stretching a name for a frame or two does not resize its plate.
        std::unordered_map<uint16_t, float> sizes;
        std::vector<ScreenBox> replacedPlates, keptPlates;
        float largestLetterHeight = 0.0f;
        // A name that comes back has its icons drawn before its letters, so they are sized by its last height.
        struct Seen
        {
            float height;
            uint32_t framesGone;
        };
        std::unordered_map<uint16_t, Seen> lastSeen;
    };
    constexpr uint32_t kRememberedNameFrames = 60;

    float LastLetterHeight(const FrameNames& last, uint16_t index);
    using TextureUse = std::unordered_map<uintptr_t, uint32_t>;

    // enlarged: the candidate being picked for a spell or ability, whose name the game draws at a fixed size, so its
    // size keeps its heights from before; 0 for none.
    FrameNames ReadFrameNames(const std::unordered_map<uint16_t, std::vector<GlyphDraw>>& credited, const TextureUse& use,
        const std::unordered_set<uint16_t>& kept, const FrameNames& last, uint16_t enlarged = 0);

    // A letter is hidden wherever it is, as names move between frames and the sub-target's is drawn larger, except inside
    // a kept name, where the game credited it to the wrong entity.
    bool HideReplacedGlyph(const ScreenBox& glyph, bool letter, float letterHeight, const std::vector<ScreenBox>& keptPlates);

    // The game often credits a name's first quad to the entity drawn before it, so a letter credited to a kept name or
    // to no one may be a replaced name's.
    bool HideStrayLetter(const ScreenBox& glyph, const ScreenBox* ownPlate, const std::vector<ScreenBox>& replacedPlates);

    // A replaced name's parts are sized by the larger of its own letters (ownerHeight) and the last frame's largest name:
    // the game can credit a big name's icon to a small one.
    bool HideGameGlyph(const ScreenBox& glyph, bool letter, bool replaced, const ScreenBox* ownerName, float ownerHeight,
        const FrameNames& last);
}
