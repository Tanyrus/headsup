#include "mobdb.h"
#include "test.h"

using namespace aggroglow;

// Expected values are read from third_party/mobdb/data (103 = Valkurm Dunes, 4 = Bibiki Bay).

TEST(name_lookup_aggressive_mob)
{
    const MobRecord* m = FindMob(103, 0, "Beach Monk");
    CHECK(m != nullptr);
    CHECK(m->aggro);
    CHECK_EQ(m->minLevel, 20);
    CHECK_EQ(m->maxLevel, 20);
}

TEST(name_lookup_passive_mob)
{
    const MobRecord* m = FindMob(103, 0, "Brutal Sheep");
    CHECK(m != nullptr);
    CHECK(!m->aggro);
    CHECK_EQ(m->maxLevel, 23);
}

TEST(escaped_names_are_unescaped)
{
    const MobRecord* m = FindMob(103, 0, "Goblin's Dragonfly");
    CHECK(m != nullptr);
    CHECK_EQ(m->minLevel, 23);
}

TEST(spawn_record_is_preferred_over_name_record)
{
    const MobRecord* spawn = FindMob(4, 12, "Kraken");
    CHECK(spawn != nullptr);
    CHECK_EQ(spawn->minLevel, 37);
    const MobRecord* named = FindMob(4, 0, "Kraken");
    CHECK(named != nullptr);
    CHECK_EQ(named->minLevel, 44);
    CHECK_EQ(named->maxLevel, 46);
}

TEST(spawn_record_for_a_different_name_is_ignored)
{
    CHECK(FindMob(4, 12, "Not Kraken") == nullptr);
}

TEST(unknown_zone_or_name)
{
    CHECK(FindMob(103, 0, "Nobody Here") == nullptr);
    CHECK(FindMob(9999, 0, "Beach Monk") == nullptr);
}

TEST(all_data_is_compiled_in)
{
    CHECK(MobRecordCount() > 19000);
}
