#include "menu_state.h"
#include "settings.h"
#include "test.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace headsup;

TEST(no_change_waiting_means_no_save)
{
    CHECK(!SaveDue(std::nullopt, 10.0, true));
    CHECK(!SaveDue(std::nullopt, 10.0, false));
}

TEST(a_single_change_saves_half_a_second_later)
{
    CHECK(!SaveDue(10.0, 10.0, true));
    CHECK(!SaveDue(10.0, 10.25, true));
    CHECK(!SaveDue(10.0, 10.49, true));
    CHECK(SaveDue(10.0, 10.5, true));
    CHECK(SaveDue(10.0, 60.0, true));
}

TEST(a_change_every_frame_saves_only_once_the_last_one_has_settled)
{
    // A slider dragged for two seconds at 60 frames a second: every frame's change is the last one so far.
    const double frame = 1.0 / 60.0;
    double changedAt   = 0.0;
    for (int f = 0; f < 120; ++f)
    {
        changedAt = 10.0 + f * frame;
        CHECK(!SaveDue(changedAt, changedAt, true));
    }
    CHECK(changedAt > 11.9);
    CHECK(!SaveDue(changedAt, changedAt + 0.25, true));
    CHECK(!SaveDue(changedAt, changedAt + 0.5 - frame, true));
    CHECK(SaveDue(changedAt, changedAt + 0.5, true));
}

TEST(a_change_waiting_when_the_window_is_closed_saves_at_once)
{
    CHECK(SaveDue(10.0, 10.0, false));
    CHECK(SaveDue(10.0, 10.25, false));
}

TEST(the_kind_pages_know_their_kind)
{
    const std::pair<Page, PlateKind> kinds[] = {{Page::Mobs, PlateKind::Mobs}, {Page::Players, PlateKind::Players},
        {Page::Self, PlateKind::Self}, {Page::Npcs, PlateKind::Npcs}, {Page::Pets, PlateKind::Pets}};
    for (const auto& [page, kind] : kinds)
        CHECK(KindOfPage(page) == kind);
    for (const Page page : {Page::Entities, Page::UserInterface, Page::TopPanel, Page::Widgets, Page::MouseCursor,
             Page::ConfigMenu})
        CHECK(!KindOfPage(page));
}

TEST(the_kind_pages_show_while_nameplates_or_outlines_are_on)
{
    Settings s;
    CHECK(KindPagesShown(s));
    s.nameplates = false;
    CHECK(KindPagesShown(s));
    s.outlines = false;
    CHECK(!KindPagesShown(s));
    s.nameplates = true;
    CHECK(KindPagesShown(s));
    s.enabled = false; // HeadsUp off: each kind's page says what switches it on
    CHECK(KindPagesShown(s));
}

TEST(labels_are_title_case_with_small_words_left_alone_inside)
{
    CHECK(TitleCaseText("Hide Level and Icons") == "Hide Level and Icons");
    CHECK(TitleCaseText("hide level and icons") == "Hide Level and Icons");
    CHECK(TitleCaseText("NM Placeholders") == "NM Placeholders");
    CHECK(TitleCaseText("Out of range") == "Out of Range");
    CHECK(TitleCaseText("when owner is you") == "When Owner is You");
    CHECK(TitleCaseText("Scale with distance") == "Scale with Distance");
    CHECK(TitleCaseText("of the sea") == "Of the Sea");
    CHECK(TitleCaseText("NPC or object") == "NPC or Object");
    CHECK(TitleCaseText("color by toughness") == "Color by Toughness");
    CHECK(TitleCaseText("sub-target") == "Sub-Target");
    CHECK(TitleCaseText("HP") == "HP");
    CHECK(TitleCaseText("ID (0x1F)") == "ID (0x1F)");
    CHECK(TitleCaseText("word") == "Word");
    CHECK(TitleCaseText("") == "");
}

TEST(the_show_dropdown_reads_always_one_condition_how_many_or_never)
{
    const auto NameOfCondition = [](ShowCondition c) {
        return std::string(kWindowLayout.OptionsOf(WindowList::ShowCondition)[static_cast<size_t>(c)].name);
    };
    const std::span<const ShowCondition> mobs = ConditionsFor(PlateKind::Mobs);
    LineShow show;
    const std::string always = ShowSummary(show, mobs, kWindowLayout);
    show.when = ConditionBit(ShowCondition::YourClaim);
    CHECK_EQ(ShowSummary(show, mobs, kWindowLayout), always); // Always wins over what is checked under it
    show.always = false;
    CHECK_EQ(ShowSummary(show, mobs, kWindowLayout), NameOfCondition(ShowCondition::YourClaim)); // one condition is named
    show.when |= ConditionBit(ShowCondition::Targeted) | ConditionBit(ShowCondition::TooWeak);
    CHECK(ShowSummary(show, mobs, kWindowLayout).starts_with("3 ")); // several are counted
    show.when = 0;
    const std::string never = ShowSummary(show, mobs, kWindowLayout);
    CHECK(never != always && !never.empty());
    // A condition the kind's list does not offer cannot be seen or unchecked there, so it is not counted.
    show.when = ConditionBit(ShowCondition::Friend) | ConditionBit(ShowCondition::Unclaimed);
    CHECK_EQ(ShowSummary(show, mobs, kWindowLayout), NameOfCondition(ShowCondition::Unclaimed));
    CHECK_EQ(ShowSummary(show, ConditionsFor(PlateKind::Players), kWindowLayout), NameOfCondition(ShowCondition::Friend));
    CHECK_EQ(ShowSummary(show, ConditionsFor(PlateKind::Npcs), kWindowLayout), never);
    show.when = ConditionBit(ShowCondition::Generally);
    CHECK_EQ(ShowSummary(show, ConditionsFor(PlateKind::Npcs), kWindowLayout), NameOfCondition(ShowCondition::Generally));
}

TEST(a_font_dropdowns_arrows_step_through_its_list_and_around_its_ends)
{
    const std::vector<std::string> fonts{"Barlow Condensed", "Cinzel", "Marcellus SC"};
    const int count = FontEntryCount(fonts, "Default"); // Default, then the three
    CHECK_EQ(FontOfEntry(fonts, "Default", StepFontEntry(count, FontEntry(fonts, "Default", "Cinzel"), 1)), "Marcellus SC");
    CHECK_EQ(FontOfEntry(fonts, "Default", StepFontEntry(count, FontEntry(fonts, "Default", "Cinzel"), -1)), "Barlow Condensed");
    CHECK_EQ(StepFontEntry(count, 3, 1), 0);  // past the last font, back to Default
    CHECK_EQ(StepFontEntry(count, 0, -1), 3); // and before Default, the last
    // A saved font the list lacks has no place in it: the arrows start from the list's ends.
    CHECK_EQ(StepFontEntry(count, kNoFontEntry, 1), 0);
    CHECK_EQ(StepFontEntry(count, kNoFontEntry, -1), 3);
    CHECK_EQ(StepFontEntry(1, 0, 1), 0);
    CHECK_EQ(StepFontEntry(0, kNoFontEntry, 1), kNoFontEntry); // nothing to step to
}

TEST(a_font_dropdown_lists_its_unset_choice_first_then_the_fonts)
{
    const std::vector<std::string> fonts{"Barlow Condensed", "Cinzel", "Marcellus SC"};
    // A line's font: Default first, standing for no name of its own.
    CHECK_EQ(FontEntryCount(fonts, "Default"), 4);
    CHECK_EQ(FontEntry(fonts, "Default", ""), 0);
    CHECK_EQ(FontEntry(fonts, "Default", "Barlow Condensed"), 1);
    CHECK_EQ(FontEntry(fonts, "Default", "Marcellus SC"), 3);
    CHECK_EQ(FontEntry(fonts, "Default", "Comic Sans MS"), kNoFontEntry); // saved, and no longer installed
    CHECK_EQ(FontOfEntry(fonts, "Default", 0), "");
    CHECK_EQ(FontOfEntry(fonts, "Default", 1), "Barlow Condensed");
    CHECK_EQ(FontOfEntry(fonts, "Default", 3), "Marcellus SC");
    CHECK_EQ(FontEntryLabel(fonts, "Default", 0), "Default");
    CHECK_EQ(FontEntryLabel(fonts, "Default", 2), "Cinzel");
    CHECK_EQ(FontShown(fonts, "Default", ""), "Default");
    CHECK_EQ(FontShown(fonts, "Default", "Cinzel"), "Cinzel");
    CHECK_EQ(FontShown(fonts, "Default", "Comic Sans MS"), "Comic Sans MS"); // shown by its own name all the same
    // A font that is always one of the list: no unset choice.
    CHECK_EQ(FontEntryCount(fonts, nullptr), 3);
    CHECK_EQ(FontEntry(fonts, nullptr, "Barlow Condensed"), 0);
    CHECK_EQ(FontEntry(fonts, nullptr, "Marcellus SC"), 2);
    CHECK_EQ(FontEntry(fonts, nullptr, ""), kNoFontEntry);
    CHECK_EQ(FontEntry(fonts, nullptr, "Comic Sans MS"), kNoFontEntry);
    CHECK_EQ(FontOfEntry(fonts, nullptr, 0), "Barlow Condensed");
    CHECK_EQ(FontOfEntry(fonts, nullptr, 2), "Marcellus SC");
    CHECK_EQ(FontEntryLabel(fonts, nullptr, 1), "Cinzel");
    CHECK_EQ(FontShown(fonts, nullptr, "Comic Sans MS"), "Comic Sans MS");
    CHECK_EQ(FontShown(fonts, nullptr, ""), "");
    // Every entry stands for the font it is found by.
    for (const char* unset : {static_cast<const char*>(nullptr), "Theme's"})
        for (int entry = 0; entry < FontEntryCount(fonts, unset); ++entry)
            CHECK_EQ(FontEntry(fonts, unset, FontOfEntry(fonts, unset, entry)), entry);
}

TEST(a_line_may_sync_with_any_other_line_that_does_not_borrow_from_it)
{
    KindSettings kind;
    CHECK((SyncTargets(kind, 0) == std::vector<int>{1, 2}));
    CHECK((SyncTargets(kind, 1) == std::vector<int>{0, 2}));
    CHECK((SyncTargets(kind, 2) == std::vector<int>{0, 1}));
    kind.lines[1].syncWith = 0; // line 2 borrows from line 1
    CHECK((SyncTargets(kind, 0) == std::vector<int>{2}));
    CHECK((SyncTargets(kind, 1) == std::vector<int>{0, 2}));
    CHECK((SyncTargets(kind, 2) == std::vector<int>{0, 1}));
    kind.lines[2].syncWith = 1; // and line 3 from line 2, so both lead to line 1
    CHECK(SyncTargets(kind, 0).empty());
    CHECK((SyncTargets(kind, 1) == std::vector<int>{0}));
    CHECK((SyncTargets(kind, 2) == std::vector<int>{0, 1}));
    // No choice a dropdown offers makes a cycle for Clamp to cut.
    for (int line = 0; line < kPlateLines; ++line)
        for (const int target : SyncTargets(kind, line))
        {
            Settings s;
            s.Kind(PlateKind::Mobs).lines[1].syncWith    = 0;
            s.Kind(PlateKind::Mobs).lines[2].syncWith    = 1;
            s.Kind(PlateKind::Mobs).lines[line].syncWith = target;
            CHECK(Clamp(s) == s);
        }
}

TEST(unchecking_a_condition_under_always_turns_always_off_and_keeps_the_rest)
{
    const std::span<const ShowCondition> offered = ConditionsFor(PlateKind::Players);
    const uint32_t all = ConditionBit(ShowCondition::Generally) | ConditionBit(ShowCondition::Targeted) |
                         ConditionBit(ShowCondition::PartyMember) | ConditionBit(ShowCondition::Friend) |
                         ConditionBit(ShowCondition::LinkshellMember);
    LineShow show; // Always, nothing checked under it
    CHECK(ToggleCondition(show, offered, 3)); // the list's fourth row: Party Member
    CHECK(!show.always);
    CHECK_EQ(show.when, all & ~ConditionBit(ShowCondition::PartyMember));
    CHECK(ToggleCondition(show, offered, 3)); // checked again: every condition, and still not Always
    CHECK(!show.always && show.when == all);
    CHECK(ToggleCondition(show, offered, 0));
    CHECK(show.always && show.when == all);
    CHECK(ToggleCondition(show, offered, 0)); // Always off leaves what was checked
    CHECK(!show.always && show.when == all);
    CHECK(!ToggleCondition(show, offered, 6) && !ToggleCondition(show, offered, -1)); // no such row
    // The last row is a row like the others: on a pet's page it is the one that asks whose pet it is.
    const std::span<const ShowCondition> pets = ConditionsFor(PlateKind::Pets);
    LineShow whose{false, 0};
    CHECK(ToggleCondition(whose, pets, static_cast<int>(pets.size())));
    CHECK_EQ(whose.when, ConditionBit(ShowCondition::OwnerIsYou));
    CHECK(!show.always && show.when == all);
    // A condition another kind's lines have, saved in the file, is not carried into what Always leaves checked.
    LineShow stray{true, ConditionBit(ShowCondition::Aggressive)};
    ToggleCondition(stray, offered, 2);
    CHECK_EQ(stray.when, all & ~ConditionBit(ShowCondition::Targeted));
    // On every kind's page Generally is the row right under Always.
    for (int kind = 0; kind < kPlateKindCount; ++kind)
    {
        LineShow first{false, 0};
        CHECK(ToggleCondition(first, ConditionsFor(static_cast<PlateKind>(kind)), 1));
        CHECK_EQ(first.when, ConditionBit(ShowCondition::Generally));
    }
}

TEST(the_sync_dropdown_lists_no_then_its_targets)
{
    const std::vector<int> targets{0, 2}; // line 2's, with nothing synced
    CHECK_EQ(SyncEntry(targets, kNoSync), 0);
    CHECK_EQ(SyncEntry(targets, 0), 1);
    CHECK_EQ(SyncEntry(targets, 2), 2);
    CHECK_EQ(SyncEntry(targets, 1), 0); // a line that is not offered shows as no sync
    CHECK_EQ(SyncOfEntry(targets, 0), kNoSync);
    CHECK_EQ(SyncOfEntry(targets, 1), 0);
    CHECK_EQ(SyncOfEntry(targets, 2), 2);
    CHECK_EQ(SyncOfEntry(targets, 3), kNoSync);
    CHECK_EQ(SyncOfEntry({}, 0), kNoSync);
    // An entry names the line it syncs with, and the first names none.
    const auto names = [&](int entry, int line) {
        const char* name = kWindowLayout.OptionsOf(WindowList::PlateLines)[static_cast<size_t>(line)].name;
        return SyncEntryLabel(targets, entry, kWindowLayout).find(name) != std::string::npos;
    };
    CHECK(names(1, 0) && !names(1, 2));
    CHECK(names(2, 2) && !names(2, 0));
    CHECK(!names(0, 0) && !names(0, 2) && !SyncEntryLabel(targets, 0, kWindowLayout).empty());
}

TEST(the_clocks_zone_dropdown_lists_local_first_and_then_every_zone)
{
    CHECK_EQ(kClockZoneChoices, kClockZoneCount + 1);
    CHECK(ClockZoneChoiceName(0, kWindowLayout) == kWindowLayout.WordOf(WindowWord::Local).name);
    CHECK(ClockZoneChoiceName(1, kWindowLayout) == ClockZoneName(ClockZone(0)));
    CHECK(ClockZoneChoice(std::nullopt) == 0 && !ClockZoneOfChoice(0).has_value());
    CHECK(!ClockZoneOfChoice(-1).has_value() && !ClockZoneOfChoice(kClockZoneChoices).has_value());
    for (int z = 0; z < kClockZoneCount; ++z)
    {
        const auto zone = static_cast<ClockZone>(z);
        CHECK(ClockZoneOfChoice(ClockZoneChoice(zone)) == zone);
        CHECK(ClockZoneChoiceName(ClockZoneChoice(zone), kWindowLayout) == ClockZoneName(zone));
    }
}

TEST(the_widgets_page_has_a_tab_for_every_widget_on_the_bar_in_rows_as_even_as_they_come)
{
    BarPlacement placement = DefaultPlacement();
    const std::vector<PanelWidget> tabs = WidgetTabs(placement);
    CHECK_EQ(tabs.size(), static_cast<size_t>(kPlacedWidgetCount));
    for (int w = 0; w < kBarWidgetCount; ++w)
    {
        const auto widget = static_cast<BarWidget>(w);
        CHECK_EQ(std::ranges::count(tabs, widget), OnTheBar(widget) ? 1 : 0); // Game help has a card of its own
    }
    CHECK(tabs.front() == BarWidget::Zone && tabs.back() == BarWidget::World);
}

