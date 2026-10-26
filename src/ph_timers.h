#pragma once

#include "mobdata.h"
#include "pose.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
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

    // Phoenix's rule: a timer is the player's who fought or watched the mob, not anyone's who stood near it. A death
    // starts one only if, while the mob lived, the player targeted it, or was active while it was claimed by them or
    // their party (curing them counts, as the claim is theirs), or while it was on their screen.
    bool EarnsTimer(bool targeted, bool partyClaim, bool onScreen, bool active);
    constexpr double kIdleSeconds = 150.0; // without moving or turning, a player is away from the keyboard

    // Whether the player has moved or turned in the last kIdleSeconds.
    class Activity
    {
    public:
        void Pose(double now, WorldPoint at, float heading);
        bool Active(double now) const { return m_MovedAt && now - *m_MovedAt <= kIdleSeconds; }

    private:
        std::optional<WorldPoint> m_At;
        float m_Heading = 0.0f;
        std::optional<double> m_MovedAt;
    };

    struct TimerLine
    {
        std::string text;
        bool up;
    };

    // A death has a known time when the server tells of it, which it does to everyone within 50 yalms of the mob.
    class PhTimers
    {
    public:
        // This life of the mob earned its timer (EarnsTimer).
        void Earned(uint32_t serverId) { m_Earned.insert(serverId); }
        // From the server's defeat message: the mob's placeholder is due back after the despawn and its respawn, if
        // this life of it earned the timer.
        void Died(double now, const PhSighting& mob);
        // What was earned in the zone left is no longer anyone's.
        void Zoned() { m_Earned.clear(); }
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
        std::unordered_set<uint32_t> m_Earned;
    };

    std::string Countdown(double seconds);
}
