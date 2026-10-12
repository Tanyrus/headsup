#include "plate_layout.h"
#include "test.h"

#include <algorithm>

using namespace headsup;

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

TEST(lines_align_against_the_widest_line)
{
    const std::vector<LineInput> lines{Text({{40.0f, 10.0f}}, Align::Left), Text({{100.0f, 10.0f}}), Text({{40.0f, 10.0f}}, Align::Right)};
    const PlateLayout l = LayoutPlate(Anchor(), lines, 0.0f, 0.0f, 0.5f, 0, 0.0f);
    CHECK_EQ(l.lines[1].left, 50.0f);
    CHECK_EQ(l.lines[0].left, 50.0f);
    CHECK_EQ(l.lines[2].left, 150.0f - 40.0f);
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

TEST(an_icon_run_is_its_icons_and_gaps_plus_the_badges_overhang)
{
    CHECK_EQ(IconRunWidth(3, 16.0f, false), 3.0f * 16.0f + 2.0f * kIconGap);
    CHECK_EQ(IconRunWidth(3, 16.0f, true), 3.0f * (16.0f + kIconGap + 16.0f * kBadgeOverhang) - kIconGap);
    CHECK_EQ(IconRunWidth(0, 16.0f, false), 0.0f);
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

TEST(the_default_mob_plate_lands_where_the_old_layout_put_it)
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
