#include "fonts.h"
#include "settings.h"
#include "test.h"

#include <algorithm>

using namespace headsup;

TEST(the_bundled_fonts_are_marcellus_cinzel_and_cormorant)
{
    const std::vector<std::string> families = BundledFamilies();
    CHECK(families == std::vector<std::string>({"Marcellus SC", "Cinzel", "Cormorant SC"}));
    size_t count             = 0;
    const BundledFont* fonts = BundledFonts(count);
    CHECK_EQ(count, families.size());
    for (size_t i = 0; i < count; ++i)
        CHECK(fonts[i].data != nullptr && fonts[i].bytes > 0);
    CHECK(std::string(kDefaultFont) == families[0]);
}
