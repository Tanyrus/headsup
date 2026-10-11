#pragma once

#include <cstdint>

namespace headsup
{
    // A title's name from Phoenix's title list; "" for 0 (none) or a number the list lacks.
    const char* TitleName(uint32_t title);
}
