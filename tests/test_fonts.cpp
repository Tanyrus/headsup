#include "bar_theme.h"
#include "fonts.h"
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
