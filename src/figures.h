#pragma once

#include <cstdint>
#include <string>

namespace headsup
{
    // 1,234,567
    std::string Grouped(uint64_t value);
    // "300 KB" below a megabyte, "40.3 MB" from one up.
    std::string KbText(uint64_t kb);
}
