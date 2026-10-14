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

    // The mob an incoming 0x029 answers a /check of, whether the answer gives its level or is "impossible to
    // gauge"; nothing for any other message.
    struct CheckAnswer
    {
        uint32_t serverId = 0;
        bool gauged       = false;
        bool operator==(const CheckAnswer&) const = default;
    };
    std::optional<CheckAnswer> ParseCheckAnswer(const uint8_t* data, uint32_t size);

    // A mob's death as the server tells everyone within 50 yalms of it, a second after: "defeats", or "falls to the
    // ground" with no attacker (CMobEntity::Die).
    constexpr uint16_t kDefeatsMessage = 6, kFallsMessage = 20;
    // The mob an incoming 0x029 says has died; nothing for any other message.
    std::optional<uint32_t> ParseMobDeath(const uint8_t* data, uint32_t size);

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
