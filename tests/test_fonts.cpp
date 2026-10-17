#include "bar_theme.h"
#include "fonts.h"
#include "rml_session.h"
#include "settings.h"
#include "test.h"

#include <algorithm>

using namespace headsup;

TEST(the_bundled_fonts_are_montserrat_for_nameplates_then_marcellus_cinzel_cormorant_and_barlow_for_badges)
{
    const std::vector<std::string> families = BundledFamilies();
    CHECK(families == std::vector<std::string>({"Montserrat SemiBold", "Marcellus SC", "Cinzel", "Cormorant SC", "Barlow Condensed"}));
    size_t count             = 0;
    const BundledFont* fonts = BundledFonts(count);
    CHECK_EQ(count, families.size());
    for (size_t i = 0; i < count; ++i)
        CHECK(fonts[i].data != nullptr && fonts[i].bytes > 0);
    CHECK(std::string(kDefaultFont) == families[0]);
    CHECK(std::string(kBadgeFont) == families.back());
}

TEST(the_bar_asks_gdi_only_for_bundled_families)
{
    // GDI draws a family it does not have in another font without a word, so each must be one HeadsUp loads.
    const std::vector<std::string> families = BundledFamilies();
    auto bundled = [&](const char* family) { return std::ranges::find(families, family) != families.end(); };
    CHECK(bundled(TitleFont(BarShape::Ornate)) && bundled(TitleFont(BarShape::Pills)) && bundled(kDetailFont));
}

namespace
{
    // How wide RmlUi measures text in a bundled family at 20 px, as the plugin's TextWidth does.
    int MeasuredWidth(const char* family, const char* text)
    {
        Rml::FontEngineInterface* engine = Rml::GetFontEngineInterface();
        const Rml::FontFaceHandle face   = engine->GetFontFaceHandle(Rml::StringUtilities::ToLower(family),
            Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Normal, 20);
        static const Rml::String kNoLanguage;
        const Rml::TextShapingContext shaping{kNoLanguage, Rml::Style::Direction::Auto, Rml::Style::FontKerning::Auto, 0.0f};
        return face != 0 ? engine->GetStringWidth(face, Rml::String(text), shaping) : -1;
    }
}

TEST(a_character_a_bundled_font_lacks_is_drawn_from_montserrat)
{
    // Barlow Condensed, Cinzel and Marcellus SC have no right arrow (U+2192), which the popup puts before what a ???'s
    // act leads to; Montserrat SemiBold has one and is the fallback, so the arrow there is Montserrat's own.
    CHECK(IsFallbackFont(kMontserratSemiBold));
    CHECK(!IsFallbackFont(kBarlowCondensed) && !IsFallbackFont(kCinzel) && !IsFallbackFont(kMarcellusSC));
    test::RmlSession session;
    const char* arrow = "\xE2\x86\x92";
    const int montserrat = MeasuredWidth(kMontserratSemiBold, arrow);
    CHECK(montserrat > 0);
    for (const char* family : {kBarlowCondensed, kCinzel, kMarcellusSC})
        CHECK_EQ(MeasuredWidth(family, arrow), montserrat);
    // A character the font has stays its own.
    CHECK(MeasuredWidth(kBarlowCondensed, "M") != MeasuredWidth(kMontserratSemiBold, "M"));
}
