#include "examine.h"
#include "test.h"

#include <cmath>
#include <cstring>
#include <vector>

using namespace headsup;

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

TEST(a_result_lasts_the_respawn_time_or_ten_minutes)
{
    const MobRecord timed = Mob(kMobAggressive, 17, 20, 330);
    CHECK_EQ(CheckLifetime(&timed), 330.0);
    const MobRecord scripted = Mob(kMobAggressive, 17, 20, 0);
    CHECK_EQ(CheckLifetime(&scripted), 600.0);
    CHECK_EQ(CheckLifetime(nullptr), 600.0);
}

TEST(a_check_you_make_is_kept_for_its_lifetime)
{
    CheckResults results;
    results.Received(Gauged(kMob, 22, Con::DecentChallenge), 300.0, 100.0);
    const CheckResult* r = results.Result(kMob, 399.0);
    CHECK(r != nullptr);
    CHECK_EQ(r->level, 22);
    CHECK(r->con == Con::DecentChallenge);
    CHECK(results.Result(kOther, 200.0) == nullptr);
    CHECK(results.Result(kMob, 400.0) == nullptr);
}

TEST(impossible_to_gauge_keeps_no_result)
{
    CheckResults results;
    results.Received(CheckReply{kMob, 0x220, false, {}}, 600.0, 0.0);
    CHECK(results.Result(kMob, 1.0) == nullptr);
}

TEST(death_forgets_the_result)
{
    CheckResults results;
    results.Received(Gauged(kMob, 22, Con::DecentChallenge), 300.0, 0.0);
    results.Forget(kMob);
    CHECK(results.Result(kMob, 1.0) == nullptr);
}
