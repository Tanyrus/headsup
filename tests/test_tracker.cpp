#include "test.h"
#include "tracker.h"

#include <string>

using namespace aggroglow;

namespace
{
    // Server IDs and names from data/phoenix_mobs.tsv (Valkurm Dunes).
    constexpr uint32_t kBountyHunter = 17199648; // Goblin Bounty Hunter: aggressive, 17-20
    constexpr uint32_t kSnipper      = 17199322; // Snipper: passive, 19-20
    constexpr uint32_t kBeachMonk    = 17199603; // Beach Monk: aggressive notorious, 23
    constexpr uint32_t kMetalShears  = 17199161; // Metal Shears: passive notorious, 22-23

    ActorInput Mob(ActorPtr actor, uint32_t serverId, const char* name, float distance = 10.0f, bool alive = true)
    {
        return ActorInput{actor, static_cast<uint16_t>(serverId & 0xFFF), serverId, true, alive, distance, name, nullptr};
    }

    ActorInput BountyHunter(ActorPtr actor, float distance = 10.0f, bool alive = true)
    {
        return Mob(actor, kBountyHunter, "Goblin Bounty Hunter", distance, alive);
    }

    ActorInput Player(ActorPtr actor)
    {
        return ActorInput{actor, 1052, 0x00012345, false, true, 0.0f, "Carrott", nullptr};
    }

    const PlayerState kLevel20{20, false};
}

TEST(players_are_tracked_without_outline_or_nameplate)
{
    Tracker t;
    t.Update({Player(0x1000)}, kLevel20, Settings{});
    const ActorInfo* info = t.Find(0x1000);
    CHECK(info != nullptr);
    CHECK(!info->outline && !info->isMob);
    CHECK(info->name[0] == '\0');
    CHECK_EQ(t.OutlinedCount(), 0u);
    CHECK(t.Mobs().empty());
}

TEST(each_category_gets_its_colour)
{
    Tracker t;
    const Settings s;
    t.Update({BountyHunter(0x2000), Mob(0x3000, kSnipper, "Snipper"), Mob(0x4000, 1, "Nobody Here"),
                 Mob(0x5000, kBeachMonk, "Beach Monk"), Mob(0x6000, kMetalShears, "Metal Shears")},
        kLevel20, s);
    CHECK_EQ(t.Find(0x2000)->argb, ToArgb(s.color[0]));
    CHECK_EQ(t.Find(0x3000)->argb, ToArgb(s.color[1]));
    CHECK_EQ(t.Find(0x4000)->argb, ToArgb(s.color[2]));
    CHECK_EQ(t.Find(0x5000)->argb, ToArgb(s.color[3]));
    CHECK_EQ(t.Find(0x6000)->argb, ToArgb(s.color[4]));
    CHECK_EQ(t.OutlinedCount(), 5u);
}

TEST(a_name_that_does_not_match_the_data_is_unknown)
{
    Tracker t;
    const Settings s;
    t.Update({Mob(0x2000, kBountyHunter, "Snipper")}, kLevel20, s);
    CHECK_EQ(t.Find(0x2000)->argb, ToArgb(s.color[2]));
}

TEST(hidden_category_is_not_outlined)
{
    Tracker t;
    Settings s;
    s.show[1] = false;
    t.Update({Mob(0x3000, kSnipper, "Snipper")}, kLevel20, s);
    CHECK(!t.Find(0x3000)->outline);
}

TEST(mobs_beyond_max_distance_are_not_outlined)
{
    Tracker t;
    t.Update({BountyHunter(0x2000, 40.0f), BountyHunter(0x3000, 40.5f)}, kLevel20, Settings{});
    CHECK(t.Find(0x2000)->outline);
    CHECK(!t.Find(0x3000)->outline);
}

TEST(defeated_mobs_are_not_outlined)
{
    Tracker t;
    t.Update({BountyHunter(0x2000, 10.0f, false)}, kLevel20, Settings{});
    CHECK(t.Find(0x2000) != nullptr);
    CHECK(!t.Find(0x2000)->outline);
}

TEST(disabled_outlines_nothing)
{
    Tracker t;
    Settings s;
    s.enabled = false;
    t.Update({BountyHunter(0x2000)}, kLevel20, s);
    CHECK(!t.Find(0x2000)->outline);
}

TEST(stencil_refs_stay_between_1_and_255)
{
    std::vector<ActorInput> mobs;
    for (ActorPtr i = 0; i < 300; ++i)
        mobs.push_back(BountyHunter(0x10000 + i * 0x10));
    Tracker t;
    t.Update(mobs, kLevel20, Settings{});
    CHECK_EQ(t.Find(0x10000)->stencilRef, 1);
    CHECK_EQ(t.Find(0x10000 + 254 * 0x10)->stencilRef, 255);
    CHECK_EQ(t.Find(0x10000 + 255 * 0x10)->stencilRef, 1);
}

TEST(unknown_pointers_are_not_found)
{
    Tracker t;
    t.Update({Player(0x1000), BountyHunter(0x2000)}, kLevel20, Settings{});
    CHECK(t.Find(0x1800) == nullptr);
    CHECK(t.Find(0x10) == nullptr);
    CHECK(t.Find(0x9000) == nullptr);
}

TEST(an_examined_spawn_uses_its_level_for_label_and_category)
{
    // Level 10 is Too Weak at 20, so the aggressive Goblin Bounty Hunter won't attack.
    Tracker t;
    const Settings s;
    const CheckResult check{10, Con::TooWeak};
    ActorInput input = BountyHunter(0x2000);
    input.examined   = &check;
    t.Update({input}, kLevel20, s);
    CHECK(std::string(t.Find(0x2000)->label.text) == "Lv 10 TW");
    CHECK_EQ(t.Find(0x2000)->argb, ToArgb(s.color[1]));
}

TEST(outlined_count_includes_only_outlined_mobs)
{
    Tracker t;
    Settings s;
    s.show[1] = false;
    t.Update({Player(0x1000), BountyHunter(0x2000), Mob(0x3000, kSnipper, "Snipper"), BountyHunter(0x4000, 50.0f)}, kLevel20, s);
    CHECK_EQ(t.OutlinedCount(), 1u);
    CHECK(t.Find(0x2000)->outline);
}

TEST(every_mob_gets_nameplate_data_at_any_distance)
{
    // Outlines stop at the max distance and hidden categories; nameplates do not.
    Tracker t;
    Settings s;
    s.show[1] = false;
    t.Update({Player(0x1000), BountyHunter(0x2000, 100.0f), Mob(0x3000, kSnipper, "Snipper")}, kLevel20, s);
    const ActorInfo* far = t.Find(0x2000);
    CHECK(far->isMob && far->alive && !far->outline);
    CHECK_EQ(far->index, 0x220);
    CHECK(std::string(far->name) == "Goblin Bounty Hunter");
    CHECK(std::string(far->label.text) == "Lv 17-20 EP-EM");
    CHECK(far->icons.count >= 1);
    CHECK(far->icons.icons[0] == Icon::AggroNQ);
    const ActorInfo* hidden = t.Find(0x3000);
    CHECK(!hidden->outline);
    CHECK(std::string(hidden->label.text) == "Lv 19-20 DC-EM");
    CHECK(hidden->icons.icons[0] == Icon::PassiveNQ);
    CHECK(t.Mobs() == (std::vector<ActorPtr>{0x2000, 0x3000}));
}

TEST(dead_mobs_keep_only_their_name)
{
    Tracker t;
    t.Update({BountyHunter(0x2000, 10.0f, false)}, kLevel20, Settings{});
    const ActorInfo* dead = t.Find(0x2000);
    CHECK(std::string(dead->name) == "Goblin Bounty Hunter");
    CHECK(dead->label.text[0] == '\0');
    CHECK_EQ(dead->icons.count, 0);
}

