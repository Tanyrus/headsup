#include "plate_layout.h"
#include "boxes.h"
#include "test.h"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

using namespace headsup;
using test::Near;

namespace
{
    ScreenBox Anchor() { ScreenBox b; b.Add(90.0f, 200.0f); b.Add(110.0f, 220.0f); return b; } // center 100, 210; top 200
    LineInput Text(std::initializer_list<ItemSize> items, Align align = Align::Center)
    {
        LineInput line;
        line.items   = items;
        line.align   = align;
        for (const ItemSize& item : items) line.textHeight = std::max(line.textHeight, item.height);
        return line;
    }
    LineInput Icons(std::initializer_list<ItemSize> items)
    {
        LineInput line;
        line.items    = items;
        line.hasIcons = true;
        return line;
    }
    PlateItem TextItem(const char* text)
    {
        PlateItem item;
        item.text = text;
        return item;
    }
    // A run of icons, the first badged of them with a badge.
    PlateItem IconItem(int count, int badged)
    {
        PlateItem item;
        item.kind = ItemKind::Icons;
        item.icons.resize(static_cast<size_t>(count));
        for (int i = 0; i < badged; ++i) item.icons[static_cast<size_t>(i)].badge = "+25%";
        return item;
    }
    PlateItem SpaceItem(int spaces)
    {
        PlateItem item;
        item.kind   = ItemKind::Space;
        item.spaces = spaces;
        return item;
    }
    ComposedLine Line(std::vector<PlateItem> items, float textSize, float iconSize, Align align = Align::Center)
    {
        ComposedLine line;
        line.items         = std::move(items);
        line.look.textSize = textSize;
        line.look.iconSize = iconSize;
        line.look.align    = align;
        return line;
    }
}

TEST(text_sinks_into_the_text_line_below_and_icons_stay_clear_of_it)
{
    // The default mob plate: an icon row, the level line, the name.
    const std::vector<LineInput> lines{Icons({{40.0f, 16.0f}}), Text({{60.0f, 13.0f}}), Text({{80.0f, 15.0f}})};
    const PlateLayout l = LayoutPlate(Anchor(), lines, 0.0f, 0.0f, 0.5f, 0, 0.0f);
    CHECK_EQ(l.lines.size(), size_t{3});
    CHECK_EQ(l.lines[2].top, 210.0f - 7.5f);
    CHECK_EQ(l.lines[2].left, 60.0f);
    CHECK_EQ(l.lines[1].top, l.lines[2].top + kNameOverlap - 13.0f);
    CHECK_EQ(l.lines[0].top, l.lines[1].top - kLineGap - 16.0f);
    CHECK_EQ(l.top, l.lines[0].top);
    CHECK_EQ(l.centerX, 100.0f);
}

TEST(nothing_sinks_into_an_icon_line_and_an_icon_line_never_sinks)
{
    const std::vector<LineInput> textOverIcons{Text({{60.0f, 13.0f}}), Icons({{40.0f, 16.0f}})};
    const PlateLayout a = LayoutPlate(Anchor(), textOverIcons, 0.0f, 0.0f, 0.5f, 0, 0.0f);
    CHECK_EQ(a.lines[0].top, a.lines[1].top - kLineGap - 13.0f);
    const std::vector<LineInput> iconsOverText{Icons({{40.0f, 16.0f}}), Text({{80.0f, 15.0f}})};
    const PlateLayout b = LayoutPlate(Anchor(), iconsOverText, 0.0f, 0.0f, 0.5f, 0, 0.0f);
    CHECK_EQ(b.lines[0].top, b.lines[1].top - kLineGap - 16.0f);
    LineInput mixed = Text({{20.0f, 16.0f}, {40.0f, 13.0f}}); // icons beside text: it has icons, so it stays clear
    mixed.textHeight = 13.0f;
    mixed.hasIcons   = true;
    const std::vector<LineInput> mixedOverText{mixed, Text({{80.0f, 15.0f}})};
    const PlateLayout c = LayoutPlate(Anchor(), mixedOverText, 0.0f, 0.0f, 0.5f, 0, 0.0f);
    CHECK_EQ(c.lines[0].top, c.lines[1].top - kLineGap - 16.0f);
}

TEST(items_sit_left_to_right_with_the_gap_and_center_on_the_line)
{
    const std::vector<LineInput> lines{Text({{20.0f, 16.0f}, {4.0f, 0.0f}, {30.0f, 12.0f}})};
    const PlateLayout l = LayoutPlate(Anchor(), lines, 0.0f, 0.0f, 0.5f, 0, 0.0f);
    const LinePlace& line = l.lines[0];
    CHECK_EQ(line.width, 20.0f + kItemGap + 4.0f + kItemGap + 30.0f);
    CHECK_EQ(line.height, 16.0f);
    CHECK_EQ(line.left, 100.0f - line.width / 2.0f);
    CHECK_EQ(line.items[0].x, line.left);
    CHECK_EQ(line.items[1].x, line.left + 20.0f + kItemGap);
    CHECK_EQ(line.items[2].x, line.left + 20.0f + kItemGap + 4.0f + kItemGap);
    CHECK_EQ(line.items[0].y, line.top);
    CHECK_EQ(line.items[2].y, line.top + 2.0f); // (16 - 12) / 2
}

TEST(lines_align_against_the_bottom_line)
{
    // The bottom line is 60 wide and centered on the name at 100, so it runs from 70 to 130 whatever its own
    // alignment says. A wider line set left starts with it and hangs past its right; a narrower one set right ends
    // with it.
    const std::vector<LineInput> lines{Text({{100.0f, 10.0f}}, Align::Left), Text({{40.0f, 10.0f}}, Align::Right),
        Text({{60.0f, 10.0f}}, Align::Left)};
    const PlateLayout l = LayoutPlate(Anchor(), lines, 0.0f, 0.0f, 0.5f, 0, 0.0f);
    CHECK_EQ(l.lines[2].left, 70.0f);
    CHECK_EQ(l.lines[0].left, 70.0f);
    CHECK_EQ(l.lines[1].left, 130.0f - 40.0f);
    // The widest line set right hangs past the bottom line's left.
    const std::vector<LineInput> wide{Text({{100.0f, 10.0f}}, Align::Right), Text({{60.0f, 10.0f}})};
    CHECK_EQ(LayoutPlate(Anchor(), wide, 0.0f, 0.0f, 0.5f, 0, 0.0f).lines[0].left, 130.0f - 100.0f);
}

TEST(the_ornament_sits_between_its_line_and_the_one_above)
{
    std::vector<LineInput> lines{Text({{60.0f, 13.0f}}), Text({{80.0f, 15.0f}})};
    lines[1].ornamentWidth  = 90.0f;
    lines[1].ornamentHeight = 6.0f;
    const PlateLayout l = LayoutPlate(Anchor(), lines, 0.0f, 0.0f, 0.5f, 0, 0.0f);
    CHECK_EQ(l.lines[1].ornament.y, l.lines[1].top + kNameOverlap - kLineGap - 6.0f);
    CHECK_EQ(l.lines[1].ornament.x, 100.0f - 45.0f);
    CHECK_EQ(l.lines[0].top, l.lines[1].ornament.y - kLineGap - 13.0f);
    std::vector<LineInput> overIcons{Text({{60.0f, 13.0f}}), Icons({{40.0f, 16.0f}})};
    overIcons[1].ornamentWidth  = 90.0f;
    overIcons[1].ornamentHeight = 6.0f;
    const PlateLayout m = LayoutPlate(Anchor(), overIcons, 0.0f, 0.0f, 0.5f, 0, 0.0f);
    CHECK_EQ(m.lines[1].ornament.y, m.lines[1].top - kLineGap - 6.0f);
}

TEST(the_cursor_sinks_into_a_text_top_line_and_your_timers_go_under_it)
{
    const std::vector<LineInput> lines{Text({{80.0f, 15.0f}})};
    const PlateLayout plain = LayoutPlate(Anchor(), lines, 20.0f, 32.0f, 0.5f, 0, 0.0f);
    CHECK_EQ(plain.cursor.x, 100.0f - 10.0f);
    CHECK_EQ(plain.cursor.y, plain.top + kNameOverlap - 32.0f);
    const PlateLayout timed = LayoutPlate(Anchor(), lines, 20.0f, 32.0f, 0.5f, 2, 13.0f);
    CHECK_EQ(timed.timerStep, 13.0f + kLineGap);
    CHECK_EQ(timed.timersY, timed.top + kNameOverlap + kLineGap - 2.0f * timed.timerStep);
    CHECK_EQ(timed.cursor.y, timed.timersY - kLineGap - 32.0f);
    const std::vector<LineInput> iconLine{Icons({{40.0f, 16.0f}})};
    const PlateLayout overIcons = LayoutPlate(Anchor(), iconLine, 20.0f, 32.0f, 0.5f, 0, 0.0f);
    CHECK_EQ(overIcons.cursor.y, overIcons.top - kLineGap - 32.0f);
}

TEST(a_plate_with_no_lines_puts_the_cursor_on_the_games_name)
{
    const std::vector<LineInput> none;
    const PlateLayout l = LayoutPlate(Anchor(), none, 20.0f, 32.0f, 0.5f, 0, 0.0f);
    CHECK_EQ(l.cursor.y, 200.0f - kLineGap - 32.0f);
    CHECK_EQ(l.cursor.x, 90.0f);
}

TEST(the_box_holds_every_item_and_ornament)
{
    std::vector<LineInput> lines{Text({{60.0f, 13.0f}}), Text({{80.0f, 15.0f}})};
    lines[1].ornamentWidth  = 120.0f;
    lines[1].ornamentHeight = 6.0f;
    const PlateLayout l = LayoutPlate(Anchor(), lines, 0.0f, 0.0f, 0.5f, 0, 0.0f);
    const PlateBox box  = BoxOf(l);
    CHECK_EQ(box.left, 40.0f);
    CHECK_EQ(box.right, 160.0f);
    CHECK_EQ(box.top, l.lines[0].top);
    CHECK_EQ(box.bottom, l.lines[1].top + 15.0f);
}

TEST(an_icon_row_is_its_icons_and_gaps_plus_each_badges_overhang)
{
    CHECK_EQ(IconRunWidth(3, 0, 16.0f), 3.0f * 16.0f + 2.0f * kIconGap);
    CHECK_EQ(IconRunWidth(3, 3, 16.0f), 3.0f * (16.0f + kIconGap + 16.0f * kBadgeOverhang) - kIconGap);
    CHECK_EQ(IconRunWidth(0, 0, 16.0f), 0.0f);
}

TEST(a_row_mixing_plain_and_badged_icons_steps_past_each_by_its_own_width)
{
    // The composer keeps badged icons in runs of their own, but a row counts its badges, so a mixed one holds: at
    // 16 px two plain icons 18 px apart, then three badged ones 23.6 px apart (a 5.6 px overhang each), 104.8 px.
    const float places[] = {0.0f, 18.0f, 36.0f, 59.6f, 83.2f};
    for (int i = 0; i < 5; ++i)
        CHECK(Near(IconOffset(i, std::max(0, i - 2), 16.0f), places[i]));
    CHECK(Near(IconRunWidth(5, 3, 16.0f), 104.8f));
    CHECK_EQ(IconRunWidth(5, 2, 16.0f), 5.0f * 16.0f + 2.0f * 16.0f * kBadgeOverhang + 4.0f * kIconGap);
    // The last icon's place, its width and its badge's overhang are the row's width: drawn as measured.
    CHECK(Near(IconOffset(4, 2, 16.0f) + 16.0f + 5.6f, IconRunWidth(5, 3, 16.0f)));
    CHECK(Near(IconOffset(2, 0, 24.0f) + 24.0f, IconRunWidth(3, 0, 24.0f)));
}

TEST(a_measured_text_line_takes_its_texts_boxes_and_its_alignment)
{
    const ComposedLine line = Line({TextItem("Lv 20"), TextItem("[006]")}, 13.0f, 16.0f, Align::Right);
    const std::vector<ItemSize> texts{{60.0f, 17.0f}, {30.0f, 19.0f}};
    // What another plate left behind goes.
    LineInput in;
    in.items          = {{1.0f, 1.0f}, {2.0f, 2.0f}, {3.0f, 3.0f}};
    in.hasIcons       = true;
    in.ornamentWidth  = 90.0f;
    in.ornamentHeight = 6.0f;
    MeasureLine(line, texts, 2.0f, in);
    CHECK_EQ(in.items.size(), size_t{2});
    CHECK_EQ(in.items[0].width, 60.0f);
    CHECK_EQ(in.items[0].height, 17.0f);
    CHECK_EQ(in.items[1].width, 30.0f);
    CHECK_EQ(in.items[1].height, 19.0f);
    CHECK_EQ(in.textHeight, 19.0f);
    CHECK(!in.hasIcons);
    CHECK(in.align == Align::Right);
    CHECK_EQ(in.ornamentWidth, 0.0f);
    CHECK_EQ(in.ornamentHeight, 0.0f);
    // The line's text height is its tallest text's, wherever that stands.
    const std::vector<ItemSize> tallFirst{{60.0f, 19.0f}, {30.0f, 17.0f}};
    MeasureLine(line, tallFirst, 2.0f, in);
    CHECK_EQ(in.textHeight, 19.0f);
}

TEST(icon_runs_on_a_line_without_text_share_a_height_with_room_for_a_badge)
{
    // "$detect $weak" at twice the size: 32 px icons, one of them badged, on a line whose text would be 26 px.
    const ComposedLine line = Line({IconItem(2, 0), SpaceItem(1), IconItem(1, 1)}, 13.0f, 16.0f);
    LineInput in;
    MeasureLine(line, {}, 2.0f, in);
    CHECK_EQ(in.items.size(), size_t{3});
    CHECK(Near(in.items[0].width, 66.0f));  // two icons and a gap
    CHECK(Near(in.items[0].height, 41.6f)); // an icon and a badge's hang of 0.3, though this run has no badge
    CHECK(Near(in.items[1].width, 7.8f));   // a space is 0.3 of the text size
    CHECK_EQ(in.items[1].height, 0.0f);
    CHECK(Near(in.items[2].width, 43.2f));  // an icon and its badge's overhang of 0.35
    CHECK(Near(in.items[2].height, 41.6f));
    CHECK_EQ(in.textHeight, 0.0f);
    CHECK(in.hasIcons);
    const ComposedLine plain = Line({IconItem(2, 0), SpaceItem(2), IconItem(1, 0)}, 13.0f, 16.0f);
    MeasureLine(plain, {}, 1.0f, in);
    CHECK_EQ(in.items[0].height, 16.0f); // no badge on the line: the icons alone
    CHECK(Near(in.items[1].width, 7.8f));
}

TEST(icons_beside_text_are_as_tall_as_the_icons_alone)
{
    // A player's icons and name: the icons center on the name, and a badge there hangs below the line.
    const ComposedLine line = Line({IconItem(2, 1), TextItem("Chadmage")}, 15.0f, 18.0f);
    const std::vector<ItemSize> texts{{70.0f, 20.0f}};
    LineInput in;
    MeasureLine(line, texts, 1.0f, in);
    CHECK_EQ(in.items.size(), size_t{2});
    CHECK(Near(in.items[0].width, 44.3f)); // two icons, a gap and one overhang
    CHECK_EQ(in.items[0].height, 18.0f);
    CHECK_EQ(in.items[1].width, 70.0f);
    CHECK_EQ(in.items[1].height, 20.0f);
    CHECK_EQ(in.textHeight, 20.0f);
    CHECK(in.hasIcons);
}

TEST(a_badge_run_past_eight_icons_wraps_onto_rows_above_against_its_outer_edge)
{
    // Four detection icons and twelve badged ones at 16 px: rows 16 + 4.8 (a badge's hang) + 2 apart, the first eight
    // badges on the bottom row and the other four above it, against the right edge as the run ends a line it shares.
    const ComposedLine line = Line({IconItem(4, 0), IconItem(12, 12)}, 13.0f, 16.0f);
    LineInput in;
    MeasureLine(line, {}, 1.0f, in);
    CHECK(Near(in.items[0].width, 70.0f));
    CHECK(Near(in.items[1].width, 186.8f)); // its first row's eight, the widest
    CHECK(Near(in.items[0].height, 43.6f)); // two rows and the hang, for the detection run too
    CHECK(Near(in.items[1].height, 43.6f));
    const RunShape detection = ShapeOfRun(line, 0, 1.0f), badges = ShapeOfRun(line, 1, 1.0f);
    CHECK(Near(IconInRun(detection, line.items[0], 0).x, 0.0f));
    CHECK(Near(IconInRun(detection, line.items[0], 0).y, 22.8f)); // on the badges' bottom row
    CHECK(Near(IconInRun(detection, line.items[0], 3).x, 54.0f));
    CHECK(Near(IconInRun(badges, line.items[1], 0).x, 0.0f));
    CHECK(Near(IconInRun(badges, line.items[1], 0).y, 22.8f));
    CHECK(Near(IconInRun(badges, line.items[1], 7).x, 165.2f));
    CHECK(Near(IconInRun(badges, line.items[1], 7).y, 22.8f));
    CHECK(Near(IconInRun(badges, line.items[1], 8).x, 94.4f)); // four icons end where the eight below them end
    CHECK(Near(IconInRun(badges, line.items[1], 8).y, 0.0f));
    CHECK(Near(IconInRun(badges, line.items[1], 11).x, 165.2f));
    CHECK(Near(IconInRun(badges, line.items[1], 11).y, 0.0f));

    // First on its line, or alone on it, the upper row starts at the left edge.
    const ComposedLine first = Line({IconItem(12, 12), IconItem(4, 0)}, 13.0f, 16.0f);
    CHECK(Near(IconInRun(ShapeOfRun(first, 0, 1.0f), first.items[0], 8).x, 0.0f));
    CHECK(Near(IconInRun(ShapeOfRun(first, 0, 1.0f), first.items[0], 8).y, 0.0f));
    CHECK(Near(IconInRun(ShapeOfRun(first, 1, 1.0f), first.items[1], 0).y, 22.8f));
    const ComposedLine alone = Line({IconItem(12, 12)}, 13.0f, 16.0f);
    CHECK(Near(IconInRun(ShapeOfRun(alone, 0, 1.0f), alone.items[0], 8).x, 0.0f));
    CHECK(Near(IconInRun(ShapeOfRun(alone, 0, 1.0f), alone.items[0], 11).x, 70.8f));
}

TEST(rows_fill_from_the_bottom_eight_at_a_time)
{
    // Eighteen badges at twice the size: three rows 32 + 9.6 + 2 apart, two icons on the top one.
    const ComposedLine three = Line({IconItem(18, 18)}, 13.0f, 16.0f);
    LineInput in;
    MeasureLine(three, {}, 2.0f, in);
    CHECK(Near(in.items[0].height, 32.0f + 2.0f * 43.6f + 9.6f));
    const RunShape shape = ShapeOfRun(three, 0, 2.0f);
    CHECK(Near(IconInRun(shape, three.items[0], 0).y, 87.2f));
    CHECK(Near(IconInRun(shape, three.items[0], 8).y, 43.6f));
    CHECK(Near(IconInRun(shape, three.items[0], 15).y, 43.6f));
    CHECK(Near(IconInRun(shape, three.items[0], 16).y, 0.0f));
    CHECK(Near(IconInRun(shape, three.items[0], 17).x, 32.0f + 2.0f + 11.2f));

    // Eight icons are one row; a ninth starts the next. Without badges the rows are an icon and a gap apart.
    const ComposedLine eight = Line({IconItem(8, 0)}, 13.0f, 16.0f);
    MeasureLine(eight, {}, 1.0f, in);
    CHECK_EQ(in.items[0].height, 16.0f);
    CHECK(Near(IconInRun(ShapeOfRun(eight, 0, 1.0f), eight.items[0], 7).x, 126.0f));
    CHECK_EQ(IconInRun(ShapeOfRun(eight, 0, 1.0f), eight.items[0], 7).y, 0.0f);
    const ComposedLine nine = Line({IconItem(9, 0)}, 13.0f, 16.0f);
    MeasureLine(nine, {}, 1.0f, in);
    CHECK(Near(in.items[0].width, 142.0f));
    CHECK_EQ(in.items[0].height, 34.0f);
    CHECK_EQ(IconInRun(ShapeOfRun(nine, 0, 1.0f), nine.items[0], 8).y, 0.0f);
    CHECK_EQ(IconInRun(ShapeOfRun(nine, 0, 1.0f), nine.items[0], 0).y, 18.0f);
}

TEST(a_wrapped_run_beside_text_is_its_own_rows_without_the_hang)
{
    const ComposedLine line = Line({IconItem(12, 12), TextItem("Lv 20")}, 13.0f, 16.0f);
    const std::vector<ItemSize> texts{{60.0f, 17.0f}};
    LineInput in;
    MeasureLine(line, texts, 1.0f, in);
    CHECK(Near(in.items[0].height, 38.8f)); // two rows 22.8 apart: the text centers on them
    CHECK(Near(IconInRun(ShapeOfRun(line, 0, 1.0f), line.items[0], 0).y, 22.8f));
    CHECK(Near(IconInRun(ShapeOfRun(line, 0, 1.0f), line.items[0], 8).y, 0.0f));
    CHECK_EQ(in.textHeight, 17.0f);
}

TEST(a_wrapped_badge_group_stands_a_gap_and_a_hang_over_the_level_line_with_the_cursor_over_its_top_row)
{
    // Four 16 px detection icons and twelve badges over a 74 by 18 level line and a 92 by 21 name on (100, 210): the
    // row is 258.8 px wide and centered, its bottom row 2 px and a hang over the level line, its upper row 22.8
    // above that against the right edge, the cursor 2 px over the upper row.
    const ComposedLine composed[] = {Line({IconItem(4, 0), IconItem(12, 12)}, 15.0f, 16.0f),
        Line({TextItem("Lv 20")}, 13.0f, 15.0f), Line({TextItem("Young Quadav")}, 15.0f, 15.0f)};
    const std::vector<ItemSize> level{{74.0f, 18.0f}}, name{{92.0f, 21.0f}};
    std::vector<LineInput> lines(3);
    MeasureLine(composed[0], {}, 1.0f, lines[0]);
    MeasureLine(composed[1], level, 1.0f, lines[1]);
    MeasureLine(composed[2], name, 1.0f, lines[2]);
    const PlateLayout l   = LayoutPlate(Anchor(), lines, 14.0f, 20.0f, 0.5f, 0, 0.0f);
    const ItemPlace left  = l.lines[0].items[0], right = l.lines[0].items[1];
    const IconSpot bottom = IconInRun(ShapeOfRun(composed[0], 1, 1.0f), composed[0].items[1], 0);
    const IconSpot upper  = IconInRun(ShapeOfRun(composed[0], 1, 1.0f), composed[0].items[1], 8);
    const IconSpot detect = IconInRun(ShapeOfRun(composed[0], 0, 1.0f), composed[0].items[0], 0);
    CHECK_EQ(l.lines[1].top, 184.5f);
    CHECK(Near(left.x + detect.x, -29.4f));
    CHECK(Near(left.y + detect.y, 161.7f));
    CHECK(Near(right.x + bottom.x, 42.6f));
    CHECK(Near(right.y + bottom.y, 161.7f));
    CHECK(Near(right.x + upper.x, 137.0f));
    CHECK(Near(right.y + upper.y, 138.9f));
    CHECK(Near(l.cursor.y, 116.9f));
    const PlateBox box = BoxOf(l);
    CHECK(Near(box.top, 138.9f));
    CHECK(Near(box.left, -29.4f));
}

TEST(a_players_icons_and_name_center_together_a_gap_apart_and_the_cursor_sinks_into_the_name)
{
    // Three 14 px icons left of a 92 by 21 name: the two centered together on the game's name (100, 210), the name
    // 2 px right of the icons, the icons centered on its height, the cursor 3 px into the name.
    const ComposedLine line = Line({IconItem(3, 0), TextItem("Chadmage")}, 14.0f, 14.0f);
    const std::vector<ItemSize> texts{{92.0f, 21.0f}};
    std::vector<LineInput> lines(1);
    MeasureLine(line, texts, 1.0f, lines[0]);
    const PlateLayout l = LayoutPlate(Anchor(), lines, 14.0f, 20.0f, 0.5f, 0, 0.0f);
    CHECK_EQ(l.lines[0].items[0].x, 30.0f);
    CHECK_EQ(l.lines[0].items[0].x + IconOffset(2, 0, 14.0f), 62.0f);
    CHECK_EQ(l.lines[0].items[0].y, 203.0f);
    CHECK_EQ(l.lines[0].items[1].x, 78.0f);
    CHECK_EQ(l.lines[0].items[1].y, 199.5f);
    CHECK_EQ(l.cursor.y, 182.5f);
}

TEST(a_detection_run_and_a_badge_run_center_as_one_row_a_gap_and_a_hang_over_the_level_line)
{
    // Two 16 px detection icons and three badged ones over a 74 by 18 level line and a 92 by 21 name: 104.8 px wide
    // with the gap between the two runs, centered, the icons' top a gap, a badge's hang (4.8) and an icon above the
    // level line.
    const ComposedLine composed[] = {Line({IconItem(2, 0), IconItem(3, 3)}, 15.0f, 16.0f),
        Line({TextItem("Lv 20")}, 13.0f, 15.0f), Line({TextItem("Young Quadav")}, 15.0f, 15.0f)};
    const std::vector<ItemSize> level{{74.0f, 18.0f}}, name{{92.0f, 21.0f}};
    std::vector<LineInput> lines(3);
    MeasureLine(composed[0], {}, 1.0f, lines[0]);
    MeasureLine(composed[1], level, 1.0f, lines[1]);
    MeasureLine(composed[2], name, 1.0f, lines[2]);
    const PlateLayout l = LayoutPlate(Anchor(), lines, 14.0f, 20.0f, 0.5f, 0, 0.0f);
    CHECK_EQ(l.lines[1].top, 184.5f);
    CHECK(Near(l.lines[0].items[0].x, 47.6f));
    CHECK(Near(l.lines[0].items[0].y, 161.7f));
    CHECK(Near(l.lines[0].items[1].x, 83.6f)); // the first badged icon, a step past the second detection icon
    CHECK(Near(l.lines[0].items[1].y, 161.7f));
    CHECK(Near(l.cursor.y, 139.7f)); // a gap over the icons
}

TEST(a_plate_closes_up_around_a_text_left_out)
{
    // Icons, the level line and the name with the ornament over it; the level text fits no texture.
    ComposedPlate plate;
    plate.lines = {Line({IconItem(4, 0)}, 15.0f, 16.0f), Line({TextItem("Lv 20")}, 13.0f, 15.0f),
        Line({TextItem("Young Quadav")}, 15.0f, 15.0f)};
    plate.lines[1].source = 1;
    plate.lines[2].source = 2;
    plate.ornamentAbove   = 2;
    DropTexts(plate, std::vector<uint8_t>{1, 0});
    CHECK_EQ(plate.lines.size(), size_t{2});
    const std::vector<ItemSize> name{{92.0f, 21.0f}};
    std::vector<LineInput> lines(2);
    MeasureLine(plate.lines[0], {}, 1.0f, lines[0]);
    MeasureLine(plate.lines[1], name, 1.0f, lines[1]);
    const PlateLayout l = LayoutPlate(Anchor(), lines, 14.0f, 20.0f, 0.5f, 0, 0.0f);
    CHECK_EQ(l.lines[1].top, 199.5f);
    CHECK_EQ(l.lines[0].top, 199.5f - kLineGap - 16.0f); // the icons a gap over the name, where the level line stood
    CHECK_EQ(l.lines[1].ornamentHeight, 0.0f);

    // A player's name left out beside the icons: the icons alone, centered, with no gap kept for the name.
    ComposedPlate player;
    player.lines = {Line({IconItem(3, 0), TextItem("Chadmage")}, 14.0f, 14.0f)};
    DropTexts(player, std::vector<uint8_t>{1});
    std::vector<LineInput> alone(1);
    MeasureLine(player.lines[0], {}, 1.0f, alone[0]);
    const PlateLayout icons = LayoutPlate(Anchor(), alone, 0.0f, 0.0f, 0.5f, 0, 0.0f);
    CHECK_EQ(icons.lines[0].width, 46.0f);
    CHECK_EQ(icons.lines[0].left, 77.0f);
    CHECK_EQ(icons.lines[0].top, 203.0f);
}

namespace
{
    LineInput IconsAndText()
    {
        LineInput line = Text({{80.0f, 28.0f}});
        line.items.insert(line.items.begin(), ItemSize{20.0f, 34.0f});
        line.hasIcons = true;
        return line;
    }
}

TEST(what_sits_above_sinks_into_the_lines_text_not_its_taller_box)
{
    const std::vector<LineInput> mixed{IconsAndText()};
    const PlateLayout m = LayoutPlate(Anchor(), mixed, 20.0f, 32.0f, 0.5f, 0, 0.0f);
    CHECK_EQ(m.lines[0].top, 210.0f - 17.0f);
    CHECK_EQ(m.lines[0].items[1].y, 196.0f);
    CHECK_EQ(m.cursor.y, 196.0f + kNameOverlap - 32.0f);
    const std::vector<LineInput> alone{Text({{80.0f, 28.0f}})};
    const PlateLayout t = LayoutPlate(Anchor(), alone, 20.0f, 32.0f, 0.5f, 0, 0.0f);
    CHECK_EQ(m.cursor.y, t.cursor.y);
    const std::vector<LineInput> textAbove{Text({{60.0f, 13.0f}}), IconsAndText()};
    const PlateLayout a = LayoutPlate(Anchor(), textAbove, 0.0f, 0.0f, 0.5f, 0, 0.0f);
    CHECK_EQ(a.lines[0].top, 196.0f + kNameOverlap - 13.0f);
    const std::vector<LineInput> iconsAbove{Icons({{40.0f, 16.0f}}), IconsAndText()};
    const PlateLayout b = LayoutPlate(Anchor(), iconsAbove, 0.0f, 0.0f, 0.5f, 0, 0.0f);
    CHECK_EQ(b.lines[0].top, 193.0f - kLineGap - 16.0f);
}

TEST(the_first_line_has_no_ornament)
{
    std::vector<LineInput> lines{Text({{60.0f, 13.0f}}), Text({{80.0f, 15.0f}})};
    const PlateLayout plain = LayoutPlate(Anchor(), lines, 20.0f, 32.0f, 0.5f, 0, 0.0f);
    lines[0].ornamentWidth  = 90.0f;
    lines[0].ornamentHeight = 6.0f;
    const PlateLayout with = LayoutPlate(Anchor(), lines, 20.0f, 32.0f, 0.5f, 0, 0.0f);
    CHECK_EQ(with.lines[0].top, plain.lines[0].top);
    CHECK_EQ(with.cursor.y, plain.cursor.y);
    CHECK_EQ(with.lines[0].ornamentHeight, 0.0f);
    const PlateBox a = BoxOf(plain);
    const PlateBox b = BoxOf(with);
    CHECK_EQ(a.left, b.left);
    CHECK_EQ(a.right, b.right);
    CHECK_EQ(a.top, b.top);
    CHECK_EQ(a.bottom, b.bottom);
}

TEST(the_default_mob_plate_has_each_line_its_ornament_timers_and_cursor_in_place)
{
    std::vector<LineInput> lines{Icons({{40.0f, 16.0f}}), Text({{60.0f, 13.0f}}), Text({{80.0f, 15.0f}})};
    const PlateLayout l = LayoutPlate(Anchor(), lines, 0.0f, 0.0f, 0.5f, 0, 0.0f);
    CHECK_EQ(l.lines[0].top, 174.5f);
    CHECK_EQ(l.lines[1].top, 192.5f);
    CHECK_EQ(l.lines[2].top, 202.5f);
    lines[2].ornamentWidth  = 90.0f;
    lines[2].ornamentHeight = 6.0f;
    const PlateLayout o = LayoutPlate(Anchor(), lines, 0.0f, 0.0f, 0.5f, 0, 0.0f);
    CHECK_EQ(o.lines[2].ornament.x, 55.0f);
    CHECK_EQ(o.lines[2].ornament.y, 197.5f);
    CHECK_EQ(o.lines[1].top, 182.5f);
    CHECK_EQ(o.lines[0].top, 164.5f);
    lines[2].ornamentHeight = 0.0f;
    const PlateLayout t = LayoutPlate(Anchor(), lines, 20.0f, 32.0f, 0.5f, 2, 13.0f);
    CHECK_EQ(t.timerStep, 15.0f);
    CHECK_EQ(t.timersY, 144.5f);
    CHECK_EQ(t.cursor.y, 110.5f);
}

TEST(an_ornament_over_an_icon_line_clears_it_by_a_gap)
{
    std::vector<LineInput> lines{Icons({{40.0f, 16.0f}}), Text({{80.0f, 15.0f}})};
    lines[1].ornamentWidth  = 90.0f;
    lines[1].ornamentHeight = 6.0f;
    const PlateLayout l = LayoutPlate(Anchor(), lines, 0.0f, 0.0f, 0.5f, 0, 0.0f);
    CHECK_EQ(l.lines[0].top + 16.0f, l.lines[1].ornament.y - kLineGap);
}

TEST(an_ornament_wider_than_every_line_stays_centered_and_the_box_holds_it)
{
    std::vector<LineInput> lines{Text({{40.0f, 10.0f}}, Align::Left), Text({{30.0f, 10.0f}}, Align::Right), Text({{50.0f, 10.0f}})};
    lines[2].ornamentWidth  = 140.0f;
    lines[2].ornamentHeight = 4.0f;
    const PlateLayout l = LayoutPlate(Anchor(), lines, 0.0f, 0.0f, 0.5f, 0, 0.0f);
    CHECK_EQ(l.lines[2].ornament.x, 30.0f);
    CHECK_EQ(l.lines[0].left, 75.0f);
    CHECK_EQ(l.lines[1].left, 125.0f - 30.0f);
    const PlateBox box = BoxOf(l);
    CHECK_EQ(box.left, 30.0f);
    CHECK_EQ(box.right, 170.0f);
}

TEST(a_line_taller_than_the_anchor_is_centered_on_it)
{
    const std::vector<LineInput> lines{Text({{80.0f, 60.0f}})};
    const PlateLayout l = LayoutPlate(Anchor(), lines, 0.0f, 0.0f, 0.5f, 0, 0.0f);
    CHECK_EQ(l.lines[0].top, 210.0f - 30.0f);
}

TEST(an_empty_plates_top_is_the_games_name)
{
    const std::vector<LineInput> none;
    CHECK_EQ(LayoutPlate(Anchor(), none, 0.0f, 0.0f, 0.5f, 0, 0.0f).top, 200.0f);
}
