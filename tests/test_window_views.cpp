#include "fake_memory.h"
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

    // Status as the game draws it: the strip's windows, the hidden menu it came from, the chat log at the bottom.
    std::vector<FrameWindow> StatusFrame()
    {
        return {{Key("helpwind"), 16, false}, {Key("titlewin"), 16, true}, {Key("buff"), 48, false},
            {Key("menuwind"), 48, true}, {Key("level"), 208, false}, {Key("statcom2"), 48, false},
            {Key("persona"), 16, false}, {Key("logwindo"), 1058, false}};
    }
}

TEST(a_frame_is_the_view_holding_most_of_its_shown_windows_and_the_smaller_one_on_a_tie)
{
    std::vector<FrameWindow> frame = StatusFrame();
    CHECK(ViewOf(kViews, frame) == std::optional<size_t>(1));
    frame.push_back({Key("profile"), 48, false});
    CHECK(ViewOf(kViews, frame) == std::optional<size_t>(2));
    frame.back().hidden = true; // a hidden window is not part of what is shown
    CHECK(ViewOf(kViews, frame) == std::optional<size_t>(1));
    const std::vector<FrameWindow> nothing = {{Key("helpwind"), 16, false}, {Key("logwindo"), 1058, false}};
    CHECK(!ViewOf(kViews, nothing).has_value());
}

TEST(a_frame_s_top_is_its_highest_shown_window_that_hangs_from_the_top_and_is_not_the_strip)
{
    CHECK(FrameTop(StatusFrame(), kScreenBottom) == std::optional<int>(16));
    std::vector<FrameWindow> currencies = StatusFrame();
    currencies[6].hidden = true; // the portrait, under the list
    CHECK(FrameTop(currencies, kScreenBottom) == std::optional<int>(48));
    const std::vector<FrameWindow> bottomOnly = {{Key("helpwind"), 16, false}, {Key("iteminfo"), 992, false},
        {Key("logwindo"), 1058, false}};
    CHECK(!FrameTop(bottomOnly, kScreenBottom).has_value());
    // The map's corner frame, 106 x 14 at the screen's corner, draws nothing: the map's own windows are the top.
    const std::vector<FrameWindow> map = {{Key("buff"), 48, false}, {Key("mapframe"), 0, false}, {Key("map0"), 48, false}};
    CHECK(FrameTop(map, kScreenBottom) == std::optional<int>(48));
}

TEST(a_frame_goes_down_or_up_by_its_view_s_shift_and_no_overlap_pushes_it_down_only)
{
    // The band at the reference settings: 62 with the help bar, 38 without it.
    CHECK(FrameShift(16, std::nullopt, 62, 62, false) == 46);
    CHECK(FrameShift(48, std::nullopt, 62, 62, false) == 14);
    CHECK(FrameShift(48, std::nullopt, 38, 38, false) == -10);
    CHECK(FrameShift(16, 20, 62, 62, false) == 20);   // the player's own shift for the view
    CHECK(FrameShift(16, 20, 62, 62, true) == 46);    // pushed clear of the band
    CHECK(FrameShift(48, std::nullopt, 62, 80, true) == 32); // the bar grew past where the views were placed
    CHECK(FrameShift(48, std::nullopt, 62, 30, true) == 14); // and a smaller bar leaves them where they are
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
