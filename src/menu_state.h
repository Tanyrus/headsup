#pragma once

#include "bar_placement.h"
#include "bar_server_time.h"
#include "plate_settings.h"
#include "plate_template.h"
#include "settings.h"
#include "window_layout.h"

#include <algorithm>
#include <span>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace headsup
{
    constexpr double kSaveSettleSeconds = 0.5;
    // Whether the settings should be written now: a change is waiting and no other has come for the settle time, so
    // a slider being dragged writes no file, or the window is closed, where nothing more can come.
    bool SaveDue(std::optional<double> changedAt, double now, bool windowOpen);

    std::optional<PlateKind> KindOfPage(Page page);
    // The pages of the kinds have nothing to set while neither nameplates nor outlines are on.
    bool KindPagesShown(const Settings& s);


    // A label with each word capitalized after a space or a hyphen ("Sub-Target"), but for the small words inside it
    // ("Hide Level and Icons"); letters already capital stay.
    std::string TitleCaseText(std::string_view text);

    // The Widgets page's tabs: every widget on the bar, in BarWidget's order, each with its other copies after it.
    std::vector<PanelWidget> WidgetTabs(const BarPlacement& placement);
    // How many tabs go in a row for count of them in rows rows: the rows as even as they come, the first the fuller.
    int TabsPerRow(int count, int rows);

    // The Clock widget's Time Zone dropdown: the computer's own zone first, as no zone picked, then every ClockZone.
    constexpr int kClockZoneChoices = kClockZoneCount + 1;
    int ClockZoneChoice(const std::optional<ClockZone>& zone);
    std::optional<ClockZone> ClockZoneOfChoice(int choice);
    std::string ClockZoneChoiceName(int choice, const WindowLayout& layout);

    // What the Show dropdown reads when closed: Always, the one condition checked, how many are, or Never. Only the
    // conditions the kind is offered count.
    std::string ShowSummary(const LineShow& show, std::span<const ShowCondition> offered, const WindowLayout& layout);
    // A click on a row of the Line Visibility list: 0 is Always, then the conditions offered. Unchecking one under
    // Always turns Always off and leaves the others checked. False for no such row.
    bool ToggleCondition(LineShow& show, std::span<const ShowCondition> offered, int row);

    // A design line's box: its caret, kept between frames, and whether one of its stretches has the keyboard this
    // frame.
    struct LineDraft
    {
        RunCaret caret;
        bool typing = false;
    };

    // The "Add a key" button's wait for a key: those held when it was pressed do not count.
    struct KeyCapture
    {
        bool on = false;
        std::vector<uint8_t> held;
    };

    // A kind page's own state, begun anew when another kind's page opens: the three lines' boxes and the Line Style
    // tab that is open.
    struct DesignEditor
    {
        PlateKind kind = PlateKind::Mobs;
        int styleTab   = 0;
        LineDraft drafts[kPlateLines] = {};
    };

    // A font dropdown lists unset first, when it has one, a word that stands for an empty name, then the fonts. A
    // saved font the list lacks has no entry, and the closed dropdown still reads its name.
    constexpr int kNoFontEntry = -1;
    int FontEntryCount(const std::vector<std::string>& fonts, const char* unset);
    int FontEntry(const std::vector<std::string>& fonts, const char* unset, const std::string& font);
    std::string FontOfEntry(const std::vector<std::string>& fonts, const char* unset, int entry);
    std::string FontEntryLabel(const std::vector<std::string>& fonts, const char* unset, int entry);
    std::string FontShown(const std::vector<std::string>& fonts, const char* unset, const std::string& font);
    // The entry one before (step -1) or after (1) this one, around the list's ends; from no entry, the list's last or
    // first.
    int StepFontEntry(int count, int entry, int step);

    // The lines a line may borrow its style from: every other line whose own chain of syncs does not lead back to
    // it, so no choice makes a cycle for Clamp to cut.
    std::vector<int> SyncTargets(const KindSettings& kind, int line);
    // The Sync dropdown lists No, then the targets: the entry a syncWith shows as (No for a line not among them),
    // the syncWith an entry stands for, and the entry's words.
    int SyncEntry(const std::vector<int>& targets, int syncWith);
    int SyncOfEntry(const std::vector<int>& targets, int entry);
    std::string SyncEntryLabel(const std::vector<int>& targets, int entry, const WindowLayout& layout);
    std::string SyncEntryTip(const std::vector<int>& targets, int entry, const WindowLayout& layout);

    // One entry of a list of pictures (the Target Cursor's styles, the Cursor Icon): one of HeadsUp's own, or a file of
    // the cursors folder. Style's last value is File.
    template <typename Style>
    struct PictureChoice
    {
        Style style{};
        std::string file; // a File's name
        bool operator==(const PictureChoice&) const = default;
    };

    // The list: HeadsUp's own, then each file, then the file the settings name when the folder has none such.
    template <typename Style>
    std::vector<PictureChoice<Style>> PictureChoices(const std::vector<std::string>& files, Style saved, const std::string& savedFile)
    {
        std::vector<PictureChoice<Style>> choices;
        for (int style = 0; style < static_cast<int>(Style::File); ++style)
            choices.push_back(PictureChoice<Style>{static_cast<Style>(style), {}});
        for (const std::string& file : files)
            choices.push_back(PictureChoice<Style>{Style::File, file});
        const PictureChoice<Style> kept{Style::File, savedFile};
        if (saved == Style::File && std::ranges::find(choices, kept) == choices.end()) choices.push_back(kept);
        return choices;
    }

    // Which entry the settings have picked, which PictureChoices always lists; the first if it ever did not.
    template <typename Style>
    int PictureChoiceOf(const std::vector<PictureChoice<Style>>& choices, Style saved, const std::string& savedFile)
    {
        const PictureChoice<Style> picked{saved, saved == Style::File ? savedFile : std::string()};
        const auto at = std::ranges::find(choices, picked);
        return at != choices.end() ? static_cast<int>(at - choices.begin()) : 0;
    }
}
