#include "test.h"
#include "tracker.h"

using namespace aggroglow;

namespace
{
    ActorInput Mob(ActorPtr actor, const char* name, float distance = 10.0f, bool alive = true)
    {
        return ActorInput{actor, 0, true, alive, distance, name};
    }

    ActorInput Player(ActorPtr actor)
    {
        return ActorInput{actor, 1052, false, true, 0.0f, "Carrott"};
    }

    const PlayerState kLevel20{20, false, ConTable::Era};
    constexpr uint16_t kValkurm = 103;
}

TEST(players_are_tracked_but_never_outlined)
{
    Tracker t;
    t.Update({Player(0x1000)}, kValkurm, kLevel20, Settings{});
    const ActorInfo* info = t.Find(0x1000);
    CHECK(info != nullptr);
    CHECK(!info->outline);
    CHECK_EQ(t.OutlinedCount(), 0u);
}

TEST(each_category_gets_its_colour)
{
    Tracker t;
    const Settings s;
    t.Update({Mob(0x2000, "Beach Monk"), Mob(0x3000, "Brutal Sheep"), Mob(0x4000, "Nobody Here")}, kValkurm, kLevel20, s);
    CHECK_EQ(t.Find(0x2000)->argb, ToArgb(s.color[0]));
    CHECK_EQ(t.Find(0x3000)->argb, ToArgb(s.color[1]));
    CHECK_EQ(t.Find(0x4000)->argb, ToArgb(s.color[2]));
    CHECK_EQ(t.OutlinedCount(), 3u);
}

TEST(hidden_category_is_not_outlined)
{
    Tracker t;
    Settings s;
    s.show[1] = false;
    t.Update({Mob(0x3000, "Brutal Sheep")}, kValkurm, kLevel20, s);
    CHECK(!t.Find(0x3000)->outline);
}

TEST(mobs_beyond_max_distance_are_not_outlined)
{
    Tracker t;
    t.Update({Mob(0x2000, "Beach Monk", 40.0f), Mob(0x3000, "Beach Monk", 40.5f)}, kValkurm, kLevel20, Settings{});
    CHECK(t.Find(0x2000)->outline);
    CHECK(!t.Find(0x3000)->outline);
}

TEST(defeated_mobs_are_not_outlined)
{
    Tracker t;
    t.Update({Mob(0x2000, "Beach Monk", 10.0f, false)}, kValkurm, kLevel20, Settings{});
    CHECK(t.Find(0x2000) != nullptr);
    CHECK(!t.Find(0x2000)->outline);
}

TEST(disabled_outlines_nothing)
{
    Tracker t;
    Settings s;
    s.enabled = false;
    t.Update({Mob(0x2000, "Beach Monk")}, kValkurm, kLevel20, s);
    CHECK(!t.Find(0x2000)->outline);
}

TEST(stencil_refs_stay_between_1_and_255)
{
    std::vector<ActorInput> mobs;
    for (ActorPtr i = 0; i < 300; ++i)
        mobs.push_back(Mob(0x10000 + i * 0x10, "Beach Monk"));
    Tracker t;
    t.Update(mobs, kValkurm, kLevel20, Settings{});
    CHECK_EQ(t.Find(0x10000)->stencilRef, 1);
    CHECK_EQ(t.Find(0x10000 + 254 * 0x10)->stencilRef, 255);
    CHECK_EQ(t.Find(0x10000 + 255 * 0x10)->stencilRef, 1);
}

TEST(con_table_setting_is_applied)
{
    // Waraxe Beak (zone 119): aggressive, levels 55-56. At 78 it is Too Weak in the era table but Incredibly
    // Easy Prey (which still aggros) in the modern table.
    Tracker t;
    Settings s;
    const PlayerState level78{78, false, ConTable::Era};
    t.Update({Mob(0x2000, "Waraxe Beak")}, 119, level78, s);
    CHECK_EQ(t.Find(0x2000)->argb, ToArgb(s.color[1]));
    s.modernConTable = true;
    t.Update({Mob(0x2000, "Waraxe Beak")}, 119, level78, s);
    CHECK_EQ(t.Find(0x2000)->argb, ToArgb(s.color[0]));
}

TEST(unknown_pointers_are_not_found)
{
    Tracker t;
    t.Update({Player(0x1000), Mob(0x2000, "Beach Monk")}, kValkurm, kLevel20, Settings{});
    CHECK(t.Find(0x1800) == nullptr);
    CHECK(t.Find(0x10) == nullptr);
    CHECK(t.Find(0x9000) == nullptr);
}
