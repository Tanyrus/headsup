#include "debug_report.h"

#include "bar_job.h"
#include "bar_party.h"
#include "bar_signet.h"
#include "con.h"
#include "icons.h"
#include "mobdata.h"
#include "name_frame.h"
#include "player_status.h"

#include <chrono>

namespace headsup
{
    namespace
    {
        constexpr int kCapturedPlatesPerFrame = 16;
        constexpr uint32_t kTargetSlots       = 2;
    }

    const char* EntityName(IEntity* entity, uint32_t index)
    {
        if (index >= entity->GetEntityMapSize() || entity->GetRawEntity(index) == nullptr) return "";
        const char* name = entity->GetName(index);
        return name != nullptr ? name : "";
    }

    const char* const kCaptureHeading =
        "\nframe  dt(ms)  game's names: hooked replaced | nameplates shown, S if drawn into the scene, s<scene to screen scale> | per entity "
        "with a game name: index m<body meshes> c<name color> (name box: center, top-bottom)\n";

    void WriteDebugReport(const DebugSources& s, const std::optional<NetCounters>& net, std::FILE* out)
    {
        IEntity* entity = s.core->GetMemoryManager()->GetEntity();
        auto onOff      = [](bool on) { return on ? "on" : "off"; };
        std::fprintf(out,
            "headsup %.2f: back buffer %.0fx%.0f, scene to screen scale %.3f x %.3f, player level %d%s, %u outlined, %u nameplates, nameplates %s outlines %s\n",
            s.version, s.names.BackBufferWidth(), s.names.BackBufferHeight(), s.names.TargetScaleX(), s.names.TargetScaleY(),
            s.player.level, s.player.sitting ? " (sitting)" : "", s.tracker.OutlinedCount(),
            static_cast<unsigned>(s.plates.LastShown().size()), onOff(s.settings.nameplates), onOff(s.settings.outlines));
        if (net)
            std::fprintf(out, "connection: R %d S %d meter %d\n", net->received, net->sent, net->health);
        else
            std::fprintf(out, "connection: counters not read\n");
        const std::vector<uint8_t>& conquest = s.state.LastConquestPacket();
        std::fprintf(out, "last 0x05E, %u bytes:", static_cast<unsigned>(conquest.size()));
        for (size_t i = 0; i < conquest.size(); ++i)
            std::fprintf(out, "%s%02X", i % 16 == 0 ? "\n  " : " ", conquest[i]);
        std::fprintf(out, "\n");
        IPlayer* player       = s.core->GetMemoryManager()->GetPlayer();
        const int16_t* icons  = player->GetStatusIcons();
        const uint32_t* times = player->GetStatusTimers();
        const int64_t unixMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        bool influence = false;
        for (int i = 0; icons != nullptr && times != nullptr && i < kStatusIconSlots; ++i)
            if (icons[i] == kSignetEffect || icons[i] == kSanctionEffect)
            {
                const std::optional<double> left = StatusSecondsLeft(times[i], unixMs);
                std::fprintf(out, "status %d in icon slot %d: timer %08X, %.0f s left\n", icons[i], i,
                    static_cast<unsigned>(times[i]), left ? *left : -1.0);
                influence = true;
            }
        if (!influence) std::fprintf(out, "status %d or %d: in no icon slot\n", kSignetEffect, kSanctionEffect);
        {
            IParty* party = s.core->GetMemoryManager()->GetParty();
            for (uint32_t m = 0; m < static_cast<uint32_t>(kPartySize); ++m)
            {
                if (party->GetMemberIsActive(m) == 0) continue;
                const char* name = party->GetMemberName(m);
                std::fprintf(out, "party %u: '%s' %s %u / %s %u HP %u (%u%%) MP %u (%u%%) TP %u zone %u (yours %u)\n", m,
                    name != nullptr ? name : "", JobName(party->GetMemberMainJob(m)), party->GetMemberMainJobLevel(m),
                    JobName(party->GetMemberSubJob(m)), party->GetMemberSubJobLevel(m), party->GetMemberHP(m),
                    party->GetMemberHPPercent(m), party->GetMemberMP(m), party->GetMemberMPPercent(m), party->GetMemberTP(m),
                    party->GetMemberZone(m), s.zone);
            }
        }
        {
            // Where the target cursor went: on a nameplate, at the game's own arrow anchor, or nowhere.
            const CursorTargets& c = s.cursor;
            const auto held        = [](uint16_t index) {
                return index == kModelTarget ? std::string("no entity") : "#" + std::to_string(index);
            };
            std::fprintf(out, "cursor: target %s sub %s%s%s, menu %.0fx%.0f, anchors %s (%.1f,%.1f) sub (%.1f,%.1f); drawn:",
                held(c.target).c_str(), held(c.subTarget).c_str(), c.locked ? " locked" : "",
                c.gameArrowsShown ? ", the game's arrows (not hooked)" : "", s.menuWidth, s.menuHeight,
                c.anchored ? "at" : "unknown", c.anchorX, c.anchorY, c.subAnchorX, c.subAnchorY);
            for (const CursorName& name : s.plates.CursorNames())
                std::fprintf(out, " %s %s", held(name.index).c_str(), name.name.valid ? "on its nameplate" : "at its anchor");
            std::fprintf(out, "%s\n", s.plates.CursorNames().empty() ? " none" : "");
            // The game's own two slots, as it holds them: a target need not be an entity.
            ITarget* target = s.core->GetMemoryManager()->GetTarget();
            for (uint32_t slot = 0; slot < kTargetSlots; ++slot)
                std::fprintf(out, "target slot %u: #%u id %u, active %u, model %u, free arrow %u at (%.1f,%.1f,%.1f)\n", slot,
                    target->GetTargetIndex(slot), target->GetServerId(slot), target->GetIsActive(slot),
                    target->GetIsModelActor(slot), target->GetIsArrowActive(slot), target->GetArrowPositionX(slot),
                    target->GetArrowPositionZ(slot), target->GetArrowPositionY(slot));
        }
        if (const ActorInfo* t = s.tracker.FindIndex(s.cursor.target); t != nullptr)
            if (const PlayerCheck* c = s.state.PlayerCheckOf(t->serverId))
                std::fprintf(out, "check of %s: jobs %u/%u levels %u/%u master %u linkshell \"%s\" title %u\n",
                    c->message.name.c_str(), c->general.mainJob, c->general.subJob, c->general.mainLevel, c->general.subLevel,
                    c->general.masterLevel, c->general.linkshell.c_str(), c->message.title);
        for (const ActorPtr actor : s.tracker.Actors())
        {
            const ActorInfo* info   = s.tracker.Find(actor);
            const NameFrame* frame = info != nullptr ? s.hook.Drawn(info->index) : nullptr;
            if (info == nullptr || (!info->outline && frame == nullptr)) continue;
            const char* name        = EntityName(entity, info->index);
            const uint32_t serverId = entity->GetServerId(info->index);
            const MobRecord* mob    = MobById(serverId);
            std::fprintf(out, "#%u %u '%s' ", info->index, serverId, name);
            if (mob != nullptr)
                std::fprintf(out, "data Lv %u-%u flags %u detects %u respawn %u", mob->minLevel, mob->maxLevel, mob->flags,
                    mob->detects, mob->respawn);
            else
                std::fprintf(out, "no data");
            // The level and con as the bar's Target widget shows them, and every icon $detect could draw.
            std::fprintf(out, " | %s outline %08X label '%s' %08X icons %d", info->alive ? "alive" : "dead",
                static_cast<unsigned>(info->argb), info->level.text,
                static_cast<unsigned>(ToArgb(s.settings.palette.toughness[static_cast<int>(info->level.shade)])),
                info->alive ? IconsFor(info->mob, kAllDetection).count : 0);
            if (const CheckResult* checked = s.checks.Result(serverId, s.now))
                std::fprintf(out, " | checked Lv %d %s", checked->level, Abbrev(checked->con));
            if (frame != nullptr)
            {
                const ScreenBox plate = NameFromFrame(*frame, s.names.TargetScaleX(), s.names.TargetScaleY()).box;
                std::fprintf(out, " | game name (%.1f,%.1f)-(%.1f,%.1f)", plate.minX, plate.minY, plate.maxX, plate.maxY);
            }
            else
                std::fprintf(out, " | no game name");
            for (const auto& shown : s.plates.LastShown())
            {
                if (shown.index != info->index) continue;
                std::fprintf(out, " | name %08X %dpx at (%.1f,%.1f)", static_cast<unsigned>(shown.nameColor), shown.nameHeight,
                    shown.nameX, shown.nameY);
                for (int i = 0; i < shown.lineCount; ++i)
                {
                    const auto& line = shown.lines[i];
                    std::fprintf(out, " | line %d '%s' at (%.1f,%.1f) %d px, %d icons", i + 1, line.text.c_str(), line.x, line.y,
                        line.height, line.icons);
                }
            }
            std::fprintf(out, "\n");
        }
    }

    std::string CaptureFrameLine(const DebugSources& s, int number, double frameMs, bool drewInScene)
    {
        const NameStats stats = s.hook.Stats();
        char part[200];
        std::snprintf(part, sizeof(part), "f%03d %6.1f  %3u %3u | %2u %c s%.3f |", number, frameMs, stats.names, stats.replaced,
            static_cast<unsigned>(s.plates.LastShown().size()), drewInScene ? 'S' : '-', s.names.TargetScaleY());
        std::string line = part;
        int listed       = 0;
        for (const ActorPtr actor : s.tracker.Actors())
        {
            const ActorInfo* info  = s.tracker.Find(actor);
            const NameFrame* frame = info != nullptr ? s.hook.Drawn(info->index) : nullptr;
            if (frame == nullptr || ++listed > kCapturedPlatesPerFrame) continue;
            const DrawnName name = NameFromFrame(*frame, s.names.TargetScaleX(), s.names.TargetScaleY());
            std::snprintf(part, sizeof(part), " %u m%u c%06X (%.0f,%.0f-%.0f)", info->index, s.names.MeshDraws(info->index),
                static_cast<unsigned>(name.color & 0xFFFFFF), name.box.CenterX(), name.box.minY, name.box.maxY);
            line += part;
        }
        return line + "\n";
    }
}
