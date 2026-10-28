#include "guild_points.h"

#include "json_text.h"

#include <charconv>
#include <cmath>

namespace headsup
{
    namespace
    {
        // The site's names for the guilds, by id, and the shops' (Clothcraft is the site's weaving).
        constexpr const char* kPointsNames[kCraftGuilds] = {"guild_fishing", "guild_woodworking", "guild_smithing",
            "guild_goldsmithing", "guild_weaving", "guild_leathercraft", "guild_bonecraft", "guild_alchemy", "guild_cooking"};
        constexpr const char* kGuildNames[kCraftGuilds] = {"Fishing", "Woodworking", "Smithing", "Goldsmithing",
            "Clothcraft", "Leathercraft", "Bonecraft", "Alchemy", "Cooking"};
        constexpr int64_t kMsPerDay = 86400000LL, kJstOffsetMs = 9LL * 3600000LL;

        std::optional<int> WholeNumber(const JsonValue* value)
        {
            if (value == nullptr || value->kind != JsonValue::Kind::Number || value->number != std::floor(value->number))
                return std::nullopt;
            return static_cast<int>(value->number);
        }

        std::string TextOf(const JsonValue* value)
        {
            return value != nullptr && value->kind == JsonValue::Kind::Text ? value->text : std::string();
        }
    }

    const char* CraftGuildName(int guild) { return guild >= 0 && guild < kCraftGuilds ? kGuildNames[guild] : ""; }

    std::optional<int> CraftGuildNamed(std::string_view name)
    {
        for (int g = 0; g < kCraftGuilds; ++g)
            if (name == kGuildNames[g]) return g;
        return std::nullopt;
    }

    std::optional<GuildPatterns> ReadGuildPatterns(std::string_view json)
    {
        std::string problem;
        const std::optional<JsonValue> root = ParseJson(json, problem);
        const JsonValue* data = root ? root->Field("data") : nullptr;
        const JsonValue* list = data != nullptr ? data->Field("guilds") : nullptr;
        const std::optional<int> pattern = data != nullptr ? WholeNumber(data->Field("pattern")) : std::nullopt;
        if (!pattern || list == nullptr || list->kind != JsonValue::Kind::List) return std::nullopt;
        GuildPatterns read;
        read.pattern = *pattern;
        for (const JsonValue& guild : list->items)
        {
            const std::optional<int> id = WholeNumber(guild.Field("guildId"));
            const JsonValue* ranks      = guild.Field("ranks");
            if (!id || *id < 0 || *id >= kCraftGuilds || TextOf(guild.Field("pointsName")) != kPointsNames[*id] ||
                ranks == nullptr || ranks->kind != JsonValue::Kind::List)
                continue;
            for (const JsonValue& rank : ranks->items)
            {
                const std::optional<int> level = WholeNumber(rank.Field("rank")), item = WholeNumber(rank.Field("itemId"));
                const std::optional<int> points = WholeNumber(rank.Field("points")), most = WholeNumber(rank.Field("maxPoints"));
                if (!level || !item || !points || !most || *level < 0 || *item <= 0 || *item > UINT16_MAX) continue;
                read.guilds[static_cast<size_t>(*id)].push_back(GuildTurnIn{static_cast<uint8_t>(*level),
                    TextOf(rank.Field("rankName")), static_cast<uint16_t>(*item), TextOf(rank.Field("itemName")), *points, *most});
            }
        }
        return read;
    }

    int64_t JstDay(int64_t unixMs)
    {
        const int64_t shifted = unixMs + kJstOffsetMs;
        return shifted >= 0 ? shifted / kMsPerDay : (shifted - kMsPerDay + 1) / kMsPerDay;
    }

    std::string GuildPatternsFile(int64_t day, std::string_view json) { return std::to_string(day) + "\n" + std::string(json); }

    std::optional<SavedGuildPatterns> ReadGuildPatternsFile(std::string_view text)
    {
        const size_t line = text.find('\n');
        if (line == std::string_view::npos) return std::nullopt;
        int64_t day = 0;
        const auto [end, error] = std::from_chars(text.data(), text.data() + line, day);
        if (error != std::errc() || end != text.data() + line) return std::nullopt;
        return SavedGuildPatterns{day, std::string(text.substr(line + 1))};
    }

    bool GuildPatternsDue(std::optional<int64_t> savedDay, int64_t unixMs) { return !savedDay || *savedDay != JstDay(unixMs); }
}
