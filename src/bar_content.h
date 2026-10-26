#pragma once

#include "bar_types.h"
#include "figures.h"
#include "function_ref.h"
#include "icons.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace headsup
{
    // How a piece of text is colored: by the theme, by its meaning (the badge colors in settings), or by its own color.
    enum class Tone : uint8_t
    {
        Normal,  // the bar's title color; a popup's text color
        Dim,     // the bar's subtitle color; quieter popup text
        Heading,
        Tag,     // a tag's word, in the tag's text color
        Neutral, // a value with no effect, #8d8d8d
        Weak,
        Resist,
        Immune,
        Own, // argb
    };

    // A picture read from the game's files at load, drawn in place of a span's text as tall as its letters.
    enum class GamePicture : uint8_t
    {
        None,
        PlayOnlineMail,
        PlayOnlineMessenger,
    };

    struct Span
    {
        std::string text;
        Tone tone           = Tone::Normal;
        uint32_t argb       = 0;
        GamePicture picture = GamePicture::None;
        bool operator==(const Span&) const = default;
    };

    struct WidgetContent
    {
        bool shown = false;
        std::vector<Icon> icons; // before the text
        bool coin = false;       // gil's coin before the text
        std::vector<Span> title;
        std::vector<Span> subtitle; // empty: one line
        std::vector<Icon> titleIcons, subtitleIcons; // after each line's text, as tall as its letters
        bool hasPopup = false;
        // The title holds figures, drawn in the detail font in every theme: an ornate title font's 0 reads as an O.
        bool figures = false;
        std::optional<float> progress; // 0 to 1: a bar along the widget's foot, as EXP to the next level
        bool progressBeside = false;   // the bar after the text instead, centered on the row
        // Each span of either line is a cell, and the cells at one place of their lines a column, the columns a gap
        // apart: the Conquest widget's two by two.
        bool cells = false;
        bool operator==(const WidgetContent&) const = default;
    };

    struct PopupCell
    {
        std::optional<Icon> icon;
        bool immune = false; // the icon carries a × in the immunity color
        std::vector<Span> text;
        bool star = false; // a star after the text: who holds what the cell's row is about
        bool operator==(const PopupCell&) const = default;
    };

    enum class RowKind : uint8_t
    {
        Line, // cells one after another
        Tags, // each cell framed as a tag
        Grid, // cells in columns
        Pair, // a label and its value, the value at the right edge
    };

    struct PopupRow
    {
        RowKind kind = RowKind::Line;
        int columns  = 0; // Grid
        std::vector<PopupCell> cells;
        bool operator==(const PopupRow&) const = default;
    };

    struct PopupSection
    {
        std::string heading; // empty for the rows under the title
        std::vector<PopupRow> rows;
        bool operator==(const PopupSection&) const = default;
    };

    struct PopupContent
    {
        std::optional<Icon> icon;
        std::vector<Span> title;
        std::vector<PopupSection> sections;
        bool operator==(const PopupContent&) const = default;
    };

    constexpr const char* kDot = " \xC2\xB7 "; // a middle dot between two parts of a line
    // Units of time, one place for every widget's clocks and spans.
    constexpr int64_t kMsPerSecond = 1000, kSecondsPerMinute = 60, kSecondsPerHour = 3600, kSecondsPerDay = 86400;
    constexpr int64_t kMsPerMinute = kMsPerSecond * kSecondsPerMinute, kMsPerHour = kMsPerSecond * kSecondsPerHour,
                      kMsPerDay = kMsPerSecond * kSecondsPerDay;

    // value in a printf format: Printed("%.1f ms", 16.66) is "16.7 ms".
    template <typename Value>
    std::string Printed(const char* format, Value value)
    {
        char text[48];
        std::snprintf(text, sizeof(text), format, value);
        return text;
    }
    // "2d 8h", "8h 30m", "5m 09s", "9s": the two largest units, rounded up to the whole second.
    std::string SpanText(double seconds);
    // "09:05"
    // "21:07", or "9:07 PM" on a 12-hour clock.
    // second is shown when it is 0 or more: "21:07:05", "9:07:05 PM".
    // A widget on one line: its second line before or after its title, dimmed, with its icons; or left off.
    void OneLine(WidgetContent& content, SubtitlePlace place);

    std::string ClockText(int hour, int minute, bool twentyFour, int second = -1);
    std::string HourMinute(int hour, int minute);
    // A figure running low, and one gone wrong.
    constexpr uint32_t kWarningColor = 0xFFFFB347;
    constexpr uint32_t kAlarmColor   = 0xFFFF5A5A;

    // A braced list would copy its items: an initializer_list's elements are const.
    template <typename T, typename... More>
    std::vector<T> ListOf(T first, More... more)
    {
        std::vector<T> items;
        items.reserve(1 + sizeof...(more));
        items.push_back(std::move(first));
        (items.push_back(std::move(more)), ...);
        return items;
    }

    Span Text(std::string text, Tone tone = Tone::Normal);
    Span Colored(std::string text, uint32_t argb);
    PopupCell Cell(std::vector<Span> text, std::optional<Icon> icon = std::nullopt);
    PopupCell Cell(Span text, std::optional<Icon> icon = std::nullopt);
    PopupRow Line(PopupCell cell);
    PopupRow LineOf(std::string text, Tone tone = Tone::Normal);
    // "Label value", the label dim and the value at the right edge.
    PopupRow Labeled(std::string label, Span value);
    // The same with the label in its own tone or color.
    PopupRow Labeled(Span label, Span value);
    PopupRow Grid(int columns, size_t cells);
    // text with each of its digits replaced by digit. A line's room is measured from this, with its font's widest
    // digit, so figures that tick never change the room it takes.
    std::string SteadyFigures(std::string_view text, char digit);
    // The digit that measures widest, the lowest of those that tie.
    char WidestDigit(FunctionRef<float(char)> measure);
}
