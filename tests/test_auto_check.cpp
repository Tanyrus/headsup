#include "auto_check.h"
#include "test.h"

#include <cstdint>
#include <vector>

using namespace headsup;

namespace
{
    constexpr uint32_t kCrab = 17186819, kWorm = 17186820, kHare = 17186821;

    std::vector<uint8_t> Bytes(const char* hex)
    {
        std::vector<uint8_t> bytes;
        for (const char* at = hex; at[0] != '\0' && at[1] != '\0'; at += 2)
        {
            const auto digit = [](char c) { return static_cast<uint8_t>(c <= '9' ? c - '0' : c - 'a' + 10); };
            bytes.push_back(static_cast<uint8_t>(digit(at[0]) << 4 | digit(at[1])));
        }
        return bytes;
    }

    CheckCandidate Mob(uint32_t serverId, float distance, bool known = false)
    {
        CheckCandidate mob;
        mob.serverId = serverId;
        mob.index    = static_cast<uint16_t>(serverId & 0xFFF);
        mob.kind     = EntityKind::Mob;
        mob.distance = distance;
        mob.alive    = true;
        mob.known    = known;
        mob.name     = "Land Crab";
        return mob;
    }
    // The stock checker addon's line for an answer, built as its checker.lua builds it from Ashita's chat library:
    // the header, the name, an arrow, the level in brackets, the con, and the defense remark in brackets.
    const std::string kCheckerStart = "\x1E\x51[\x1E\x06" "checker\x1E\x51]\x1E\x01 \x1E\x6ALand Crab\x1E\x01 "
                                      "\x1E\x52\x81\xA8\x1E\x01 ";
    const std::string kCheckerLine = kCheckerStart + "\x1E\x51(Lv. \x1E\x52" "5\x1E\x01\x1E\x51)\x1E\x01 "
                                     "\x1E\x43too weak to be worthwhile\x1E\x01 "
                                     "\x1E\x51(\x1E\x6AHigh Defense\x1E\x01\x1E\x51)\x1E\x01";
    // And for an answer the server would not gauge.
    const std::string kCheckerUngauged = kCheckerStart + "\x1E\x51(Lv. \x1E\x52???\x1E\x01\x1E\x51)\x1E\x01 "
                                         "\x1E\x05Impossible to gauge!\x1E\x01";
}

// What the client sent for /check on a Land Crab (index 3) and what Phoenix answered, as captured on the test
// server on 2026-10-13.
TEST(a_check_is_asked_as_the_client_asks_it)
{
    const std::vector<uint8_t> clients = Bytes("dd086b49034006010300000000808843");
    const auto request                 = CheckRequest(kCrab, 3);
    CHECK_EQ(request.size(), clients.size());
    // The same but for the sync word, which the game fills in, and the three bytes of padding the client leaves
    // whatever they were.
    for (const size_t at : {0u, 1u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u})
        CHECK_EQ(request[at], clients[at]);
    CHECK(request[13] == 0 && request[14] == 0 && request[15] == 0);
    CHECK(CheckRequestTarget(clients.data(), static_cast<uint32_t>(clients.size())) == kCrab);
    CHECK(!CheckRequestTarget(clients.data(), 7));
}

TEST(a_check_is_the_players_own_only_when_the_game_sent_it_and_it_went_out)
{
    const std::vector<uint8_t> clients = Bytes("dd086b49034006010300000000808843");
    const auto size                    = static_cast<uint32_t>(clients.size());
    CHECK(PlayersCheck(kCheckRequestPacket, clients.data(), size, false, false) == kCrab);
    CHECK(!PlayersCheck(kCheckRequestPacket, clients.data(), size, true, false)); // a plugin's, HeadsUp's own among them
    CHECK(!PlayersCheck(kCheckRequestPacket, clients.data(), size, false, true)); // one that never left
    CHECK(!PlayersCheck(0x0DE, clients.data(), size, false, false));              // another packet
    CHECK(!PlayersCheck(kCheckRequestPacket, clients.data(), 7, false, false));
}

TEST(only_a_living_mob_that_is_nobodys_pet_is_ever_asked_about)
{
    // The server tells a player who examined them: nothing but a mob may ever be asked about, whatever is handed in.
    CheckCandidate player = Mob(65002, 1.0f);
    player.kind           = EntityKind::Player;
    player.index          = 0x401;
    CheckCandidate npc = Mob(kWorm, 1.5f);
    npc.kind           = EntityKind::Npc;
    CheckCandidate pet = Mob(kHare, 2.0f);
    pet.pet            = true;
    // An entity read as a mob that carries a player's id, or stands at a player's index.
    CheckCandidate playersId    = Mob(65003, 2.5f);
    CheckCandidate playersIndex = Mob(kCrab + 1, 2.6f);
    playersIndex.index          = 0x402;
    AutoCheck checks;
    CHECK(!checks.Due({{player, npc, pet, playersId, playersIndex}}, 10.0));
    const std::optional<CheckCandidate> due = checks.Due({{player, npc, pet, playersId, playersIndex, Mob(kCrab, 30.0f)}}, 10.0);
    CHECK(due && due->serverId == kCrab);
}

TEST(a_mob_the_player_has_just_checked_is_not_checked_again_by_headsup)
{
    AutoCheck checks;
    const std::vector<CheckCandidate> mobs{Mob(kCrab, 5.0f)};
    checks.PlayerAsked(kCrab, 10.0);
    CHECK(!checks.Due(mobs, 10.1)); // the answer on its way is the player's: a second would be read twice
    // An answer that never came leaves the mob to HeadsUp.
    CHECK(checks.Due(mobs, 10.0 + kOwnAnswerWait + 0.1).has_value());
}

TEST(a_mob_that_dies_while_headsups_check_is_on_its_way_still_has_the_answer_kept_out)
{
    AutoCheck checks;
    const std::vector<CheckCandidate> mobs{Mob(kCrab, 5.0f)};
    CHECK(checks.Due(mobs, 10.0).has_value());
    checks.Forget(kCrab); // seen dead before the answer came
    CHECK(checks.OwnReport(kCheckerLine, 10.25));
    CHECK(checks.TakeAnswer(CheckAnswer{kCrab, true}, 10.3));
    CHECK(checks.OwnReport(kCheckerLine, 10.35));
    // Its answer in, what comes back is another spawn, asked about as soon as it is up.
    checks.Forget(kCrab);
    CHECK(checks.Due(mobs, 12.0).has_value());
}

TEST(an_answer_to_a_check_names_its_mob_whether_it_could_be_gauged_or_not)
{
    std::vector<uint8_t> answer = Bytes("290e6649e9fd000003400601050000004000000000040300ad000000");
    const auto size             = static_cast<uint32_t>(answer.size());
    CHECK((ParseCheckAnswer(answer.data(), size) == CheckAnswer{kCrab, true}));
    answer[0x18] = 249; // "strength is impossible to gauge", a notorious monster's
    CHECK((ParseCheckAnswer(answer.data(), size) == CheckAnswer{kCrab, false}));
    answer[0x18] = 0x1F; // any other battle message is no answer to a check
    CHECK(!ParseCheckAnswer(answer.data(), size));
    CHECK(!ParseCheckAnswer(answer.data(), 0x10));
    // The nine remarks on defense and evasion are answers, from the first to the last and no further.
    for (const uint8_t message : {uint8_t{0xAA}, uint8_t{0xB2}})
    {
        answer[0x18] = message;
        CHECK((ParseCheckAnswer(answer.data(), size) == CheckAnswer{kCrab, true}));
    }
    for (const uint8_t message : {uint8_t{0xA9}, uint8_t{0xB3}})
    {
        answer[0x18] = message;
        CHECK(!ParseCheckAnswer(answer.data(), size));
    }
}

TEST(the_nearest_mob_in_range_with_no_answer_is_checked_and_no_sooner_than_the_gap_after_the_last)
{
    AutoCheck checks;
    const std::vector<CheckCandidate> mobs{Mob(kWorm, 20.0f), Mob(kCrab, 8.0f), Mob(kHare, 3.0f, true)};
    const std::optional<CheckCandidate> first = checks.Due(mobs, 100.0);
    CHECK(first && first->serverId == kCrab && first->index == (kCrab & 0xFFF)); // the hare's answer is held already
    CHECK(!checks.Due(mobs, 100.0 + kAutoCheckGap / 2.0));
    const std::optional<CheckCandidate> second = checks.Due(mobs, 100.0 + kAutoCheckGap);
    CHECK(second && second->serverId == kWorm);
    // Each is asked once: with no answer yet, neither is asked again for a while.
    CHECK(!checks.Due(mobs, 102.0));
    // No answer at all, a packet lost, and it is asked again later.
    const std::optional<CheckCandidate> again = checks.Due(mobs, 100.0 + kAskAgain + 1.0);
    CHECK(again && again->serverId == kCrab);
}

TEST(what_is_far_dead_or_never_gauged_is_not_asked_about)
{
    AutoCheck checks;
    CheckCandidate far = Mob(kCrab, kAutoCheckRange + 0.5f), dead = Mob(kWorm, 5.0f), named = Mob(kHare, 5.0f);
    dead.alive      = false;
    named.gaugeable = false; // a notorious monster by the data: the server's answer is known
    CHECK(!checks.Due({{far, dead, named}}, 10.0));
    far.distance = kAutoCheckRange;
    CHECK(checks.Due({{far, dead, named}}, 10.0).has_value());
    // One the server would not gauge is left alone from then on, until it has died and come back.
    AutoCheck boss;
    const std::vector<CheckCandidate> mobs{Mob(kCrab, 5.0f)};
    CHECK(boss.Due(mobs, 10.0).has_value());
    CHECK(boss.TakeAnswer(CheckAnswer{kCrab, false}, 10.2));
    CHECK(!boss.Due(mobs, 10.0 + kAskAgain + 5.0));
    boss.Forget(kCrab);
    CHECK(boss.Due(mobs, 10.0 + kAskAgain + 6.0).has_value());
    // In another zone the ids mean other mobs: nothing is remembered.
    boss.TakeAnswer(CheckAnswer{kCrab, false}, 80.0);
    boss.Clear();
    CHECK(boss.Due(mobs, 200.0).has_value());
}

TEST(only_an_answer_headsup_alone_asked_for_is_kept_from_the_chat_log)
{
    AutoCheck checks;
    const std::vector<CheckCandidate> mobs{Mob(kCrab, 5.0f)};
    CHECK(checks.Due(mobs, 10.0).has_value());
    CHECK(checks.TakeAnswer(CheckAnswer{kCrab, true}, 10.3));
    // Once: a second answer about it is somebody else's.
    CHECK(!checks.TakeAnswer(CheckAnswer{kCrab, true}, 10.4));
    // The player's own check is theirs to read, though HeadsUp asked about the same mob a moment before.
    AutoCheck both;
    CHECK(both.Due(mobs, 10.0).has_value());
    both.PlayerAsked(kCrab, 10.1);
    CHECK(!both.TakeAnswer(CheckAnswer{kCrab, true}, 10.3));
    // An answer nobody here asked for, or one that comes long after the asking, is let through.
    AutoCheck none;
    CHECK(!none.TakeAnswer(CheckAnswer{kCrab, true}, 10.0));
    AutoCheck late;
    CHECK(late.Due(mobs, 10.0).has_value());
    CHECK(!late.TakeAnswer(CheckAnswer{kCrab, true}, 10.0 + kOwnAnswerWait + 1.0));
    // A check the player made long before, or of another mob, takes nothing from HeadsUp's own.
    AutoCheck stale;
    stale.PlayerAsked(kCrab, 1.0);
    CHECK(stale.Due(mobs, 10.0).has_value());
    CHECK(stale.TakeAnswer(CheckAnswer{kCrab, true}, 10.2));
    AutoCheck another;
    CHECK(another.Due(mobs, 10.0).has_value());
    another.PlayerAsked(kWorm, 10.1);
    CHECK(another.TakeAnswer(CheckAnswer{kCrab, true}, 10.2));
}

TEST(nothing_is_checked_with_the_setting_off_between_zones_or_in_an_event)
{
    CHECK(MayAutoCheck(true, false, false, 20));
    CHECK(!MayAutoCheck(false, false, false, 20));
    CHECK(!MayAutoCheck(true, true, false, 20));  // zoning
    CHECK(!MayAutoCheck(true, false, true, 20));  // in a cutscene or a dialog: the server refuses a check then
    CHECK(!MayAutoCheck(true, false, false, 0));  // your own level not known yet
}

TEST(the_checker_addons_line_for_a_check_headsup_made_is_kept_out_too)
{
    // The addon writes a line for every answer, whoever asked. It may see the answer before HeadsUp does or after.
    const std::vector<CheckCandidate> mobs{Mob(kCrab, 5.0f)};
    AutoCheck before;
    CHECK(!before.OwnReport(kCheckerLine, 9.0)); // nothing asked: a line like any other
    CHECK(before.Due(mobs, 10.0).has_value());
    CHECK(before.OwnReport(kCheckerLine, 10.2));
    CHECK(before.TakeAnswer(CheckAnswer{kCrab, true}, 10.2));
    CHECK(before.OwnReport(kCheckerLine, 10.25)); // written just after the answer was taken
    CHECK(!before.OwnReport(kCheckerLine, 12.0)); // and no longer: the next such line is of a check somebody made
    // The player's own check of a crab, while HeadsUp asks about another, is theirs to read.
    AutoCheck both;
    CHECK(both.Due(mobs, 10.0).has_value());
    both.PlayerAsked(kWorm, 10.1);
    CHECK(!both.OwnReport(kCheckerLine, 10.2));
    // A line that is not the addon's report, or that names another mob, is let through.
    AutoCheck other;
    CHECK(other.Due(mobs, 10.0).has_value());
    CHECK(!other.OwnReport("Land Crab is no longer stunned.", 10.2));
    CHECK(!other.OwnReport("[distance] Land Crab Lv. 5", 10.2));
    CHECK(!other.OwnReport(kCheckerStart + "widescan cleared", 10.2)); // the addon's, and no report of a level
    std::string worm = kCheckerLine;
    worm.replace(worm.find("Land Crab"), 9, "Carrion Worm");
    CHECK(!other.OwnReport(worm, 10.2));
    // The report of an answer the server would not gauge is the addon's line all the same.
    CHECK(other.OwnReport(kCheckerUngauged, 10.2));
    // An ask that never got its answer does not keep a later line out.
    AutoCheck lost;
    CHECK(lost.Due(mobs, 10.0).has_value());
    CHECK(!lost.OwnReport(kCheckerLine, 10.0 + kOwnAnswerWait + 1.0));
}
