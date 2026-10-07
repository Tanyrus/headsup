#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace headsup
{
    // Seconds from a mob's death to its despawn, when Phoenix starts its respawn timer (CMobEntity::Die).
    constexpr double kDeathToDespawnSeconds = 15.0;
    // How long a timer stays, reading "up", once its placeholder is due back.
    constexpr double kUpSeconds = 300.0;

    // A lottery placeholder in range this frame.
    struct PhSighting
    {
        uint32_t serverId;
        uint32_t respawnSeconds; // from the Phoenix data; 0 when it is not on a timer, like a Dynamis statue's spawn
        std::string nmName;      // the NM it can pop
        bool alive;
        bool onScreen; // its body was drawn lately
    };

    // A line above your name: the NM, the placeholder's last three hex digits, and its respawn countdown or "up".
    struct TimerLine
    {
        std::string text;
        bool up;
    };

    // Respawn timers for the lottery placeholders on a respawn timer that you see die, on screen and not already a
    // corpse when they came into range. Phoenix starts a mob's respawn timer when it despawns, kDeathToDespawnSeconds after it dies. A placeholder
    // seen alive again loses its timer; one due back reads "up" for kUpSeconds, then goes.
    class PhTimers
    {
    public:
        void Update(double now, const std::vector<PhSighting>& placeholders);
        // The timers of this zone's placeholders, soonest first.
        std::vector<TimerLine> Lines(uint16_t zone, double now) const;

    private:
        struct Timer
        {
            uint32_t serverId;
            std::string nmName;
            double dueAt;
        };
        std::unordered_map<uint32_t, bool> m_WasAlive; // the placeholders in range the frame before
        std::unordered_map<uint32_t, Timer> m_Timers;
    };

    // Seconds left as "m:ss", or "h:mm:ss" from an hour, rounded up.
    std::string Countdown(double seconds);
}
