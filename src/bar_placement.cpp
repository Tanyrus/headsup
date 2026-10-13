#include "bar_placement.h"

#include <iterator>
#include <optional>
#include <sstream>
#include <utility>

namespace headsup
{
    namespace
    {
        const char* const kWidgetNames[] = {"zone", "weather", "vanaTime", "target", "gil", "bags", "earthTime", "gameHelp",
            "fps", "job", "connection", "party", "signet", "distance"};
        static_assert(std::size(kWidgetNames) == kBarWidgetCount); // by BarWidget
        constexpr BarWidget kDefaultLeft[]   = {BarWidget::Zone, BarWidget::Weather, BarWidget::VanaTime, BarWidget::Job,
            BarWidget::Party, BarWidget::Signet, BarWidget::Distance};
        constexpr BarWidget kDefaultCenter[] = {BarWidget::Target};
        constexpr BarWidget kDefaultRight[]  = {BarWidget::Gil, BarWidget::Bags, BarWidget::Connection, BarWidget::Fps,
            BarWidget::EarthTime};
        static_assert(std::size(kDefaultLeft) + std::size(kDefaultCenter) + std::size(kDefaultRight) ==
                      kPlacedWidgetCount);

        std::optional<BarWidget> WidgetNamed(const std::string& name)
        {
            for (int w = 0; w < kBarWidgetCount; ++w)
                if (name == kWidgetNames[w]) return static_cast<BarWidget>(w);
            return std::nullopt;
        }
    }

    const BarPlacement& DefaultPlacement()
    {
        static const BarPlacement placement{std::vector<BarWidget>(std::begin(kDefaultLeft), std::end(kDefaultLeft)),
            std::vector<BarWidget>(std::begin(kDefaultCenter), std::end(kDefaultCenter)),
            std::vector<BarWidget>(std::begin(kDefaultRight), std::end(kDefaultRight))};
        return placement;
    }

    const char* BarWidgetName(BarWidget widget)
    {
        const int w = static_cast<int>(widget);
        return w >= 0 && w < kBarWidgetCount ? kWidgetNames[w] : nullptr;
    }

    BarPlacement PlacementFrom(const std::array<std::string, kBarSectionCount>& names)
    {
        BarPlacement placement;
        std::array<bool, kBarWidgetCount> placed{};
        for (int s = 0; s < kBarSectionCount; ++s)
        {
            std::istringstream words(names[s]);
            for (std::string word; words >> word;)
            {
                const std::optional<BarWidget> w = WidgetNamed(word);
                if (w && OnTheBar(*w) && !std::exchange(placed[static_cast<int>(*w)], true)) placement[s].push_back(*w);
            }
        }
        const BarPlacement& home = DefaultPlacement();
        for (int s = 0; s < kBarSectionCount; ++s)
            for (const BarWidget w : home[s])
                if (!placed[static_cast<int>(w)]) placement[s].push_back(w);
        return placement;
    }

    std::string PlacementNames(const std::vector<BarWidget>& section)
    {
        std::string text;
        for (const BarWidget w : section)
            text += (text.empty() ? "" : " ") + std::string(BarWidgetName(w));
        return text;
    }

    bool MoveWidget(BarPlacement& placement, BarSection from, size_t index, BarSection to, size_t at)
    {
        std::vector<BarWidget>& source = placement[static_cast<int>(from)];
        std::vector<BarWidget>& target = placement[static_cast<int>(to)];
        if (index >= source.size() || at > target.size()) return false;
        if (from == to && (at == index || at == index + 1)) return false;
        const BarWidget widget = source[index];
        source.erase(source.begin() + static_cast<std::ptrdiff_t>(index));
        if (from == to && at > index) --at;
        target.insert(target.begin() + static_cast<std::ptrdiff_t>(at), widget);
        return true;
    }
}
