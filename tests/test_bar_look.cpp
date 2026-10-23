#include "bar_look.h"
#include "bar_placement.h"
#include "test.h"

#include <algorithm>
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
            BarTextFont(settings), std::nullopt};
    }

    bool Family(const TextFace& face, const char* family) { return std::string(face.family) == family; }
}

TEST(the_custom_theme_takes_the_settings_palette_and_starts_as_phoenix_gold)
{
    Settings s;
    CHECK(PaletteFor(s) == PaletteOf(BarTheme::PhoenixGold) && s.customPalette == PaletteOf(BarTheme::PhoenixGold));
    s.customPalette.accent = 0xFF00FF00;
    CHECK(PaletteFor(s) == PaletteOf(BarTheme::PhoenixGold)); // kept for when Custom is picked
    s.barTheme = BarTheme::Custom;
    CHECK(PaletteFor(s).accent == 0xFF00FF00u && PaletteFor(s).stripTop == PaletteOf(BarTheme::PhoenixGold).stripTop);
    s.barTheme = BarTheme::UmbraDark;
    CHECK(PaletteFor(s) == PaletteOf(BarTheme::UmbraDark));
}

TEST(each_tone_takes_its_color_on_the_bar_and_in_a_popup)
{
    // Spec, Palettes: the bar's title and subtitle text, the popups' text, headings and tag text by theme; values take
    // the badge colors from settings, and neutral ones #8d8d8d.
    Settings settings;
    settings.weakBadgeColor   = ColorFromRgb(0x112233);
    settings.resistBadgeColor = ColorFromRgb(0x445566);
    settings.immuneBadgeColor = ColorFromRgb(0x778899);
    for (int theme = 0; theme < kFixedThemeCount; ++theme)
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

TEST(the_ornate_target_is_framed_and_a_lit_widget_tinted)
{
    Settings settings;
    const BarLook look  = LookOf(BarTheme::PhoenixGold, settings);
    const BarPalette& p = look.palette;
    CHECK(!look.BoxOf(BarWidget::Weather, false).has_value());
    const std::optional<BoxStyle> tint = look.BoxOf(BarWidget::Weather, true);
    CHECK(tint.has_value() && tint->top == p.highlight && tint->bottom == p.highlight && tint->border == 0u);
    const std::optional<BoxStyle> frame = look.BoxOf(BarWidget::Target, false);
    CHECK(frame.has_value() && frame->top == p.highlight && frame->border == p.accent);
    // The frame always has the hover tint, so hovering the target doubles it: 0x1A, 10%, to 0x34.
    const std::optional<BoxStyle> hovered = look.BoxOf(BarWidget::Target, true);
    CHECK(hovered.has_value() && hovered->border == p.accent);
    CHECK(hovered->top == 0x34C9A24Au && hovered->bottom == 0x34C9A24Au);
}

TEST(pills_frame_every_widget_but_zone_and_earth_time_until_lit)
{
    Settings settings;
    const BarLook look  = LookOf(BarTheme::UmbraDark, settings);
    const BarPalette& p = look.palette;
    for (const BarWidget flat : {BarWidget::Zone, BarWidget::EarthTime})
    {
        CHECK(!look.BoxOf(flat, false).has_value());
        const std::optional<BoxStyle> lit = look.BoxOf(flat, true);
        CHECK(lit.has_value() && lit->top == p.highlight && lit->border == p.accent);
    }
    const std::optional<BoxStyle> bags = look.BoxOf(BarWidget::Bags, false);
    CHECK(bags.has_value() && bags->top == p.pill && bags->border == p.pillBorder);
    const std::optional<BoxStyle> target = look.BoxOf(BarWidget::Target, false);
    CHECK(target.has_value() && target->top == p.pill && target->border == p.accent);
    CHECK(Near(bags->radius, look.metrics.pillRadius));
}

TEST(a_widget_of_figures_draws_its_title_in_the_detail_font_in_every_theme)
{
    // Marcellus SC's 0 reads as an O and its small caps turn "2d 8h" into capitals: figures take Barlow Condensed at
    // the title's size, and a title of words keeps the shape's title font.
    Settings settings;
    WidgetContent figures, words;
    figures.figures = true;
    for (int theme = 0; theme < kFixedThemeCount; ++theme)
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

TEST(the_games_art_is_in_use_for_its_theme_or_a_custom_theme_of_its_shape)
{
    Settings s;
    CHECK(!UsesGameArt(s));
    s.barTheme = BarTheme::GameWindow;
    CHECK(UsesGameArt(s));
    s.barTheme = BarTheme::Custom;
    CHECK(!UsesGameArt(s));
    s.customPalette.shape = BarShape::Window;
    CHECK(UsesGameArt(s));
    s.barTheme = BarTheme::Aurora;
    CHECK(!UsesGameArt(s));
}

TEST(game_window_without_the_games_art_is_phoenix_gold)
{
    Settings s;
    s.barTheme = BarTheme::GameWindow;
    CHECK(PaletteFor(s) == PaletteOf(BarTheme::PhoenixGold));
}

namespace
{
    double Linear(uint32_t argb, int shift)
    {
        const double c = static_cast<double>(argb >> shift & 0xFF) / 255.0;
        return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
    }
    // WCAG's contrast ratio of two opaque colors.
    double Contrast(uint32_t a, uint32_t b)
    {
        const auto luminance = [](uint32_t c) { return 0.2126 * Linear(c, 16) + 0.7152 * Linear(c, 8) + 0.0722 * Linear(c, 0); };
        const double la = luminance(a), lb = luminance(b);
        return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
    }
    int Brightness(uint32_t argb) { return static_cast<int>((argb >> 16 & 0xFF) + (argb >> 8 & 0xFF) + (argb & 0xFF)); }
}

TEST(the_settings_window_takes_the_top_panels_theme)
{
    Settings s;
    // Phoenix Gold, the default, gives a window lit in its gold.
    CHECK(MenuThemeFor(s, nullptr) == MenuTheme::Phoenix);
    CHECK_EQ(MenuColorsFor(s, nullptr).accent, PaletteOf(BarTheme::PhoenixGold).accent);
    // The Phoenix Red themes keep the window drawn from phoenix-xi.com's palette, and Aurora its glass.
    s.barTheme = BarTheme::PhoenixRedPills;
    CHECK(MenuColorsFor(s, nullptr) == kPhoenixMenuColors);
    s.barTheme = BarTheme::Aurora;
    CHECK(MenuThemeFor(s, nullptr) == MenuTheme::Aurora && MenuColorsFor(s, nullptr) == kAuroraMenuColors);
    // Custom's own colors.
    s.barTheme             = BarTheme::Custom;
    s.customPalette.accent = 0xFF12AB34;
    CHECK_EQ(MenuColorsFor(s, nullptr).accent, 0xFF12AB34u);
    // The game's art, once read, for Game Window and a Custom theme of its shape; Phoenix Gold's colors until then.
    const GameSkinLook skin{SkinFrame{2, 1.0f, 0.5f, 1.0f}, PaletteOf(BarTheme::UmbraDark), kAuroraMenuColors};
    s.customPalette.shape = BarShape::Window;
    CHECK(MenuThemeFor(s, &skin) == MenuTheme::GameWindow && MenuColorsFor(s, &skin) == kAuroraMenuColors);
    s.barTheme = BarTheme::GameWindow;
    CHECK(MenuThemeFor(s, &skin) == MenuTheme::GameWindow);
    CHECK(MenuThemeFor(s, nullptr) == MenuTheme::Phoenix);
    CHECK_EQ(MenuColorsFor(s, nullptr).accent, PaletteOf(BarTheme::PhoenixGold).accent);
}

TEST(a_window_from_a_panels_colors_reads_well_and_steps_up_from_its_page)
{
    for (int t = 0; t < kFixedThemeCount; ++t)
    {
        const MenuColors c = MenuColorsFrom(PaletteOf(static_cast<BarTheme>(t)));
        const uint32_t page = c.background | 0xFF000000u;
        CHECK(Contrast(c.text, page) >= 7.0 && Contrast(c.heading, page) >= 7.0 && Contrast(c.label, page) >= 4.5);
        // Cards, controls and a hovered control each a step lighter than the page, and the trays darker.
        CHECK(Brightness(page) < Brightness(c.card) && Brightness(c.card) < Brightness(c.control) &&
              Brightness(c.control) < Brightness(c.hover) && Brightness(c.inset) < Brightness(page));
        CHECK(c.accent == PaletteOf(static_cast<BarTheme>(t)).accent);
    }
}
