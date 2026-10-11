#pragma once

#include "generated/mobflags.h"

#include <cstdint>
#include <string_view>

namespace headsup
{
    // A mob's immunities (MobImmunity bits), damage taken per type (sdt: damage is multiplied by 1 + sdt / 10000) and
    // elemental and status resistance ranks (Phoenix clamps them to -3..11), in mobflags.h's Sdt, Element and StatusRank
    // orders.
    struct MobResistances
    {
        uint32_t immunity;
        int16_t sdt[kSdtCount];
        int8_t elementRanks[kElementCount];
        int8_t statusRanks[kStatusRankCount];
    };

    // One mob as Phoenix's map server loads it (tools/phoenix/refresh.sh).
    struct MobRecord
    {
        uint32_t id;      // server ID
        const char* name; // Phoenix's display name
        uint8_t minLevel; // 0/0 when a script sets the level at spawn
        uint8_t maxLevel;
        uint8_t flags;       // MobFlag bits
        uint32_t respawn;    // seconds; 0 when the mob is not on a respawn timer
        uint16_t detects;    // MobDetect bits
        int16_t expLevelMod; // added to its level for /check and experience
        uint32_t placeholderOf; // the NM it is a lottery placeholder for, by server ID; 0 when none
        uint16_t resistances;   // index into the table ResistancesOf reads; 0 is neutral
    };

    bool IsAggressive(const MobRecord& mob);

    // Whether two names have the same letters and digits, ignoring case. Phoenix drops apostrophes the client shows:
    // its "Goblins Dragonfly" is the client's "Goblin's Dragonfly".
    bool SameMobName(std::string_view a, std::string_view b);
    // Phoenix names a mob whose template has no display name after its template key, which adds tags after the name
    // the client shows: its "Stink Bats OHR" and "Lost Soul war ENS" are the client's "Stink Bats" and "Lost Soul".
    bool TaggedMobName(std::string_view recorded, std::string_view shown);

    // The record for this server ID whatever its name; nullptr when there is none.
    const MobRecord* MobById(uint32_t serverId);

    // The record for this server ID when its name matches the entity's; nullptr when there is none or the names
    // differ (data older than the server's ID assignments).
    const MobRecord* FindMob(uint32_t serverId, std::string_view displayName);

    // Its resistances; the neutral set when its index is past the table.
    const MobResistances& ResistancesOf(const MobRecord& mob);

}
