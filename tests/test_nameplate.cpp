#include "nameplate.h"
#include "boxes.h"
#include "plate_compose.h"
#include "test.h"

#include <algorithm>
#include <cmath>
#include <functional>

using namespace headsup;
using test::Box;
using test::Near;

TEST(a_name_shows_whenever_some_of_it_is_on_screen)
{
    for (const ScreenBox& edge : {Box(1000.0f, 200.0f, 1080.0f, 210.0f), Box(-5.0f, 200.0f, 60.0f, 210.0f),
             Box(2500.0f, 200.0f, 2570.0f, 210.0f), Box(1000.0f, -3.0f, 1080.0f, 7.0f), Box(1000.0f, 1435.0f, 1080.0f, 1445.0f)})
        CHECK(NameOnScreen(edge, 2560.0f, 1440.0f));
    for (const ScreenBox& off : {Box(-90.0f, 200.0f, -10.0f, 210.0f), Box(2570.0f, 200.0f, 2650.0f, 210.0f),
             Box(1000.0f, -20.0f, 1080.0f, -10.0f), Box(1000.0f, 1450.0f, 1080.0f, 1460.0f), ScreenBox{}})
        CHECK(!NameOnScreen(off, 2560.0f, 1440.0f));
}

namespace
{
    // The game's arrow anchors on screen: from a capture targeting a Telepoint, which has no name, its anchor sat one
    // unit above it at (1400, 608).
    CursorTargets Anchored(uint16_t target, uint16_t subTarget)
    {
        CursorTargets t{target, subTarget, false};
        t.anchored   = true;
        t.anchorX    = 1400.0f, t.anchorY = 608.0f;
        t.subAnchorX = 900.0f, t.subAnchorY = 300.0f;
        return t;
    }
}

TEST(a_target_without_a_name_gets_a_cursor_at_the_games_arrow)
{
    const Settings s;
    const auto lone = LoneCursors(Anchored(554, 0), s, {});
    CHECK_EQ(lone.size(), 1u);
    CHECK(lone[0].index == 554 && lone[0].target == TargetState::Targeted && lone[0].x == 1400.0f && lone[0].y == 608.0f);
    CHECK(LoneCursors(Anchored(554, 0), s, {554}).empty()); // its nameplate already has one
    CHECK(LoneCursors(Anchored(0, 0), s, {}).empty());
    CursorTargets unknown = Anchored(554, 0);
    unknown.anchored      = false; // the menu's size was not known
    CHECK(LoneCursors(unknown, s, {}).empty());
    Settings off = s;
    off.cursorStyle = CursorStyle::Vanilla;
    CHECK(LoneCursors(Anchored(554, 0), off, {}).empty());
}

// The Mog House's door on the test client (2026-10-12): the window's anchor at (1706,418) of a 1920x1080 menu, which
// the debug report gave as (2274.7,557.3) on the 2560x1440 back buffer, and no nameplate anywhere for it.
TEST(a_targeted_door_gets_the_cursor_at_the_games_anchor)
{
    constexpr uint32_t kEntities = 2304;
    constexpr float kReportRounding = 0.05f; // the report prints one decimal
    const Settings s;
    // Slot 0 was active with index 0 and the model flag, slot 1 neither.
    const uint16_t empty = SlotTarget(0, false, false, kEntities);
    CursorTargets door   = TargetsFromSlots(false, SlotTarget(0, true, true, kEntities), empty, false);
    PlaceAnchors(door, CursorWindow{1706.0f, 418.0f, 0.0f, 0.0f}, 1920.0f, 1080.0f, 2560.0f, 1440.0f);
    const auto lone = LoneCursors(door, s, {1024}); // your own nameplate is the only one
    CHECK_EQ(lone.size(), 1u);
    if (lone.empty()) return;
    CHECK(lone[0].target == TargetState::Targeted && Near(lone[0].x, 2274.7f, kReportRounding) &&
          Near(lone[0].y, 557.3f, kReportRounding));
    door.locked = true;
    CHECK(LoneCursors(door, s, {})[0].target == TargetState::Locked);
    // Untargeted, the window keeps its last anchor and the slot is not active: nothing is drawn there.
    CursorTargets none = TargetsFromSlots(false, empty, empty, false);
    PlaceAnchors(none, CursorWindow{1706.0f, 418.0f, 0.0f, 0.0f}, 1920.0f, 1080.0f, 2560.0f, 1440.0f);
    CHECK(LoneCursors(none, s, {}).empty());
}

// The same door's anchor on the 2560 by 1440 back buffer, from debug reports on the test client: in view, off the
// screen's left edge with the camera turned, and behind the camera, where the game leaves it at the corner. With
// HeadsUp unloaded the game drew its own arrow in the first case only.
TEST(a_cursor_without_a_nameplate_shows_only_while_its_anchor_is_on_screen)
{
    constexpr float kWidth = 2560.0f, kHeight = 1440.0f;
    CHECK(AnchorOnScreen(2274.7f, 557.3f, kWidth, kHeight));
    CHECK(!AnchorOnScreen(-353.3f, 588.0f, kWidth, kHeight));
    CHECK(!AnchorOnScreen(0.0f, 0.0f, kWidth, kHeight));
    CHECK(!AnchorOnScreen(2600.0f, 588.0f, kWidth, kHeight));  // past the right edge
    CHECK(!AnchorOnScreen(1200.0f, -40.0f, kWidth, kHeight));  // above
    CHECK(!AnchorOnScreen(1200.0f, 1500.0f, kWidth, kHeight)); // below
}

TEST(while_picking_the_candidate_takes_the_sub_anchor_and_the_target_the_main_one)
{
    const Settings s;
    CursorTargets t = Anchored(1105, 554);
    t.outOfRange    = true;
    const auto lone = LoneCursors(t, s, {});
    CHECK_EQ(lone.size(), 2u);
    CHECK(lone[0].index == 554 && lone[0].target == TargetState::OutOfRange && lone[0].x == 900.0f && lone[0].y == 300.0f);
    CHECK(lone[1].index == 1105 && lone[1].target == TargetState::Targeted && lone[1].x == 1400.0f);
    // Picking the target itself: its own cursor at the main anchor, under the candidate's, as the game draws two arrows.
    const auto same = LoneCursors(Anchored(554, 554), s, {});
    CHECK_EQ(same.size(), 2u);
    CHECK(same[0].index == 554 && same[0].under && same[0].target == TargetState::Targeted && same[0].x == 1400.0f &&
          same[0].y == 608.0f);
    CHECK(same[1].index == 554 && !same[1].under && same[1].target == TargetState::SubTarget && same[1].x == 900.0f &&
          same[1].y == 300.0f);
    CHECK(!lone[0].under && !lone[1].under);
}

TEST(a_cursor_is_drawn_for_your_target_and_the_one_you_are_picking_alone)
{
    const Settings s;
    const CursorTargets picking = Anchored(1105, 554);
    CHECK(CursorShown(1105, s, picking) && CursorShown(554, s, picking));
    CHECK(!CursorShown(700, s, picking));
    CHECK(!CursorShown(0, s, picking) && !CursorShown(0, s, CursorTargets{})); // index 0 is no entity, target or not
    Settings off;
    off.cursorStyle = CursorStyle::Vanilla;
    CHECK(!CursorShown(1105, off, picking));
    CursorTargets unhooked   = picking;
    unhooked.gameArrowsShown = true; // the game draws its own arrows
    CHECK(!CursorShown(1105, s, unhooked) && !CursorShown(554, s, unhooked));
    CursorTargets unanchored = picking;
    unanchored.anchored      = false; // a cursor on a nameplate hangs on the name, not the game's anchor
    CHECK(CursorShown(1105, s, unanchored));
}

TEST(a_lone_cursor_points_at_its_anchor)
{
    // A 20x32 arrow pointing from its middle: its tip on the anchor, the rest above it.
    const CursorSpot spot = CursorAtAnchor(1400.0f, 608.0f, 20.0f, 32.0f, 0.5f);
    CHECK(Near(spot.x, 1390.0f) && Near(spot.y, 576.0f));
    CHECK(Near(CursorAtAnchor(1400.0f, 608.0f, 20.0f, 32.0f, 0.25f).x, 1395.0f));
    // Raised, it stands that far over the anchor; a raise below nothing lowers it onto it.
    CHECK(Near(CursorAtAnchor(1400.0f, 608.0f, 20.0f, 32.0f, 0.5f, 5.0f).y, 571.0f));
    CHECK(Near(CursorAtAnchor(1400.0f, 608.0f, 20.0f, 32.0f, 0.5f, -10.0f).y, 586.0f));
    CHECK(Near(CursorAtAnchor(1400.0f, 608.0f, 20.0f, 32.0f, 0.5f, 5.0f).x, 1390.0f));
}

TEST(every_width_step_widens_the_cursors_picture_however_small_it_is_shown)
{
    // The tester's report: on the feather, a width step only sometimes changed the width. A cursor walked away from
    // and back, scaling or not, at heights from the smallest to the largest.
    for (const bool scales : {true, false})
        for (int height = kMinCursorSide; height <= kMaxCursorSide; height += 4)
        {
            int raster = 0;
            for (int tenths = 40; tenths <= 1200; tenths += 7)
                for (const float shown : {tenths / 10.0f, 124.0f - tenths / 10.0f})
                {
                    raster = CursorRaster(shown, raster, scales, height);
                    for (int width = kMinCursorSide; width < kMaxCursorSide; ++width)
                        if (CursorPixelWidth(raster, width + 1, height) <= CursorPixelWidth(raster, width, height))
                        {
                            CHECK(false);
                            return;
                        }
                }
        }
}

TEST(a_cursors_picture_is_drawn_at_its_shown_height_and_never_below_its_height_setting)
{
    // Scaling, the steps of 1.25 from 6 pixels first pass 100 at 109.1; kept the same size, the shown height itself.
    CHECK_EQ(CursorRaster(100.0f, 0, true, 20), 109);
    CHECK_EQ(CursorRaster(40.0f, 0, false, 20), 40);
    CHECK_EQ(CursorRaster(40.4f, 0, false, 20), 40);
    // Shown smaller than its height setting, it is drawn at that setting and shrunk.
    CHECK_EQ(CursorRaster(5.0f, 0, true, 20), 20);
    CHECK_EQ(CursorRaster(12.0f, 0, false, 20), 20);
}

TEST(a_cursors_picture_is_as_wide_for_its_height_as_the_settings_have_it)
{
    // Drawn 40 pixels tall where the settings say 9 by 20: 18 wide. An edge of 100 percent is a twentieth of the height.
    CHECK_EQ(CursorPixelWidth(40, 9, 20), 18);
    CHECK_EQ(CursorPixelWidth(20, 25, 20), 25);
    CHECK_EQ(CursorPixelWidth(10, 1, 64), 1); // never nothing
    CHECK_EQ(CursorEdgePixels(40, 100), 2);
    CHECK_EQ(CursorEdgePixels(40, 400), 8);
    CHECK_EQ(CursorEdgePixels(40, 50), 1);
    CHECK_EQ(CursorEdgePixels(8, 50), 1); // at least a pixel
}

namespace
{
    constexpr float kLeastScale = 0.5f, kMostScale = 2.5f; // the settings' defaults
}

TEST(scale_follows_the_game_name_with_limits)
{
    // 1440p: a typical game letter is 8 px (1440 / 180).
    CHECK(Near(DistanceScale(8.0f, 1440.0f, kLeastScale, kMostScale), 1.0f));
    CHECK(Near(DistanceScale(10.0f, 1440.0f, kLeastScale, kMostScale), 1.25f)); // smoothly, not in whole pixels
    CHECK(Near(DistanceScale(100.0f, 1440.0f, kLeastScale, kMostScale), 2.5f)); // at most 2.5x
    CHECK(Near(DistanceScale(1.0f, 1440.0f, kLeastScale, kMostScale), 0.5f));   // at least 0.5x
    CHECK(Near(DistanceScale(std::nanf(""), 1440.0f, kLeastScale, kMostScale), 1.0f));
    CHECK(Near(DistanceScale(8.0f, 0.0f, kLeastScale, kMostScale), 1.0f));
    // The limits are the caller's.
    CHECK(Near(DistanceScale(100.0f, 1440.0f, 0.8f, 1.2f), 1.2f));
    CHECK(Near(DistanceScale(1.0f, 1440.0f, 0.8f, 1.2f), 0.8f));
    CHECK(Near(DistanceScale(8.0f, 1440.0f, 0.8f, 1.2f), 1.0f));
}

TEST(text_is_redrawn_only_when_its_size_moves_a_step)
{
    CHECK_EQ(RasterHeight(15.0f, 0), 18);  // the first size at or above it: 6 px times 1.25 steps
    CHECK_EQ(RasterHeight(15.0f, 18), 18); // shrunk by less than a step: drawn smaller
    CHECK_EQ(RasterHeight(14.5f, 18), 18);
    CHECK_EQ(RasterHeight(18.8f, 18), 18); // grown by less than 5%: drawn a little larger
    CHECK_EQ(RasterHeight(19.0f, 18), 23);
    CHECK_EQ(RasterHeight(14.0f, 18), 15);
    CHECK_EQ(RasterHeight(15.2f, 15), 15); // and back up is not a step yet: no flicker at the edge
    CHECK_EQ(RasterHeight(2.0f, 0), 6);    // the smallest
}

TEST(text_is_drawn_no_taller_than_the_tallest_step_and_stretched_past_it)
{
    // Up to 180 px (48 px letters at 2.5 times their size in an image 1.5 times the screen) the limit moves no step.
    struct Case
    {
        float pixels;
        int current, raster;
    };
    for (const Case& c : {Case{55.0f, 0, 56}, Case{56.0f, 0, 70}, Case{72.0f, 70, 70}, Case{108.0f, 0, 109}, Case{135.0f, 0, 136},
             Case{137.0f, 136, 136}, Case{150.0f, 136, 171}, Case{170.0f, 171, 171}, Case{179.0f, 171, 171}, Case{180.0f, 171, 213},
             Case{180.0f, 0, 213}, Case{200.0f, 213, 213}, Case{213.0f, 0, 213}})
        CHECK_EQ(RasterHeight(c.pixels, c.current), c.raster);
    // 768 px letters in that image: the tallest step, which the quad stretches.
    CHECK_EQ(RasterHeight(1152.0f, 0), 213);
    CHECK_EQ(RasterHeight(214.0f, 0), 213);
    CHECK_EQ(RasterHeight(1152.0f, 171), 213);
    // Once there, no size above it asks for another raster, so nothing is redrawn as the size moves.
    for (const float pixels : {181.0f, 213.0f, 224.0f, 266.0f, 300.0f, 1152.0f})
        CHECK_EQ(RasterHeight(pixels, 213), 213);
    CHECK_EQ(RasterHeight(169.0f, 213), 171); // and a smaller size steps down from it

    // Text that keeps its size is drawn at it, to the same limit.
    CHECK_EQ(FixedRasterHeight(72.4f), 72);
    CHECK_EQ(FixedRasterHeight(0.2f), 1);
    CHECK_EQ(FixedRasterHeight(213.0f), 213);
    CHECK_EQ(FixedRasterHeight(288.0f), 213);
}

TEST(a_text_too_large_for_a_texture_is_drawn_smaller_until_its_image_fits)
{
    // Ink 1500 by 130 with a 20 px halo around it is 1540 by 170.
    CHECK(FitsTexture(1500, 130, 20, 2048));
    CHECK(FitsTexture(2008, 130, 20, 2048)); // exactly the side
    CHECK(!FitsTexture(2009, 130, 20, 2048));
    CHECK(!FitsTexture(300, 2009, 20, 2048)); // too tall
    CHECK_EQ(FittingRaster(100, 1500, 130, 20, 2048), 100);
    CHECK_EQ(FittingRaster(100, 2008, 130, 20, 2048), 100);
    CHECK_EQ(FittingRaster(100, 4056, 130, 20, 2048), 50);  // 4096 wide: twice too wide
    CHECK_EQ(FittingRaster(100, 400000, 130, 20, 2048), 6); // never below the smallest step
    CHECK_EQ(FittingRaster(100, 2000, 130, 40, 2048), 98);  // the halo counts: 2080 wide
    CHECK_EQ(FittingRaster(100, 2000, 130, 24, 2048), 100); // and 2048 with a thinner one
    CHECK_EQ(FittingRaster(200, 300, 2260, 150, 2048), 160); // a glow's halo can make it too tall: 2560
    CHECK_EQ(FittingRaster(213, 4990, 300, 0, 4096), 174);  // 174.8, not rounded up: at 175 it is 4 px too wide
    CHECK_EQ(FittingRaster(4, 400000, 130, 20, 2048), 4);   // a height under the smallest step is not raised to it
}

namespace
{
    // Letters that each advance a whole number of pixels, as GDI's do, under a text's shadow margin. asked counts
    // the heights it is measured at.
    std::function<ImageSize(int)> Letters(int count, float em, int& asked)
    {
        return [count, em, &asked](int height) {
            ++asked;
            const auto size = static_cast<float>(height);
            const int halo  = static_cast<int>(std::ceil(size * 0.12f + std::round(size * 0.05f)));
            return ImageSize{count * static_cast<int>(std::lround(em * size)), height, halo};
        };
    }
}

TEST(a_long_text_steps_down_until_its_measured_image_fits)
{
    int asked = 0;
    CHECK_EQ(RasterThatFits(100, 4096, Letters(10, 0.5f, asked)).value_or(0), 100);
    CHECK_EQ(asked, 1); // it fits as it is
    // 100 letters of 0.85 em are 11648 px at 136, which scales to 47, where they measure 4016.
    asked = 0;
    CHECK_EQ(RasterThatFits(136, 4096, Letters(100, 0.85f, asked)).value_or(0), 47);
    CHECK_EQ(asked, 2);
    // From 171 and from 213 they scale to 48, where whole-pixel advances make them 4116: one step more, so the line
    // is drawn at every distance and not at some.
    asked = 0;
    CHECK_EQ(RasterThatFits(171, 4096, Letters(100, 0.85f, asked)).value_or(0), 47);
    CHECK_EQ(asked, 3);
    CHECK_EQ(RasterThatFits(213, 4096, Letters(100, 0.85f, asked)).value_or(0), 47);
    // 255 letters of 0.3 em from 213 scale to 53, measure 4100 there and at 52, and 3845 at 51.
    asked = 0;
    CHECK_EQ(RasterThatFits(213, 4096, Letters(255, 0.3f, asked)).value_or(0), 51);
    CHECK_EQ(asked, 4);
    // Too long at the smallest step: no height, and it is not asked there twice.
    asked = 0;
    CHECK(!RasterThatFits(213, 4096, Letters(2000, 1.0f, asked)).has_value());
    CHECK_EQ(asked, 2);
    asked = 0;
    CHECK(!RasterThatFits(4, 4096, Letters(2000, 1.0f, asked)).has_value());
    CHECK_EQ(asked, 1);
}

TEST(the_cursor_bobs_up_from_its_place_and_back_again)
{
    float lowest = 0.0f, highest = -100.0f;
    for (double t = 0.0; t < 3.0; t += 0.01)
    {
        const float bob = CursorBob(t, 20.0f, true);
        CHECK(Near(bob, CursorBob(t + 1.2, 20.0f, true)));
        lowest  = std::min(lowest, bob);
        highest = std::max(highest, bob);
        CHECK_EQ(CursorBob(t, 20.0f, false), 0.0f); // one that does not float stays in its place
    }
    CHECK(Near(lowest, -3.0f, 0.05f)); // 15% of 20 px, upward
    CHECK(Near(highest, 0.0f, 0.05f));
}

TEST(your_target_and_the_mob_you_are_picking_draw_over_every_other_plate)
{
    CursorTargets targets;
    targets.target    = 0x220;
    targets.subTarget = 0x221;
    CHECK_EQ(PlateRank(0x222, targets), 0);
    CHECK(PlateRank(0x220, targets) > PlateRank(0x222, targets));
    CHECK(PlateRank(0x221, targets) > PlateRank(0x220, targets));
    CHECK_EQ(PlateRank(0, CursorTargets{}), 0); // no target is not entity 0's
    CHECK(DrawnBefore(0, 0.99f, 0, 0.98f));  // within a rank, farther plates first
    CHECK(!DrawnBefore(0, 0.98f, 0, 0.99f));
    CHECK(DrawnBefore(0, 0.90f, 1, 0.99f));  // the target last, though it is farther
    CHECK(!DrawnBefore(1, 0.99f, 0, 0.90f));
}

TEST(a_plate_eases_toward_the_games_stepped_name_size)
{
    CHECK_EQ(EaseScale(0.0f, 1.4f, 0.016), 1.4f); // a new plate starts at the size
    CHECK_EQ(EaseScale(1.0f, 1.2f, 0.0), 1.0f);
    // Two thirds of the way (1 - 1/e) after one time constant of 0.15 s, all of it long after.
    CHECK(std::fabs(EaseScale(1.0f, 1.2f, 0.15) - (1.0f + 0.2f * (1.0f - std::exp(-1.0f)))) < 1e-4f);
    CHECK(std::fabs(EaseScale(1.0f, 1.2f, 5.0) - 1.2f) < 1e-4f);
}

TEST(a_frame_redraws_textures_until_its_budget_is_spent_but_always_draws_a_first_one)
{
    CHECK(RedrawNow(true, 0.0));
    CHECK(!RedrawNow(true, kRedrawBudgetMs));
    CHECK(RedrawNow(false, kRedrawBudgetMs * 10.0)); // a plate with nothing to show yet never waits
}

TEST(no_cursor_is_drawn_over_arrows_the_game_could_not_be_kept_from_drawing)
{
    const Settings s;
    CursorTargets targets{1105, 0, false};
    targets.gameArrowsShown = true;
    targets.anchored        = true;
    CHECK(LoneCursors(targets, s, {}).empty());
}

TEST(a_badge_hangs_past_its_icons_bottom_right_corner)
{
    const IconSpot at = BadgeAt(IconSpot{1041.0f, 160.2f}, 16.0f, 10.0f, 7.0f);
    CHECK(Near(at.x, 1052.6f) && Near(at.y, 174.0f));
}

TEST(a_badge_is_readable_on_a_default_icon_and_leaves_the_icons_top_half_clear)
{
    const Settings s;
    const float iconSize = static_cast<float>(s.baseIconSize * s.Kind(PlateKind::Mobs).lines[0].iconSize) / 100.0f;
    CHECK(iconSize * kBadgeShare >= 10.0f);             // its text box, about 7 px of digits
    CHECK(1.0f + kBadgeHang - kBadgeShare >= 0.5f); // where its box starts, as a share of the icon from the icon's top
}

TEST(a_nameplates_opacity_goes_by_what_you_target_and_by_distance_when_it_fades_in)
{
    OpacitySettings o;
    o.noTarget    = 80;
    o.notTargeted = 40;
    o.targeted    = 90;
    CHECK_EQ(PlateAlpha(o, kPlainRank, false, 5.0f), 204u); // you have no target
    CHECK_EQ(PlateAlpha(o, kPlainRank, true, 5.0f), 102u);  // you have one, and it is another
    CHECK_EQ(PlateAlpha(o, kTargetRank, true, 5.0f), 230u); // it is this one
    CHECK_EQ(PlateAlpha(o, kPickedRank, true, 5.0f), 230u); // or this is the one you are picking
    CHECK_EQ(PlateAlpha(o, kPickedRank, false, 5.0f), 230u);
    CHECK_EQ(PlateAlpha(o, kPlainRank, true, 45.0f), 102u); // distance counts for nothing unless it fades in
    // Fading in, what you do not target is whole within 10 yalms and a hundredth at the range, evenly between.
    o.fadeIn = true;
    o.range  = 50.0f;
    CHECK_EQ(PlateAlpha(o, kPlainRank, true, 8.0f), 102u);
    CHECK_EQ(PlateAlpha(o, kPlainRank, true, 30.0f), 52u); // 102 * 0.505
    CHECK_EQ(PlateAlpha(o, kPlainRank, true, 50.0f), 1u);
    CHECK_EQ(PlateAlpha(o, kPlainRank, false, 30.0f), 103u); // 204 * 0.505
    // Your target's, and the one you are picking, never fades with distance: it is as opaque as Is Targeted says.
    CHECK_EQ(PlateAlpha(o, kTargetRank, true, 30.0f), 230u);
    CHECK_EQ(PlateAlpha(o, kTargetRank, true, 50.0f), 230u);
    CHECK_EQ(PlateAlpha(o, kPickedRank, true, 45.0f), 230u);
    o.range = 30.0f;
    CHECK_EQ(PlateAlpha(o, kPlainRank, false, 20.0f), 103u); // half way to a nearer range
    CHECK_EQ(PlateAlpha(OpacitySettings{}, kPlainRank, true, 45.0f), kOpaqueAlpha); // a new install's: whole
    CHECK_EQ(WithAlpha(0xFF123456, 102), 0x66123456u);
    CHECK_EQ(WithAlpha(0x80123456, kOpaqueAlpha), 0x80123456u);
}

TEST(text_keeps_its_shape_in_a_scene_image_of_another_shape_than_the_screen)
{
    // An image the screen's shape and 1.5 times its size: a 120 x 30 texture is drawn as it is, its 3 px halo 2 screen
    // pixels up and left of the text's corner.
    CHECK(PlaceSquarePixels(100.0f, 50.0f, 120.0f, 30.0f, 3.0f, 1.0f, 1.5f, 1.5f) == (SquareQuad{98.0f, 48.0f, 120.0f, 30.0f}));
    // A 2048 x 2048 image for a 1920 x 1080 screen: text sized for the image's height is drawn 0.5625 as wide, and
    // the game's copy to the screen gives it back the 4 to 1 shape it was drawn with.
    const float toX = 2048.0f / 1920.0f, toY = 2048.0f / 1080.0f;
    const SquareQuad q = PlaceSquarePixels(100.0f, 50.0f, 120.0f, 30.0f, 0.0f, 1.0f, toX, toY);
    CHECK(Near(q.width, 67.5f) && Near(q.height, 30.0f));
    CHECK(Near((q.width / toX) / (q.height / toY), 4.0f));
    // A fit scales both ways alike.
    CHECK(Near(PlaceSquarePixels(0.0f, 0.0f, 120.0f, 30.0f, 0.0f, 0.5f, toX, toY).width, 33.75f));
}

namespace
{
    constexpr float kStackGap = 2.0f;
}

TEST(plates_that_do_not_cover_each_other_keep_their_places)
{
    // One beside the other, and one well above.
    const std::vector<PlateBox> boxes{{100.0f, 200.0f, 200.0f, 230.0f}, {200.0f, 200.0f, 300.0f, 230.0f},
        {100.0f, 100.0f, 200.0f, 130.0f}};
    CHECK(StackRises(boxes, kStackGap) == (std::vector<float>{0.0f, 0.0f, 0.0f}));
}

TEST(a_plate_that_would_cover_one_placed_before_it_rises_clear_of_it)
{
    // The second starts 10 lower than the first and half across it: its bottom goes a gap above the first's top.
    const std::vector<PlateBox> boxes{{100.0f, 200.0f, 200.0f, 230.0f}, {150.0f, 210.0f, 250.0f, 240.0f}};
    CHECK(StackRises(boxes, kStackGap) == (std::vector<float>{0.0f, 42.0f}));
}

TEST(plates_stack_one_above_another_and_a_rise_onto_a_third_goes_over_it_too)
{
    // Three in one spot: 32 apart, each a gap over the last.
    const PlateBox spot{100.0f, 200.0f, 200.0f, 230.0f};
    CHECK(StackRises({spot, spot, spot}, kStackGap) == (std::vector<float>{0.0f, 32.0f, 64.0f}));
    // The third covers only the first, but clearing it lands on the second, placed above: it goes over both.
    const std::vector<PlateBox> boxes{{100.0f, 200.0f, 200.0f, 230.0f}, {100.0f, 160.0f, 200.0f, 190.0f},
        {150.0f, 205.0f, 250.0f, 235.0f}};
    CHECK(StackRises(boxes, kStackGap) == (std::vector<float>{0.0f, 0.0f, 77.0f}));
}

TEST(a_plates_rise_eases_to_where_it_should_be_and_settles_there)
{
    CHECK(EaseRise(0.0f, 40.0f, 0.0) == 0.0f);
    const float soon = EaseRise(0.0f, 40.0f, 0.05);
    CHECK(soon > 5.0f && soon < 40.0f);
    CHECK(EaseRise(soon, 40.0f, 0.05) > soon);
    CHECK(EaseRise(0.0f, 40.0f, 5.0) == 40.0f); // long after: there
    CHECK(EaseRise(39.8f, 40.0f, 0.001) == 40.0f); // within half a pixel: there, so it stops moving
    CHECK(EaseRise(40.0f, 0.0f, 5.0) == 0.0f);    // and back down when nothing is under it
}

TEST(a_plates_size_follows_how_far_it_is_as_the_games_own_names_do)
{
    // The game's own names, measured in one frame (a name dump): 0.5625 of full size 10.17 yalms from the camera and
    // 0.4125 at 14.55, which DistanceScale makes 2.373 and 1.740. The game steps its size; a plate's is smooth.
    CHECK(Near(ScaleAtDistance(10.174f, kLeastScale, kMostScale), 2.373f, 0.03f));
    CHECK(Near(ScaleAtDistance(14.55f, kLeastScale, kMostScale), 1.740f, 0.03f));
    CHECK(ScaleAtDistance(30.0f, kLeastScale, kMostScale) < ScaleAtDistance(29.0f, kLeastScale, kMostScale)); // every yalm tells: no steps
    CHECK(Near(ScaleAtDistance(1.0f, kLeastScale, kMostScale), 2.5f) && Near(ScaleAtDistance(0.0f, kLeastScale, kMostScale), 2.5f)); // at most 2.5x
    CHECK(Near(ScaleAtDistance(500.0f, kLeastScale, kMostScale), 0.5f));                                                          // at least 0.5x
    CHECK(Near(ScaleAtDistance(std::nanf(""), kLeastScale, kMostScale), 1.0f));
    // The limits are the caller's: a plate at your feet is the largest it may be, a far one the smallest.
    CHECK(Near(ScaleAtDistance(1.0f, 0.8f, 1.2f), 1.2f) && Near(ScaleAtDistance(0.0f, 0.8f, 1.2f), 1.2f));
    CHECK(Near(ScaleAtDistance(500.0f, 0.8f, 1.2f), 0.8f));
    CHECK(Near(ScaleAtDistance(14.55f, 0.8f, 2.0f), 1.740f, 0.03f));
}

TEST(your_own_name_can_keep_one_size)
{
    Settings s;
    CHECK(ScalesWithDistance(s, true) && ScalesWithDistance(s, false));
    s.scaleOwnName = false;
    CHECK(!ScalesWithDistance(s, true));
    CHECK(ScalesWithDistance(s, false)); // everyone else still scales
    s.scaleOwnName      = true;
    s.scaleWithDistance = false; // the main switch turns every name off
    CHECK(!ScalesWithDistance(s, true) && !ScalesWithDistance(s, false));
}

TEST(a_cursor_is_kept_inside_the_screen)
{
    const CursorSpot inside = KeepOnScreen(CursorSpot{100.0f, 100.0f}, 20.0f, 32.0f, 2560.0f, 1440.0f);
    CHECK_EQ(inside.x, 100.0f);
    CHECK_EQ(inside.y, 100.0f);
    const CursorSpot above = KeepOnScreen(CursorSpot{100.0f, -50.0f}, 20.0f, 32.0f, 2560.0f, 1440.0f);
    CHECK_EQ(above.y, 0.0f);
    const CursorSpot right = KeepOnScreen(CursorSpot{2555.0f, 100.0f}, 20.0f, 32.0f, 2560.0f, 1440.0f);
    CHECK_EQ(right.x, 2540.0f);
    const CursorSpot left = KeepOnScreen(CursorSpot{-5.0f, 100.0f}, 20.0f, 32.0f, 2560.0f, 1440.0f);
    CHECK_EQ(left.x, 0.0f);
    const CursorSpot huge = KeepOnScreen(CursorSpot{50.0f, 50.0f}, 3000.0f, 2000.0f, 2560.0f, 1440.0f);
    CHECK_EQ(huge.x, 0.0f);
    CHECK_EQ(huge.y, 0.0f);
    const CursorSpot below = KeepOnScreen(CursorSpot{100.0f, 1430.0f}, 20.0f, 32.0f, 2560.0f, 1440.0f);
    CHECK_EQ(below.y, 1408.0f);
    // The top panel lies over the scene: a cursor that would go under it stays just below it, and one already
    // below it is where it was.
    CHECK_EQ(KeepOnScreen(CursorSpot{100.0f, 20.0f}, 20.0f, 32.0f, 2560.0f, 1440.0f, 61.0f).y, 61.0f);
    CHECK_EQ(KeepOnScreen(CursorSpot{100.0f, -50.0f}, 20.0f, 32.0f, 2560.0f, 1440.0f, 61.0f).y, 61.0f);
    CHECK_EQ(KeepOnScreen(CursorSpot{100.0f, 100.0f}, 20.0f, 32.0f, 2560.0f, 1440.0f, 61.0f).y, 100.0f);
    CHECK_EQ(KeepOnScreen(CursorSpot{100.0f, 1430.0f}, 20.0f, 32.0f, 2560.0f, 1440.0f, 61.0f).y, 1408.0f);
}
