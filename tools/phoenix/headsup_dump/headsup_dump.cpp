/************************************************************************
 * headsup_dump
 *
 * A Phoenix C++ module added by headsup's tools/phoenix/refresh.sh. Once
 * zones have spawned their mobs, it spawns the rest, so every mob runs its
 * spawn script, and watches them all live a while, noting any it sees
 * attacking, then writes every loaded mob and the /check and aggro rules
 * the server loaded to $HEADSUP_DUMP_PATH as JSON and exits.
 ************************************************************************/

#include "map/ai/ai_container.h"
#include "map/entities/mob_entity.h"
#include "map/utils/charutils.h"
#include "map/utils/moduleutils.h"
#include "map/utils/zoneutils.h"
#include "map/zone.h"

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

        size_t count       = 0;
        CMobEntity* anyMob = nullptr;
        std::fputs("{\"mobs\":[\n", out);
        zoneutils::ForEachZone([&](CZone* PZone) {
            PZone->ForEachMob([&](CMobEntity* PMob) {
                const auto respawn = std::chrono::duration_cast<std::chrono::seconds>(PMob->m_RespawnTime).count();
                const bool follows = (PMob->m_roamFlags & xi::RoamFlag::Follow) != xi::RoamFlag::None;
                std::fprintf(out,
                    "%s{\"id\":%u,\"zone\":%u,\"name\":\"%s\",\"minLevel\":%u,\"maxLevel\":%u,\"respawn\":%lld,"
                    "\"aggro\":%s,\"alwaysAggro\":%d,\"noAggro\":%d,\"type\":%u,"
                    "\"link\":%s,\"detects\":%d,\"trueDetection\":%s,\"expLevelMod\":%d,\"follows\":%s",
                    count ? ",\n" : "", PMob->id, static_cast<unsigned>(PZone->GetID()),
                    JsonEscape(PMob->getPacketName()).c_str(), PMob->m_minLevel, PMob->m_maxLevel,
                    static_cast<long long>(respawn), PMob->m_Aggro ? "true" : "false",
                    PMob->getMobMod(xi::MobMod::AlwaysAggro), PMob->getMobMod(xi::MobMod::NoAggro),
                    static_cast<unsigned>(PMob->m_Type), PMob->m_Link != 0 ? "true" : "false",
                    static_cast<int>(PMob->getMobMod(xi::MobMod::Detection)), PMob->m_TrueDetection ? "true" : "false",
                    static_cast<int>(PMob->getMod(xi::Mod::EXP_LVL_MOD)), follows ? "true" : "false");
                if (const auto seen = seenAttacking.find(PMob); seen != seenAttacking.end())
                    std::fprintf(out, ",\"seenAttacking\":{\"aggro\":%s,\"alwaysAggro\":%d,\"noAggro\":%d}",
                        seen->second.aggro ? "true" : "false", seen->second.alwaysAggro, seen->second.noAggro);
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
        WriteRules(out, anyMob);
        std::fputs("}\n", out);
        if (std::ferror(out) != 0 || std::fclose(out) != 0)
        {
            ShowError("headsup_dump: writing %s failed: %s; the file is incomplete", path, std::strerror(errno));
            std::_Exit(1);
        }
        ShowInfo("headsup_dump: wrote %zu mobs and the rules to %s", count, path);
        std::_Exit(0);
    }
};

REGISTER_CPP_MODULE(HeadsUpDumpModule);
