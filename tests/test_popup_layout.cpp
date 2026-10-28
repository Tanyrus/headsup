#include "popup_layout.h"
#include "test.h"

#include <algorithm>
#include <cmath>
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

TEST(a_fold_row_has_its_arrow_and_its_whole_row_to_click_and_the_rows_it_holds_sit_under_its_label)
{
    const BarMetrics m = MetricsFor(36, BarShape::Pills);
    PopupContent c;
    c.title = {{"World Info", Tone::Normal}};
    PopupRow airships = Labeled("Airships", Text("Boards in 1:24"));
    airships.fold     = "Airships";
    airships.open     = true;
    PopupRow route    = Labeled("Jeuno to Bastok", Text("Boards in 1:24"));
    route.nested      = true;
    PopupRow boats    = Labeled("Boats", Text("Boards in 2:23"));
    boats.fold        = "Boats";
    c.sections.push_back(PopupSection{"Travel", {airships, route, boats}});
    const PopupLayout l = LayoutPopup(c, m, BarShape::Pills, false, Fake);
    const std::vector<const PopupPiece*> arrows = Of(l, PopupPiece::Kind::Fold);
    CHECK_EQ(arrows.size(), static_cast<size_t>(2));
    CHECK(arrows[0]->open && !arrows[1]->open);
    CHECK(AllInside(l, m));
    const PopupPiece& card  = *Of(l, PopupPiece::Kind::Card).front();
    const PopupPiece& label = *Find(l, "Airships "), &nested = *Find(l, "Jeuno to Bastok "), &next = *Find(l, "Boats ");
    // The label after its arrow, the arrow centered on its line's ink, and the row it holds under the label.
    CHECK(label.x >= arrows[0]->x + arrows[0]->width);
    const TextSize t = Fake(label.span, PopupFont::Text);
    CHECK(Near(arrows[0]->y + arrows[0]->height / 2.0f, label.y + (t.inkTop + t.inkBottom) / 2.0f));
    CHECK(Near(nested.x, label.x) && Near(next.x, label.x));
    // Each fold's place is its row across the card, and holds neither its neighbour's label.
    CHECK_EQ(l.folds.size(), static_cast<size_t>(2));
    CHECK(l.folds[0].key == "Airships" && l.folds[1].key == "Boats");
    for (const PopupFold& fold : l.folds)
        CHECK(Near(fold.x, card.x) && Near(fold.width, card.width));
    const auto holds = [](const PopupFold& f, const PopupPiece& p) {
        return p.y >= f.y - 0.01f && p.y + p.height <= f.y + f.height + 0.01f;
    };
    CHECK(holds(l.folds[0], label) && !holds(l.folds[0], nested) && holds(l.folds[1], next));
    // A cut that drops the card drops its folds.
    PopupLayout cut = l;
    CutPopup(cut, card.y - 0.5f, m.popupPadding);
    CHECK(cut.folds.empty());
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
