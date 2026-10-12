#pragma once

#include "nameplate.h"
#include "plate_settings.h"

#include <vector>

namespace headsup
{
    constexpr float kLineGap     = 2.0f; // pixels between a line and what sits clear above it
    constexpr float kItemGap     = 2.0f; // pixels between a line's items
    constexpr float kIconGap     = 2.0f; // pixels between a run's icons
    constexpr float kNameOverlap = 3.0f; // pixels: a font's own empty space above its letters
    constexpr float kSpaceShare  = 0.3f; // a space's width, of its line's text size

    struct ItemSize
    {
        float width = 0.0f, height = 0.0f;
    };

    // A composed line as the renderer measured it, in screen pixels: each item's box (a text's ink box without its
    // halo, an icon run with its gaps and badge overhang, a gap's width), its tallest text item's height (0 for a
    // line without text), whether it holds icons, and the size of the ornament above it (0 by 0 for none). The first
    // line never has an ornament: it sits between a line and the one above it, and the layout ignores one here.
    struct LineInput
    {
        std::vector<ItemSize> items;
        Align align          = Align::Center;
        float textHeight     = 0.0f;
        bool hasIcons        = false;
        float ornamentWidth  = 0.0f;
        float ornamentHeight = 0.0f;
    };

    struct ItemPlace
    {
        float x = 0.0f, y = 0.0f;
    };

    struct LinePlace
    {
        std::vector<ItemPlace> items;
        float left = 0.0f, top = 0.0f, width = 0.0f, height = 0.0f;
        ItemPlace ornament;
        float ornamentWidth = 0.0f, ornamentHeight = 0.0f; // 0 when the line has none or is the first
    };

    struct PlateLayout
    {
        std::vector<LinePlace> lines;
        float centerX = 0.0f, top = 0.0f;
        float timersY = 0.0f, timerStep = 0.0f;
        CursorSpot cursor{};
    };

    // Lines top to bottom: the last sits on the game's name and the rest stack above it. What sits above a line (a
    // line, its ornament, your timers, the cursor) sinks kNameOverlap into that line's text; a line with icons never
    // sinks into the line below it, as badges hang off its icons.
    PlateLayout LayoutPlate(const ScreenBox& anchor, const std::vector<LineInput>& lines, float cursorWidth,
        float cursorHeight, float cursorTip, int timerCount, float timerHeight);
    PlateBox BoxOf(const PlateLayout& layout);

    // Icons in a run sit iconSize + kIconGap apart; a run with badges is wider by each badge's overhang.
    float IconRunWidth(int count, float iconSize, bool badges);
}
