#include "titles.h"

#include <algorithm>
#include <iterator>

namespace headsup
{
    namespace
    {
        struct TitleRecord
        {
            uint32_t id;
            const char* name;
        };
        constexpr TitleRecord kTitles[] = {
#include "generated/titles.inc"
        };
    }

    const char* TitleName(uint32_t title)
    {
        const auto* it = std::lower_bound(std::begin(kTitles), std::end(kTitles), title,
            [](const TitleRecord& r, uint32_t id) { return r.id < id; });
        return it != std::end(kTitles) && it->id == title ? it->name : "";
    }
}
