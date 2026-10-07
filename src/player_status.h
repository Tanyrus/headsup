#pragma once

#include "icons.h"

#include <cstdint>
#include <optional>

namespace headsup
{
    constexpr uint16_t kOtherPlayerPacket = 0x00D; // server to client: another player's update
    constexpr uint16_t kOwnStatusPacket   = 0x037; // server to client: the player's own status

    // What the game shows beside a player's name.
    struct PlayerStatus
    {
        bool seekingParty  = false;
        bool bazaar        = false;
        bool linkshell     = false;
        bool away          = false;
        bool mentor        = false;
        bool newAdventurer = false;
        bool gm            = false;
        uint32_t linkshellArgb = 0xFFFFFFFF; // the linkshell's color
    };

    struct PlayerUpdate
    {
        uint16_t index; // entity target index
        bool despawn;
        std::optional<PlayerStatus> status; // when the update carries it
    };

    // Layouts from Phoenix's src/map/packets/char_update.cpp (0x00D) and char_status.cpp (0x037).
    std::optional<PlayerUpdate> ParseOtherPlayer(const uint8_t* data, uint32_t size);
    std::optional<PlayerStatus> ParseOwnStatus(const uint8_t* data, uint32_t size);

    // What the game keeps for every player in memory, so it is known as soon as HeadsUp loads: seeking a party
    // (Render.Flags1 bit 20), a linkshell (Flags1 bit 27) with its color (blue, green, red) and a bazaar (Flags2 bit 9),
    // matched against the packets for the same players in a capture.
    PlayerStatus StatusFromRender(uint32_t flags1, uint32_t flags2, uint32_t linkshellBgr);

    // The memory's status with away, mentor, new adventurer and GM from the packets, when HeadsUp has seen them.
    PlayerStatus WithPacketStatus(const PlayerStatus& memory, const PlayerStatus* packet);

    // The status's icons, left to right; seeking a party sits nearest the name.
    IconSet PlayerIcons(const PlayerStatus& status);
}
