#pragma once

#include "bar_theme.h"
#include "bar_widgets.h"
#include "function_ref.h"

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace headsup
{
    enum class PopupFont : uint8_t
    {
        Title,   // TitleFont at 1.35 popupText
        Text,    // the detail font at popupText
        Heading, // TitleFont at heading size; uppercase and letter-spaced in the detail font for pills
        Pin,     // the pinned mark: a heading's font and size, set solid
    };

    struct PopupPiece
    {
        enum class Kind : uint8_t
        {
            Text,
            Icon,
            ImmuneMark, // a ×, its ink centered in the piece: inside an immune icon, at its bottom right
            Star,       // a starred cell's star, after its text
            Rule,       // an ornate heading's fading line
            Tag,        // a tag's frame, under its text
            Card,       // a section's rounded ground, under its heading and rows
            Fold,       // a fold row's arrow: pointing right while closed, down while open
            Socket,     // the sunken ground under the rows an open fold row holds
            Tab,        // a card's tab's chip, under its name; open while picked
        };
        Kind kind = Kind::Text;
        float x = 0.0f, y = 0.0f, width = 0.0f, height = 0.0f; // from the popup's top-left corner
        Span span;                                             // Text
        PopupFont font = PopupFont::Text;
        Icon icon      = Icon::AggroNQ; // Icon
        size_t line    = 0;             // in the layout's lines
        bool open      = false;         // Fold
        bool right     = false;         // Text: drawn against its box's right, as a pair's value is
    };

    // Where a fold row is, from the popup's top-left corner: the whole row across its card, to click; or a tab's chip,
    // its key the card's heading.
    struct PopupFold
    {
        std::string key;
        float x = 0.0f, y = 0.0f, width = 0.0f, height = 0.0f;
        size_t line = 0;
        std::string tab{}; // a tab's name; empty for a fold row
    };

    // The title, or a section's card: a popup cut short keeps or drops each whole.
    struct PopupLine
    {
        float bottom = 0.0f; // from the popup's top
    };

    struct PopupLayout
    {
        float width = 0.0f, height = 0.0f;
        std::vector<PopupPiece> pieces; // line by line
        std::vector<PopupLine> lines;   // top to bottom
        std::vector<PopupFold> folds;
    };

    struct TextSize
    {
        float width = 0.0f, height = 0.0f;     // the line box GDI draws
        float inkTop = 0.0f, inkBottom = 0.0f; // its font's ink in it, from its top: capitals and figures to descenders
        float overhang = 0.0f;                 // how far its ink reaches past width: a last letter can pass its advance
    };
    using SpanMeasure = FunctionRef<TextSize(const Span&, PopupFont)>;

    // The title, then each section with a heading as a card holding it and its rows; rows without a heading sit
    // under the title, on the panel. A pair's value ends at its card's right edge. When the cards stacked would run
    // taller than kTwoColumnsAbove popup text sizes, they flow into two columns as even as their order allows, and
    // into more when two would run taller than room, a card too tall for it going on in the next under its heading.
    // The pinned mark ("pinned", in the heading tone) sits at the top right when pinned.
    constexpr float kTwoColumnsAbove = 24.0f;
    PopupLayout LayoutPopup(const PopupContent& content, const BarMetrics& metrics, BarShape shape, bool pinned,
                            SpanMeasure measure, float room = std::numeric_limits<float>::infinity());
    // A layout taller than height keeps the title and the cards that end padding or more above it, and shrinks to
    // them with its padding under the lowest.
    void CutPopup(PopupLayout& layout, float height, float padding);
}
