#include "fonts.h"

#include <algorithm>
#include <cctype>
#include <iterator>

namespace headsup
{
    namespace
    {
#include "generated/fonts.inc"
    }

    const BundledFont* BundledFonts(size_t& count)
    {
        count = std::size(kBundledFonts);
        return kBundledFonts;
    }

    std::vector<std::string> BundledFamilies()
    {
        std::vector<std::string> families;
        for (const BundledFont& font : kBundledFonts)
            families.emplace_back(font.family);
        return families;
    }

    bool SameFamily(std::string_view a, std::string_view b)
    {
        return std::ranges::equal(a, b, [](char x, char y) {
            return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
        });
    }
}
