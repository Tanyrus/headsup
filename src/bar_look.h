#pragma once

#include "bar_layout.h"
#include "bar_theme.h"
#include "bar_widgets.h"
#include "plate_image.h"
#include "popup_layout.h"
#include "settings.h"

#include <cstdint>
#include <optional>

namespace headsup
{
    // A text's font family, pixel size and letter-spacing.
    struct TextFace
    {
        const char* family = nullptr;
        float size         = 0.0f;
        int tracking       = 0; // pixels after each letter
    };

    // A widget's pill, the ornate target's frame or a hover tint: where it is drawn and how.
    struct WidgetBox
    {
        BarRect rect;
        BoxStyle style;
    };

    // A frame's palette, sizes and fonts: what every part of the bar is drawn with.
    struct BarLook
    {
        const BarPalette& palette;
        const Settings& settings;
        BarMetrics metrics;
        BarRow row;    // the pills' and the ornate target frame's
        TextRow lines; // the widgets' text
        const char* titleFont;
        bool ornate;
        const char* detailFont = kDetailFont; // details, figures and popup text

        // A span's color on the bar, and in a popup.
        uint32_t BarColor(const Span& span) const;
        uint32_t PopupColor(const Span& span) const;
        // A widget's title and detail line.
        TextFace TitleFace() const;
        // A widget's title: in the detail font at the title's size for a widget of figures, in the title face else.
        TextFace TitleFace(const WidgetContent& content) const;
        TextFace DetailFace() const;
        TextFace FaceOf(PopupFont font) const;
        // The icons and coin before a widget's text.
        float Leading(const WidgetContent& content) const;
        // lit: hovered or open, with a popup. Ornate: the target's frame and a lit widget's tint, on the pill row.
        // Pills: every widget's pill, zone and Earth time flat until lit.
        std::optional<WidgetBox> BoxOf(const PlacedWidget& placed, bool lit) const;
    };

    // The bar's fonts: the player's, or the theme's when they set none. Each name lasts as long as the process, for the
    // caches that keep it by reference.
    const char* BarTitleFont(const Settings& settings, BarShape shape);
    const char* BarTextFont(const Settings& settings);

    // Gil's coin, in whole pixels.
    int CoinPixels(const BarMetrics& metrics);

    struct BarPoint
    {
        float x = 0.0f, y = 0.0f;
    };
    // The corner of a text's line box that centers its glyphs' ink in box: ink is where the ink is in its image, which
    // has margin pixels around the line box. Nothing without ink.
    std::optional<BarPoint> CenteredInk(const BarRect& box, const InkBox& ink, float margin);
}
