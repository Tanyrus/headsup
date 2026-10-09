#include "fonts.h"

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
}
