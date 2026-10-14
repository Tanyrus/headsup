#pragma once

#include "mobdata.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace headsup
{
    // Seconds from a mob's death to its despawn, when Phoenix starts its respawn timer (CMobEntity::Die), and to the
    // message that tells of the death (m_DropItemTime).
    constexpr double kDeathToDespawnSeconds = 15.0;
    constexpr double kDefeatMessageSeconds  = 1.0;
    // How long a timer stays, reading "up", once its placeholder is due back.
    constexpr double kUpSeconds = 300.0;

    struct PhSighting
    {
        uint32_t serverId;
        uint32_t respawnSeconds; // 0 when it is not on a timer, like a Dynamis statue's spawn
        std::string nmName;
    };

    // A lottery placeholder, timed by its own respawn and named for its NM, or a lottery NM, timed by its placeholders'
    // respawn, which Phoenix sets on the placeholder when the NM dies; nullopt for any other mob.
    std::optional<PhSighting> SightingOf(const MobRecord* mob);

    struct TimerLine
    {
        std::string text;
        bool up;
    };

    // A death has a known time when the server tells of it, which it does to everyone within 50 yalms of the mob.
    class PhTimers
    {
    public:
        // From the server's defeat message: the mob's placeholder is due back after the despawn and its respawn.
        void Died(double now, const PhSighting& mob);
        // The mobs seen alive in range lose their timers; those long up go.
        void Update(double now, const std::vector<uint32_t>& aliveMobs);
        std::vector<TimerLine> Lines(uint16_t zone, double now) const;
        // Seconds until this placeholder is due back, 0 once it is; nullopt when no timer runs for it.
        std::optional<double> SecondsLeft(uint32_t serverId, double now) const;

    private:
        struct Timer
        {
            uint32_t serverId;
            std::string nmName;
            double dueAt;
        };
        std::unordered_map<uint32_t, Timer> m_Timers;
    };

    std::string Countdown(double seconds);
}
