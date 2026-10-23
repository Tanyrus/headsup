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
        // How a widget's box is drawn, or nothing for none. lit: hovered or open, with a popup. Ornate: the target's
        // frame and a lit widget's tint. Pills: every widget's pill, zone and Earth time flat until lit.
        std::optional<BoxStyle> BoxOf(BarWidget widget, bool lit) const;
    };

    // Whether the game's window art is drawn: its theme, a Custom theme of its shape, or the settings window's theme.
    bool UsesGameArt(const Settings& settings);

    // What Game Window looks as while the game's art is not read.
    constexpr BarTheme kGameArtFallback = BarTheme::PhoenixGold;
    // The palette the bar is drawn with: the picked theme's, or the player's own for Custom.
    const BarPalette& PaletteFor(const Settings& settings);

    // The bar's fonts: the player's, or the theme's when they set none. Each name lasts as long as the process, for the
    // caches that keep it by reference.
    const char* BarTitleFont(const Settings& settings, BarShape shape);
    const char* BarTextFont(const Settings& settings);

    // Gil's coin, in whole pixels.
    int CoinPixels(const BarMetrics& metrics);
    constexpr uint32_t kCoinColor  = 0xFFE8C35A;
    constexpr float kCapScale      = 1.4f;  // an end cap's diamond, in diamonds
    constexpr float kLineIconGap   = 2.0f;  // pixels between the icons after a line's text
    constexpr float kProgressInset = 2.0f;  // pixels from the pill row's foot, inside its border
    // A widget's progress bar: a thin track along the pill row's foot, or a taller one beside the text.
    float ProgressHeight(const BarMetrics& metrics, bool beside);
    // A bar beside the text is this long.
    float BesideWidth(const BarMetrics& metrics);
    // What a widget's title and detail line can hold, for how far their ink reaches: capitals, figures (taller
    // than the capitals in Marcellus, and some descend), a "[S]" zone's brackets, the bags' slash, descenders
    // (Marcellus's Q and J too) and commas; the detail line's ascenders, descenders and comma. Popup text holds
    // what a detail line does.
    constexpr const char* kTitleGlyphs  = "H0123456789[]/gjpqyQJ,";
    constexpr const char* kDetailGlyphs = "Hl0123456789%gjpqy,";
    // The row the palette draws under a strip, when it has one.
    float UnderLine(const BarPalette& palette);
    // Where the top bar's strip and its under line end.
    float BarBottom(const BarPalette& palette, const BarMetrics& metrics);
    // Where the help bar starts: the bar's gap under the top bar, so the two read as separate bars.
    float HelpBarTop(const BarPalette& palette, const BarMetrics& metrics);
    // As far from the screen's left edge as from the top bar.
    float HelpBarLeft(const BarMetrics& metrics);
    // Clear above and below the help bar's row.
    float HelpBarPad(const BarMetrics& metrics);
    // The help's text: the detail font at a widget's title size, so the row is one line of the bar's text.
    TextFace HelpFace(const BarLook& look);
    // The widest the help bar gets: half the screen, or its least width on a narrow one.
    float HelpBarWidth(float screenWidth);
    // The boxes a popup and the help bar are drawn with: a popup's panel with its outer border, a section's card (a
    // lift on the panel, rounded as a pill theme's panel is), a tag's pill height tall (the ornate themes keep their
    // gold edge around it), and the help bar's panel, bordered all round.
    BoxStyle PopupPanel(const BarLook& look);
    BoxStyle PopupCard(const BarLook& look);
    BoxStyle PopupTag(const BarLook& look, float height);
    BoxStyle HelpBarPanel(const BarLook& look);
    constexpr uint32_t kStarColor     = kCoinColor; // a popup's star, in the coin's gold
    constexpr float kShadowStrength   = 0.6f;       // the bar's text shadow, of the mockup's

}
