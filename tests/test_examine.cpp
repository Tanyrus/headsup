#include "examine.h"
#include "test.h"

#include <cmath>
#include <cstring>
#include <vector>

using namespace aggroglow;

namespace
{
    constexpr uint32_t kMob   = 0x01067220; // 17199648, Goblin Bounty Hunter (target index 0x220)
    constexpr uint32_t kOther = 0x010670FA; // 17199354

    // A 0x029 packet as the server sends it for a check (0x1C bytes, header included).
    std::vector<uint8_t> Reply(uint32_t serverId, uint16_t index, int32_t level, uint32_t type, uint16_t message)
    {
        std::vector<uint8_t> p(0x1C, 0);
        p[0] = 0x29;
        std::memcpy(&p[0x08], &serverId, 4);
        std::memcpy(&p[0x0C], &level, 4);
        std::memcpy(&p[0x10], &type, 4);
        std::memcpy(&p[0x16], &index, 2);
        std::memcpy(&p[0x18], &message, 2);
        return p;
    }

    MobRecord Mob(uint8_t flags, uint8_t minLevel, uint8_t maxLevel, uint32_t respawn = 300)
    {
        return MobRecord{kMob, 103, "Goblin Bounty Hunter", minLevel, maxLevel, flags, respawn, 0};
    }

    CheckReply Gauged(uint32_t serverId, int level, Con con)
    {
        return CheckReply{serverId, 0x220, true, {level, con}};
    }
}

TEST(check_request_bytes)
{
    const auto p                 = BuildCheckRequest(kMob, 0x220);
    const uint8_t expected[16] = {0, 0, 0, 0, 0x20, 0x72, 0x06, 0x01, 0x20, 0x02, 0, 0, 0, 0, 0, 0};
    CHECK(std::memcmp(p.data(), expected, 16) == 0);
}

TEST(check_reply_is_parsed)
{
    const auto p     = Reply(kMob, 0x220, 22, 0x43, 0xAA);
    const auto reply = ParseCheckReply(p.data(), static_cast<uint32_t>(p.size()));
    CHECK(reply.has_value());
    CHECK_EQ(reply->serverId, kMob);
    CHECK_EQ(reply->targetIndex, 0x220);
    CHECK(reply->gauged);
    CHECK_EQ(reply->result.level, 22);
    CHECK(reply->result.con == Con::DecentChallenge);
    CHECK(ParseCheckReply(Reply(kMob, 1, 30, 0x47, 0xB2).data(), 0x1C)->result.con == Con::IncrediblyTough);
}

TEST(impossible_to_gauge_has_no_result)
{
    const auto p     = Reply(kMob, 0x220, 0, 0, 0xF9);
    const auto reply = ParseCheckReply(p.data(), static_cast<uint32_t>(p.size()));
    CHECK(reply.has_value());
    CHECK(!reply->gauged);
}

TEST(other_packets_are_not_check_replies)
{
    CHECK(!ParseCheckReply(Reply(kMob, 1, 22, 0x43, 0x06).data(), 0x1C).has_value()); // a defeat message
    CHECK(!ParseCheckReply(Reply(kMob, 1, 22, 0x48, 0xAA).data(), 0x1C).has_value()); // check type out of range
    CHECK(!ParseCheckReply(Reply(kMob, 1, 0, 0x43, 0xAA).data(), 0x1C).has_value());   // no level
    CHECK(!ParseCheckReply(Reply(kMob, 1, 22, 0x43, 0xAA).data(), 0x19).has_value()); // too short
    CHECK(!ParseCheckReply(nullptr, 0x1C).has_value());
}

TEST(cooldown_is_the_respawn_time_or_ten_minutes)
{
    const MobRecord timed = Mob(kMobAggressive, 17, 20, 330);
    CHECK_EQ(ExamineCooldown(&timed), 330.0);
    const MobRecord scripted = Mob(kMobAggressive, 17, 20, 0);
    CHECK_EQ(ExamineCooldown(&scripted), 600.0);
    CHECK_EQ(ExamineCooldown(nullptr), 600.0);
}

TEST(eligible_when_it_would_give_exp)
{
    const MobRecord m = Mob(kMobAggressive, 17, 20);
    CHECK(IsExamineEligible(ExamineTarget{&m, true, 10.0f, 20, false}));
    CHECK(IsExamineEligible(ExamineTarget{&m, true, 45.0f, 20, false}));
}

TEST(not_eligible_when_too_weak_far_dead_or_busy)
{
    const MobRecord m = Mob(kMobAggressive, 17, 20);
    CHECK(!IsExamineEligible(ExamineTarget{&m, true, 10.0f, 75, false}));  // Too Weak at 75
    CHECK(!IsExamineEligible(ExamineTarget{&m, true, 45.5f, 20, false}));
    CHECK(!IsExamineEligible(ExamineTarget{&m, true, std::nanf(""), 20, false}));
    CHECK(!IsExamineEligible(ExamineTarget{&m, false, 10.0f, 20, false}));
    CHECK(!IsExamineEligible(ExamineTarget{&m, true, 10.0f, 20, true}));
    CHECK(!IsExamineEligible(ExamineTarget{&m, true, 10.0f, 0, false}));
}

TEST(notorious_and_battlefield_mobs_are_never_examined)
{
    const MobRecord nm = Mob(kMobAggressive | kMobNotorious, 20, 20);
    CHECK(!IsExamineEligible(ExamineTarget{&nm, true, 10.0f, 20, false}));
    const MobRecord bf = Mob(kMobBattlefield, 20, 20);
    CHECK(!IsExamineEligible(ExamineTarget{&bf, true, 10.0f, 20, false}));
}

TEST(unknown_levels_and_missing_data_are_eligible)
{
    const MobRecord scripted = Mob(kMobAggressive, 0, 0);
    CHECK(IsExamineEligible(ExamineTarget{&scripted, true, 10.0f, 75, false}));
    CHECK(IsExamineEligible(ExamineTarget{nullptr, true, 10.0f, 75, false}));
}

TEST(one_check_per_mob_per_cooldown)
{
    Examiner e;
    CHECK(e.CanSend(kMob, 0.0));
    e.Sent(kMob, 300.0, 0.0);
    CHECK(!e.CanSend(kMob, 2.0));
    CHECK(!e.CanSend(kMob, 299.9));
    CHECK(e.CanSend(kMob, 300.0));
}

TEST(at_most_one_automatic_check_per_second)
{
    Examiner e;
    e.Sent(kMob, 300.0, 10.0);
    CHECK(!e.CanSend(kOther, 10.5));
    CHECK(e.CanSend(kOther, 11.0));
}

TEST(reply_to_the_pending_check_is_hidden_once)
{
    Examiner e;
    e.Sent(kMob, 300.0, 0.0);
    CHECK(e.Received(Gauged(kMob, 22, Con::DecentChallenge), 300.0, 1.0));
    CHECK(!e.Received(Gauged(kMob, 22, Con::DecentChallenge), 300.0, 1.5)); // a manual check right after
}

TEST(late_or_unrelated_replies_are_shown)
{
    Examiner e;
    e.Sent(kMob, 300.0, 0.0);
    CHECK(!e.Received(Gauged(kOther, 22, Con::DecentChallenge), 300.0, 1.0));
    CHECK(!e.Received(Gauged(kMob, 22, Con::DecentChallenge), 300.0, 3.5));
}

TEST(impossible_to_gauge_answers_the_check_without_a_result)
{
    Examiner e;
    e.Sent(kMob, 600.0, 0.0);
    CHECK(e.Received(CheckReply{kMob, 0x220, false, {}}, 600.0, 0.5));
    CHECK(e.Result(kMob, 1.0) == nullptr);
}

TEST(results_from_any_check_last_their_lifetime)
{
    Examiner e;
    CHECK(!e.Received(Gauged(kMob, 22, Con::DecentChallenge), 300.0, 100.0)); // a manual /check
    const CheckResult* r = e.Result(kMob, 399.0);
    CHECK(r != nullptr);
    CHECK_EQ(r->level, 22);
    CHECK(!e.CanSend(kMob, 200.0)); // already known: no automatic check
    CHECK(e.Result(kMob, 400.0) == nullptr);
    CHECK(e.CanSend(kMob, 400.0));
}

TEST(death_forgets_the_result_but_not_the_cooldown)
{
    Examiner e;
    e.Sent(kMob, 300.0, 0.0);
    e.Received(Gauged(kMob, 22, Con::DecentChallenge), 300.0, 0.5);
    e.Forget(kMob);
    CHECK(e.Result(kMob, 1.0) == nullptr);
    CHECK(!e.CanSend(kMob, 2.0));
}

TEST(squared_distance_becomes_yalms_and_keeps_nan)
{
    // Ashita reports squared distances. A NaN must stay NaN so IsExamineEligible rejects it; std::max(0, NaN) is 0.
    CHECK_EQ(DistanceFromSquared(16.0f), 4.0f);
    CHECK_EQ(DistanceFromSquared(-1.0f), 0.0f);
    CHECK(std::isnan(DistanceFromSquared(std::nanf(""))));
    const MobRecord m = Mob(kMobAggressive, 17, 20);
    CHECK(!IsExamineEligible(ExamineTarget{&m, true, DistanceFromSquared(std::nanf("")), 20, false}));
}
