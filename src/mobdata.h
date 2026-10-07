#pragma once

#include <cstdint>
#include <string_view>

namespace headsup
{
    // Bits of MobRecord::flags (the flags column of data/phoenix_mobs.tsv).
    enum MobFlag : uint8_t
    {
        kMobAggressive  = 1,  // m_Aggro
        kMobAlwaysAggro = 2,  // MobMod AlwaysAggro: aggros even when Too Weak
        kMobNoAggro     = 4,  // MobMod NoAggro
        kMobNeutral     = 8,  // m_neutral
        kMobNotorious   = 16, // MobType Notorious
        kMobBattlefield = 32, // MobType Battlefield
        kMobLink        = 64, // m_Link: links with its family
        kMobTrueDetection = 128, // m_TrueDetection: true sight or true sound
    };

    // Bits of MobRecord::detects: Phoenix's MobMod Detection (data/enums/detects.yaml).
    enum MobDetect : uint16_t
    {
        kDetectSight   = 0x001,
        kDetectHearing = 0x002,
        kDetectLowHp   = 0x004,
        kDetectMagic   = 0x020,
        kDetectAbility = 0x040, // job abilities and weapon skills
        kDetectScent   = 0x100,
    };

    // One mob as Phoenix's map server loads it (tools/phoenix/refresh.sh).
    struct MobRecord
    {
        uint32_t id;      // server ID
        uint16_t zone;
        const char* name; // Phoenix's display name
        uint8_t minLevel; // 0/0 when a script sets the level at spawn
        uint8_t maxLevel;
        uint8_t flags;    // MobFlag bits
        uint32_t respawn; // seconds; 0 when the mob is not on a respawn timer
        uint16_t detects; // MobDetect bits
    };

    // Whether two names have the same letters and digits, ignoring case. Phoenix drops apostrophes the client shows:
    // its "Goblins Dragonfly" is the client's "Goblin's Dragonfly".
    bool SameMobName(std::string_view a, std::string_view b);

    // The record for this server ID when its name matches the entity's; nullptr when there is none or the names
    // differ (data older than the server's ID assignments).
    const MobRecord* FindMob(uint32_t serverId, std::string_view displayName);

}
