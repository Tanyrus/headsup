#include "bar_game_help.h"

#include "bar_layout.h"
#include "utf16.h"

#include <algorithm>

namespace headsup
{
    namespace
    {
        size_t Characters(const std::string& text)
        {
            size_t count = 0;
            for (const char c : text)
                if (!ContinuesCharacter(c)) ++count;
            return count;
        }

        constexpr const char* kPopupTitle = "Game help"; // a popup's title when there is no caption
        constexpr char kSpace             = ' ';

        std::vector<std::string> Words(const std::string& text)
        {
            std::vector<std::string> words;
            for (size_t start = 0; start < text.size();)
            {
                const size_t end = std::min(text.find(kSpace, start), text.size());
                if (end > start) words.push_back(text.substr(start, end - start));
                start = end + 1;
            }
            return words;
        }
    }

    std::vector<std::string> WrapWords(const std::string& text, size_t chars)
    {
        std::vector<std::string> lines;
        std::string line;
        for (const std::string& word : Words(text))
        {
            if (!line.empty() && Characters(line) + 1 + Characters(word) > chars)
            {
                lines.push_back(line);
                line.clear();
            }
            line += (line.empty() ? "" : " ") + word;
        }
        if (!line.empty()) lines.push_back(line);
        return lines;
    }

    HelpBarText HelpBarLine(const StripText& strip, float room, float gap,
        FunctionRef<float(const std::string&)> captionMeasure, FunctionRef<float(const std::string&)> helpMeasure)
    {
        HelpBarText bar;
        // A text too wide for its room keeps what fits and "...".
        const auto fit = [&](const std::string& text, float width, FunctionRef<float(const std::string&)> measure) {
            if (measure(text) <= width) return text;
            bar.cut = true;
            return FitText(text, width, measure);
        };
        if (!strip.caption.empty()) bar.caption = fit(strip.caption, room, captionMeasure);
        std::string help; // a space between words, whatever the game pads them with
        for (const std::string& word : Words(strip.help))
            help += (help.empty() ? "" : std::string(1, kSpace)) + word;
        if (!help.empty())
            bar.help = fit(help, bar.caption.empty() ? room : room - captionMeasure(bar.caption) - gap, helpMeasure);
        return bar;
    }

    HelpBarRow PlaceHelpBar(float left, float top, float height, float padding, float gap, float captionWidth,
        float bagWidth, float helpWidth)
    {
        HelpBarRow row;
        const float start = left + padding;
        float x           = start + captionWidth;
        row.captionX      = start;
        if (bagWidth > 0.0f)
        {
            if (x > start) x += gap;
            row.bagX = x;
            x += bagWidth;
        }
        if (helpWidth > 0.0f)
        {
            if (x > start) x += padding;
            row.helpX = x;
            x += helpWidth;
        }
        row.box = BarRect{left, top, x + padding - left, height};
        return row;
    }

    PopupContent GameHelpPopup(const std::optional<StripText>& strip, const std::optional<Span>& bagFill)
    {
        PopupContent popup;
        if (!strip) return popup;
        popup.title = ListOf(Text(strip->caption.empty() ? kPopupTitle : strip->caption));
        PopupSection rows;
        for (std::string& line : WrapWords(strip->help, kHelpLineChars))
            rows.rows.push_back(LineOf(std::move(line)));
        if (bagFill) rows.rows.push_back(Labeled("Items", *bagFill));
        if (!rows.rows.empty()) popup.sections = ListOf(std::move(rows));
        return popup;
    }
}
