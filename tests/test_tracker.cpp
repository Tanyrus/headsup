#include "test.h"
#include "tracker.h"

#include <algorithm>
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

    // Outlines for every mob row, whatever the defaults show.
    Settings EveryCategory()
    {
        Settings s;
        for (RowSettings& row : s.Kind(PlateKind::Mobs).rows)
            row.outline = OutlineMode::Shown;
        return s;
    }

    uint32_t Aggression(const Settings& s, Nature nature) { return ToArgb(s.palette.aggression[static_cast<int>(nature)]); }

    void SetMobOutline(Settings& s, Category row, OutlineMode mode) { s.Kind(PlateKind::Mobs).rows[CategoryIndex(row)].outline = mode; }
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
        const MobRecord mob{kBountyHunter, "Test Mob", r.minLevel, r.maxLevel, r.flags, 300, 0, r.levelMod, 0, 0, 0};
        auto outcome = [&](Category c) { return std::string(r.why) + ": " + std::to_string(CategoryIndex(c)); };
        CHECK_EQ(outcome(Classify(&mob, r.checkedLevel, r.player)), outcome(r.expected));
    }
}

TEST(players_and_npcs_keep_their_names_without_outlines)
{
    Tracker t;
    t.Update({Player(0x1000), Npc(0x1100)}, kLevel20, Settings{}, CursorTargets{});
    const ActorInfo* player = t.Find(0x1000);
    const ActorInfo* npc    = t.Find(0x1100);
    CHECK(player != nullptr && npc != nullptr);
    CHECK(player->kind == EntityKind::Player && npc->kind == EntityKind::Npc);
    CHECK(std::string(player->name) == "Carrott");
    CHECK(std::string(npc->name) == "Home Point #1");
    CHECK(!player->outline && !npc->outline);
    CHECK(player->level.text[0] == '\0' && npc->mob == nullptr); // levels and icons come from mob data
    CHECK_EQ(t.OutlinedCount(), 0u);
    CHECK(t.Actors() == (std::vector<ActorPtr>{0x1000, 0x1100}));
}

TEST(owner_is_the_first_tracked_pointer_of_any_kind)
{
    Tracker t;
    t.Update({Player(0x1000), BountyHunter(0x2000)}, kLevel20, Settings{}, CursorTargets{});
    const uint32_t playerDraw[] = {0x5, 0x1234, 0x1000, 0x2000}; // stale mob pointer above the live player
    CHECK(FindOwner(std::begin(playerDraw), std::end(playerDraw), t) == t.Find(0x1000));
    const uint32_t mobDraw[] = {0x5, 0x2000, 0x1000};
    CHECK(FindOwner(std::begin(mobDraw), std::end(mobDraw), t) == t.Find(0x2000));
    const uint32_t noActor[] = {0x5, 0x6};
    CHECK(FindOwner(std::begin(noActor), std::end(noActor), t) == nullptr);
}

TEST(owner_is_looked_for_past_the_games_draw_call_not_in_other_plugins_leftovers)
{
    Tracker t;
    t.Update({Player(0x1000), BountyHunter(0x2000)}, kLevel20, Settings{}, CursorTargets{});
    constexpr uint32_t kCodeBegin = 0x400000, kCodeEnd = 0x500000;
    // A plugin that ran first left the target (the mob) under the game's return address, 0x401234.
    const uint32_t playerDraw[] = {0x5, 0x2000, 0x401234, 0x1000};
    CHECK(FindOwnerAboveCaller(std::begin(playerDraw), std::end(playerDraw), t, kCodeBegin, kCodeEnd) == t.Find(0x1000));
    const uint32_t noReturn[] = {0x5, 0x2000, 0x1000}; // no game address: the plain scan
    CHECK(FindOwnerAboveCaller(std::begin(noReturn), std::end(noReturn), t, kCodeBegin, kCodeEnd) == t.Find(0x2000));
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
    t.Update({seated, Player(0x1200)}, kLevel20, Settings{}, CursorTargets{});
    CHECK(t.Find(0x1100)->pose == Pose::Chair);
    CHECK_EQ(t.Find(0x1100)->feet.z, -98.0f);
    CHECK(t.Find(0x1200)->pose == Pose::Standing);
}

TEST(players_keep_their_status_and_linkshell_color)
{
    PlayerStatus status;
    status.bazaar = status.linkshell = true;
    status.linkshellArgb = 0xFF8F1FFF;
    ActorInput player    = Player(0x1000);
    player.status        = status;
    Tracker t;
    t.Update({player, Player(0x1100)}, kLevel20, Settings{}, CursorTargets{});
    const ActorInfo* shopping = t.Find(0x1000);
    CHECK(shopping->status && shopping->status->bazaar && shopping->status->linkshell && !shopping->status->away);
    CHECK_EQ(shopping->linkshellArgb, 0xFF8F1FFFu);
    CHECK(!t.Find(0x1100)->status); // no status seen yet
}

TEST(each_category_gets_its_colour)
{
    Tracker t;
    const Settings s = EveryCategory();
    t.Update({BountyHunter(0x2000), Mob(0x3000, kSnipper, "Snipper"), Mob(0x4000, 1, "Nobody Here"),
                 Mob(0x5000, kBeachMonk, "Beach Monk"), Mob(0x6000, kBigmouthBilly, "Bigmouth Billy")},
        kLevel20, s, CursorTargets{});
    CHECK_EQ(t.Find(0x2000)->argb, Aggression(s, Nature::Aggressive));
    CHECK_EQ(t.Find(0x3000)->argb, Aggression(s, Nature::Passive));
    CHECK_EQ(t.Find(0x4000)->argb, Aggression(s, Nature::NoData));
    CHECK_EQ(t.Find(0x5000)->argb, Aggression(s, Nature::AggressiveNm));
    CHECK_EQ(t.Find(0x6000)->argb, Aggression(s, Nature::PassiveNm));
    CHECK_EQ(t.OutlinedCount(), 5u);
}

TEST(a_renamed_mob_keeps_the_data_its_server_id_gives)
{
    // The renamer addon writes its own names over the game's, often to mark placeholders and the notorious monsters
    // they bring: a mob's record follows its server ID alone.
    Tracker t;
    const Settings s;
    t.Update({Mob(0x2000, kCarrionWorm, "Carrion Worm (PH)"), Mob(0x3000, kBigmouthBilly, "NM Billy"),
                 Mob(0x4000, kBountyHunter, "")},
        kLevel20, s, CursorTargets{});
    const ActorInfo* ph = t.Find(0x2000);
    CHECK(ph->mob != nullptr && ph->placeholder && ph->state.nature == Nature::Placeholder);
    const ActorInfo* nm = t.Find(0x3000);
    CHECK(nm->mob != nullptr && !nm->placeholder && nm->state.nature == Nature::PassiveNm);
    const ActorInfo* unnamed = t.Find(0x4000);
    CHECK(unnamed->mob != nullptr && unnamed->state.nature == Nature::Aggressive);
}

TEST(a_placeholder_is_outlined_by_its_own_row_in_its_own_color)
{
    Tracker t;
    Settings s;
    ActorInput worm   = Mob(0x2000, kCarrionWorm, "Carrion Worm");
    worm.claimed      = true;
    t.Update({worm, BountyHunter(0x3000), Mob(0x5000, kCarrionWorm, "Carrion Worm")}, kLevel20, s, CursorTargets{});
    const ActorInfo* ph = t.Find(0x2000);
    CHECK(std::string(ph->level.text) == "Lv 4-5 TW");
    CHECK(ph->placeholder && ph->state.tooWeak && ph->state.claim == ClaimState::Others);
    CHECK(t.Find(0x5000)->outline && t.Find(0x5000)->argb == Aggression(s, Nature::Placeholder));
    CHECK(ph->outline && ph->argb == Aggression(s, Nature::Placeholder)); // claimed, and the claim row defers to it
    const ActorInfo* hunter = t.Find(0x3000);
    CHECK(std::string(hunter->level.text) == "Lv 17-20 EP-EM");
    CHECK(!hunter->placeholder && !hunter->state.tooWeak && hunter->state.claim == ClaimState::Unclaimed);
    SetMobOutline(s, Category::Placeholder, OutlineMode::ByNature);
    t.Update({Mob(0x2000, kCarrionWorm, "Carrion Worm"), Mob(0x4000, kPoisonFunguar, "Poison Funguar")}, kLevel20, s, CursorTargets{});
    CHECK(!t.Find(0x2000)->outline); // passive, so not outlined
    CHECK(t.Find(0x4000)->outline && t.Find(0x4000)->argb == Aggression(s, Nature::Aggressive)); // the Will Attack row's color
    SetMobOutline(s, Category::Placeholder, OutlineMode::Disabled);
    t.Update({Mob(0x2000, kCarrionWorm, "Carrion Worm"), Mob(0x4000, kPoisonFunguar, "Poison Funguar")}, kLevel20, s, CursorTargets{});
    CHECK(!t.Find(0x2000)->outline && !t.Find(0x4000)->outline); // a placeholder takes its own row, aggressive or not
}

TEST(a_claimed_mob_takes_its_claim_rows_mode_and_color)
{
    Tracker t;
    Settings s;
    ActorInput ours   = Mob(0x2000, kBountyHunter, "Goblin Bounty Hunter");
    ours.claimed      = true;
    ours.claimedByParty = true;
    ActorInput theirs = Mob(0x3000, kBountyHunter, "Goblin Bounty Hunter");
    theirs.claimed    = true;
    SetMobOutline(s, Category::PartyClaim, OutlineMode::Disabled);
    SetMobOutline(s, Category::OtherClaim, OutlineMode::Disabled);
    t.Update({ours, theirs}, kLevel20, s, CursorTargets{});
    CHECK(!t.Find(0x2000)->outline && !t.Find(0x3000)->outline); // the claim rows decide, not the aggression rows
    for (const Category claim : {Category::PartyClaim, Category::OtherClaim})
    {
        SetMobOutline(s, claim, OutlineMode::Shown);
        s.Kind(PlateKind::Mobs).rows[CategoryIndex(claim)].outlineColor.source = ColorSource::Claim;
    }
    t.Update({ours, theirs}, kLevel20, s, CursorTargets{});
    CHECK(t.Find(0x2000)->outline && t.Find(0x2000)->argb == ToArgb(s.palette.claim[static_cast<int>(ClaimState::Yours)]));
    CHECK(t.Find(0x3000)->outline && t.Find(0x3000)->argb == ToArgb(s.palette.claim[static_cast<int>(ClaimState::Others)]));
}

TEST(a_mob_whose_server_id_the_data_lacks_is_unknown)
{
    Tracker t;
    const Settings s = EveryCategory();
    t.Update({Mob(0x2000, 1, "Goblin Bounty Hunter")}, kLevel20, s, CursorTargets{}); // a known name is not enough
    CHECK(t.Find(0x2000)->mob == nullptr);
    CHECK_EQ(t.Find(0x2000)->argb, Aggression(s, Nature::NoData));
}

TEST(mobs_beyond_max_distance_are_not_outlined)
{
    Tracker t;
    const Settings s;
    t.Update({BountyHunter(0x2000, s.maxDistance), BountyHunter(0x3000, s.maxDistance + 0.5f)}, kLevel20, s, CursorTargets{});
    CHECK(t.Find(0x2000)->outline);
    CHECK(!t.Find(0x3000)->outline);
}

TEST(a_mob_that_cannot_be_targeted_is_not_outlined)
{
    Tracker t;
    const Settings s;
    ActorInput hidden = BountyHunter(0x3000);
    hidden.untargetable = true; // as a Chigoe is until it engages
    t.Update({BountyHunter(0x2000), hidden}, kLevel20, s, CursorTargets{});
    CHECK(t.Find(0x2000)->outline);
    CHECK(!t.Find(0x3000)->outline);
}

TEST(an_outline_is_as_opaque_as_the_setting_and_fainter_with_distance_only_when_it_fades_in)
{
    Tracker t;
    Settings s;
    const uint32_t color = ToArgb(s.palette.aggression[static_cast<int>(Nature::Aggressive)]);
    t.Update({BountyHunter(0x2000, 8.0f), BountyHunter(0x3000, 30.0f)}, kLevel20, s, CursorTargets{});
    CHECK_EQ(t.Find(0x2000)->outlineAlpha, 153); // 60 percent, a new install's
    CHECK_EQ(t.Find(0x3000)->outlineAlpha, 153);
    CHECK_EQ(t.Find(0x3000)->argb, color); // the color itself stays whole
    CHECK_EQ(t.Find(0x3000)->distance, 30.0f); // which a nameplate that fades in reads too
    s.outlineOpacity = 100;
    s.outlineFadeIn  = true;
    t.Update({BountyHunter(0x2000, 8.0f), BountyHunter(0x3000, 30.0f), BountyHunter(0x4000, s.maxDistance)}, kLevel20, s,
        CursorTargets{});
    CHECK_EQ(t.Find(0x2000)->outlineAlpha, 255);
    CHECK_EQ(t.Find(0x3000)->outlineAlpha, 129); // half way from 10 yalms to the range of 50
    CHECK_EQ(t.Find(0x4000)->outlineAlpha, 3);   // a hundredth, at the range
    s.maxDistance = 30.0f;
    t.Update({BountyHunter(0x3000, 20.0f)}, kLevel20, s, CursorTargets{});
    CHECK_EQ(t.Find(0x3000)->outlineAlpha, 129); // the fade ends at the range, wherever that is set
}

TEST(disabled_outlines_nothing)
{
    Tracker t;
    Settings s;
    s.enabled = false;
    t.Update({BountyHunter(0x2000)}, kLevel20, s, CursorTargets{});
    CHECK(!t.Find(0x2000)->outline);
}

TEST(stencil_refs_stay_between_1_and_255)
{
    std::vector<ActorInput> mobs;
    for (ActorPtr i = 0; i < 300; ++i)
        mobs.push_back(BountyHunter(0x10000 + i * 0x10));
    Tracker t;
    t.Update(mobs, kLevel20, Settings{}, CursorTargets{});
    CHECK_EQ(t.Find(0x10000)->stencilRef, 1);
    CHECK_EQ(t.Find(0x10000 + 254 * 0x10)->stencilRef, 255); // an 8-bit stencil's largest value
    CHECK_EQ(t.Find(0x10000 + 255 * 0x10)->stencilRef, 1);
}

TEST(unknown_pointers_are_not_found)
{
    Tracker t;
    t.Update({Player(0x1000), BountyHunter(0x2000)}, kLevel20, Settings{}, CursorTargets{});
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
    t.Update({input}, kLevel20, s, CursorTargets{});
    CHECK_EQ(t.Find(0x2000)->level.text, "Lv 10 TW");
    CHECK_EQ(t.Find(0x2000)->argb, Aggression(s, Nature::Passive));
}

TEST(outlined_count_includes_only_outlined_mobs)
{
    Tracker t;
    Settings s = EveryCategory();
    SetMobOutline(s, Category::WontAttack, OutlineMode::Disabled);
    t.Update({Player(0x1000), BountyHunter(0x2000), Mob(0x3000, kSnipper, "Snipper"), BountyHunter(0x4000, s.maxDistance + 10.0f)},
        kLevel20, s, CursorTargets{});
    CHECK_EQ(t.OutlinedCount(), 1u);
    CHECK(t.Find(0x2000)->outline);
}

TEST(every_mob_gets_nameplate_data_at_any_distance)
{
    // Outlines stop at the max distance and hidden categories; nameplates do not.
    Tracker t;
    Settings s; // passive mobs are not outlined by default
    t.Update({Player(0x1000), BountyHunter(0x2000, 100.0f), Mob(0x3000, kSnipper, "Snipper")}, kLevel20, s, CursorTargets{});
    const ActorInfo* far = t.Find(0x2000);
    CHECK(far->kind == EntityKind::Mob && far->alive && !far->outline);
    CHECK_EQ(far->index, 0x220);
    CHECK(std::string(far->name) == "Goblin Bounty Hunter");
    CHECK(std::string(far->level.text) == "Lv 17-20 EP-EM");
    CHECK(far->mob != nullptr && far->mob->id == kBountyHunter);
    CHECK(far->state.row == Category::WillAttack); // its name still glows in the color it would be outlined in
    const ActorInfo* hidden = t.Find(0x3000);
    CHECK(!hidden->outline);
    CHECK(std::string(hidden->level.text) == "Lv 19-20 DC-EM");
    CHECK(hidden->mob != nullptr && hidden->mob->id == kSnipper);
    CHECK(hidden->state.row == Category::WontAttack);
    CHECK(t.Actors() == (std::vector<ActorPtr>{0x1000, 0x2000, 0x3000})); // in entity order
}

TEST(dead_mobs_keep_only_their_name)
{
    Tracker t;
    t.Update({BountyHunter(0x2000, 10.0f, false)}, kLevel20, Settings{}, CursorTargets{});
    const ActorInfo* dead = t.Find(0x2000);
    CHECK(!dead->outline);
    CHECK(std::string(dead->name) == "Goblin Bounty Hunter");
    CHECK(dead->level.text[0] == '\0');
}

// The target's popup reads a dead mob's record; only its level goes.
TEST(a_dead_mob_keeps_its_record_for_the_popup)
{
    Tracker t;
    t.Update({BountyHunter(0x2000, 10.0f, false)}, kLevel20, Settings{}, CursorTargets{});
    const ActorInfo* dead = t.Find(0x2000);
    CHECK(dead->mob != nullptr && dead->mob->id == kBountyHunter);
    CHECK(dead->level.text[0] == '\0');
}

TEST(squared_distance_becomes_yalms_and_keeps_nan)
{
    // Ashita reports squared distances. A NaN must stay NaN so no distance limit passes it; std::max(0, NaN) is 0.
    CHECK_EQ(DistanceFromSquared(16.0f), 4.0f);
    CHECK_EQ(DistanceFromSquared(-1.0f), 0.0f);
    CHECK(std::isnan(DistanceFromSquared(std::nanf(""))));
}

TEST(you_are_in_combat_with_your_weapon_out_or_while_your_party_holds_a_living_mobs_claim)
{
    ActorInput self = Player(0x100);
    self.self       = true;
    ActorInput mob  = BountyHunter(0x200);
    CHECK(!PlayerInCombat(std::vector{self, mob}));
    self.engaged = true;
    CHECK(PlayerInCombat(std::vector{self}));
    // A mage never draws a weapon: the party's claim on a mob is the fight.
    self.engaged       = false;
    mob.claimed        = true;
    mob.claimedByParty = true;
    CHECK(PlayerInCombat(std::vector{self, mob}));
    ActorInput dead = mob;
    dead.alive      = false;
    CHECK(!PlayerInCombat(std::vector{self, dead}));
    ActorInput theirs     = mob;
    theirs.claimedByParty = false;
    CHECK(!PlayerInCombat(std::vector{self, theirs}));
    // Someone else's weapon, or a pet's, is not yours.
    ActorInput other = Player(0x300);
    other.engaged    = true;
    ActorInput pet   = BountyHunter(0x400);
    pet.pet          = true;
    pet.engaged      = true;
    CHECK(!PlayerInCombat(std::vector{self, other, pet}));
}

TEST(a_claim_by_you_or_your_party_is_told_from_anyone_elses)
{
    // A mob's claim is its claimer's whole server ID, and 0 while nobody holds it: 65001 while the test character
    // fought a Land Crab, 0 four seconds after it stopped.
    const std::vector<uint32_t> party{65001, 0x01098ABC};
    CHECK(IsClaimed(65001) && IsClaimed(0x01098ABC) && !IsClaimed(0));
    CHECK(ClaimedByParty(65001, party));       // you, with an ID that fits in 16 bits
    CHECK(ClaimedByParty(0x01098ABC, party));  // a party member
    CHECK(!ClaimedByParty(0x00008ABC, party)); // someone whose ID only ends as a member's does
    CHECK(!ClaimedByParty(0, party));
    CHECK(!ClaimedByParty(65001, {}));
}

TEST(a_mob_carries_its_server_id_record_and_level_line_and_is_found_by_target_index)
{
    Tracker t;
    t.Update({BountyHunter(0x2000)}, kLevel20, Settings{}, CursorTargets{});
    const ActorInfo* info = t.FindIndex(BountyHunter(0x2000).index);
    CHECK(info != nullptr && info == t.Find(0x2000));
    CHECK_EQ(info->serverId, kBountyHunter);
    CHECK(info->mob != nullptr && info->mob->id == kBountyHunter);
    CHECK(std::string(info->level.text).rfind("Lv ", 0) == 0);
    CHECK(t.FindIndex(0) == nullptr);
}

TEST(an_actor_listed_twice_keeps_only_what_its_later_entry_says)
{
    Tracker t;
    t.Update({BountyHunter(0x2000), Npc(0x2000)}, kLevel20, EveryCategory(), CursorTargets{});
    const ActorInfo* info = t.Find(0x2000);
    CHECK(info != nullptr && info->kind == EntityKind::Npc && std::string(info->name) == "Home Point #1");
    CHECK(!info->outline && info->mob == nullptr && info->level.text[0] == '\0');
    CHECK(t.FindIndex(BountyHunter(0x2000).index) == nullptr);
}

TEST(the_tracker_fills_each_entitys_state)
{
    // The file's helpers: BountyHunter (Goblin Bounty Hunter, aggressive, 17-20), Mob with kSnipper (passive,
    // 19-20), Player (index 1052), kLevel20. At level 20 the Bounty Hunter cons EP-EM, so its toughness is Even Match.
    ActorInput ours     = BountyHunter(0x2000);
    ours.claimedByParty = true;
    ours.claimed        = true;
    ActorInput pet      = Mob(0x3000, kSnipper, "Snipper");
    pet.pet             = true;
    ActorInput member   = Player(0x4000);
    member.party        = true;
    member.jobs         = PlayerJobs{1, 75, 2, 37};
    member.title        = 266;
    ActorInput you      = Player(0x5000);
    you.index           = 1053;
    you.self            = true;
    you.engaged         = true;
    CursorTargets targets;
    targets.target = ours.index;
    Tracker t;
    t.Update({ours, pet, member, you}, kLevel20, Settings{}, targets);
    const ActorInfo* mob = t.Find(0x2000);
    CHECK(mob->state.kind == EntityKind::Mob);
    CHECK(mob->state.nature == Nature::Aggressive);
    CHECK(mob->state.row == Category::PartyClaim);
    CHECK(mob->state.claim == ClaimState::Yours);
    CHECK(mob->state.target == TargetState::Targeted);
    CHECK(mob->state.toughness == LabelShade::EvenMatch);
    CHECK(!mob->checked);
    CHECK(t.Find(0x3000)->state.pet);
    CHECK(t.Find(0x3000)->state.claim == ClaimState::Unclaimed);
    CHECK(PlateKindOf(t.Find(0x3000)->state) == PlateKind::Pets);
    CHECK(t.Find(0x4000)->state.playerRow == PlayerRow::PartyMember);
    CHECK(t.Find(0x4000)->jobs == (PlayerJobs{1, 75, 2, 37}));
    CHECK_EQ(t.Find(0x4000)->title, 266u);
    CHECK(t.Find(0x4000)->state.target == TargetState::None);
    CHECK(t.Find(0x5000)->state.self && t.Find(0x5000)->state.engaged);
    CHECK(PlateKindOf(t.Find(0x5000)->state) == PlateKind::Self);
}

TEST(the_tracker_fills_claims_rows_placeholders_checks_and_dead_mobs)
{
    ActorInput friendly = Player(0x1000);
    friendly.isFriend   = true;
    ActorInput shared   = Player(0x1100);
    shared.linkshell    = true;
    ActorInput trader   = Player(0x1200);
    trader.status       = PlayerStatus{};
    trader.status->bazaar = true;
    ActorInput others   = BountyHunter(0x2000);
    others.claimed      = true;
    const CheckResult result{18, Con::DecentChallenge};
    ActorInput checkedMob = Mob(0x3000, kBountyHunter + 1, "Goblin Bounty Hunter");
    checkedMob.serverId   = kBountyHunter;
    checkedMob.index      = 2001;
    checkedMob.checked    = &result;
    ActorInput worm       = Mob(0x4000, kCarrionWorm, "Carrion Worm");
    ActorInput dead       = BountyHunter(0x5000, 10.0f, false);
    dead.index            = 2002;
    ActorInput pet        = Mob(0x6000, kSnipper, "Snipper");
    pet.pet               = true;
    pet.ownerName         = "Carrott";
    Tracker t;
    t.Update({friendly, shared, trader, others, checkedMob, worm, dead, pet}, kLevel20, Settings{}, CursorTargets{});
    CHECK(t.Find(0x1000)->state.playerRow == PlayerRow::Friend);
    CHECK(t.Find(0x1100)->state.playerRow == PlayerRow::LinkshellMember);
    CHECK(t.Find(0x1200)->status && t.Find(0x1200)->status->bazaar);
    CHECK(t.Find(0x2000)->state.claim == ClaimState::Others);
    CHECK(t.Find(0x2000)->state.row == Category::OtherClaim);
    CHECK(t.Find(0x4000)->placeholder);
    CHECK(t.Find(0x4000)->state.nature == Nature::Placeholder);
    CHECK(t.Find(0x4000)->state.row == Category::Placeholder);
    CHECK(t.Find(0x4000)->state.tooWeak);
    CHECK(t.Find(0x4000)->state.toughness == LabelShade::TooWeak);
    CHECK(t.Find(0x3000)->checked && t.Find(0x3000)->checked->level == 18);
    CHECK(t.Find(0x3000)->state.toughness == LabelShade::DecentChallenge);
    CHECK(!t.Find(0x5000)->state.alive);
    CHECK(t.Find(0x5000)->state.nature == Nature::Aggressive);
    CHECK(t.Find(0x5000)->state.row == Category::Dead);
    CHECK(!t.Find(0x5000)->state.tooWeak);
    CHECK(t.Find(0x5000)->state.toughness == LabelShade::Unknown);
    CHECK(t.Find(0x6000)->state.pet);
    CHECK(std::string(t.Find(0x6000)->ownerName) == "Carrott");
}

TEST(an_outline_follows_its_rows_mode_and_color_pick)
{
    Settings s;
    KindSettings& mobs = s.Kind(PlateKind::Mobs);
    mobs.rows[CategoryIndex(Category::WontAttack)].outline      = OutlineMode::WhenTargeted;
    mobs.rows[CategoryIndex(Category::WontAttack)].outlineColor = ColorPick{ColorSource::Custom, {{0.1f, 0.2f, 0.3f}}};
    s.Kind(PlateKind::Players).rows[static_cast<int>(PlayerRow::Player)].outline      = OutlineMode::Shown;
    s.Kind(PlateKind::Players).rows[static_cast<int>(PlayerRow::Player)].outlineColor = ColorPick{ColorSource::Type};
    const ActorInput aggressive = BountyHunter(0x2000);
    const ActorInput passive    = Mob(0x3000, kSnipper, "Snipper");
    const ActorInput player     = Player(0x4000);
    CursorTargets targets;
    targets.target = passive.index;
    Tracker t;
    t.Update({aggressive, passive, player}, kLevel20, s, targets);
    CHECK(t.Find(0x2000)->outline);
    CHECK_EQ(t.Find(0x2000)->argb, ToArgb(s.palette.aggression[static_cast<int>(Nature::Aggressive)]));
    CHECK(t.Find(0x3000)->outline); // targeted
    CHECK_EQ(t.Find(0x3000)->argb, ToArgb(Color{{0.1f, 0.2f, 0.3f}}));
    CHECK(t.Find(0x4000)->outline);
    CHECK_EQ(t.Find(0x4000)->argb, ToArgb(s.palette.type[static_cast<int>(TargetType::Player)]));
    CHECK_EQ(t.OutlinedCount(), 3u);
    t.Update({aggressive, passive, player}, kLevel20, s, CursorTargets{});
    CHECK(!t.Find(0x3000)->outline);
    CHECK_EQ(RowColor(t.Find(0x3000)->state, s), ToArgb(Color{{0.1f, 0.2f, 0.3f}})); // its row's color, outlined or not
    CHECK_EQ(t.OutlinedCount(), 2u);
    s.outlines = false;
    t.Update({aggressive, passive, player}, kLevel20, s, CursorTargets{});
    CHECK_EQ(t.OutlinedCount(), 0u);
}

TEST(a_dead_far_or_disabled_row_mob_gets_no_outline)
{
    const Settings s;
    Tracker t;
    t.Update({BountyHunter(0x2000, 10.0f, false), BountyHunter(0x3000, s.maxDistance + 1.0f), Mob(0x4000, kSnipper, "Snipper")},
        kLevel20, s, CursorTargets{});
    CHECK(!t.Find(0x2000)->outline); // dead, and the Dead row has no outline by default
    CHECK(!t.Find(0x3000)->outline); // past the range
    CHECK(!t.Find(0x4000)->outline); // the Passive row is Disabled by default
    CHECK_EQ(t.OutlinedCount(), 0u);
    // The Dead row can give a dead mob one, in the color its pick takes from what the mob was.
    Settings marked = s;
    SetMobOutline(marked, Category::Dead, OutlineMode::Shown);
    t.Update({BountyHunter(0x2000, 10.0f, false)}, kLevel20, marked, CursorTargets{});
    CHECK(t.Find(0x2000)->outline);
    CHECK_EQ(t.Find(0x2000)->argb, ToArgb(marked.palette.aggression[static_cast<int>(Nature::Aggressive)]));
    // A dead player gets none, whatever the players' rows say.
    marked.Kind(PlateKind::Players).rows[static_cast<int>(PlayerRow::Player)].outline = OutlineMode::Shown;
    ActorInput fallen = Player(0x5000);
    fallen.self       = false;
    fallen.alive      = false;
    t.Update({fallen}, kLevel20, marked, CursorTargets{});
    CHECK(!t.Find(0x5000)->outline);
    // Nor does a dead pet, which is a mob to the game and goes by the pets' rows: those have none for the dead.
    marked.Kind(PlateKind::Pets).rows[static_cast<int>(PetRow::Yours)].outline = OutlineMode::Shown;
    ActorInput pet = Mob(0x6000, kSnipper, "Snipper");
    pet.pet        = true;
    pet.petRow     = PetRow::Yours;
    t.Update({pet}, kLevel20, marked, CursorTargets{});
    CHECK(t.Find(0x6000)->outline);
    pet.alive = false;
    t.Update({pet}, kLevel20, marked, CursorTargets{});
    CHECK(!t.Find(0x6000)->outline);
}

namespace
{
    ActorInput Claimed(ActorInput mob, bool byParty)
    {
        mob.claimed        = true;
        mob.claimedByParty = byParty;
        return mob;
    }

    const uint32_t kAggressiveArgb = ToArgb(Settings{}.palette.aggression[static_cast<int>(Nature::Aggressive)]);
    const uint32_t kPlaceholderArgb = ToArgb(Settings{}.palette.aggression[static_cast<int>(Nature::Placeholder)]);
}

TEST(a_claimed_mob_keeps_the_outline_its_nature_gives_it_by_default)
{
    const Settings s;
    Tracker t;
    t.Update({Claimed(BountyHunter(0x2000), true), Claimed(Mob(0x3000, kSnipper, "Snipper"), true),
                 Claimed(BountyHunter(0x4000), false), Claimed(Mob(0x5000, kCarrionWorm, "Carrion Worm"), true)},
        kLevel20, s, CursorTargets{});
    CHECK(t.Find(0x2000)->outline && t.Find(0x2000)->argb == kAggressiveArgb);
    CHECK(!t.Find(0x3000)->outline);
    CHECK(t.Find(0x4000)->outline && t.Find(0x4000)->argb == kAggressiveArgb);
    CHECK(t.Find(0x5000)->outline && t.Find(0x5000)->argb == kPlaceholderArgb);
}

TEST(a_claim_row_set_to_shown_or_disabled_decides_for_the_claimed_mob)
{
    Settings s;
    SetMobOutline(s, Category::PartyClaim, OutlineMode::Shown);
    Tracker t;
    const ActorInput snipper = Claimed(Mob(0x3000, kSnipper, "Snipper"), true);
    t.Update({snipper}, kLevel20, s, CursorTargets{});
    CHECK(t.Find(0x3000)->outline);
    CHECK_EQ(t.Find(0x3000)->argb, ToArgb(s.palette.claim[static_cast<int>(ClaimState::Yours)]));
    SetMobOutline(s, Category::PartyClaim, OutlineMode::Disabled);
    t.Update({Claimed(BountyHunter(0x2000), true)}, kLevel20, s, CursorTargets{});
    CHECK(!t.Find(0x2000)->outline);
}

TEST(a_placeholder_row_by_nature_leaves_the_outline_to_the_mobs_aggression)
{
    Settings s;
    SetMobOutline(s, Category::Placeholder, OutlineMode::ByNature);
    Tracker t;
    t.Update({Mob(0x2000, kCarrionWorm, "Carrion Worm"), Mob(0x3000, kPoisonFunguar, "Poison Funguar")}, kLevel20, s, CursorTargets{});
    CHECK(!t.Find(0x2000)->outline); // passive, and the Passive row is Disabled
    CHECK(t.Find(0x3000)->outline && t.Find(0x3000)->argb == kAggressiveArgb);
    SetMobOutline(s, Category::PartyClaim, OutlineMode::ByNature);
    t.Update({Claimed(Mob(0x3000, kPoisonFunguar, "Poison Funguar"), true)}, kLevel20, s, CursorTargets{});
    CHECK(t.Find(0x3000)->outline && t.Find(0x3000)->argb == kAggressiveArgb);
}

TEST(a_claim_row_by_nature_follows_the_aggressive_rows_when_targeted_mode)
{
    Settings s;
    SetMobOutline(s, Category::PartyClaim, OutlineMode::ByNature);
    SetMobOutline(s, Category::WillAttack, OutlineMode::WhenTargeted);
    const ActorInput hunter = Claimed(BountyHunter(0x2000), true);
    CursorTargets targets;
    targets.target = hunter.index;
    Tracker t;
    t.Update({hunter}, kLevel20, s, targets);
    CHECK(t.Find(0x2000)->outline);
    t.Update({hunter}, kLevel20, s, CursorTargets{});
    CHECK(!t.Find(0x2000)->outline);
}

TEST(a_mob_that_is_someones_pet_is_outlined_by_the_pets_row_alone)
{
    ActorInput pet = BountyHunter(0x2000);
    pet.pet        = true;
    Settings s;
    Tracker t;
    t.Update({pet}, kLevel20, s, CursorTargets{});
    CHECK(!t.Find(0x2000)->outline); // the Pets rows are Disabled by default, whatever its aggression
    // A party member's pet goes by the Party Pets row, and no other.
    pet.petRow = PetRow::Party;
    s.Kind(PlateKind::Pets).rows[static_cast<int>(PetRow::Others)].outline = OutlineMode::Shown;
    t.Update({pet}, kLevel20, s, CursorTargets{});
    CHECK(!t.Find(0x2000)->outline && t.Find(0x2000)->state.petRow == PetRow::Party);
    const int partyPets = static_cast<int>(PetRow::Party);
    s.Kind(PlateKind::Pets).rows[partyPets].outline      = OutlineMode::Shown;
    s.Kind(PlateKind::Pets).rows[partyPets].outlineColor = ColorPick{ColorSource::Custom, {{0.7f, 0.6f, 0.5f}}};
    for (const Category claim : {Category::PartyClaim, Category::OtherClaim})
        SetMobOutline(s, claim, OutlineMode::Shown);
    SetMobOutline(s, Category::WillAttack, OutlineMode::Disabled);
    t.Update({Claimed(pet, true)}, kLevel20, s, CursorTargets{});
    CHECK(t.Find(0x2000)->outline);
    CHECK_EQ(t.Find(0x2000)->argb, ToArgb(Color{{0.7f, 0.6f, 0.5f}}));
}
