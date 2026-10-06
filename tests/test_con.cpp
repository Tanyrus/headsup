#include "con.h"
#include "test.h"

using namespace aggroglow;

// Expected values are read from the Phoenix tables in src/con_tables.inc.

TEST(era_even_match_at_75)
{
    CHECK_EQ(BaseExp(ConTable::Era, 75, 75), 100u);
    CHECK(!IsTooWeak(ConTable::Era, 75, 75));
}

TEST(era_too_weak_boundary_at_75)
{
    CHECK_EQ(BaseExp(ConTable::Era, 75, 56), 15u);
    CHECK(!IsTooWeak(ConTable::Era, 75, 56));
    CHECK_EQ(BaseExp(ConTable::Era, 75, 55), 0u);
    CHECK(IsTooWeak(ConTable::Era, 75, 55));
}

TEST(era_low_level_bracket)
{
    CHECK_EQ(BaseExp(ConTable::Era, 10, 3), 20u);
    CHECK_EQ(BaseExp(ConTable::Era, 10, 2), 15u);
    CHECK(IsTooWeak(ConTable::Era, 10, 1));
}

TEST(level_difference_is_clamped_to_the_table)
{
    CHECK_EQ(BaseExp(ConTable::Era, 1, 99), 600u);
    CHECK_EQ(BaseExp(ConTable::Modern, 1, 99), 800u);
}

TEST(unknown_player_level_gives_no_exp)
{
    CHECK_EQ(BaseExp(ConTable::Era, 0, 10), 0u);
}

TEST(player_levels_above_99_use_99)
{
    CHECK_EQ(BaseExp(ConTable::Era, 120, 99), BaseExp(ConTable::Era, 99, 99));
}

TEST(modern_even_match_and_easy_prey_threshold)
{
    CHECK_EQ(BaseExp(ConTable::Modern, 75, 75), 200u);
    CHECK_EQ(BaseExp(ConTable::Modern, 10, 2), 60u);
    CHECK(!IsTooWeak(ConTable::Modern, 10, 2));
    CHECK(IsTooWeak(ConTable::Modern, 10, 1));
}

TEST(modern_incredibly_easy_prey_needs_mob_level_56)
{
    CHECK_EQ(BaseExp(ConTable::Modern, 78, 56), 55u);
    CHECK(!IsTooWeak(ConTable::Modern, 78, 56));
    CHECK_EQ(BaseExp(ConTable::Modern, 76, 33), 14u);
    CHECK(IsTooWeak(ConTable::Modern, 76, 33));
}
