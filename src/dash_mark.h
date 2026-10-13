#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace headsup
{
    // What a dash of a nameplate's text is drawn as: itself, or a small mark.
    enum class DashMark : uint8_t { None, Dot, Square, Diamond };
    constexpr int kDashMarkCount = 4;

    constexpr char kDash = '-';
    // In a composed text, the character standing for a dash that is drawn as a mark: control characters, which no
    // font draws and no design or name holds. One for each mark, so a text says by itself how it is drawn.
    constexpr char DashChar(DashMark mark)
    {
        constexpr char kFirstMarkChar = '\x1D'; // Dot's; Square's and Diamond's follow
        return mark == DashMark::None ? kDash : static_cast<char>(kFirstMarkChar + static_cast<int>(mark) - 1);
    }
    constexpr std::optional<DashMark> MarkOfChar(char c)
    {
        for (const DashMark mark : {DashMark::Dot, DashMark::Square, DashMark::Diamond})
            if (c == DashChar(mark)) return mark;
        return std::nullopt;
    }
    // The text with its dashes as dashes again, for a report or a log.
    std::string WithDashes(std::string text);

    // A piece of a text between its marks, or one of the marks.
    struct TextPiece
    {
        std::string text;
        DashMark mark = DashMark::None; // a mark, when not None: its text is empty
        bool operator==(const TextPiece&) const = default;
    };
    // The text in the order it is drawn: stretches of letters and the marks between them. No piece is empty.
    std::vector<TextPiece> SplitMarks(std::string_view text);
}
