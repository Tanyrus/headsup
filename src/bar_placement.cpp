#include "bar_placement.h"

#include <algorithm>
#include <iterator>
#include <optional>
#include <sstream>
#include <utility>

namespace headsup
{
    namespace
    {
        const char* const kWidgetNames[] = {"zone", "weather", "vanaTime", "target", "gil", "bags", "earthTime", "gameHelp",
            "fps", "job", "connection", "party", "signet", "distance", "memory", "exp", "conquest", "world"};
        static_assert(std::size(kWidgetNames) == kBarWidgetCount); // by BarWidget
        constexpr BarWidget kDefaultLeft[]   = {BarWidget::Zone, BarWidget::Weather, BarWidget::VanaTime, BarWidget::Job,
            BarWidget::Exp, BarWidget::Party, BarWidget::Signet, BarWidget::Distance, BarWidget::Conquest, BarWidget::World};
        constexpr BarWidget kDefaultCenter[] = {BarWidget::Target};
        constexpr BarWidget kDefaultRight[]  = {BarWidget::Gil, BarWidget::Bags, BarWidget::Connection, BarWidget::Memory,
            BarWidget::Fps, BarWidget::EarthTime};
        static_assert(std::size(kDefaultLeft) + std::size(kDefaultCenter) + std::size(kDefaultRight) ==
                      kPlacedWidgetCount);

        constexpr char kCopyMark = ':'; // between a widget's name and its copy's number in the settings file

        std::optional<PanelWidget> WidgetNamed(const std::string& name)
        {
            const size_t mark = name.find(kCopyMark);
            const std::string kind = name.substr(0, mark);
            int copy = 0;
            if (mark != std::string::npos)
            {
                const std::string number = name.substr(mark + 1);
                if (number.empty() || number.size() > 1 || number[0] < '2' || number[0] > '0' + kMostCopies) return std::nullopt;
                copy = number[0] - '1';
            }
            for (int w = 0; w < kBarWidgetCount; ++w)
                if (kind == kWidgetNames[w] && (copy == 0 || HasCopies(static_cast<BarWidget>(w))))
                    return PanelWidget{static_cast<BarWidget>(w), copy};
            return std::nullopt;
        }

        template <size_t N>
        std::vector<PanelWidget> Section(const BarWidget (&widgets)[N])
        {
            return std::vector<PanelWidget>(std::begin(widgets), std::end(widgets));
        }
    }

    std::optional<PanelWidget> PanelWidgetNamed(const std::string& name) { return WidgetNamed(name); }

    PanelGroups Combine(const BarPlacement& placement, const CombineLinks& links)
    {
        std::array<std::optional<PanelWidget>, kPanelSlots> next{};
        std::array<bool, kPanelSlots> pulled{};
        for (int slot = 0; slot < kPanelSlots; ++slot)
        {
            const PanelWidget from = WidgetInSlot(slot);
            const std::optional<PanelWidget> to = links[slot];
            if (!to || *to == from || !Combines(from.kind) || !Combines(to->kind) || !Placed(placement, from) ||
                !Placed(placement, *to) || pulled[SlotOf(*to)])
                continue;
            bool ring = false;
            for (std::optional<PanelWidget> on = to; on && !ring; on = next[SlotOf(*on)])
                ring = *on == from;
            if (ring) continue;
            next[slot]          = to;
            pulled[SlotOf(*to)] = true;
        }
        PanelGroups groups{{}, AloneGroups()};
        for (int s = 0; s < kBarSectionCount; ++s)
            for (const PanelWidget head : placement[s])
            {
                if (pulled[SlotOf(head)]) continue;
                for (std::optional<PanelWidget> on = head; on; on = next[SlotOf(*on)])
                {
                    groups.placement[s].push_back(*on);
                    groups.group[SlotOf(*on)] = head;
                }
            }
        return groups;
    }

    const std::array<PanelWidget, kPanelSlots>& AloneGroups()
    {
        static const std::array<PanelWidget, kPanelSlots> alone = [] {
            std::array<PanelWidget, kPanelSlots> each{};
            for (int slot = 0; slot < kPanelSlots; ++slot)
                each[slot] = WidgetInSlot(slot);
            return each;
        }();
        return alone;
    }

    std::vector<PanelWidget> Members(const PanelGroups& groups, PanelWidget head)
    {
        std::vector<PanelWidget> members;
        for (const std::vector<PanelWidget>& section : groups.placement)
            for (const PanelWidget w : section)
                if (groups.group[SlotOf(w)] == head) members.push_back(w);
        return members;
    }

    const BarPlacement& DefaultPlacement()
    {
        static const BarPlacement placement{Section(kDefaultLeft), Section(kDefaultCenter), Section(kDefaultRight)};
        return placement;
    }

    std::string PanelWidgetName(PanelWidget widget)
    {
        const char* name = BarWidgetName(widget.kind);
        if (name == nullptr) return {};
        return widget.copy == 0 ? std::string(name) : std::string(name) + kCopyMark + std::to_string(widget.copy + 1);
    }

    const char* BarWidgetName(BarWidget widget)
    {
        const int w = static_cast<int>(widget);
        return w >= 0 && w < kBarWidgetCount ? kWidgetNames[w] : nullptr;
    }

    BarPlacement PlacementFrom(const std::array<std::string, kBarSectionCount>& names)
    {
        BarPlacement placement;
        std::array<bool, kPanelSlots> placed{};
        for (int s = 0; s < kBarSectionCount; ++s)
        {
            std::istringstream words(names[s]);
            for (std::string word; words >> word;)
            {
                const std::optional<PanelWidget> w = WidgetNamed(word);
                if (w && OnTheBar(w->kind) && !std::exchange(placed[SlotOf(*w)], true)) placement[s].push_back(*w);
            }
        }
        const BarPlacement& home = DefaultPlacement();
        for (int s = 0; s < kBarSectionCount; ++s)
            for (const PanelWidget w : home[s])
                if (!placed[SlotOf(w)]) placement[s].push_back(w);
        return placement;
    }

    std::string PlacementNames(const std::vector<PanelWidget>& section)
    {
        std::string text;
        for (const PanelWidget w : section)
            text += (text.empty() ? "" : " ") + PanelWidgetName(w);
        return text;
    }

    std::pair<int, size_t> PlaceOf(const BarPlacement& placement, PanelWidget widget)
    {
        for (int s = 0; s < kBarSectionCount; ++s)
            if (const auto at = std::ranges::find(placement[s], widget); at != placement[s].end())
                return {s, static_cast<size_t>(at - placement[s].begin())};
        return {0, placement[0].size()};
    }

    bool Placed(const BarPlacement& placement, PanelWidget widget)
    {
        return std::ranges::any_of(placement, [&](const auto& section) { return std::ranges::find(section, widget) != section.end(); });
    }

    std::optional<PanelWidget> AddCopy(BarPlacement& placement, BarWidget widget)
    {
        if (!HasCopies(widget)) return std::nullopt;
        int copy = 1;
        while (copy < kMostCopies && Placed(placement, PanelWidget{widget, copy}))
            ++copy;
        if (copy == kMostCopies) return std::nullopt;
        // After the last copy there is, in its section.
        std::pair<int, size_t> after = PlaceOf(placement, widget);
        for (int c = 1; c < kMostCopies; ++c)
            if (Placed(placement, PanelWidget{widget, c})) after = PlaceOf(placement, PanelWidget{widget, c});
        std::vector<PanelWidget>& section = placement[after.first];
        const PanelWidget added{widget, copy};
        section.insert(section.begin() + static_cast<std::ptrdiff_t>(std::min(after.second + 1, section.size())), added);
        return added;
    }

    bool RemoveCopy(BarPlacement& placement, PanelWidget widget)
    {
        if (widget.copy == 0) return false;
        for (std::vector<PanelWidget>& section : placement)
            if (const auto at = std::ranges::find(section, widget); at != section.end())
            {
                section.erase(at);
                return true;
            }
        return false;
    }

    bool MoveWidget(BarPlacement& placement, BarSection from, size_t index, BarSection to, size_t at)
    {
        std::vector<PanelWidget>& source = placement[static_cast<int>(from)];
        std::vector<PanelWidget>& target = placement[static_cast<int>(to)];
        if (index >= source.size() || at > target.size()) return false;
        if (from == to && (at == index || at == index + 1)) return false;
        const PanelWidget widget = source[index];
        source.erase(source.begin() + static_cast<std::ptrdiff_t>(index));
        if (from == to && at > index) --at;
        target.insert(target.begin() + static_cast<std::ptrdiff_t>(at), widget);
        return true;
    }
}
