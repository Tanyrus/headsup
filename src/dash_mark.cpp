#include "dash_mark.h"

#include <algorithm>

namespace headsup
{
    std::string WithDashes(std::string text)
    {
        std::ranges::replace_if(text, [](char c) { return MarkOfChar(c).has_value(); }, kDash);
        return text;
    }

    std::vector<TextPiece> SplitMarks(std::string_view text)
    {
        std::vector<TextPiece> pieces;
        std::string letters;
        const auto flush = [&] {
            if (!letters.empty()) pieces.push_back(TextPiece{std::move(letters), DashMark::None});
            letters.clear();
        };
        for (const char c : text)
        {
            const std::optional<DashMark> mark = MarkOfChar(c);
            if (!mark)
            {
                letters += c;
                continue;
            }
            flush();
            pieces.push_back(TextPiece{{}, *mark});
        }
        flush();
        return pieces;
    }
}
