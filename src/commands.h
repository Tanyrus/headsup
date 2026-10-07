#pragma once

#include <string>
#include <vector>

namespace headsup
{
    // A typed command's words, lower-cased, split on spaces and tabs.
    std::vector<std::string> SplitLower(const char* command);
}
