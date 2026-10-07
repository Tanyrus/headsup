#pragma once

#include "argb.h"
#include "icons.h"

#include <cstdint>
#include <optional>

namespace headsup
{
    struct Settings;

    constexpr uint16_t kOtherPlayerPacket = 0x00D; // server to client: another player's update
    constexpr uint16_t kOwnStatusPacket   = 0x037; // server to client: the player's own status
    constexpr uint16_t kCharSyncPacket    = 0x067; // server to client: a character's sync, level sync among it

    // The icons beside a player's name, in their order on either side.
    enum class PlayerIcon : uint8_t
    {
        Gm,
        Mentor,
        NewAdventurer,
        LevelSync,
        Away,
        Linkshell,
        Bazaar,
        SeekingParty,
    };
    constexpr int kPlayerIconCount = 8;
    constexpr int PlayerIconIndex(PlayerIcon icon) { return static_cast<int>(icon); }

    // Where a player icon goes: either side of the name, or nowhere.
    enum class IconSide : uint8_t
    {
        Left,
        Right,
        Hidden,
    };
    constexpr int kIconSideCount = 3;

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
        bool levelSync     = false;
        uint32_t linkshellArgb = kWhite; // the linkshell's color
    };

    struct PlayerUpdate
    {
        uint16_t index; // entity target index
        bool despawn;
        std::optional<PlayerStatus> status; // when the update carries it
    };

    struct LevelSyncUpdate
    {
        uint16_t index; // entity target index, yours included
        bool synced;
    };

    // Layouts from Phoenix's src/map/packets/char_update.cpp (0x00D), char_status.cpp (0x037) and char_sync.cpp (0x067,
    // which also names other kinds of entity: those give nullopt).
    std::optional<PlayerUpdate> ParseOtherPlayer(const uint8_t* data, uint32_t size);
    std::optional<PlayerStatus> ParseOwnStatus(const uint8_t* data, uint32_t size);
    std::optional<LevelSyncUpdate> ParseCharSync(const uint8_t* data, uint32_t size);

    // What the game keeps for every player in memory, so it is known as soon as HeadsUp loads: seeking a party, a
    // linkshell with its color (blue, green, red) and a bazaar. The bits were matched against the packets for the same
    // players in a capture.
    PlayerStatus StatusFromRender(uint32_t flags1, uint32_t flags2, uint32_t linkshellBgr);

    // Level sync from the buffs the game keeps, so it is known as soon as HeadsUp loads: your own (IPlayer's buffs), and
    // each other member of your party's status icons as packet 0x076 carries them, each icon's low byte in its slot and
    // its high bits two per slot in bitMask. Both hold kStatusIconSlots.
    constexpr int kStatusIconSlots = 32;
    bool LevelSyncInBuffs(const int16_t* buffs);
    bool LevelSyncInPartyIcons(const uint8_t* icons, uint64_t bitMask);

    // The memory's status with away, mentor, new adventurer and GM from the packets, when HeadsUp has seen them.
    PlayerStatus WithPacketStatus(const PlayerStatus& memory, const PlayerStatus* packet);

    // The status's icons the settings show, each on the side of the name it is set to, in PlayerIcon order.
    struct PlayerIconRows
    {
        IconSet left, right;
    };
    PlayerIconRows PlayerIcons(const PlayerStatus& status, const Settings& settings);
}
