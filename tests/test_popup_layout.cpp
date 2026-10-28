#include "popup_layout.h"
#include "test.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>

using namespace headsup;

namespace
{
    bool Near(float a, float b) { return std::fabs(a - b) < 0.01f; }

    // 7 px a character; lines 14 px (text), 18 px (title), 12 px (heading and pinned mark). Like GDI's, a line is
    // taller than its ink: leading above the capitals, and the descenders end a pixel above the line's bottom.
    TextSize Fake(const Span& span, PopupFont font)
    {
        const bool small   = font == PopupFont::Heading || font == PopupFont::Pin;
        const float height = font == PopupFont::Title ? 18.0f : small ? 12.0f : 14.0f;
        return TextSize{7.0f * static_cast<float>(span.text.size()), height, 0.3f * height, height - 1.0f};
    }

    std::vector<const PopupPiece*> Of(const PopupLayout& l, PopupPiece::Kind kind)
    {
        std::vector<const PopupPiece*> out;
        for (const PopupPiece& p : l.pieces)
            if (p.kind == kind) out.push_back(&p);
        return out;
    }

    PopupContent Sample()
    {
        PopupContent c;
        c.icon  = Icon::AggroNQ;
        c.title = {{"Goblin", Tone::Normal}};
        PopupRow grid{RowKind::Grid, 2, {}};
        grid.cells.push_back(PopupCell{Icon::Fire, false, {{"+30%", Tone::Weak}}});
        grid.cells.push_back(PopupCell{Icon::Ice, false, {{"+100%", Tone::Weak}}});
        grid.cells.push_back(PopupCell{Icon::Wind, false, {{"0%", Tone::Neutral}}});
        c.sections.push_back(PopupSection{"Elements", {grid}});
        PopupRow immune{RowKind::Line, 0, {PopupCell{Icon::Bind, true, {}}}};
        c.sections.push_back(PopupSection{"Immune", {immune}});
        return c;
    }

    const PopupPiece* Find(const PopupLayout& l, const std::string& text)
    {
        for (const PopupPiece& p : l.pieces)
            if (p.span.text == text) return &p;
        return nullptr;
    }

    PopupCell Word(const char* text) { return PopupCell{std::nullopt, false, {{text, Tone::Normal}}}; }

    PopupContent OneRow(PopupRow row)
    {
        PopupContent c;
        c.title = {{"Mob", Tone::Normal}};
        c.sections.push_back(PopupSection{"", {row}});
        return c;
    }

    bool Holds(const PopupPiece& frame, float x0, float y0, float x1, float y1, float sideways, float vertical)
    {
        return x0 >= frame.x + sideways - 0.01f && x1 <= frame.x + frame.width - sideways + 0.01f &&
               y0 >= frame.y + vertical - 0.01f && y1 <= frame.y + frame.height - vertical + 0.01f;
    }

    // Every piece lies inside the popup's padding; a tag's text, by its ink, inside its frame with clear space on
    // every side (sideways 0.15 H as along the bar, above and below 2 px and room for the text's shadow, both inside
    // the frame's 1 px border); an immune mark inside its icon, so inside the icon's row; a heading's rule 3 px or
    // more after the heading, as two clear pixels still read as touching.
    bool AllInside(const PopupLayout& l, const BarMetrics& m)
    {
        const PopupPiece* tag     = nullptr;
        const PopupPiece* icon    = nullptr;
        const PopupPiece* heading = nullptr;
        for (const PopupPiece& p : l.pieces)
        {
            if (p.x < m.popupPadding - 0.01f || p.x + p.width > l.width - m.popupPadding + 0.01f ||
                p.y + p.height > l.height + 0.01f)
                return false;
            const TextSize t = Fake(p.span, p.font);
            if (p.kind == PopupPiece::Kind::Tag) tag = &p;
            if (p.kind == PopupPiece::Kind::Icon) icon = &p;
            if (p.kind == PopupPiece::Kind::Text && p.font == PopupFont::Heading) heading = &p;
            if (p.kind == PopupPiece::Kind::Rule &&
                (heading == nullptr || p.x < heading->x + heading->width + 3.0f - 0.01f))
                return false;
            const bool tagged =
                tag != nullptr && p.x >= tag->x && p.x < tag->x + tag->width && p.y < tag->y + tag->height;
            if (p.kind == PopupPiece::Kind::Text && tagged &&
                !Holds(*tag, p.x, p.y + t.inkTop, p.x + p.width, p.y + t.inkBottom, 0.15f * m.height + 1.0f, 3.5f))
                return false;
            if (p.kind == PopupPiece::Kind::ImmuneMark &&
                (icon == nullptr || !Holds(*icon, p.x, p.y, p.x + p.width, p.y + p.height, 0.0f, 0.0f)))
                return false;
        }
        return true;
    }
}

TEST(the_title_sits_beside_its_icon_inside_the_padding)
{
    const BarMetrics m  = MetricsFor(36, BarShape::Ornate);
    const PopupLayout l = LayoutPopup(Sample(), m, BarShape::Ornate, false, Fake);
    const PopupPiece* icon  = Of(l, PopupPiece::Kind::Icon).front();
    CHECK(Near(icon->x, m.popupPadding) && Near(icon->y, m.popupPadding));
    const PopupPiece& title = l.pieces[1];
    CHECK(title.kind == PopupPiece::Kind::Text && title.font == PopupFont::Title && title.span.text == "Goblin");
    CHECK(title.x > icon->x + icon->width);
}

TEST(grid_columns_line_up_and_the_popup_wraps_its_contents)
{
    const BarMetrics m  = MetricsFor(36, BarShape::Ornate);
    const PopupLayout l = LayoutPopup(Sample(), m, BarShape::Ornate, false, Fake);
    std::vector<const PopupPiece*> texts;
    for (const PopupPiece& p : l.pieces)
        if (p.kind == PopupPiece::Kind::Text && p.font == PopupFont::Text) texts.push_back(&p);
    CHECK(texts.size() == 3);
    CHECK(Near(texts[0]->x, texts[2]->x)); // +30% and 0% share the first column
    CHECK(texts[1]->x > texts[0]->x + texts[0]->width && Near(texts[0]->y, texts[1]->y));
    CHECK(texts[2]->y > texts[0]->y);
    for (const PopupPiece& p : l.pieces)
        CHECK(p.x + p.width <= l.width - m.popupPadding + 0.01f && p.y + p.height <= l.height + 0.01f);
}

TEST(each_heading_gets_a_card_holding_its_rows_and_goes_uppercase_as_pills)
{
    for (const BarShape shape : {BarShape::Ornate, BarShape::Pills})
    {
        const BarMetrics m  = MetricsFor(36, shape);
        const PopupLayout l = LayoutPopup(Sample(), m, shape, false, Fake);
        const auto cards    = Of(l, PopupPiece::Kind::Card);
        CHECK(cards.size() == 2 && Of(l, PopupPiece::Kind::Rule).empty());
        // Elements' heading and grid inside its card, clear of its edges; the Immune icon inside the next.
        for (const PopupPiece& p : l.pieces)
        {
            if (p.kind == PopupPiece::Kind::Card || p.font == PopupFont::Title || p.line == 0) continue;
            const PopupPiece& card = *cards[p.line - 1];
            CHECK(p.x > card.x && p.x + p.width < card.x + card.width && p.y > card.y &&
                  p.y + p.height < card.y + card.height);
        }
        // Cards one under the other, the full width inside the padding.
        CHECK(Near(cards[0]->x, m.popupPadding) && Near(cards[0]->width, l.width - 2.0f * m.popupPadding));
        CHECK(cards[1]->y > cards[0]->y + cards[0]->height);
        bool upper = false;
        for (const PopupPiece& p : l.pieces)
            upper |= p.font == PopupFont::Heading && p.span.text == "ELEMENTS";
        CHECK(upper == (shape == BarShape::Pills));
    }
}

TEST(a_starred_cell_has_a_star_after_its_text_from_the_top_of_the_texts_ink)
{
    const BarMetrics m = MetricsFor(36, BarShape::Pills);
    const auto laid = [&](bool starred) {
        PopupRow held = Labeled(Text("San d'Oria and its two allies"), Text("61%"));
        held.cells[0].star = starred;
        PopupContent c;
        c.title = {{"Zone", Tone::Normal}};
        c.sections.push_back(PopupSection{"Influence", {held, Labeled(Text("Bastok"), Text("21%"))}});
        return LayoutPopup(c, m, BarShape::Pills, false, Fake);
    };
    const PopupLayout l = laid(true), bare = laid(false);
    CHECK(Of(bare, PopupPiece::Kind::Star).empty());
    const std::vector<const PopupPiece*> stars = Of(l, PopupPiece::Kind::Star);
    CHECK_EQ(stars.size(), 1u);
    if (stars.empty()) return;
    const PopupPiece& star = *stars.front(), &name = *Find(l, "San d'Oria and its two allies ");
    CHECK(Near(star.width, star.height) && star.width > 0.5f * m.popupText && star.width <= m.popupText);
    const float gap = star.x - (name.x + name.width);
    CHECK(gap > 0.0f && gap < m.popupText);
    CHECK(Near(star.y, name.y + 0.3f * 14.0f)); // Fake's ink starts 30% down its 14 px line
    CHECK(star.line == name.line && star.x + star.width < Find(l, "61%")->x);
    CHECK(Near(l.width - bare.width, gap + star.width)); // the widest row is as much wider
    CHECK(Near(Find(l, "61%")->x + Find(l, "61%")->width, Find(l, "21%")->x + Find(l, "21%")->width));
}

namespace
{
    // Travel's tabs: two airship routes, and boats with a long route name and more of them.
    PopupSection Travel(size_t picked)
    {
        PopupSection s{"Travel", {}};
        PopupTab airships{Text("Airships"), {}};
        for (const char* route : {"Jeuno to Bastok", "Bastok to Jeuno"})
            airships.rows.push_back(Labeled(route, Text("Boards in 1:24")));
        PopupTab boats{Text("Boats"), {}};
        for (const char* route : {"Selbina to Mhaura, the long way round", "Mhaura to Selbina", "Whitegate to Nashmau",
                 "Nashmau to Whitegate"})
            boats.rows.push_back(Labeled(route, Text("Boards in 2:23")));
        s.tabs = {airships, boats};
        s.tab  = picked;
        return s;
    }
}

TEST(a_card_with_tabs_shows_the_picked_ones_rows_under_its_tabs_at_one_width)
{
    const BarMetrics m = MetricsFor(36, BarShape::Pills);
    std::vector<float> widths, heights;
    for (const size_t picked : {0u, 1u})
    {
        PopupContent c;
        c.title = {{"World Info", Tone::Normal}};
        c.sections.push_back(Travel(picked));
        const PopupLayout l = LayoutPopup(c, m, BarShape::Pills, false, Fake);
        CHECK(AllInside(l, m));
        const std::vector<const PopupPiece*> tabs = Of(l, PopupPiece::Kind::Tab);
        CHECK_EQ(tabs.size(), static_cast<size_t>(2));
        CHECK(tabs[picked]->open && !tabs[1 - picked]->open);
        // Each tab is a place to click, by the card's heading and the tab's name, over its frame.
        CHECK_EQ(l.chips.size(), static_cast<size_t>(2));
        for (size_t i = 0; i < 2; ++i)
            CHECK(l.chips[i].key == "Travel" && l.chips[i].tab == (i == 0 ? "Airships" : "Boats") &&
                  Near(l.chips[i].x, tabs[i]->x) && Near(l.chips[i].y, tabs[i]->y) && Near(l.chips[i].width, tabs[i]->width));
        // A cut that drops the card drops its tabs.
        PopupLayout cut = l;
        CutPopup(cut, Of(l, PopupPiece::Kind::Card).front()->y - 0.5f, m.popupPadding);
        CHECK(cut.chips.empty());
        // Only the picked tab's rows, under the tabs.
        CHECK((Find(l, "Jeuno to Bastok ") != nullptr) == (picked == 0));
        CHECK((Find(l, "Mhaura to Selbina ") != nullptr) == (picked == 1));
        const PopupPiece& row = *Find(l, picked == 0 ? "Jeuno to Bastok " : "Mhaura to Selbina ");
        CHECK(row.y > tabs[0]->y + tabs[0]->height);
        widths.push_back(Of(l, PopupPiece::Kind::Card).front()->width);
        heights.push_back(l.height);
    }
    // As wide for either tab, the widest; as tall as the picked one's rows.
    CHECK(Near(widths[0], widths[1]) && heights[1] > heights[0]);
}

TEST(cards_keep_their_columns_whichever_tab_is_picked)
{
    const BarMetrics m = MetricsFor(36, BarShape::Pills);
    std::vector<std::vector<float>> places;
    for (const size_t picked : {0u, 1u})
    {
        PopupContent c;
        c.title = {{"World Info", Tone::Normal}};
        // Short airships and many boats: by their heights alone, the two tabs would split the cards differently.
        PopupSection travel = Travel(picked);
        for (int i = 0; i < 12; ++i)
            travel.tabs[1].rows.push_back(Labeled("Boat " + std::to_string(i), Text("Boards in 2:23")));
        c.sections.push_back(travel);
        for (const char* heading : {"One", "Two", "Three"})
        {
            PopupSection card{heading, {}};
            for (int i = 0; i < 5; ++i)
                card.rows.push_back(Labeled("Row " + std::to_string(i), Text("in 1:00")));
            c.sections.push_back(card);
        }
        std::vector<float> place; // each card's left: its column
        for (const PopupPiece* card : Of(LayoutPopup(c, m, BarShape::Pills, false, Fake), PopupPiece::Kind::Card))
            place.push_back(card->x);
        places.push_back(place);
    }
    // The longer tab grows the card down where it is: the cards stay in their columns.
    CHECK(places[0].size() == 4 && places[0] == places[1]);
}

namespace
{
    // A tab's rows: a count, then a table of 30 zones under its heads.
    PopupContent Zones(size_t scrolled)
    {
        PopupContent c;
        c.title = {{"Weather", Tone::Normal}};
        PopupSection s{"By Element", {}};
        PopupTab dark{Text("Dark"), {}};
        PopupRow count = Labeled("Zones", Text("30 today"));
        count.nested   = true;
        count.scroll   = "By Element/Dark";
        count.scrolled = scrolled;
        PopupRow grid{RowKind::Grid, 2, {Word("Zone"), Word("Today")}};
        grid.header = true;
        grid.nested = true;
        for (int i = 0; i < 30; ++i)
        {
            grid.cells.push_back(Word(("Zone " + std::to_string(i)).c_str()));
            grid.cells.push_back(Word("Gloom 50%"));
        }
        dark.rows = {count, grid};
        s.tabs    = {dark};
        c.sections.push_back(s);
        return c;
    }
}

TEST(a_socket_longer_than_ten_lines_shows_ten_and_scrolls_under_its_heads)
{
    const BarMetrics m = MetricsFor(36, BarShape::Pills);
    const PopupLayout top = LayoutPopup(Zones(0), m, BarShape::Pills, false, Fake);
    // The count and nine zones make ten lines; the table's heads stay.
    CHECK(Find(top, "Zones ") && Find(top, "Zone") && Find(top, "Zone 8") && !Find(top, "Zone 9"));
    CHECK_EQ(top.scrolls.size(), static_cast<size_t>(1));
    const PopupScroll& area = top.scrolls.front();
    const PopupPiece& socket = *Of(top, PopupPiece::Kind::Socket).front();
    CHECK(area.key == "By Element/Dark" && area.most == 21 && Near(area.x, socket.x) && Near(area.y, socket.y) &&
          Near(area.width, socket.width) && Near(area.height, socket.height));
    // Scrolled five lines: the heads stay, the count and the first four zones are gone, as tall as before.
    const PopupLayout down = LayoutPopup(Zones(5), m, BarShape::Pills, false, Fake);
    CHECK(Find(down, "Zone") && !Find(down, "Zones ") && !Find(down, "Zone 3") && Find(down, "Zone 4") &&
          Find(down, "Zone 13") && !Find(down, "Zone 14"));
    CHECK(Near(down.height, top.height) && Near(down.width, top.width));
    CHECK(Find(down, "Zone")->y < Find(down, "Zone 4")->y);
    // Past the end, it stops at the last ten.
    const PopupLayout end = LayoutPopup(Zones(100), m, BarShape::Pills, false, Fake);
    CHECK(Find(end, "Zone 29") && Find(end, "Zone 20") && !Find(end, "Zone 19"));
}

TEST(a_scrolling_socket_has_a_scrollbar_in_its_right_padding_showing_where_it_is)
{
    const BarMetrics m = MetricsFor(36, BarShape::Pills);
    std::vector<std::pair<float, float>> thumbs; // its top and bottom, from the track's top, as a share of the track
    for (const size_t scrolled : {0u, 5u, 100u})
    {
        const PopupLayout l = LayoutPopup(Zones(scrolled), m, BarShape::Pills, false, Fake);
        const std::vector<const PopupPiece*> tracks = Of(l, PopupPiece::Kind::ScrollTrack), bars = Of(l, PopupPiece::Kind::ScrollThumb);
        CHECK(tracks.size() == 1 && bars.size() == 1);
        const PopupPiece& track = *tracks.front(), &thumb = *bars.front();
        const PopupPiece& sock  = *Of(l, PopupPiece::Kind::Socket).front();
        const PopupPiece& name  = *Find(l, "Zone"); // the table's heads, which stay
        // In the socket's right padding, clear of its rows, from its first row's top to its last's bottom.
        if (const PopupPiece* value = Find(l, "30 today")) CHECK(track.x >= value->x + value->width);
        CHECK(track.x + track.width <= sock.x + sock.width);
        CHECK(Near(track.x + track.width / 2.0f, sock.x + sock.width - (name.x - sock.x) / 2.0f));
        CHECK(track.y > sock.y && track.y + track.height < sock.y + sock.height);
        // The thumb in the track, as long as the share of lines shown.
        CHECK(Near(thumb.x, track.x) && Near(thumb.width, track.width));
        CHECK(thumb.y >= track.y - 0.01f && thumb.y + thumb.height <= track.y + track.height + 0.01f);
        CHECK(Near(thumb.height, track.height * 10.0f / 31.0f));
        // Its grip, to drag, is the socket's right padding down the track; the thumb's length goes with it.
        const PopupScroll& area = l.scrolls.front();
        CHECK(Near(area.grip.x, sock.x + sock.width - (name.x - sock.x)) && Near(area.grip.x + area.grip.width, sock.x + sock.width));
        CHECK(Near(area.grip.y, track.y) && Near(area.grip.height, track.height) && Near(area.thumb, thumb.height));
        thumbs.emplace_back((thumb.y - track.y) / track.height, (thumb.y + thumb.height - track.y) / track.height);
    }
    // At the top, five lines down of 21, and at the end.
    CHECK(Near(thumbs[0].first, 0.0f) && Near(thumbs[2].second, 1.0f));
    CHECK(Near(thumbs[1].first, (1.0f - 10.0f / 31.0f) * 5.0f / 21.0f));
    // A socket that shows all its lines has none.
    PopupContent fits;
    fits.title = {{"World Info", Tone::Normal}};
    fits.sections.push_back(Travel(0));
    CHECK(Of(LayoutPopup(fits, m, BarShape::Pills, false, Fake), PopupPiece::Kind::ScrollTrack).empty());
}

TEST(a_sockets_rows_keep_as_much_room_on_their_right_as_on_their_left)
{
    const BarMetrics m = MetricsFor(36, BarShape::Pills);
    const PopupLayout l    = LayoutPopup(Zones(0), m, BarShape::Pills, false, Fake);
    const PopupPiece& sock = *Of(l, PopupPiece::Kind::Socket).front();
    const PopupPiece& name = *Find(l, "Zones "), &value = *Find(l, "30 today");
    CHECK(Near(sock.x + sock.width - (value.x + value.width), name.x - sock.x));
    // And the widest line of the table fits inside the same room.
    for (const PopupPiece& p : l.pieces)
        if (p.kind == PopupPiece::Kind::Text && p.y > sock.y && p.y < sock.y + sock.height)
            CHECK(p.x + p.width <= sock.x + sock.width - (name.x - sock.x) + 0.01f);
}

TEST(a_pairs_value_ends_at_its_cards_right_edge_and_its_label_at_the_left)
{
    const BarMetrics m = MetricsFor(36, BarShape::Pills);
    PopupContent c;
    c.title = {{"Gil", Tone::Normal}};
    c.sections.push_back(PopupSection{"Currencies", {Labeled("Gil", Text("4,751")),
                                                        Labeled("Imperial Standing", Text("12"))}});
    const PopupLayout l    = LayoutPopup(c, m, BarShape::Pills, false, Fake);
    const PopupPiece& card = *Of(l, PopupPiece::Kind::Card).front();
    const PopupPiece& gil = *Find(l, "4,751"), &standing = *Find(l, "12");
    CHECK(Near(gil.x + gil.width, standing.x + standing.width)); // the values line up at their right
    CHECK(gil.x + gil.width < card.x + card.width && gil.x + gil.width > card.x + card.width - m.popupText);
    CHECK(Near(Find(l, "Gil ")->x, Find(l, "Imperial Standing ")->x)); // the labels at their left
    CHECK(standing.x > Find(l, "Imperial Standing ")->x + Find(l, "Imperial Standing ")->width);
    // A value is drawn against the right of its box, so values that tick stay lined up at their right edge.
    CHECK(gil.right && standing.right && !Find(l, "Gil ")->right);
}

TEST(a_tall_popup_flows_its_cards_into_two_columns)
{
    const BarMetrics m = MetricsFor(36, BarShape::Pills);
    PopupContent c;
    c.title = {{"Mob", Tone::Normal}};
    for (const char* heading : {"One", "Two", "Three", "Four"})
    {
        PopupSection s{heading, {}};
        for (int i = 0; i < 4; ++i)
            s.rows.push_back(PopupRow{RowKind::Line, 0, {Word("row")}});
        c.sections.push_back(s);
    }
    const PopupLayout l = LayoutPopup(c, m, BarShape::Pills, false, Fake);
    const auto cards    = Of(l, PopupPiece::Kind::Card);
    CHECK(cards.size() == 4);
    // Two and two, in order down the left then the right, the right column level with the left.
    CHECK(Near(cards[0]->x, cards[1]->x) && Near(cards[2]->x, cards[3]->x) && cards[2]->x > cards[0]->x);
    CHECK(Near(cards[0]->y, cards[2]->y) && cards[1]->y > cards[0]->y);
    CHECK(cards[2]->x >= cards[0]->x + cards[0]->width);
    CHECK(Near(cards[3]->x + cards[3]->width, l.width - m.popupPadding));
    // A short popup keeps one column.
    c.sections.resize(2);
    c.sections[0].rows.resize(1);
    c.sections[1].rows.resize(1);
    const PopupLayout shortOne = LayoutPopup(c, m, BarShape::Pills, false, Fake);
    CHECK(Near(Of(shortOne, PopupPiece::Kind::Card)[0]->x, Of(shortOne, PopupPiece::Kind::Card)[1]->x));
}

TEST(a_table_taller_than_the_room_sets_its_lines_side_by_side_in_its_card_and_drops_nothing)
{
    const BarMetrics m = MetricsFor(36, BarShape::Pills);
    PopupContent c;
    c.title = {{"Weather", Tone::Normal}};
    c.sections.push_back(PopupSection{"Forecast", {PopupRow{RowKind::Line, 0, {Word("Today")}}}});
    PopupRow grid{RowKind::Grid, 2, {Word("Zone"), Word("Today")}};
    grid.header = true;
    for (int i = 0; i < 60; ++i)
    {
        grid.cells.push_back(Word(("Zone " + std::to_string(i)).c_str()));
        grid.cells.push_back(Word(i % 3 == 0 ? "Snow 50% wide" : "-"));
    }
    c.sections.push_back(PopupSection{"By Element", {grid}});
    const PopupLayout whole = LayoutPopup(c, m, BarShape::Pills, false, Fake);
    const float room        = whole.height / 3.0f;
    const PopupLayout l     = LayoutPopup(c, m, BarShape::Pills, false, Fake, room);
    CHECK(l.height <= room + 0.01f && l.width > whole.width);
    CHECK(AllInside(l, m));
    // Still its own card, once, wider: its lines in groups side by side, each under the table's heads, in order down
    // each group before the next.
    CHECK_EQ(Of(l, PopupPiece::Kind::Card).size(), static_cast<size_t>(2));
    size_t headings = 0, heads = 0;
    for (const PopupPiece& p : l.pieces)
    {
        headings += p.span.text == "BY ELEMENT";
        heads += p.span.text == "Zone";
    }
    CHECK(headings == 1 && heads >= 2);
    std::map<int, std::set<int>> chanceX; // by the zones' x, in whole pixels
    for (int i = 0; i < 60; ++i)
    {
        const PopupPiece* zone = Find(l, "Zone " + std::to_string(i));
        CHECK(zone != nullptr);
        const PopupPiece* chance = nullptr;
        for (const PopupPiece& p : l.pieces)
            if (&p != zone && Near(p.y, zone->y) && p.x > zone->x && (chance == nullptr || p.x < chance->x)) chance = &p;
        CHECK(chance != nullptr && chance->span.text == (i % 3 == 0 ? "Snow 50% wide" : "-"));
        chanceX[static_cast<int>(std::lround(zone->x))].insert(static_cast<int>(std::lround(chance->x)));
        if (i > 0)
        {
            const PopupPiece* before = Find(l, "Zone " + std::to_string(i - 1));
            CHECK(Near(before->x, zone->x) ? before->y < zone->y : before->x < zone->x);
        }
    }
    CHECK(chanceX.size() >= 2);
    for (const auto& [x, at] : chanceX)
        CHECK_EQ(at.size(), static_cast<size_t>(1));
    // With room for it whole, it stays one group.
    const PopupLayout roomy = LayoutPopup(c, m, BarShape::Pills, false, Fake, 10000.0f);
    CHECK(Near(roomy.width, whole.width) && Near(roomy.height, whole.height));
}

TEST(a_card_too_tall_for_the_room_without_a_table_goes_on_in_another_column)
{
    const BarMetrics m = MetricsFor(36, BarShape::Pills);
    PopupContent c;
    c.title = {{"Gil", Tone::Normal}};
    PopupSection currencies{"Currencies", {}};
    for (int i = 0; i < 40; ++i)
        currencies.rows.push_back(PopupRow{RowKind::Line, 0, {Word(("Currency " + std::to_string(i)).c_str())}});
    c.sections.push_back(currencies);
    const float room    = LayoutPopup(c, m, BarShape::Pills, false, Fake).height / 2.5f;
    const PopupLayout l = LayoutPopup(c, m, BarShape::Pills, false, Fake, room);
    CHECK(l.height <= room + 0.01f && Of(l, PopupPiece::Kind::Card).size() >= 3);
    for (int i = 0; i < 40; ++i)
        CHECK(Find(l, "Currency " + std::to_string(i)) != nullptr);
}

TEST(immune_icons_get_their_mark_and_pinned_shows)
{
    const BarMetrics m  = MetricsFor(36, BarShape::Pills);
    const PopupLayout l = LayoutPopup(Sample(), m, BarShape::Pills, true, Fake);
    CHECK(Of(l, PopupPiece::Kind::ImmuneMark).size() == 1);
    bool pinned = false;
    for (const PopupPiece& p : l.pieces)
        pinned |= p.kind == PopupPiece::Kind::Text && p.span.text == "pinned" && p.span.tone == Tone::Heading &&
                  p.font == PopupFont::Pin && Near(p.x + p.width, l.width - m.popupPadding);
    CHECK(pinned);
}

TEST(an_unpinned_popup_says_nothing_of_it)
{
    const PopupLayout l = LayoutPopup(Sample(), MetricsFor(36, BarShape::Ornate), BarShape::Ornate, false, Fake);
    for (const PopupPiece& p : l.pieces)
        if (p.kind == PopupPiece::Kind::Text) CHECK(p.span.text != "pinned");
}

TEST(an_empty_popup_is_just_its_padding_and_minimum_width)
{
    const BarMetrics m  = MetricsFor(36, BarShape::Pills);
    const PopupLayout l = LayoutPopup(PopupContent{}, m, BarShape::Pills, false, Fake);
    CHECK(l.pieces.empty());
    CHECK(Near(l.width, m.popupText * 12.0f + 2.0f * m.popupPadding));
    CHECK(Near(l.height, 2.0f * m.popupPadding));
}

TEST(rows_without_cells_add_no_pieces_and_a_grid_without_cells_does_not_divide_by_zero)
{
    const BarMetrics m = MetricsFor(36, BarShape::Ornate);
    PopupContent c     = OneRow(PopupRow{RowKind::Grid, 3, {}});
    c.sections[0].rows.push_back(PopupRow{RowKind::Line, 0, {}});
    c.sections[0].rows.push_back(PopupRow{RowKind::Tags, 0, {}});
    c.sections[0].rows.push_back(PopupRow{RowKind::Grid, 0, {}});
    const PopupLayout l = LayoutPopup(c, m, BarShape::Ornate, false, Fake);
    CHECK_EQ(l.pieces.size(), static_cast<size_t>(1)); // the title
    CHECK(AllInside(l, m));
}

TEST(a_grid_without_columns_is_one_line_of_cells)
{
    const BarMetrics m = MetricsFor(36, BarShape::Ornate);
    for (const int columns : {0, -2})
    {
        const PopupLayout l =
            LayoutPopup(OneRow(PopupRow{RowKind::Grid, columns, {Word("aa"), Word("bbbb"), Word("c")}}), m,
                        BarShape::Ornate, false, Fake);
        std::vector<const PopupPiece*> words;
        for (const PopupPiece& p : l.pieces)
            if (p.font == PopupFont::Text) words.push_back(&p);
        CHECK_EQ(words.size(), static_cast<size_t>(3));
        CHECK(Near(words[0]->y, words[1]->y) && Near(words[1]->y, words[2]->y));
        CHECK(words[1]->x > words[0]->x + words[0]->width && words[2]->x > words[1]->x + words[1]->width);
    }
}

TEST(a_grid_wider_than_its_cells_stays_on_one_line_and_a_short_last_line_keeps_the_columns)
{
    const BarMetrics m = MetricsFor(36, BarShape::Ornate);
    const PopupLayout wide =
        LayoutPopup(OneRow(PopupRow{RowKind::Grid, 4, {Word("aa"), Word("bb")}}), m, BarShape::Ornate, false, Fake);
    CHECK(Near(Of(wide, PopupPiece::Kind::Text)[1]->y, Of(wide, PopupPiece::Kind::Text)[2]->y));

    const PopupLayout wrapped = LayoutPopup(
        OneRow(PopupRow{RowKind::Grid, 2, {Word("a"), Word("bbb"), Word("cc"), Word("d"), Word("eeeee")}}), m,
        BarShape::Ornate, false, Fake);
    const std::vector<const PopupPiece*> t = Of(wrapped, PopupPiece::Kind::Text); // [0] is the title
    CHECK_EQ(t.size(), static_cast<size_t>(6));
    CHECK(Near(t[1]->x, t[3]->x) && Near(t[3]->x, t[5]->x));
    CHECK(Near(t[2]->x, t[4]->x));
    CHECK(t[3]->y > t[1]->y && t[5]->y > t[3]->y && Near(t[1]->y, t[2]->y));
    CHECK(AllInside(wrapped, m));
}

TEST(each_tag_gets_a_frame_around_its_text)
{
    const BarMetrics m  = MetricsFor(36, BarShape::Pills);
    const PopupLayout l = LayoutPopup(OneRow(PopupRow{RowKind::Tags, 0, {Word("Fire"), Word("Ice")}}), m,
                                      BarShape::Pills, false, Fake);
    const std::vector<const PopupPiece*> tags = Of(l, PopupPiece::Kind::Tag);
    CHECK_EQ(tags.size(), static_cast<size_t>(2));
    const std::vector<const PopupPiece*> texts = Of(l, PopupPiece::Kind::Text); // [0] is the title
    for (size_t i = 0; i < 2; ++i)
    {
        const PopupPiece& tag = *tags[i];
        const PopupPiece& txt = *texts[i + 1];
        const TextSize ink    = Fake(txt.span, txt.font);
        CHECK(txt.x > tag.x && txt.x + txt.width < tag.x + tag.width);
        // Centered by its ink: as much room above its capitals as below its descenders.
        const float above = txt.y + ink.inkTop - tag.y, below = tag.y + tag.height - (txt.y + ink.inkBottom);
        CHECK(above > 0.0f && Near(above, below));
    }
    CHECK(tags[1]->x > tags[0]->x + tags[0]->width);
    CHECK(AllInside(l, m));
}

TEST(tags_and_immune_marks_keep_inside_their_frames_and_rows_at_every_bar_height)
{
    PopupContent c = Sample();
    c.sections.front().rows.push_back(PopupRow{RowKind::Tags, 0, {Word("Aggressive"), Word("NM")}});
    PopupRow immune{RowKind::Line, 0, {PopupCell{Icon::Bind, true, {}}, PopupCell{Icon::Sleep, true, {}}}};
    c.sections.push_back(PopupSection{"Spawn", {immune}});
    for (const int h : {24, 36, 64})
        for (const BarShape shape : {BarShape::Pills, BarShape::Ornate})
        {
            const BarMetrics m  = MetricsFor(h, shape);
            const PopupLayout l = LayoutPopup(c, m, shape, true, Fake);
            CHECK(Of(l, PopupPiece::Kind::ImmuneMark).size() == 3 && Of(l, PopupPiece::Kind::Tag).size() == 2);
            CHECK(Of(l, PopupPiece::Kind::Card).size() == 3);
            CHECK(AllInside(l, m));
        }
}

TEST(a_heading_wider_than_the_rows_widens_its_card_and_the_popup)
{
    const BarMetrics m = MetricsFor(36, BarShape::Ornate);
    PopupContent c     = OneRow(PopupRow{RowKind::Line, 0, {Word("short")}});
    c.sections[0].heading   = std::string(60, 'h');
    const PopupLayout l     = LayoutPopup(c, m, BarShape::Ornate, false, Fake);
    const PopupPiece& card  = *Of(l, PopupPiece::Kind::Card).front();
    const PopupPiece& title = *Find(l, std::string(60, 'h'));
    CHECK(card.width > title.width + 2.0f * (title.x - card.x) - 0.01f);
    CHECK(Near(l.width, card.width + 2.0f * m.popupPadding));
    CHECK(AllInside(l, m));
}

TEST(a_section_without_a_heading_has_no_rule)
{
    const BarMetrics m  = MetricsFor(36, BarShape::Ornate);
    const PopupLayout l =
        LayoutPopup(OneRow(PopupRow{RowKind::Line, 0, {Word("x")}}), m, BarShape::Ornate, false, Fake);
    CHECK(Of(l, PopupPiece::Kind::Rule).empty());
}

TEST(a_pinned_popup_with_a_long_title_keeps_the_mark_clear_of_the_title)
{
    const BarMetrics m = MetricsFor(36, BarShape::Ornate);
    PopupContent c;
    c.icon  = Icon::AggroNQ;
    c.title = {{"A very long mob name that is wider than the minimum", Tone::Normal}};
    const PopupLayout l = LayoutPopup(c, m, BarShape::Ornate, true, Fake);
    const PopupPiece& title = l.pieces[1];
    const PopupPiece& pin   = l.pieces[2];
    CHECK(pin.span.text == "pinned");
    CHECK(pin.x > title.x + title.width);
    CHECK(AllInside(l, m));
}

namespace
{
    // A title with the pin mark, an untitled section with a line and tags, a two-line grid, immune icons and a
    // three-line section: every kind of piece, and headings with rows of each kind under them.
    PopupContent Tall()
    {
        PopupContent c = Sample();
        c.sections.insert(c.sections.begin(),
            PopupSection{"", {PopupRow{RowKind::Line, 0, {Word("ID 220")}},
                                 PopupRow{RowKind::Tags, 0, {Word("Aggressive"), Word("Links"), Word("NM")}}}});
        c.sections.push_back(PopupSection{"Spawn", {PopupRow{RowKind::Line, 0, {Word("Respawn 5:00")}},
                                                       PopupRow{RowKind::Line, 0, {Word("Placeholder for Billy")}},
                                                       PopupRow{RowKind::Line, 0, {Word("Back in 4:59")}}}});
        return c;
    }

    bool Inside(float x, float y, const PopupPiece& frame)
    {
        return x >= frame.x && x < frame.x + frame.width && y >= frame.y && y < frame.y + frame.height;
    }

    bool SamePiece(const PopupPiece& a, const PopupPiece& b)
    {
        return a.kind == b.kind && a.x == b.x && a.y == b.y && a.width == b.width && a.span.text == b.span.text;
    }

    size_t Count(const PopupLayout& l, const PopupPiece& piece)
    {
        size_t n = 0;
        for (const PopupPiece& p : l.pieces)
            n += SamePiece(p, piece);
        return n;
    }

    bool IsHeading(const PopupPiece& p)
    {
        return p.kind == PopupPiece::Kind::Text && p.font == PopupFont::Heading;
    }

    // What a cut keeps of full belongs together: a tag and the text inside it, an icon and the immune mark on it, a
    // heading and its rule (at its middle, after it) go or stay together, and each heading kept has a row under it.
    bool Whole(const PopupLayout& cut, const PopupLayout& full)
    {
        for (const PopupPiece& a : full.pieces)
            for (const PopupPiece& b : full.pieces)
            {
                const bool tagText = a.kind == PopupPiece::Kind::Tag && b.kind == PopupPiece::Kind::Text &&
                                     Inside(b.x + b.width / 2.0f, b.y + b.height / 2.0f, a);
                const bool mark = a.kind == PopupPiece::Kind::Icon && b.kind == PopupPiece::Kind::ImmuneMark &&
                                  Inside(b.x, b.y, a);
                const bool rule = IsHeading(a) && b.kind == PopupPiece::Kind::Rule && b.x > a.x + a.width &&
                                  b.y > a.y && b.y < a.y + a.height;
                if ((tagText || mark || rule) && Count(cut, a) != Count(cut, b)) return false;
            }
        for (const PopupPiece& heading : cut.pieces)
        {
            if (!IsHeading(heading)) continue;
            bool under = false;
            for (const PopupPiece& p : cut.pieces)
                under |= p.kind != PopupPiece::Kind::Rule && p.y >= heading.y + heading.height;
            if (!under) return false;
        }
        return true;
    }

    float Bottom(const PopupPiece& p) { return p.y + p.height; }

    // The title's line: its icon and the name beside it.
    float TitleBottom(const PopupLayout& l)
    {
        return std::max(Bottom(*Of(l, PopupPiece::Kind::Icon).front()), Bottom(*Find(l, "Goblin")));
    }
}

TEST(a_popup_that_fits_is_not_cut)
{
    const BarMetrics m  = MetricsFor(36, BarShape::Ornate);
    const PopupLayout l = LayoutPopup(Tall(), m, BarShape::Ornate, true, Fake);
    for (const float room : {l.height, l.height + 100.0f})
    {
        PopupLayout cut = l;
        CutPopup(cut, room, m.popupPadding);
        CHECK(Near(cut.height, l.height) && Near(cut.width, l.width) && cut.pieces.size() == l.pieces.size());
        for (size_t i = 0; i < l.pieces.size(); ++i)
            CHECK(SamePiece(cut.pieces[i], l.pieces[i]));
    }
}

// The screen's bottom cuts a tall popup: it keeps the title and whole cards, padding clear under the lowest, and
// shrinks to them; a card it drops goes with everything on it.
TEST(a_cut_popup_keeps_whole_cards_with_its_padding_under_them)
{
    for (const int h : {24, 36, 64})
        for (const BarShape shape : {BarShape::Pills, BarShape::Ornate})
        {
            const BarMetrics m  = MetricsFor(h, shape);
            const PopupLayout l = LayoutPopup(Tall(), m, shape, true, Fake);
            for (float room = TitleBottom(l) + m.popupPadding; room < l.height; room += 0.5f)
            {
                PopupLayout cut = l;
                CutPopup(cut, room, m.popupPadding);
                CHECK(cut.height <= room + 0.01f && cut.height < l.height && Near(cut.width, l.width));
                CHECK(!cut.pieces.empty() && cut.pieces.size() < l.pieces.size());
                for (const PopupPiece& p : cut.pieces)
                    CHECK(Bottom(p) <= cut.height - m.popupPadding + 0.01f && Count(l, p) == 1);
                // Each line is kept whole or dropped whole.
                for (size_t line = 0; line < l.lines.size(); ++line)
                {
                    size_t all = 0, kept = 0;
                    for (const PopupPiece& p : l.pieces)
                        all += p.line == line;
                    for (const PopupPiece& p : cut.pieces)
                        kept += p.line == line;
                    CHECK(kept == 0 || kept == all);
                    CHECK(line == 0 || (kept == all) == (l.lines[line].bottom <= room - m.popupPadding));
                }
                CHECK(Whole(cut, l));
            }
        }
}

TEST(a_cut_through_a_card_drops_the_card_whole)
{
    const BarMetrics m      = MetricsFor(36, BarShape::Ornate);
    const PopupLayout l     = LayoutPopup(Tall(), m, BarShape::Ornate, false, Fake);
    const PopupPiece& spawn = *Find(l, "Spawn");
    PopupLayout cut         = l;
    CutPopup(cut, spawn.y + spawn.height + m.popupPadding + 0.5f, m.popupPadding);
    CHECK(Find(cut, "Spawn") == nullptr && Find(cut, "Respawn 5:00") == nullptr);
    CHECK(Of(cut, PopupPiece::Kind::Card).size() == Of(l, PopupPiece::Kind::Card).size() - 1);
    CHECK(Whole(cut, l));
}

TEST(a_cut_above_the_first_row_keeps_the_title_alone)
{
    const BarMetrics m     = MetricsFor(36, BarShape::Ornate);
    const PopupLayout l    = LayoutPopup(Tall(), m, BarShape::Ornate, true, Fake);
    PopupLayout cut        = l;
    CutPopup(cut, Bottom(*Find(l, "ID 220")) + m.popupPadding - 0.5f, m.popupPadding);
    CHECK_EQ(cut.pieces.size(), static_cast<size_t>(3)); // the icon, the name and the pin mark
    CHECK(Find(cut, "Goblin") != nullptr && Find(cut, "pinned") != nullptr);
    CHECK(Near(cut.height, TitleBottom(l) + m.popupPadding));
}
