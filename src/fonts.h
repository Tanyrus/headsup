#pragma once

#include "generated/font_families.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace headsup
{
    // A font compiled into the plugin: family is the name GDI matches on once the bytes are loaded.
    struct BundledFont
    {
        const char* family;
        const unsigned char* data;
        size_t bytes;
    };

    const BundledFont* BundledFonts(size_t& count);
    std::vector<std::string> BundledFamilies();
    // Whether two font names are one family: GDI matches names in any case.
    bool SameFamily(std::string_view a, std::string_view b);
    // Whether RmlUi draws a character from this bundled font when the font a text asks for lacks it: Montserrat
    // SemiBold has the arrows and marks Barlow Condensed, Cinzel and Marcellus SC do not.
    bool IsFallbackFont(std::string_view family);
}
