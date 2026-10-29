#include "game_cursor.h"
#include "boxes.h"
#include "mouse_messages.h"
#include "test.h"

using namespace headsup;
using test::Box;

TEST(the_cursor_targets_come_from_the_target_slots)
{
    const CursorTargets plain = TargetsFromSlots(false, 1105, 0, true);
    CHECK_EQ(plain.target, 1105);
    CHECK_EQ(plain.subTarget, 0);
    CHECK(plain.locked);
    const CursorTargets picking = TargetsFromSlots(true, 0x220, 1105, false); // slot 1 is the target while picking
    CHECK_EQ(picking.target, 1105);
    CHECK_EQ(picking.subTarget, 0x220);
}

// Slot 0 in a Mog House, from three debug reports on the test client (2026-10-12): nothing targeted (index 0, not
// active), the Moogle (index 141, active), and the door, which is no entity (index 0, active, the model flag set).
TEST(a_target_that_is_no_entity_is_still_a_target)
{
    constexpr uint32_t kEntities = 2304;
    const uint16_t nothing = SlotTarget(0, false, false, kEntities), door = SlotTarget(0, true, true, kEntities);
    CHECK_EQ(nothing, 0);
    CHECK_EQ(SlotTarget(141, true, false, kEntities), 141);
    CHECK_EQ(door, kModelTarget);
    CHECK_EQ(SlotTarget(5000, true, false, kEntities), 0); // past the entity map
    CHECK(kModelTarget >= kEntities);                     // never an entity's own index
    // Not seen in game, so not taken for a target: either flag alone with no index.
    CHECK_EQ(SlotTarget(0, true, false, kEntities), 0);
    CHECK_EQ(SlotTarget(0, false, true, kEntities), 0);
    const CursorTargets targets = TargetsFromSlots(false, door, nothing, false);
    CHECK_EQ(targets.target, kModelTarget);
    CHECK_EQ(targets.subTarget, 0);
    // The same door as the candidate of a pick started with no target.
    CHECK(Picking(false, true, door));
    CHECK(!Picking(false, true, nothing));
}

TEST(a_pick_started_with_no_target_is_still_a_pick)
{
    // ITarget's flag, the target window's and slot 0 in four draw dumps: a pick with a target held, an <stnpc> pick
    // with none, a plain target, and nothing targeted after a pick.
    CHECK(Picking(true, true, 13));
    CHECK(Picking(false, true, 6));
    CHECK(!Picking(false, false, 13));
    CHECK(!Picking(false, true, 0));
    const CursorTargets alone = TargetsFromSlots(Picking(false, true, 6), 6, 0, false);
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

TEST(the_game_gets_a_release_only_for_a_press_it_got)
{
    GamePresses g;
    // A press the game got: its release reaches it, even over a popup that would keep it.
    CHECK(!g.Keep(kLeftDown, false));
    CHECK(!g.Keep(kLeftUp, true));
    // A press HeadsUp kept: its release is kept, even where nothing of HeadsUp's is under it.
    CHECK(g.Keep(kLeftDown, true));
    CHECK(g.Keep(kLeftUp, false));
    // A release with no press since loading is kept.
    CHECK(GamePresses{}.Keep(kRightUp, false));
}

TEST(a_double_click_the_game_ignores_keeps_the_release_after_it)
{
    GamePresses g;
    CHECK(!g.Keep(kLeftDown, false));
    CHECK(!g.Keep(kLeftUp, false));
    CHECK(!g.Keep(kLeftDouble, false)); // passed on, but the game acts on no double-click
    CHECK(g.Keep(kLeftUp, false));
    // A press whose release went to another window: the game still holds it, and the next release frees it.
    CHECK(!g.Keep(kLeftDown, false));
    CHECK(!g.Keep(kLeftDouble, false));
    CHECK(!g.Keep(kLeftUp, true));
}

TEST(each_button_is_paired_on_its_own_and_other_messages_go_as_decided)
{
    GamePresses g;
    CHECK(g.Keep(kLeftDown, true));
    CHECK(!g.Keep(kRightDown, false));
    CHECK(!g.Keep(kMiddleDown, false));
    CHECK(!g.Keep(kRightUp, true));
    CHECK(g.Keep(kLeftUp, false));
    CHECK(!g.Keep(kMiddleUp, false));
    for (const uint32_t other : {kMouseMove, kMouseWheel})
    {
        CHECK(g.Keep(other, true));
        CHECK(!g.Keep(other, false));
    }
}
