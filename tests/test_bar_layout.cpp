#include "bar_layout.h"
#include "bar_placement.h"
#include "test.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

using namespace headsup;

namespace
{
    bool Near(float a, float b) { return std::fabs(a - b) < 0.01f; }

    std::array<float, kPanelSlots> Widths(float zone, float weather, float vana, float target, float gil,
                                              float bags, float earth)
    {
        std::array<float, kPanelSlots> widths{};
        const std::pair<BarWidget, float> each[] = {{BarWidget::Zone, zone}, {BarWidget::Weather, weather},
            {BarWidget::VanaTime, vana}, {BarWidget::Target, target}, {BarWidget::Gil, gil}, {BarWidget::Bags, bags},
            {BarWidget::EarthTime, earth}};
        for (const auto& [widget, width] : each)
            widths[SlotOf(widget)] = width;
        return widths;
    }

    // Natural widths, a 36 px bar.
    const BarMetrics kPills  = MetricsFor(36, BarShape::Pills);
    const BarMetrics kOrnate = MetricsFor(36, BarShape::Ornate);
}

TEST(pills_put_the_groups_at_the_edges_and_the_target_in_the_middle)
{
    const BarLayout l = LayoutBar(1920.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 200, 90, 60, 50), 0.0f);
    CHECK(Near(l.height, 36.0f) && l.widgets.size() == 6);
    const float g = kPills.gap;
    CHECK(l.widgets[0].widget == BarWidget::Zone && Near(l.widgets[0].rect.x, g));
    CHECK(l.widgets[1].widget == BarWidget::VanaTime && Near(l.widgets[1].rect.x, g + 100.0f + g)); // weather is off
    CHECK(Near(RectOf(l, BarWidget::EarthTime)->x + 50.0f, 1920.0f - g));
    CHECK(Near(RectOf(l, BarWidget::Bags)->x + 60.0f, 1920.0f - g - 50.0f - g));
    CHECK(Near(RectOf(l, BarWidget::Target)->x, 860.0f) && Near(l.targetWidth, 200.0f));
    CHECK(Near(l.widgets[0].rect.height, PillRow(kPills).height) && Near(l.widgets[0].rect.y, PillRow(kPills).y));
    CHECK(l.dividers.empty());
    std::vector<PanelWidget> order;
    for (const PlacedWidget& p : l.widgets)
        order.push_back(p.widget);
    CHECK(order == (std::vector<PanelWidget>{BarWidget::Zone, BarWidget::VanaTime, BarWidget::Target, BarWidget::Gil,
                                           BarWidget::Bags, BarWidget::EarthTime})); // in bar order
}

TEST(ornate_starts_after_the_caps_and_divides_neighbours_with_diamonds)
{
    const BarLayout l = LayoutBar(1920.0f, kOrnate, BarShape::Ornate, Widths(100, 80, 120, 200, 90, 60, 50), 0.0f);
    CHECK(Near(l.widgets[0].rect.x, kOrnate.cap) && Near(l.widgets[0].rect.height, 36.0f) &&
          Near(l.widgets[0].rect.y, 0.0f));
    CHECK(Near(l.widgets[1].rect.x, kOrnate.cap + 100.0f));
    CHECK(l.dividers.size() == 4); // two between three on the left, two on the right
    CHECK(Near(l.dividers[0], kOrnate.cap + 100.0f));
    // Each on the boundary between two neighbours: the start of the later one.
    CHECK(Near(l.dividers[1], RectOf(l, BarWidget::VanaTime)->x));
    CHECK(Near(l.dividers[2], RectOf(l, BarWidget::Bags)->x));
    CHECK(Near(l.dividers[3], RectOf(l, BarWidget::EarthTime)->x));
    CHECK(Near(RectOf(l, BarWidget::Target)->height, PillRow(kOrnate).height)); // the target's frame
    CHECK(Near(RectOf(l, BarWidget::Target)->y, PillRow(kOrnate).y));
}

TEST(a_narrow_screen_shrinks_the_target_then_drops_it_and_nothing_overlaps)
{
    BarLayout l = LayoutBar(700.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 300, 90, 60, 50), 0.0f);
    CHECK(l.targetWidth > 0.0f && l.targetWidth < 300.0f);
    l = LayoutBar(300.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 300, 90, 60, 50), 0.0f);
    CHECK(RectOf(l, BarWidget::Target) == nullptr && l.targetWidth == 0.0f);
    for (size_t i = 0; i < l.widgets.size(); ++i)
    {
        CHECK(l.widgets[i].rect.width > 0.0f);
        for (size_t j = i + 1; j < l.widgets.size(); ++j)
            CHECK(l.widgets[i].rect.x + l.widgets[i].rect.width <= l.widgets[j].rect.x + 0.01f ||
                  l.widgets[j].rect.x + l.widgets[j].rect.width <= l.widgets[i].rect.x + 0.01f);
    }
}

TEST(hit_tests_popup_placement_and_fitting_text)
{
    const BarLayout l = LayoutBar(1920.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 200, 90, 60, 50), 0.0f);
    CHECK(WidgetAt(l, 960.0f, 18.0f) == BarWidget::Target);
    CHECK(!WidgetAt(l, 500.0f, 18.0f).has_value());
    const BarRect target = *RectOf(l, BarWidget::Target);
    const BarRect under  = PlacePopup(target, 36.0f, 400.0f, 300.0f, 1920.0f, 1080.0f, 8.0f, 0.0f);
    CHECK(Near(under.x, 760.0f) && Near(under.y, 44.0f) && Near(under.height, 300.0f));
    const BarRect edge =
        PlacePopup(*RectOf(l, BarWidget::EarthTime), 36.0f, 400.0f, 2000.0f, 1920.0f, 1080.0f, 8.0f, 0.0f);
    CHECK(Near(edge.x, 1920.0f - 8.0f - 400.0f) && Near(edge.height, 1080.0f - 44.0f - 8.0f));
    auto measure = [](const std::string& s) { return 7.0f * static_cast<float>(s.size()); };
    CHECK(FitText("Goblin Bounty Hunter", 1000.0f, measure) == "Goblin Bounty Hunter");
    CHECK(FitText("Goblin Bounty Hunter", 70.0f, measure) == "Goblin...");
    CHECK(FitText("Goblin Bounty Hunter", 10.0f, measure).empty());
}

TEST(hovering_opens_leaving_closes_and_a_click_pins)
{
    const BarLayout l = LayoutBar(1920.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 200, 90, 60, 50), 0.0f);
    std::array<bool, kPanelSlots> popups{};
    popups[SlotOf(BarWidget::Target)]   = true;
    popups[SlotOf(BarWidget::VanaTime)] = true;
    BarInput input;
    input.Frame(l, popups);
    input.Move(960.0f, 18.0f);
    CHECK(input.Open() == BarWidget::Target);
    input.SetPopup(BarRect{760.0f, 44.0f, 400.0f, 300.0f});
    input.Move(960.0f, 200.0f); // into the popup
    CHECK(input.Open() == BarWidget::Target);
    input.Move(960.0f, 700.0f);
    CHECK(!input.Open().has_value());
    input.SetPopup(BarRect{});
    CHECK(!input.Press(960.0f, 700.0f, true)); // the game's
    CHECK(input.Press(960.0f, 18.0f, true));   // pinned
    CHECK(input.Pinned() == BarWidget::Target);
    input.Move(960.0f, 700.0f);
    CHECK(input.Open() == BarWidget::Target); // stays
    const BarRect vana = *RectOf(l, BarWidget::VanaTime);
    input.Move(vana.x + 5.0f, 18.0f);
    CHECK(input.Open() == BarWidget::VanaTime); // hovering another shows it
    input.Move(960.0f, 700.0f);
    CHECK(input.Open() == BarWidget::Target); // back to the pinned one
    input.SetPopup(BarRect{760.0f, 44.0f, 400.0f, 300.0f});
    CHECK(input.Press(960.0f, 200.0f, true)); // a click on the pinned popup unpins
    CHECK(!input.Pinned().has_value());
    const BarRect gil = *RectOf(l, BarWidget::Gil);
    CHECK(input.Press(gil.x + 5.0f, 18.0f, true)); // the bar's, but gil has no popup
    CHECK(!input.Pinned().has_value());
    CHECK(input.Press(5.0f, 30.0f, false)); // a right press on the strip is the bar's too
}

TEST(a_cut_help_bar_under_the_strip_opens_and_pins_its_popup_and_takes_only_its_own_presses)
{
    // The renderer places Game help's own bar under the strip when its text was cut.
    BarLayout l = LayoutBar(1920.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 200, 90, 60, 50), 0.0f);
    l.widgets.push_back(PlacedWidget{BarWidget::GameHelp, BarRect{0.0f, 46.0f, 480.0f, 60.0f}});
    std::array<bool, kPanelSlots> popups{};
    popups[SlotOf(BarWidget::GameHelp)] = true;
    BarInput input;
    input.Frame(l, popups);
    input.Move(100.0f, 70.0f);
    CHECK(input.Open() == BarWidget::GameHelp);
    CHECK(!input.Press(600.0f, 70.0f, true)); // beside it, the game's
    CHECK(input.Press(100.0f, 70.0f, true));
    CHECK(input.Pinned() == BarWidget::GameHelp);
}

TEST(pinning_another_widget_moves_the_pin)
{
    const BarLayout l = LayoutBar(1920.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 200, 90, 60, 50), 0.0f);
    std::array<bool, kPanelSlots> popups{};
    popups[SlotOf(BarWidget::Target)]   = true;
    popups[SlotOf(BarWidget::VanaTime)] = true;
    BarInput input;
    input.Frame(l, popups);
    CHECK(input.Press(960.0f, 18.0f, true));
    CHECK(input.Pinned() == BarWidget::Target);
    const BarRect vana = *RectOf(l, BarWidget::VanaTime);
    CHECK(input.Press(vana.x + 5.0f, 18.0f, true));
    CHECK(input.Pinned() == BarWidget::VanaTime);
}

// Targeting a player or nothing while the pointer rests on the target takes its popup away: the hover goes with it,
// and a pin stays (a pinned target popup reads "No target").
TEST(a_hovered_popup_closes_when_its_widget_stops_having_one)
{
    const BarLayout l = LayoutBar(1920.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 200, 90, 60, 50), 0.0f);
    std::array<bool, kPanelSlots> popups{};
    popups[SlotOf(BarWidget::Target)] = true;
    std::array<bool, kPanelSlots> none{};
    BarInput input;
    input.Frame(l, popups);
    input.Move(960.0f, 18.0f);
    input.SetPopup(BarRect{760.0f, 44.0f, 400.0f, 300.0f});
    CHECK(input.Open() == BarWidget::Target);
    input.Frame(l, none);
    CHECK(!input.Open().has_value());
    CHECK(!input.Press(960.0f, 200.0f, true)); // where the popup was: the game's
    CHECK(!input.Pinned().has_value());
    input.Frame(l, popups);
    CHECK(input.Press(960.0f, 18.0f, true));
    input.Frame(l, none);
    CHECK(input.Open() == BarWidget::Target && input.Pinned() == BarWidget::Target);
}

TEST(a_pinned_widget_that_is_switched_off_loses_its_pin)
{
    std::array<bool, kPanelSlots> popups{};
    popups[SlotOf(BarWidget::Target)] = true;
    BarInput input;
    input.Frame(LayoutBar(1920.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 200, 90, 60, 50), 0.0f), popups);
    input.Press(960.0f, 18.0f, true);
    CHECK(input.Pinned() == BarWidget::Target);
    input.Frame(LayoutBar(1920.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 0, 90, 60, 50), 0.0f), popups);
    CHECK(!input.Pinned().has_value());
}

TEST(fitting_text_with_no_text_or_no_room_gives_nothing)
{
    auto measure = [](const std::string& s) { return 7.0f * static_cast<float>(s.size()); };
    CHECK(FitText("", 100.0f, measure).empty());
    CHECK(FitText("", -5.0f, measure).empty()); // a shrunk target minus its padding can be negative
    CHECK(FitText("Goblin", -5.0f, measure).empty());
    CHECK(FitText("Goblin", 0.0f, measure).empty());
}

TEST(a_cut_text_is_the_longest_start_that_fits_at_every_width)
{
    // Characters of different widths, spaces and UTF-8 (its continuation bytes add nothing).
    auto measure = [](const std::string& s) {
        float width = 0.0f;
        for (const unsigned char c : s)
            width += c == ' ' ? 3.0f : c == 'W' ? 12.0f : (c & 0xC0) == 0x80 ? 0.0f : 7.0f;
        return width;
    };
    for (const std::string text : {"Goblin Bounty Hunter", "W W  W", "Fi\xC3\xB1o Le\xC3\xB3n", "  Bat", "A"})
        for (float width = -1.0f; width <= measure(text) + 1.0f; width += 0.5f)
        {
            // Every start that ends on a whole character, its trailing spaces dropped, tried with "...".
            std::string longest;
            for (size_t end = 1; end < text.size(); ++end)
            {
                std::string cut = text.substr(0, end);
                if ((static_cast<unsigned char>(text[end]) & 0xC0) == 0x80) continue;
                while (!cut.empty() && cut.back() == ' ')
                    cut.pop_back();
                if (!cut.empty() && measure(cut + "...") <= width) longest = cut + "...";
            }
            CHECK(FitText(text, width, measure) == (measure(text) <= width ? text : longest));
        }
}

TEST(a_shrunk_target_with_fractional_widths_stays_between_the_groups)
{
    const auto check = [](float screen, const std::array<float, kPanelSlots>& natural)
    {
        const BarLayout l = LayoutBar(screen, kPills, BarShape::Pills, natural, 0.0f);
        const BarRect* target = RectOf(l, BarWidget::Target);
        if (target == nullptr) return;
        float leftEnd = 0.0f, rightStart = screen;
        for (const PlacedWidget& p : l.widgets)
        {
            if (p.widget.kind < BarWidget::Target) leftEnd = std::max(leftEnd, p.rect.x + p.rect.width);
            if (p.widget.kind > BarWidget::Target) rightStart = std::min(rightStart, p.rect.x);
        }
        CHECK(target->x >= leftEnd + kPills.gap - 0.01f);
        CHECK(target->x + target->width <= rightStart - kPills.gap + 0.01f);
        for (size_t i = 0; i < l.widgets.size(); ++i)
            for (size_t j = i + 1; j < l.widgets.size(); ++j)
                CHECK(l.widgets[i].rect.x + l.widgets[i].rect.width <= l.widgets[j].rect.x + 0.01f ||
                      l.widgets[j].rect.x + l.widgets[j].rect.width <= l.widgets[i].rect.x + 0.01f);
    };
    check(700.37f, Widths(100.13f, 0, 120.71f, 300.29f, 90.53f, 60.17f, 50.89f));
    for (int i = 0; i < 400; ++i) // the float rounding of the target's bounds depends on the numbers
        check(600.0f + 0.37f * static_cast<float>(i), Widths(100.13f, 0, 120.71f, 300.29f, 90.53f, 60.17f, 50.89f));
}

TEST(the_hover_holds_across_the_gap_between_a_widget_and_its_popup)
{
    const BarLayout l = LayoutBar(1920.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 200, 90, 60, 50), 0.0f);
    std::array<bool, kPanelSlots> popups{};
    popups[SlotOf(BarWidget::Target)] = true;
    BarInput input;
    input.Frame(l, popups);
    input.Move(960.0f, 18.0f);
    input.SetPopup(BarRect{760.0f, 44.0f, 400.0f, 300.0f});
    for (const float y : {34.0f, 40.0f, 43.0f, 60.0f})
    {
        input.Move(960.0f, y);
        CHECK(input.Open() == BarWidget::Target);
    }
    CHECK(!input.Press(960.0f, 40.0f, true)); // below the strip and above the popup: the game's
    CHECK(!input.Pinned().has_value());
    input.Move(500.0f, 40.0f); // outside the popup's columns, on no widget
    CHECK(!input.Open().has_value());
}

// Out of the game's client area no move reaches it, so Present says the pointer left: a popup hovering opened closes
// until the pointer comes back, wherever it was last, and a pinned one stays.
TEST(leaving_the_game_closes_a_hovered_popup_until_the_pointer_is_back_and_keeps_a_pinned_one)
{
    const BarLayout l = LayoutBar(1920.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 200, 90, 60, 50), 0.0f);
    std::array<bool, kPanelSlots> popups{};
    popups[SlotOf(BarWidget::Target)]   = true;
    popups[SlotOf(BarWidget::VanaTime)] = true;
    const BarRect popup{760.0f, 44.0f, 400.0f, 300.0f};
    BarInput input;
    input.Frame(l, popups);
    input.Move(960.0f, 18.0f);
    input.SetPopup(popup);
    input.Move(960.0f, 200.0f); // into the popup
    CHECK(input.Open() == BarWidget::Target);
    input.Leave();
    CHECK(!input.Open().has_value() && !input.Hovered().has_value());
    input.Frame(l, popups); // the next frame, at the same layout
    CHECK(!input.Open().has_value());
    input.SetPopup(BarRect{});
    input.Move(960.0f, 18.0f);
    CHECK(input.Open() == BarWidget::Target);
    input.Leave(); // from the widget itself
    input.Frame(l, popups);
    CHECK(!input.Open().has_value() && !input.Hovered().has_value());
    input.Move(960.0f, 18.0f);
    CHECK(input.Press(960.0f, 18.0f, true));
    const BarRect vana = *RectOf(l, BarWidget::VanaTime);
    input.Move(vana.x + 5.0f, 18.0f);
    CHECK(input.Open() == BarWidget::VanaTime);
    input.Leave();
    CHECK(input.Open() == BarWidget::Target && input.Pinned() == BarWidget::Target);
}

TEST(resetting_drops_a_pinned_popup_and_the_pointer)
{
    const BarLayout l = LayoutBar(1920.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 200, 90, 60, 50), 0.0f);
    std::array<bool, kPanelSlots> popups{};
    popups[SlotOf(BarWidget::Target)] = true;
    BarInput input;
    input.Frame(l, popups);
    input.Move(960.0f, 18.0f);
    CHECK(input.Press(960.0f, 18.0f, true));
    CHECK(input.Pinned() == BarWidget::Target);
    input.Reset();
    CHECK(!input.Open().has_value() && !input.Pinned().has_value() && !input.Hovered().has_value());
    CHECK(!input.Over(960.0f, 18.0f));
    input.Frame(l, popups); // the next login's first frame
    CHECK(!input.Open().has_value());
}

namespace
{
    // GDI's line box is taller than the ink in it: leading above the capitals, and the title's descenders reach a
    // quarter of its size below its baseline.
    LineInk FakeInk(bool title, int pixels)
    {
        const float px = static_cast<float>(pixels);
        return title ? LineInk{0.30f * px, 1.00f * px, 1.25f * px} : LineInk{0.25f * px, 0.95f * px, 1.20f * px};
    }

    // Ink a quarter of the size: anything fits.
    LineInk SmallInk(bool, int pixels)
    {
        const float px = static_cast<float>(pixels);
        return LineInk{0.0f, 0.2f * px, 0.25f * px};
    }

}

TEST(pill_rows_are_whole_pixels_centered_above_the_strips_bottom_line)
{
    for (int h = kMinBarHeight; h <= kMaxBarHeight; ++h)
        for (const BarShape shape : {BarShape::Pills, BarShape::Ornate})
        {
            const BarMetrics m = MetricsFor(h, shape);
            const BarRow row   = PillRow(m);
            const float above = row.y, below = (m.height - 1.0f) - (row.y + row.height); // the last row is the line
            CHECK(Near(row.y, std::round(row.y)) && Near(row.height, std::round(row.height)));
            CHECK(above >= std::max(2.0f, 0.06f * m.height) && below >= std::max(2.0f, 0.06f * m.height));
            CHECK(std::fabs(above - below) <= 1.0f);
            const BarLayout l = LayoutBar(1920.0f, m, shape, Widths(100, 80, 120, 200, 90, 60, 50), 0.0f);
            for (const PlacedWidget& p : l.widgets)
                if (shape == BarShape::Pills || p.widget == BarWidget::Target)
                    CHECK(Near(p.rect.y, row.y) && Near(p.rect.height, row.height));
        }
}

TEST(a_titles_descenders_stay_clear_of_its_detail_line_and_both_inside_the_pill)
{
    for (int h = kMinBarHeight; h <= kMaxBarHeight; ++h)
        for (const BarShape shape : {BarShape::Pills, BarShape::Ornate})
        {
            const BarMetrics m  = MetricsFor(h, shape);
            const BarRow row    = PillRow(m);
            const TextRow t     = FitTextRow(m, FakeInk);
            const LineInk title = FakeInk(true, t.title);
            // Inside the pill's 1 px border, the ink keeps clear space above and below.
            const float margin = std::max(2.0f, 0.06f * row.height);
            const float inTop = row.y + 1.0f, inBottom = row.y + row.height - 1.0f;
            if (h >= 36) CHECK(t.twoLines);
            if (t.twoLines)
            {
                const LineInk detail = FakeInk(false, t.detail);
                CHECK(t.title > t.detail && t.detail >= 9);
                CHECK(t.title <= WholePixels(m.title) && t.detail <= WholePixels(m.subtitle));
                const float above = (t.titleY + title.top) - inTop;
                const float below = inBottom - (t.detailY + detail.bottom);
                CHECK(above >= margin - 0.01f && below >= margin - 0.01f && std::fabs(above - below) <= 1.01f);
                // Three rows clear from the title's descenders to the detail's capitals: two still read as touching.
                CHECK((t.detailY + detail.top) - (t.titleY + title.bottom) >= 3.0f - 0.01f);
            }
            // A title alone (gil, the clock, or every title when two lines do not fit): all its ink inside.
            const float aloneTop = t.aloneY + title.top, aloneBottom = t.aloneY + title.bottom;
            CHECK(aloneTop >= inTop + margin - 0.01f && aloneBottom <= inBottom - margin + 0.01f);
        }
}

TEST(a_row_too_short_for_two_lines_kept_apart_shows_titles_alone_at_full_size)
{
    for (const BarShape shape : {BarShape::Pills, BarShape::Ornate})
    {
        const BarMetrics m = MetricsFor(24, shape);
        const TextRow t    = FitTextRow(m, FakeInk);
        CHECK(!t.twoLines);
        CHECK_EQ(t.title, WholePixels(m.title));
        // Its capitals centered in the pill, within a pixel.
        const LineInk title = FakeInk(true, t.title);
        const BarRow row    = PillRow(m);
        const float above = (t.aloneY + title.top) - row.y, below = row.y + row.height - (t.aloneY + title.baseline);
        CHECK(std::fabs(above - below) <= 1.01f);
    }
}

// The user finds small text hard to read: a detail line under 9 px is not worth drawing, and a taller bar never gets
// smaller titles, except where it gains room for a detail line under them.
TEST(no_line_is_under_9_px_and_a_taller_bar_never_shrinks_the_titles)
{
    for (const BarShape shape : {BarShape::Pills, BarShape::Ornate})
    {
        int twoLines = 0, compact = 0;
        for (int h = kMinBarHeight; h <= kMaxBarHeight; ++h)
        {
            const TextRow t = FitTextRow(MetricsFor(h, shape), FakeInk);
            CHECK(t.title >= 9);
            if (t.twoLines)
            {
                CHECK(t.detail >= 9);
                CHECK(t.title >= twoLines);
                twoLines = t.title;
            }
            else
            {
                CHECK(t.title >= compact);
                compact = t.title;
            }
        }
    }
}

// Gil and the clock have no detail line: they take the row's title size, and their capitals are centered in the pill.
TEST(a_title_without_a_detail_line_takes_the_rows_title_size_centered)
{
    for (int h = kMinBarHeight; h <= kMaxBarHeight; ++h)
        for (const BarShape shape : {BarShape::Pills, BarShape::Ornate})
        {
            const BarMetrics m  = MetricsFor(h, shape);
            const BarRow row    = PillRow(m);
            const TextRow t     = FitTextRow(m, FakeInk);
            const LineInk title = FakeInk(true, t.title);
            const float above = (t.aloneY + title.top) - row.y;
            const float below = row.y + row.height - (t.aloneY + title.baseline);
            CHECK(std::fabs(above - below) <= 1.01f);
        }
}

TEST(text_shrinks_only_when_its_ink_would_not_fit)
{
    const BarMetrics m = MetricsFor(36, BarShape::Pills);
    const TextRow roomy = FitTextRow(m, SmallInk);
    CHECK(roomy.twoLines);
    CHECK_EQ(roomy.title, WholePixels(m.title));
    CHECK_EQ(roomy.detail, WholePixels(m.subtitle));
    const TextRow tight = FitTextRow(m, FakeInk); // the full sizes leave no gap between the lines
    CHECK(tight.title <= WholePixels(m.title) && tight.detail <= WholePixels(m.subtitle));
    CHECK(tight.title + tight.detail < WholePixels(m.title) + WholePixels(m.subtitle));
    // Both lines shrink together, so the title stays the larger by about as much.
    CHECK(std::fabs(static_cast<float>(tight.detail) / static_cast<float>(tight.title) - m.subtitle / m.title) < 0.1f);
}

TEST(a_target_too_narrow_for_its_padding_icons_and_a_few_letters_is_dropped)
{
    const auto natural = Widths(100, 0, 120, 300, 90, 60, 50);
    const auto room = [](const BarLayout& l) { return l.targetWidth; };
    // 700 wide leaves the target some room, less than all of it.
    const BarLayout some = LayoutBar(700.0f, kPills, BarShape::Pills, natural, 0.0f);
    CHECK(room(some) > 0.0f && room(some) < 300.0f);
    const BarLayout dropped = LayoutBar(700.0f, kPills, BarShape::Pills, natural, room(some) + 1.0f);
    CHECK(RectOf(dropped, BarWidget::Target) == nullptr && dropped.targetWidth == 0.0f);
    const BarLayout kept = LayoutBar(700.0f, kPills, BarShape::Pills, natural, room(some) - 1.0f);
    CHECK(RectOf(kept, BarWidget::Target) != nullptr && RectOf(kept, BarWidget::Target)->width >= room(some) - 1.01f);
    // A short name needs no more than all of itself.
    const BarLayout shortName = LayoutBar(1920.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 60, 90, 60, 50), 90.0f);
    CHECK(RectOf(shortName, BarWidget::Target) != nullptr && Near(shortName.targetWidth, 60.0f));
}

TEST(a_popup_and_its_outer_ring_keep_the_gap_from_the_screen_and_the_bar)
{
    const float gap = 6.0f, ring = 3.0f, bar = 37.0f, screenW = 1280.0f, screenH = 720.0f;
    for (const float widgetX : {0.0f, 400.0f, 1250.0f})
    {
        const BarRect p = PlacePopup(BarRect{widgetX, 3.0f, 30.0f, 29.0f}, bar, 300.0f, 2000.0f, screenW, screenH, gap,
                                     ring);
        CHECK(p.x - ring >= gap - 0.01f && p.x + p.width + ring <= screenW - gap + 0.01f);
        CHECK(p.y - ring >= bar + gap - 0.01f && p.y + p.height + ring <= screenH - gap + 0.01f);
        // Cut to the room there is, which a popup can be laid out for beforehand.
        CHECK(Near(p.height, PopupRoom(bar, screenH, gap, ring)));
        CHECK(Near(p.y + p.height + ring, screenH - gap));
    }
    // A screen too small for it still gives a popup no negative size.
    const BarRect tiny = PlacePopup(BarRect{0.0f, 0.0f, 10.0f, 10.0f}, 30.0f, 300.0f, 200.0f, 200.0f, 35.0f, gap, ring);
    CHECK(tiny.height >= 0.0f && tiny.width >= 0.0f);
}

TEST(the_shortest_cut_of_a_name_is_its_first_letters_and_an_ellipsis)
{
    CHECK(Shortest("Crimson-toothed Pawberry", 3) == "Cri...");
    CHECK(Shortest("Bat", 3) == "Bat");
    CHECK(Shortest("Ab Cd", 3) == "Ab...");
    CHECK(Shortest("\xC3\x89" "clair", 3) == "\xC3\x89" "cl...");
    CHECK(Shortest("", 3).empty());
}

namespace
{
    // Windows' mouse messages and the button flags in their wParam (winuser.h).
    constexpr uint32_t WM_MOUSEMOVE = 0x200, WM_LBUTTONDOWN = 0x201, WM_LBUTTONUP = 0x202, WM_LBUTTONDBLCLK = 0x203,
                       WM_RBUTTONDOWN = 0x204, WM_RBUTTONUP = 0x205, WM_RBUTTONDBLCLK = 0x206, WM_MOUSEWHEEL = 0x20A;
    constexpr uint32_t MK_LBUTTON = 0x1, MK_RBUTTON = 0x2;

    // On the target's pill, and well below the bar: the game's.
    constexpr float kOnBarX = 960.0f, kOnBarY = 18.0f, kGameY = 700.0f;

    struct Clicks
    {
        BarInput input;
        BarClicks clicks{input};
        Clicks()
        {
            std::array<bool, kPanelSlots> popups{};
            popups[SlotOf(BarWidget::Target)] = true;
            input.Frame(LayoutBar(1920.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 200, 90, 60, 50), 0.0f),
                popups);
        }
        // The bar on, nothing blocking and no Ashita window under the pointer, unless said.
        bool Take(uint32_t message, float y, uint32_t buttons = 0, bool barOn = true, bool overGui = false,
            bool blocked = false)
        {
            return clicks.Take(message, buttons, kOnBarX, y, barOn, overGui, blocked);
        }
    };
}

TEST(a_press_the_bar_takes_keeps_its_release_from_the_game_once)
{
    Clicks c;
    CHECK(c.Take(WM_LBUTTONDOWN, kOnBarY));
    CHECK(c.input.Pinned() == BarWidget::Target);
    CHECK(!c.Take(WM_MOUSEMOVE, kGameY, MK_LBUTTON)); // dragged off the bar, still held
    CHECK(c.Take(WM_LBUTTONUP, kGameY));             // released over the game: still the bar's
    CHECK(!c.Take(WM_LBUTTONUP, kGameY));            // the next one is the game's
    CHECK(c.Take(WM_RBUTTONDOWN, kOnBarY));
    CHECK(!c.Take(WM_LBUTTONUP, kOnBarY)); // each button keeps its own
    CHECK(c.Take(WM_RBUTTONUP, kGameY));
    CHECK(!c.Take(WM_RBUTTONUP, kGameY));
}

TEST(a_double_click_on_the_bar_is_a_press_the_bar_takes)
{
    Clicks c;
    CHECK(c.Take(WM_LBUTTONDBLCLK, kOnBarY));
    CHECK(c.Take(WM_LBUTTONUP, kOnBarY));
    CHECK(c.Take(WM_RBUTTONDBLCLK, kOnBarY));
    CHECK(c.Take(WM_RBUTTONUP, kOnBarY));
}

TEST(a_move_with_the_button_up_ends_a_press_released_outside_the_window)
{
    Clicks c;
    CHECK(c.Take(WM_LBUTTONDOWN, kOnBarY));
    CHECK(c.Take(WM_RBUTTONDOWN, kOnBarY));
    CHECK(!c.Take(WM_MOUSEMOVE, kGameY, MK_RBUTTON)); // the left one went up outside
    CHECK(!c.Take(WM_LBUTTONUP, kGameY));
    CHECK(c.Take(WM_RBUTTONUP, kGameY));
}

// Released outside the window and back with no move the game saw (Alt-Tab with the pointer already over it): the
// next press shows the last one ended, and the game gets both halves of a click it was given.
TEST(a_press_the_bar_does_not_take_ends_one_it_took)
{
    Clicks c;
    CHECK(c.Take(WM_LBUTTONDOWN, kOnBarY));
    CHECK(!c.Take(WM_LBUTTONDOWN, kGameY));
    CHECK(!c.Take(WM_LBUTTONUP, kGameY));
    CHECK(c.Take(WM_RBUTTONDOWN, kOnBarY));
    CHECK(!c.Take(WM_RBUTTONDBLCLK, kOnBarY, 0, true, true)); // over an Ashita window: not the bar's either
    CHECK(!c.Take(WM_RBUTTONUP, kOnBarY));
    CHECK(c.Take(WM_LBUTTONDOWN, kOnBarY));
    CHECK(!c.Take(WM_LBUTTONDOWN, kOnBarY, 0, true, false, true)); // another plugin blocked it
    CHECK(!c.Take(WM_LBUTTONUP, kOnBarY));
}

// Over an Ashita window, or blocked by another plugin, a move is not the bar's, and it is the only news the pointer
// left the bar: a popup hovering opened closes, and a pinned one stays.
TEST(a_move_the_bar_may_not_see_closes_a_hovered_popup_and_keeps_a_pinned_one)
{
    for (const bool overGui : {true, false})
    {
        Clicks c;
        CHECK(!c.Take(WM_MOUSEMOVE, kOnBarY));
        CHECK(c.input.Open() == BarWidget::Target);
        CHECK(!c.Take(WM_MOUSEMOVE, kOnBarY, 0, true, overGui, !overGui));
        CHECK(!c.input.Open().has_value());
        CHECK(c.Take(WM_LBUTTONDOWN, kOnBarY));
        CHECK(c.Take(WM_LBUTTONUP, kOnBarY));
        CHECK(!c.Take(WM_MOUSEMOVE, kOnBarY, 0, true, overGui, !overGui));
        CHECK(c.input.Open() == BarWidget::Target && !c.input.Hovered().has_value());
    }
}

TEST(the_bar_going_off_gives_the_game_every_release)
{
    Clicks c;
    CHECK(c.Take(WM_LBUTTONDOWN, kOnBarY));
    CHECK(c.Take(WM_RBUTTONDOWN, kOnBarY));
    CHECK(!c.Take(WM_MOUSEMOVE, kOnBarY, MK_LBUTTON | MK_RBUTTON, false));
    CHECK(!c.Take(WM_LBUTTONUP, kOnBarY));
    CHECK(!c.Take(WM_RBUTTONUP, kOnBarY));
}

TEST(the_bar_takes_nothing_off_it_or_while_it_may_not)
{
    Clicks c;
    for (const uint32_t message : {WM_LBUTTONDOWN, WM_LBUTTONUP, WM_RBUTTONDOWN, WM_RBUTTONUP, WM_MOUSEWHEEL})
        CHECK(!c.Take(message, kGameY));
    CHECK(!c.Take(WM_LBUTTONDOWN, kOnBarY, 0, false));              // off
    CHECK(!c.Take(WM_LBUTTONDOWN, kOnBarY, 0, true, true));         // under an Ashita window
    CHECK(!c.Take(WM_LBUTTONDOWN, kOnBarY, 0, true, false, true));  // blocked
    CHECK(!c.Take(WM_LBUTTONUP, kOnBarY));
    CHECK(!c.input.Pinned().has_value());
    CHECK(!c.Take(WM_MOUSEWHEEL, kOnBarY)); // the wheel stays the game's on the bar too
    // Only a move the bar may see hovers.
    CHECK(!c.Take(WM_MOUSEMOVE, kOnBarY, 0, true, true));
    CHECK(!c.input.Open().has_value());
    CHECK(!c.Take(WM_MOUSEMOVE, kOnBarY, 0, true, false, true));
    CHECK(!c.input.Open().has_value());
    CHECK(!c.Take(WM_MOUSEMOVE, kOnBarY));
    CHECK(c.input.Open() == BarWidget::Target);
}

TEST(every_widget_has_one_place_on_the_bar_and_none_overlap_on_a_narrow_screen)
{
    std::array<float, kPanelSlots> natural{};
    natural.fill(10.0f);
    const BarLayout wide = LayoutBar(4000.0f, kPills, BarShape::Pills, natural, 0.0f);
    CHECK_EQ(wide.widgets.size(), static_cast<size_t>(kPlacedWidgetCount));
    for (int w = 0; w < kBarWidgetCount; ++w)
        CHECK((RectOf(wide, static_cast<BarWidget>(w)) != nullptr) == OnTheBar(static_cast<BarWidget>(w)));
    for (const auto& [width, screen] : {std::pair{120.0f, 1280.0f}, std::pair{250.0f, 640.0f}})
    {
        natural.fill(width);
        const BarLayout narrow = LayoutBar(screen, kPills, BarShape::Pills, natural, 60.0f);
        CHECK(!narrow.widgets.empty());
        for (size_t a = 0; a < narrow.widgets.size(); ++a)
        {
            const BarRect& r = narrow.widgets[a].rect;
            CHECK(r.x >= 0.0f && r.x + r.width <= screen); // at 640 px the left group alone is wider than the screen
            for (size_t b = a + 1; b < narrow.widgets.size(); ++b)
            {
                const BarRect& o = narrow.widgets[b].rect;
                CHECK(r.x + r.width <= o.x || o.x + o.width <= r.x);
            }
        }
    }
}

namespace
{
    BarPlacement Sections(std::vector<PanelWidget> left, std::vector<PanelWidget> center, std::vector<PanelWidget> right)
    {
        return BarPlacement{std::move(left), std::move(center), std::move(right)};
    }

    std::array<float, kPanelSlots> SomeWidths()
    {
        std::array<float, kPanelSlots> natural{};
        natural[SlotOf(BarWidget::Zone)]      = 100.0f;
        natural[SlotOf(BarWidget::Target)]    = 200.0f;
        natural[SlotOf(BarWidget::Gil)]       = 90.0f;
        natural[SlotOf(BarWidget::Bags)]      = 60.0f;
        natural[SlotOf(BarWidget::EarthTime)] = 50.0f;
        return natural;
    }

    int SectionOf(const BarPlacement& p, PanelWidget w)
    {
        for (int s = 0; s < kBarSectionCount; ++s)
            if (std::ranges::find(p[s], w) != p[s].end()) return s;
        return -1;
    }

    // Every widget an edge (a pills gap, an ornate end cap) from the screen's sides; widgets in different sections a
    // gap apart, pills a gap apart within one too, and an ornate target clear of the diamond on its neighbour's edge.
    void CheckApartAndInside(const BarLayout& l, float screen, const BarPlacement& p = DefaultPlacement(),
                             BarShape shape = BarShape::Pills)
    {
        const bool ornate        = shape == BarShape::Ornate;
        const BarMetrics& m      = ornate ? kOrnate : kPills;
        const float edge         = ornate ? m.cap : m.gap;
        const float diamondClear = static_cast<float>(WholePixels(m.diamond)) / 2.0f + 2.0f;
        for (size_t a = 0; a < l.widgets.size(); ++a)
        {
            const BarRect& r = l.widgets[a].rect;
            CHECK(r.x >= edge - 0.01f && r.x + r.width <= screen - edge + 0.01f);
            for (size_t b = a + 1; b < l.widgets.size(); ++b)
            {
                const PanelWidget wa = l.widgets[a].widget, wb = l.widgets[b].widget;
                const BarRect& o   = l.widgets[b].rect;
                const bool framed  = wa == BarWidget::Target || wb == BarWidget::Target;
                const float apart  = SectionOf(p, wa) != SectionOf(p, wb) || !ornate ? m.gap
                                     : framed                                       ? diamondClear
                                                                                    : 0.0f;
                CHECK(r.x + r.width + apart <= o.x + 0.01f || o.x + o.width + apart <= r.x + 0.01f);
            }
        }
    }
}

TEST(the_center_holds_several_widgets_centered_as_a_group)
{
    const float g     = kPills.gap;
    const BarLayout l = LayoutBar(1920.0f, kPills, BarShape::Pills, SomeWidths(), 0.0f,
        Sections({BarWidget::Zone}, {BarWidget::Gil, BarWidget::Target, BarWidget::Bags}, {BarWidget::EarthTime}));
    const float total = 90.0f + g + 200.0f + g + 60.0f, x0 = 960.0f - total / 2.0f;
    CHECK(Near(RectOf(l, BarWidget::Gil)->x, x0) && Near(RectOf(l, BarWidget::Target)->x, x0 + 90.0f + g));
    CHECK(Near(RectOf(l, BarWidget::Bags)->x, x0 + 90.0f + g + 200.0f + g));
    CHECK(WidgetAt(l, x0 + 10.0f, PillRow(kPills).y + 1.0f) == BarWidget::Gil); // hover finds it where it is
}

TEST(an_empty_center_leaves_the_sides_alone_and_the_target_can_sit_in_a_side)
{
    const float g     = kPills.gap;
    const BarLayout l = LayoutBar(1920.0f, kPills, BarShape::Pills, SomeWidths(), 0.0f,
        Sections({BarWidget::Zone, BarWidget::Target}, {}, {BarWidget::Gil, BarWidget::Bags, BarWidget::EarthTime}));
    CHECK(Near(RectOf(l, BarWidget::Target)->x, g + 100.0f + g) && Near(l.targetWidth, 200.0f));
    CHECK(Near(RectOf(l, BarWidget::Target)->y, PillRow(kPills).y));
    CHECK(Near(RectOf(l, BarWidget::EarthTime)->x + 50.0f, 1920.0f - g));
    const BarLayout ornate = LayoutBar(1920.0f, kOrnate, BarShape::Ornate, SomeWidths(), 0.0f,
        Sections({BarWidget::Zone, BarWidget::Target}, {}, {BarWidget::Gil}));
    CHECK(Near(RectOf(ornate, BarWidget::Target)->height, PillRow(kOrnate).height)); // still framed
}

TEST(a_target_in_a_side_section_shrinks_to_the_room_left_and_goes_when_that_is_too_little)
{
    const float g = kPills.gap;
    const auto right = Sections({BarWidget::Zone}, {}, {BarWidget::Target, BarWidget::Gil});
    // At 400 px the room between zone and gil, a gap from each, is 400 - 4g - 190 = 185.5 px: under the target's 200.
    const float room  = (400.0f - g - 90.0f - g) - (g + 100.0f + g);
    const BarLayout l = LayoutBar(400.0f, kPills, BarShape::Pills, SomeWidths(), 40.0f, right);
    CHECK(Near(l.targetWidth, std::floor(room))); // whole pixels
    const BarRect& target = *RectOf(l, BarWidget::Target);
    CHECK(target.x >= g + 100.0f + g - 0.01f && target.x + target.width + g <= RectOf(l, BarWidget::Gil)->x + 0.01f);
    CheckApartAndInside(l, 400.0f, right);
    const BarLayout tight = LayoutBar(400.0f, kPills, BarShape::Pills, SomeWidths(), room + 1.0f, right);
    CHECK(RectOf(tight, BarWidget::Target) == nullptr && RectOf(tight, BarWidget::Gil) != nullptr);
}

TEST(a_target_first_in_the_left_section_gives_way_before_any_right_widget)
{
    const float g     = kPills.gap;
    const auto first  = Sections({BarWidget::Target, BarWidget::Zone}, {}, {BarWidget::Gil, BarWidget::Bags,
                                                                            BarWidget::EarthTime});
    // Everything at its width: 2 edges, target, gap, zone, then a gap, gil, gap, bags, gap, Earth time.
    const float demand = g + 200.0f + g + 100.0f + g + 90.0f + g + 60.0f + g + 50.0f + g;
    const BarLayout l  = LayoutBar(500.0f, kPills, BarShape::Pills, SomeWidths(), 40.0f, first);
    CHECK(Near(l.targetWidth, std::floor(200.0f - (demand - 500.0f))));
    for (const BarWidget w : {BarWidget::Zone, BarWidget::Gil, BarWidget::Bags, BarWidget::EarthTime})
        CHECK(RectOf(l, w) != nullptr);
    CheckApartAndInside(l, 500.0f, first);
}

TEST(a_crowded_center_shrinks_its_target_then_drops_it_then_drops_from_its_end)
{
    const float g     = kPills.gap;
    const auto center = Sections({BarWidget::Zone}, {BarWidget::Target, BarWidget::Gil, BarWidget::Bags}, {});
    // At 450 px the center has 450 - 3g - 100 px: about 31 px short of the target, gil and bags at their widths.
    const BarLayout l = LayoutBar(450.0f, kPills, BarShape::Pills, SomeWidths(), 40.0f, center);
    const float room  = 450.0f - 2.0f * g - (g + 100.0f);
    CHECK(Near(l.targetWidth, std::floor(room - 90.0f - 60.0f - 2.0f * g)) && RectOf(l, BarWidget::Bags) != nullptr);
    const BarLayout noTarget = LayoutBar(450.0f, kPills, BarShape::Pills, SomeWidths(), 400.0f, center);
    CHECK(RectOf(noTarget, BarWidget::Target) == nullptr && RectOf(noTarget, BarWidget::Bags) != nullptr);
    // At 250 px, gil and bags alone are too wide: bags, at the center's end, goes.
    const BarLayout narrow = LayoutBar(250.0f, kPills, BarShape::Pills, SomeWidths(), 400.0f, center);
    CHECK(RectOf(narrow, BarWidget::Gil) != nullptr && RectOf(narrow, BarWidget::Bags) == nullptr);
    for (const BarLayout* each : {&l, &noTarget, &narrow})
        CheckApartAndInside(*each, each == &narrow ? 250.0f : 450.0f, center);
}

TEST(any_widget_in_any_section_keeps_every_widget_apart_and_on_screen)
{
    std::array<float, kPanelSlots> natural{};
    for (int w = 0; w < kBarWidgetCount; ++w)
        for (int s = 0; s < kBarSectionCount && OnTheBar(static_cast<BarWidget>(w)); ++s)
            for (const auto& [width, screen] : {std::pair{10.0f, 4000.0f}, std::pair{120.0f, 1280.0f},
                     std::pair{250.0f, 640.0f}})
            {
                BarPlacement p = DefaultPlacement();
                int from       = 0;
                size_t index   = 0;
                for (int f = 0; f < kBarSectionCount; ++f)
                    for (size_t i = 0; i < p[f].size(); ++i)
                        if (p[f][i] == static_cast<BarWidget>(w)) from = f, index = i;
                MoveWidget(p, static_cast<BarSection>(from), index, static_cast<BarSection>(s), p[s].size());
                natural.fill(width);
                for (const BarShape shape : {BarShape::Pills, BarShape::Ornate})
                {
                    const BarLayout l = LayoutBar(screen, shape == BarShape::Pills ? kPills : kOrnate, shape, natural,
                        60.0f, p);
                    if (screen > 3000.0f) CHECK_EQ(l.widgets.size(), static_cast<size_t>(kPlacedWidgetCount));
                    CheckApartAndInside(l, screen, p, shape);
                    // The target gives way first: whatever shows without it shows with it.
                    std::array<float, kPanelSlots> noTarget = natural;
                    noTarget[SlotOf(BarWidget::Target)] = 0.0f;
                    for (const PlacedWidget& kept : LayoutBar(screen, shape == BarShape::Pills ? kPills : kOrnate,
                             shape, noTarget, 60.0f, p).widgets)
                        CHECK(RectOf(l, kept.widget) != nullptr);
                }
            }
}

TEST(a_target_that_gave_way_comes_back_into_the_room_the_dropped_widgets_left)
{
    const float g = kPills.gap;
    // Zone 100 and Vana'diel time 120 on the left, gil 90, bags 60 and Earth time 50 on the right. Against every
    // widget the target would keep floor(S - 420 - 7g) px, under its 40 px minimum; gil needs S >= 420 + 6g, so it
    // goes too, and without gil the room between the groups is S - 330 - 6g = 70.5 px.
    const float screen = 400.5f + 6.0f * g;
    const BarLayout l  = LayoutBar(screen, kPills, BarShape::Pills, Widths(100, 0, 120, 200, 90, 60, 50), 40.0f);
    CHECK(RectOf(l, BarWidget::Gil) == nullptr);
    const BarRect* target = RectOf(l, BarWidget::Target);
    CHECK(target != nullptr);
    const float room = RectOf(l, BarWidget::Bags)->x - g - (RectOf(l, BarWidget::VanaTime)->x + 120.0f) - g;
    CHECK(Near(room, 70.5f) && Near(l.targetWidth, std::floor(room)) && target && Near(target->width, 70.0f));
    CHECK(target && target->x >= RectOf(l, BarWidget::VanaTime)->x + 120.0f + g - 0.01f);
    CheckApartAndInside(l, screen);
}

TEST(an_ornate_target_in_a_side_section_keeps_its_frame_clear_of_the_diamonds_beside_it)
{
    // A divider's diamond is drawn WholePixels(diamond) wide, centered on its x; a frame keeps 2 px clear of it.
    const float clear = static_cast<float>(WholePixels(kOrnate.diamond)) / 2.0f + 2.0f;
    const auto sides  = Sections({BarWidget::Zone, BarWidget::Target, BarWidget::Gil}, {}, {BarWidget::EarthTime});
    const BarLayout l = LayoutBar(1920.0f, kOrnate, BarShape::Ornate, SomeWidths(), 0.0f, sides);
    const BarRect& target = *RectOf(l, BarWidget::Target);
    CHECK(Near(target.width, 200.0f));
    CHECK_EQ(l.dividers.size(), static_cast<size_t>(2)); // zone | target | gil
    for (const float d : l.dividers)
        CHECK(std::fabs(d - target.x) >= clear - 0.01f && std::fabs(d - (target.x + target.width)) >= clear - 0.01f &&
              !(d > target.x && d < target.x + target.width));
    // The diamonds still sit on the plain neighbours' edges, and the section is that much wider.
    CHECK(Near(l.dividers[0], kOrnate.cap + 100.0f) && Near(target.x, kOrnate.cap + 100.0f + clear));
    CHECK(Near(RectOf(l, BarWidget::Gil)->x, target.x + 200.0f + clear));
    CHECK(Near(l.dividers[1], RectOf(l, BarWidget::Gil)->x));
    CheckApartAndInside(l, 1920.0f, sides, BarShape::Ornate);
}

TEST(a_target_in_a_side_section_also_takes_back_the_room_a_dropped_neighbour_left)
{
    const float g = kPills.gap;
    // The center case's widths with the target first on the right: placed inward, Earth time, bags, gil, then the
    // target. Against every widget it would keep floor(S - 420 - 7g) px, under 40; gil needs S >= 420 + 6g and goes;
    // without it the target has S - 330 - 6g = 70.5 px between Vana'diel time and bags.
    const float screen = 400.5f + 6.0f * g;
    const auto right   = Sections({BarWidget::Zone, BarWidget::VanaTime}, {},
        {BarWidget::Target, BarWidget::Gil, BarWidget::Bags, BarWidget::EarthTime});
    const BarLayout l  = LayoutBar(screen, kPills, BarShape::Pills, Widths(100, 0, 120, 200, 90, 60, 50), 40.0f, right);
    CHECK(RectOf(l, BarWidget::Gil) == nullptr && RectOf(l, BarWidget::Target) != nullptr);
    CHECK(Near(l.targetWidth, 70.0f));
    CheckApartAndInside(l, screen, right);
}

TEST(a_target_in_a_left_section_that_passes_the_screen_edge_takes_back_the_room_left)
{
    const float g = kPills.gap;
    // Left: zone 100, the target 200, gil 90. Against all three the target would keep floor(S - 190 - 4g) px, less
    // than nothing; gil then needs S >= 190 + 3g and goes past the edge; zone alone leaves the target S - 100 - 3g.
    const float screen = 170.5f + 3.0f * g;
    const auto left    = Sections({BarWidget::Zone, BarWidget::Target, BarWidget::Gil}, {}, {});
    const BarLayout l  = LayoutBar(screen, kPills, BarShape::Pills, Widths(100, 0, 0, 200, 90, 0, 0), 40.0f, left);
    CHECK(RectOf(l, BarWidget::Gil) == nullptr && RectOf(l, BarWidget::Zone) != nullptr);
    CHECK(Near(l.targetWidth, 70.0f));
    CheckApartAndInside(l, screen, left);
}

TEST(a_target_that_fits_exactly_keeps_its_whole_width)
{
    // The default bar with the screen exactly as wide as everything: float sums can come out a hair over the screen,
    // which must not cost the target a pixel.
    const float g = kPills.gap;
    for (int i = 0; i < 1000; ++i)
    {
        const float want = 800.0f + 0.001f * static_cast<float>(i);
        const double sum = 2.0 * g + 100.3 + g + 120.7 + g + want + g + 90.1 + g + 60.9 + g + 50.3;
        const BarLayout l = LayoutBar(static_cast<float>(sum), kPills, BarShape::Pills,
            Widths(100.3f, 0, 120.7f, want, 90.1f, 60.9f, 50.3f), 40.0f);
        CHECK_EQ(l.targetWidth, want);
    }
}

TEST(the_right_section_keeps_a_gap_from_the_left_or_its_inner_widget_goes)
{
    const float g     = kPills.gap;
    const auto sides  = Sections({BarWidget::Zone}, {}, {BarWidget::Gil});
    // Gil against the right edge would start half a gap after zone ends.
    const float screen = g + 100.0f + g / 2.0f + 90.0f + g;
    const BarLayout l  = LayoutBar(screen, kPills, BarShape::Pills, Widths(100, 0, 0, 0, 90, 0, 0), 0.0f, sides);
    CHECK(RectOf(l, BarWidget::Zone) != nullptr && RectOf(l, BarWidget::Gil) == nullptr);
    CheckApartAndInside(l, screen, sides);
}

TEST(separators_follow_the_theme_until_the_player_picks)
{
    CHECK(DiamondsOn(BarSeparators::Theme, BarShape::Ornate) && !DiamondsOn(BarSeparators::Theme, BarShape::Pills));
    for (const BarShape shape : {BarShape::Ornate, BarShape::Pills})
        CHECK(DiamondsOn(BarSeparators::Diamonds, shape) && !DiamondsOn(BarSeparators::None, shape));
}

TEST(pills_with_diamonds_center_one_in_a_wider_gap_between_neighbours_of_a_section)
{
    const float g = kPills.gap, d = static_cast<float>(WholePixels(kPills.diamond));
    const BarLayout l = LayoutBar(1920.0f, kPills, BarShape::Pills, Widths(100, 80, 120, 200, 90, 60, 50), 0.0f,
        DefaultPlacement(), BarSeparators::Diamonds);
    const BarRect& zone = *RectOf(l, BarWidget::Zone);
    const BarRect& weather = *RectOf(l, BarWidget::Weather);
    CHECK(Near(weather.x, zone.x + zone.width + g + d + g)); // a gap, the diamond, a gap
    CHECK(l.dividers.size() == 4);                          // two on the left, two on the right, none by the target
    CHECK(Near(l.dividers[0], zone.x + zone.width + g + d / 2.0f));
    for (const float dx : l.dividers) // every diamond keeps a gap from both pills beside it
        for (const PlacedWidget& p : l.widgets)
            CHECK(dx + d / 2.0f + g <= p.rect.x + 0.01f || p.rect.x + p.rect.width + g <= dx - d / 2.0f + 0.01f);
    CHECK(LayoutBar(1920.0f, kPills, BarShape::Pills, Widths(100, 80, 120, 200, 90, 60, 50), 0.0f).dividers.empty());
}

TEST(ornate_without_diamonds_keeps_its_widgets_where_they_were)
{
    const auto widths = Widths(100, 80, 120, 200, 90, 60, 50);
    const BarLayout with = LayoutBar(1920.0f, kOrnate, BarShape::Ornate, widths, 0.0f);
    const BarLayout none = LayoutBar(1920.0f, kOrnate, BarShape::Ornate, widths, 0.0f, DefaultPlacement(),
        BarSeparators::None);
    CHECK(none.dividers.empty() && with.dividers.size() == 4);
    for (size_t i = 0; i < with.widgets.size(); ++i)
        CHECK(with.widgets[i].rect == none.widgets[i].rect);
}

TEST(diamonds_divide_neighbours_within_each_section_and_never_between_sections)
{
    const auto center = Sections({BarWidget::Zone}, {BarWidget::Gil, BarWidget::Bags}, {BarWidget::EarthTime});
    const BarLayout l = LayoutBar(1920.0f, kPills, BarShape::Pills, SomeWidths(), 0.0f, center, BarSeparators::Diamonds);
    CHECK(l.dividers.size() == 1); // between gil and bags, in the center
    CHECK(l.dividers[0] > RectOf(l, BarWidget::Gil)->x && l.dividers[0] < RectOf(l, BarWidget::Bags)->x);
}

TEST(a_bar_text_size_that_fits_is_drawn_and_one_that_does_not_is_shrunk_to_what_fits)
{
    for (const BarShape shape : {BarShape::Ornate, BarShape::Pills})
    {
        const TextRow roomy = FitTextRow(MetricsFor(64, shape, 16, kAutoBarTextSize), FakeInk);
        CHECK(roomy.twoLines && roomy.title == 16 && roomy.detail == WholePixels(16.0f * 0.33f / 0.40f));
        const BarMetrics small = MetricsFor(24, shape, kMaxBarTextSize, kAutoBarTextSize);
        const TextRow tight    = FitTextRow(small, FakeInk);
        CHECK(tight.title < kMaxBarTextSize);
        // As large as the row holds: what fits at Auto's size for this height, no less.
        CHECK(tight.title >= FitTextRow(MetricsFor(24, shape), FakeInk).title);
        const BarRow row = PillRow(small);
        const LineInk ink = FakeInk(true, tight.title);
        const float y     = tight.twoLines ? tight.titleY : tight.aloneY;
        CHECK(y + ink.top >= row.y && y + ink.bottom <= row.y + row.height); // its ink inside the row
    }
}

TEST(a_set_bar_text_size_is_drawn_at_that_size_alone_when_two_lines_do_not_fit)
{
    for (const BarShape shape : {BarShape::Ornate, BarShape::Pills})
    {
        const BarRow row = PillRow(MetricsFor(45, shape));
        for (const int size : {16, 24})
        {
            const TextRow t = FitTextRow(MetricsFor(45, shape, size, kAutoBarTextSize), FakeInk);
            CHECK_EQ(t.title, size);
            const LineInk title = FakeInk(true, size);
            if (t.twoLines)
            {
                CHECK(t.detail >= 9 && t.detail <= WholePixels(static_cast<float>(size) * 0.33f / 0.40f));
                const LineInk detail = FakeInk(false, t.detail);
                CHECK(t.titleY + title.top >= row.y && t.detailY + detail.bottom <= row.y + row.height);
            }
            else
                CHECK(t.aloneY + title.top >= row.y && t.aloneY + title.bottom <= row.y + row.height);
        }
        // The row holds 28 px of ink (36 less its border and 3 px margins). A 24 px title's ink is 22.8 px: not even a
        // 9 px detail line (8.55) fits 3 px under it, so it shows alone.
        CHECK(!FitTextRow(MetricsFor(45, shape, 24, kAutoBarTextSize), FakeInk).twoLines);
    }
}

TEST(a_set_bar_text_size_keeps_its_title_and_shrinks_only_the_detail_line_to_fit)
{
    for (const BarShape shape : {BarShape::Ornate, BarShape::Pills})
    {
        // As Auto at 45 px: two lines, a 14 px title over a detail line of at least 9 px.
        const TextRow fourteen = FitTextRow(MetricsFor(45, shape, 14, kAutoBarTextSize), FakeInk);
        CHECK(fourteen.twoLines && fourteen.title == 14 && fourteen.detail >= 9);
        // 16 px over its 13 px share needs 15.2 + 12.35 + 3 of the 28 px; the detail takes the most that fits under the
        // title, 28 - 15.2 - 3 = 9.8 px of ink, a 10 px line.
        const TextRow sixteen = FitTextRow(MetricsFor(45, shape, 16, kAutoBarTextSize), FakeInk);
        CHECK(sixteen.twoLines && sixteen.title == 16);
        CHECK_EQ(sixteen.detail, 10);
        const BarRow row     = PillRow(MetricsFor(45, shape));
        const LineInk detail = FakeInk(false, sixteen.detail);
        CHECK(sixteen.detailY + detail.bottom <= row.y + row.height);
    }
}

TEST(a_larger_bar_text_size_never_draws_smaller_titles)
{
    for (const BarShape shape : {BarShape::Ornate, BarShape::Pills})
        for (int h = kMinBarHeight; h <= kMaxBarHeight; ++h)
        {
            int last = 0;
            for (int size = kAutoBarTextSize + 1; size <= kMaxBarTextSize; ++size)
            {
                const int drawn = FitTextRow(MetricsFor(h, shape, size, kAutoBarTextSize), FakeInk).title;
                CHECK(drawn >= last && drawn <= size);
                last = drawn;
            }
        }
}

TEST(each_copy_of_a_widget_takes_a_place_of_its_own)
{
    BarPlacement placement = DefaultPlacement();
    const std::optional<PanelWidget> copy = AddCopy(placement, BarWidget::Gil);
    CHECK(copy.has_value());
    std::array<float, kPanelSlots> natural = Widths(100, 0, 120, 200, 90, 60, 50);
    natural[SlotOf(*copy)] = 70.0f;
    const BarLayout l = LayoutBar(1920.0f, kPills, BarShape::Pills, natural, 0.0f, placement);
    const BarRect* first = RectOf(l, BarWidget::Gil);
    const BarRect* second = RectOf(l, *copy);
    CHECK(first != nullptr && second != nullptr);
    CHECK(Near(second->width, 70.0f) && Near(second->x, first->x + first->width + kPills.gap)); // right after it
}

TEST(a_widgets_own_spacing_adds_room_between_it_and_each_neighbour)
{
    const std::array<float, kPanelSlots> natural = Widths(100, 80, 120, 200, 90, 60, 50);
    std::array<float, kPanelSlots> spacing{};
    spacing[SlotOf(BarWidget::Weather)] = 10.0f;
    for (const auto& [shape, metrics] : {std::pair{BarShape::Pills, kPills}, std::pair{BarShape::Ornate, kOrnate}})
    {
        const BarLayout plain = LayoutBar(1920.0f, metrics, shape, natural, 0.0f);
        const BarLayout spaced = LayoutBar(1920.0f, metrics, shape, natural, 0.0f, DefaultPlacement(), BarSeparators::Theme, spacing);
        CHECK(Near(RectOf(spaced, BarWidget::Zone)->x, RectOf(plain, BarWidget::Zone)->x));
        CHECK(Near(RectOf(spaced, BarWidget::Weather)->x, RectOf(plain, BarWidget::Weather)->x + 10.0f));
        CHECK(Near(RectOf(spaced, BarWidget::VanaTime)->x, RectOf(plain, BarWidget::VanaTime)->x + 20.0f));
        // An ornate divider stands in the middle of the room it is in.
        if (shape == BarShape::Ornate)
        {
            const BarRect& zone = *RectOf(spaced, BarWidget::Zone);
            CHECK(Near(spaced.dividers[0], zone.x + zone.width + 5.0f));
        }
    }
}
