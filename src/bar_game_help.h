#pragma once

#include "bar_content.h"
#include "bar_layout.h"
#include "function_ref.h"
#include "strip_text.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace headsup
{
    constexpr size_t kHelpLineChars = 80; // a popup line's longest: the popup grows to its widest line

    // text in lines of at most chars characters (UTF-8 code points), broken at spaces; a longer word on a line alone.
    std::vector<std::string> WrapWords(const std::string& text, size_t chars);

    // What the help bar under the top bar shows, on one line: the game's caption, then its help. The caption is cut to
    // room, and the help to what the caption and the gap after it leave, each ending in "..." when more is left; cut
    // says something was, so the bar's popup has the rest.
    struct HelpBarText
    {
        std::string caption;
        std::string help;
        bool cut = false;
        bool operator==(const HelpBarText&) const = default;
    };
    HelpBarText HelpBarLine(const StripText& strip, float room, float gap,
        FunctionRef<float(const std::string&)> captionMeasure, FunctionRef<float(const std::string&)> helpMeasure);

    // The help bar's box, height tall from left and top, and where each part of its row starts: the caption a padding
    // in, the bag's count a gap after it, the help a padding after those, and a padding to the box's right edge. A
    // part of no width takes no room.
    struct HelpBarRow
    {
        BarRect box;
        float captionX = 0.0f, bagX = 0.0f, helpX = 0.0f;
    };
    HelpBarRow PlaceHelpBar(float left, float top, float height, float padding, float gap, float captionWidth,
        float bagWidth, float helpWidth);

    // bagFill: the used and total slots of the bag the caption names, as the Bags widget writes them.
    PopupContent GameHelpPopup(const std::optional<StripText>& strip, const std::optional<Span>& bagFill);
}
