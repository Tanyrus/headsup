#pragma once

#include "con.h"
#include "mobdata.h"

#include <cstdint>
#include <optional>
#include <unordered_map>

namespace headsup
{
    constexpr uint16_t kCheckReplyPacket = 0x029; // server to client: Message Basic
    constexpr double kDefaultCheckLifetime = 600.0; // seconds, when the data has no respawn time

    // An incoming 0x029 (Message Basic) packet that answers a /check.
    struct CheckReply
    {
        uint32_t serverId;    // the checked mob
        uint16_t targetIndex;
        bool gauged;          // false for "impossible to gauge" (notorious monsters, battlefields)
        CheckResult result;   // set when gauged
    };
    std::optional<CheckReply> ParseCheckReply(const uint8_t* data, uint32_t size);

    // How long a check result stays valid: the mob's respawn time, after which a new spawn may have another level,
    // else kDefaultCheckLifetime.
    double CheckLifetime(const MobRecord* mob);

    // The latest result of the player's own /check of each spawn, keyed by server ID.
    class CheckResults
    {
    public:
        void Received(const CheckReply& reply, double lifetime, double now);
        // The latest result for this spawn while it is valid, else nullptr.
        const CheckResult* Result(uint32_t serverId, double now) const;
        // The mob was seen dead: its next spawn rolls a new level.
        void Forget(uint32_t serverId);

    private:
        struct Stored
        {
            CheckResult result;
            double expires;
        };
        std::unordered_map<uint32_t, Stored> m_Results;
    };
}
