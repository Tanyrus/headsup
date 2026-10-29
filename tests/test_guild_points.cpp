#include "guild_points.h"
#include "test.h"

#include <string>

using namespace headsup;

namespace
{
    // The site's answer, cut down: Fishing with two ranks, Clothcraft (its "weaving") with an item and its +1 of one
    // rank, and a guild of an id past the crafts.
    constexpr const char* kAnswer =
        R"({"data":{"pattern":7,"guilds":[)"
        R"({"guildId":0,"pointsName":"guild_fishing","ranks":[)"
        R"({"rank":0,"rankName":"Amateur","itemId":4443,"itemName":"Cobalt Jellyfish","itemIconUrl":"x","points":24,"maxPoints":1200},)"
        R"({"rank":1,"rankName":"Recruit","itemId":4403,"itemName":"Yellow Globe","itemIconUrl":"x","points":60,"maxPoints":1680}]},)"
        R"({"guildId":4,"pointsName":"guild_weaving","ranks":[)"
        R"({"rank":0,"rankName":"Amateur","itemId":12,"itemName":"Nomad's Hose","itemIconUrl":"x","points":118,"maxPoints":1680},)"
        R"({"rank":0,"rankName":"Amateur","itemId":13,"itemName":null,"itemIconUrl":"x","points":110,"maxPoints":1680}]},)"
        R"({"guildId":9,"pointsName":"guild_tenshodo","ranks":[]}]}})";
}

TEST(the_sites_answer_reads_as_each_craft_guilds_turn_ins_by_rank)
{
    const std::optional<GuildPatterns> read = ReadGuildPatterns(kAnswer);
    CHECK(read.has_value());
    CHECK_EQ(read->pattern, 7);
    const std::vector<GuildTurnIn>& fishing = read->guilds[0];
    CHECK_EQ(fishing.size(), static_cast<size_t>(2));
    CHECK(fishing[0].rank == 0 && fishing[0].rankName == "Amateur" && fishing[0].item == 4443 &&
          fishing[0].itemName == "Cobalt Jellyfish" && fishing[0].points == 24 && fishing[0].maxPoints == 1200);
    CHECK(fishing[1].rank == 1 && fishing[1].item == 4403);
    // The site's weaving is Clothcraft; an item it has no name for keeps its id.
    CHECK_EQ(std::string(CraftGuildName(4)), "Clothcraft");
    const std::vector<GuildTurnIn>& cloth = read->guilds[4];
    CHECK(cloth.size() == 2 && cloth[1].item == 13 && cloth[1].itemName.empty());
    CHECK(read->guilds[1].empty());
    CHECK(CraftGuildNamed("Clothcraft") == 4 && CraftGuildNamed("Cooking") == 8 && !CraftGuildNamed("Tenshodo"));
}

TEST(an_answer_that_is_not_the_sites_reads_as_nothing)
{
    CHECK(!ReadGuildPatterns("<!DOCTYPE html><html></html>"));
    CHECK(!ReadGuildPatterns(R"({"error":"WORLD_INFO_ENABLED is off"})"));
    CHECK(!ReadGuildPatterns(R"({"data":{"guilds":[]}})")); // no pattern
    // A guild whose name is not the one its id stands for is left out rather than put in the wrong tab.
    const std::optional<GuildPatterns> moved = ReadGuildPatterns(
        R"({"data":{"pattern":1,"guilds":[{"guildId":0,"pointsName":"guild_cooking","ranks":[{"rank":0,"rankName":"Amateur","itemId":1,"itemName":"A","points":1,"maxPoints":2}]}]}})");
    CHECK(moved && moved->guilds[0].empty());
}

TEST(the_saved_answer_keeps_the_day_in_japan_it_was_fetched_and_is_due_again_the_next)
{
    // 14:59:59 UTC is 23:59:59 in Japan; a second later it is the next day there.
    constexpr int64_t kBeforeReset = 1761577199000LL, kAfterReset = kBeforeReset + 1000;
    CHECK_EQ(JstDay(kAfterReset), JstDay(kBeforeReset) + 1);
    const std::string file = GuildPatternsFile(JstDay(kBeforeReset), kAnswer);
    const std::optional<SavedGuildPatterns> saved = ReadGuildPatternsFile(file);
    CHECK(saved && saved->day == JstDay(kBeforeReset) && saved->json == kAnswer);
    CHECK(!GuildPatternsDue(saved->day, kBeforeReset));
    CHECK(GuildPatternsDue(saved->day, kAfterReset));
    CHECK(GuildPatternsDue(std::nullopt, kBeforeReset));
    CHECK(!ReadGuildPatternsFile("not a day\n{}"));
    CHECK(!ReadGuildPatternsFile(""));
}
