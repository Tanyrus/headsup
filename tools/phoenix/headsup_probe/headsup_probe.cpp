/************************************************************************
 * headsup_probe: a map server module for HeadsUp's data refresh (tools/phoenix/refresh.sh).
 *
 * Does nothing unless HEADSUP_PROBE_LUA and HEADSUP_PROBE_OUT are set. At tick 30, once every zone has loaded, it
 * hands tools/phoenix/npc_probe.lua (HEADSUP_PROBE_LUA) every ??? a player can see as HEADSUP_PROBE_TARGETS: each
 * NPC named ??? that content restriction leaves on and that is not a cutscene-only prop. The script probes them and
 * writes HEADSUP_PROBE_OUT, then the server exits.
 ************************************************************************/

#include "common/lua.h"
#include "map/entities/npc_entity.h"
#include "map/utils/moduleutils.h"
#include "map/utils/zoneutils.h"
#include "map/zone.h"

#include <cstdio>
#include <cstdlib>

namespace
{
    constexpr int kProbeTick = 30;

    // Ends the server once the probe is done. The server's logger writes from a thread of its own, which _Exit cuts
    // off, so the last word goes to stderr.
    [[noreturn]] void Finish(int status, const char* message)
    {
        std::fprintf(stderr, "headsup_probe: %s\n", message);
        std::fflush(stderr);
        std::_Exit(status);
    }
}

class HeadsUpProbeModule : public CPPModule
{
    int ticks = 0;

    void OnInit() override
    {
    }

    void OnTimeServerTick() override
    {
        const char* script = std::getenv("HEADSUP_PROBE_LUA");
        if (script == nullptr || std::getenv("HEADSUP_PROBE_OUT") == nullptr) return;
        if (++ticks != kProbeTick) return;

        sol::table targets = ::lua.create_table();
        int count          = 0;
        zoneutils::ForEachZone([&](CZone* PZone) {
            PZone->ForEachNpc([&](CNpcEntity* PNpc) {
                if (PNpc->getPacketName() != "???" || PNpc->contentDisabled() || PNpc->status == xi::Status::CutsceneOnly)
                    return;
                sol::table target  = ::lua.create_table();
                target["id"]       = PNpc->id;
                target["zone"]     = static_cast<int>(PZone->GetID());
                target["zoneName"] = PZone->getName();
                target["script"]   = PNpc->getName();
                targets[++count]   = target;
            });
        });
        ::lua["HEADSUP_PROBE_TARGETS"] = targets;

        auto result = ::lua.safe_script_file(script, sol::script_pass_on_error);
        if (!result.valid())
        {
            const sol::error err = result;
            Finish(1, err.what());
        }
        Finish(0, "done");
    }
};

REGISTER_CPP_MODULE(HeadsUpProbeModule);
