#pragma once

#include "argb.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace headsup
{
    constexpr uint16_t kOtherPlayerPacket = 0x00D; // server to client: another player's update
    constexpr uint16_t kOwnStatusPacket   = 0x037; // server to client: the player's own status
    constexpr uint16_t kCharSyncPacket    = 0x067; // server to client: a character's sync, level sync among it

    struct PlayerStatus
    {
        bool seekingParty  = false;
        bool bazaar        = false;
        bool linkshell     = false;
        bool away          = false;
        bool mentor        = false;
        bool newAdventurer = false;
        bool gm            = false;
        bool levelSync     = false;
        bool anonymous     = false; // the game hides their jobs and levels
        uint32_t linkshellArgb = kWhite;
    };

    struct PlayerJobs
    {
        uint8_t mainJob = 0, mainLevel = 0, subJob = 0, subLevel = 0;
        bool operator==(const PlayerJobs&) const = default;
    };

    struct PlayerUpdate
    {
        uint16_t index; // entity target index
        bool despawn;
        std::optional<PlayerStatus> status; // when the update carries it
    };

    // A player's model as 0x00D carries it: the face and race, then each piece of gear's graphics.
    constexpr size_t kModelParts = 9;
    using PlayerLook = std::array<uint16_t, kModelParts>;
    std::optional<PlayerLook> ParseLook(const uint8_t* data, uint32_t size);

    // A level up or down the server shows everyone in range (0x02D, "<player> attains level #!", "falls to level #."):
    // whose, and the main job's new level.
    constexpr uint16_t kLevelMessagePacket = 0x02D;
    struct LevelChange
    {
        uint16_t index;
        uint8_t level;
    };
    std::optional<LevelChange> ParseLevelMessage(const uint8_t* data, uint32_t size);

    struct CharSyncUpdate
    {
        uint16_t index; // entity target index, yours included
        bool synced;   // the sync icon: 0x10's bit, off in battlefields
        bool held;     // a cap holds the level, in battlefields too
        uint8_t level; // the main job's, held to the cap
    };

    // Layouts from Phoenix's src/map/packets/char_update.cpp (0x00D), char_status.cpp (0x037) and char_sync.cpp (0x067,
    // which also names other kinds of entity: those give nullopt).
    std::optional<PlayerUpdate> ParseOtherPlayer(const uint8_t* data, uint32_t size);
    std::optional<PlayerStatus> ParseOwnStatus(const uint8_t* data, uint32_t size);
    std::optional<CharSyncUpdate> ParseCharSync(const uint8_t* data, uint32_t size);

    // The game keeps these for every player, so they show as soon as HeadsUp loads; the bits were matched against the
    // same players' packets in a capture.
    PlayerStatus StatusFromRender(uint32_t flags1, uint32_t flags2, uint32_t linkshellBgr);

    // Your buffs, and a party member's status icons as packet 0x076 packs them (each icon's low byte in its slot, its
    // high bits two per slot in bitMask), are known as soon as HeadsUp loads. Both hold kStatusIconSlots.
    constexpr int kStatusIconSlots = 32;
    bool LevelSyncInBuffs(const int16_t* buffs);
    bool LevelSyncInPartyIcons(const uint8_t* icons, uint64_t bitMask);

    // Away, mentor, new adventurer and GM are only in the packets; memory is current for the rest.
    PlayerStatus WithPacketStatus(const PlayerStatus& memory, const PlayerStatus* packet);
}
