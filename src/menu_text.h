#pragma once

#include "bar_placement.h"
#include "bar_types.h"
#include "icons.h"
#include "native_locate.h"
#include "plate_settings.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

// What the settings window's pages say and the small questions they ask, whichever kit draws them.
namespace headsup
{
    const char* const kBarWidgetLabels[] = {"Zone", "Weather", "Vana'diel", "Target", "Gil",
        "Bags", "Clock", "Game help", "FPS", "Job", "Connection", "Party", "Signet", "Distance"};
    const char* const kBarWidgetTips[] = {
        "The zone, its region and who holds it. Hover for the influence there and the conquest tally.",
        "The weather now and today's chance of it. Hover for the forecast.",
        "The Vana'diel day, time and moon.",
        "Your target's name, level and con. Hover a mob for its details.",
        "Gil, or another currency. Hover for every currency.",
        "How full your inventory is. Hover for every bag.",
        "Your computer's time, or another time zone's, over the server's if you like. Hover for the daily reset and "
        "conquest tally.",
        "The game's help text, in a bar of its own under the top panel.",
        "Frames per second. Hover for frame times.",
        "Jobs, levels and EXP to the next level, with a bar. Hover for EXP per hour.",
        "Bytes sent and received each second; amber or red when data stops. Hover for latency.",
        "Party size and the lowest HP and MP. Hover for each member.",
        "Time left on Signet or Sanction, while one is on.",
        "How far your target is, in yalms."};
    static_assert(std::size(kBarWidgetLabels) == kBarWidgetCount && std::size(kBarWidgetTips) == kBarWidgetCount);
    const char* const kBarSectionLabels[] = {"Left", "Center", "Right"};
    static_assert(std::size(kBarSectionLabels) == kBarSectionCount);
    constexpr const char* kThemeFont = "Theme's"; // a bar font left to its theme
    constexpr int kWidgetTabRows     = 2;

    // Which widgets have a second line, or icons, for their options to offer switching off.
    inline bool HasSecondLine(BarWidget w)
    {
        return w != BarWidget::EarthTime && w != BarWidget::Fps && w != BarWidget::Connection &&
               w != BarWidget::Distance && w != BarWidget::GameHelp;
    }
    inline bool HasIcons(BarWidget w) { return w == BarWidget::Weather || w == BarWidget::Target || w == BarWidget::Gil; }

    // The game's windows HeadsUp moves out from under the help bar, and those it has seen there.
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

    // The section a widget is in, and where.
    inline std::pair<int, size_t> PlaceOf(const BarPlacement& placement, BarWidget widget)
    {
        for (int section = 0; section < kBarSectionCount; ++section)
        {
            const std::vector<BarWidget>& list = placement[section];
            if (const auto at = std::ranges::find(list, widget); at != list.end())
                return {section, static_cast<size_t>(at - list.begin())};
        }
        return {0, 0};
    }

    constexpr const char* kDefaultFontChoice = "Default"; // a line's font: the one on the Entities page

    const char* const kMobRowTips[kCategoryCount] = {
        "Would attack you at your level. Too Weak mobs only attack while you sit or rest.",
        "Passive, or aggressive but Too Weak for you.",
        "Not in the Phoenix data.",
        "Notorious monsters that would attack you.",
        "Notorious monsters that would leave you alone.",
        "Lottery placeholders, whose death can pop an NM, while nobody has claimed them.",
        "Claimed by you or your party, whatever else it is.",
        "Claimed by anyone outside your party, whatever else it is."}; // by Category
    const char* const kPlayerRowTips[kPlayerRowCount] = {
        "Every player the rows below do not cover.",
        "In your party.",
        "Named in the Friends card below, and not in your party.",
        "In your linkshell, and neither in your party nor a friend. The game tells a player's linkshell only when "
        "you /check them."}; // by PlayerRow
    constexpr const char* kPlateModesTip =
        "Hidden: no name at all. Shown: HeadsUp's nameplate. When Targeted: the nameplate while it is your target, "
        "and no name otherwise. Expanding: its name alone, and the whole nameplate while it is your target. Vanilla: "
        "the game's own name.";
    constexpr const char* kOutlineModesTip =
        "Disabled: no outline. Shown: an outline. When Targeted: an outline while it is your target.";
    constexpr const char* kByNatureOfClaimTip       = " By Nature: the outline it would have unclaimed.";
    constexpr const char* kByNatureOfPlaceholderTip = " By Nature: the outline its aggression gives it, as any other "
                                                      "mob's.";

    inline const char* RowTip(PlateKind kind, int row)
    {
        switch (kind)
        {
        case PlateKind::Mobs: return kMobRowTips[row];
        case PlateKind::Players: return kPlayerRowTips[row];
        case PlateKind::Pets: return "Any player's pet.";
        default: return nullptr;
        }
    }

    // A cutoff as its dropdown shows it: the rank as badges read it, or Off past the end.
    inline std::string CutoffText(int rank, bool ranks)
    {
        return rank == kNoWeakRank || rank == kNoResistRank ? "Off" : RankText(rank, ranks);
    }

    inline std::string ElementCountText(int count)
    {
        if (count == 0) return "None";
        return count == kElementCount ? "All" : std::to_string(count);
    }
}
