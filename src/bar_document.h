#pragma once

#include "bar_content.h"
#include "bar_game_help.h"
#include "bar_look.h"
#include "bar_theme.h"
#include "bar_types.h"
#include "icons.h"
#include "markup.h"
#include "plate_image.h"
#include "popup_layout.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace headsup
{
    // The images the bar's documents name, drawn by the bar's own image code when RmlUi asks for them: an icon by
    // its number, a theme's strip column (its rows and the rows fading below), a diamond or a coin by its pixels.
    constexpr std::string_view kHostSource = "hu:";
    std::string IconSource(Icon icon);
    std::string StripSource(const BarPalette& palette, int rows, int belowRows);
    std::string DiamondSource(uint32_t argb, int pixels);
    std::string CoinSource(int pixels);
    std::string StarSource(int pixels);
    std::string ResetSource(uint32_t argb, int pixels); // the circular arrow that puts a color back
    // The image a name stands for; nothing for a name that is none of the above.
    std::optional<Image> HostImage(std::string_view name);

    // A line of a widget as RmlUi lays it out: its line box's height, where its top goes from the strip's top (over
    // a detail line, and alone), and how far under that top the icons after its text start.
    struct BarLinePlace
    {
        int height     = 0;
        float top      = 0.0f, aloneTop = 0.0f;
        float iconDrop = 0.0f;
        bool operator==(const BarLinePlace&) const = default;
    };
    // Where the widgets' lines go so their ink lands on the rows the bar's text row gives: a title in the title
    // font, a title of figures in the detail font, and a detail line.
    struct BarTextPlace
    {
        BarLinePlace title, figures, detail;
        bool operator==(const BarTextPlace&) const = default;
    };

    // A widget's own text, where its fonts or size are not the bar's: those fonts, and the text row and places they
    // come to.
    struct WidgetText
    {
        PanelWidget widget;
        const char* titleFont  = nullptr; // interned, as the bar's are
        const char* detailFont = nullptr;
        TextRow lines;
        BarTextPlace place;
        bool operator==(const WidgetText&) const = default;
    };

    // The bar's markup and style: the strip across the screen, the ornate caps, a divider for each pair of
    // neighbours and every placed widget's element, all sized from the look, with the text rules of each widget in
    // own under its id. The widgets and dividers carry the hidden class and no place: the plugin shows and places them.
    std::string BarDocument(const BarLook& look, const BarTextPlace& place, const std::vector<WidgetText>& own = {});
    std::string WidgetId(PanelWidget widget);
    // A widget's padding on each side as its element takes it, for a room from its edge to what it shows: inside a
    // pill's or a frame's border, or past a plain ornate widget's margin.
    float SidePadding(const BarLook& look, PanelWidget widget, float room);
    std::string DividerId(int divider);
    constexpr int kDividerCount       = kMostPlaced - 1;
    constexpr const char* kStripId    = "strip";
    constexpr const char* kRightCapId = "cap-right";
    constexpr const char* kHiddenClass = "hidden";
    constexpr const char* kLitClass    = "lit"; // hovered or open, with a popup
    // A widget's inner markup: its icons, coin, title and detail lines in their colors, and its progress bar.
    std::string WidgetRml(const WidgetContent& content, const BarLook& look);

    // Game help's own bar under the top bar: its box when it has a background, and its caption, the bag's count and
    // its help where the row puts them, each centered on the row. Nothing for a row with no box.
    struct HelpBarParts
    {
        HelpBarRow row;
        std::string caption, help;
        std::optional<Span> bag;
    };
    std::string HelpBarRml(const HelpBarParts& parts, const BarLook& look, const BarTextPlace& place);
    constexpr const char* kHelpId = "help";

    // A popup's text faces, by PopupFont, as RmlUi sets them: the look's faces in whole pixels, each with the
    // height of its line box and how far under a laid out line's top that box starts, so its baseline is the
    // layout's.
    struct PopupFace
    {
        std::string family;
        int pixels = 0, line = 0, tracking = 0;
        float drop = 0.0f;
        bool operator==(const PopupFace&) const = default;
    };
    constexpr int kPopupFontCount = static_cast<int>(PopupFont::Pin) + 1;
    using PopupFaces = std::array<PopupFace, kPopupFontCount>;
    // The popups' document: an empty popup over the whole screen that takes no clicks, styled from the look.
    std::string PopupDocument(const BarLook& look, const PopupFaces& faces);
    constexpr const char* kOpenClass = "open"; // on a popup while it shows, so what it does as it opens plays each time
    constexpr const char* kPopupId = "popup";
    // A laid out popup's markup, from its top left corner: its panel, then each piece where the layout put it.
    std::string PopupRml(const PopupLayout& layout, const BarLook& look, const PopupFaces& faces);
}
