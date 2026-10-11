#include "frame_cache.h"
#include "test.h"

#include <string>
#include <string_view>
#include <utility>

using namespace headsup;

namespace
{
    using Cache = FrameCache<std::pair<std::string, int>, int>;
}

TEST(plates_asking_for_the_same_key_share_one_entry)
{
    Cache cache;
    cache.Use({"+25", 7}, 10) = 42;
    CHECK_EQ(cache.Use({"+25", 7}, 11), 42);
    CHECK_EQ(cache.Use({"+25", 8}, 11), 0); // another size is another entry
    CHECK_EQ(cache.Size(), 2u);
}

TEST(entries_unused_for_a_while_are_handed_back_to_be_released)
{
    Cache cache;
    cache.Use({"+25", 7}, 10) = 1;
    cache.Use({"-50", 7}, 10) = 2;
    cache.Use({"-50", 7}, 105); // still in use
    CHECK(cache.TakeIdle(110, 100).empty()); // 100 frames is not more than 100
    const std::vector<int> idle = cache.TakeIdle(111, 100);
    CHECK(idle == std::vector<int>{1});
    CHECK_EQ(cache.Size(), 1u);
    CHECK_EQ(cache.Use({"-50", 7}, 112), 2);
}

TEST(everything_can_be_taken_at_once)
{
    Cache cache;
    cache.Use({"a", 1}, 1) = 1;
    cache.Use({"b", 1}, 1) = 2;
    CHECK(cache.TakeAll() == (std::vector<int>{1, 2}));
    CHECK_EQ(cache.Size(), 0u);
}

TEST(a_view_of_a_key_finds_its_entry_and_a_new_one_keeps_its_own_copy)
{
    FrameCache<std::string, int> cache;
    std::string name = "Bloodtear Baldurf";
    cache.Use(std::string_view(name), 1) = 7;
    name = "Andhrimnir";
    CHECK_EQ(cache.Use(std::string_view("Bloodtear Baldurf"), 2), 7);
    CHECK_EQ(cache.Use(std::string("Bloodtear Baldurf"), 3), 7);
    CHECK_EQ(cache.Size(), 1u);
}
