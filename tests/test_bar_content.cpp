#include "bar_content.h"
#include "test.h"

#include <string>
#include <vector>

using namespace headsup;

TEST(a_labeled_row_is_a_pair_of_its_label_dim_and_its_value)
{
    const PopupRow row = Labeled("Latency", Text("48 ms"));
    CHECK(row.kind == RowKind::Pair && row.cells.size() == 2);
    CHECK(row.cells[0].text == (std::vector<Span>{Span{"Latency ", Tone::Dim, 0}}));
    CHECK(row.cells[1].text == (std::vector<Span>{Span{"48 ms", Tone::Normal, 0}}));
}

TEST(a_span_of_time_reads_as_its_two_largest_units_rounded_up)
{
    CHECK_EQ(SpanText(2 * 86400.0 + 8.5 * 3600.0), std::string("2d 8h"));
    CHECK_EQ(SpanText(8.5 * 3600.0), std::string("8h 30m"));
    CHECK_EQ(SpanText(3600.0), std::string("1h 00m"));
    CHECK_EQ(SpanText(309.0), std::string("5m 09s"));
    CHECK_EQ(SpanText(8.2), std::string("9s"));
    CHECK_EQ(SpanText(-1.0), std::string("0s"));
    CHECK_EQ(HourMinute(9, 5), std::string("09:05"));
}

TEST(printed_text_is_one_value_in_a_printf_format)
{
    CHECK_EQ(Printed("%.1f ms", 16.66), std::string("16.7 ms"));
    CHECK_EQ(Printed("%02d", 7), std::string("07"));
    CHECK_EQ(Printed("%03X", 0x2Au), std::string("02A"));
}

TEST(a_widget_on_one_line_puts_its_second_line_beside_its_title_or_leaves_it_off)
{
    WidgetContent bags;
    bags.title         = {Span{"27/30"}};
    bags.subtitle      = {Span{"Bags"}};
    bags.subtitleIcons = {Icon::Invite};
    WidgetContent after = bags;
    OneLine(after, SubtitlePlace::After);
    CHECK(after.title.size() == 2 && after.title[0].text == "27/30 " && after.title[1].text == "Bags");
    CHECK(after.title[1].tone == Tone::Dim && after.subtitle.empty() && after.titleIcons.size() == 1);
    WidgetContent before = bags;
    OneLine(before, SubtitlePlace::Before);
    CHECK(before.title[0].text == "Bags " && before.title[1].text == "27/30");
    WidgetContent hidden = bags;
    OneLine(hidden, SubtitlePlace::Hide);
    CHECK(hidden.title.size() == 1 && hidden.subtitle.empty() && hidden.titleIcons.empty());
}
