#include "nameplate.h"
#include "boxes.h"
#include "test.h"

#include <algorithm>
#include <cmath>

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
        bool selfEngaged = false;
        bool iconsFailed = false;
        CursorTargets targets{};
    };

    PlateLines Lines(const ActorInfo& info, const Settings& settings, const Frame& frame = {})
    {
        return ChooseLines(info, frame.selfEngaged, settings, frame.targets, frame.iconsFailed);
    }
}

TEST(a_living_mob_shows_its_level_line_and_icons)
{
    const Settings s;
    const PlateLines mob = Lines(Mob(0x220), s);
    CHECK(mob.name && mob.label && mob.mobIconCount == 3 && mob.leftIconCount == 0 && mob.cursor == CursorKind::None);
    ActorInfo dead = Mob(0x220);
    dead.alive     = false;
    const PlateLines corpse = Lines(dead, s);
    CHECK(corpse.name && !corpse.label && corpse.mobIconCount == 0);
    ActorInfo unlabeled     = Mob(0x220); // level and con off and no ID (LevelLine)
    unlabeled.label.text[0] = '\0';
    const PlateLines bare   = Lines(unlabeled, s);
    CHECK(!bare.label && bare.mobIconCount == 3);
    Settings kept           = s;
    kept.replaceMobNames    = false;
    const PlateLines over   = Lines(Mob(0x220), kept); // above the game's own name
    CHECK(!over.name && over.label && over.mobIconCount == 3);
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
    CHECK(Near(l.iconsY, 165.0f)); // 2 px above the label
    // The detection icons start at the level line's left edge (1000), reading left to right.
    CHECK(Near(IconAt(l, l.detection, 0).x, 1000.0f) && Near(IconAt(l, l.detection, 2).x, 1036.0f));
    CHECK(Near(IconAt(l, l.detection, 0).y, 165.0f) && Near(l.detection.step, 18.0f));
}

TEST(without_our_name_the_lines_stack_above_the_games)
{
    const ScreenBox plate   = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    const NameplateLayout l = LayoutNameplate(plate, {.labelWidth = 80.0f, .labelHeight = 16.0f, .mobIconCount = 1, .iconSize = 16.0f}, false);
    CHECK(Near(l.labelX, 1000.0f) && Near(l.labelY, 182.0f)); // 2 px above the game's name
    CHECK(Near(IconAt(l, l.detection, 0).x, 1000.0f) && Near(l.iconsY, 164.0f)); // the level line's left edge
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

TEST(the_ornament_sits_between_the_level_line_and_the_name)
{
    // Game name centered at (1040, 205). Name 100x18, label 80x16, an ornament 108x7.
    const ScreenBox plate = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    LineSizes sizes;
    sizes.nameWidth = 100.0f, sizes.nameHeight = 18.0f, sizes.labelWidth = 80.0f, sizes.labelHeight = 16.0f;
    sizes.ornamentWidth = 108.0f, sizes.ornamentHeight = 7.0f;
    const NameplateLayout l = LayoutNameplate(plate, sizes, true);
    CHECK(Near(l.nameY, 196.0f));
    CHECK(Near(l.ornamentY, 190.0f) && Near(l.ornamentX, 986.0f)); // centered, 2 px under the label
    CHECK(Near(l.labelY, 172.0f));                                 // the label moved up to make room
}

TEST(no_ornament_leaves_the_label_where_it_was)
{
    const ScreenBox plate = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    LineSizes sizes;
    sizes.nameWidth = 100.0f, sizes.nameHeight = 18.0f, sizes.labelWidth = 80.0f, sizes.labelHeight = 16.0f;
    CHECK(Near(LayoutNameplate(plate, sizes, true).labelY, 183.0f));
}

TEST(your_name_and_other_players_names_can_each_keep_one_size)
{
    Settings s;
    CHECK(ScalesWithDistance(s, EntityKind::Player, true) && ScalesWithDistance(s, EntityKind::Player, false));
    s.scaleOwnName = false;
    CHECK(!ScalesWithDistance(s, EntityKind::Player, true));
    CHECK(ScalesWithDistance(s, EntityKind::Player, false)); // everyone else still scales
    s.scaleOwnName     = true;
    s.scalePlayerNames = false;
    CHECK(ScalesWithDistance(s, EntityKind::Player, true));
    CHECK(!ScalesWithDistance(s, EntityKind::Player, false));
    CHECK(ScalesWithDistance(s, EntityKind::Mob, false) && ScalesWithDistance(s, EntityKind::Npc, false));
    s.scalePlayerNames  = true;
    s.scaleWithDistance = false; // the main switch turns every name off
    CHECK(!ScalesWithDistance(s, EntityKind::Player, true) && !ScalesWithDistance(s, EntityKind::Mob, false));
}

TEST(each_kind_of_name_has_its_own_size)
{
    Settings s;
    s.mobNameSize    = 11;
    s.playerNameSize = 12;
    s.selfNameSize   = 13;
    s.npcNameSize    = 14;
    CHECK_EQ(NameSize(s, EntityKind::Mob, false), 11);
    CHECK_EQ(NameSize(s, EntityKind::Player, false), 12);
    CHECK_EQ(NameSize(s, EntityKind::Player, true), 13);
    CHECK_EQ(NameSize(s, EntityKind::Npc, false), 14);
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
    CHECK(Lines(Mob(1105), s, {.targets = targets}).cursor == CursorKind::None);
    targets.anchored = true;
    CHECK(LoneCursors(targets, s, {}).empty());
}

TEST(the_badges_end_at_the_right_edge_with_room_for_their_overhang)
{
    const ScreenBox plate = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    const LineSizes sizes{.nameWidth = 100.0f, .nameHeight = 18.0f, .labelWidth = 80.0f, .labelHeight = 16.0f, .mobIconCount = 3,
        .iconSize = 16.0f, .infoIconCount = 2};
    const NameplateLayout l = LayoutNameplate(plate, sizes, true);
    CHECK(Near(l.iconsY, 160.2f)); // the badges hang 4.8 px below the row, above the label's 2 px gap at 181
    // Three detection icons (52), a gap and two badges (45.2) need 99.2, more than the level line's 80.
    const IconSpot last = IconAt(l, l.info, 1);
    CHECK(Near(IconAt(l, l.info, 0).x, 1044.4f) && Near(last.x, 1068.0f)); // step 16 + 2 + 5.6
    CHECK(Near(last.x + 16.0f * (1.0f + kBadgeOverhang), 1089.6f)); // its badge ends at the row's right edge
    CHECK(Near(IconAt(l, l.detection, 0).x, 990.4f) && Near(IconAt(l, l.detection, 2).y, 160.2f));
}

TEST(the_row_lines_up_with_the_level_line_not_the_name)
{
    const ScreenBox plate = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    const LineSizes sizes{.nameWidth = 140.0f, .nameHeight = 18.0f, .labelWidth = 100.0f, .labelHeight = 16.0f, .mobIconCount = 1,
        .iconSize = 16.0f, .infoIconCount = 1};
    const NameplateLayout l = LayoutNameplate(plate, sizes, true);
    CHECK(Near(IconAt(l, l.detection, 0).x, 990.0f));
    CHECK(Near(IconAt(l, l.info, 0).x + 16.0f * (1.0f + kBadgeOverhang), 1090.0f));
}

TEST(without_a_level_line_the_row_lines_up_with_the_name)
{
    const ScreenBox plate = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    const LineSizes sizes{.nameWidth = 100.0f, .nameHeight = 18.0f, .mobIconCount = 1, .iconSize = 16.0f};
    CHECK(Near(IconAt(LayoutNameplate(plate, sizes, true), LayoutNameplate(plate, sizes, true).detection, 0).x, 990.0f));
}

TEST(swapped_the_badges_start_at_the_left_edge_and_the_detection_icons_end_at_the_right)
{
    const ScreenBox plate = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    const LineSizes sizes{.labelWidth = 80.0f, .labelHeight = 16.0f, .mobIconCount = 1, .iconSize = 16.0f, .infoIconCount = 2,
        .detectionLeft = false, .badgesLeft = true};
    const NameplateLayout l = LayoutNameplate(plate, sizes, false);
    CHECK(Near(IconAt(l, l.info, 0).x, 1000.0f) && Near(IconAt(l, l.info, 1).x, 1023.6f));
    CHECK(Near(IconAt(l, l.detection, 0).x + 16.0f, 1080.0f));
}

TEST(both_groups_on_the_right_leave_the_edge_to_the_badges)
{
    const ScreenBox plate = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    const LineSizes sizes{.labelWidth = 120.0f, .labelHeight = 16.0f, .mobIconCount = 3, .iconSize = 16.0f, .infoIconCount = 2,
        .detectionLeft = false};
    const NameplateLayout l = LayoutNameplate(plate, sizes, false);
    const IconSpot last = IconAt(l, l.info, 1);
    CHECK(Near(IconAt(l, l.info, 0).x, 1054.8f) && Near(last.x + 16.0f * (1.0f + kBadgeOverhang), 1100.0f));
    CHECK(Near(IconAt(l, l.detection, 2).x + 16.0f, 1052.8f)); // a gap short of the badges
    CHECK(Near(IconAt(l, l.detection, 0).x, 1000.8f));
}

TEST(both_groups_on_the_left_keep_the_badges_overhang_clear_of_the_detection_icons)
{
    const ScreenBox plate = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    const LineSizes sizes{.labelWidth = 120.0f, .labelHeight = 16.0f, .mobIconCount = 3, .iconSize = 16.0f, .infoIconCount = 2,
        .badgesLeft = true};
    const NameplateLayout l = LayoutNameplate(plate, sizes, false);
    const IconSpot last = IconAt(l, l.info, 1);
    CHECK(Near(IconAt(l, l.info, 0).x, 980.0f) && Near(last.x, 1003.6f));
    CHECK(Near(IconAt(l, l.detection, 0).x, last.x + 16.0f * (1.0f + kBadgeOverhang) + 2.0f)); // a gap past the last badge
    CHECK(Near(IconAt(l, l.detection, 0).x, 1027.2f));
}

TEST(groups_too_wide_for_the_name_widen_the_row_evenly_and_each_keeps_its_edge)
{
    const ScreenBox plate = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    const LineSizes sizes{.labelWidth = 120.0f, .labelHeight = 16.0f, .mobIconCount = 9, .iconSize = 16.0f, .infoIconCount = 1,
        .detectionLeft = false};
    const NameplateLayout l = LayoutNameplate(plate, sizes, false);
    // Eight detection icons (142), a gap and one badge with its overhang (21.6): 165.6 around the center at 1040.
    CHECK(Near(IconAt(l, l.detection, 0).x, 957.2f));
    CHECK(Near(IconAt(l, l.info, 0).x + 16.0f * (1.0f + kBadgeOverhang), 1122.8f));
    CHECK(Near(IconAt(l, l.detection, 8).x + 16.0f, 1099.2f)); // the ninth, alone on its row, still ends with the first row
}

TEST(each_group_wraps_at_eight_onto_rows_above_and_the_cursor_clears_them)
{
    const ScreenBox plate = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    const LineSizes sizes{.nameWidth = 100.0f, .nameHeight = 18.0f, .labelWidth = 80.0f, .labelHeight = 16.0f, .mobIconCount = 9,
        .iconSize = 16.0f, .infoIconCount = 10, .cursorWidth = 20.0f, .cursorHeight = 16.0f};
    const NameplateLayout l = LayoutNameplate(plate, sizes, true);
    CHECK(Near(l.iconRowStep, 22.8f)); // 16 + 4.8 of badge + 2
    // Eight detection icons (142), a gap and eight badges (186.8): 330.8 around the center at 1040.
    const IconSpot ninth = IconAt(l, l.info, 8); // the first of two on its row, which ends at the right edge
    CHECK(Near(ninth.x, 1160.2f) && Near(ninth.y, 137.4f));
    const IconSpot lone = IconAt(l, l.detection, 8); // alone on its row, still starting at the left edge
    CHECK(Near(lone.x, 874.6f) && Near(lone.y, 137.4f));
    CHECK(Near(l.cursor.y, 135.4f - 16.0f));
}

TEST(no_badge_group_leaves_the_row_where_it_was)
{
    const ScreenBox plate   = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    const NameplateLayout l = LayoutNameplate(plate, {.labelWidth = 80.0f, .labelHeight = 16.0f, .mobIconCount = 1, .iconSize = 16.0f}, false);
    CHECK(Near(l.iconsY, 164.0f) && l.info.count == 0);
}

TEST(a_badge_hangs_past_its_icons_bottom_right_corner)
{
    const IconSpot at = BadgeAt(IconSpot{1041.0f, 160.2f}, 16.0f, 10.0f, 7.0f);
    CHECK(Near(at.x, 1052.6f) && Near(at.y, 174.0f));
}

TEST(a_badge_is_readable_on_a_default_icon_and_leaves_the_icons_top_half_clear)
{
    const float iconSize = static_cast<float>(Settings{}.iconSize);
    CHECK(iconSize * kBadgeShare >= 10.0f);             // its text box, about 7 px of digits
    CHECK(1.0f + kBadgeHang - kBadgeShare >= 0.5f); // where its box starts, as a share of the icon from the icon's top
}

TEST(the_badge_group_hides_with_the_level_line_but_not_with_the_detection_icons)
{
    Settings s;
    ActorInfo mob       = Mob(0x220);
    mob.infoIcons.count = 2;
    CHECK(Lines(mob, s).infoIconCount == 2);
    CHECK(Lines(mob, s, {.iconsFailed = true}).infoIconCount == 0);
    s.showIcons = false;
    CHECK(Lines(mob, s).infoIconCount == 2 && Lines(mob, s).mobIconCount == 0);
    s.hideTooWeak = true;
    mob.tooWeak   = true;
    CHECK(Lines(mob, s).infoIconCount == 0);
    ActorInfo bare       = Mob(0x221);
    bare.mobIcons.count  = 0;
    bare.label.text[0]   = '\0';
    bare.infoIcons.count = 1;
    Settings plain;
    plain.replaceMobNames = false;
    CHECK(Lines(bare, plain).Any()); // a badge alone still makes a plate
}

TEST(a_badge_group_longer_than_the_detection_icons_still_lifts_the_cursor)
{
    const ScreenBox plate = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    const LineSizes sizes{.nameWidth = 100.0f, .nameHeight = 18.0f, .labelWidth = 80.0f, .labelHeight = 16.0f, .mobIconCount = 1,
        .iconSize = 16.0f, .infoIconCount = 9, .cursorWidth = 20.0f, .cursorHeight = 16.0f};
    const NameplateLayout l = LayoutNameplate(plate, sizes, true);
    CHECK(Near(l.cursor.y, 135.4f - 16.0f)); // two rows of badges: 160.2, then 137.4
}

TEST(other_mobs_plates_fade_while_you_have_a_target)
{
    Settings s;
    s.untargetedOpacity = 40;
    CHECK_EQ(PlateAlpha(s, EntityKind::Mob, kPlainRank, true), 102u);
    CHECK_EQ(PlateAlpha(s, EntityKind::Mob, kPlainRank, false), kOpaqueAlpha); // no target: nothing fades
    CHECK_EQ(PlateAlpha(s, EntityKind::Mob, kTargetRank, true), kOpaqueAlpha);
    CHECK_EQ(PlateAlpha(s, EntityKind::Player, kPlainRank, true), kOpaqueAlpha);
    CHECK_EQ(WithAlpha(0xFF123456, 102), 0x66123456u);
    CHECK_EQ(WithAlpha(0x80123456, kOpaqueAlpha), 0x80123456u);
}

TEST(centered_the_icon_groups_sit_together_over_the_middle)
{
    const ScreenBox plate = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    const LineSizes sizes{.labelWidth = 100.0f, .labelHeight = 16.0f, .mobIconCount = 1, .iconSize = 16.0f,
        .infoIconCount = 1, .iconPlace = IconPlace::Center};
    const NameplateLayout l = LayoutNameplate(plate, sizes, false);
    // One icon (16), a gap and one badge with its overhang (21.6): 39.6 around the center at 1040.
    CHECK(Near(IconAt(l, l.detection, 0).x, 1020.2f));
    CHECK(Near(IconAt(l, l.info, 0).x + 16.0f * (1.0f + kBadgeOverhang), 1059.8f));
}

TEST(inline_the_icon_groups_sit_on_the_level_line_at_their_sides)
{
    const ScreenBox plate = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    const LineSizes sizes{.labelWidth = 100.0f, .labelHeight = 16.0f, .mobIconCount = 1, .iconSize = 16.0f,
        .infoIconCount = 1, .iconPlace = IconPlace::Inline, .cursorWidth = 10.0f, .cursorHeight = 10.0f};
    const NameplateLayout l = LayoutNameplate(plate, sizes, false);
    const IconSpot detection = IconAt(l, l.detection, 0);
    CHECK(Near(detection.x + 16.0f, l.labelX - 4.0f)); // two gaps before the line
    CHECK(Near(IconAt(l, l.info, 0).x, l.labelX + 100.0f + 4.0f));
    CHECK(Near(detection.y, l.labelY));
    // Nothing above the level line moves up for them.
    const LineSizes alone{.labelWidth = 100.0f, .labelHeight = 16.0f, .iconSize = 16.0f, .cursorWidth = 10.0f,
        .cursorHeight = 10.0f};
    CHECK(Near(l.cursor.y, LayoutNameplate(plate, alone, false).cursor.y));
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

TEST(a_plates_box_holds_its_name_level_line_and_icon_rows)
{
    const ScreenBox plate = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    const LineSizes sizes{.nameWidth = 140.0f, .nameHeight = 18.0f, .labelWidth = 100.0f, .labelHeight = 16.0f,
        .mobIconCount = 1, .iconSize = 16.0f};
    const NameplateLayout l = LayoutNameplate(plate, sizes, true);
    const PlateBox box      = BoxOf(l, sizes, true);
    CHECK(Near(box.left, l.nameX) && Near(box.right, l.nameX + 140.0f)); // the name is its widest line
    CHECK(Near(box.top, l.iconsY) && Near(box.bottom, l.nameY + 18.0f));
    // A player's icons beside the name widen it.
    const LineSizes player{.nameWidth = 100.0f, .nameHeight = 18.0f, .leftIconCount = 2, .rightIconCount = 1,
        .playerIconSize = 16.0f};
    const NameplateLayout pl = LayoutNameplate(plate, player, true);
    const PlateBox wide      = BoxOf(pl, player, true);
    CHECK(Near(wide.left, pl.leftIconsX) && Near(wide.right, pl.rightIconsX + 16.0f));
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
    CHECK(Near(ScaleAtDistance(10.174f), 2.373f, 0.03f));
    CHECK(Near(ScaleAtDistance(14.55f), 1.740f, 0.03f));
    CHECK(ScaleAtDistance(30.0f) < ScaleAtDistance(29.0f)); // every yalm tells: no steps
    CHECK(Near(ScaleAtDistance(1.0f), 2.5f) && Near(ScaleAtDistance(0.0f), 2.5f)); // at most 2.5x
    CHECK(Near(ScaleAtDistance(500.0f), 0.5f));                                    // at least 0.5x
    CHECK(Near(ScaleAtDistance(std::nanf("")), 1.0f));
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
}
