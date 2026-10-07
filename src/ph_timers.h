#pragma once

#include "mobdata.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace headsup
{
    // Seconds from a mob's death to its despawn, when Phoenix starts its respawn timer (CMobEntity::Die).
    constexpr double kDeathToDespawnSeconds = 15.0;
    // How long a timer stays, reading "up", once its placeholder is due back.
    constexpr double kUpSeconds = 300.0;

    struct PhSighting
    {
        uint32_t serverId;
        uint32_t respawnSeconds; // 0 when it is not on a timer, like a Dynamis statue's spawn
        std::string nmName;
        bool alive;
        bool bodyDrawn;
    };

    // How this mob is seen when it is a lottery placeholder; nullopt for any other.
    std::optional<PhSighting> SightingOf(const MobRecord* mob, bool alive, bool bodyDrawn);

    struct TimerLine
    {
        std::string text;
        bool up;
    };

    // Only a death you see has a known time: the placeholder was alive in range the frame before and its body was drawn.
    class PhTimers
    {
    public:
        void Update(double now, const std::vector<PhSighting>& placeholders);
        std::vector<TimerLine> Lines(uint16_t zone, double now) const;

    private:
        struct Timer
        {
            uint32_t serverId;
            std::string nmName;
            double dueAt;
        };
        std::unordered_map<uint32_t, bool> m_WasAlive;
        std::unordered_map<uint32_t, Timer> m_Timers;
    };

    std::string Countdown(double seconds);
}
