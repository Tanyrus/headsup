#pragma once

#include <array>
#include <cstdint>

namespace headsup
{
    enum class BarTheme : uint8_t
    {
        PhoenixGold,
        UmbraDark,
        PhoenixRedOrnate,
        PhoenixRedPills,
        Aurora, // glass, and a line of light under the strip
        Custom,     // the player's own palette, Settings::customPalette
        GameWindow, // the game's own window art, of the Window Type in use, and colors from it
    };
    constexpr int kBarThemeCount   = static_cast<int>(BarTheme::GameWindow) + 1;
    constexpr int kFixedThemeCount = static_cast<int>(BarTheme::Custom); // those with a palette of their own

    // What divides neighbouring widgets: the theme's own (ornate diamonds, pills none), or the player's pick.
    enum class BarSeparators : uint8_t
    {
        Theme,
        Diamonds,
        None,
    };
    constexpr int kBarSeparatorsCount = static_cast<int>(BarSeparators::None) + 1;

    // Tables by BarWidget (settings keys, menu labels, defaults) hold one entry each; kDefaultLeft, kDefaultCenter and
    // kDefaultRight in bar_placement.cpp give the bar's first order; the player's is Settings::barSections.
    enum class BarWidget : uint8_t
    {
        Zone,
        Weather,
        VanaTime,
        Target,
        Gil,
        Bags,
        EarthTime,
        GameHelp,
        Fps,
        Job,
        Connection,
        Party,
        Signet,
        Distance,
        Memory,
        Exp, // the Job widget's EXP and bar alone
    };
    constexpr int kBarWidgetCount = static_cast<int>(BarWidget::Exp) + 1;
    // Which widgets a new settings file shows, by BarWidget. to_array takes its size from the list, so a widget added
    // to BarWidget without a default here fails the assert.
    constexpr auto kBarWidgetDefaults =
        std::to_array<bool>({true, true, true, true, true, true, true, true, true, true, true, false, false, true, false, false});
    static_assert(kBarWidgetDefaults.size() == kBarWidgetCount);

    // A widget on the panel: which, and which of its copies. Copy 0 is the one every settings file has; the others are
    // there while the player keeps them on the panel.
    constexpr int kMostCopies = 4; // of one widget
    struct PanelWidget
    {
        BarWidget kind = BarWidget::Zone;
        int copy       = 0;
        constexpr PanelWidget() = default;
        constexpr PanelWidget(BarWidget widget, int number = 0) // a widget alone is its first copy
            : kind(widget), copy(number)
        {}
        bool operator==(const PanelWidget&) const = default;
    };
    // Tables by PanelWidget hold a slot for every copy of every widget.
    constexpr int kPanelSlots = kBarWidgetCount * kMostCopies;
    constexpr int SlotOf(PanelWidget widget) { return static_cast<int>(widget.kind) * kMostCopies + widget.copy; }
    constexpr PanelWidget WidgetInSlot(int slot) { return PanelWidget{static_cast<BarWidget>(slot / kMostCopies), slot % kMostCopies}; }
    // The target has one frame the bar gives way around, and game help a bar of its own: neither has copies.
    constexpr bool HasCopies(BarWidget widget) { return widget != BarWidget::Target && widget != BarWidget::GameHelp; }

    // Where a widget's second line goes when the bar is too short for two: nowhere, or beside its title.
    enum class SubtitlePlace : uint8_t
    {
        Hide,
        Before,
        After,
    };
    constexpr int kSubtitlePlaceCount = static_cast<int>(SubtitlePlace::After) + 1;

    // Where the Job widget's EXP bar goes: under its text, beside it, or nowhere.
    enum class ExpBarPlace : uint8_t
    {
        Under,
        Beside,
        Off,
    };
    constexpr int kExpBarPlaceCount = static_cast<int>(ExpBarPlace::Off) + 1;

    // What the Job widget shows larger: its jobs and levels over its EXP, the EXP over them, or the EXP alone.
    enum class JobLayout : uint8_t
    {
        JobsFirst,
        ExpFirst,
        ExpOnly,
    };
    constexpr int kJobLayoutCount = static_cast<int>(JobLayout::ExpOnly) + 1;

    // What the EXP says, any of them in this order: what is earned of the level, what is left to the next, and the
    // share earned as a percent.
    enum class ExpText : uint8_t
    {
        Earned,
        ToNext,
        Percent,
    };
    constexpr int kExpTextCount = static_cast<int>(ExpText::Percent) + 1;
    constexpr uint8_t ExpTextBit(ExpText part) { return static_cast<uint8_t>(1u << static_cast<int>(part)); }
    constexpr uint8_t kDefaultExpText = ExpTextBit(ExpText::ToNext);

    constexpr int kMinBarHeight = 24, kMaxBarHeight = 64; // pixels
    // A bar or popup text size setting's leftmost position, Auto: the size follows the bar height. Set sizes run from
    // the next pixel, the fonts' readable floor and the bar's smallest detail line.
    constexpr int kAutoBarTextSize = 8, kMaxBarTextSize = 32; // pixels
    // The popup text size's Auto sits a pixel higher: at 9 px a popup's icons come 2 px from their numbers, which reads
    // as touching, so its set sizes start at 10.
    constexpr int kAutoPopupTextSize = kAutoBarTextSize + 1;
}
