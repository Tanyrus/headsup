#include "nameplate.h"
#include "boxes.h"
#include "test.h"

#include <algorithm>
#include <cmath>

using namespace headsup;
using test::Box;
using test::Near;

TEST(labels_wait_for_a_drawn_body_and_a_steady_name)
{
    CHECK(Steady(0, kStableFrames));                     // body drawn this frame
    CHECK(Steady(kMeshGraceFrames, kStableFrames));      // or lately: on some frames the game's draws credit it to another
    CHECK(!Steady(kMeshGraceFrames + 1, kStableFrames)); // body not drawn
    CHECK(!Steady(0, kStableFrames - 1));                // name just appeared
}

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
    PlateFacts Facts(uint16_t index)
    {
        return PlateFacts{index, true, true, true, true, 3, 2, 1};
    }
}

TEST(a_plate_shows_the_lines_its_entity_and_the_settings_allow)
{
    const Settings s;
    const PlateLines all = ChooseLines(Facts(0x220), s, CursorTargets{}, false);
    CHECK(all.name && all.label && all.icons == 3 && all.leftIcons == 2 && all.rightIcons == 1 && all.cursor == CursorKind::None);
    PlateFacts unsteady = Facts(0x220);
    unsteady.steady     = false;
    const PlateLines early = ChooseLines(unsteady, s, CursorTargets{}, false);
    CHECK(early.name && !early.label && early.icons == 0); // the name never waits; labels and MobDB icons do
    PlateFacts dead = Facts(0x220);
    dead.alive      = false;
    CHECK(!ChooseLines(dead, s, CursorTargets{}, false).label);
    PlateFacts kept = Facts(0x220);
    kept.replaced   = false;
    const PlateLines keptLines = ChooseLines(kept, s, CursorTargets{}, false);
    CHECK(!keptLines.name && keptLines.leftIcons == 0 && keptLines.rightIcons == 0);
    const PlateLines failed = ChooseLines(Facts(0x220), s, CursorTargets{}, true);
    CHECK(failed.icons == 0 && failed.leftIcons == 0 && failed.rightIcons == 0 && failed.name);
    Settings off = s;
    off.showLabels = off.showIcons = off.showPlayerIcons = false;
    const PlateLines bare = ChooseLines(Facts(0x220), off, CursorTargets{}, false);
    CHECK(bare.name && !bare.label && bare.icons == 0 && bare.leftIcons == 0 && bare.rightIcons == 0);
    PlateFacts nothing = kept;
    nothing.steady     = false;
    CHECK(!ChooseLines(nothing, s, CursorTargets{}, false).Any());
}

TEST(the_cursor_marks_the_target_locked_on_or_being_picked)
{
    const Settings s;
    CHECK(ChooseLines(Facts(1105), s, CursorTargets{1105, 0, false}, false).cursor == CursorKind::Target);
    CHECK(ChooseLines(Facts(1105), s, CursorTargets{1105, 0, true}, false).cursor == CursorKind::Locked);
    CHECK(ChooseLines(Facts(0x220), s, CursorTargets{1105, 0x220, true}, false).cursor == CursorKind::SubTarget);
    CHECK(ChooseLines(Facts(0x221), s, CursorTargets{1105, 0x220, false}, false).cursor == CursorKind::None);
    // Picking something out of range for the spell or ability: only the candidate's cursor says so.
    CHECK(ChooseLines(Facts(0x220), s, CursorTargets{1105, 0x220, false, true}, false).cursor == CursorKind::OutOfRange);
    CHECK(ChooseLines(Facts(1105), s, CursorTargets{1105, 0x220, false, true}, false).cursor == CursorKind::Target);
    Settings off = s;
    off.replaceCursor = false;
    CHECK(ChooseLines(Facts(1105), off, CursorTargets{1105, 0, false}, false).cursor == CursorKind::None);
}

TEST(name_icons_line_up_against_the_name)
{
    const ScreenBox plate = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    LineSizes sizes{100.0f, 18.0f, 0.0f, 0.0f, 0, 16.0f};
    sizes.leftIconCount = 2;
    sizes.rightIconCount = 1;
    sizes.nameIconSize  = 16.0f;
    const NameplateLayout l = LayoutNameplate(plate, sizes, true);
    CHECK(Near(l.nameX, 990.0f));                // the name stays centered on the game's
    CHECK(Near(l.leftIconsX, 954.0f));           // two 16 px icons and their gap, 2 px left of it
    CHECK(Near(l.rightIconsX, 1092.0f));         // 2 px after the 100 px name
    CHECK(Near(l.nameIconsY, 197.0f));           // centered on the name's 18 px
    CHECK(Near(l.nameIconStep, 18.0f));
}

TEST(a_name_and_its_icons_can_be_centered_together)
{
    const ScreenBox plate = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    LineSizes sizes{100.0f, 18.0f, 0.0f, 0.0f, 0, 16.0f, 20.0f, 16.0f};
    sizes.leftIconCount     = 2;
    sizes.nameIconSize      = 16.0f;
    sizes.centerNameAndIcons = true;
    const NameplateLayout l = LayoutNameplate(plate, sizes, true);
    CHECK(Near(l.leftIconsX, 972.0f)); // 34 px of icons, a 2 px gap and the 100 px name, centered on 1040
    CHECK(Near(l.nameX, 1008.0f));
    CHECK(Near(l.cursorX, 1030.0f)); // the cursor stays over the center
    sizes.rightIconCount = 1;
    const NameplateLayout both = LayoutNameplate(plate, sizes, true);
    CHECK(Near(both.leftIconsX, 963.0f)); // 34 + 2 + 100 + 2 + 16 = 154 px, centered on 1040
    CHECK(Near(both.nameX, 999.0f));
    CHECK(Near(both.rightIconsX, 1101.0f));
    sizes.leftIconCount = 0;
    const NameplateLayout right = LayoutNameplate(plate, sizes, true);
    CHECK(Near(right.nameX, 981.0f)); // 100 + 2 + 16 = 118 px, centered on 1040
    CHECK(Near(right.rightIconsX, 1083.0f));
    sizes.rightIconCount = 0;
    CHECK(Near(LayoutNameplate(plate, sizes, true).nameX, 990.0f));
}

TEST(replace_mode_stacks_icons_label_and_name)
{
    // Game name centered at (1040, 205). Name 100x18, label 80x16, three 16 px icons.
    const ScreenBox plate = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    const NameplateLayout l = LayoutNameplate(plate, LineSizes{100.0f, 18.0f, 80.0f, 16.0f, 3, 16.0f}, true);
    CHECK(Near(l.nameX, 990.0f) && Near(l.nameY, 196.0f));   // centered on the game's name
    CHECK(Near(l.labelX, 1000.0f) && Near(l.labelY, 183.0f)); // overlapping the name's text box by 3 px
    CHECK(Near(l.iconsX, 1014.0f) && Near(l.iconsY, 165.0f)); // 52 px wide row, 2 px above the label
    CHECK(Near(l.iconStep, 18.0f));
}

TEST(label_mode_stacks_above_the_game_name)
{
    const ScreenBox plate = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    const NameplateLayout l = LayoutNameplate(plate, LineSizes{0.0f, 0.0f, 80.0f, 16.0f, 1, 16.0f}, false);
    CHECK(Near(l.labelX, 1000.0f) && Near(l.labelY, 182.0f)); // 2 px above the game's name
    CHECK(Near(l.iconsX, 1032.0f) && Near(l.iconsY, 164.0f));
    const NameplateLayout iconsOnly = LayoutNameplate(plate, LineSizes{0.0f, 0.0f, 0.0f, 0.0f, 2, 16.0f}, false);
    CHECK(Near(iconsOnly.iconsY, 182.0f));
}

TEST(the_cursor_sits_above_the_top_line)
{
    const ScreenBox plate = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    const NameplateLayout withIcons =
        LayoutNameplate(plate, LineSizes{100.0f, 18.0f, 80.0f, 16.0f, 3, 16.0f, 20.0f, 16.0f}, true);
    CHECK(Near(withIcons.cursorX, 1030.0f) && Near(withIcons.cursorY, 147.0f)); // 2 px above the icons at 165
    const NameplateLayout labelOnly =
        LayoutNameplate(plate, LineSizes{0.0f, 0.0f, 80.0f, 16.0f, 0, 16.0f, 20.0f, 16.0f}, false);
    CHECK(Near(labelOnly.cursorY, 164.0f)); // 2 px above the label at 182
}

TEST(the_cursors_point_sits_over_the_center)
{
    const ScreenBox plate = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    LineSizes sizes{0.0f, 0.0f, 80.0f, 16.0f, 0, 16.0f, 20.0f, 16.0f};
    sizes.cursorTip = 0.25f; // a shape that points from a quarter of its width
    CHECK(Near(LayoutNameplate(plate, sizes, false).cursorX, 1035.0f));
}

TEST(a_name_is_centered_over_its_entity_not_its_letters)
{
    // Shio: the game centers the letters and its icons together over the player, at x 965.1.
    const ScreenBox letters  = Box(933.9f, 444.8f, 1054.8f, 466.7f);
    const ScreenBox whole    = Box(875.4f, 444.8f, 1054.8f, 477.7f);
    const ScreenBox centered = CenteredOver(letters, whole);
    CHECK(Near(centered.CenterX(), 965.1f));
    CHECK(Near(centered.Width(), letters.Width()) && Near(centered.minY, letters.minY) && Near(centered.maxY, letters.maxY));
    CHECK(Near(CenteredOver(letters, letters).minX, letters.minX)); // no icons: nothing moves
}

TEST(scale_follows_the_game_name_with_limits)
{
    // 1440p: a typical game letter is 8 px (1440 / 180).
    CHECK(Near(DistanceScale(8.0f, 1440.0f), 1.0f));
    CHECK(Near(DistanceScale(10.0f, 1440.0f), 1.25f)); // smoothly, not in whole pixels
    CHECK(Near(DistanceScale(100.0f, 1440.0f), 2.5f)); // at most 2.5x
    CHECK(Near(DistanceScale(1.0f, 1440.0f), 0.5f));   // at least 0.5x
    CHECK(Near(DistanceScale(std::nanf(""), 1440.0f), 1.0f));
    CHECK(Near(DistanceScale(8.0f, 0.0f), 1.0f));
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

TEST(the_cursor_sits_on_the_name_when_nothing_else_is_above_it)
{
    // A player: the name 100x18 centered at (1040, 205) and the cursor 20x16 straight above it, overlapping the name's box
    // by 3 px like the label would.
    const ScreenBox plate = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    LineSizes sizes;
    sizes.nameWidth = 100.0f, sizes.nameHeight = 18.0f, sizes.cursorWidth = 20.0f, sizes.cursorHeight = 16.0f;
    const NameplateLayout l = LayoutNameplate(plate, sizes, true);
    CHECK(Near(l.nameY, 196.0f));
    CHECK(Near(l.cursorY, 183.0f));
}

TEST(the_cursor_bobs_up_to_a_seventh_of_its_height_and_repeats)
{
    float lowest = 0.0f, highest = -100.0f;
    for (double t = 0.0; t < 3.0; t += 0.01)
    {
        const float bob = CursorBob(t, 20.0f);
        CHECK(Near(bob, CursorBob(t + 1.2, 20.0f)));
        lowest  = std::min(lowest, bob);
        highest = std::max(highest, bob);
    }
    CHECK(Near(lowest, -3.0f, 0.05f)); // 15% of 20 px, upward
    CHECK(Near(highest, 0.0f, 0.05f));
}
