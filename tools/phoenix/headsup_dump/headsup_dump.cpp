/************************************************************************
 * headsup_dump
 *
 * A Phoenix C++ module added by headsup's tools/phoenix/refresh.sh. Once
 * the map server has been running for 20 time-server ticks (zones have
 * spawned their mobs and run their spawn scripts), it writes every loaded
 * mob to $HEADSUP_DUMP_PATH as JSON and exits the process.
 ************************************************************************/

#include "map/ai/ai_container.h"
#include "map/entities/mob_entity.h"
#include "map/utils/moduleutils.h"
#include "map/utils/zoneutils.h"
#include "map/zone.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace
{
    constexpr int kDumpTick = 20;

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
}

class HeadsUpDumpModule : public CPPModule
{
    int ticks = 0;

    void OnInit() override
    {
    }

    void OnTimeServerTick() override
    {
        if (++ticks != kDumpTick) return;

        const char* path = std::getenv("HEADSUP_DUMP_PATH");
        FILE* out        = std::fopen(path != nullptr ? path : "headsup_mobs.json", "wb");
        if (out == nullptr)
        {
            ShowError("headsup_dump: cannot open the output file");
            std::_Exit(1);
        }

        size_t count = 0;
        std::fputs("[\n", out);
        zoneutils::ForEachZone([&](CZone* PZone) {
            PZone->ForEachMob([&](CMobEntity* PMob) {
                const auto respawn = std::chrono::duration_cast<std::chrono::seconds>(PMob->m_RespawnTime).count();
                std::fprintf(out,
                    "%s{\"id\":%u,\"zone\":%u,\"name\":\"%s\",\"minLevel\":%u,\"maxLevel\":%u,\"respawn\":%lld,"
                    "\"aggro\":%s,\"alwaysAggro\":%d,\"noAggro\":%d,\"neutral\":%s,\"type\":%u,\"spawned\":%s,"
                    "\"link\":%s,\"detects\":%d,\"trueDetection\":%s}",
                    count ? ",\n" : "", PMob->id, static_cast<unsigned>(PZone->GetID()),
                    JsonEscape(PMob->getPacketName()).c_str(), PMob->m_minLevel, PMob->m_maxLevel,
                    static_cast<long long>(respawn), PMob->m_Aggro ? "true" : "false",
                    PMob->getMobMod(xi::MobMod::AlwaysAggro), PMob->getMobMod(xi::MobMod::NoAggro),
                    PMob->m_neutral ? "true" : "false", static_cast<unsigned>(PMob->m_Type),
                    PMob->PAI->IsSpawned() ? "true" : "false", PMob->m_Link != 0 ? "true" : "false",
                    static_cast<int>(PMob->getMobMod(xi::MobMod::Detection)), PMob->m_TrueDetection ? "true" : "false");
                ++count;
            });
        });
        std::fputs("\n]\n", out);
        std::fclose(out);
        ShowInfo("headsup_dump: wrote %zu mobs", count);
        std::_Exit(0);
    }
};

REGISTER_CPP_MODULE(HeadsUpDumpModule);
