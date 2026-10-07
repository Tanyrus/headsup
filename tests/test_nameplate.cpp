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
    ActorInfo Mob(uint16_t index)
    {
        ActorInfo mob{.index = index, .kind = EntityKind::Mob, .alive = true, .name = "Goblin Bounty Hunter",
            .label = {"Lv 17-20 EP-EM", LabelShade::EvenMatch}};
        mob.mobIcons.count = 3;
        return mob;
    }

    ActorInfo Player(uint16_t index)
    {
        ActorInfo player{.index = index, .kind = EntityKind::Player, .alive = true, .name = "Shio"};
        player.playerIcons.left.count  = 2;
        player.playerIcons.right.count = 1;
        return player;
    }

    struct Frame
    {
        bool steady      = true;
        bool selfEngaged = false;
        bool iconsFailed = false;
        CursorTargets targets{};
    };

    PlateLines Lines(const ActorInfo& info, const Settings& settings, const Frame& frame = {})
    {
        return ChooseLines(info, frame.steady, frame.selfEngaged, settings, frame.targets, frame.iconsFailed);
    }
}

TEST(a_mob_shows_its_level_line_and_icons_once_its_name_is_steady)
{
    const Settings s;
    const PlateLines mob = Lines(Mob(0x220), s);
    CHECK(mob.name && mob.label && mob.mobIconCount == 3 && mob.leftIconCount == 0 && mob.cursor == CursorKind::None);
    const PlateLines early = Lines(Mob(0x220), s, {.steady = false});
    CHECK(early.name && !early.label && early.mobIconCount == 0 && early.Any()); // the name never waits
    ActorInfo dead = Mob(0x220);
    dead.alive     = false;
    const PlateLines corpse = Lines(dead, s);
    CHECK(corpse.name && !corpse.label && corpse.mobIconCount == 0);
    ActorInfo unlabeled     = Mob(0x220); // level and con off and no ID (LevelLine)
    unlabeled.label.text[0] = '\0';
    const PlateLines bare   = Lines(unlabeled, s);
    CHECK(!bare.label && bare.mobIconCount == 3);
    Settings kept        = s;
    kept.replaceMobNames = false;
    CHECK(!Lines(Mob(0x220), kept, {.steady = false}).Any());
}

TEST(a_replaced_players_name_has_their_icons_beside_it)
{
    const Settings s;
    const PlateLines player = Lines(Player(0x400), s);
    CHECK(player.name && !player.label && player.mobIconCount == 0 && player.leftIconCount == 2 && player.rightIconCount == 1);
    Settings kept           = s;
    kept.replacePlayerNames = false;
    const PlateLines game   = Lines(Player(0x400), kept);
    CHECK(!game.name && game.leftIconCount == 0 && game.rightIconCount == 0);
    Settings plain        = s;
    plain.showPlayerIcons = false;
    const PlateLines bare = Lines(Player(0x400), plain);
    CHECK(bare.name && bare.leftIconCount == 0 && bare.rightIconCount == 0);
}

TEST(no_icons_once_their_textures_have_failed_or_are_off)
{
    Settings s;
    const PlateLines mob    = Lines(Mob(0x220), s, {.iconsFailed = true});
    const PlateLines player = Lines(Player(0x400), s, {.iconsFailed = true});
    CHECK(mob.label && mob.mobIconCount == 0 && player.name && player.leftIconCount == 0 && player.rightIconCount == 0);
    s.showIcons = false;
    CHECK(Lines(Mob(0x220), s).mobIconCount == 0 && Lines(Player(0x400), s).leftIconCount == 2);
}

TEST(the_settings_can_hide_the_level_and_icons_of_mobs_your_party_or_anyone_claimed_or_too_weak)
{
    struct Case
    {
        bool ActorInfo::*fact;
        bool Settings::*setting;
    };
    for (const Case& c : {Case{&ActorInfo::claimedByParty, &Settings::hideClaimedByParty}, Case{&ActorInfo::claimed, &Settings::hideClaimed},
             Case{&ActorInfo::tooWeak, &Settings::hideTooWeak}})
    {
        ActorInfo mob = Mob(0x220);
        mob.*c.fact   = true;
        Settings s;
        CHECK(Lines(mob, s).label); // off by default
        s.*c.setting            = true;
        const PlateLines hidden = Lines(mob, s, {.targets = {0x220, 0, false}});
        CHECK(!hidden.label && hidden.mobIconCount == 0);
        CHECK(hidden.name && hidden.cursor == CursorKind::Target); // the rest stays
        CHECK(Lines(Mob(0x220), s).label);                        // only the mobs it is about
    }
}

TEST(the_settings_can_hide_every_level_line_and_mob_icon_while_you_fight)
{
    Settings s;
    CHECK(Lines(Mob(0x220), s, {.selfEngaged = true}).label); // off by default
    s.hideWhileEngaged      = true;
    const PlateLines hidden = Lines(Mob(0x220), s, {.selfEngaged = true});
    CHECK(!hidden.label && hidden.mobIconCount == 0 && hidden.name);
    CHECK(Lines(Player(0x400), s, {.selfEngaged = true}).leftIconCount == 2); // a player's icons stay
    CHECK(Lines(Mob(0x220), s).label);                                       // only while you fight
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
    CHECK(lone[0].index == 554 && lone[0].kind == CursorKind::Target && lone[0].x == 1400.0f && lone[0].y == 608.0f);
    CHECK(LoneCursors(Anchored(554, 0), s, {554}).empty()); // its nameplate already has one
    CHECK(LoneCursors(Anchored(0, 0), s, {}).empty());
    CursorTargets unknown = Anchored(554, 0);
    unknown.anchored      = false; // the menu's size was not known
    CHECK(LoneCursors(unknown, s, {}).empty());
    Settings off = s;
    off.replaceCursor = false;
    CHECK(LoneCursors(Anchored(554, 0), off, {}).empty());
}

TEST(while_picking_the_candidate_takes_the_sub_anchor_and_the_target_the_main_one)
{
    const Settings s;
    CursorTargets t = Anchored(1105, 554);
    t.outOfRange    = true;
    const auto lone = LoneCursors(t, s, {});
    CHECK_EQ(lone.size(), 2u);
    CHECK(lone[0].index == 554 && lone[0].kind == CursorKind::OutOfRange && lone[0].x == 900.0f && lone[0].y == 300.0f);
    CHECK(lone[1].index == 1105 && lone[1].kind == CursorKind::Target && lone[1].x == 1400.0f);
    const auto same = LoneCursors(Anchored(554, 554), s, {}); // picking the target itself: one cursor, the candidate's
    CHECK(same.size() == 1u && same[0].kind == CursorKind::SubTarget && same[0].y == 300.0f);
}

TEST(a_lone_cursor_points_at_its_anchor)
{
    // A 20x32 arrow pointing from its middle: its tip on the anchor, the rest above it.
    const CursorSpot spot = CursorAtAnchor(1400.0f, 608.0f, 20.0f, 32.0f, 0.5f);
    CHECK(Near(spot.x, 1390.0f) && Near(spot.y, 576.0f));
    CHECK(Near(CursorAtAnchor(1400.0f, 608.0f, 20.0f, 32.0f, 0.25f).x, 1395.0f));
}

TEST(the_cursor_marks_the_target_locked_on_or_being_picked)
{
    const Settings s;
    CHECK(Lines(Mob(1105), s, {.targets = {1105, 0, false}}).cursor == CursorKind::Target);
    CHECK(Lines(Mob(1105), s, {.targets = {1105, 0, true}}).cursor == CursorKind::Locked);
    CHECK(Lines(Mob(0x220), s, {.targets = {1105, 0x220, true}}).cursor == CursorKind::SubTarget);
    CHECK(Lines(Mob(0x221), s, {.targets = {1105, 0x220, false}}).cursor == CursorKind::None);
    // Picking something out of range for the spell or ability: only the candidate's cursor says so.
    CHECK(Lines(Mob(0x220), s, {.targets = {1105, 0x220, false, true}}).cursor == CursorKind::OutOfRange);
    CHECK(Lines(Mob(1105), s, {.targets = {1105, 0x220, false, true}}).cursor == CursorKind::Target);
    Settings off      = s;
    off.replaceCursor = false;
    CHECK(Lines(Mob(1105), off, {.targets = {1105, 0, false}}).cursor == CursorKind::None);
}

TEST(a_players_icons_line_up_against_their_name)
{
    const ScreenBox plate = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    const LineSizes sizes{.nameWidth = 100.0f, .nameHeight = 18.0f, .leftIconCount = 2, .rightIconCount = 1, .playerIconSize = 16.0f};
    const NameplateLayout l = LayoutNameplate(plate, sizes, true);
    CHECK(Near(l.nameX, 990.0f));          // the name stays centered on the game's
    CHECK(Near(l.leftIconsX, 954.0f));     // two 16 px icons and their gap, 2 px left of it
    CHECK(Near(l.rightIconsX, 1092.0f));   // 2 px after the 100 px name
    CHECK(Near(l.playerIconsY, 197.0f));   // centered on the name's 18 px
    CHECK(Near(l.playerIconStep, 18.0f));
}

TEST(a_name_and_its_icons_can_be_centered_together)
{
    const ScreenBox plate = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    LineSizes sizes{.nameWidth = 100.0f, .nameHeight = 18.0f, .cursorWidth = 20.0f, .cursorHeight = 16.0f, .leftIconCount = 2,
        .playerIconSize = 16.0f, .centerNameAndIcons = true};
    const NameplateLayout l = LayoutNameplate(plate, sizes, true);
    CHECK(Near(l.leftIconsX, 972.0f)); // 34 px of icons, a 2 px gap and the 100 px name, centered on 1040
    CHECK(Near(l.nameX, 1008.0f));
    CHECK(Near(l.cursor.x, 1030.0f)); // the cursor stays over the center
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

TEST(a_drawn_name_has_its_label_and_icons_stacked_above_it)
{
    // Game name centered at (1040, 205). Name 100x18, label 80x16, three 16 px icons.
    const ScreenBox plate = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    const LineSizes sizes{.nameWidth = 100.0f, .nameHeight = 18.0f, .labelWidth = 80.0f, .labelHeight = 16.0f, .mobIconCount = 3,
        .iconSize = 16.0f};
    const NameplateLayout l = LayoutNameplate(plate, sizes, true);
    CHECK(Near(l.nameX, 990.0f) && Near(l.nameY, 196.0f));   // centered on the game's name
    CHECK(Near(l.labelX, 1000.0f) && Near(l.labelY, 183.0f)); // overlapping the name's text box by 3 px
    CHECK(Near(l.iconsX, 1014.0f) && Near(l.iconsY, 165.0f)); // 52 px wide row, 2 px above the label
    CHECK(Near(l.iconStep, 18.0f));
}

TEST(without_our_name_the_lines_stack_above_the_games)
{
    const ScreenBox plate   = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    const NameplateLayout l = LayoutNameplate(plate, {.labelWidth = 80.0f, .labelHeight = 16.0f, .mobIconCount = 1, .iconSize = 16.0f}, false);
    CHECK(Near(l.labelX, 1000.0f) && Near(l.labelY, 182.0f)); // 2 px above the game's name
    CHECK(Near(l.iconsX, 1032.0f) && Near(l.iconsY, 164.0f));
    const NameplateLayout iconsOnly = LayoutNameplate(plate, {.mobIconCount = 2, .iconSize = 16.0f}, false);
    CHECK(Near(iconsOnly.iconsY, 182.0f));
}

TEST(the_cursor_sits_above_the_top_line)
{
    const ScreenBox plate = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    const NameplateLayout withIcons = LayoutNameplate(plate, {.nameWidth = 100.0f, .nameHeight = 18.0f, .labelWidth = 80.0f,
        .labelHeight = 16.0f, .mobIconCount = 3, .iconSize = 16.0f, .cursorWidth = 20.0f, .cursorHeight = 16.0f}, true);
    CHECK(Near(withIcons.cursor.x, 1030.0f) && Near(withIcons.cursor.y, 147.0f)); // 2 px above the icons at 165
    const NameplateLayout labelOnly = LayoutNameplate(plate, {.labelWidth = 80.0f, .labelHeight = 16.0f, .cursorWidth = 20.0f,
        .cursorHeight = 16.0f}, false);
    CHECK(Near(labelOnly.cursor.y, 164.0f)); // 2 px above the label at 182
}

TEST(the_cursors_point_sits_over_the_center)
{
    // A shape that points from a quarter of its width.
    const ScreenBox plate   = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    const LineSizes sizes{.labelWidth = 80.0f, .labelHeight = 16.0f, .cursorWidth = 20.0f, .cursorHeight = 16.0f, .cursorTip = 0.25f};
    CHECK(Near(LayoutNameplate(plate, sizes, false).cursor.x, 1035.0f));
}

TEST(a_name_is_centered_over_its_entity_not_its_letters)
{
    // Shio: the game centers the letters and their icons together over the player, at x 965.1.
    const ScreenBox letters = Box(933.9f, 444.8f, 1054.8f, 466.7f);
    const ScreenBox whole   = Box(875.4f, 444.8f, 1054.8f, 477.7f);
    const ScreenBox placed  = PlaceName(letters, &whole, nullptr, WorldPoint{}, Pose::Standing);
    CHECK(Near(placed.CenterX(), 965.1f));
    CHECK(Near(placed.Width(), letters.Width()) && Near(placed.minY, letters.minY) && Near(placed.maxY, letters.maxY));
    CHECK(Near(PlaceName(letters, &letters, nullptr, WorldPoint{}, Pose::Standing).minX, letters.minX)); // no icons: nothing moves
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
    const ScreenBox plate   = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    const NameplateLayout l = LayoutNameplate(plate, {.nameWidth = 100.0f, .nameHeight = 18.0f, .cursorWidth = 20.0f,
        .cursorHeight = 16.0f}, true);
    CHECK(Near(l.nameY, 196.0f));
    CHECK(Near(l.cursor.y, 183.0f));
}

TEST(your_timers_stack_over_your_name_under_the_cursor)
{
    // Two 12 px timer lines over the name at y 196, the soonest on top, then the cursor.
    const ScreenBox plate   = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    const NameplateLayout l = LayoutNameplate(plate, {.nameWidth = 100.0f, .nameHeight = 18.0f, .cursorWidth = 20.0f,
        .cursorHeight = 16.0f, .timerCount = 2, .timerHeight = 12.0f}, true);
    CHECK(Near(l.timersY, 173.0f) && Near(l.timerStep, 14.0f)); // the lower ends at 199, overlapping the name by 3 px
    CHECK(Near(l.centerX, 1040.0f));
    CHECK(Near(l.cursor.y, 155.0f)); // 2 px above the top timer
}

TEST(the_cursor_bobs_up_from_its_place_and_back_again)
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
