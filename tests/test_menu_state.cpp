#include "menu_state.h"
#include "test.h"

using namespace headsup;

TEST(no_change_waiting_means_no_save)
{
    CHECK(!SaveDue(std::nullopt, 10.0, 0.0));
    CHECK(!SaveDue(std::nullopt, 10.0, 500.0));
}

TEST(a_change_saves_at_once_after_a_quick_save)
{
    CHECK(SaveDue(10.0, 10.0, 0.0));
    CHECK(SaveDue(10.0, 10.0, kSaveStallMs));
}

TEST(a_change_after_a_slow_save_waits_until_it_settles)
{
    CHECK(!SaveDue(10.0, 10.0, kSaveStallMs + 1.0));
    CHECK(!SaveDue(10.0, 10.0 + kSaveSettleSeconds / 2.0, kSaveStallMs + 1.0));
    CHECK(SaveDue(10.0, 10.0 + kSaveSettleSeconds, kSaveStallMs + 1.0));
}
