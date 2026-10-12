#pragma once

#include "plate_settings.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace headsup
{
    constexpr double kSaveStallMs       = 20.0; // a save slower than a frame is deferred
    constexpr double kSaveSettleSeconds = 0.5;  // until this long after the last change
    // Whether the settings should be written now: a change is waiting, and either the last save was quick or the
    // change has settled.
    bool SaveDue(std::optional<double> changedAt, double now, double lastSaveMs);

    enum class Page : int { Entities, Mobs, Players, Self, Npcs, Pets, UserInterface, TopBar, MouseCursor, ConfigMenu, Debug };
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

    // A design line's box between frames: its text while typed in, which ImGui takes back on Escape only from a
    // buffer that has kept it, and its caret in bytes, where a clicked tag goes (past the text means its end).
    struct LineDraft
    {
        char text[kMaxTemplate + 1] = {};
        size_t caret                = kMaxTemplate;
        bool typing                 = false;
    };

    // A font dropdown lists unset first, when it has one, a word that stands for an empty name, then the fonts. A
    // saved font the list lacks has no entry, and the closed dropdown still reads its name.
    constexpr int kNoFontEntry = -1;
    int FontEntryCount(const std::vector<std::string>& fonts, const char* unset);
    int FontEntry(const std::vector<std::string>& fonts, const char* unset, const std::string& font);
    std::string FontOfEntry(const std::vector<std::string>& fonts, const char* unset, int entry);
    std::string FontEntryLabel(const std::vector<std::string>& fonts, const char* unset, int entry);
    std::string FontShown(const std::vector<std::string>& fonts, const char* unset, const std::string& font);

    // The lines a line may borrow its style from: every other line whose own chain of syncs does not lead back to
    // it, so no choice makes a cycle for Clamp to cut.
    std::vector<int> SyncTargets(const KindSettings& kind, int line);
    // The Sync dropdown lists No, then the targets: the entry a syncWith shows as (No for a line not among them),
    // the syncWith an entry stands for, and the entry's words.
    int SyncEntry(const std::vector<int>& targets, int syncWith);
    int SyncOfEntry(const std::vector<int>& targets, int entry);
    std::string SyncEntryLabel(const std::vector<int>& targets, int entry);
}
