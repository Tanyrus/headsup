#pragma once

#include "bar_placement.h"
#include "bar_types.h"
#include "icons.h"
#include "native_locate.h"
#include "outline_math.h"
#include "plate_settings.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

// What the settings window's pages say and the small questions they ask.
namespace headsup
{
    constexpr int kWidgetTabsPerRow  = 7; // the most a row of the Widgets page's tabs holds: more, and names are cut

    // Which widgets have a second line, or icons, for their options to offer switching off.
    inline bool HasSecondLine(BarWidget w)
    {
        return w != BarWidget::EarthTime && w != BarWidget::Fps && w != BarWidget::Connection &&
               w != BarWidget::Distance && w != BarWidget::GameHelp;
    }
    inline bool HasIcons(BarWidget w) { return w == BarWidget::Weather || w == BarWidget::Target || w == BarWidget::Gil; }

    // Whether text holds part, in any case.
    inline bool Matches(std::string_view text, const char* part)
    {
        const std::string_view p(part);
        if (p.empty()) return true;
        const auto lower = [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); };
        for (size_t at = 0; at + p.size() <= text.size(); ++at)
            if (std::equal(p.begin(), p.end(), text.begin() + static_cast<std::ptrdiff_t>(at),
                    [&](char a, char b) { return lower(a) == lower(b); }))
                return true;
        return false;
    }

    // A window as the menu names it: HeadsUp's name for it, or the game's.
    inline std::string WindowName(const std::string& window)
    {
        const char* label = WindowLabel(window);
        return label != nullptr ? label : window;
    }

    // The Replace Dashes list shows the marks themselves, as the window's style draws them (menu_document).
    constexpr const char* kDotMark     = "<div class=\"dash dot\"></div>";
    constexpr const char* kSquareMark  = "<div class=\"dash square\"></div>";
    constexpr const char* kDiamondMark = "<div class=\"dash diamond\"></div>";
    // A cutoff as its dropdown shows it: the rank, which no chance stands for without a mob, or Off past the end.
    inline std::string CutoffText(int rank)
    {
        return rank == kNoWeakRank || rank == kNoResistRank ? "Off" : std::to_string(rank);
    }

    // weak: the cutoff is a weakness's; a resistance's otherwise.
    inline std::string CutoffTip(int rank, bool weak)
    {
        const char* what = weak ? "weaknesses" : "resistances";
        if (rank == kNoWeakRank || rank == kNoResistRank) return std::string("No badges for ") + what + ".";
        return std::string("A badge for ") + what + " of rank " + std::to_string(rank) + " and stronger.";
    }

    inline std::string ElementCountText(int count)
    {
        if (count == 0) return "None";
        return count == kElementCount ? "All" : std::to_string(count);
    }

    inline std::string ElementCountTip(int count)
    {
        if (count == 0) return "No element badges.";
        if (count == kElementCount) return "Every element past the cutoffs.";
        return "The " + std::to_string(count) + " elements farthest from neutral.";
    }
}
