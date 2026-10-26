#include "bar_placement.h"
#include "test.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

using namespace headsup;

namespace
{
    constexpr BarSection kLeft = BarSection::Left, kCenter = BarSection::Center, kRight = BarSection::Right;
    using List = std::vector<PanelWidget>;

    const List& In(const BarPlacement& p, BarSection s) { return p[static_cast<int>(s)]; }

    // The widgets the bar had before the newer ones were added, in the section's order.
    List Original(const List& section)
    {
        List kept;
        for (const PanelWidget w : section)
            if (static_cast<int>(w.kind) <= static_cast<int>(BarWidget::EarthTime)) kept.push_back(w);
        return kept;
    }
}

TEST(the_default_placement_is_todays_bar_with_every_widget_once)
{
    const BarPlacement& d = DefaultPlacement();
    CHECK(Original(In(d, kLeft)) == (List{BarWidget::Zone, BarWidget::Weather, BarWidget::VanaTime}));
    CHECK(In(d, kCenter) == (List{BarWidget::Target})); // the target alone in the center
    CHECK(Original(In(d, kRight)) == (List{BarWidget::Gil, BarWidget::Bags, BarWidget::EarthTime}));
    for (int w = 0; w < kBarWidgetCount; ++w)
    {
        int seen = 0;
        for (const List& section : d)
            seen += static_cast<int>(std::count(section.begin(), section.end(), static_cast<BarWidget>(w)));
        CHECK_EQ(seen, OnTheBar(static_cast<BarWidget>(w)) ? 1 : 0); // Game help has its own bar
        CHECK(BarWidgetName(static_cast<BarWidget>(w)) != nullptr);
    }
}

TEST(a_placement_reads_back_what_it_wrote)
{
    const BarPlacement& d = DefaultPlacement();
    CHECK_EQ(PlacementNames(In(d, kCenter)), std::string("target"));
    CHECK(PlacementFrom({PlacementNames(In(d, kLeft)), PlacementNames(In(d, kCenter)), PlacementNames(In(d, kRight))}) ==
          d);
    CHECK(PlacementFrom({"", "", ""}) == d); // an old file: every widget in its default place
}

TEST(unknown_and_repeated_names_are_dropped_and_a_widget_named_nowhere_goes_home_at_the_end)
{
    const BarPlacement p = PlacementFrom({"vanaTime moon vanaTime", "zone", "earthTime target"});
    CHECK(Original(In(p, kLeft)) == (List{BarWidget::VanaTime, BarWidget::Weather})); // weather named nowhere
    CHECK(In(p, kCenter) == (List{BarWidget::Zone}));
    CHECK(Original(In(p, kRight)) == (List{BarWidget::EarthTime, BarWidget::Target, BarWidget::Gil, BarWidget::Bags}));
}

TEST(a_file_that_placed_game_help_in_a_section_loses_it_there)
{
    // Game help left the sections for its own bar under the top bar; a file from before still names it.
    const BarPlacement p = PlacementFrom({"zone gameHelp", "target", ""});
    for (const List& section : p)
        CHECK(std::count(section.begin(), section.end(), BarWidget::GameHelp) == 0);
    CHECK(In(p, kLeft).front() == BarWidget::Zone && In(p, kCenter) == (List{BarWidget::Target}));
}

TEST(a_widget_moves_up_and_down_within_its_section)
{
    BarPlacement p = DefaultPlacement();
    const List left = In(p, kLeft);
    CHECK(MoveWidget(p, kLeft, 2, kLeft, 0)); // Vana'diel time to the front
    CHECK(Original(In(p, kLeft)) == (List{BarWidget::VanaTime, BarWidget::Zone, BarWidget::Weather}));
    CHECK(MoveWidget(p, kLeft, 0, kLeft, 2)); // back down, before Weather: after Zone
    CHECK(Original(In(p, kLeft)) == (List{BarWidget::Zone, BarWidget::VanaTime, BarWidget::Weather}));
    CHECK(MoveWidget(p, kLeft, 0, kLeft, In(p, kLeft).size())); // to the end
    CHECK(In(p, kLeft).back() == BarWidget::Zone && In(p, kLeft).size() == left.size());
}

TEST(a_widget_moves_across_sections_and_into_an_empty_one)
{
    BarPlacement p = DefaultPlacement();
    CHECK(MoveWidget(p, kRight, 0, kLeft, 1)); // Gil after Zone
    CHECK(In(p, kLeft)[1] == BarWidget::Gil && In(p, kRight).front() == BarWidget::Bags);
    CHECK(MoveWidget(p, kCenter, 0, kRight, In(p, kRight).size())); // the target to the right group's end
    CHECK(In(p, kCenter).empty() && In(p, kRight).back() == BarWidget::Target);
    CHECK(MoveWidget(p, kRight, 0, kCenter, 0)); // into the empty center
    CHECK(In(p, kCenter) == (List{BarWidget::Bags}));
}

TEST(a_drop_where_the_widget_already_is_or_out_of_range_changes_nothing)
{
    BarPlacement p = DefaultPlacement();
    CHECK(!MoveWidget(p, kLeft, 1, kLeft, 1) && !MoveWidget(p, kLeft, 1, kLeft, 2)); // before or after itself
    CHECK(!MoveWidget(p, kLeft, In(p, kLeft).size(), kRight, 0));                    // no such widget
    CHECK(!MoveWidget(p, kLeft, 0, kRight, In(p, kRight).size() + 1));                // past the end
    CHECK(p == DefaultPlacement());
}

TEST(a_copy_goes_after_the_last_of_its_widget_with_the_lowest_number_free_and_four_at_most)
{
    BarPlacement p = DefaultPlacement();
    const auto [section, at] = PlaceOf(p, BarWidget::Gil);
    CHECK(AddCopy(p, BarWidget::Gil) == (PanelWidget{BarWidget::Gil, 1}));
    CHECK(p[section][at + 1] == (PanelWidget{BarWidget::Gil, 1}));
    CHECK(AddCopy(p, BarWidget::Gil) == (PanelWidget{BarWidget::Gil, 2}));
    CHECK(p[section][at + 2] == (PanelWidget{BarWidget::Gil, 2}));
    // Taken off, a copy's number is the next one given.
    CHECK(RemoveCopy(p, PanelWidget{BarWidget::Gil, 1}) && !Placed(p, PanelWidget{BarWidget::Gil, 1}));
    CHECK(AddCopy(p, BarWidget::Gil) == (PanelWidget{BarWidget::Gil, 1}));
    CHECK(AddCopy(p, BarWidget::Gil) == (PanelWidget{BarWidget::Gil, 3}));
    CHECK(!AddCopy(p, BarWidget::Gil).has_value());
    // The first copy always stays, and the target and game help have none.
    CHECK(!RemoveCopy(p, BarWidget::Gil) && Placed(p, BarWidget::Gil));
    CHECK(!AddCopy(p, BarWidget::Target).has_value() && !AddCopy(p, BarWidget::GameHelp).has_value());
}

TEST(copies_read_back_by_their_numbers_and_a_number_out_of_range_is_dropped)
{
    BarPlacement p = DefaultPlacement();
    AddCopy(p, BarWidget::Gil);
    AddCopy(p, BarWidget::Gil);
    const std::array<std::string, kBarSectionCount> names{PlacementNames(In(p, kLeft)), PlacementNames(In(p, kCenter)),
        PlacementNames(In(p, kRight))};
    CHECK(names[static_cast<int>(kRight)].find("gil gil:2 gil:3") != std::string::npos);
    CHECK(PlacementFrom(names) == p);
    const BarPlacement odd = PlacementFrom({"gil:5 gil:1 target:2 gil:x zone:2", "", ""});
    CHECK(In(odd, kLeft).front() == (PanelWidget{BarWidget::Zone, 1})); // and the first copies after, each in its place
    for (int copy = 1; copy < kMostCopies; ++copy)
        CHECK(!Placed(odd, PanelWidget{BarWidget::Gil, copy}));
    CHECK(PlaceOf(odd, BarWidget::Target).first == static_cast<int>(kCenter));
}
