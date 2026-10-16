#pragma once

#include "generated/mobflags.h"

#include <cstdint>
#include <span>
#include <string_view>

namespace headsup
{
    // A mob's immunities (MobImmunity bits), damage taken per type (sdt: damage is multiplied by 1 + sdt / 10000),
    // elemental and status resistance ranks (Phoenix clamps them to -3..11), magic evasion against each element and
    // status, and each status's resist trait (the chance it resists outright, past its first 5), in mobflags.h's Sdt,
    // Element and StatusRank orders. A status's magic evasion and resist trait include what every status shares.
    struct MobResistances
    {
        uint32_t immunity;
        int16_t sdt[kSdtCount];
        int8_t elementRanks[kElementCount];
        int8_t statusRanks[kStatusRankCount];
        int16_t elementMeva[kElementCount];
        int16_t statusMeva[kStatusRankCount];
        int16_t statusResist[kStatusRankCount];
    };

    // A mob's stats at one level, in mobflags.h's Stat order: STR to CHR as the magic hit rate reads them, then its
    // magic evasion, which its rank multiplies.
    struct MobStats
    {
        int16_t value[kStatCount];
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
        uint8_t placeholders;   // an NM's: how many mobs are its lottery placeholders
        uint16_t resistances;   // index into the table ResistancesOf reads; 0 is neutral
        uint16_t drops = 0;     // index into the lists DropsOf reads; 0 is none
        uint32_t placeholderRespawn = 0; // an NM's: seconds its placeholder takes to return after it despawns; 0 if none
        uint16_t stats = 0;              // index into the runs StatsAt reads, a level each from minLevel; 0 is none
    };

    // What a piece of equipment adds to magic accuracy: its own, each element's and each elemental staff bonus (ten
    // magic accuracy a point), in Element order, and an instrument's skill (Phoenix's skill type, 0 for any other).
    struct ItemMagic
    {
        uint16_t id;
        int16_t macc;
        int16_t elementMacc[kElementCount];
        int16_t staffBonus[kElementCount];
        uint8_t skill;
    };

    // One entry of a mob's drop list, as the server loaded it. Rates are per mille (1000 is always) and are tiers, not
    // chances: the server rolls each with Treasure Hunter's table, which gives the chance of the highest tier at or
    // below the rate (very common is 24% without Treasure Hunter, and anything from 24% up to just under 100% rolls as
    // very common), and then applies its drop rate multiplier.
    //   An ungrouped Drop (group 0) rolls on its own at its rate.
    //   A grouped Drop's rate is a weight: its group rolls once at the group's rate (DropGroupOf) and then picks one of
    //   its members, each with the chance rate / the group's weight. Item 0 is the share of picking nothing.
    //   Steal and Despoil each pick one of the mob's entries of that kind, all alike, whatever their rates; the server
    //   also rolls those rates at death as if they were drops, so a Despoil weight of 50 drops 5% of the time too.
    struct MobDrop
    {
        uint16_t item; // the game's item ID
        uint16_t rate; // per mille, or a weight inside a group
        DropKind kind;
        uint8_t group; // 0 when ungrouped, else the group of the list, from 1, whose roll picks it
    };

    // One group of a drop list: the per mille rate it rolls at, and its members' summed rates.
    struct MobDropGroup
    {
        uint16_t rate;
        uint16_t weight;
    };

    // What else the server knows of a mob (data/phoenix_mob_info.tsv), beside its MobRecord.
    struct MobInfo
    {
        uint8_t mainJob;          // Phoenix's xi::Job, 1 WAR to 22 RUN
        uint8_t subJob;
        uint8_t sight;            // the yalms it sees players from
        uint8_t sound;            // and hears them from
        uint8_t windowStart;      // the Vana'diel hour it spawns from, and until; kNoWindow when it spawns at any
        uint8_t windowEnd;
        uint8_t lotteryChance;    // an NM's percent chance to take its spawn slot's roll after a kill; 0 for any other
        uint32_t lotteryCooldown; // seconds it sits out of the roll after its own kill
        uint8_t traits;           // MobTrait bits
        const char* addEffect;    // what its melee hits add, "" when nothing
        uint16_t defenses;        // index into the table DefensesOf reads; 0 is none
        uint16_t combat;          // index into the runs CombatAt reads; 0 is none
        uint16_t skills;          // its TP move list, 0 when none
        uint16_t spells;          // its spell list, 0 when none
        uint16_t regimes;         // the run of regimes it counts for, 0 when none
    };

    // A mob's HP, MP, attack, defense, evasion and accuracy at one level, in mobflags.h's Combat order.
    struct MobCombat
    {
        int32_t value[kCombatCount];
    };

    // A mob's percent chance to absorb each element, and its damage taken of each DamageKind: damage is multiplied
    // by 1 + value / 10000, past the SDT of MobResistances.
    struct MobDefenses
    {
        int16_t absorb[kElementCount];
        int16_t damageTaken[kDamageKindCount];
    };

    struct MobSkill
    {
        uint16_t id;
        const char* name;
    };

    // A spell a mob casts from minLevel to maxLevel of its own level.
    struct MobSpell
    {
        uint16_t spell; // the game's spell ID
        uint8_t minLevel;
        uint8_t maxLevel;
    };

    // A Fields or Grounds of Valor regime: its zone and page there, how many of each of its targets to kill, its
    // level range and the experience it gives.
    struct Regime
    {
        uint16_t id;
        RegimeType type;
        uint16_t zone;
        uint8_t page;
        uint8_t counts[kRegimeTargets];
        uint8_t minLevel;
        uint8_t maxLevel;
        uint16_t exp;
    };

    // A regime a mob counts for, and which of the page's targets it is, from 1.
    struct MobRegime
    {
        uint16_t regime;
        uint8_t target;
    };

    bool IsAggressive(const MobRecord& mob);

    // The record for this server ID; nullptr when there is none. The name an entity shows is not asked: the renamer
    // addon writes its own over the game's, and Phoenix's own names carry tags the client's lack.
    const MobRecord* MobById(uint32_t serverId);

    // Its resistances; the neutral set when its index is past the table.
    const MobResistances& ResistancesOf(const MobRecord& mob);

    // Its stats at a level of its range; nullptr when the data has none for it or the level is outside the range.
    const MobStats* StatsAt(const MobRecord& mob, int level);

    // What a piece of equipment adds to magic accuracy; nullptr for one that adds nothing.
    const ItemMagic* ItemMagicOf(uint16_t itemId);

    // Its drop list in the server's order, the ungrouped entries first; empty when it has no loot or its index is past
    // the table.
    std::span<const MobDrop> DropsOf(const MobRecord& mob);

    // Group 1 and up of its drop list; {0, 0} for group 0 or one it does not have.
    const MobDropGroup& DropGroupOf(const MobRecord& mob, uint8_t group);

    // Its info by its server ID; one of nothing (no jobs, no window) for a record the data does not have.
    const MobInfo& InfoOf(const MobRecord& mob);

    // Its combat stats at a level of its range; nullptr when the data has none for it or the level is outside it.
    const MobCombat* CombatAt(const MobRecord& mob, int level);

    // Its absorbs and damage taken; none for a record the data does not have.
    const MobDefenses& DefensesOf(const MobRecord& mob);

    // Its TP moves, its spells and the regimes it counts for, each empty when it has none.
    std::span<const MobSkill> SkillsOf(const MobRecord& mob);
    std::span<const MobSpell> SpellsOf(const MobRecord& mob);
    std::span<const MobRegime> RegimesOf(const MobRecord& mob);

    // The regime of that ID; nullptr when there is none.
    const Regime* RegimeById(uint16_t id);

}
