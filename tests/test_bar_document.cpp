#include "bar_document.h"
#include "window_skin.h"
#include "test.h"

#include <cmath>
#include <string>

using namespace headsup;

namespace
{
    BarLook LookOf(const BarPalette& p, const Settings& settings)
    {
        const BarMetrics m  = MetricsFor(45, p.shape);
        return BarLook{p, settings, m, PillRow(m), TextRow{13, 11, true, 10.0f, 24.0f, 12.0f},
            BarTitleFont(settings, p.shape), p.shape == BarShape::Ornate, BarTextFont(settings), std::nullopt};
    }
    BarLook LookOf(BarTheme theme, const Settings& settings) { return LookOf(PaletteOf(theme), settings); }

    // Line boxes 16 and 14 px tall, the title's top at row 9 over a detail at row 23, or alone at row 14.
    BarTextPlace Place()
    {
        return BarTextPlace{BarLinePlace{16, 9.0f, 14.0f, 2.0f}, BarLinePlace{17, 8.0f, 13.0f, 1.0f},
            BarLinePlace{14, 23.0f, 0.0f, 3.0f},
            BarLinePlace{14, 15.0f, 15.0f, 3.0f}};
    }

    bool Has(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }
}

TEST(markup_text_is_escaped)
{
    CHECK_EQ(EscapedRml("Fish & <Chips> \"here\""), "Fish &amp; &lt;Chips&gt; &quot;here&quot;");
    CHECK_EQ(EscapedRml("plain"), "plain");
}

TEST(colors_are_written_with_their_alpha_last)
{
    CHECK_EQ(RcssColor(0xFFC9A24Au), "#c9a24aff");
    CHECK_EQ(RcssColor(0x1AC9A24Au), "#c9a24a1a");
    CHECK_EQ(RcssColor(0x00000000u), "#00000000");
}

TEST(image_names_round_trip_to_their_images)
{
    CHECK_EQ(IconSource(Icon::Link), "hu:icon/4");
    const BarPalette& umbra = PaletteOf(BarTheme::UmbraDark);
    const std::string umbraStrip = "hu:strip/ff2b2b30/ff212125/ff17171a/ff000000/00000000/80000000/1/45/13";
    CHECK_EQ(StripSource(umbra, 45, 13), umbraStrip);
    CHECK_EQ(DiamondSource(0xFFC9A24Au, 10), "hu:diamond/ffc9a24a/10");
    CHECK_EQ(CoinSource(17), "hu:coin/17");
    const std::optional<Image> icon = HostImage("hu:icon/4");
    CHECK(icon && icon->width == static_cast<int>(IconImage(Icon::Link).width));
    const std::optional<Image> strip = HostImage(umbraStrip);
    CHECK(strip && strip->width == 1 && strip->height == 45 + 13);
    CHECK(strip->argb == StripColumn(45, 13, umbra.stripTop, umbra.stripMiddle, umbra.stripBottom, umbra.bottomLine,
                             umbra.underLine, umbra.below, umbra.border).argb);
    BarPalette flat = umbra, one = umbra; // no gradient: its top all the way down, the other stops kept
    flat.gradient   = false;
    one.stripMiddle = one.stripBottom = umbra.stripTop;
    CHECK(StripSource(flat, 45, 13) == StripSource(one, 45, 13) && flat.stripBottom == umbra.stripBottom);
    BarPalette thick = umbra;
    thick.border     = 4;
    const std::optional<Image> bordered = HostImage(StripSource(thick, 45, 13));
    CHECK(bordered && bordered->argb[41] == umbra.bottomLine && bordered->argb[40] != umbra.bottomLine);
    const std::optional<Image> diamond = HostImage("hu:diamond/ffc9a24a/10");
    CHECK(diamond && diamond->width == 10 && diamond->height == 10);
    CHECK(diamond->argb[5 * 10 + 5] == 0xFFC9A24Au); // its middle pixel is solid in its color
    const std::optional<Image> coin = HostImage("hu:coin/17");
    CHECK(coin && coin->width == 17);
    CHECK(!HostImage("hu:icon/999") && !HostImage("hu:nothing") && !HostImage("icon.png") && !HostImage("hu:strip/9/45/13"));
    CHECK(!HostImage("hu:strip/ff2b2b30/ff212125/zz17171a/ff000000/00000000/80000000/1/45/13"));
    // The reset arrow, in the color asked for: the bottom of its ring is that color.
    const std::optional<Image> reset = HostImage(ResetSource(0xFF8C7270u, 32));
    CHECK(reset && reset->width == 32 && reset->height == 32);
    CHECK_EQ(reset->argb[(16 + 10) * 32 + 16], 0xFF8C7270u);
    CHECK(!HostImage("hu:reset/32"));
}

TEST(the_document_holds_the_strip_the_caps_the_dividers_and_every_placed_widget)
{
    Settings settings;
    const std::string rml = BarDocument(LookOf(BarTheme::PhoenixGold, settings), Place());
    CHECK(Has(rml, "<rml>") && Has(rml, "</rml>") && Has(rml, "<body class=\"ornate\">"));
    CHECK(Has(rml, StripSource(PaletteOf(BarTheme::PhoenixGold), 45, 13).c_str())); // 45 rows and 0.28 H below
    CHECK(Has(rml, "height:59px")); // those, and the gold theme's line under the strip
    CHECK(Has(rml, "id=\"w-zone\"") && Has(rml, "id=\"w-target\"") && Has(rml, "id=\"w-earthTime\""));
    CHECK(!Has(rml, "id=\"w-gameHelp\"")); // never on the bar
    // A divider between each two of the most widgets there can be: the target, and four copies of each of the rest.
    CHECK(Has(rml, "id=\"d-0\"") && Has(rml, ("id=\"d-" + std::to_string(kDividerCount - 1) + "\"").c_str()) &&
          !Has(rml, ("id=\"d-" + std::to_string(kDividerCount) + "\"").c_str()));
    CHECK_EQ(kDividerCount, 1 + (kBarWidgetCount - 2) * kMostCopies - 1); // game help is not on the bar
    CHECK(Has(rml, "id=\"w-gil-2\"") && Has(rml, "id=\"w-gil-4\"") && !Has(rml, "id=\"w-gil-5\""));
    CHECK(!Has(rml, "id=\"w-target-2\"")); // one target only
    CHECK(Has(rml, "class=\"widget hidden target\" id=\"w-target\""));
    CHECK(Has(rml, "font-size:13px") && Has(rml, "font-size:11px"));
    CHECK(Has(rml, "\"Marcellus SC\"") && Has(rml, "\"Barlow Condensed\""));
    CHECK(Has(rml, "hu:diamond/ffc9a24a/14")); // the end caps in the accent: 1.4 diamonds of 0.22 H, whole pixels
    CHECK(Has(rml, "hu:diamond/ff8a6d2c/10")); // the dividers in the divider color
    // The pill row is 36 px from row 4: content starts at row 5, so a title at row 9 sits 4 px under it, alone 9,
    // and the detail at row 23 comes 23 - (9 + 16) = -2 px after the title's line.
    CHECK(Has(rml, ".widget{position:absolute;top:4px;height:36px;"));
    CHECK(Has(rml, "line-height:16px;margin-top:4px;") && Has(rml, ".title.alone{margin-top:9px;}"));
    CHECK(Has(rml, "line-height:14px;margin-top:-2px;"));
    // After a title of figures, whose line is 17 px from row 8: 23 - 25 again.
    CHECK(Has(rml, ".detail.fig{margin-top:-2px;}"));
    // The target's frame: the accent around the hover tint, 3 px corners.
    CHECK(Has(rml, ".ornate .widget.target{border-width:1px;margin:0px;padding:0px 13.85px;"
                   "background-color:#c9a24a1a;border-color:#c9a24aff;border-radius:3px;}"));
    CHECK(Has(rml, ".ornate .widget.target.lit{background-color:#c9a24a34;")); // twice the tint
}

TEST(a_pill_theme_frames_each_widget_and_has_no_caps)
{
    Settings settings;
    const std::string rml = BarDocument(LookOf(BarTheme::UmbraDark, settings), Place());
    CHECK(Has(rml, "<body class=\"pills\">") && !Has(rml, "cap-right\" src"));
    // 0.17 H corners, the pill's fill and edge.
    CHECK(Has(rml, ".pills .widget{background-color:#222226ff;border-color:#3a3a42ff;border-radius:7.65px;}"));
    CHECK(Has(rml, ".pills .widget.flat{background-color:transparent;border-color:transparent;}"));
    CHECK(Has(rml, ".pills .widget.lit{background-color:#2a2a30ff;border-color:#5b8defff;"));
    CHECK(Has(rml, "class=\"widget hidden flat\" id=\"w-zone\"")); // zone and Earth time show no pill until lit
    CHECK(Has(rml, "class=\"widget hidden\" id=\"w-weather\""));
    CHECK(Has(rml, "hu:diamond/ff5b8def/10")); // a pill theme's dividers take the accent
    CHECK(Has(rml, "height:58px")); // no line under this strip
}

TEST(a_widgets_markup_has_its_icons_coin_lines_and_progress)
{
    Settings settings;
    const BarLook look = LookOf(BarTheme::PhoenixGold, settings);
    WidgetContent c;
    c.shown      = true;
    c.icons      = {Icon::Sight, Icon::Sound};
    c.coin       = true;
    c.title      = {Text("Rolanberry <Fields>"), Colored(" [S]", 0xFF6FE36Fu)};
    c.subtitle   = {Text("Sandy", Tone::Dim)};
    c.titleIcons = {Icon::Link};
    c.progress   = 0.25f;
    const std::string rml = WidgetRml(c, look);
    CHECK(Has(rml, "<img class=\"icon\" src=\"hu:icon/5\"/>") && Has(rml, "hu:icon/7"));
    CHECK(Has(rml, "class=\"coin\"") && Has(rml, "hu:coin/17")); // 0.75 of 0.50 H: 16.875
    CHECK(Has(rml, "Rolanberry &lt;Fields&gt;") && Has(rml, "#6fe36fff") && Has(rml, "#efe3c6ff"));
    CHECK(Has(rml, "<span class=\"line-icons\"><img class=\"line-icon\" src=\"hu:icon/4\"/></span>"));
    CHECK(Has(rml, "<div class=\"title\">") && Has(rml, "class=\"detail\"") && Has(rml, "#a8936dff") && Has(rml, "Sandy"));
    CHECK(Has(rml, "class=\"progress\"") && Has(rml, "width: 25%"));
    c.progressBeside = true;
    CHECK(Has(WidgetRml(c, look), "class=\"beside\"") && !Has(WidgetRml(c, look), "class=\"progress\""));
    c.progress.reset();
    c.subtitle.clear();
    c.coin = false;
    const std::string bare = WidgetRml(c, look);
    CHECK(!Has(bare, "progress") && !Has(bare, "detail") && !Has(bare, "coin"));
}

TEST(a_widget_of_figures_takes_the_detail_font_for_its_title)
{
    Settings settings;
    const BarLook look = LookOf(BarTheme::PhoenixGold, settings);
    WidgetContent c;
    c.shown   = true;
    c.title   = {Text("21:07")};
    c.figures = true;
    // No second line: the title alone.
    CHECK(Has(WidgetRml(c, look), "class=\"title figures alone\""));
    c.figures = false;
    CHECK(Has(WidgetRml(c, look), "class=\"title alone\""));
    c.figures  = true;
    c.subtitle = {Text("Earth", Tone::Dim)};
    CHECK(Has(WidgetRml(c, look), "class=\"title figures\"") && Has(WidgetRml(c, look), "class=\"detail fig\""));
}

TEST(the_bars_text_carries_its_shadow)
{
    Settings settings;
    const std::string rml = BarDocument(LookOf(BarTheme::PhoenixGold, settings), Place());
    // A 13 px title's shadow blurs 0.12 of its height: a Gaussian of 0.78 px, which RmlUi makes from a width of 2.
    // Once in place and once a pixel down, at 60%.
    CHECK(Has(rml, "font-effect:glow(0px 2px 0px 1px #00000099),glow(0px 2px 0px 0px #00000099);"));
}

TEST(the_help_bar_is_its_box_and_its_three_texts_centered_on_the_row)
{
    Settings settings;
    const BarLook look = LookOf(BarTheme::PhoenixGold, settings);
    HelpBarParts parts;
    parts.row     = HelpBarRow{BarRect{8.0f, 54.0f, 400.0f, 24.0f}, 23.0f, 120.0f, 180.0f};
    parts.caption = "Items";
    parts.bag     = Colored("6/30", 0xFF6FE36Fu);
    parts.help    = "Use <this>";
    const std::string rml = HelpBarRml(parts, look, Place());
    // The panel's gradient and gold edge, 0.11 H corners.
    CHECK(Has(rml, "<div class=\"help-box\" style=\"left:8px;top:54px;width:400px;height:24px;\"></div>"));
    // A 16 px line in a 24 px row starts 4 px down; the help's 17 px line, 4 (3.5 rounded).
    CHECK(Has(rml, "<div class=\"p hc\" style=\"left:23px;top:58px;color:#efe3c6ff;\">Items</div>"));
    CHECK(Has(rml, "<div class=\"p hc\" style=\"left:120px;top:58px;color:#6fe36fff;\">6/30</div>"));
    CHECK(Has(rml, "<div class=\"p hh\" style=\"left:180px;top:58px;color:#e4d8bdff;\">Use &lt;this&gt;</div>"));
    settings.helpBarBackground = false;
    CHECK(!Has(HelpBarRml(parts, look, Place()), "help-box"));
    parts.row = HelpBarRow{};
    CHECK(HelpBarRml(parts, look, Place()).empty());
    const std::string document = BarDocument(look, Place());
    CHECK(Has(document, "id=\"help\"") && Has(document, ".help-box{"));
    CHECK(Has(document, "decorator:vertical-gradient(#1c160ff7 #0c0a08f7)"));
}

TEST(a_popup_is_its_panel_and_each_piece_where_the_layout_put_it)
{
    Settings settings;
    const BarLook look = LookOf(BarTheme::PhoenixGold, settings);
    PopupLayout layout;
    layout.width  = 220.0f;
    layout.height = 120.0f;
    auto piece = [](PopupPiece::Kind kind, float x, float y, float width, float height) {
        PopupPiece p;
        p.kind = kind, p.x = x, p.y = y, p.width = width, p.height = height;
        return p;
    };
    PopupPiece card  = piece(PopupPiece::Kind::Card, 16.0f, 40.4f, 188.0f, 60.0f);
    PopupPiece title = piece(PopupPiece::Kind::Text, 16.4f, 16.6f, 80.0f, 24.0f);
    title.span = Text("Zone & <more>");
    title.font = PopupFont::Title;
    PopupPiece value = piece(PopupPiece::Kind::Text, 100.0f, 50.0f, 30.0f, 18.0f);
    value.span = Text("50%", Tone::Weak);
    PopupPiece time = piece(PopupPiece::Kind::Text, 140.4f, 30.0f, 40.2f, 18.0f);
    time.span       = Text("5:16");
    time.right      = true;
    PopupPiece icon = piece(PopupPiece::Kind::Icon, 20.0f, 50.0f, 19.3f, 19.3f);
    icon.icon       = Icon::Link;
    PopupPiece mark = piece(PopupPiece::Kind::ImmuneMark, 30.0f, 60.0f, 12.0f, 12.0f);
    mark.span       = Text("x", Tone::Immune);
    const PopupPiece star = piece(PopupPiece::Kind::Star, 140.0f, 52.0f, 14.9f, 14.9f);
    const PopupPiece rule = piece(PopupPiece::Kind::Rule, 60.0f, 46.0f, 100.0f, 1.0f);
    const PopupPiece tag  = piece(PopupPiece::Kind::Tag, 20.0f, 80.0f, 50.0f, 21.0f);
    const PopupPiece socket = piece(PopupPiece::Kind::Socket, 24.0f, 70.0f, 172.0f, 26.0f);
    PopupPiece arrow        = piece(PopupPiece::Kind::Fold, 24.0f, 56.0f, 9.4f, 9.4f);
    arrow.span              = Text("", Tone::Dim);
    arrow.open              = true;
    PopupPiece tab          = piece(PopupPiece::Kind::Tab, 24.0f, 104.0f, 50.0f, 21.0f);
    tab.open                = true;
    const PopupPiece track  = piece(PopupPiece::Kind::ScrollTrack, 190.0f, 72.0f, 4.0f, 22.0f);
    const PopupPiece thumb  = piece(PopupPiece::Kind::ScrollThumb, 190.0f, 76.4f, 4.0f, 7.1f);
    layout.pieces           = {card, title, value, icon, mark, star, rule, tag, socket, arrow, time, tab, track, thumb};
    // The title's line box starts 2 px under where the layout put its line; the text's, at it.
    const PopupFaces faces = {PopupFace{"Marcellus SC", 24, 29, 0, 2.0f}, PopupFace{"Barlow Condensed", 18, 22, 0, 0.0f},
        PopupFace{"Marcellus SC", 15, 18, 0, 0.0f}, PopupFace{"Marcellus SC", 15, 18, 0, 0.0f}};
    const std::string rml = PopupRml(layout, look, faces);
    // The gold theme's panel has a second border 2 px outside it.
    CHECK(Has(rml, "<div id=\"outer\" style=\"left:-3px;top:-3px;width:226px;height:126px;\"></div>"));
    CHECK(Has(rml, "<div id=\"panel\" style=\"width:220px;height:120px;\"></div>"));
    CHECK(Has(rml, "<div class=\"p card\" style=\"left:16px;top:40px;width:188px;height:60px;\"></div>"));
    CHECK(Has(rml, "<div class=\"p f0\" style=\"left:16px;top:19px;color:#e4d8bdff;\">Zone &amp; &lt;more&gt;</div>"));
    CHECK(Has(rml, "<div class=\"p f1\" style=\"left:100px;top:50px;color:#66ff66ff;\">50%</div>")); // the weak badge color
    // A value in its whole box, against its right: from its left's pixel to its right's.
    CHECK(Has(rml, "<div class=\"p f1\" style=\"left:140px;top:30px;width:41px;text-align:right;color:#e4d8bdff;\">5:16</div>"));
    CHECK(Has(rml, "<img class=\"p\" src=\"hu:icon/4\" style=\"left:20px;top:50px;width:19.3px;height:19.3px;\"/>"));
    CHECK(Has(rml, "<div class=\"p f1 mark\" style=\"left:30px;top:60px;width:12px;height:12px;line-height:12px;color:#ff4040ff;\">x</div>"));
    CHECK(Has(rml, "<img class=\"p\" src=\"hu:star/15\" style=\"left:140px;top:52px;width:15px;height:15px;\"/>"));
    CHECK(Has(rml, "<div class=\"p rule\" style=\"left:60px;top:46px;width:100px;\"></div>"));
    CHECK(Has(rml, "<div class=\"p tag\" style=\"left:20px;top:80px;width:50px;height:21px;border-radius:10.5px;\"></div>"));
    CHECK(Has(rml, "<div class=\"p socket\" style=\"left:24px;top:70px;width:172px;height:26px;\"></div>"));
    // A scrollbar's track and its thumb, both round at their ends.
    CHECK(Has(rml, "<div class=\"p track\" style=\"left:190px;top:72px;width:4px;height:22px;border-radius:2px;\"></div>"));
    CHECK(Has(rml, "<div class=\"p thumb\" style=\"left:190px;top:76px;width:4px;height:7px;border-radius:2px;\"></div>"));
    // The picked tab is lit.
    CHECK(Has(rml, "<div class=\"p tab lit\" style=\"left:24px;top:104px;width:50px;height:21px;border-radius:10.5px;\"></div>"));
    const std::string arrowTag = "<img class=\"p\" src=\"" + FoldSource(look.PopupColor(arrow.span), 9, true) +
                                 "\" style=\"left:24px;top:56px;width:9px;height:9px;\"/>";
    CHECK(Has(rml, arrowTag.c_str()));
    // A pill theme's panel has no outer border.
    const std::string pills = PopupRml(layout, LookOf(BarTheme::UmbraDark, settings), faces);
    CHECK(!Has(pills, "id=\"outer\""));
}

TEST(the_popup_document_styles_its_faces_and_boxes_from_the_look)
{
    Settings settings;
    const BarLook look = LookOf(BarTheme::PhoenixRedPills, settings);
    const PopupFaces faces = {PopupFace{"Barlow Condensed", 24, 29, 0}, PopupFace{"Barlow Condensed", 18, 22, 0},
        PopupFace{"Barlow Condensed", 15, 18, 2}, PopupFace{"Barlow Condensed", 15, 18, 0}};
    const std::string rml = PopupDocument(look, faces);
    CHECK(Has(rml, "pointer-events:none") && Has(rml, "<div id=\"popup\" class=\"hidden\"></div>"));
    CHECK(Has(rml, ".f0{font-family:\"Barlow Condensed\";font-size:24px;line-height:29px;letter-spacing:0px;"));
    CHECK(Has(rml, ".f2{font-family:\"Barlow Condensed\";font-size:15px;line-height:18px;letter-spacing:2px;"));
    // An open fold's rows sit sunk into the card: darker, its upper edges dark and its lower ones lit.
    CHECK(Has(rml, ".socket{box-sizing:border-box;border:1px;border-color:#00000059 #ffffff14 #ffffff14 #00000059;"));
    CHECK(Has(rml, ".tab{") && Has(rml, ".tab.lit{"));
    CHECK(Has(rml, ".track{") && Has(rml, ".thumb{"));
    // The panel: one fill top to bottom, the theme's faint edge, 0.28 of the popup's unit rounded.
    CHECK(Has(rml, "#panel{position:absolute;left:0px;top:0px;box-sizing:border-box;border:1px #d2abab33;"
                   "border-radius:12.6px;decorator:vertical-gradient(#180e0eff #180e0eff);}"));
    CHECK(Has(rml, ".card{box-sizing:border-box;border:1px #ffffff1e;border-radius:12.6px;background-color:#ffffff12;}"));
    CHECK(Has(rml, ".tag{box-sizing:border-box;border:1px transparent;background-color:#321f1fff;}"));
    CHECK(Has(rml, "hu:star/") == false);
}

TEST(a_star_is_served_in_the_coins_gold)
{
    CHECK_EQ(StarSource(15), "hu:star/15");
    const std::optional<Image> star = HostImage("hu:star/15");
    CHECK(star && star->width == 15 && star->height == 15);
    CHECK(star->argb[7 * 15 + 7] == 0xFFE8C35Au); // its middle
}

TEST(a_fold_arrow_is_served_in_its_color_and_way)
{
    CHECK_EQ(FoldSource(0xFFB0A8C0, 9, true), "hu:fold/ffb0a8c0/9/1");
    const std::optional<Image> down = HostImage(FoldSource(0xFFB0A8C0, 9, true)), right = HostImage(FoldSource(0xFFB0A8C0, 9, false));
    CHECK(down && right && down->argb == FoldArrow(9, 0xFFB0A8C0, true).argb && right->argb == FoldArrow(9, 0xFFB0A8C0, false).argb);
    CHECK(!HostImage("hu:fold/ffb0a8c0/9/2"));
}

TEST(every_theme_eases_breathes_and_pulses_in_its_own_colors)
{
    Settings settings;
    const PopupFaces faces = {PopupFace{"Barlow Condensed", 24, 29, 0}, PopupFace{"Barlow Condensed", 18, 22, 0},
        PopupFace{"Barlow Condensed", 15, 18, 2}, PopupFace{"Barlow Condensed", 15, 18, 0}};
    struct Lit
    {
        BarTheme theme;
        const char* breathe;
        const char* fill;
    };
    // The accent at about a third, into what it runs into at four fifths: gold into pale gold, blue into light
    // blue, the royal red into the ember, cyan into violet.
    for (const Lit& lit : {Lit{BarTheme::PhoenixGold, "from{border-color:#c9a24a59;}to{border-color:#efe3c6cc;}", "#c9a24aff #efe3c6ff"},
             Lit{BarTheme::UmbraDark, "from{border-color:#5b8def59;}to{border-color:#9cc2ffcc;}", "#5b8defff #9cc2ffff"},
             Lit{BarTheme::PhoenixRedOrnate, "from{border-color:#c5515159;}to{border-color:#e06f5ecc;}", "#c55151ff #e06f5eff"},
             Lit{BarTheme::PhoenixRedPills, "from{border-color:#c5515159;}to{border-color:#e06f5ecc;}", "#c55151ff #e06f5eff"},
             Lit{BarTheme::Aurora, "from{border-color:#5ee6ff59;}to{border-color:#b48cffcc;}", "#5ee6ffff #b48cffff"}})
    {
        const BarLook look    = LookOf(lit.theme, settings);
        const std::string rml = BarDocument(look, Place());
        // Widgets ease into their lit look, the target's frame breathes until it is lit (its border eases no other
        // way), and progress is a gradient that pulses.
        CHECK(Has(rml, "transition:background-color 0.2s cubic-out,border-color 0.2s cubic-out;"));
        CHECK(Has(rml, ".widget.target{animation:2.4s sine-in-out infinite alternate breathe;"
                       "transition:background-color 0.2s cubic-out;}"));
        CHECK(Has(rml, (std::string("@keyframes breathe{") + lit.breathe + "}").c_str()));
        CHECK(Has(rml, ".widget.lit{animation:none;}"));
        CHECK(Has(rml, (std::string("decorator:horizontal-gradient(") + lit.fill +
                           ");animation:1.8s sine-in-out infinite alternate pulse;}").c_str()));
        CHECK(Has(rml, "@keyframes pulse{from{opacity:0.65;}to{opacity:1;}}"));
        // A popup drops in from its widget.
        const std::string popup = PopupDocument(look, faces);
        CHECK(Has(popup, "#popup{position:absolute;transform-origin:50% 0%;}"));
        CHECK(Has(popup, "#popup.open{animation:0.2s cubic-out drop-in;}"));
        CHECK(Has(popup, "@keyframes drop-in{from{opacity:0;transform:translateY(-10px) scale(0.96);}to{opacity:1;"
                         "transform:translateY(0px) scale(1);}}"));
    }
}

TEST(the_line_of_light_and_the_glass_are_the_glass_shapes_alone)
{
    Settings settings;
    const BarLook aurora  = LookOf(BarTheme::Aurora, settings);
    const std::string rml = BarDocument(aurora, Place());
    // The strip's foot: a line of light, cyan to violet, fading out to both sides.
    CHECK(Has(rml, "<div class=\"glint a\"></div><div class=\"glint b\"></div><div class=\"glint c\"></div>"));
    CHECK(Has(rml, ".glint.a{left:0%;width:30%;decorator:horizontal-gradient(#5ee6ff00 #5ee6ffff);}"));
    CHECK(Has(rml, ".glint.c{left:70%;width:30%;decorator:horizontal-gradient(#b48cffff #b48cff00);}"));
    const PopupFaces faces = {PopupFace{"Barlow Condensed", 24, 29, 0}, PopupFace{"Barlow Condensed", 18, 22, 0},
        PopupFace{"Barlow Condensed", 15, 18, 2}, PopupFace{"Barlow Condensed", 15, 18, 0}};
    // The panel is glass, lighter at its top, in a frame of the accent at a quarter.
    CHECK(Has(PopupDocument(aurora, faces),
        "#panel{background-color:transparent;decorator:vertical-gradient(#1b2740f2 #0c1220f2);border-color:#5ee6ff40;}"));
    // The others keep their strips and panels.
    const BarLook gold = LookOf(BarTheme::PhoenixGold, settings);
    CHECK(!Has(BarDocument(gold, Place()), "glint"));
    CHECK(!Has(PopupDocument(gold, faces), "background-color:transparent;decorator:vertical-gradient(#1b2740f2"));
    CHECK(Has(PopupDocument(gold, faces), "decorator:vertical-gradient(#1c160ff7 #0c0a08f7);"));
    // It is the shape's, whatever the colors: Phoenix Gold's as glass has them, Aurora's as pills has neither.
    BarPalette glassGold = PaletteOf(BarTheme::PhoenixGold), pillAurora = PaletteOf(BarTheme::Aurora);
    glassGold.shape      = BarShape::Glass;
    pillAurora.shape     = BarShape::Pills;
    CHECK(Has(BarDocument(LookOf(glassGold, settings), Place()), "glint"));
    CHECK(Has(PopupDocument(LookOf(glassGold, settings), faces), "#panel{background-color:transparent;"));
    CHECK(!Has(BarDocument(LookOf(pillAurora, settings), Place()), "glint"));
    CHECK(!Has(PopupDocument(LookOf(pillAurora, settings), faces), "#panel{background-color:transparent;"));
}

TEST(the_strip_and_the_popups_are_drawn_with_the_games_art_when_the_shape_is_window)
{
    Settings settings;
    BarPalette palette = PaletteOf(BarTheme::UmbraDark);
    palette.shape      = BarShape::Window;
    BarLook look       = LookOf(palette, settings);
    look.skin          = SkinFrame{3, 1.0f, 0.5f, 1.0f, palette.panelBorder};
    const std::string bar = BarDocument(look, Place());
    CHECK(bar.find(SkinSource(3, SkinPiece::Back, kSkinTile)) != std::string::npos);
    CHECK(bar.find("hu:strip") == std::string::npos);
    // The art is the bar's own height, so the widgets centered in the bar are centered in the frame: no room for the
    // glow other themes fade out under the strip.
    const std::string strip = bar.substr(bar.find("#strip{"));
    CHECK(strip.substr(0, strip.find('}')).find("height:" + Px(look.metrics.height) + ";") != std::string::npos);
    // The strip, the help bar and the popups are each the art's own rules: its fill in its even border.
    const auto rule = [&](const std::string& document, const char* name) {
        const std::string box = document.substr(document.find(name));
        return box.substr(0, box.find('}'));
    };
    const PopupFaces faces{};
    for (const char* name : {"#strip{", ".help-box{"})
        CHECK(rule(bar, name).find(SkinRules(*look.skin)) != std::string::npos);
    CHECK(rule(PopupDocument(look, faces), "#panel{").find(SkinRules(*look.skin)) != std::string::npos);
    // Border off: the same art without its edge, everywhere.
    settings.gameWindowBorder = false;
    CHECK(FrameAsSet(settings, *look.skin) == (SkinFrame{3, 1.0f, 0.5f, 1.0f}));
    // Without the art read, the shape draws its colors.
    look.skin = std::nullopt;
    CHECK(BarDocument(look, Place()).find("hu:strip") != std::string::npos);
    CHECK(BarDocument(look, Place()).find("hu:skin") == std::string::npos);
    CHECK(PopupDocument(look, faces).find("hu:skin") == std::string::npos);
}

TEST(a_widget_with_text_of_its_own_has_rules_of_its_own_under_its_id)
{
    Settings settings;
    const BarLook look = LookOf(BarTheme::PhoenixGold, settings);
    const PanelWidget gil{BarWidget::Gil, 1};
    // Its own fonts at 20 px titles over 15 px details, a line box 24 px tall from row 6.
    const WidgetText own{gil, kCinzel, kMarcellusSC, TextRow{20, 15, true, 6.0f, 26.0f, 12.0f},
        BarTextPlace{BarLinePlace{24, 6.0f, 12.0f, 2.0f}, BarLinePlace{24, 6.0f, 12.0f, 2.0f}, BarLinePlace{18, 26.0f, 0.0f, 2.0f}, BarLinePlace{18, 16.0f, 16.0f, 2.0f}}};
    const std::string rml = BarDocument(look, Place(), {own});
    CHECK(Has(rml, "#w-gil-2 .title{font-family:\"Cinzel\";font-size:20px;line-height:24px;"));
    CHECK(Has(rml, "#w-gil-2 .detail{font-family:\"Marcellus SC\";font-size:15px;"));
    CHECK(!Has(rml, "#w-gil .title") && !Has(BarDocument(look, Place()), "#w-gil-2 .title"));
    // A widget's own row without room for two lines has its title alone: no detail rules of its own.
    WidgetText alone = own;
    alone.lines.twoLines = false;
    CHECK(!Has(BarDocument(look, Place(), {alone}), "#w-gil-2 .detail{"));
}

TEST(a_target_without_its_border_has_none_lit_or_not_and_does_not_breathe)
{
    Settings settings;
    const std::string ornate = BarDocument(LookOf(BarTheme::PhoenixGold, settings), Place());
    CHECK(Has(ornate, ".ornate .widget.target.borderless,.ornate .widget.target.borderless.lit{border-color:transparent;}"));
    CHECK(Has(ornate, ".widget.target.borderless{animation:none;}"));
    // Pills: the target's pill takes every widget's edge, and a lit one the accent as theirs do.
    const BarPalette& umbra = PaletteOf(BarTheme::UmbraDark);
    const std::string pills = BarDocument(LookOf(BarTheme::UmbraDark, settings), Place());
    const std::string edge  = ".pills .widget.target.borderless{border-color:" + RcssColor(umbra.pillBorder) + ";}";
    CHECK(pills.find(edge) != std::string::npos && pills.find(edge) < pills.find(".pills .widget.lit{"));
}

TEST(a_widget_of_cells_lays_each_in_its_column_a_gap_apart)
{
    Settings settings;
    const BarLook look = LookOf(BarTheme::PhoenixGold, settings);
    WidgetContent c;
    c.shown    = true;
    c.cells    = true;
    c.title    = {Text("S 1"), Text("B 2")};
    c.subtitle = {Text("W 1"), Text("BM 14")};
    const std::string rml = WidgetRml(c, look, {30.0f, 44.0f});
    // The first column its width and the gap; the last its width.
    const std::string first = "class=\"cell\" style=\"width:" + Px(30.0f + look.metrics.gap) + ";";
    const std::string last  = "class=\"cell\" style=\"width:" + Px(44.0f) + ";";
    CHECK(rml.find(first) != std::string::npos && rml.find(last) != std::string::npos);
    CHECK(rml.find(">S 1<") < rml.find(">B 2<") && rml.find(">B 2<") < rml.find(">W 1<"));
    CHECK_EQ(CellsWidth({30.0f, 44.0f}, look.metrics.gap), 30.0f + look.metrics.gap + 44.0f);
    // Measured, a column is its widest cell over both lines.
    const std::vector<float> columns = CellColumns(c, [](bool title, const Span& s) {
        return static_cast<float>(s.text.size()) * (title ? 10.0f : 8.0f);
    });
    CHECK((columns == std::vector<float>{30.0f, 40.0f})); // "S 1" at 10 a letter, "BM 14" at 8
}

TEST(a_span_of_a_game_picture_draws_that_picture_as_tall_as_its_letters_from_what_was_read_at_load)
{
    Settings settings;
    const BarLook look = LookOf(BarTheme::PhoenixGold, settings);
    WidgetContent c;
    c.shown = true;
    c.title = {Text("S 300"), Span{"", Tone::Normal, 0, GamePicture::PlayOnlineMessenger}, Text("2", Tone::Dim)};
    const std::string rml = WidgetRml(c, look);
    const std::string img = "<img class=\"line-icon pic\" src=\"" + GamePictureSource(GamePicture::PlayOnlineMessenger) + "\"/>";
    CHECK(rml.find(img) != std::string::npos && rml.find(img) < rml.find(">2<"));
    // Its pixels are the game's, B, G, R, A as an ARGB word keeps them.
    PlayOnlineIcons icons{ColorPicture{"menu    mesicon ", 1, 1, {1, 2, 3, 255}}, ColorPicture{"menu    mesicon ", 1, 1, {4, 5, 6, 255}}};
    const std::optional<Image> messenger = GamePictureImage(icons, GamePictureSource(GamePicture::PlayOnlineMessenger));
    CHECK(messenger && messenger->width == 1 && messenger->argb == std::vector<uint32_t>{0xFF060504u});
    CHECK(!GamePictureImage(std::nullopt, GamePictureSource(GamePicture::PlayOnlineMail)));
    CHECK(!GamePictureImage(icons, IconSource(Icon::Gm)));
}

TEST(a_meter_is_as_wide_as_its_bar_and_side_texts_with_the_texts_above_and_below_centered_on_the_bar)
{
    Meter m;
    m.width = 100;
    // Left 30 and a gap of 4 reach further than half of the 140 above past the bar; right 20 and its gap do too.
    const MeterPlace p = PlaceMeter(m, MeterWidths{140.0f, 0.0f, 30.0f, 20.0f}, 4.0f);
    CHECK_EQ(p.barX, 34.0f);
    CHECK_EQ(p.width, 34.0f + 100.0f + 24.0f);
    CHECK_EQ(p.aboveX, 34.0f + 50.0f - 70.0f);
    // A text above wider than all of that sets the width, the bar in its middle.
    const MeterPlace wide = PlaceMeter(m, MeterWidths{300.0f, 0.0f, 30.0f, 20.0f}, 4.0f);
    CHECK(wide.barX == 100.0f && wide.width == 300.0f && wide.aboveX == 0.0f);
}

TEST(a_meters_texts_are_measured_in_the_face_they_are_drawn_in)
{
    Settings settings;
    BarLook look = LookOf(BarTheme::PhoenixGold, settings);
    for (const bool twoLines : {true, false})
    {
        // On a row of one line they take the title's size.
        look.lines.twoLines = twoLines;
        look.lines.side     = twoLines ? look.lines.detail : look.lines.title;
        const TextFace face = look.MeterFace();
        const std::string rule = ".mtext{font-family:\"" + std::string(face.family) + "\";font-size:" + Px(face.size) + ";";
        CHECK(Has(BarDocument(look, Place()), rule.c_str()));
    }
}

TEST(a_meters_bar_has_its_middle_on_the_rows_middle_with_its_texts_around_it)
{
    Settings settings;
    const BarLook look = LookOf(BarTheme::PhoenixGold, settings);
    WidgetContent c;
    c.shown = true;
    Meter m;
    m.done   = 0.25f;
    m.width  = 100;
    m.height = 6;
    m.above  = {Text("12,345 / 20,000 EXP")};
    m.right  = {Text("61%")};
    c.meter  = m;
    const MeterPlace place = PlaceMeter(m, MeterWidths{120.0f, 0.0f, 0.0f, 20.0f}, look.metrics.gap);
    const std::string rml = WidgetRml(c, look, {}, place);
    const float barTop = std::round((look.row.height - 6.0f) / 2.0f);
    const std::string bar = "class=\"mbar\" style=\"left:" + Px(place.barX) + ";top:" + Px(barTop) + ";width:" + Px(100.0f) +
                            ";height:" + Px(6.0f) + ";\"";
    CHECK(rml.find(bar) != std::string::npos);
    CHECK(rml.find("width: 25%") != std::string::npos);
    // Above ends a gap over the bar's top; right starts a gap after its end.
    CHECK(rml.find("left:" + Px(place.aboveX) + ";bottom:" + Px(look.row.height - barTop + kMeterTextGap) + ";") != std::string::npos);
    CHECK(rml.find("class=\"mtext mside\" style=\"left:" + Px(place.barX + 100.0f + look.metrics.gap) + ";\"") != std::string::npos);
    CHECK(rml.find(">12,345 / 20,000 EXP<") != std::string::npos && rml.find(">61%<") != std::string::npos);
}
