#pragma once

#include "bar_types.h"
#include "fonts.h"

#include <cstdint>

namespace headsup
{
    enum class BarShape : uint8_t
    {
        Ornate, // a strip with an edge line and glow, diamond caps and dividers, a framed target
        Pills,  // a strip with each widget in a rounded pill
        Glass,  // pills of glass that show only when lit, a line of light under the strip, and glass popups
    };

    // D3DCOLORs (ARGB); 0 is none.
    struct BarPalette
    {
        BarShape shape;
        uint32_t stripTop, stripMiddle, stripBottom; // the strip's gradient: top, 55% down, bottom
        uint32_t bottomLine, underLine, below;       // the strip's last row, one row under it, and what fades below
        uint32_t accent;                             // diamonds, frames, the pin mark, the target's border
        uint32_t accentEnd;                          // what the accent runs into where it is a gradient
        uint32_t pill, pillBorder;
        uint32_t title, subtitle, divider;
        uint32_t panelTop, panelBottom, panelBorder, panelOuter;
        uint32_t text, heading, tagFill, tagBorder, tagText;
        uint32_t highlight; // over a hovered widget
    };

    const BarPalette& PaletteOf(BarTheme theme);
    // The menu's name for a theme.

    // Pixel sizes, following the bar's height H.
    struct BarMetrics
    {
        float height;       // H, clamped to kMinBarHeight..kMaxBarHeight
        float title;        // 0.40 H or the set size at most: FitTextRow makes both lines smaller when their ink would
                            // not fit
        float subtitle;     // 0.33 H, or 0.33 / 0.40 of the set size, at most
        bool titleSet;      // a set size: FitTextRow keeps it and shrinks only the detail line, never the title
        float icon;         // 0.50 H
        float padding;      // 0.33 H inside a widget, left and right
        float gap;          // 0.17 H between pills, around the target, after a widget's icons and around a popup
        float cap;          // 0.64 H for an ornate end cap's width, 0.15 H clear either side of its diamond
        float diamond;      // 0.22 H
        float pillHeight;   // 0.80 H, rounded to whole pixels by PillRow
        float pillRadius;   // 0.17 H
        float below;        // 0.28 H for the glow or shadow under the strip
        float popupText;    // 0.39 H, or the set size
        float heading;      // 0.33 H, following the popup's text size
        float popupPadding; // 0.36 H, following the popup's text size
        float rowGap;       // 0.11 H between popup rows, following the popup's text size
        float panelRadius;  // ornate 0.11 H, pills 0.28 H, following the popup's text size
        bool operator==(const BarMetrics&) const = default;
    };
    BarMetrics MetricsFor(int barHeight, BarShape shape, int barText = kAutoBarTextSize,
        int popupText = kAutoPopupTextSize);

    // Ornate titles are Marcellus SC; everything else is the bundled Barlow Condensed.
    const char* TitleFont(BarShape shape);
    constexpr const char* kDetailFont = kBarlowCondensed;
}
