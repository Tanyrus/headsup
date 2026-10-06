#include "Ashita.h"

#include "examine.h"
#include "labels_render.h"
#include "menu.h"
#include "mobdata.h"
#include "outline.h"
#include "settings.h"
#include "tracker.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

namespace
{
    constexpr const char* kName        = "aggroglow";
    constexpr const char* kConfigAlias = "aggroglow";
    constexpr const char* kConfigFile  = "aggroglow/settings.ini"; // relative to Ashita's config folder
    constexpr const char* kSection     = "settings";

    // Settings persistence through Ashita's configuration manager.
    class AshitaStore final : public aggroglow::SettingsStore
    {
    public:
        explicit AshitaStore(IConfigurationManager* config)
            : m_Config(config)
        {}

        bool GetBool(const char* key, bool fallback) override { return m_Config->GetBool(kConfigAlias, kSection, key, fallback); }
        float GetFloat(const char* key, float fallback) override { return m_Config->GetFloat(kConfigAlias, kSection, key, fallback); }
        void Set(const char* key, const char* value) override { m_Config->SetValue(kConfigAlias, kSection, key, value); }

    private:
        IConfigurationManager* m_Config;
    };

    std::vector<std::string> SplitLower(const char* command)
    {
        std::vector<std::string> args;
        std::string current;
        for (const char* p = command; *p; ++p)
        {
            if (*p == ' ' || *p == '\t')
            {
                if (!current.empty()) args.push_back(current), current.clear();
            }
            else
                current += static_cast<char>(std::tolower(static_cast<unsigned char>(*p)));
        }
        if (!current.empty()) args.push_back(current);
        return args;
    }
}

class AggroGlow final : public IPlugin
{
    IAshitaCore* m_AshitaCore = nullptr;
    aggroglow::Settings m_Settings;
    aggroglow::Tracker m_Tracker;
    aggroglow::OutlineRenderer m_Outline;
    aggroglow::LabelRenderer m_Labels;
    aggroglow::Menu m_Menu;
    aggroglow::PlayerState m_Player;
    aggroglow::Examiner m_Examiner;
    bool m_DebugPending = false;
    bool m_LabelsUpdated    = false; // placed this frame at the back-buffer EndScene
    uint32_t m_EndSceneUpdates = 0;  // for /ag debug
    int m_CaptureLeft   = 0; // frames /ag debug still records
    std::string m_CapturePath;
    std::string m_Capture;
    double m_CaptureLast = 0.0;
    std::vector<aggroglow::ActorInput> m_Inputs;
    LARGE_INTEGER m_QpcFrequency{};
    LARGE_INTEGER m_LastPresent{};
    double m_FrameMs = 0.0;

public:
    const char* GetName(void) const override { return kName; }
    const char* GetAuthor(void) const override { return "tanyrus"; }
    const char* GetDescription(void) const override { return "Outlines nearby monsters, colored by whether they will attack you."; }
    const char* GetLink(void) const override { return ""; }
    double GetVersion(void) const override { return 2.00; }
    // Before the Addons plugin (priority 0), so a hidden check reply is already blocked when addons see it.
    int32_t GetPriority(void) const override { return -10; }
    uint32_t GetFlags(void) const override
    {
        return static_cast<uint32_t>(Ashita::PluginFlags::UseCommands | Ashita::PluginFlags::UsePackets |
                                     Ashita::PluginFlags::UseDirect3D);
    }

    bool Initialize(IAshitaCore* core, ILogManager* logger, const uint32_t id) override
    {
        UNREFERENCED_PARAMETER(logger);
        UNREFERENCED_PARAMETER(id);
        m_AshitaCore = core;
        QueryPerformanceFrequency(&m_QpcFrequency);
        LoadSettings();
        m_Labels.Initialize(core->GetFontManager());
        return true;
    }

    void Release(void) override
    {
        m_Labels.Release();
    }

    bool Direct3DInitialize(IDirect3DDevice8* device) override
    {
        m_Outline.SetDevice(device);
        return true;
    }

    bool HandleCommand(int32_t mode, const char* command, bool injected) override
    {
        UNREFERENCED_PARAMETER(mode);
        UNREFERENCED_PARAMETER(injected);
        if (command == nullptr) return false;
        const std::vector<std::string> args = SplitLower(command);
        if (args.empty() || (args[0] != "/aggroglow" && args[0] != "/ag")) return false;

        if (args.size() == 1)
            m_Menu.open = !m_Menu.open;
        else if (args[1] == "on" || args[1] == "off")
        {
            m_Settings.enabled = args[1] == "on";
            SaveSettings();
            Print(std::string("outlines ") + (m_Settings.enabled ? "on" : "off"));
        }
        else if (args[1] == "debug")
            m_DebugPending = true;
        else
        {
            Print("/aggroglow or /ag: open or close the settings window");
            Print("/ag on | /ag off: turn outlines on or off");
            Print("/ag debug: write what every outlined mob shows to logs/aggroglow");
        }
        return true;
    }

    // /check replies, manual or automatic, become the exact label; the reply to an automatic check is hidden.
    bool HandleIncomingPacket(uint16_t id, uint32_t size, const uint8_t* data, uint8_t* modified, uint32_t sizeChunk,
        const uint8_t* dataChunk, bool injected, bool blocked) override
    {
        UNREFERENCED_PARAMETER(modified);
        UNREFERENCED_PARAMETER(sizeChunk);
        UNREFERENCED_PARAMETER(dataChunk);
        UNREFERENCED_PARAMETER(injected);
        UNREFERENCED_PARAMETER(blocked);
        if (id != 0x029) return false;
        const auto reply = aggroglow::ParseCheckReply(data, size);
        if (!reply) return false;
        IEntity* entity  = m_AshitaCore->GetMemoryManager()->GetEntity();
        const char* name = reply->targetIndex < entity->GetEntityMapSize() ? entity->GetName(reply->targetIndex) : nullptr;
        const aggroglow::MobRecord* mob = aggroglow::FindMob(reply->serverId, name != nullptr ? name : "");
        return m_Examiner.Received(*reply, aggroglow::ExamineCooldown(mob), Now());
    }

    void Direct3DPresent(const RECT* source, const RECT* dest, HWND window, const RGNDATA* dirty) override
    {
        UNREFERENCED_PARAMETER(source);
        UNREFERENCED_PARAMETER(dest);
        UNREFERENCED_PARAMETER(window);
        UNREFERENCED_PARAMETER(dirty);
        MeasureFrame();
        m_Outline.NewFrame();
        if (m_Outline.TakeStencilWarning())
            Print("a mob could not be outlined: the game's depth buffer has no stencil bits.");
        // The nameplates collected during the frame that just ended belong to this tracker state. Normally the labels were
        // already placed at the back-buffer EndScene, right after the nameplates were drawn.
        if (!m_LabelsUpdated) m_Labels.Update(m_Tracker, m_Outline, LabelsOn());
        m_LabelsUpdated = false;
        if (m_Labels.TakeFailure())
            Print("labels are off: Ashita could not create a font object.");
        if (m_DebugPending)
        {
            m_DebugPending = false;
            WriteDebugReport();
        }
        CaptureFrame();
        const double now = Now();
        UpdateTracker(now);
        AutoExamine(now);
        const aggroglow::MenuStatus status{m_Tracker.OutlinedCount(), m_Outline.MeshesLastFrame(),
            static_cast<uint32_t>(m_Labels.LastShown().size()), m_FrameMs, m_Outline.StencilAvailable()};
        if (m_Menu.Draw(m_AshitaCore->GetGuiManager(), m_Settings, status))
            SaveSettings();
    }

    // The game draws nameplates into its scene image, copies it to the back buffer and ends that scene before Present.
    // Placing the font labels here, in the same frame, keeps them from trailing the nameplates by a frame.
    void Direct3DEndScene(bool isRenderingBackBuffer) override
    {
        if (!isRenderingBackBuffer || !m_Outline.TextPending()) return;
        m_Outline.FinishText();
        m_Labels.Update(m_Tracker, m_Outline, LabelsOn());
        m_LabelsUpdated = true;
        ++m_EndSceneUpdates;
    }

    bool Direct3DDrawIndexedPrimitive(D3DPRIMITIVETYPE type, UINT minIndex, UINT numVertices, UINT startIndex, UINT primCount) override
    {
        return m_Outline.OnDrawIndexed(type, minIndex, numVertices, startIndex, primCount, m_Tracker, m_Settings);
    }

    bool Direct3DDrawPrimitiveUP(D3DPRIMITIVETYPE type, UINT primCount, const void* vertices, UINT stride) override
    {
        m_Outline.OnDrawUP(type, primCount, vertices, stride, m_Tracker, LabelsOn());
        return false;
    }

private:
    bool LabelsOn() const
    {
        return m_Settings.enabled && m_Settings.showLabels;
    }

    void Print(const std::string& message)
    {
        const std::string line = Ashita::Chat::Header(kName) + Ashita::Chat::Message(message);
        m_AshitaCore->GetChatManager()->Write(1, false, line.c_str());
    }

    void LoadSettings()
    {
        IConfigurationManager* config = m_AshitaCore->GetConfigurationManager();
        config->Load(kConfigAlias, kConfigFile); // a missing file leaves every value at its default
        AshitaStore store(config);
        m_Settings = aggroglow::LoadSettings(store);
    }

    void SaveSettings()
    {
        const std::string folder = std::string(m_AshitaCore->GetInstallPath()) + "config\\aggroglow";
        CreateDirectoryA(folder.c_str(), nullptr);
        IConfigurationManager* config = m_AshitaCore->GetConfigurationManager();
        AshitaStore store(config);
        aggroglow::SaveSettings(m_Settings, store);
        if (!config->Save(kConfigAlias, kConfigFile))
            Print("could not save config/aggroglow/settings.ini");
    }

    double Now() const
    {
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        return m_QpcFrequency.QuadPart != 0 ? static_cast<double>(counter.QuadPart) / static_cast<double>(m_QpcFrequency.QuadPart) : 0.0;
    }

    void MeasureFrame()
    {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        if (m_LastPresent.QuadPart != 0 && m_QpcFrequency.QuadPart != 0)
        {
            const double ms = static_cast<double>(now.QuadPart - m_LastPresent.QuadPart) * 1000.0 / static_cast<double>(m_QpcFrequency.QuadPart);
            m_FrameMs       = m_FrameMs == 0.0 ? ms : m_FrameMs * 0.95 + ms * 0.05;
        }
        m_LastPresent = now;
    }

    // Snapshot every entity with an actor, then let the tracker decide which mobs get outlined next frame.
    void UpdateTracker(double now)
    {
        IEntity* entity = m_AshitaCore->GetMemoryManager()->GetEntity();
        IParty* party   = m_AshitaCore->GetMemoryManager()->GetParty();
        IPlayer* player = m_AshitaCore->GetMemoryManager()->GetPlayer();
        m_Inputs.clear();
        const uint32_t count = std::min<uint32_t>(entity->GetEntityMapSize(), 4096);
        for (uint32_t i = 0; i < count; ++i)
        {
            if (entity->GetRawEntity(i) == nullptr) continue;
            const uintptr_t actor = entity->GetActorPointer(i);
            if (actor == 0) continue;
            const char* name        = entity->GetName(i);
            const uint32_t serverId = entity->GetServerId(i);
            const bool isMob        = (entity->GetSpawnFlags(i) & 0x10) != 0;
            const bool alive        = entity->GetHPPercent(i) > 0;
            if (isMob && !alive) m_Examiner.Forget(serverId); // the next spawn rolls a new level
            m_Inputs.push_back(aggroglow::ActorInput{static_cast<aggroglow::ActorPtr>(actor), static_cast<uint16_t>(i),
                serverId, isMob, alive, std::sqrt(std::max(0.0f, entity->GetDistance(i))), name != nullptr ? name : "",
                isMob ? m_Examiner.Result(serverId, now) : nullptr});
        }
        m_Player.level   = player->GetMainJobLevel();
        m_Player.sitting = aggroglow::IsSittingStatus(entity->GetStatus(party->GetMemberTargetIndex(0)));
        m_Tracker.Update(m_Inputs, m_Player, m_Settings);
    }

    // Sends a /check for the main target when the spec's conditions hold (section 4). The main target is slot 1
    // while the sub-target cursor is up.
    void AutoExamine(double now)
    {
        if (!m_Settings.autoExamine) return;
        IEntity* entity      = m_AshitaCore->GetMemoryManager()->GetEntity();
        ITarget* target      = m_AshitaCore->GetMemoryManager()->GetTarget();
        IParty* party        = m_AshitaCore->GetMemoryManager()->GetParty();
        const uint32_t index = target->GetTargetIndex(target->GetIsSubTargetActive() != 0 ? 1 : 0);
        if (index == 0 || index >= entity->GetEntityMapSize() || entity->GetRawEntity(index) == nullptr) return;
        if ((entity->GetSpawnFlags(index) & 0x10) == 0) return; // mobs only
        const uint32_t serverId         = entity->GetServerId(index);
        const char* name                = entity->GetName(index);
        const aggroglow::MobRecord* mob = aggroglow::FindMob(serverId, name != nullptr ? name : "");
        const bool inEvent              = entity->GetStatus(party->GetMemberTargetIndex(0)) == 4;
        const aggroglow::ExamineTarget candidate{mob, entity->GetHPPercent(index) > 0,
            aggroglow::DistanceFromSquared(entity->GetDistance(index)), m_Player.level, inEvent};
        if (!aggroglow::IsExamineEligible(candidate) || !m_Examiner.CanSend(serverId, now)) return;
        auto packet = aggroglow::BuildCheckRequest(serverId, static_cast<uint16_t>(index));
        m_AshitaCore->GetPacketManager()->AddOutgoingPacket(0x0DD, static_cast<uint32_t>(packet.size()), packet.data());
        m_Examiner.Sent(serverId, aggroglow::ExamineCooldown(mob), now);
    }

    // /ag debug: what each outlined mob showed in the last frame, written to logs/aggroglow/ for checking labels.
    void WriteDebugReport()
    {
        const std::string logs = std::string(m_AshitaCore->GetInstallPath()) + "logs";
        CreateDirectoryA(logs.c_str(), nullptr);
        CreateDirectoryA((logs + "\\aggroglow").c_str(), nullptr);
        char file[48];
        const std::time_t now = std::time(nullptr);
        std::strftime(file, sizeof(file), "\\aggroglow\\debug-%Y%m%d-%H%M%S.txt", std::localtime(&now));
        const std::string path = logs + file;
        std::FILE* out = std::fopen(path.c_str(), "w");
        if (out == nullptr)
        {
            Print("could not write " + path);
            return;
        }

        IEntity* entity = m_AshitaCore->GetMemoryManager()->GetEntity();
        std::fprintf(out, "aggroglow %.2f: back buffer %.0fx%.0f, player level %d%s, %u outlined, %u labels\n",
            GetVersion(), m_Outline.BackBufferWidth(), m_Outline.BackBufferHeight(), m_Player.level,
            m_Player.sitting ? " (sitting)" : "", m_Tracker.OutlinedCount(), static_cast<unsigned>(m_Labels.LastShown().size()));
        for (const aggroglow::ActorPtr actor : m_Tracker.OutlinedActors())
        {
            const aggroglow::ActorInfo* info = m_Tracker.Find(actor);
            if (info == nullptr) continue;
            const char* name                = entity->GetName(info->index);
            const uint32_t serverId         = entity->GetServerId(info->index);
            const aggroglow::MobRecord* mob = aggroglow::FindMob(serverId, name != nullptr ? name : "");
            std::fprintf(out, "#%u %u '%s' ", info->index, serverId, name != nullptr ? name : "");
            if (mob != nullptr)
                std::fprintf(out, "data Lv %u-%u flags %u respawn %u", mob->minLevel, mob->maxLevel, mob->flags, mob->respawn);
            else
                std::fprintf(out, "no data");
            std::fprintf(out, " | outline %08X label '%s' %08X", static_cast<unsigned>(info->argb), info->label.text,
                static_cast<unsigned>(info->label.argb));
            if (const aggroglow::CheckResult* checked = m_Examiner.Result(serverId, Now()))
                std::fprintf(out, " | examined Lv %d %s", checked->level, aggroglow::Abbrev(checked->con));
            if (const aggroglow::ScreenBox* plate = m_Outline.NameplateBox(info->index))
                std::fprintf(out, " | plate (%.1f,%.1f)-(%.1f,%.1f)", plate->minX, plate->minY, plate->maxX, plate->maxY);
            else
                std::fprintf(out, " | no plate");
            for (const auto& shown : m_Labels.LastShown())
                if (shown.index == info->index)
                    std::fprintf(out, " | label at (%.1f,%.1f) %.0fx%.0f", shown.x, shown.y, shown.width, shown.height);
            std::fprintf(out, "\n");
        }
        std::fclose(out);
        Print("wrote " + path + "; recording 120 frames");
        m_CapturePath = path;
        m_Capture     = "\nlabel updates at EndScene so far: " + std::to_string(m_EndSceneUpdates) +
                    "\nframe  dt(ms)  text draws: in-scene owned no-owner other | labels shown, SetText calls | per outlined mob:"
                    " index:P<glyph draws> r<frames in row> m<body meshes> used(nameplate box) raw(all attributed glyphs), or index:- m<meshes>\n";
        m_CaptureLeft = 120;
        m_CaptureLast = Now();
    }

    // One line per frame after /ag debug: where nameplate draws went and what the labels did.
    void CaptureFrame()
    {
        if (m_CaptureLeft <= 0) return;
        const double now = Now();
        const auto& stats = m_Outline.TextStatsLastFrame();
        char line[200];
        std::snprintf(line, sizeof(line), "f%03d %6.1f  %3u %3u %3u %3u | %2u %2u |", 120 - m_CaptureLeft,
            (now - m_CaptureLast) * 1000.0, stats.inScene, stats.owned, stats.noOwner, stats.otherOwner,
            static_cast<unsigned>(m_Labels.LastShown().size()), m_Labels.TextChangesLastUpdate());
        m_Capture += line;
        int listed = 0;
        for (const aggroglow::ActorPtr actor : m_Tracker.OutlinedActors())
        {
            const aggroglow::ActorInfo* info = m_Tracker.Find(actor);
            if (info == nullptr || ++listed > 16) continue;
            const aggroglow::ScreenBox* plate = m_Outline.NameplateBox(info->index);
            if (plate != nullptr)
            {
                const aggroglow::ScreenBox* raw = m_Outline.RawNameplateBox(info->index);
                std::snprintf(line, sizeof(line), " %u:P%u r%u m%u used(%.0f-%.0f,%.0f-%.0f) raw(%.0f-%.0f,%.0f-%.0f)", info->index,
                    m_Outline.PlateDraws(info->index), m_Outline.PlateFramesInRow(info->index), m_Outline.MeshDraws(info->index),
                    plate->minX, plate->maxX, plate->minY,
                    plate->maxY, raw ? raw->minX : 0.0f, raw ? raw->maxX : 0.0f, raw ? raw->minY : 0.0f, raw ? raw->maxY : 0.0f);
            }
            else
                std::snprintf(line, sizeof(line), " %u:-m%u", info->index, m_Outline.MeshDraws(info->index));
            m_Capture += line;
        }
        m_Capture += "\n";
        m_CaptureLast = now;
        if (--m_CaptureLeft == 0)
        {
            if (std::FILE* out = std::fopen(m_CapturePath.c_str(), "a"))
            {
                std::fputs(m_Capture.c_str(), out);
                std::fclose(out);
                Print("frame capture added to " + m_CapturePath);
            }
            m_Capture.clear();
        }
    }
};

extern "C" __declspec(dllexport) double __stdcall expGetInterfaceVersion(void)
{
    return ASHITA_INTERFACE_VERSION;
}

extern "C" __declspec(dllexport) IPlugin* __stdcall expCreatePlugin(const char* args)
{
    UNREFERENCED_PARAMETER(args);
    return new AggroGlow();
}

extern "C" __declspec(dllexport) void __stdcall expDestroyPlugin(void* instance)
{
    delete static_cast<IPlugin*>(instance);
}
