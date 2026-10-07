#pragma once

#include "con.h"
#include "mobdata.h"

#include <cstdint>
#include <optional>
#include <unordered_map>

namespace headsup
{
    constexpr uint16_t kCheckReplyPacket = 0x029; // server to client: Message Basic

    // An incoming 0x029 (Message Basic) packet that answers a /check with a level and con. Anything else, "impossible
    // to gauge" (notorious monsters, battlefields) included, is nullopt.
    struct CheckReply
    {
        uint32_t serverId;
        uint16_t targetIndex;
        CheckResult result;
    };
    std::optional<CheckReply> ParseCheckReply(const uint8_t* data, uint32_t size);

    // How long a check result stays valid: the mob's respawn time, after which a new spawn may have another level,
    // else ten minutes.
    double CheckLifetime(const MobRecord* mob);

    // The latest result of the player's own /check of each spawn, keyed by server ID.
    class CheckResults
    {
    public:
        // Keeps the reply until now + lifetime, and drops every result that has expired.
        void Received(const CheckReply& reply, double lifetime, double now);
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
