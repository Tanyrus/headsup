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
            if (*p != ' ' && *p != '\t')
                current += static_cast<char>(std::tolower(static_cast<unsigned char>(*p)));
            else if (!current.empty())
            {
                words.push_back(current);
                current.clear();
            }
        }
        if (!current.empty()) words.push_back(current);
        return words;
    }
}
