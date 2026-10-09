#pragma once

#include "screen_box.h"

#include <cstdint>
#include <optional>

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
        // The game's arrows could not be hooked, so it draws them still and HeadsUp draws no cursor of its own.
        bool gameArrowsShown = false;
    };
    // The color the target window passes for its arrow over the candidate being picked: red when it is out of range of
    // the spell or ability, blue in range, and a yellow taken as out of range too (unconfirmed). Its arrow over the
    // target stays gray.
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

    // A mouse button pressed or double-clicked: before the game takes it, its pointer is moved to where the click is.
    bool ClickMessage(uint32_t message);

    struct CursorName
    {
        uint16_t index;
        ScreenBox name;
    };
}
