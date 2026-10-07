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
    constexpr int kPlayerIconCount = static_cast<int>(PlayerIcon::SeekingParty) + 1;

    enum class IconSide : uint8_t
    {
        Left,
        Right,
        Hidden,
    };
    constexpr int kIconSideCount = static_cast<int>(IconSide::Hidden) + 1;

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
        uint32_t linkshellArgb = kWhite;
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

    struct PlayerIconRows
    {
        IconSet left, right;
    };
    PlayerIconRows PlayerIcons(const PlayerStatus& status, const Settings& settings);
}
