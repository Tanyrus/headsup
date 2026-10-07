#include "con.h"
#include "test.h"

#include <string>

using namespace headsup;

// Expected values are read from Phoenix's era table and curve (modules/era/lua/globals/toau_experience_points.lua at
// 9b93232a), which its map server loads.

TEST(even_match_at_75)
{
    CHECK_EQ(BaseExp(75, 75), 100u);
    CHECK(Difficulty(75, 75) == Con::EvenMatch);
}

TEST(too_weak_boundary_at_75)
{
    CHECK_EQ(BaseExp(75, 56), 15u);
    CHECK_EQ(BaseExp(75, 55), 0u);
}

TEST(low_level_bracket)
{
    CHECK_EQ(BaseExp(10, 3), 20u);
    CHECK_EQ(BaseExp(10, 2), 15u);
    CHECK(Difficulty(10, 1) == Con::TooWeak);
}

TEST(level_difference_is_clamped_to_the_table)
{
    CHECK_EQ(BaseExp(1, 99), 600u);
    CHECK_EQ(BaseExp(99, 1), 0u);
}

TEST(unknown_player_level_gives_no_exp)
{
    CHECK_EQ(BaseExp(0, 10), 0u);
    CHECK(Difficulty(0, 10) == Con::TooWeak);
}

TEST(player_levels_above_99_use_99)
{
    CHECK_EQ(BaseExp(120, 99), 100u); // an even match at 99
}

TEST(difficulty_follows_the_era_curve)
{
    // Player 75: -20 0 exp, -19 15, -8 47, -7 50, -1 93, +1 130, +3 200, and player 20: +6 450.
    CHECK(Difficulty(75, 55) == Con::TooWeak);
    CHECK(Difficulty(75, 56) == Con::EasyPrey);
    CHECK(Difficulty(75, 67) == Con::EasyPrey);
    CHECK(Difficulty(75, 68) == Con::DecentChallenge);
    CHECK(Difficulty(75, 74) == Con::DecentChallenge);
    CHECK(Difficulty(75, 76) == Con::Tough);
    CHECK(Difficulty(75, 77) == Con::Tough);
    CHECK(Difficulty(75, 78) == Con::VeryTough);
    CHECK(Difficulty(75, 82) == Con::VeryTough);       // +7: 360
    CHECK(Difficulty(75, 83) == Con::IncrediblyTough); // +8: 400
    CHECK(Difficulty(20, 26) == Con::IncrediblyTough);
}

TEST(an_unknown_con_abbreviates_to_question_marks)
{
    CHECK(std::string(Abbrev(static_cast<Con>(kConCount))) == "??");
}
