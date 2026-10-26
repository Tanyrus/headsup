#pragma once

#include "bar_types.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace headsup
{
    enum class BarSection : uint8_t
    {
        Left,   // from the left edge
        Center, // centered as a group
        Right,  // against the right edge
    };
    constexpr int kBarSectionCount = static_cast<int>(BarSection::Right) + 1;
    // Each section's widgets, left to right: the first copy of every widget the bar places in exactly one section,
    // shown or not, and each other copy the player keeps, once.
    using BarPlacement = std::array<std::vector<PanelWidget>, kBarSectionCount>;
    // Game help has its own bar under the top bar, so no section holds it.
    constexpr bool OnTheBar(BarWidget widget) { return widget != BarWidget::GameHelp; }
    constexpr int kPlacedWidgetCount = kBarWidgetCount - 1;
    // With every copy there can be: the target once, and the rest kMostCopies times.
    constexpr int kMostPlaced = 1 + (kPlacedWidgetCount - 1) * kMostCopies;

    // Zone, weather and Vana'diel time on the left, the target alone in the center, gil, bags and Earth time on the
    // right, with each newer widget in its place among them.
    const BarPlacement& DefaultPlacement();
    // A widget's name in the settings file ("zone", "vanaTime"); nullptr past the last widget.
    const char* BarWidgetName(BarWidget widget);
    // A copy's: its widget's name, and after the first copy its number from 1 ("gil:2").
    std::string PanelWidgetName(PanelWidget widget);
    // From the settings file, each section's names separated by spaces: an unknown name goes, and so do a repeat
    // (the first place a widget is named wins) and a widget no section holds; a widget named nowhere, as a widget newer than the file, goes to the
    // end of its default section.
    BarPlacement PlacementFrom(const std::array<std::string, kBarSectionCount>& names);
    std::string PlacementNames(const std::vector<PanelWidget>& section);
    // The section a widget is in and its place there; the first section's end for one the placement lacks.
    std::pair<int, size_t> PlaceOf(const BarPlacement& placement, PanelWidget widget);
    // Whether the placement holds this copy.
    bool Placed(const BarPlacement& placement, PanelWidget widget);
    // Another copy of a widget, after the last of its copies: the lowest number not placed. None for a widget
    // without copies, or with kMostCopies of them.
    std::optional<PanelWidget> AddCopy(BarPlacement& placement, BarWidget widget);
    // Takes a copy off the panel; the first copy of a widget stays. False when nothing went.
    bool RemoveCopy(BarPlacement& placement, PanelWidget widget);
    // A drop: the widget at index in section from goes before position at of section to, at past the end making it
    // the last; at counts the section as it was before the move. False, with nothing changed, for an index or a
    // position out of range or a drop where the widget already is.
    bool MoveWidget(BarPlacement& placement, BarSection from, size_t index, BarSection to, size_t at);
}
