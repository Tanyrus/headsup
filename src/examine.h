#pragma once

#include "con.h"
#include "mobdata.h"

#include <array>
#include <cstdint>
#include <optional>
#include <unordered_map>

namespace aggroglow
{
    constexpr float kExamineMaxDistance = 45.0f;  // yalms; the server logs a warning past 50
    constexpr double kExamineInterval   = 1.0;    // seconds between automatic checks
    constexpr double kDefaultCooldown   = 600.0;  // seconds, when the data has no respawn time
    constexpr double kPendingTimeout    = 3.0;    // seconds a reply may take and still be hidden

    // Outgoing packet 0x0DD (Check) for a mob. Bytes 0-3 stay 0: Ashita fills in the header.
    std::array<uint8_t, 16> BuildCheckRequest(uint32_t serverId, uint16_t targetIndex);

    // An incoming 0x029 (Message Basic) packet that answers a /check.
    struct CheckReply
    {
        uint32_t serverId;    // the checked mob
        uint16_t targetIndex;
        bool gauged;          // false for "impossible to gauge" (notorious monsters, battlefields)
        CheckResult result;   // set when gauged
    };
    std::optional<CheckReply> ParseCheckReply(const uint8_t* data, uint32_t size);

    // Seconds between automatic checks of one mob, and how long a result stays valid: its respawn time, else
    // kDefaultCooldown.
    double ExamineCooldown(const MobRecord* mob);

    // Yalms from Ashita's squared entity distance. A NaN stays NaN, so IsExamineEligible rejects it; a negative value
    // becomes 0.
    float DistanceFromSquared(float squared);

    struct ExamineTarget
    {
        const MobRecord* mob; // nullptr when there is no data
        bool alive;
        float distance;       // yalms
        int playerLevel;
        bool playerInEvent;   // the server refuses checks during events and cutscenes
    };

    // Whether the targeted mob may be checked automatically (spec section 4), cooldowns aside.
    bool IsExamineEligible(const ExamineTarget& target);

    // Automatic-check cooldowns, the pending check, and the latest result per spawn (keyed by server ID).
    class Examiner
    {
    public:
        // True when no result for this mob is still valid, its cooldown has passed, and no automatic check went
        // out in the last kExamineInterval seconds.
        bool CanSend(uint32_t serverId, double now) const;
        // Records an automatic check that was just sent.
        void Sent(uint32_t serverId, double cooldown, double now);
        // Records a check reply, manual or automatic. Returns true when it answers the pending automatic check
        // within kPendingTimeout; the caller then hides it from chat.
        bool Received(const CheckReply& reply, double lifetime, double now);
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
        struct Pending
        {
            uint32_t serverId = 0;
            double time       = 0.0;
            bool active       = false;
        };
        std::unordered_map<uint32_t, double> m_NextAllowed;
        std::unordered_map<uint32_t, Stored> m_Results;
        Pending m_Pending;
        double m_LastSent  = 0.0;
        bool m_SentAny     = false;
    };
}
