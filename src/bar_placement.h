#pragma once

#include "bar_types.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
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
    // Each section's widgets, left to right; every widget the bar places in exactly one section, shown or not.
    using BarPlacement = std::array<std::vector<BarWidget>, kBarSectionCount>;
    // Game help has its own bar under the top bar, so no section holds it.
    constexpr bool OnTheBar(BarWidget widget) { return widget != BarWidget::GameHelp; }
    constexpr int kPlacedWidgetCount = kBarWidgetCount - 1;

    // Zone, weather and Vana'diel time on the left, the target alone in the center, gil, bags and Earth time on the
    // right, with each newer widget in its place among them.
    const BarPlacement& DefaultPlacement();
    // A widget's name in the settings file ("zone", "vanaTime"); nullptr past the last widget.
    const char* BarWidgetName(BarWidget widget);
    // From the settings file, each section's names separated by spaces: an unknown name goes, and so do a repeat
    // (the first place a widget is named wins) and a widget no section holds; a widget named nowhere, as a widget newer than the file, goes to the
    // end of its default section.
    BarPlacement PlacementFrom(const std::array<std::string, kBarSectionCount>& names);
    std::string PlacementNames(const std::vector<BarWidget>& section);
    // A drop: the widget at index in section from goes before position at of section to, at past the end making it
    // the last; at counts the section as it was before the move. False, with nothing changed, for an index or a
    // position out of range or a drop where the widget already is.
    bool MoveWidget(BarPlacement& placement, BarSection from, size_t index, BarSection to, size_t at);
}
