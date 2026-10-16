#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace headsup
{
    struct KeyText
    {
        std::string key, text;
        bool operator==(const KeyText&) const = default;
    };

    // A settings file's text with each key set in a section and every other byte kept. The section and a key's line
    // are found as Windows' profile reader, which Ashita loads through, finds them: the first of the name, any case.
    std::string WithIniValues(std::string_view file, std::string_view section, std::span<const KeyText> values);
    // Whether the file holds every one of them already: setting them would change nothing.
    bool HoldsIniValues(std::string_view file, std::string_view section, std::span<const KeyText> values);
    // A key's value in a section, trimmed, as that reader reads it: none when the first section of the name lacks it.
    std::optional<std::string> IniValue(std::string_view file, std::string_view section, std::string_view key);
}
