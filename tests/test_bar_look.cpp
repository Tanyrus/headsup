#include "bar_look.h"
#include "bar_placement.h"
#include "test.h"

#include <array>
#include <cmath>
#include <string>

using namespace headsup;

namespace
{
    bool Near(float a, float b) { return std::fabs(a - b) < 0.01f; }

    BarLook LookOf(BarTheme theme, const Settings& settings, TextRow lines = {}, int height = 45)
    {
        const BarPalette& p = PaletteOf(theme);
        const BarMetrics m  = MetricsFor(height, p.shape);
        return BarLook{p, settings, m, PillRow(m), lines, BarTitleFont(settings, p.shape), p.shape == BarShape::Ornate,
            BarTextFont(settings)};
    }

    bool Family(const TextFace& face, const char* family) { return std::string(face.family) == family; }
}

TEST(each_tone_takes_its_color_on_the_bar_and_in_a_popup)
{
    // Spec, Palettes: the bar's title and subtitle text, the popups' text, headings and tag text by theme; values take
    // the badge colors from settings, and neutral ones #8d8d8d.
    Settings settings;
    settings.weakBadgeColor   = ColorFromRgb(0x112233);
    settings.resistBadgeColor = ColorFromRgb(0x445566);
    settings.immuneBadgeColor = ColorFromRgb(0x778899);
    for (int theme = 0; theme < kBarThemeCount; ++theme)
    {
        const BarLook look  = LookOf(static_cast<BarTheme>(theme), settings);
        const BarPalette& p = look.palette;
        auto bar   = [&](Tone tone) { return look.BarColor(Span{"x", tone, 0xFF010203}); };
        auto popup = [&](Tone tone) { return look.PopupColor(Span{"x", tone, 0xFF010203}); };
        CHECK(bar(Tone::Normal) == p.title && popup(Tone::Normal) == p.text);
        CHECK(bar(Tone::Dim) == p.subtitle && popup(Tone::Dim) == p.subtitle);
        CHECK(popup(Tone::Heading) == p.heading && popup(Tone::Tag) == p.tagText);
        CHECK_EQ(popup(Tone::Neutral), 0xFF8D8D8Du);
        CHECK(popup(Tone::Weak) == 0xFF112233u && popup(Tone::Resist) == 0xFF445566u &&
              popup(Tone::Immune) == 0xFF778899u);
        CHECK(bar(Tone::Own) == 0xFF010203u && popup(Tone::Own) == 0xFF010203u);
    }
}

TEST(ornate_popups_head_their_sections_in_the_title_font_and_pills_popups_in_the_detail_font)
{
    // Spec, Shapes: ornate titles and headings in Marcellus SC, the rest in Barlow Condensed; pills all in Barlow.
    Settings settings;
    const BarLook ornate = LookOf(BarTheme::PhoenixGold, settings, TextRow{14, 11, true});
    const BarLook pills  = LookOf(BarTheme::UmbraDark, settings, TextRow{13, 11, true});
    CHECK(Family(ornate.FaceOf(PopupFont::Title), "Marcellus SC") &&
          Family(ornate.FaceOf(PopupFont::Heading), "Marcellus SC") &&
          Family(ornate.FaceOf(PopupFont::Text), "Barlow Condensed"));
    for (const PopupFont font : {PopupFont::Title, PopupFont::Text, PopupFont::Heading})
        CHECK(Family(pills.FaceOf(font), "Barlow Condensed"));
    // At 45 px: popup text 0.39 H, headings 0.33 H, a popup's title 1.35 times its text.
    CHECK(Near(ornate.FaceOf(PopupFont::Text).size, 17.55f) && Near(ornate.FaceOf(PopupFont::Heading).size, 14.85f) &&
          Near(ornate.FaceOf(PopupFont::Title).size, 23.69f));
    // A widget's title in the shape's title font, its detail line in Barlow, at the sizes their row fitted.
    CHECK(Family(ornate.TitleFace(), "Marcellus SC") && ornate.TitleFace().size == 14.0f);
    CHECK(Family(pills.TitleFace(), "Barlow Condensed") && pills.TitleFace().size == 13.0f);
    CHECK(Family(ornate.DetailFace(), "Barlow Condensed") && ornate.DetailFace().size == 11.0f);
}

TEST(pills_section_headings_are_letter_spaced_and_nothing_else_is)
{
    // Spec, Shapes: pills popups have "uppercase, letter-spaced section headings"; the approved mockup spaces them
    // 1.2 px at 10.5 px, which is 2 px for the 15 px headings at 45 px and 1 px for the 12 px ones at 36 px. The
    // pinned mark takes the headings' family and size, set solid as in the mockup.
    Settings settings;
    const BarLook pills = LookOf(BarTheme::UmbraDark, settings), ornate = LookOf(BarTheme::PhoenixGold, settings);
    CHECK_EQ(pills.FaceOf(PopupFont::Heading).tracking, 2);
    CHECK_EQ(LookOf(BarTheme::PhoenixRedPills, settings, {}, 36).FaceOf(PopupFont::Heading).tracking, 1);
    for (const PopupFont font : {PopupFont::Title, PopupFont::Text, PopupFont::Pin})
        CHECK_EQ(pills.FaceOf(font).tracking, 0);
    for (const PopupFont font : {PopupFont::Title, PopupFont::Text, PopupFont::Heading, PopupFont::Pin})
        CHECK_EQ(ornate.FaceOf(font).tracking, 0);
    CHECK(pills.TitleFace().tracking == 0 && pills.DetailFace().tracking == 0);
    for (const BarLook* look : {&pills, &ornate})
        CHECK(look->FaceOf(PopupFont::Pin).family == look->FaceOf(PopupFont::Heading).family &&
              look->FaceOf(PopupFont::Pin).size == look->FaceOf(PopupFont::Heading).size);
}

TEST(a_widgets_text_starts_after_its_icons_and_coin_each_with_a_gap)
{
    Settings settings;
    const BarLook look = LookOf(BarTheme::PhoenixGold, settings);
    WidgetContent weather;
    weather.icons = {Icon::Lightning, Icon::Lightning};
    CHECK(Near(look.Leading(weather), 2.0f * (22.5f + 7.65f))); // icons 0.50 H, gaps 0.17 H
    CHECK(Near(look.Leading(WidgetContent{}), 0.0f));
    // Gil's coin is drawn 17 px wide at 45 px (0.375 H, whole pixels), and its amount starts a gap after that.
    WidgetContent gil;
    gil.coin = true;
    CHECK_EQ(CoinPixels(look.metrics), 17);
    CHECK(Near(look.Leading(gil), 17.0f + 7.65f));
}

TEST(the_ornate_target_is_framed_and_a_lit_widget_tinted_on_the_pill_row)
{
    Settings settings;
    const BarLook look  = LookOf(BarTheme::PhoenixGold, settings);
    const BarPalette& p = look.palette;
    const BarRect rect{100.0f, 0.0f, 80.0f, 45.0f};
    CHECK(!look.BoxOf(PlacedWidget{BarWidget::Weather, rect}, false).has_value());
    const std::optional<WidgetBox> tint = look.BoxOf(PlacedWidget{BarWidget::Weather, rect}, true);
    CHECK(tint.has_value() && tint->rect.y == look.row.y && tint->rect.height == look.row.height);
    CHECK(tint->style.top == p.highlight && tint->style.bottom == p.highlight && tint->style.border == 0u);
    const std::optional<WidgetBox> frame = look.BoxOf(PlacedWidget{BarWidget::Target, rect}, false);
    CHECK(frame.has_value() && frame->rect == (BarRect{100.0f, look.row.y, 80.0f, look.row.height}));
    CHECK(frame->style.top == p.highlight && frame->style.border == p.accent);
    // The frame always has the hover tint, so hovering the target doubles it: 0x1A, 10%, to 0x34.
    const std::optional<WidgetBox> hovered = look.BoxOf(PlacedWidget{BarWidget::Target, rect}, true);
    CHECK(hovered.has_value() && hovered->rect == frame->rect && hovered->style.border == p.accent);
    CHECK(hovered->style.top == 0x34C9A24Au && hovered->style.bottom == 0x34C9A24Au);
}

TEST(a_lit_ornate_widgets_tint_stops_clear_of_the_dividers_beside_it)
{
    // Ornate neighbours meet at a divider, a diamond 0.22 H across drawn in whole pixels from a rounded corner (as
    // AddImage draws it): a hovered widget's tint stops two clear pixels short of it, the bar's least clear space,
    // and leaves the widget's icons and text at least as far inside it.
    Settings settings;
    size_t dividers = 0; // one between each pair of neighbours in a section
    for (const std::vector<BarWidget>& section : DefaultPlacement())
        dividers += section.empty() ? 0 : section.size() - 1;
    for (const BarTheme theme : {BarTheme::PhoenixGold, BarTheme::PhoenixRedOrnate})
        for (int h = kMinBarHeight; h <= kMaxBarHeight; ++h)
        {
            const BarLook look  = LookOf(theme, settings, {}, h);
            const BarMetrics& m = look.metrics;
            std::array<float, kBarWidgetCount> natural{};
            natural.fill(1800.0f / kBarWidgetCount); // narrow enough that the whole default bar fits
            const BarLayout layout = LayoutBar(1920.0f, m, BarShape::Ornate, natural, 0.0f);
            CHECK_EQ(layout.dividers.size(), dividers);
            const auto size = static_cast<float>(WholePixels(m.diamond));
            for (const PlacedWidget& placed : layout.widgets)
            {
                if (placed.widget == BarWidget::Target) continue;
                const std::optional<WidgetBox> tint = look.BoxOf(placed, true);
                CHECK(tint.has_value());
                const float left = tint->rect.x, right = tint->rect.x + tint->rect.width;
                for (const float divider : layout.dividers)
                {
                    const float drawn = std::round(divider - size / 2.0f);
                    CHECK(left >= drawn + size + 2.0f || right <= drawn - 2.0f);
                }
                const BarRect& r = placed.rect;
                CHECK(r.x + m.padding - left >= 2.0f && right - (r.x + r.width - m.padding) >= 2.0f);
            }
        }
}

TEST(pills_frame_every_widget_but_zone_and_earth_time_until_lit)
{
    Settings settings;
    const BarLook look  = LookOf(BarTheme::UmbraDark, settings);
    const BarPalette& p = look.palette;
    const BarRect rect{100.0f, look.row.y, 80.0f, look.row.height};
    for (const BarWidget flat : {BarWidget::Zone, BarWidget::EarthTime})
    {
        CHECK(!look.BoxOf(PlacedWidget{flat, rect}, false).has_value());
        const std::optional<WidgetBox> lit = look.BoxOf(PlacedWidget{flat, rect}, true);
        CHECK(lit.has_value() && lit->rect == rect && lit->style.top == p.highlight && lit->style.border == p.accent);
    }
    const std::optional<WidgetBox> bags = look.BoxOf(PlacedWidget{BarWidget::Bags, rect}, false);
    CHECK(bags.has_value() && bags->style.top == p.pill && bags->style.border == p.pillBorder);
    const std::optional<WidgetBox> target = look.BoxOf(PlacedWidget{BarWidget::Target, rect}, false);
    CHECK(target.has_value() && target->style.top == p.pill && target->style.border == p.accent);
    CHECK(Near(bags->style.radius, look.metrics.pillRadius));
}

TEST(a_widget_of_figures_draws_its_title_in_the_detail_font_in_every_theme)
{
    // Marcellus SC's 0 reads as an O and its small caps turn "2d 8h" into capitals: figures take Barlow Condensed at
    // the title's size, and a title of words keeps the shape's title font.
    Settings settings;
    WidgetContent figures, words;
    figures.figures = true;
    for (int theme = 0; theme < kBarThemeCount; ++theme)
    {
        const BarLook look = LookOf(static_cast<BarTheme>(theme), settings, TextRow{14, 11, true});
        CHECK(Family(look.TitleFace(figures), "Barlow Condensed") && look.TitleFace(figures).size == 14.0f);
        CHECK(look.TitleFace(figures).tracking == 0);
        CHECK(Family(look.TitleFace(words), look.TitleFace().family) && look.TitleFace(words).size == 14.0f);
    }
    CHECK(Family(LookOf(BarTheme::PhoenixGold, settings).TitleFace(words), "Marcellus SC"));
}

TEST(the_bar_takes_the_players_fonts_or_its_themes)
{
    Settings settings;
    CHECK(std::string(BarTitleFont(settings, BarShape::Ornate)) == TitleFont(BarShape::Ornate));
    CHECK(std::string(BarTextFont(settings)) == kDetailFont);
    settings.barTitleFont = "Cinzel";
    settings.barTextFont  = "Verdana";
    const char* title     = BarTitleFont(settings, BarShape::Ornate);
    CHECK(std::string(title) == "Cinzel" && std::string(BarTextFont(settings)) == "Verdana");
    settings.barTitleFont = "Georgia";
    BarTitleFont(settings, BarShape::Ornate);
    CHECK(std::string(title) == "Cinzel"); // an earlier name stays where caches keep it
    const BarLook look = LookOf(BarTheme::PhoenixGold, settings);
    CHECK(std::string(look.DetailFace().family) == "Verdana" && std::string(look.TitleFace().family) == "Georgia");
}
