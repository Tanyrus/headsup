#include "bar_content.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace headsup
{
    Span Text(std::string text, Tone tone) { return Span{std::move(text), tone, 0}; }

    Span Colored(std::string text, uint32_t argb) { return Span{std::move(text), Tone::Own, argb}; }

    PopupCell Cell(std::vector<Span> text, std::optional<Icon> icon) { return PopupCell{icon, false, std::move(text)}; }

    PopupCell Cell(Span text, std::optional<Icon> icon) { return Cell(ListOf(std::move(text)), icon); }

    PopupRow Line(PopupCell cell) { return PopupRow{RowKind::Line, 0, ListOf(std::move(cell))}; }

    PopupRow LineOf(std::string text, Tone tone) { return Line(Cell(Text(std::move(text), tone))); }

    PopupRow Labeled(std::string label, Span value)
    {
        return Labeled(Text(std::move(label), Tone::Dim), std::move(value));
    }

    PopupRow Labeled(Span label, Span value)
    {
        label.text += " ";
        return PopupRow{RowKind::Pair, 0, ListOf(Cell(std::move(label)), Cell(std::move(value)))};
    }

    PopupRow Grid(int columns, size_t cells)
    {
        PopupRow row{RowKind::Grid, columns, {}};
        row.cells.reserve(cells);
        return row;
    }

    std::string Grouped(uint32_t value)
    {
        std::string digits = std::to_string(value), out;
        for (size_t i = 0; i < digits.size(); ++i)
        {
            if (i > 0 && (digits.size() - i) % 3 == 0) out += ',';
            out += digits[i];
        }
        return out;
    }

    std::string SteadyFigures(std::string_view text, char digit)
    {
        std::string steady(text);
        for (char& c : steady)
            if (c >= '0' && c <= '9') c = digit;
        return steady;
    }

    char WidestDigit(FunctionRef<float(char)> measure)
    {
        char widest = '0';
        float most  = measure(widest);
        for (char digit = '1'; digit <= '9'; ++digit)
            if (const float width = measure(digit); width > most)
            {
                widest = digit;
                most   = width;
            }
        return widest;
    }

    std::string SpanText(double seconds)
    {
        const auto total = static_cast<long long>(std::ceil(std::max(0.0, seconds)));
        const long long days = total / kSecondsPerDay, hours = total % kSecondsPerDay / kSecondsPerHour;
        const long long minutes = total % kSecondsPerHour / kSecondsPerMinute, rest = total % kSecondsPerMinute;
        char text[32];
        if (days > 0)
            std::snprintf(text, sizeof(text), "%lldd %lldh", days, hours);
        else if (hours > 0)
            std::snprintf(text, sizeof(text), "%lldh %02lldm", hours, minutes);
        else if (minutes > 0)
            std::snprintf(text, sizeof(text), "%lldm %02llds", minutes, rest);
        else
            std::snprintf(text, sizeof(text), "%llds", rest);
        return text;
    }

    void OneLine(WidgetContent& content, SubtitlePlace place)
    {
        std::vector<Span> second = std::move(content.subtitle);
        content.subtitle.clear();
        std::vector<Icon> icons = std::move(content.subtitleIcons);
        content.subtitleIcons.clear();
        if (place == SubtitlePlace::Hide || second.empty()) return;
        for (Span& s : second)
            if (s.tone == Tone::Normal) s.tone = Tone::Dim;
        std::vector<Span>& title = content.title;
        if (place == SubtitlePlace::Before)
        {
            second.back().text += " ";
            title.insert(title.begin(), second.begin(), second.end());
        }
        else
        {
            if (!title.empty()) title.back().text += " ";
            title.insert(title.end(), second.begin(), second.end());
        }
        content.titleIcons.insert(content.titleIcons.end(), icons.begin(), icons.end());
    }

    std::string ClockText(int hour, int minute, bool twentyFour, int second)
    {
        char seconds[16] = "";
        if (second >= 0) std::snprintf(seconds, sizeof(seconds), ":%02d", second);
        if (twentyFour) return HourMinute(hour, minute) + seconds;
        constexpr int kHalfDay = 12;
        char text[40];
        std::snprintf(text, sizeof(text), "%d:%02d%s %s", hour % kHalfDay == 0 ? kHalfDay : hour % kHalfDay, minute,
            seconds, hour < kHalfDay ? "AM" : "PM");
        return text;
    }

    std::string HourMinute(int hour, int minute)
    {
        char text[8];
        std::snprintf(text, sizeof(text), "%02d:%02d", hour, minute);
        return text;
    }
}
