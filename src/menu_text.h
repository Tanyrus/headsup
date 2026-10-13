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

    // What each palette's colors tell, under its card's title and on the color source that takes them.
    constexpr const char* kAggressionColorsTip =
        "Colors indicating whether an enemy will attack you, or not, and if they are a notorious monster.";
    constexpr const char* kToughnessColorsTip =
        "Colors indicating how dangerous an enemy is to you at your current level.";
    constexpr const char* kClaimColorsTip =
        "Colors indicating whether or not the mob is claimed, and who is the owner of the claim.";
    constexpr const char* kTargetingColorsTip =
        "Colors indicating whether you are targeting the entity, locked onto it, sub-targeting, etc.";
    constexpr const char* kEntityTypeColorsTip =
        "Colors indicating what type of entity the target is, or what their relation is to you.";

    const char* const kMobRowTips[kCategoryCount] = {
        "Would attack you at your level. Too Weak mobs only attack while you sit or rest.",
        "Passive, or aggressive but Too Weak for you.",
        "Not in the Phoenix data.",
        "Notorious monsters that would attack you.",
        "Notorious monsters that would leave you alone.",
        "Lottery placeholders, whose death can pop an NM, while nobody has claimed them.",
        "Claimed by you or your party, whatever else it is.",
        "Claimed by anyone outside your party, whatever else it is.",
        "Dead, until its body is gone. It keeps its name alone."}; // by Category
    const char* const kPlayerRowTips[kPlayerRowCount] = {
        "Every player the rows below do not cover.",
        "In your party.",
        "Named in the Friends card below, and not in your party.",
        "In your linkshell, and neither in your party nor a friend. The game tells a player's linkshell only when "
        "you /check them."}; // by PlayerRow
    const char* const kPetRowTips[kPetRowCount] = {"A pet of yours.", "A pet of someone in your party.",
        "A pet of a friend who is not in your party.", "Anyone else's pet."}; // by PetRow

    const char* const kSmoothnessLabels[kSmoothnessLevelCount] = {"Low", "Medium", "High", "Ultra"}; // by SmoothnessLevel

    // By PlateKind: when a line that drops its glow in combat does so.
    const char* const kGlowOffTips[kPlateKindCount] = {"No glow on this line once you or your party has claimed the mob.",
        "No glow on this line while the player has their weapon out.", "No glow on this line while you have your weapon out.",
        "No glow on this line while the NPC is fighting.", "No glow on this line while the pet is fighting."};

    // The Replace Dashes list shows the marks themselves, as the window's style draws them (menu_document).
    constexpr const char* kDotMark     = "<div class=\"dash dot\"></div>";
    constexpr const char* kSquareMark  = "<div class=\"dash square\"></div>";
    constexpr const char* kDiamondMark = "<div class=\"dash diamond\"></div>";
    const char* const kDashMarkTips[kDashMarkCount] = {"Dashes stay dashes.", "A dot.", "A square.",
        "A diamond, as at the middle of an ornament."}; // by DashMark

    // By CursorStyle: a File's label is its own name.
    const char* const kCursorStyleLabels[kCursorStyleCount] = {"Vanilla", "Triangle", "Phoenix Feather", "Chocobo", ""};
    const char* const kCursorStyleTips[kCursorStyleCount] = {
        "Uses the vanilla target cursor. WARNING: May have issues if the vanilla target frame is hidden.",
        "Uses a simple triangle target cursor, that looks similar to the vanilla target cursor.",
        "Uses a phoenix feather icon as the target cursor.", "Uses the PlayOnline mouse pointer as the target cursor.",
        "Uses a custom icon as the target cursor."};

    // What each option of a dropdown does, which it says under the pointer.
    inline const char* PlateModeTip(PlateMode mode)
    {
        static const char* const kTips[kPlateModeCount] = {"No name at all.", "HeadsUp's nameplate.",
            "The nameplate while it is your target, and no name otherwise.", "The game's own name, and nothing of HeadsUp's."};
        return kTips[static_cast<int>(mode)];
    }
    // claimRow: whether the row that would inherit is a claim's; the other that can is the placeholders'.
    inline const char* OutlineModeTip(OutlineMode mode, bool claimRow)
    {
        switch (mode)
        {
        case OutlineMode::Disabled: return "No outline.";
        case OutlineMode::Shown: return "An outline whenever it is in range.";
        case OutlineMode::WhenTargeted: return "An outline while it is your target.";
        case OutlineMode::ByNature: break;
        }
        return claimRow ? "The outline it would have unclaimed." : "The outline its aggression gives it, as any other mob's.";
    }
    inline const char* SourceTip(ColorSource source)
    {
        static const char* const kTips[kColorSourceCount] = {"A color of your own.", "The color the game gives the name.",
            kAggressionColorsTip, kToughnessColorsTip, kClaimColorsTip, kTargetingColorsTip, kEntityTypeColorsTip};
        return kTips[static_cast<int>(source)];
    }
    inline const char* OrnamentPlaceTip(OrnamentPlace place)
    {
        static const char* const kTips[kOrnamentPlaceCount] = {"No ornament.", "Over the nameplate's first line.",
            "Between the top and middle lines, while both show.", "Between the middle and bottom lines, while both show.",
            "Under the nameplate's last line."};
        return kTips[static_cast<int>(place)];
    }
    constexpr const char* kShowAlwaysTip = "The line shows whatever else holds.";
    inline const char* ConditionTip(ShowCondition condition)
    {
        static const char* const kTips[kShowConditionCount] = {"While it is your target.", "While nobody has claimed the mob.",
            "While you or your party has claimed the mob.", "While someone outside your party has claimed the mob.",
            "While it is fighting.", "While the mob is too weak to be worth experience.", "While the mob would attack you.",
            "While the mob would leave you alone.", "While the player is in your party.", "While the player is a friend.",
            "While the player is in your linkshell.", "While the pet is yours."};
        return kTips[static_cast<int>(condition)];
    }

    inline const char* RowTip(PlateKind kind, int row)
    {
        switch (kind)
        {
        case PlateKind::Mobs: return kMobRowTips[row];
        case PlateKind::Players: return kPlayerRowTips[row];
        case PlateKind::Pets: return kPetRowTips[row];
        default: return nullptr;
        }
    }

    // A cutoff as its dropdown shows it: the rank as badges read it, or Off past the end.
    inline std::string CutoffText(int rank, bool ranks)
    {
        return rank == kNoWeakRank || rank == kNoResistRank ? "Off" : RankText(rank, ranks);
    }

    // weak: the cutoff is a weakness's; a resistance's otherwise.
    inline std::string CutoffTip(int rank, bool ranks, bool weak)
    {
        const char* what = weak ? "weaknesses" : "resistances";
        if (rank == kNoWeakRank || rank == kNoResistRank) return std::string("No badges for ") + what + ".";
        return std::string("A badge for ") + what + " of " + RankText(rank, ranks) + " and stronger.";
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
