#pragma once

#include "Ashita.h"
#include "check.h"
#include "game_cursor.h"
#include "game_names.h"
#include "game_state.h"
#include "nameplate_render.h"
#include "native_hook.h"
#include "native_locate.h"
#include "settings.h"
#include "tracker.h"

#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>

namespace headsup
{
    // An entity's name as the game has it: empty past the entity map's end or where it holds no entity.
    const char* EntityName(IEntity* entity, uint32_t index);

    // What /hu debug reads, as the plugin holds it.
    struct DebugSources
    {
        IAshitaCore* core;
        const Tracker& tracker;
        const NameHook& hook;
        const GameNames& names;
        const NameplateRenderer& plates;
        const GameState& state;
        const CheckResults& checks;
        const CursorTargets& cursor;
        const PlayerState& player;
        const Settings& settings;
        float menuWidth, menuHeight;
        uint16_t zone;
        double now;
        double version;
    };

    // What every outline and nameplate shows, after the connection's counters (net), the last conquest packet, your
    // Signet's timer, your party and the target cursor.
    void WriteDebugReport(const DebugSources& sources, const std::optional<NetCounters>& net, std::FILE* out);

    // The frame capture that follows a report: its heading, and one frame's line, frameMs after the frame before.
    extern const char* const kCaptureHeading;
    std::string CaptureFrameLine(const DebugSources& sources, int number, double frameMs, bool drewInScene);
}
