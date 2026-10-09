#include "test.h"
#include "tracker.h"

#include <cmath>

#include <iterator>
#include <string>
#include <unordered_set>

using namespace headsup;

namespace
{
    // Server IDs and names from data/phoenix_mobs.tsv (Valkurm Dunes, and East Ronfaure for Bigmouth Billy).
    constexpr uint32_t kBountyHunter  = 17199648; // Goblin Bounty Hunter: aggressive, 17-20
    constexpr uint32_t kSnipper       = 17199322; // Snipper: passive, 19-20
    constexpr uint32_t kBeachMonk     = 17199603; // Beach Monk: aggressive notorious, 23
    constexpr uint32_t kBigmouthBilly = 17191196; // Bigmouth Billy: passive notorious, 9-10
    constexpr uint32_t kCarrionWorm   = 17191194; // Carrion Worm: passive, 4-5, Bigmouth Billy's lottery placeholder
    constexpr uint32_t kPoisonFunguar = 17195258; // Poison Funguar: aggressive, 14-15, a placeholder in La Theine Plateau

    ActorInput Mob(ActorPtr actor, uint32_t serverId, const char* name, float distance = 10.0f, bool alive = true)
    {
        return ActorInput{.actor = actor, .index = static_cast<uint16_t>(serverId & 0xFFF), .serverId = serverId,
            .kind = EntityKind::Mob, .alive = alive, .distance = distance, .name = name};
    }

    ActorInput BountyHunter(ActorPtr actor, float distance = 10.0f, bool alive = true)
    {
        return Mob(actor, kBountyHunter, "Goblin Bounty Hunter", distance, alive);
    }

    ActorInput Player(ActorPtr actor)
    {
        return ActorInput{.actor = actor, .index = 1052, .serverId = 0x00012345, .kind = EntityKind::Player, .alive = true,
            .distance = 0.0f, .name = "Carrott"};
    }

    ActorInput Npc(ActorPtr actor)
    {
        return ActorInput{.actor = actor, .index = 1100, .serverId = 0x01011234, .kind = EntityKind::Npc, .alive = true,
            .distance = 3.0f, .name = "Home Point #1"};
    }

    const PlayerState kLevel20{20, false};
    const PlayerState kLevel75{75, false};

    // Outlines for every category, whatever the defaults show.
    Settings EveryCategory()
    {
        Settings s;
        for (bool& shown : s.show)
            shown = true;
        return s;
    }
}

TEST(a_mob_attacks_as_phoenixs_aggro_check_decides)
{
    CHECK(Classify(nullptr, 0, kLevel75) == Category::Unknown);
    struct Row
    {
        const char* why;
        uint8_t flags;
        uint8_t minLevel, maxLevel;
        int16_t levelMod;
        int checkedLevel;
        PlayerState player;
        Category expected;
    };
    const PlayerState sitting{75, true}, unknown{0, false};
    for (const Row& r : {
             Row{"passive", 0, 70, 75, 0, 0, kLevel75, Category::WontAttack},
             Row{"aggressive even match", kMobAggressive, 75, 75, 0, 0, kLevel75, Category::WillAttack},
             Row{"aggressive but Too Weak", kMobAggressive, 50, 55, 0, 0, kLevel75, Category::WontAttack},
             Row{"the top of the range decides", kMobAggressive, 50, 56, 0, 0, kLevel75, Category::WillAttack},
             Row{"Too Weak still attacks you sitting", kMobAggressive, 50, 55, 0, 0, sitting, Category::WillAttack},
             Row{"your level unknown", kMobAggressive, 1, 1, 0, 0, unknown, Category::WillAttack},
             Row{"a level a spawn script sets", kMobAggressive, 0, 0, 0, 0, kLevel75, Category::WillAttack},
             Row{"a check gives a scripted mob its level", kMobAggressive, 0, 0, 0, 55, kLevel75, Category::WontAttack},
             Row{"always aggro ignores Too Weak", kMobAlwaysAggro, 50, 55, 0, 0, kLevel75, Category::WillAttack},
             Row{"no aggro wins", kMobAggressive | kMobAlwaysAggro | kMobNoAggro, 75, 75, 0, 0, kLevel75, Category::WontAttack},
             // Phoenix lets a mob with the Follow roam flag follow instead, even an always-aggro one.
             Row{"followers never attack", kMobAggressive | kMobAlwaysAggro | kMobFollows, 75, 75, 0, 0, kLevel75, Category::WontAttack},
             // At 75, level 56 gives 15 exp (Easy Prey) and 55 none.
             Row{"a lowering level mod counts", kMobAggressive, 50, 56, -1, 0, kLevel75, Category::WontAttack},
             Row{"a raising level mod counts", kMobAggressive, 50, 55, 1, 0, kLevel75, Category::WillAttack},
             Row{"a checked level already has the mod", kMobAggressive, 50, 56, -1, 56, kLevel75, Category::WillAttack},
             Row{"a checked level replaces the range", kMobAggressive, 50, 56, 0, 55, kLevel75, Category::WontAttack},
             Row{"a notorious monster that attacks", kMobAggressive | kMobNotorious, 75, 75, 0, 0, kLevel75, Category::NmWillAttack},
             Row{"a passive notorious monster", kMobNotorious, 75, 75, 0, 0, kLevel75, Category::NmWontAttack},
             Row{"a Too Weak notorious monster", kMobAggressive | kMobNotorious, 50, 55, 0, 0, kLevel75, Category::NmWontAttack},
         })
    {
        const MobRecord mob{kBountyHunter, "Test Mob", r.minLevel, r.maxLevel, r.flags, 300, 0, r.levelMod, 0};
        auto outcome = [&](Category c) { return std::string(r.why) + ": " + std::to_string(CategoryIndex(c)); };
        CHECK_EQ(outcome(Classify(&mob, r.checkedLevel, r.player)), outcome(r.expected));
    }
}

TEST(players_and_npcs_keep_their_names_without_outlines)
{
    Tracker t;
    t.Update({Player(0x1000), Npc(0x1100)}, kLevel20, Settings{});
    const ActorInfo* player = t.Find(0x1000);
    const ActorInfo* npc    = t.Find(0x1100);
    CHECK(player != nullptr && npc != nullptr);
    CHECK(player->kind == EntityKind::Player && npc->kind == EntityKind::Npc);
    CHECK(std::string(player->name) == "Carrott");
    CHECK(std::string(npc->name) == "Home Point #1");
    CHECK(!player->outline && !npc->outline);
    CHECK(player->label.text[0] == '\0' && npc->mobIcons.count == 0); // levels and icons come from mob data
    CHECK_EQ(t.OutlinedCount(), 0u);
    CHECK(t.Actors() == (std::vector<ActorPtr>{0x1000, 0x1100}));
}

TEST(a_name_is_replaced_when_its_kind_is_and_it_has_one)
{
    ActorInput unnamed = Npc(0x1300);
    unnamed.index      = 1101;
    unnamed.name       = "";
    const std::vector<ActorInput> everyone{BountyHunter(0x1000), Player(0x1100), Npc(0x1200), unnamed};
    struct Case
    {
        bool mobs, players, npcs;
        bool mob, player, npc;
    };
    for (const Case& c : {Case{true, false, false, true, false, false}, Case{false, true, false, false, true, false},
             Case{false, false, true, false, false, true}})
    {
        Settings s;
        s.replaceMobNames  = c.mobs;
        s.replacePlayerNames = c.players;
        s.replaceNpcNames    = c.npcs;
        Tracker t;
        t.Update(everyone, kLevel20, s);
        CHECK(ReplacesName(s, *t.Find(0x1000)) == c.mob);
        CHECK(ReplacesName(s, *t.Find(0x1100)) == c.player);
        CHECK(ReplacesName(s, *t.Find(0x1200)) == c.npc);
        CHECK(!ReplacesName(s, *t.Find(0x1300)));
    }
}

TEST(owner_is_the_first_tracked_pointer_of_any_kind)
{
    Tracker t;
    t.Update({Player(0x1000), BountyHunter(0x2000)}, kLevel20, Settings{});
    const uint32_t playerDraw[] = {0x5, 0x1234, 0x1000, 0x2000}; // stale mob pointer above the live player
    CHECK(FindOwner(std::begin(playerDraw), std::end(playerDraw), t) == t.Find(0x1000));
    const uint32_t mobDraw[] = {0x5, 0x2000, 0x1000};
    CHECK(FindOwner(std::begin(mobDraw), std::end(mobDraw), t) == t.Find(0x2000));
    const uint32_t noActor[] = {0x5, 0x6};
    CHECK(FindOwner(std::begin(noActor), std::end(noActor), t) == nullptr);
}

TEST(spawn_flags_give_the_kind)
{
    CHECK(KindFromSpawnFlags(0x10) == EntityKind::Mob);
    CHECK(KindFromSpawnFlags(0x01) == EntityKind::Player);
    CHECK(KindFromSpawnFlags(0x11) == EntityKind::Mob); // the mob bit wins
    CHECK(KindFromSpawnFlags(0x00) == EntityKind::Npc);
    CHECK(KindFromSpawnFlags(0x0E) == EntityKind::Npc);
}

TEST(players_keep_their_pose_and_feet)
{
    Tracker t;
    ActorInput seated = Player(0x1100);
    seated.pose       = Pose::Chair;
    seated.feet       = WorldPoint{61.9f, -0.9f, -98.0f};
    t.Update({seated, Player(0x1200)}, kLevel20, Settings{});
    CHECK(t.Find(0x1100)->pose == Pose::Chair);
    CHECK_EQ(t.Find(0x1100)->feet.z, -98.0f);
    CHECK(t.Find(0x1200)->pose == Pose::Standing);
}

TEST(players_get_their_status_icons_and_linkshell_color)
{
    PlayerStatus status;
    status.bazaar = status.linkshell = true;
    status.linkshellArgb = 0xFF8F1FFF;
    ActorInput player    = Player(0x1000);
    player.status        = status;
    Tracker t;
    t.Update({player, Player(0x1100)}, kLevel20, Settings{});
    const ActorInfo* shopping = t.Find(0x1000);
    CHECK_EQ(shopping->playerIcons.left.count, 2);
    CHECK(shopping->playerIcons.left.icons[0] == Icon::Linkshell && shopping->playerIcons.left.icons[1] == Icon::Bazaar);
    CHECK_EQ(shopping->linkshellArgb, 0xFF8F1FFFu);
    CHECK_EQ(t.Find(0x1100)->playerIcons.left.count, 0); // no status seen yet
    Settings right;
    right.playerIconSide[static_cast<int>(PlayerIcon::Bazaar)] = IconSide::Right;
    t.Update({player}, kLevel20, right);
    CHECK(t.Find(0x1000)->playerIcons.left.count == 1 && t.Find(0x1000)->playerIcons.right.icons[0] == Icon::Bazaar);
}

TEST(each_category_gets_its_colour)
{
    Tracker t;
    const Settings s = EveryCategory();
    t.Update({BountyHunter(0x2000), Mob(0x3000, kSnipper, "Snipper"), Mob(0x4000, 1, "Nobody Here"),
                 Mob(0x5000, kBeachMonk, "Beach Monk"), Mob(0x6000, kBigmouthBilly, "Bigmouth Billy")},
        kLevel20, s);
    CHECK_EQ(t.Find(0x2000)->argb, ToArgb(s.color[CategoryIndex(Category::WillAttack)]));
    CHECK_EQ(t.Find(0x3000)->argb, ToArgb(s.color[CategoryIndex(Category::WontAttack)]));
    CHECK_EQ(t.Find(0x4000)->argb, ToArgb(s.color[CategoryIndex(Category::Unknown)]));
    CHECK_EQ(t.Find(0x5000)->argb, ToArgb(s.color[CategoryIndex(Category::NmWillAttack)]));
    CHECK_EQ(t.Find(0x6000)->argb, ToArgb(s.color[CategoryIndex(Category::NmWontAttack)]));
    CHECK_EQ(t.OutlinedCount(), 5u);
}

TEST(a_mobs_level_line_carries_its_id_or_ph_and_a_placeholder_glows_purple)
{
    Tracker t;
    Settings s;
    s.mobId           = MobIdFormat::LastThree;
    ActorInput worm   = Mob(0x2000, kCarrionWorm, "Carrion Worm");
    worm.claimed      = true;
    t.Update({worm, BountyHunter(0x3000)}, kLevel20, s);
    const ActorInfo* ph = t.Find(0x2000);
    CHECK(std::string(ph->label.text) == "Lv 4-5 TW [PH]");
    CHECK(ph->tooWeak && ph->claimed);
    CHECK(ph->outline && ph->argb == ToArgb(s.color[CategoryIndex(Category::Placeholder)]));
    const ActorInfo* hunter = t.Find(0x3000);
    CHECK(std::string(hunter->label.text) == "Lv 17-20 EP-EM [220]");
    CHECK(!hunter->tooWeak && !hunter->claimed);
    s.show[CategoryIndex(Category::Placeholder)] = false;
    t.Update({Mob(0x2000, kCarrionWorm, "Carrion Worm"), Mob(0x4000, kPoisonFunguar, "Poison Funguar")}, kLevel20, s);
    CHECK(!t.Find(0x2000)->outline); // colored like any mob: passive, so not outlined
    CHECK(t.Find(0x4000)->outline && t.Find(0x4000)->argb == ToArgb(s.color[CategoryIndex(Category::WillAttack)]));
}

TEST(a_name_that_does_not_match_the_data_is_unknown)
{
    Tracker t;
    const Settings s = EveryCategory();
    t.Update({Mob(0x2000, kBountyHunter, "Snipper")}, kLevel20, s);
    CHECK_EQ(t.Find(0x2000)->argb, ToArgb(s.color[CategoryIndex(Category::Unknown)]));
}

TEST(mobs_beyond_max_distance_are_not_outlined)
{
    Tracker t;
    const Settings s;
    t.Update({BountyHunter(0x2000, s.maxDistance), BountyHunter(0x3000, s.maxDistance + 0.5f)}, kLevel20, s);
    CHECK(t.Find(0x2000)->outline);
    CHECK(!t.Find(0x3000)->outline);
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
    CHECK_EQ(t.Find(0x10000 + 254 * 0x10)->stencilRef, 255); // an 8-bit stencil's largest value
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

TEST(a_checked_spawn_uses_its_level_for_label_and_category)
{
    // Level 10 is Too Weak at 20, so the aggressive Goblin Bounty Hunter won't attack.
    Tracker t;
    const Settings s = EveryCategory();
    const CheckResult check{10, Con::TooWeak};
    ActorInput input = BountyHunter(0x2000);
    input.checked    = &check;
    t.Update({input}, kLevel20, s);
    CHECK_EQ(t.Find(0x2000)->label.text, "Lv 10 TW");
    CHECK_EQ(t.Find(0x2000)->argb, ToArgb(s.color[CategoryIndex(Category::WontAttack)]));
}

TEST(outlined_count_includes_only_outlined_mobs)
{
    Tracker t;
    Settings s = EveryCategory();
    s.show[CategoryIndex(Category::WontAttack)] = false;
    t.Update({Player(0x1000), BountyHunter(0x2000), Mob(0x3000, kSnipper, "Snipper"), BountyHunter(0x4000, s.maxDistance + 10.0f)},
        kLevel20, s);
    CHECK_EQ(t.OutlinedCount(), 1u);
    CHECK(t.Find(0x2000)->outline);
}

TEST(every_mob_gets_nameplate_data_at_any_distance)
{
    // Outlines stop at the max distance and hidden categories; nameplates do not.
    Tracker t;
    Settings s; // passive mobs are not outlined by default
    t.Update({Player(0x1000), BountyHunter(0x2000, 100.0f), Mob(0x3000, kSnipper, "Snipper")}, kLevel20, s);
    const ActorInfo* far = t.Find(0x2000);
    CHECK(far->kind == EntityKind::Mob && far->alive && !far->outline);
    CHECK_EQ(far->index, 0x220);
    CHECK(std::string(far->name) == "Goblin Bounty Hunter");
    CHECK(std::string(far->label.text) == "Lv 17-20 EP-EM");
    CHECK(far->mobIcons.count >= 1);
    CHECK(far->mobIcons.icons[0] == Icon::AggroNQ);
    CHECK(far->category == Category::WillAttack); // its name still glows in the color it would be outlined in
    const ActorInfo* hidden = t.Find(0x3000);
    CHECK(!hidden->outline);
    CHECK(std::string(hidden->label.text) == "Lv 19-20 DC-EM");
    CHECK(hidden->mobIcons.icons[0] == Icon::PassiveNQ);
    CHECK(hidden->category == Category::WontAttack);
    CHECK(t.Actors() == (std::vector<ActorPtr>{0x1000, 0x2000, 0x3000})); // in entity order
}

TEST(dead_mobs_keep_only_their_name)
{
    Tracker t;
    t.Update({BountyHunter(0x2000, 10.0f, false)}, kLevel20, Settings{});
    const ActorInfo* dead = t.Find(0x2000);
    CHECK(!dead->outline);
    CHECK(std::string(dead->name) == "Goblin Bounty Hunter");
    CHECK(dead->label.text[0] == '\0');
    CHECK_EQ(dead->mobIcons.count, 0);
}

TEST(squared_distance_becomes_yalms_and_keeps_nan)
{
    // Ashita reports squared distances. A NaN must stay NaN so no distance limit passes it; std::max(0, NaN) is 0.
    CHECK_EQ(DistanceFromSquared(16.0f), 4.0f);
    CHECK_EQ(DistanceFromSquared(-1.0f), 0.0f);
    CHECK(std::isnan(DistanceFromSquared(std::nanf(""))));
}

TEST(a_claim_by_you_or_your_party_is_told_from_anyone_elses)
{
    // A mob's claim: the claimer's server ID in its low 16 bits, and 1 in its high 16 while it is claimed.
    const std::vector<uint32_t> party{0x00012345, 0x01098ABC};
    CHECK(ClaimedByParty(0x00012345, party));  // you
    CHECK(ClaimedByParty(0x00018ABC, party));  // a party member, by the low half of their ID
    CHECK(!ClaimedByParty(0x00002345, party)); // the claim ended; the last claimer stays
    CHECK(!ClaimedByParty(0x00011111, party)); // someone else's
    CHECK(!ClaimedByParty(0x00012345, {}));
}

TEST(a_mob_your_party_claimed_is_marked)
{
    ActorInput ours     = BountyHunter(0x2000);
    ours.claimedByParty = true;
    Tracker t;
    t.Update({Player(0x1000), ours, Mob(0x3000, kSnipper, "Snipper")}, kLevel20, Settings{});
    CHECK(t.Find(0x2000)->claimedByParty);
    CHECK(!t.Find(0x3000)->claimedByParty);
}
