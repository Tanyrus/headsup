#pragma once

#include "screen_box.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace headsup
{
    struct CursorTargets
    {
        uint16_t target    = 0;
        uint16_t subTarget = 0;
        bool locked        = false;
        bool outOfRange    = false;
        // The target window's anchors on screen, known only once the menu's size is.
        bool anchored    = false;
        float anchorX    = 0.0f, anchorY = 0.0f;
        float subAnchorX = 0.0f, subAnchorY = 0.0f;
    };
    // The game draws its arrow over the candidate being picked red when it is out of range of the spell or ability and
    // blue in range; its arrow over the target stays gray.
    std::optional<bool> PickedOutOfRange(uint32_t argb);

    // While a sub-target is being picked, ITarget's slot 1 holds the target and slot 0 the candidate.
    CursorTargets TargetsFromSlots(bool picking, uint32_t slot0, uint32_t slot1, bool locked, uint32_t entityCount);

    // The target window's anchors, in the menu's pixels.
    struct CursorWindow
    {
        float ankX, ankY, subAnkX, subAnkY;
    };
    // In the back buffer's pixels; a size not known yet (0) leaves the targets unanchored.
    void PlaceAnchors(CursorTargets& targets, const CursorWindow& window, float menuWidth, float menuHeight,
        float backBufferWidth, float backBufferHeight);

    struct CursorName
    {
        uint16_t index;
        ScreenBox name;
    };

    // Where the bottom center of the game's cursor sits, in the UI image's pixels.
    struct CursorAnchor
    {
        uint16_t index;
        float x, y;
    };
    // The sub anchor keeps its last position after picking ends, so it counts only while picking.
    std::vector<CursorAnchor> GameCursorAnchors(const std::vector<CursorName>& names, const CursorWindow& window, bool picking);

    // Its bottom may sit up to its height above the anchor, as the cursor bobs.
    bool AtCursorAnchor(const ScreenBox& quad, float anchorX, float anchorY);
    // The menu's pointer is drawn from the arrows' texture too, but is wider than tall.
    bool LooksLikeTargetArrow(const ScreenBox& quad);
    // The game centers its cursor on the name and the icons beside it together.
    bool IsGameCursor(const ScreenBox& quad, const ScreenBox& name);

    struct CursorQuad
    {
        ScreenBox ui;
        ScreenBox screen;
        uintptr_t texture;
    };
    // A cheap test before the stack scan for who drew the quad.
    bool MayBeGameCursor(const CursorQuad& quad, uintptr_t arrowTexture, const std::vector<CursorAnchor>& anchors,
        const std::vector<CursorName>& names);

    struct CursorVerdict
    {
        bool block      = false;
        bool learnArrow = false;
    };
    // The arrows' texture is learned only from an arrow credited to its entity and taller than any letter, never from the
    // names' font, whose letters are credited to the target too.
    CursorVerdict JudgeGameCursor(const CursorQuad& quad, uintptr_t arrowTexture, uintptr_t fontTexture,
        const std::vector<CursorAnchor>& anchors, const std::vector<CursorName>& names, std::optional<uint16_t> owner);
}
