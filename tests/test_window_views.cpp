#include "fake_memory.h"
#include "mouse_messages.h"
#include "native_locate.h"
#include "test.h"
#include "window_views.h"

#include <array>
#include <cstring>
#include <string>
#include <vector>

using namespace headsup;

namespace
{
    // The client's window list as its draw loop walks it (RVA 0x15F6E8 in the client of 2026-10-13): the list
    // object's first dword is the first link; a link holds the next at +0, its window's record at +0x10 and a skip
    // byte at +0x14.
    // A record holds its descriptor at +0x04 (the 16-byte name at +0x46), its type at +0x10 (0x0E hidden) and its
    // rectangle as four int16 from +0x3A.
    constexpr uint32_t kList = 0x036E1838, kLinkBytes = 0x18, kRecordBytes = 0xA0, kDescriptorBytes = 0x58;

    struct ListedWindow
    {
        const char* name;
        int16_t left, top, right, bottom;
        uint32_t type = 0;
        bool skipped  = false;
    };

    // Each window's link, record and descriptor in their own regions, linked in the order given.
    FakeMemory ListOf(const std::vector<ListedWindow>& windows)
    {
        FakeMemory memory;
        memory.Map(kList, sizeof(uint32_t));
        uint32_t at = 0x10000000;
        uint32_t previousAt    = kList;
        for (const ListedWindow& w : windows)
        {
            const uint32_t link = at, record = at + 0x100, descriptor = at + 0x200;
            memory.Map(link, kLinkBytes);
            memory.Map(record, kRecordBytes);
            memory.Map(descriptor, kDescriptorBytes);
            memory.Put32(previousAt, link);
            memory.Put32(link + 0x10, record);
            const uint8_t skip = w.skipped ? 1 : 0;
            memory.Put(link + 0x14, &skip, 1);
            memory.Put32(record + 0x04, descriptor);
            memory.Put32(record + 0x10, w.type);
            const int16_t rect[] = {w.left, w.top, w.right, w.bottom};
            memory.Put(record + 0x3A, rect, sizeof(rect));
            memory.Put(descriptor + 0x46, w.name, std::strlen(w.name));
            previousAt = link;
            at += 0x1000;
        }
        return memory;
    }

    std::string NameOf(const OpenWindow& w) { return ShortName(w.key); }
}

TEST(the_game_s_window_list_is_read_in_its_order_with_each_window_s_place_and_whether_it_is_hidden)
{
    FakeMemory memory = ListOf({
        {"menu    helpwind", 130, 16, 1790, 46},
        {"menu    menuwind", 1792, 48, 1904, 250, kWindowOpening},
        {"menu    statcom2", 1792, 48, 1904, 266},
    });
    std::array<OpenWindow, 8> out{};
    const size_t count = ReadWindowList(kList, memory.Access(), out);
    CHECK(count == 3);
    CHECK(NameOf(out[0]) == "helpwind" && out[0].left == 130 && out[0].top == 16 && out[0].right == 1790 &&
          out[0].bottom == 46 && !out[0].hidden);
    CHECK(NameOf(out[1]) == "menuwind" && out[1].hidden);
    CHECK(NameOf(out[2]) == "statcom2" && out[2].bottom == 266 && !out[2].hidden);
    CHECK(out[0].mcd != 0 && out[0].mcd != out[1].mcd);
}

TEST(a_link_the_game_skips_and_a_record_without_a_menu_name_are_left_out)
{
    FakeMemory memory = ListOf({
        {"menu    helpwind", 130, 16, 1790, 46},
        {"menu    buff    ", 142, 48, 382, 52, 0, true},
        {"other   thing   ", 0, 0, 10, 10},
        {"menu    logwindo", 16, 1058, 1790, 1064},
    });
    std::array<OpenWindow, 8> out{};
    const size_t count = ReadWindowList(kList, memory.Access(), out);
    CHECK(count == 2);
    CHECK(NameOf(out[0]) == "helpwind" && NameOf(out[1]) == "logwindo");
}

TEST(the_list_read_stops_at_memory_that_cannot_be_read_and_at_the_room_given)
{
    FakeMemory memory = ListOf({
        {"menu    helpwind", 130, 16, 1790, 46},
        {"menu    titlewin", 16, 16, 128, 46},
        {"menu    netstat ", 1792, 16, 1904, 46},
    });
    std::array<OpenWindow, 2> two{};
    CHECK(ReadWindowList(kList, memory.Access(), two) == 2);
    memory.Free(0x10001000); // the second link
    std::array<OpenWindow, 8> out{};
    CHECK(ReadWindowList(kList, memory.Access(), out) == 1);
    memory.Free(kList);
    CHECK(ReadWindowList(kList, memory.Access(), out) == 0);
}

TEST(a_window_list_that_loops_back_on_itself_is_read_no_further_than_its_bound)
{
    FakeMemory memory = ListOf({
        {"menu    helpwind", 130, 16, 1790, 46, 0, true},
        {"menu    titlewin", 16, 16, 128, 46, 0, true},
    });
    memory.Put32(0x10001000, 0x10000000); // the second link's next is the first again, and both are skipped
    std::array<OpenWindow, 8> out{};
    CHECK(ReadWindowList(kList, memory.Access(), out) == 0);
}

namespace
{
    WindowKey Key(const char* shortName) { return *KeyFromShortName(shortName); }

    // Views as the game opened them with HeadsUp unloaded (the sweep of 2026-10-14, a 1920 x 1080 menu): Status has
    // the portrait at 16, its Currencies hide it under a list at 48, Profile adds its window at 48.
    const WindowKey kStatus[]     = {Key("buff"), Key("level"), Key("statcom2"), Key("persona")};
    const WindowKey kProfile[]    = {Key("buff"), Key("level"), Key("statcom2"), Key("persona"), Key("profile")};
    const WindowKey kCurrencies[] = {Key("buff"), Key("statcom2"), Key("evitem")};
    const WindowKey kRest[]       = {Key("buff")};
    const WindowView kViews[]     = {
        {"rest", "Nothing Open", 48, kRest},
        {"status", "Status", 16, kStatus},
        {"status/profile", "Status > Profile", 16, kProfile},
        {"status/currencies", "Status > Currencies", 48, kCurrencies},
    };
    constexpr int kScreenBottom = 1080;
    constexpr int kScreenRight  = 1920;

    // Status as the game draws it: the strip's windows, the hidden menu it came from, the chat log at the bottom.
    std::vector<FrameWindow> StatusFrame()
    {
        return {{Key("helpwind"), 130, 16, 1790, false}, {Key("titlewin"), 16, 16, 128, true},
            {Key("buff"), 142, 48, 382, false}, {Key("menuwind"), 1792, 48, 1904, true},
            {Key("level"), 16, 208, 128, false}, {Key("statcom2"), 1792, 48, 1904, false},
            {Key("persona"), 16, 16, 128, false}, {Key("logwindo"), 16, 1058, 1790, false}};
    }
}

TEST(a_frame_is_the_view_holding_most_of_its_shown_windows_and_the_smaller_one_on_a_tie)
{
    std::vector<FrameWindow> frame = StatusFrame();
    CHECK(ViewOf(kViews, frame) == std::optional<size_t>(1));
    frame.push_back({Key("profile"), 130, 48, 382, false});
    CHECK(ViewOf(kViews, frame) == std::optional<size_t>(2));
    frame.back().hidden = true; // a hidden window is not part of what is shown
    CHECK(ViewOf(kViews, frame) == std::optional<size_t>(1));
    const std::vector<FrameWindow> nothing = {{Key("helpwind"), 130, 16, 1790, false},
        {Key("logwindo"), 16, 1058, 1790, false}};
    CHECK(!ViewOf(kViews, nothing).has_value());
}

TEST(a_frame_s_top_is_its_highest_shown_window_that_hangs_from_the_top_and_is_not_the_strip)
{
    CHECK(FrameTop(StatusFrame(), kScreenBottom) == std::optional<int>(16));
    std::vector<FrameWindow> currencies = StatusFrame();
    currencies[6].hidden = true; // the portrait, under the list
    CHECK(FrameTop(currencies, kScreenBottom) == std::optional<int>(48));
    const std::vector<FrameWindow> bottomOnly = {{Key("helpwind"), 130, 16, 1790, false},
        {Key("iteminfo"), 16, 992, 382, false}, {Key("logwindo"), 16, 1058, 1790, false}};
    CHECK(!FrameTop(bottomOnly, kScreenBottom).has_value());
    // The map's corner frame, 106 x 14 at the screen's corner, draws nothing: the map's own windows are the top.
    const std::vector<FrameWindow> map = {{Key("buff"), 142, 48, 382, false}, {Key("mapframe"), 0, 0, 106, false},
        {Key("map0"), 1792, 48, 1904, false}};
    CHECK(FrameTop(map, kScreenBottom) == std::optional<int>(48));
}

TEST(a_frame_goes_down_or_up_by_its_view_s_shift_and_never_under_the_band)
{
    // The band at the reference settings: 62 with the help bar, 38 without it.
    CHECK(FrameShift(16, std::nullopt, 62, 62) == 46);
    CHECK(FrameShift(48, std::nullopt, 62, 62) == 14);
    CHECK(FrameShift(48, std::nullopt, 38, 38) == -10);
    CHECK(FrameShift(16, 20, 62, 30) == 20);          // the player's own shift for the view
    CHECK(FrameShift(16, 20, 62, 62) == 46);          // pushed clear of the band
    CHECK(FrameShift(48, std::nullopt, 62, 80) == 32); // the bar grew past where the views were placed
    CHECK(FrameShift(48, std::nullopt, 62, 30) == 14); // and a smaller bar leaves them where they are
}

TEST(view_shifts_read_back_what_they_wrote_and_drop_what_no_view_could_take)
{
    const std::vector<ViewShift> shifts = {{"status", 20}, {"status/profile", -6}};
    CHECK(ViewShiftsFrom(ViewShiftsText(shifts), kViews) == shifts);
    CHECK(ViewShiftsText(shifts) == "status:20|status/profile:-6");
    const std::vector<ViewShift> read = ViewShiftsFrom("status:20|nowhere:5|status:9|rest:x|rest:99999", kViews);
    CHECK((read == std::vector<ViewShift>{{"status", 20}, {"rest", kMaxWindowOffset}}));
    CHECK(ViewShiftsFrom("", kViews).empty());
}

TEST(the_help_is_on_while_the_panel_shows_it_or_the_game_s_own_strip_is_not_hidden)
{
    CHECK(ModeOf(true, true) == HelpMode::On);   // the panel's help line, the game's strip hidden
    CHECK(ModeOf(false, false) == HelpMode::On); // the game's strip, moved under the panel or where the game put it
    CHECK(ModeOf(false, true) == HelpMode::Off); // neither
}

TEST(the_views_shipped_are_named_once_each_and_the_reference_places_clear_their_bands)
{
    const std::span<const WindowView> views = WindowViews();
    CHECK(!views.empty());
    for (size_t a = 0; a < views.size(); ++a)
    {
        CHECK(!views[a].windows.empty());
        for (size_t b = a + 1; b < views.size(); ++b)
            CHECK(views[a].id != views[b].id);
    }
    CHECK(ReferenceBand(HelpMode::On) > ReferenceBand(HelpMode::Off) && ReferenceBand(HelpMode::Off) > 0);
}

TEST(the_game_s_strip_moved_goes_right_under_the_panel_and_the_windows_under_it_and_left_it_is_cleared)
{
    const StripRows strip{16, 46}; // the help and title windows as the game puts them
    CHECK((PlaceStrip(38, std::nullopt, true, 3) == StripPlace{0, 38})); // hidden: the panel's band alone
    CHECK((PlaceStrip(38, strip, false, 3) == StripPlace{0, 49}));       // left where it is, half under the panel
    CHECK((PlaceStrip(62, strip, false, 3) == StripPlace{0, 62}));       // the panel's help line reaches further
    CHECK((PlaceStrip(38, strip, true, 3) == StripPlace{22, 71}));       // moved: right under the panel
    CHECK((PlaceStrip(62, strip, true, 3) == StripPlace{46, 95}));       // and under its help line too
    CHECK((PlaceStrip(0, strip, true, 3) == StripPlace{0, 0}));          // the panel off: nothing goes anywhere
}

TEST(wide_scan_s_label_under_the_strip_s_title_moves_with_the_frame_s_windows_and_not_with_the_title)
{
    // The title window as the game puts it; the label is drawn at its left, 3 below its bottom, over the list at 48.
    const std::array<int16_t, 4> title{16, 16, 128, 46};
    std::array<int16_t, 4> rect = title;
    LabelPlace(rect, 16, 0); // the list went down 16 and the strip stayed
    CHECK((rect == std::array<int16_t, 4>{16, 32, 128, 62}));
    rect = {16, 38, 128, 68}; // the strip moved 22 under the panel, and the list 46
    LabelPlace(rect, 46, 22);
    CHECK((rect == std::array<int16_t, 4>{16, 62, 128, 92}));
    rect = {16, 38, 128, 68}; // the strip moved, the windows left where the game put them: the label stays too
    LabelPlace(rect, 0, 22);
    CHECK(rect == title);
}

TEST(a_click_over_a_hidden_window_is_swallowed_and_a_move_or_one_elsewhere_is_not)
{
    const std::vector<WindowRect> hidden = {{16, 64, 198, 254}};
    for (const uint32_t button : {kLeftDown, kLeftUp, kLeftDouble, kRightDown, kRightUp, kMiddleDown, kMiddleUp})
        CHECK(SwallowsClick(button, 100, 100, hidden));
    CHECK(!SwallowsClick(kMouseMove, 100, 100, hidden));
    CHECK(!SwallowsClick(kLeftDown, 10, 10, hidden) && !SwallowsClick(kLeftDown, 198, 100, hidden));
    CHECK(SwallowsClick(kLeftDown, 16, 64, hidden) && !SwallowsClick(kLeftDown, 100, 254, hidden));
    CHECK(!SwallowsClick(kLeftDown, 100, 100, {}));
}

TEST(a_frame_no_view_holds_goes_right_under_the_band_and_a_view_s_to_where_its_windows_start)
{
    // A player's check: the inspect window at the game's 48, under a band of 60 and with windows starting at 64.
    const std::vector<FrameWindow> check = {{Key("inspect"), 16, 48, 198, false}};
    std::vector<std::optional<int>> shifts(WindowViews().size());
    const FramePlan unheld = PlanOf(WindowViews(), check, shifts, 64);
    CHECK(!unheld.chosen && !unheld.placedUnder);
    const FrameShifts shifted = ShiftsOf(check, unheld.chosen, unheld.placedUnder, 60, kScreenRight, kScreenBottom);
    CHECK_EQ(shifted.For(16, 198, kScreenRight), 12);
    // Status is a view: it goes where windows start, or by the player's own shift for it.
    const FramePlan status = PlanOf(WindowViews(), StatusFrame(), shifts, 64);
    CHECK(!status.chosen && status.placedUnder == 64);
    shifts[*ViewOf(WindowViews(), StatusFrame())] = 30;
    CHECK(PlanOf(WindowViews(), StatusFrame(), shifts, 64).chosen == 30);
    CHECK(!PlanOf(WindowViews(), check, shifts, 64).chosen);
}

TEST(the_side_holding_the_view_s_highest_window_takes_its_shift_and_the_other_hangs_under_the_band_by_its_own)
{
    // Status: the status column at 16 on the left, and its menu at 48 on the right, where the Commands menu was.
    const auto column = [](const FrameShifts& s) { return s.For(16, 128, kScreenRight); };
    const auto menu   = [](const FrameShifts& s) { return s.For(1792, 1904, kScreenRight); };
    const FrameShifts placed = ShiftsOf(StatusFrame(), std::nullopt, 64, 64, kScreenRight, kScreenBottom);
    CHECK(column(placed) == 48 && menu(placed) == 16);
    const FrameShifts chosen = ShiftsOf(StatusFrame(), 30, 64, 30, kScreenRight, kScreenBottom); // a smaller panel
    CHECK(column(chosen) == 30 && menu(chosen) == 16);
    const FrameShifts pushed = ShiftsOf(StatusFrame(), std::nullopt, 64, 80, kScreenRight, kScreenBottom);
    CHECK(column(pushed) == 64 && menu(pushed) == 32);
    // Items: the list on the left and its sort menu on the right start together, so the player's shift moves both.
    const std::vector<FrameWindow> items = {{Key("buff"), 142, 48, 382, false}, {Key("inventor"), 16, 48, 198, false},
        {Key("itmsort2"), 1792, 48, 1904, false}, {Key("iteminfo"), 16, 992, 382, false}};
    const FrameShifts both = ShiftsOf(items, 20, 64, 64, kScreenRight, kScreenBottom);
    CHECK(both.For(16, 198, kScreenRight) == 20 && both.For(1792, 1904, kScreenRight) == 20);
    // A view on the right alone takes its shift there.
    const std::vector<FrameWindow> conquest = {{Key("conquer1"), 1792, 48, 1904, false}};
    CHECK(ShiftsOf(conquest, 6, 64, 50, kScreenRight, kScreenBottom).For(1792, 1904, kScreenRight) == 6);
}

TEST(a_window_the_game_hangs_under_the_strip_beside_the_status_column_rises_to_line_up_with_it)
{
    // Status > Magic Skills: the column in the strip's row, the list beside it a row lower, the Level box under it.
    std::vector<FrameWindow> magic = StatusFrame();
    magic.push_back({Key("mgcskill"), 130, 48, 382, false});
    const auto top = [](const FrameShifts& s, int left, int top, int right) {
        return top + s.For(left, right, kScreenRight);
    };
    const FrameShifts placed = ShiftsOf(magic, std::nullopt, 64, 64, kScreenRight, kScreenBottom);
    CHECK(top(placed, 16, 16, 128) == 64);
    CHECK(top(placed, 130, 48, 382) == 64);
    CHECK(top(placed, 16, 208, 128) == 256);
    CHECK(top(placed, 142, 48, 382) == 64); // the status icons stay where they were before Status opened
    CHECK(top(placed, 1792, 48, 1904) == 64);
    const FrameShifts chosen = ShiftsOf(magic, 30, 64, 30, kScreenRight, kScreenBottom);
    CHECK(top(chosen, 16, 16, 128) == 46 && top(chosen, 130, 48, 382) == 46);
    // Currencies hides the column under its list: nothing is beside it, and the icons stay too.
    std::vector<FrameWindow> currencies = StatusFrame();
    currencies[6].hidden = true;
    currencies.push_back({Key("evitem"), 16, 48, 382, false});
    const FrameShifts hidden = ShiftsOf(currencies, std::nullopt, 64, 64, kScreenRight, kScreenBottom);
    CHECK(top(hidden, 16, 48, 382) == 64 && top(hidden, 142, 48, 382) == 64);
    CHECK(FrameShifts{}.For(130, 382, kScreenRight) == 0); // windows not moved
}

TEST(the_full_map_covers_the_panel_and_other_windows_do_not)
{
    // The windows the test client drew with the map open, and before it in West Ronfaure (/hu windowlog).
    CHECK(MapShown({"logwindo", "map0", "mapframe", "partywin"}));
    CHECK(!MapShown({"buff", "logwin2", "logwindo", "partywin"}));
    CHECK(!MapShown({}));
}
