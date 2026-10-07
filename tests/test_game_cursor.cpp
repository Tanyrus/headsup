#include "game_cursor.h"
#include "boxes.h"
#include "game_glyphs.h"
#include "test.h"

using namespace headsup;
using test::Box;

TEST(the_games_cursor_sits_on_its_anchor)
{
    // From captures: the game's cursor quads in the UI image and the anchors its target window held.
    CHECK(AtCursorAnchor(Box(1185.5f, 216.5f, 1205.5f, 248.5f), 1196.0f, 249.0f));
    CHECK(AtCursorAnchor(Box(713.5f, 300.5f, 733.5f, 332.5f), 724.0f, 333.0f));
    CHECK(AtCursorAnchor(Box(713.5f, 290.5f, 733.5f, 322.5f), 724.0f, 333.0f));   // bobbing
    CHECK(!AtCursorAnchor(Box(743.5f, 300.5f, 763.5f, 332.5f), 724.0f, 333.0f));  // beside it
    CHECK(!AtCursorAnchor(Box(713.5f, 380.5f, 733.5f, 412.5f), 724.0f, 333.0f));  // below it
    CHECK(!AtCursorAnchor(Box(713.5f, 200.5f, 733.5f, 232.5f), 724.0f, 333.0f));  // well above it
    // The target window's text, credited to the target too.
    CHECK(!AtCursorAnchor(Box(1796.5f, 995.5f, 1805.5f, 1006.5f), 1196.0f, 249.0f));
}

TEST(the_target_arrow_is_taller_than_wide)
{
    // From a capture: the target arrow, and the menu's pointer drawn from the same texture.
    CHECK(LooksLikeTargetArrow(Box(1185.5f, 216.5f, 1205.5f, 248.5f)));
    CHECK(!LooksLikeTargetArrow(Box(4.5f, 810.5f, 36.5f, 830.5f)));
}

TEST(the_games_cursor_is_the_quad_right_above_the_name)
{
    // From a capture: Goblin Tinkerer's name at (1407,261)-(1472,267) and the game's 20x32 cursor at
    // (1068.5,162.5)-(1088.5,194.5) in the 1920x1080 UI, which is 4/3 smaller than the 2560x1440 screen.
    const ScreenBox name   = Box(1407.0f, 261.0f, 1472.0f, 267.0f);
    const ScreenBox cursor = Box(1068.5f, 162.5f, 1088.5f, 194.5f).Scaled(4.0f / 3.0f, 4.0f / 3.0f);
    CHECK(IsGameCursor(cursor, name));
    CHECK(!IsGameCursor(Box(1484.7f, 216.7f, 1511.3f, 259.3f), name));        // beside the name
    CHECK(!IsGameCursor(Box(1424.7f, 126.7f, 1451.3f, 169.3f), name));        // well above it
    CHECK(!IsGameCursor(Box(1424.7f, 256.7f, 1451.3f, 299.3f), name));        // reaching below its top
    CHECK(!IsGameCursor(Box(0.0f, 0.0f, 2560.0f, 1440.0f), name));          // the screen-sized copy
    CHECK(!IsGameCursor(Box(48.1f, 855.8f, 98.8f, 878.8f), name));           // the target window
}

TEST(the_games_cursor_is_centered_on_a_players_name_and_icons)
{
    // From a capture: Shio's letters at (933.9,444.8)-(1054.8,466.7) with the game's icon strip against their left, and
    // the game's cursor at (713.5,300.5)-(733.5,332.5) in the 1920x1080 UI.
    const ScreenBox letters = Box(933.9f, 444.8f, 1054.8f, 466.7f);
    const ScreenBox icons   = Box(875.4f, 444.8f, 933.9f, 477.7f);
    const ScreenBox cursor  = Box(713.5f, 300.5f, 733.5f, 332.5f).Scaled(4.0f / 3.0f, 4.0f / 3.0f);
    CHECK(BesideName(icons, letters));
    ScreenBox whole = letters;
    whole.Add(icons);
    CHECK(IsGameCursor(cursor, whole));
    CHECK(!IsGameCursor(cursor, letters)); // centered on the letters alone, it would be missed
}

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

TEST(the_game_draws_cursors_at_the_target_windows_anchors)
{
    const CursorWindow window{724.0f, 333.0f, 1294.0f, 329.0f};
    const std::vector<CursorName> names{{1105, Box(933.9f, 444.8f, 1054.8f, 466.7f)}};
    const auto plain = GameCursorAnchors(names, window, false);
    CHECK_EQ(plain.size(), 1u); // the sub anchor keeps a stale position after picking ends
    CHECK(plain[0].index == 1105 && plain[0].x == 724.0f && plain[0].y == 333.0f);
    const auto picking = GameCursorAnchors(names, window, true);
    CHECK_EQ(picking.size(), 2u);
    CHECK(picking[1].x == 1294.0f && picking[1].y == 329.0f);
    CHECK(GameCursorAnchors({}, window, true).empty());
}

namespace
{
    constexpr uintptr_t kArrows = 0x14F59748, kFont = 0x15589C38;

    // The game's cursor in a capture: (713.5,300.5)-(733.5,332.5) in the 1920x1080 UI image, over Shio at the anchor.
    CursorQuad Arrow(uintptr_t texture = kArrows)
    {
        const ScreenBox ui = Box(713.5f, 300.5f, 733.5f, 332.5f);
        return CursorQuad{ui, ui.Scaled(4.0f / 3.0f, 4.0f / 3.0f), texture};
    }

    const std::vector<CursorName> kShio{{1105, Box(875.4f, 444.8f, 1054.8f, 477.7f)}};
    const std::vector<CursorAnchor> kAnchor{{1105, 724.0f, 333.0f}};
}

TEST(the_games_cursor_is_blocked_only_while_headsup_draws_one)
{
    CHECK(!MayBeGameCursor(Arrow(), kArrows, kAnchor, {}));
    CHECK(!JudgeGameCursor(Arrow(), kArrows, kFont, kAnchor, {}, 1105).block);
    CHECK(MayBeGameCursor(Arrow(), 0, kAnchor, kShio));
    CHECK(JudgeGameCursor(Arrow(), 0, kFont, kAnchor, kShio, std::nullopt).block); // on the anchor: whoever drew it
}

TEST(the_arrows_texture_is_learned_from_a_cursor_credited_to_its_entity)
{
    const CursorVerdict credited = JudgeGameCursor(Arrow(), 0, kFont, kAnchor, kShio, 1105);
    CHECK(credited.block && credited.learnArrow);
    CHECK(!JudgeGameCursor(Arrow(), 0, kFont, kAnchor, kShio, 1070).learnArrow); // credited to someone else
    CHECK(!JudgeGameCursor(Arrow(kFont), 0, kFont, kAnchor, kShio, 1105).learnArrow); // a letter of the font
    // A tall letter of the target window, on the anchor: too small to be the arrow.
    const ScreenBox letter = Box(721.0f, 320.0f, 727.0f, 332.0f);
    CHECK(!JudgeGameCursor(CursorQuad{letter, letter.Scaled(4.0f / 3.0f, 4.0f / 3.0f), 0x1234}, 0, kFont, kAnchor, kShio, 1105).learnArrow);
}

TEST(once_learned_any_arrow_in_its_texture_is_blocked)
{
    const ScreenBox elsewhere = Box(100.0f, 100.0f, 120.0f, 132.0f);
    const CursorQuad arrow{elsewhere, elsewhere.Scaled(4.0f / 3.0f, 4.0f / 3.0f), kArrows};
    CHECK(MayBeGameCursor(arrow, kArrows, kAnchor, kShio));
    CHECK(JudgeGameCursor(arrow, kArrows, kFont, kAnchor, kShio, std::nullopt).block);
    const ScreenBox pointer = Box(4.5f, 810.5f, 36.5f, 830.5f); // the menu's pointer, from the same texture
    CHECK(!JudgeGameCursor(CursorQuad{pointer, pointer.Scaled(4.0f / 3.0f, 4.0f / 3.0f), kArrows}, kArrows, kFont, kAnchor,
        kShio, std::nullopt).block);
}

TEST(the_games_picking_arrow_is_red_out_of_range_and_blue_in_range)
{
    // From two captures picking the same player: far, then close. The arrow over the main target stays gray.
    CHECK(PickedOutOfRange(0xFFD04040) == std::optional<bool>(true));
    CHECK(PickedOutOfRange(0xFF4040D0) == std::optional<bool>(false));
    CHECK(!PickedOutOfRange(0xFF808080).has_value());
}
