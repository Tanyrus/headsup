#pragma once

#include "bar_placement.h"
#include "bar_server_time.h"
#include "plate_settings.h"

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

    enum class Page : int
    {
        Entities,
        Mobs,
        Players,
        Self,
        Npcs,
        Pets,
        UserInterface,
        TopPanel,
        Widgets,
        MouseCursor,
        ConfigMenu,
        Debug,
    };
    constexpr int kPageCount = static_cast<int>(Page::Debug) + 1;
    struct PageInfo
    {
        Page page;
        const char* label;
        bool child; // indented under the major page before it
    };
    extern const PageInfo kPages[kPageCount]; // in sidebar order

    // The label in lower case without spaces: how /hu menu names a page.
    std::string PageKey(Page page);
    std::optional<Page> PageNamed(std::string_view key);
    std::optional<PlateKind> KindOfPage(Page page);

    // A label with each word capitalized after a space or a hyphen ("Sub-Target"), but for the small words inside it
    // ("Hide Level and Icons"); letters already capital stay.
    std::string TitleCaseText(std::string_view text);

    // The Widgets page's tabs: every widget on the bar, in BarWidget's order.
    std::vector<BarWidget> WidgetTabs();
    // How many tabs go in a row for count of them in rows rows: the rows as even as they come, the first the fuller.
    int TabsPerRow(int count, int rows);

    // The Clock widget's Time Zone dropdown: the computer's own zone first, as no zone picked, then every ClockZone.
    constexpr int kClockZoneChoices = kClockZoneCount + 1;
    int ClockZoneChoice(const std::optional<ClockZone>& zone);
    std::optional<ClockZone> ClockZoneOfChoice(int choice);
    std::string ClockZoneChoiceName(int choice);

    struct Settings;
    // A size slider's format: its percent, then the pixels that percent of the base size is drawn at before the
    // distance scale, as "%d%% (13 px)".
    std::string SizeFormat(const Settings& settings, int percent);

    constexpr const char* kShowAlways = "Always";
    // What the Show dropdown reads when closed: Always, the one condition checked, how many are, or Never. Only the
    // conditions the kind is offered count.
    std::string ShowSummary(const LineShow& show, const std::vector<ShowCondition>& offered);

    // A name typed into the Friends card as it joins the list: trimmed. Nothing when it is no character's name or
    // is there already in any case.
    std::optional<std::string> FriendToAdd(const std::vector<std::string>& friends, std::string_view typed);

    // A design line's box between frames: whether it is typed in, and its caret in bytes, where a clicked tag goes
    // (past the text means its end).
    struct LineDraft
    {
        size_t caret = kMaxTemplate;
        bool typing  = false;
    };

    // The "Add a key" button's wait for a key: those held when it was pressed do not count.
    struct KeyCapture
    {
        bool on = false;
        std::vector<uint8_t> held;
    };

    // A kind page's own state, begun anew when another kind's page opens: the line last typed in, which takes a
    // clicked tag, the three lines' boxes, the Line Style tab that is open and the name being typed for Friends.
    struct DesignEditor
    {
        PlateKind kind = PlateKind::Mobs;
        int line       = 0;
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
    std::string SyncEntryLabel(const std::vector<int>& targets, int entry);
}
