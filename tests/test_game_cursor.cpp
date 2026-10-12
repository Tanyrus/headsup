#include "game_cursor.h"
#include "boxes.h"
#include "test.h"

using namespace headsup;
using test::Box;

TEST(the_cursor_targets_come_from_the_target_slots)
{
    const CursorTargets plain = TargetsFromSlots(false, 1105, 0, true, 2304);
    CHECK_EQ(plain.target, 1105);
    CHECK_EQ(plain.subTarget, 0);
    CHECK(plain.locked);
    const CursorTargets picking = TargetsFromSlots(true, 0x220, 1105, false, 2304); // slot 1 is the target while picking
    CHECK_EQ(picking.target, 1105);
    CHECK_EQ(picking.subTarget, 0x220);
    CHECK_EQ(TargetsFromSlots(false, 5000, 0, false, 2304).target, 0); // past the entity map
}

TEST(a_pick_started_with_no_target_is_still_a_pick)
{
    // ITarget's flag, the target window's and slot 0 in four draw dumps: a pick with a target held, an <stnpc> pick
    // with none, a plain target, and nothing targeted after a pick.
    CHECK(Picking(true, true, 13));
    CHECK(Picking(false, true, 6));
    CHECK(!Picking(false, false, 13));
    CHECK(!Picking(false, true, 0));
    const CursorTargets alone = TargetsFromSlots(Picking(false, true, 6), 6, 0, false, 2304);
    CHECK_EQ(alone.target, 0);
    CHECK_EQ(alone.subTarget, 6);
}

TEST(the_target_windows_anchors_scale_from_the_menu_to_the_back_buffer)
{
    const CursorWindow window{724.0f, 333.0f, 1294.0f, 329.0f};
    CursorTargets targets;
    PlaceAnchors(targets, window, 1280.0f, 720.0f, 2560.0f, 1080.0f); // twice as wide, half again as tall
    CHECK(targets.anchored);
    CHECK(targets.anchorX == 1448.0f && targets.anchorY == 499.5f);
    CHECK(targets.subAnchorX == 2588.0f && targets.subAnchorY == 493.5f);
    struct Sizes
    {
        float menuWidth, menuHeight, backBufferWidth, backBufferHeight;
    };
    for (const Sizes& unknown : {Sizes{0.0f, 720.0f, 2560.0f, 1080.0f}, Sizes{1280.0f, 0.0f, 2560.0f, 1080.0f},
             Sizes{1280.0f, 720.0f, 0.0f, 0.0f}})
    {
        CursorTargets none;
        PlaceAnchors(none, window, unknown.menuWidth, unknown.menuHeight, unknown.backBufferWidth, unknown.backBufferHeight);
        CHECK(!none.anchored);
    }
}

TEST(the_games_picking_arrow_is_red_out_of_range_and_blue_in_range)
{
    // The colors the target window's draw passes for the arrows (RVA 0x1514BB to 0x15153D), which two captures picking
    // the same player far and then close drew as FFD04040 and FF4040D0. The arrow over the main target stays gray.
    CHECK(PickedOutOfRange(0xE0D04040) == std::optional<bool>(true));
    CHECK(PickedOutOfRange(0xE04040D0) == std::optional<bool>(false));
    CHECK(PickedOutOfRange(0xE0D0D040) == std::optional<bool>(true)); // its third, yellow: not usable from here either
    CHECK(!PickedOutOfRange(0xE0808080).has_value());
    CHECK(!PickedOutOfRange(0).has_value()); // none drawn yet
}

TEST(the_game_pointer_is_synced_before_each_click)
{
    for (const uint32_t click : {0x201u, 0x203u, 0x204u, 0x206u, 0x207u, 0x209u, 0x20Bu, 0x20Du}) // down and double-click
        CHECK(ClickMessage(click));
    for (const uint32_t other : {0x200u, 0x202u, 0x205u, 0x208u, 0x20Au, 0x20Cu}) // a move, each button's up, the wheel
        CHECK(!ClickMessage(other));
}

TEST(the_menu_size_is_the_registrys_when_set)
{
    const MenuSize size = MenuSizeOf(1920.0f, 1080.0f, 2560.0f, 1440.0f);
    CHECK_EQ(size.width, 1920.0f);
    CHECK_EQ(size.height, 1080.0f);
}

TEST(the_menu_size_falls_back_to_the_back_buffer)
{
    const MenuSize size = MenuSizeOf(0.0f, 0.0f, 2560.0f, 1440.0f);
    CHECK_EQ(size.width, 2560.0f);
    CHECK_EQ(size.height, 1440.0f);
    const MenuSize half = MenuSizeOf(1920.0f, 0.0f, 2560.0f, 1440.0f);
    CHECK_EQ(half.width, 1920.0f);
    CHECK_EQ(half.height, 1440.0f);
    const MenuSize other = MenuSizeOf(0.0f, 1080.0f, 2560.0f, 1440.0f);
    CHECK_EQ(other.width, 2560.0f);
    CHECK_EQ(other.height, 1080.0f);
}
