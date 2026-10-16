/************************************************************************
 * headsup_dump
 *
 * A Phoenix C++ module added by headsup's tools/phoenix/refresh.sh. Once
 * zones have spawned their mobs, it spawns the rest, so every mob runs its
 * spawn script, and watches them all live a while, noting any it sees
 * attacking, then writes every loaded mob, with its immunities, damage
 * taken, resistance ranks, magic evasion, resist traits, jobs, spawn
 * window, lottery chance, senses, absorbs, traits, TP moves, spells, stats
 * and combat stats at each level and loot, every item with magic accuracy,
 * the TP move and spell lists, the /check and aggro rules the server
 * loaded and the settings it ran with to $HEADSUP_DUMP_PATH as JSON and
 * exits.
 ************************************************************************/

#include "common/lua.h"
#include "map/ai/ai_container.h"
#include "map/entities/mob_entity.h"
#include "map/items/item_equipment.h"
#include "map/items/item_weapon.h"
#include "map/mob_spell_list.h"
#include "map/mobskill.h"
#include "map/spawn_slot.h"
#include "map/status_effect_container.h"
#include "map/utils/battleutils.h"
#include "map/utils/charutils.h"
#include "map/utils/itemutils.h"
#include "map/utils/mobutils.h"
#include "map/utils/moduleutils.h"
#include "map/utils/zoneutils.h"
#include "map/zone.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// charutils.cpp's /check curve and Incredibly Easy Prey rule, as the server's scripts last loaded them.
extern std::vector<std::pair<uint16, EMobDifficulty>> ExpToDifficultyTable;
extern std::pair<uint16, uint8> IncrediblyEasyPreyCheck;

namespace
{
    constexpr int kSpawnTick        = 20; // time-server ticks (2.4 s) after start, once every zone has spawned its mobs
    // Two minutes for the AI to settle: past the 15 s calm after a spawn and the first form change of mobs like the
    // Ghrahs, which spawn passive and change form every minute.
    constexpr int kDumpTick         = kSpawnTick + 50;
    constexpr int kSampleTicks      = 5; // how often to look for mobs attacking: 12 s, well inside a Ghrah's minute
    constexpr int kWidestDifference = 99; // mob level minus player level; GetBaseExp clamps it to its table
    constexpr int kLevelsPerColumn  = 5;  // GetBaseExp reads one column per five player levels
    constexpr int kColumns          = 20; // player levels 1 to 100
    constexpr int kAnimations       = 256;
    // In tools/gen_mobdata.py's SDT_ORDER, ELEMENT_ORDER and STATUS_RANK_ORDER.
    constexpr xi::Mod kSdtMods[] = {xi::Mod::SLASH_SDT, xi::Mod::PIERCE_SDT, xi::Mod::IMPACT_SDT, xi::Mod::HTH_SDT,
        xi::Mod::FIRE_SDT, xi::Mod::ICE_SDT, xi::Mod::WIND_SDT, xi::Mod::EARTH_SDT, xi::Mod::THUNDER_SDT, xi::Mod::WATER_SDT,
        xi::Mod::LIGHT_SDT, xi::Mod::DARK_SDT};
    constexpr xi::Mod kElementRankMods[] = {xi::Mod::FIRE_RES_RANK, xi::Mod::ICE_RES_RANK, xi::Mod::WIND_RES_RANK,
        xi::Mod::EARTH_RES_RANK, xi::Mod::THUNDER_RES_RANK, xi::Mod::WATER_RES_RANK, xi::Mod::LIGHT_RES_RANK,
        xi::Mod::DARK_RES_RANK};
    constexpr xi::Mod kStatusRankMods[] = {xi::Mod::PARALYZE_RES_RANK, xi::Mod::BIND_RES_RANK, xi::Mod::SILENCE_RES_RANK,
        xi::Mod::SLOW_RES_RANK, xi::Mod::POISON_RES_RANK, xi::Mod::LIGHT_SLEEP_RES_RANK, xi::Mod::DARK_SLEEP_RES_RANK,
        xi::Mod::BLIND_RES_RANK, xi::Mod::STUN_RES_RANK, xi::Mod::GRAVITY_RES_RANK};
    constexpr xi::Mod kElementMevaMods[] = {xi::Mod::FIRE_MEVA, xi::Mod::ICE_MEVA, xi::Mod::WIND_MEVA, xi::Mod::EARTH_MEVA,
        xi::Mod::THUNDER_MEVA, xi::Mod::WATER_MEVA, xi::Mod::LIGHT_MEVA, xi::Mod::DARK_MEVA};
    // scripts/data/status_effect_tables.lua's magic evasion and resist trait of each status; both sleeps are SLEEP_I's.
    constexpr xi::Mod kStatusMevaMods[] = {xi::Mod::PARALYZE_MEVA, xi::Mod::BIND_MEVA, xi::Mod::SILENCE_MEVA,
        xi::Mod::SLOW_MEVA, xi::Mod::POISON_MEVA, xi::Mod::SLEEP_MEVA, xi::Mod::SLEEP_MEVA, xi::Mod::BLIND_MEVA,
        xi::Mod::STUN_MEVA, xi::Mod::GRAVITY_MEVA};
    constexpr xi::Mod kStatusResistMods[] = {xi::Mod::PARALYZERES, xi::Mod::BINDRES, xi::Mod::SILENCERES, xi::Mod::SLOWRES,
        xi::Mod::POISONRES, xi::Mod::SLEEPRES, xi::Mod::SLEEPRES, xi::Mod::BLINDRES, xi::Mod::STUNRES, xi::Mod::GRAVITYRES};
    constexpr xi::Mod kElementMaccMods[] = {xi::Mod::FIRE_MACC, xi::Mod::ICE_MACC, xi::Mod::WIND_MACC, xi::Mod::EARTH_MACC,
        xi::Mod::THUNDER_MACC, xi::Mod::WATER_MACC, xi::Mod::LIGHT_MACC, xi::Mod::DARK_MACC};
    constexpr xi::Mod kStaffBonusMods[] = {xi::Mod::FIRE_STAFF_BONUS, xi::Mod::ICE_STAFF_BONUS, xi::Mod::WIND_STAFF_BONUS,
        xi::Mod::EARTH_STAFF_BONUS, xi::Mod::THUNDER_STAFF_BONUS, xi::Mod::WATER_STAFF_BONUS, xi::Mod::LIGHT_STAFF_BONUS,
        xi::Mod::DARK_STAFF_BONUS};
    // In tools/gen_mobdata.py's STAT_ORDER: STR to CHR as the magic hit rate reads them (getStat), magic evasion,
    // then HP, MP, attack, defense, evasion and accuracy.
    using Stats = std::array<int, 14>;
    // In tools/phoenix/compact.py's DAMAGE_MODS: the uncapped and capped damage taken of each kind, and DMG.
    constexpr xi::Mod kDamageMods[] = {xi::Mod::UDMGPHYS, xi::Mod::DMGPHYS, xi::Mod::UDMGMAGIC, xi::Mod::DMGMAGIC,
        xi::Mod::DMGMAGIC_II, xi::Mod::UDMGRANGE, xi::Mod::DMGRANGE, xi::Mod::UDMGBREATH, xi::Mod::DMGBREATH, xi::Mod::DMG};
    constexpr xi::Mod kAbsorbMods[] = {xi::Mod::FIRE_ABSORB, xi::Mod::ICE_ABSORB, xi::Mod::WIND_ABSORB,
        xi::Mod::EARTH_ABSORB, xi::Mod::LTNG_ABSORB, xi::Mod::WATER_ABSORB, xi::Mod::LIGHT_ABSORB, xi::Mod::DARK_ABSORB};
    // In compact.py's TRAIT_MODS.
    constexpr xi::Mod kTraitMods[] = {xi::Mod::REGEN, xi::Mod::REFRESH, xi::Mod::REGAIN, xi::Mod::BLINK,
        xi::Mod::UTSUSEMI, xi::Mod::COUNTER, xi::Mod::SPIKES, xi::Mod::ENSPELL};

    template <size_t N>
    void WriteMods(FILE* out, const char* key, CMobEntity* PMob, const xi::Mod (&mods)[N])
    {
        std::fprintf(out, ",\"%s\":[", key);
        for (size_t i = 0; i < N; ++i)
            std::fprintf(out, "%s%d", i ? "," : "", static_cast<int>(PMob->getMod(mods[i])));
        std::fputs("]", out);
    }

    // Each status's value with what every status shares added, as the magic hit rate and the resist trait roll add it.
    template <size_t N>
    void WriteModsWith(FILE* out, const char* key, CMobEntity* PMob, const xi::Mod (&mods)[N], xi::Mod shared)
    {
        std::fprintf(out, ",\"%s\":[", key);
        for (size_t i = 0; i < N; ++i)
            std::fprintf(out, "%s%d", i ? "," : "", static_cast<int>(PMob->getMod(mods[i]) + PMob->getMod(shared)));
        std::fputs("]", out);
    }

    Stats StatsOf(CMobEntity* PMob)
    {
        return {PMob->STR(), PMob->DEX(), PMob->VIT(), PMob->AGI(), PMob->INT(), PMob->MND(), PMob->CHR(),
            PMob->getMod(xi::Mod::MEVA), PMob->GetMaxHP(), PMob->GetMaxMP(), PMob->ATT(SLOT_MAIN), PMob->DEF(), PMob->EVA(),
            PMob->ACC(0, 0)};
    }

    // What the server's calculation gives at a level, set as CMobEntity::Spawn sets it. Only a recovering calculation
    // works out HP and MP; it also ends the mob's effects, which its record has been written with.
    Stats CalculatedAt(CMobEntity* PMob, uint8 level)
    {
        PMob->TraitList.clear();
        PMob->SetMLevel(level);
        PMob->SetSLevel(level);
        mobutils::CalculateMobStats(PMob, true);
        return StatsOf(PMob);
    }

    // Its stats at each level of its range from minLevel, with what its SQL mods and spawn script added at the level it
    // spawned at; none for a mob whose script sets its level. It recalculates the mob, so it comes last in its record.
    void WriteLevelStats(FILE* out, CMobEntity* PMob)
    {
        std::fputs(",\"stats\":[", out);
        const uint8 spawned = PMob->GetMLevel();
        if (PMob->m_minLevel > 0 && spawned > 0)
        {
            const Stats seen       = StatsOf(PMob);
            const Stats calculated = CalculatedAt(PMob, spawned);
            for (int level = PMob->m_minLevel; level <= PMob->m_maxLevel; ++level)
            {
                const Stats at = CalculatedAt(PMob, static_cast<uint8>(level));
                std::fputs(level == PMob->m_minLevel ? "[" : ",[", out);
                for (size_t i = 0; i < at.size(); ++i)
                    std::fprintf(out, "%s%d", i ? "," : "", at[i] + seen[i] - calculated[i]);
                std::fputs("]", out);
            }
        }
        std::fputs("]", out);
    }

    // Every piece of equipment the magic hit rate reads: its magic accuracy, each element's and each elemental staff
    // bonus, and an instrument's skill, as songs add a wind instrument's.
    void WriteItems(FILE* out)
    {
        std::fputs("\"items\":[", out);
        bool first = true;
        for (uint32 id = 0; id < MAX_ITEMID; ++id)
        {
            const auto* item = xi::items::lookup<CItemEquipment>(static_cast<uint16>(id));
            if (item == nullptr) continue;
            const auto* weapon  = dynamic_cast<const CItemWeapon*>(item);
            const auto skill    = weapon != nullptr ? weapon->getSkillType() : xi::SkillType::None;
            const bool instrument = skill == xi::SkillType::WindInstrument || skill == xi::SkillType::StringInstrument;
            bool any            = instrument || item->getModifier(xi::Mod::MACC) != 0;
            for (size_t i = 0; i < std::size(kElementMaccMods); ++i)
                any = any || item->getModifier(kElementMaccMods[i]) != 0 || item->getModifier(kStaffBonusMods[i]) != 0;
            if (!any) continue;
            std::fprintf(out, "%s\n{\"id\":%u,\"macc\":%d,\"elementMacc\":[", first ? "" : ",", id,
                static_cast<int>(item->getModifier(xi::Mod::MACC)));
            first = false;
            for (size_t i = 0; i < std::size(kElementMaccMods); ++i)
                std::fprintf(out, "%s%d", i ? "," : "", static_cast<int>(item->getModifier(kElementMaccMods[i])));
            std::fputs("],\"staffBonus\":[", out);
            for (size_t i = 0; i < std::size(kStaffBonusMods); ++i)
                std::fprintf(out, "%s%d", i ? "," : "", static_cast<int>(item->getModifier(kStaffBonusMods[i])));
            std::fprintf(out, "],\"skill\":%d}", instrument ? static_cast<int>(skill) : 0);
        }
        std::fputs("\n]", out);
    }

    std::string JsonEscape(const std::string& text)
    {
        std::string out;
        for (const char c : text)
        {
            if (c == '"' || c == '\\')
            {
                out += '\\';
                out += c;
            }
            else if (static_cast<unsigned char>(c) < 0x20)
            {
                char escaped[8];
                std::snprintf(escaped, sizeof(escaped), "\\u%04x", c);
                out += escaped;
            }
            else
                out += c;
        }
        return out;
    }

    // Every lottery placeholder: placeholder server ID to the NM's, the first when it can roll several. A spawn slot
    // (a zone's mobs.yaml `slots`) holds its NM as the member with a spawn chance; an NM script's entity.phList, as the
    // server loaded it, names the rest.
    std::unordered_map<uint32, uint32> Placeholders()
    {
        std::unordered_map<uint32, uint32> nmOf;
        zoneutils::ForEachZone([&](CZone* PZone) {
            PZone->ForEachMob([&](CMobEntity* PMob) {
                const SpawnSlot* slot = PMob->GetSpawnSlot();
                if (slot == nullptr) return;
                const auto& entries = slot->GetEntries();
                const auto own = std::ranges::find(entries, PMob, &SpawnSlotEntry::mob);
                const auto nm  = std::ranges::find_if(entries, [](const SpawnSlotEntry& e) { return e.spawnChance > 0; });
                if (own != entries.end() && own->spawnChance == 0 && nm != entries.end())
                    nmOf.try_emplace(PMob->id, nm->mob->id);
            });
        });
        const auto zones = ::lua["xi"]["zones"].get<sol::optional<sol::table>>();
        if (!zones) return nmOf;
        for (const auto& [zoneName, zone] : *zones)
        {
            if (!zone.is<sol::table>()) continue;
            const auto mobs = zone.as<sol::table>()["mobs"].get<sol::optional<sol::table>>();
            if (!mobs) continue;
            for (const auto& [mobName, script] : *mobs)
            {
                if (!script.is<sol::table>()) continue;
                const auto phList = script.as<sol::table>()["phList"].get<sol::optional<sol::table>>();
                if (!phList) continue;
                for (const auto& [ph, nm] : *phList)
                {
                    if (!ph.is<uint32>()) continue;
                    if (nm.is<uint32>())
                        nmOf.try_emplace(ph.as<uint32>(), nm.as<uint32>());
                    else if (nm.is<sol::table>())
                        if (const auto first = nm.as<sol::table>()[1].get<sol::optional<uint32>>()) nmOf.try_emplace(ph.as<uint32>(), *first);
                }
            }
        }
        return nmOf;
    }

    // GetBaseExp at every level difference out to the widest, so the table's own size comes from the server.
    void WriteBaseExp(FILE* out)
    {
        std::fprintf(out, "\"firstDifference\":%d,\"baseExp\":[", -kWidestDifference);
        for (int difference = -kWidestDifference; difference <= kWidestDifference; ++difference)
        {
            std::fputs(difference == -kWidestDifference ? "[" : ",[", out);
            for (int column = 0; column < kColumns; ++column)
            {
                const int playerLevel = column * kLevelsPerColumn + 1;
                std::fprintf(out, "%s%u", column ? "," : "", charutils::GetBaseExp(playerLevel, playerLevel + difference));
            }
            std::fputs("]", out);
        }
        std::fputs("]", out);
    }

    // One loot entry as [DROP_TYPE, item ID, rate, fixed rate].
    void WriteDropItem(FILE* out, const DropItem_t& item, bool first)
    {
        std::fprintf(out, "%s[%u,%u,%u,%s]", first ? "" : ",", static_cast<unsigned>(item.DropType),
            static_cast<unsigned>(item.ItemID), static_cast<unsigned>(item.DropRate), item.hasFixedRate ? "true" : "false");
    }

    // Each drop list as the server holds it: its ungrouped entries (drops, steals and despoils), then its groups, each
    // with its rate and the entries one roll of the group picks among.
    void WriteDropLists(FILE* out, const std::vector<const DropList_t*>& lists)
    {
        std::fputs("\"dropLists\":[", out);
        for (size_t i = 0; i < lists.size(); ++i)
        {
            std::fputs(i ? ",\n{\"items\":[" : "\n{\"items\":[", out);
            for (size_t j = 0; j < lists[i]->Items.size(); ++j)
                WriteDropItem(out, lists[i]->Items[j], j == 0);
            std::fputs("],\"groups\":[", out);
            for (size_t j = 0; j < lists[i]->Groups.size(); ++j)
            {
                const DropGroup_t& group = lists[i]->Groups[j];
                std::fprintf(out, "%s{\"rate\":%u,\"fixed\":%s,\"items\":[", j ? "," : "",
                    static_cast<unsigned>(group.GroupRate), group.hasFixedRate ? "true" : "false");
                for (size_t k = 0; k < group.Items.size(); ++k)
                    WriteDropItem(out, group.Items[k], k == 0);
                std::fputs("]}", out);
            }
            std::fputs("]}", out);
        }
        std::fputs("\n]", out);
    }

    // Every TP move list a mob reads, with its moves, and each move's name as the server holds it.
    void WriteSkillLists(FILE* out, const std::vector<uint16>& lists)
    {
        std::fputs("\"skillLists\":[", out);
        std::vector<uint16> moves;
        for (size_t i = 0; i < lists.size(); ++i)
        {
            std::fprintf(out, "%s\n{\"id\":%u,\"skills\":[", i ? "," : "", lists[i]);
            const std::vector<uint16>& skills = battleutils::GetMobSkillList(lists[i]);
            for (size_t j = 0; j < skills.size(); ++j)
            {
                std::fprintf(out, "%s%u", j ? "," : "", skills[j]);
                if (std::ranges::find(moves, skills[j]) == moves.end()) moves.push_back(skills[j]);
            }
            std::fputs("]}", out);
        }
        std::fputs("\n],\n\"mobSkills\":[", out);
        bool first = true;
        for (const uint16 id : moves)
        {
            CMobSkill* skill = battleutils::GetMobSkill(id);
            if (skill == nullptr) continue;
            std::fprintf(out, "%s\n{\"id\":%u,\"name\":\"%s\"}", first ? "" : ",", id, JsonEscape(skill->getName()).c_str());
            first = false;
        }
        std::fputs("\n]", out);
    }

    // Each spell list a mob casts from, numbered from 1 in the order the mobs name them: [spell, min level, max level].
    void WriteSpellLists(FILE* out, const std::vector<const CMobSpellList*>& lists)
    {
        std::fputs("\"spellLists\":[", out);
        for (size_t i = 0; i < lists.size(); ++i)
        {
            std::fputs(i ? ",\n[" : "\n[", out);
            bool first = true;
            for (const MobSpell_t& spell : lists[i]->m_spellList)
            {
                std::fprintf(out, "%s[%u,%u,%u]", first ? "" : ",", static_cast<unsigned>(spell.spellId), spell.min_level,
                    spell.max_level);
                first = false;
            }
            std::fputs("]", out);
        }
        std::fputs("\n]", out);
    }

    // isSitting reads only the entity's animation: try each on one mob, then give it its own back.
    void WriteSittingAnimations(FILE* out, CMobEntity* PMob)
    {
        const xi::Animation own = PMob->animation;
        bool first              = true;
        std::fputs("\"sittingAnimations\":[", out);
        for (int animation = 0; animation < kAnimations; ++animation)
        {
            PMob->animation = static_cast<xi::Animation>(animation);
            if (!PMob->isSitting()) continue;
            std::fprintf(out, "%s%d", first ? "" : ",", animation);
            first = false;
        }
        std::fputs("]", out);
        PMob->animation = own;
    }

    // Every setting of main.lua and map.lua that is a boolean or a number, as the server ran with it. Strings are
    // left out: some hold the game's Shift-JIS text, which JSON cannot carry.
    void WriteSettings(FILE* out)
    {
        std::fputs("\"settings\":{", out);
        bool firstTable = true;
        for (const char* table : {"main", "map"})
        {
            std::fprintf(out, "%s\"%s\":{", firstTable ? "" : ",", table);
            firstTable = false;
            bool first        = true;
            const auto values = ::lua["xi"]["settings"][table].get<sol::optional<sol::table>>();
            if (values)
                for (const auto& [key, value] : *values)
                {
                    if (!key.is<std::string>()) continue;
                    std::string text;
                    if (value.get_type() == sol::type::boolean)
                        text = value.as<bool>() ? "true" : "false";
                    else if (value.get_type() == sol::type::number)
                    {
                        char number[32];
                        std::snprintf(number, sizeof(number), "%.17g", value.as<double>());
                        text = number;
                    }
                    else
                        continue;
                    std::fprintf(out, "%s\"%s\":%s", first ? "" : ",", JsonEscape(key.as<std::string>()).c_str(), text.c_str());
                    first = false;
                }
            std::fputs("}", out);
        }
        std::fputs("}", out);
    }

    void WriteRules(FILE* out, CMobEntity* PMob)
    {
        std::fputs("\"rules\":{", out);
        WriteBaseExp(out);
        std::fputs(",\"difficulty\":[", out);
        for (size_t i = 0; i < ExpToDifficultyTable.size(); ++i)
            std::fprintf(out, "%s{\"minExp\":%u,\"con\":%u}", i ? "," : "", static_cast<unsigned>(ExpToDifficultyTable[i].first),
                static_cast<unsigned>(ExpToDifficultyTable[i].second));
        std::fprintf(out, "],\"incrediblyEasyPrey\":{\"minLevel\":%u,\"minExp\":%u},",
            static_cast<unsigned>(IncrediblyEasyPreyCheck.first), static_cast<unsigned>(IncrediblyEasyPreyCheck.second));
        WriteSittingAnimations(out, PMob);
        std::fputs("}", out);
    }
}

struct Aggro
{
    bool aggro;
    int alwaysAggro;
    int noAggro;
};

class HeadsUpDumpModule : public CPPModule
{
    int ticks = 0;
    // The first state the dump saw each mob attack in, before its spawn or while it lived: some change on a timer
    // (the Ghrahs turn aggressive and back every minute) or with the hour (some sleep at night).
    std::unordered_map<CMobEntity*, Aggro> seenAttacking;

    static Aggro AggroOf(CMobEntity* PMob)
    {
        return Aggro{ PMob->m_Aggro, PMob->getMobMod(xi::MobMod::AlwaysAggro), PMob->getMobMod(xi::MobMod::NoAggro) };
    }

    // The rule compact.py's attacks() applies.
    static bool Attacks(const Aggro& a)
    {
        return (a.aggro || a.alwaysAggro > 0) && a.noAggro <= 0;
    }

    void Note(CMobEntity* PMob)
    {
        const Aggro now = AggroOf(PMob);
        if (Attacks(now)) seenAttacking.try_emplace(PMob, now);
    }

    void Sample()
    {
        zoneutils::ForEachZone([&](CZone* PZone) { PZone->ForEachMob([&](CMobEntity* PMob) { Note(PMob); }); });
    }

    // Spawn every mob not up yet (most NMs, lottery and scripted spawns) the way the server does, so what their spawn
    // scripts set is in the dump too. Collected first: a spawn script may add entities to a zone.
    void SpawnTheRest()
    {
        std::vector<CMobEntity*> unspawned;
        zoneutils::ForEachZone([&](CZone* PZone) {
            PZone->ForEachMob([&](CMobEntity* PMob) {
                if (!PMob->PAI->IsSpawned()) unspawned.push_back(PMob);
            });
        });
        size_t spawned = 0;
        for (CMobEntity* PMob : unspawned)
        {
            if (PMob->PAI->IsSpawned()) continue;
            Note(PMob);
            PMob->Spawn();
            ++spawned;
        }
        ShowInfo("headsup_dump: spawned %zu mobs to run their spawn scripts", spawned);
    }

    void OnInit() override
    {
    }

    void OnTimeServerTick() override
    {
        ++ticks;
        if (ticks == kSpawnTick) SpawnTheRest();
        if (ticks >= kSpawnTick && (ticks - kSpawnTick) % kSampleTicks == 0) Sample();
        if (ticks != kDumpTick) return;

        const char* path = std::getenv("HEADSUP_DUMP_PATH");
        if (path == nullptr)
        {
            ShowError("headsup_dump: HEADSUP_DUMP_PATH is not set; nothing was written");
            std::_Exit(1);
        }
        FILE* out = std::fopen(path, "wb");
        if (out == nullptr)
        {
            ShowError("headsup_dump: cannot open %s: %s", path, std::strerror(errno));
            std::_Exit(1);
        }

        const std::unordered_map<uint32, uint32> nmOf = Placeholders();
        size_t count       = 0;
        CMobEntity* anyMob = nullptr;
        // Every mob of one template shares its drop list, so each list is written once and the mobs give its index.
        std::vector<const DropList_t*> dropLists;
        std::unordered_map<const DropList_t*, size_t> dropListIndex;
        std::vector<uint16> skillLists;
        std::vector<const CMobSpellList*> spellLists;
        std::unordered_map<const CMobSpellList*, size_t> spellListIndex;
        std::fputs("{\"mobs\":[\n", out);
        zoneutils::ForEachZone([&](CZone* PZone) {
            PZone->ForEachMob([&](CMobEntity* PMob) {
                // Idle mobs cast buffs on themselves (Protect, spikes, Regen): the record is of the mob without them.
                PMob->StatusEffectContainer->KillAllStatusEffect();
                const auto respawn = std::chrono::duration_cast<std::chrono::seconds>(PMob->m_RespawnTime).count();
                const bool follows = (PMob->m_roamFlags & xi::RoamFlag::Follow) != xi::RoamFlag::None;
                std::fprintf(out,
                    "%s{\"id\":%u,\"zone\":%u,\"name\":\"%s\",\"minLevel\":%u,\"maxLevel\":%u,\"respawn\":%lld,"
                    "\"aggro\":%s,\"alwaysAggro\":%d,\"noAggro\":%d,\"type\":%u,"
                    "\"link\":%s,\"detects\":%d,\"trueDetection\":%s,\"expLevelMod\":%d,\"follows\":%s,\"placeholderOf\":%u",
                    count ? ",\n" : "", PMob->id, static_cast<unsigned>(PZone->GetID()),
                    JsonEscape(PMob->getPacketName()).c_str(), PMob->m_minLevel, PMob->m_maxLevel,
                    static_cast<long long>(respawn), PMob->m_Aggro ? "true" : "false",
                    PMob->getMobMod(xi::MobMod::AlwaysAggro), PMob->getMobMod(xi::MobMod::NoAggro),
                    static_cast<unsigned>(PMob->m_Type), PMob->m_Link != 0 ? "true" : "false",
                    static_cast<int>(PMob->getMobMod(xi::MobMod::Detection)), PMob->m_TrueDetection ? "true" : "false",
                    static_cast<int>(PMob->getMod(xi::Mod::EXP_LVL_MOD)), follows ? "true" : "false",
                    nmOf.contains(PMob->id) ? nmOf.at(PMob->id) : 0u);
                std::fprintf(out, ",\"immunity\":%u", static_cast<unsigned>(PMob->m_Immunity));
                // The list the server rolls at death and reads for Steal: the template's, else the numbered SQL list a
                // script gave it (an empty one when none did).
                const DropList_t* drops = PMob->dropList();
                if (drops != nullptr && (!drops->Items.empty() || !drops->Groups.empty()))
                {
                    const auto [entry, added] = dropListIndex.try_emplace(drops, dropLists.size());
                    if (added) dropLists.push_back(drops);
                    std::fprintf(out, ",\"dropList\":%zu", entry->second);
                }
                WriteMods(out, "sdt", PMob, kSdtMods);
                WriteMods(out, "elementRanks", PMob, kElementRankMods);
                WriteMods(out, "statusRanks", PMob, kStatusRankMods);
                WriteMods(out, "elementMeva", PMob, kElementMevaMods);
                WriteModsWith(out, "statusMeva", PMob, kStatusMevaMods, xi::Mod::STATUS_MEVA);
                WriteModsWith(out, "statusResist", PMob, kStatusResistMods, xi::Mod::STATUSRES);
                if (const auto seen = seenAttacking.find(PMob); seen != seenAttacking.end())
                    std::fprintf(out, ",\"seenAttacking\":{\"aggro\":%s,\"alwaysAggro\":%d,\"noAggro\":%d}",
                        seen->second.aggro ? "true" : "false", seen->second.alwaysAggro, seen->second.noAggro);
                // Its script is scripts/zones/<zoneName>/mobs/<script>.lua, as luautils finds it.
                std::fprintf(out, ",\"script\":\"%s\",\"zoneName\":\"%s\",\"jobs\":[%d,%d]",
                    JsonEscape(PMob->getName()).c_str(), JsonEscape(PZone->getName()).c_str(),
                    static_cast<int>(PMob->GetMJob()), static_cast<int>(PMob->GetSJob(true)));
                if (const auto& window = PMob->spawnWindow())
                    std::fprintf(out, ",\"window\":[%u,%u]", window->spawnHour, window->despawnHour);
                if (const SpawnSlot* slot = PMob->GetSpawnSlot())
                    for (const SpawnSlotEntry& e : slot->GetEntries())
                        if (e.mob == PMob && e.spawnChance > 0)
                            std::fprintf(out, ",\"lottery\":[%u,%lld]", static_cast<unsigned>(e.spawnChance),
                                static_cast<long long>(std::chrono::duration_cast<std::chrono::seconds>(e.cooldown).count()));
                std::fprintf(out, ",\"charmable\":%d,\"senses\":[%d,%d],\"addEffect\":%d,\"skillList\":%d",
                    PMob->getMobMod(xi::MobMod::Charmable), PMob->getMobMod(xi::MobMod::SightRange),
                    PMob->getMobMod(xi::MobMod::SoundRange), PMob->getMobMod(xi::MobMod::AddEffect),
                    PMob->getMobMod(xi::MobMod::SkillList));
                WriteMods(out, "damageMods", PMob, kDamageMods);
                WriteMods(out, "absorb", PMob, kAbsorbMods);
                WriteMods(out, "traits", PMob, kTraitMods);
                if (const int16 list = PMob->getMobMod(xi::MobMod::SkillList); list > 0 &&
                    std::ranges::find(skillLists, static_cast<uint16>(list)) == skillLists.end())
                    skillLists.push_back(static_cast<uint16>(list));
                if (const CMobSpellList* spells = PMob->m_SpellListContainer; spells != nullptr && !spells->m_spellList.empty())
                {
                    const auto [entry, added] = spellListIndex.try_emplace(spells, spellLists.size() + 1);
                    if (added) spellLists.push_back(spells);
                    std::fprintf(out, ",\"spellList\":%zu", entry->second);
                }
                WriteLevelStats(out, PMob);
                std::fputs("}", out);
                anyMob = PMob;
                ++count;
            });
        });
        if (anyMob == nullptr)
        {
            ShowError("headsup_dump: no zone loaded any mob; the dump is incomplete");
            std::_Exit(1);
        }
        std::fputs("\n],\n", out);
        WriteDropLists(out, dropLists);
        std::fputs(",\n", out);
        WriteSkillLists(out, skillLists);
        std::fputs(",\n", out);
        WriteSpellLists(out, spellLists);
        std::fputs(",\n", out);
        WriteItems(out);
        std::fputs(",\n", out);
        WriteRules(out, anyMob);
        std::fputs(",\n", out);
        WriteSettings(out);
        std::fputs("}\n", out);
        if (std::ferror(out) != 0 || std::fclose(out) != 0)
        {
            ShowError("headsup_dump: writing %s failed: %s; the file is incomplete", path, std::strerror(errno));
            std::_Exit(1);
        }
        ShowInfo("headsup_dump: wrote %zu mobs (%zu lottery placeholders, %zu drop lists) and the rules to %s", count,
            nmOf.size(), dropLists.size(), path);
        std::_Exit(0);
    }
};

REGISTER_CPP_MODULE(HeadsUpDumpModule);
