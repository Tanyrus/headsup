#include "menu_state.h"
#include "settings.h"
#include "test.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
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

TEST(the_sidebar_has_the_four_major_pages_with_their_children_under_them)
{
    CHECK_EQ(kPageCount, 12);
    CHECK(kPages[0].page == Page::Entities && !kPages[0].child);
    for (int i = 1; i <= 5; ++i)
        CHECK(kPages[i].child);
    CHECK(kPages[6].page == Page::UserInterface && !kPages[6].child);
    CHECK(kPages[7].page == Page::TopPanel && kPages[7].child);
    CHECK(kPages[8].page == Page::Widgets && kPages[8].child);
    CHECK(kPages[9].page == Page::MouseCursor && kPages[9].child); // under User Interface
    CHECK(kPages[10].page == Page::ConfigMenu && !kPages[10].child);
    CHECK(kPages[11].page == Page::Debug && !kPages[11].child);
    CHECK(std::string_view(kPages[6].label) == "User Interface");
    CHECK(std::string_view(kPages[7].label) == "Top Panel" && std::string_view(kPages[8].label) == "Widgets");
}

TEST(every_page_is_found_by_its_key_and_nothing_else)
{
    for (const PageInfo& info : kPages)
        CHECK(PageNamed(PageKey(info.page)) == info.page);
    CHECK(PageKey(Page::UserInterface) == "userinterface");
    CHECK(PageKey(Page::TopPanel) == "toppanel" && PageKey(Page::Widgets) == "widgets");
    CHECK(!PageNamed("global"));
    CHECK(!PageNamed("topbar")); // the Top Panel page's name before
    CHECK(!PageNamed(""));
}

TEST(every_row_of_the_table_is_the_page_at_its_index)
{
    for (int i = 0; i < kPageCount; ++i)
        CHECK_EQ(static_cast<int>(kPages[i].page), i);
}

TEST(the_kind_pages_know_their_kind)
{
    const std::pair<Page, PlateKind> kinds[] = {{Page::Mobs, PlateKind::Mobs}, {Page::Players, PlateKind::Players},
        {Page::Self, PlateKind::Self}, {Page::Npcs, PlateKind::Npcs}, {Page::Pets, PlateKind::Pets}};
    for (const auto& [page, kind] : kinds)
        CHECK(KindOfPage(page) == kind);
    for (const Page page : {Page::Entities, Page::UserInterface, Page::TopPanel, Page::Widgets, Page::MouseCursor,
             Page::ConfigMenu, Page::Debug})
        CHECK(!KindOfPage(page));
}

TEST(labels_are_title_case_with_small_words_left_alone_inside)
{
    CHECK(TitleCaseText("Hide Level and Icons") == "Hide Level and Icons");
    CHECK(TitleCaseText("hide level and icons") == "Hide Level and Icons");
    CHECK(TitleCaseText("NM Placeholders") == "NM Placeholders");
    CHECK(TitleCaseText("Out of range") == "Out of Range");
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
    const std::vector<ShowCondition> mobs = ConditionsFor(PlateKind::Mobs);
    LineShow show;
    CHECK_EQ(ShowSummary(show, mobs), "Always");
    show.when = ConditionBit(ShowCondition::YourClaim);
    CHECK_EQ(ShowSummary(show, mobs), "Always"); // Always wins over what is checked under it
    show.always = false;
    CHECK_EQ(ShowSummary(show, mobs), "When Claimed by You");
    show.when |= ConditionBit(ShowCondition::Targeted) | ConditionBit(ShowCondition::TooWeak);
    CHECK_EQ(ShowSummary(show, mobs), "3 Conditions");
    show.when = 0;
    CHECK_EQ(ShowSummary(show, mobs), "Never");
    // A condition the kind's list does not offer cannot be seen or unchecked there, so it is not counted.
    show.when = ConditionBit(ShowCondition::Friend) | ConditionBit(ShowCondition::Unclaimed);
    CHECK_EQ(ShowSummary(show, mobs), "When Unclaimed");
    CHECK_EQ(ShowSummary(show, ConditionsFor(PlateKind::Players)), "When Friend");
    CHECK_EQ(ShowSummary(show, ConditionsFor(PlateKind::Npcs)), "Never");
}

TEST(a_friend_is_added_trimmed_once_and_only_as_a_character_name)
{
    const std::vector<std::string> friends{"Chadmage", "pollux"};
    CHECK(FriendToAdd(friends, "Tanyrus") == std::string("Tanyrus"));
    CHECK(FriendToAdd(friends, "  Tanyrus\t") == std::string("Tanyrus"));
    CHECK(FriendToAdd({}, "Abc") == std::string("Abc"));
    CHECK(!FriendToAdd(friends, ""));
    CHECK(!FriendToAdd(friends, "   "));
    CHECK(!FriendToAdd(friends, "Chadmage"));
    CHECK(!FriendToAdd(friends, " chadMAGE "));
    CHECK(!FriendToAdd(friends, "POLLUX"));
    CHECK(FriendToAdd(friends, "Chad") == std::string("Chad")); // a part of a name is another name
    CHECK(!FriendToAdd(friends, "Two Words")); // the list is saved with spaces between its names
    CHECK(!FriendToAdd({}, "Ab"));             // the game takes no name under three letters
    CHECK(FriendToAdd({}, "Abcdefghijklmno") == std::string("Abcdefghijklmno"));
    CHECK(!FriendToAdd({}, "Abcdefghijklmnop")); // nor over fifteen
    CHECK(!FriendToAdd({}, "Chad1"));            // nor anything but letters
    CHECK(!FriendToAdd({}, "Tom's"));
    // Whatever it takes, Clamp keeps.
    for (const char* name : {"Tanyrus", "Abc", "Abcdefghijklmno"})
    {
        Settings s;
        s.friends = {*FriendToAdd({}, name)};
        CHECK(Clamp(s).friends == s.friends);
    }
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
    const std::vector<ShowCondition> offered = ConditionsFor(PlateKind::Players);
    const uint32_t all = ConditionBit(ShowCondition::Targeted) | ConditionBit(ShowCondition::PartyMember) |
                         ConditionBit(ShowCondition::Friend) | ConditionBit(ShowCondition::LinkshellMember);
    LineShow show; // Always, nothing checked under it
    CHECK(ToggleCondition(show, offered, 2)); // the list's third row: Party Member
    CHECK(!show.always);
    CHECK_EQ(show.when, all & ~ConditionBit(ShowCondition::PartyMember));
    CHECK(ToggleCondition(show, offered, 2)); // checked again: every condition, and still not Always
    CHECK(!show.always && show.when == all);
    CHECK(ToggleCondition(show, offered, 0));
    CHECK(show.always && show.when == all);
    CHECK(ToggleCondition(show, offered, 0)); // Always off leaves what was checked
    CHECK(!show.always && show.when == all);
    CHECK(!ToggleCondition(show, offered, 5) && !ToggleCondition(show, offered, -1)); // no such row
    CHECK(!show.always && show.when == all);
    // A condition another kind's lines have, saved in the file, is not carried into what Always leaves checked.
    LineShow stray{true, ConditionBit(ShowCondition::Aggressive)};
    ToggleCondition(stray, offered, 1);
    CHECK_EQ(stray.when, all & ~ConditionBit(ShowCondition::Targeted));
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
    CHECK_EQ(SyncEntryLabel(targets, 0), "Do Not Sync");
    CHECK_EQ(SyncEntryLabel(targets, 1), "Sync with Top Line");
    CHECK_EQ(SyncEntryLabel(targets, 2), "Sync with Bottom Line");
}

TEST(the_clocks_zone_dropdown_lists_local_first_and_then_every_zone)
{
    CHECK_EQ(kClockZoneChoices, kClockZoneCount + 1);
    CHECK(ClockZoneChoiceName(0) == "Local" && ClockZoneChoiceName(1) == "Japan (JST)");
    CHECK(ClockZoneChoiceName(kClockZoneChoices - 1) == "Australia Eastern (AET)");
    CHECK(ClockZoneChoice(std::nullopt) == 0 && !ClockZoneOfChoice(0).has_value());
    CHECK(!ClockZoneOfChoice(-1).has_value() && !ClockZoneOfChoice(kClockZoneChoices).has_value());
    for (int z = 0; z < kClockZoneCount; ++z)
    {
        const auto zone = static_cast<ClockZone>(z);
        CHECK(ClockZoneOfChoice(ClockZoneChoice(zone)) == zone);
        CHECK(ClockZoneChoiceName(ClockZoneChoice(zone)) == ClockZoneName(zone));
    }
}

TEST(the_widgets_page_has_a_tab_for_every_widget_on_the_bar_in_rows_as_even_as_they_come)
{
    const std::vector<BarWidget> tabs = WidgetTabs();
    CHECK_EQ(tabs.size(), static_cast<size_t>(kPlacedWidgetCount));
    for (int w = 0; w < kBarWidgetCount; ++w)
    {
        const auto widget = static_cast<BarWidget>(w);
        CHECK_EQ(std::ranges::count(tabs, widget), OnTheBar(widget) ? 1 : 0); // Game help has a card of its own
    }
    CHECK(tabs.front() == BarWidget::Zone && tabs.back() == BarWidget::Distance);
    CHECK_EQ(TabsPerRow(14, 2), 7);
    CHECK_EQ(TabsPerRow(13, 2), 7); // an odd one out goes in the first row
    CHECK_EQ(TabsPerRow(3, 1), 3);
    CHECK_EQ(TabsPerRow(2, 3), 1); // more rows than tabs: one each
    CHECK_EQ(TabsPerRow(0, 2), 1); // never none, which a row's width is divided by
}

