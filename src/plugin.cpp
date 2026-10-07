#include "Ashita.h"

#include "examine.h"
#include "nameplate_render.h"
#include "menu.h"
#include "mobdata.h"
#include "outline.h"
#include "settings.h"
#include "tracker.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

namespace
{
    constexpr const char* kName          = "headsup";
    constexpr const char* kConfigAlias   = "headsup";
    constexpr const char* kConfigFile    = "headsup/settings.ini";   // relative to Ashita's config folder
    constexpr const char* kOldConfigFile = "aggroglow\\settings.ini"; // the plugin's settings under its old name
    constexpr const char* kSection       = "settings";
    constexpr uint32_t kSpawnFlagMob   = 0x10;  // IEntity::GetSpawnFlags
    constexpr uint32_t kMaxEntities    = 4096;  // more than the client's entity map holds
    constexpr int kCaptureFrames       = 120;   // frames /hu debug records
    constexpr int kCaptureMobs         = 16;    // nameplates listed per captured frame
    constexpr double kFrameTimeWeight  = 0.05;  // smoothing of the menu's frame time

    // The entity's name, or "" for an empty or out-of-range slot.
    const char* EntityName(IEntity* entity, uint32_t index)
    {
        if (index >= entity->GetEntityMapSize() || entity->GetRawEntity(index) == nullptr) return "";
        const char* name = entity->GetName(index);
        return name != nullptr ? name : "";
    }

    // Settings persistence through Ashita's configuration manager.
    class AshitaStore final : public headsup::SettingsStore
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

class HeadsUp final : public IPlugin
{
    IAshitaCore* m_AshitaCore = nullptr;
    headsup::Settings m_Settings;
    headsup::Tracker m_Tracker;
    headsup::OutlineRenderer m_Outline;
    headsup::NameplateRenderer m_Nameplates;
    headsup::Menu m_Menu;
    headsup::PlayerState m_Player;
    headsup::CheckResults m_Checks;
    bool m_DebugPending = false;
    bool m_PlatesPlaced = false; // placed this frame, in the scene or at the back-buffer EndScene
    bool m_Drawing      = false; // drawing nameplates: our own draws come back through the hooks
    bool m_DrewInScene  = false; // this frame, for /hu debug
    IDirect3DDevice8* m_Device = nullptr;
    int m_CaptureLeft   = 0; // frames /hu debug still records
    std::string m_CapturePath;
    std::string m_Capture;
    double m_CaptureLast = 0.0;
    std::vector<headsup::ActorInput> m_Inputs;
    LARGE_INTEGER m_QpcFrequency{};
    LARGE_INTEGER m_LastPresent{};
    double m_FrameMs = 0.0;

public:
    const char* GetName(void) const override { return kName; }
    const char* GetAuthor(void) const override { return "tanyrus"; }
    const char* GetDescription(void) const override { return "Outlines monsters by whether they will attack you and labels their names."; }
    const char* GetLink(void) const override { return ""; }
    double GetVersion(void) const override { return 2.10; }
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
        return true;
    }

    void Release(void) override
    {
        m_Nameplates.Release();
    }

    bool Direct3DInitialize(IDirect3DDevice8* device) override
    {
        m_Device = device;
        m_Outline.SetDevice(device);
        m_Nameplates.SetDevice(device);
        return true;
    }

    bool HandleCommand(int32_t mode, const char* command, bool injected) override
    {
        UNREFERENCED_PARAMETER(mode);
        UNREFERENCED_PARAMETER(injected);
        if (command == nullptr) return false;
        const std::vector<std::string> args = SplitLower(command);
        if (args.empty() || (args[0] != "/headsup" && args[0] != "/hu")) return false;

        if (args.size() == 1)
            m_Menu.open = !m_Menu.open;
        else if (args[1] == "on" || args[1] == "off")
        {
            m_Settings.enabled = args[1] == "on";
            SaveSettings();
            Print(std::string("outlines and nameplates ") + (m_Settings.enabled ? "on" : "off"));
        }
        else if (args[1] == "debug")
            m_DebugPending = true;
        else
        {
            Print("/headsup or /hu: open or close the settings window");
            Print("/hu on | /hu off: turn outlines and nameplates on or off");
            Print("/hu debug: write what every mob's outline and nameplate show to logs/headsup");
        }
        return true;
    }

    // The reply to the player's own /check: the mob's exact level, for its label until it respawns. Chat shows it.
    bool HandleIncomingPacket(uint16_t id, uint32_t size, const uint8_t* data, uint8_t* modified, uint32_t sizeChunk,
        const uint8_t* dataChunk, bool injected, bool blocked) override
    {
        UNREFERENCED_PARAMETER(modified);
        UNREFERENCED_PARAMETER(sizeChunk);
        UNREFERENCED_PARAMETER(dataChunk);
        UNREFERENCED_PARAMETER(injected);
        UNREFERENCED_PARAMETER(blocked);
        if (id != headsup::kCheckReplyPacket) return false;
        const auto reply = headsup::ParseCheckReply(data, size);
        if (!reply) return false;
        IEntity* entity = m_AshitaCore->GetMemoryManager()->GetEntity();
        const headsup::MobRecord* mob = headsup::FindMob(reply->serverId, EntityName(entity, reply->targetIndex));
        m_Checks.Received(*reply, headsup::CheckLifetime(mob), Now());
        return false;
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
        // A frame whose nameplates were not drawn must not hide the game's names in the next.
        if (!m_PlatesPlaced)
        {
            m_Nameplates.Clear();
            m_Outline.SetReplacedNames(m_Nameplates.ReplacingNames());
        }
        m_PlatesPlaced = false;
        if (m_Nameplates.TakeFailure())
            Print("names and labels are off: a text texture could not be made.");
        if (m_Nameplates.TakeIconFailure())
            Print("icons are off: an icon texture could not be made.");
        if (m_DebugPending)
        {
            m_DebugPending = false;
            WriteDebugReport();
        }
        CaptureFrame();
        const bool drewInScene = m_DrewInScene;
        m_DrewInScene          = false;
        const double now       = Now();
        UpdateTracker(now);
        const auto& letters = m_Outline.TextStatsLastFrame();
        const headsup::MenuStatus status{m_Tracker.OutlinedCount(), m_Outline.MeshesLastFrame(),
            static_cast<uint32_t>(m_Nameplates.LastShown().size()), m_FrameMs, m_Outline.StencilAvailable(), drewInScene,
            letters.inScene, letters.owned, letters.hidden, m_Player.level, m_Player.sitting};
        if (m_Menu.Draw(m_AshitaCore->GetGuiManager(), m_Settings, status))
            SaveSettings();
        if (m_Menu.TakeDebugRequest()) m_DebugPending = true;
    }

    // The game draws nameplates into its scene image, copies it to the back buffer and ends that scene before Present.
    // Unless they were drawn into the scene, our nameplates go on top here, in the same frame as the game's names.
    void Direct3DEndScene(bool isRenderingBackBuffer) override
    {
        if (m_Drawing || !isRenderingBackBuffer || m_PlatesPlaced || !m_Outline.TextPending()) return;
        m_Outline.FinishText();
        WithSavedState([&] { DrawNameplates(1.0f, 1.0f, false); });
    }

    bool Direct3DDrawPrimitive(D3DPRIMITIVETYPE type, UINT startVertex, UINT primCount) override
    {
        UNREFERENCED_PARAMETER(type);
        UNREFERENCED_PARAMETER(startVertex);
        UNREFERENCED_PARAMETER(primCount);
        if (!m_Drawing) DrawNameplatesBeforeSceneCopy();
        return false;
    }

    bool Direct3DDrawIndexedPrimitive(D3DPRIMITIVETYPE type, UINT minIndex, UINT numVertices, UINT startIndex, UINT primCount) override
    {
        if (m_Drawing) return false;
        DrawNameplatesBeforeSceneCopy();
        return m_Outline.OnDrawIndexed(type, minIndex, numVertices, startIndex, primCount, m_Tracker, m_Settings);
    }

    bool Direct3DDrawPrimitiveUP(D3DPRIMITIVETYPE type, UINT primCount, const void* vertices, UINT stride) override
    {
        if (m_Drawing) return false;
        DrawNameplatesBeforeSceneCopy();
        // In replace mode the game's own mob nameplate letters are measured, then blocked.
        return m_Outline.OnDrawUP(type, primCount, vertices, stride, m_Tracker, headsup::NameplatesOn(m_Settings),
            m_Settings.enabled && m_Settings.replaceNameplates);
    }

    bool Direct3DDrawIndexedPrimitiveUP(D3DPRIMITIVETYPE type, UINT minIndex, UINT numVertices, UINT primCount,
        const void* indices, D3DFORMAT indexFormat, const void* vertices, UINT stride) override
    {
        UNREFERENCED_PARAMETER(type);
        UNREFERENCED_PARAMETER(minIndex);
        UNREFERENCED_PARAMETER(numVertices);
        UNREFERENCED_PARAMETER(primCount);
        UNREFERENCED_PARAMETER(indices);
        UNREFERENCED_PARAMETER(indexFormat);
        UNREFERENCED_PARAMETER(vertices);
        UNREFERENCED_PARAMETER(stride);
        if (!m_Drawing) DrawNameplatesBeforeSceneCopy();
        return false;
    }

private:
    // Saves the game's render states, render target and depth surface around draw, which may change any of them.
    template <typename Draw>
    bool WithSavedState(Draw draw)
    {
        DWORD saved = 0;
        if (m_Device == nullptr || FAILED(m_Device->CreateStateBlock(D3DSBT_ALL, &saved))) return false;
        IDirect3DSurface8* target = nullptr;
        IDirect3DSurface8* depth  = nullptr;
        m_Device->GetRenderTarget(&target);
        m_Device->GetDepthStencilSurface(&depth);
        m_Drawing = true;
        draw();
        m_Drawing = false;
        m_Device->SetRenderTarget(target, depth);
        m_Device->ApplyStateBlock(saved); // after SetRenderTarget, which resets the viewport
        m_Device->DeleteStateBlock(saved);
        if (target != nullptr) target->Release();
        if (depth != nullptr) depth->Release();
        return true;
    }

    // Lays out and draws the nameplates into the bound render target, toX and toY pixels per back-buffer pixel.
    void DrawNameplates(float toX, float toY, bool depthTest)
    {
        m_Nameplates.Update(m_Tracker, m_Outline, m_Settings, toX, toY);
        m_Outline.SetReplacedNames(m_Nameplates.ReplacingNames());
        m_Nameplates.Draw(depthTest);
        m_PlatesPlaced = true;
    }

    // The game draws its 3D scene, names included, into an off-screen image and then copies that to the back buffer.
    // Drawn into the image just before the copy, at the name's depth, our nameplates are hidden by walls like the
    // game's names, and sit under the game's menus. The copy is the first draw to the back buffer after the names.
    void DrawNameplatesBeforeSceneCopy()
    {
        if (!m_Settings.enabled || !m_Settings.hideBehindWalls || !headsup::NameplatesOn(m_Settings) ||
            m_Outline.TargetScaleX() <= 0.0f || m_Outline.TargetScaleY() <= 0.0f || !m_Outline.SceneCopyStarting())
            return;
        m_Outline.FinishText();
        WithSavedState([&] {
            if (FAILED(m_Device->SetRenderTarget(m_Outline.SceneTarget(), m_Outline.SceneDepth()))) return;
            DrawNameplates(1.0f / m_Outline.TargetScaleX(), 1.0f / m_Outline.TargetScaleY(), true);
            m_DrewInScene = true;
        });
    }

    void Print(const std::string& message)
    {
        const std::string line = Ashita::Chat::Header(kName) + Ashita::Chat::Message(message);
        m_AshitaCore->GetChatManager()->Write(1, false, line.c_str());
    }

    void LoadSettings()
    {
        // The plugin was called AggroGlow: its settings carry over once, and the old file is left as it was.
        const std::string configs = std::string(m_AshitaCore->GetInstallPath()) + "config\\";
        const std::string current = configs + "headsup\\settings.ini";
        if (GetFileAttributesA(current.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            CreateDirectoryA((configs + "headsup").c_str(), nullptr);
            CopyFileA((configs + kOldConfigFile).c_str(), current.c_str(), TRUE);
        }
        IConfigurationManager* config = m_AshitaCore->GetConfigurationManager();
        config->Load(kConfigAlias, kConfigFile); // a missing file leaves every value at its default
        AshitaStore store(config);
        m_Settings = headsup::LoadSettings(store);
    }

    void SaveSettings()
    {
        const std::string folder = std::string(m_AshitaCore->GetInstallPath()) + "config\\headsup";
        CreateDirectoryA(folder.c_str(), nullptr);
        IConfigurationManager* config = m_AshitaCore->GetConfigurationManager();
        AshitaStore store(config);
        headsup::SaveSettings(m_Settings, store);
        if (!config->Save(kConfigAlias, kConfigFile))
            Print("could not save config/headsup/settings.ini");
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
            m_FrameMs       = m_FrameMs == 0.0 ? ms : m_FrameMs + (ms - m_FrameMs) * kFrameTimeWeight;
        }
        m_LastPresent = now;
    }

    void UpdateTracker(double now)
    {
        IEntity* entity = m_AshitaCore->GetMemoryManager()->GetEntity();
        IParty* party   = m_AshitaCore->GetMemoryManager()->GetParty();
        IPlayer* player = m_AshitaCore->GetMemoryManager()->GetPlayer();
        m_Inputs.clear();
        const uint32_t count = std::min<uint32_t>(entity->GetEntityMapSize(), kMaxEntities);
        for (uint32_t i = 0; i < count; ++i)
        {
            if (entity->GetRawEntity(i) == nullptr) continue;
            const uintptr_t actor = entity->GetActorPointer(i);
            if (actor == 0) continue;
            const uint32_t serverId = entity->GetServerId(i);
            const bool isMob        = (entity->GetSpawnFlags(i) & kSpawnFlagMob) != 0;
            const bool alive        = entity->GetHPPercent(i) > 0;
            if (isMob && !alive) m_Checks.Forget(serverId); // the next spawn rolls a new level
            m_Inputs.push_back(headsup::ActorInput{static_cast<headsup::ActorPtr>(actor), static_cast<uint16_t>(i),
                serverId, isMob, alive, headsup::DistanceFromSquared(entity->GetDistance(i)), EntityName(entity, i),
                isMob ? m_Checks.Result(serverId, now) : nullptr});
        }
        m_Player.level   = player->GetMainJobLevel();
        m_Player.sitting = headsup::IsSittingStatus(entity->GetStatus(party->GetMemberTargetIndex(0)));
        m_Tracker.Update(m_Inputs, m_Player, m_Settings);
    }

    // /hu debug: what each mob's outline and nameplate showed in the last frame, written to logs/headsup/.
    void WriteDebugReport()
    {
        const std::string logs = std::string(m_AshitaCore->GetInstallPath()) + "logs";
        CreateDirectoryA(logs.c_str(), nullptr);
        CreateDirectoryA((logs + "\\headsup").c_str(), nullptr);
        char file[48];
        const std::time_t now = std::time(nullptr);
        std::strftime(file, sizeof(file), "\\headsup\\debug-%Y%m%d-%H%M%S.txt", std::localtime(&now));
        const std::string path = logs + file;
        std::FILE* out = std::fopen(path.c_str(), "w");
        if (out == nullptr)
        {
            Print("could not write " + path);
            return;
        }

        IEntity* entity = m_AshitaCore->GetMemoryManager()->GetEntity();
        std::fprintf(out, "headsup %.2f: back buffer %.0fx%.0f, player level %d%s, %u outlined, %u nameplates, replace %s\n",
            GetVersion(), m_Outline.BackBufferWidth(), m_Outline.BackBufferHeight(), m_Player.level,
            m_Player.sitting ? " (sitting)" : "", m_Tracker.OutlinedCount(),
            static_cast<unsigned>(m_Nameplates.LastShown().size()), m_Settings.replaceNameplates ? "on" : "off");
        for (const headsup::ActorPtr actor : m_Tracker.Mobs())
        {
            const headsup::ActorInfo* info = m_Tracker.Find(actor);
            if (info == nullptr || (!info->outline && m_Outline.NameplateBox(info->index) == nullptr)) continue;
            const char* name                = EntityName(entity, info->index);
            const uint32_t serverId         = entity->GetServerId(info->index);
            const headsup::MobRecord* mob = headsup::FindMob(serverId, name);
            std::fprintf(out, "#%u %u '%s' ", info->index, serverId, name);
            if (mob != nullptr)
                std::fprintf(out, "data Lv %u-%u flags %u detects %u respawn %u", mob->minLevel, mob->maxLevel, mob->flags,
                    mob->detects, mob->respawn);
            else
                std::fprintf(out, "no data");
            std::fprintf(out, " | %s outline %08X label '%s' %08X icons %d", info->alive ? "alive" : "dead",
                static_cast<unsigned>(info->argb), info->label.text, static_cast<unsigned>(headsup::ToArgb(m_Settings.labelColor[static_cast<int>(info->label.shade)])), info->icons.count);
            if (const headsup::CheckResult* checked = m_Checks.Result(serverId, Now()))
                std::fprintf(out, " | checked Lv %d %s", checked->level, headsup::Abbrev(checked->con));
            if (const headsup::ScreenBox* plate = m_Outline.NameplateBox(info->index))
                std::fprintf(out, " | plate (%.1f,%.1f)-(%.1f,%.1f)", plate->minX, plate->minY, plate->maxX, plate->maxY);
            else
                std::fprintf(out, " | no plate");
            for (const auto& shown : m_Nameplates.LastShown())
                if (shown.index == info->index)
                    std::fprintf(out, " | name %08X %dpx at (%.1f,%.1f), label %dpx at (%.1f,%.1f), %d icons %dpx at (%.1f,%.1f)",
                        static_cast<unsigned>(shown.nameColor), shown.nameHeight, shown.nameX, shown.nameY, shown.labelHeight,
                        shown.labelX, shown.labelY, shown.icons, shown.iconSize, shown.iconsX, shown.iconsY);
            std::fprintf(out, "\n");
        }
        std::fclose(out);
        Print("wrote " + path + "; recording " + std::to_string(kCaptureFrames) + " frames");
        m_CapturePath = path;
        m_Capture     = "\nframe  dt(ms)  name letters: in-scene owned no-owner other hidden | nameplates shown, S if drawn into the scene, s<scene to screen scale> | per mob "
                    "with a nameplate: index r<frames in row> m<body meshes> g<glyphs> c<name color> (nameplate box)\n";
        m_CaptureLeft = kCaptureFrames;
        m_CaptureLast = Now();
    }

    // One line per frame after /hu debug: where nameplate draws went and what the labels did.
    void CaptureFrame()
    {
        if (m_CaptureLeft <= 0) return;
        const double now = Now();
        const auto& stats = m_Outline.TextStatsLastFrame();
        char line[200];
        std::snprintf(line, sizeof(line), "f%03d %6.1f  %3u %3u %3u %3u %3u | %2u %c s%.3f |", kCaptureFrames - m_CaptureLeft,
            (now - m_CaptureLast) * 1000.0, stats.inScene, stats.owned, stats.noOwner, stats.otherOwner, stats.hidden,
            static_cast<unsigned>(m_Nameplates.LastShown().size()), m_DrewInScene ? 'S' : '-', m_Outline.TargetScaleY());
        m_Capture += line;
        int listed = 0;
        for (const headsup::ActorPtr actor : m_Tracker.Mobs())
        {
            const headsup::ActorInfo* info = m_Tracker.Find(actor);
            const headsup::ScreenBox* plate = info != nullptr ? m_Outline.NameplateBox(info->index) : nullptr;
            if (plate == nullptr || ++listed > kCaptureMobs) continue;
            std::snprintf(line, sizeof(line), " %u r%u m%u g%u c%06X (%.0f-%.0f,%.0f-%.0f)", info->index,
                m_Outline.PlateFramesInRow(info->index), m_Outline.MeshDraws(info->index), m_Outline.GlyphsLastFrame(info->index),
                static_cast<unsigned>(m_Outline.NameplateColor(info->index) & 0xFFFFFF), plate->minX, plate->maxX, plate->minY,
                plate->maxY);
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
    return new HeadsUp();
}

extern "C" __declspec(dllexport) void __stdcall expDestroyPlugin(void* instance)
{
    delete static_cast<IPlugin*>(instance);
}
