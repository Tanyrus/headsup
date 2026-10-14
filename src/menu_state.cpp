#include "menu_state.h"

#include "settings.h"

#include <cctype>
#include <algorithm>
#include <array>

namespace headsup
{
    bool SaveDue(std::optional<double> changedAt, double now, bool windowOpen)
    {
        return changedAt && (!windowOpen || now - *changedAt >= kSaveSettleSeconds);
    }

    std::optional<PlateKind> KindOfPage(Page page)
    {
        switch (page)
        {
        case Page::Mobs: return PlateKind::Mobs;
        case Page::Players: return PlateKind::Players;
        case Page::Self: return PlateKind::Self;
        case Page::Npcs: return PlateKind::Npcs;
        case Page::Pets: return PlateKind::Pets;
        default: return std::nullopt;
        }
    }

    std::string TitleCaseText(std::string_view text)
    {
        constexpr std::array<std::string_view, 10> kSmallWords = {"and", "or", "of", "the", "to", "in", "on", "with", "by", "is"};
        std::string result(text);
        for (size_t at = 0; at < result.size();)
        {
            size_t end = at;
            while (end < result.size() && result[end] != ' ' && result[end] != '-') ++end;
            std::string word(result, at, end - at);
            for (char& c : word) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (at == 0 || std::ranges::find(kSmallWords, word) == kSmallWords.end())
                result[at] = static_cast<char>(std::toupper(static_cast<unsigned char>(result[at])));
            at = end < result.size() ? end + 1 : end;
        }
        return result;
    }

    std::vector<BarWidget> WidgetTabs()
    {
        std::vector<BarWidget> tabs;
        for (int w = 0; w < kBarWidgetCount; ++w)
            if (OnTheBar(static_cast<BarWidget>(w))) tabs.push_back(static_cast<BarWidget>(w));
        return tabs;
    }

    int TabsPerRow(int count, int rows) { return std::max(1, (count + std::max(rows, 1) - 1) / std::max(rows, 1)); }

    int ClockZoneChoice(const std::optional<ClockZone>& zone) { return zone ? static_cast<int>(*zone) + 1 : 0; }

    std::optional<ClockZone> ClockZoneOfChoice(int choice)
    {
        if (choice < 1 || choice >= kClockZoneChoices) return std::nullopt;
        return static_cast<ClockZone>(choice - 1);
    }

    std::string ClockZoneChoiceName(int choice, const WindowLayout& layout)
    {
        const std::optional<ClockZone> zone = ClockZoneOfChoice(choice);
        return zone ? ClockZoneName(*zone) : layout.WordOf(WindowWord::Local).name;
    }

    std::string ShowSummary(const LineShow& show, const std::vector<ShowCondition>& offered, const WindowLayout& layout)
    {
        if (show.always) return layout.WordOf(WindowWord::Always).name;
        const auto names = layout.OptionsOf(WindowList::ShowCondition);
        int checked      = 0;
        const char* only = layout.WordOf(WindowWord::Never).name;
        for (const ShowCondition condition : offered)
        {
            if ((show.when & ConditionBit(condition)) == 0) continue;
            ++checked;
            only = names[static_cast<size_t>(condition)].name;
        }
        return checked > 1 ? Filled(layout.WordOf(WindowWord::Conditions).name, kCountMark, std::to_string(checked)) : only;
    }

    bool ToggleCondition(LineShow& show, const std::vector<ShowCondition>& offered, int row)
    {
        if (row < 0 || row > static_cast<int>(offered.size())) return false;
        if (row == 0)
        {
            show.always = !show.always;
            return true;
        }
        const uint32_t bit = ConditionBit(offered[static_cast<size_t>(row) - 1]);
        if (show.always)
        {
            show.always = false;
            show.when   = 0;
            for (const ShowCondition condition : offered)
                show.when |= ConditionBit(condition);
        }
        show.when ^= bit;
        return true;
    }

    std::vector<int> SyncTargets(const KindSettings& kind, int line)
    {
        std::vector<int> targets;
        for (int other = 0; other < kPlateLines; ++other)
        {
            int at = other;
            for (int hop = 0; hop < kPlateLines && at != line && at >= 0 && at < kPlateLines; ++hop)
                at = kind.lines[at].syncWith;
            if (at != line) targets.push_back(other);
        }
        return targets;
    }

    int SyncEntry(const std::vector<int>& targets, int syncWith)
    {
        const auto at = std::ranges::find(targets, syncWith);
        return at == targets.end() ? 0 : static_cast<int>(at - targets.begin()) + 1;
    }

    int SyncOfEntry(const std::vector<int>& targets, int entry)
    {
        return entry >= 1 && entry <= static_cast<int>(targets.size()) ? targets[static_cast<size_t>(entry) - 1] : kNoSync;
    }

    std::string SyncEntryLabel(const std::vector<int>& targets, int entry, const WindowLayout& layout)
    {
        const int line = SyncOfEntry(targets, entry);
        if (line == kNoSync) return layout.WordOf(WindowWord::LineStylesDoNotSync).name;
        return Filled(layout.WordOf(WindowWord::LineStylesSyncWith).name, kLineMark,
            layout.OptionsOf(WindowList::PlateLines)[static_cast<size_t>(line)].name);
    }

    std::string SyncEntryTip(const std::vector<int>& targets, int entry, const WindowLayout& layout)
    {
        const int line = SyncOfEntry(targets, entry);
        if (line == kNoSync) return std::string(Said(layout.WordOf(WindowWord::LineStylesDoNotSync).tip));
        return Filled(Said(layout.WordOf(WindowWord::LineStylesSyncWith).tip), kLineMark,
            layout.OptionsOf(WindowList::PlateLines)[static_cast<size_t>(line)].name);
    }

    namespace
    {
        // The entry the fonts start at: past the unset choice, when there is one.
        int FirstFont(const char* unset) { return unset != nullptr ? 1 : 0; }
    }

    int FontEntryCount(const std::vector<std::string>& fonts, const char* unset)
    {
        return static_cast<int>(fonts.size()) + FirstFont(unset);
    }

    int FontEntry(const std::vector<std::string>& fonts, const char* unset, const std::string& font)
    {
        if (font.empty()) return unset != nullptr ? 0 : kNoFontEntry;
        const auto at = std::ranges::find(fonts, font);
        return at == fonts.end() ? kNoFontEntry : static_cast<int>(at - fonts.begin()) + FirstFont(unset);
    }

    std::string FontOfEntry(const std::vector<std::string>& fonts, const char* unset, int entry)
    {
        const int at = entry - FirstFont(unset);
        return at >= 0 && at < static_cast<int>(fonts.size()) ? fonts[static_cast<size_t>(at)] : std::string();
    }

    std::string FontEntryLabel(const std::vector<std::string>& fonts, const char* unset, int entry)
    {
        return unset != nullptr && entry == 0 ? unset : FontOfEntry(fonts, unset, entry);
    }

    int StepFontEntry(int count, int entry, int step)
    {
        if (count <= 0) return kNoFontEntry;
        if (entry == kNoFontEntry) return step > 0 ? 0 : count - 1;
        return ((entry + step) % count + count) % count;
    }

    std::string FontShown(const std::vector<std::string>& fonts, const char* unset, const std::string& font)
    {
        const int entry = FontEntry(fonts, unset, font);
        return entry == kNoFontEntry ? font : FontEntryLabel(fonts, unset, entry);
    }
}
