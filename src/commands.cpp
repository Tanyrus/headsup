#include "commands.h"

#include <cctype>

namespace headsup
{
    std::vector<std::string> SplitLower(const char* command)
    {
        std::vector<std::string> words;
        std::string current;
        for (const char* p = command; *p; ++p)
        {
            if (*p == ' ' || *p == '\t')
            {
                if (!current.empty()) words.push_back(current), current.clear();
            }
            else
                current += static_cast<char>(std::tolower(static_cast<unsigned char>(*p)));
        }
        if (!current.empty()) words.push_back(current);
        return words;
    }
}
