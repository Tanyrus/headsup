#include "bar_theme.h"
#include "test.h"

#include <cmath>
#include <string>

using namespace headsup;

namespace
{
    bool Near(float a, float b) { return std::fabs(a - b) < 0.01f; }
}

TEST(each_theme_has_the_mockups_colors_and_shape)
{
    const BarPalette& gold = PaletteOf(BarTheme::PhoenixGold);
    CHECK(gold.shape == BarShape::Ornate && gold.accent == 0xFFC9A24Au && gold.title == 0xFFEFE3C6u);
    CHECK_EQ(gold.below, 0x59C9A24Au); // the gold glow at 35%
    const BarPalette& umbra = PaletteOf(BarTheme::UmbraDark);
    CHECK(umbra.shape == BarShape::Pills && umbra.accent == 0xFF5B8DEFu && umbra.pill == 0xFF222226u);
    CHECK(PaletteOf(BarTheme::PhoenixRedOrnate).shape == BarShape::Ornate);
    CHECK_EQ(PaletteOf(BarTheme::PhoenixRedOrnate).accent, 0xFFC55151u);
    CHECK(PaletteOf(BarTheme::PhoenixRedPills).shape == BarShape::Pills);
    CHECK_EQ(PaletteOf(BarTheme::PhoenixRedPills).pillBorder, 0x33D2ABABu); // #d2abab at 20%
    CHECK(std::string(BarThemeName(BarTheme::PhoenixRedPills)) == "Phoenix red, pills");
}

TEST(sizes_follow_the_bar_height)
{
    const BarMetrics m = MetricsFor(36, BarShape::Ornate);
    CHECK(Near(m.height, 36.0f) && Near(m.title, 14.4f) && Near(m.subtitle, 11.88f) && Near(m.icon, 18.0f));
    // The pill leaves room above and below it for its text; the end cap, room either side of its diamond.
    CHECK(Near(m.pillHeight, 28.8f) && Near(m.cap, 23.04f));
    CHECK(Near(MetricsFor(36, BarShape::Pills).panelRadius, 10.08f) && Near(m.panelRadius, 3.96f));
    CHECK(Near(MetricsFor(200, BarShape::Pills).height, 64.0f) && Near(MetricsFor(1, BarShape::Pills).height, 24.0f));
    CHECK(std::string(TitleFont(BarShape::Ornate)) == "Marcellus SC" &&
          std::string(TitleFont(BarShape::Pills)) == kDetailFont);
}

TEST(auto_text_sizes_are_the_heights_as_before)
{
    for (const int h : {24, 36, 45, 64})
        for (const BarShape shape : {BarShape::Ornate, BarShape::Pills})
        {
            const BarMetrics m = MetricsFor(h, shape, kAutoBarTextSize, kAutoBarTextSize);
            const float H      = static_cast<float>(h);
            CHECK(Near(m.title, 0.40f * H) && Near(m.subtitle, 0.33f * H) && Near(m.popupText, 0.39f * H));
            CHECK(Near(m.heading, 0.33f * H) && Near(m.popupPadding, 0.36f * H) && Near(m.rowGap, 0.11f * H));
            CHECK(Near(m.panelRadius, (shape == BarShape::Ornate ? 0.11f : 0.28f) * H));
            CHECK(Near(m.icon, 0.50f * H) && Near(m.gap, 0.17f * H)); // the bar's own sizes never change
        }
}

TEST(a_set_bar_text_size_is_the_titles_and_the_detail_line_keeps_its_share)
{
    const BarMetrics m = MetricsFor(45, BarShape::Ornate, 16, kAutoBarTextSize);
    CHECK(Near(m.title, 16.0f) && Near(m.subtitle, 16.0f * 0.33f / 0.40f));
    CHECK(Near(m.popupText, 0.39f * 45.0f)); // the popups keep theirs
}

TEST(a_set_popup_text_size_scales_the_popup_and_the_height_no_longer_does)
{
    const BarMetrics low = MetricsFor(24, BarShape::Pills, kAutoBarTextSize, 18);
    const BarMetrics high = MetricsFor(64, BarShape::Pills, kAutoBarTextSize, 18);
    const float u = 18.0f / 0.39f; // the height whose Auto popup text is 18 px
    for (const BarMetrics* m : {&low, &high})
    {
        CHECK(Near(m->popupText, 18.0f) && Near(m->heading, 0.33f * u) && Near(m->popupPadding, 0.36f * u));
        CHECK(Near(m->rowGap, 0.11f * u) && Near(m->panelRadius, 0.28f * u));
    }
    CHECK(Near(low.title, 0.40f * 24.0f) && Near(high.title, 0.40f * 64.0f)); // the bar keeps its own
}

TEST(a_popup_text_size_at_its_auto_position_follows_the_height)
{
    const BarMetrics autoSize = MetricsFor(45, BarShape::Ornate, kAutoBarTextSize, kAutoPopupTextSize);
    CHECK(Near(autoSize.popupText, 0.39f * 45.0f) && Near(autoSize.popupPadding, 0.36f * 45.0f));
    CHECK(Near(MetricsFor(45, BarShape::Ornate, kAutoBarTextSize, kAutoPopupTextSize + 1).popupText, 10.0f));
}
