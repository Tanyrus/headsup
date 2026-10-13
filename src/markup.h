#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace headsup
{
    // Text as markup takes it: the characters it reads specially become entities.
    std::string EscapedRml(std::string_view text);
    // A color as RCSS reads it: #rrggbbaa.
    std::string RcssColor(uint32_t argb);
    // A number as RCSS reads it: two decimals at most, none it does not need.
    std::string RcssNumber(float value);
    std::string Px(float value);
    std::string Em(float value);
}
