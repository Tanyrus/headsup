#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace headsup
{
    // A JSON value. An object keeps its fields in the file's order: their names beside their values in items.
    struct JsonValue
    {
        enum class Kind : uint8_t
        {
            Null,
            Flag,
            Number,
            Text,
            List,
            Object,
        };
        Kind kind     = Kind::Null;
        bool flag     = false;
        double number = 0.0;
        std::string text; // UTF-8
        std::vector<std::string> names;
        std::vector<JsonValue> items;

        // An object's field of this name, its last where it has two; none where it has none.
        const JsonValue* Field(std::string_view name) const;
    };

    constexpr size_t kMostJsonDepth = 64; // lists and objects within each other: far more than a layout holds
    // The value the text holds, or none with what is wrong with it, and on which line, in problem.
    std::optional<JsonValue> ParseJson(std::string_view text, std::string& problem);
}
