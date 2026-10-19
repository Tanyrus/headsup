#pragma once

#include "nameplate.h"
#include "plate_compose.h"
#include "plate_settings.h"

#include <span>
#include <vector>

namespace headsup
{
    constexpr float kLineGap     = 2.0f; // pixels between a line and what sits clear above it
    constexpr float kItemGap     = 2.0f; // pixels between a line's items
    constexpr float kIconGap     = 2.0f; // pixels between a run's icons
    constexpr float kNameOverlap = 3.0f; // pixels: a font's own empty space above its letters
    constexpr float kSpaceShare  = 0.3f; // a space's width, of its line's text size
    constexpr float kHangShare   = 0.25f; // the gap between a line's text and icons hanging beside it, of its text size

    // A text item also has its capitals' rows from its box's top: their top and the baseline, 0 and 0 for none.
    struct ItemSize
    {
        float width = 0.0f, height = 0.0f;
        float inkTop = 0.0f, inkBottom = 0.0f;
    };

    // A composed line as the renderer measured it, in screen pixels: its items' boxes, its tallest text's height (0
    // without text), whether it holds icons, its ornament's size and side, and the room kept under and over it.
    struct LineInput
    {
        std::vector<ItemSize> items;
        Align align          = Align::Center;
        float textHeight     = 0.0f;
        float inkTop = 0.0f, inkBottom = 0.0f; // its tallest text's capitals, which an icon beside it centers on
        float textSize = 0.0f;                 // its letters' size
        bool hasIcons        = false;
        float padBelow = 0.0f, padAbove = 0.0f;
        int firstText = -1, lastText = -1; // its first and last text item; icons outside them hang past the text
        float ornamentWidth  = 0.0f;
        float ornamentHeight = 0.0f;
        bool ornamentUnder   = false;
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
        float ornamentWidth = 0.0f, ornamentHeight = 0.0f; // 0 when the line has none
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
    PlateLayout LayoutPlate(const ScreenBox& anchor, std::span<const LineInput> lines, float cursorWidth,
        float cursorHeight, float cursorTip, int timerCount, float timerHeight, float cursorRaise = 0.0f);
    PlateBox BoxOf(const PlateLayout& layout);

    // Where an icon sits right of its row's first: past the before icons ahead of it, iconSize + kIconGap each, and
    // the overhang of each of the badged ones among them.
    float IconOffset(int before, int badged, float iconSize);
    // A row of count icons, badges of them with a badge.
    float IconRunWidth(int count, int badges, float iconSize);
    // An icon run in its item's box: rows of kIconsPerRow from the bottom up, rowStep apart, the lowest bottomRowTop
    // under the box's top; a partial row sits against the box's right edge when right is set, else its left.
    struct RunShape
    {
        float iconSize = 0.0f;
        float width = 0.0f, height = 0.0f;
        float rowStep = 0.0f, bottomRowTop = 0.0f;
        bool right = false;
    };
    // The line's item, an icon run: as wide as its first row, right when it ends a line it shares. Beside text its
    // box is its own rows; without text every run takes the line's tallest rows and the hang of any badge on the line.
    RunShape ShapeOfRun(const ComposedLine& line, size_t item, float scale);
    // Icon i of the run, from its item's top left corner.
    IconSpot IconInRun(const RunShape& shape, const PlateItem& run, size_t i);
    // A composed line as LayoutPlate takes it, without an ornament: texts holds its Text items' boxes in order.
    void MeasureLine(const ComposedLine& line, std::span<const ItemSize> texts, float scale, LineInput& into);
}
