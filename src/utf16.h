#pragma once

#include <string>

namespace headsup
{
    // Any byte that is not valid UTF-8 becomes U+FFFD, so text always draws as something.
    std::wstring Utf16(const std::string& utf8);
}
