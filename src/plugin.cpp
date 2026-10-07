#include "Ashita.h"

#include "check.h"
#include "commands.h"
#include "game_cursor.h"
#include "game_names.h"
#include "menu.h"
#include "mobdata.h"
#include "nameplate_render.h"
#include "outline.h"
#include "player_status.h"
#include "pointer_swap.h"
#include "settings.h"
#include "tracker.h"
#ifdef HEADSUP_DEV
#include "drawdump.h"
#endif

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
    constexpr const char* kName          = "headsup";
    constexpr const char* kConfigAlias   = "headsup";
    constexpr const char* kFolder        = "headsup"; // HeadsUp's own, in Ashita's config and logs folders
    constexpr const char* kSettingsFile  = "settings.ini";
    constexpr const char* kSection       = "settings";
    constexpr uint32_t kLockedOn         = 0x01; // ITarget::GetLockedOnFlags
    constexpr uint16_t kZoneInPacket     = 0x00A;
    constexpr int kCaptureFrames         = 120;  // frames /hu debug records
    constexpr int kCaptureMobs           = 16;   // nameplates listed per captured frame
    constexpr double kFrameTimeWeight    = 0.05; // smoothing of the menu's frame time
    constexpr int32_t kChatMode          = 1;    // the chat mode Ashita's own plugins print in
    // The game's menu resolution, which its target window's arrow anchors are in, from its registry settings.
    constexpr const char* kBootConfig    = "boot";
    constexpr const char* kRegistry      = "ffxi.registry";
    constexpr const char* kMenuWidth     = "0037";
    constexpr const char* kMenuHeight    = "0038";
    constexpr uint32_t kPartyIconMembers = 5;    // the other members of your party, whose buffs the game keeps
    constexpr uint32_t kPartyMembers     = 18;   // you, your party and the two other alliance parties

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
        std::string GetString(const char* key, const char* fallback) override
        {
            const char* value = m_Config->GetString(kConfigAlias, kSection, key);
            return value != nullptr ? value : fallback;
        }
        void Set(const char* key, const char* value) override { m_Config->SetValue(kConfigAlias, kSection, key, value); }

    private:
        IConfigurationManager* m_Config;
    };

}

class HeadsUp final : public IPlugin
{
    IAshitaCore* m_AshitaCore = nullptr;
    headsup::Settings m_Settings;
    headsup::Tracker m_Tracker;
    headsup::OutlineRenderer m_Outline;
    headsup::GameNames m_Names;
    headsup::NameplateRenderer m_Nameplates;
    headsup::PointerSwap m_Pointer;
    bool m_PointerFailed = false; // said once; tried again when the setting changes
    headsup::Menu m_Menu;
    headsup::PlayerState m_Player;
    headsup::CheckResults m_Checks;
    std::unordered_map<uint16_t, headsup::PlayerStatus> m_PlayerStatus; // other players, by target index
    std::unordered_set<uint16_t> m_LevelSynced; // players, you included, by target index, from packets
    std::unordered_set<uint32_t> m_BuffSynced;  // you and your party, from the buffs the game keeps, this frame
    std::vector<uint32_t> m_PartyIds;            // server IDs of you, your party and alliance, this frame
    std::optional<headsup::PlayerStatus> m_OwnStatus;
    bool m_DebugPending = false;
    bool m_PlatesPlaced = false; // placed this frame, in the scene or at the back-buffer EndScene

#ifdef HEADSUP_DEV
    headsup::DrawDump m_DrawDump;
#endif
    bool m_Drawing      = false; // drawing nameplates: our own draws come back through the hooks
    bool m_DrewInScene  = false; // this frame, for /hu debug
    bool m_StateWarned  = false; // said once that the game's render states could not be saved
    IDirect3DDevice8* m_Device = nullptr;
    headsup::CursorTargets m_CursorTargets;
    bool m_Picking = false; // a sub-target is being picked
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
        m_Pointer.Stop();
        m_Nameplates.Release();
    }

    bool Direct3DInitialize(IDirect3DDevice8* device) override
    {
        m_Device = device;
        m_Outline.SetDevice(device);
        m_Names.SetDevice(device);
        m_Nameplates.SetDevice(device);
        return true;
    }

    bool HandleCommand(int32_t mode, const char* command, bool injected) override
    {
        UNREFERENCED_PARAMETER(mode);
        UNREFERENCED_PARAMETER(injected);
        if (command == nullptr) return false;
        const std::vector<std::string> args = headsup::SplitLower(command);
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
#ifdef HEADSUP_DEV
        else if (args[1] == "drawdump")
            Print(m_DrawDump.Start(args.size() > 2 ? args[2] : "", Now(), m_AshitaCore, OwnFolder("logs")));
#endif
        else
        {
            Print("/headsup or /hu: open or close the settings window");
            Print("/hu on | /hu off: turn outlines and nameplates on or off");
            Print("/hu debug: write what every outline and nameplate show to logs/headsup");
#ifdef HEADSUP_DEV
            Print("/hu drawdump [seconds | watch | stop]: write every draw call of one frame to logs/headsup after the delay, or watch every frame for name glitches until stopped");
#endif
        }
        return true;
    }

    // Players' statuses and level sync, for the icons beside their names, and the reply to the player's own /check: the mob's exact
    // level, for its label until it respawns. Every packet still reaches the game.
    bool HandleIncomingPacket(uint16_t id, uint32_t size, const uint8_t* data, uint8_t* modified, uint32_t sizeChunk,
        const uint8_t* dataChunk, bool injected, bool blocked) override
    {
        UNREFERENCED_PARAMETER(modified);
        UNREFERENCED_PARAMETER(sizeChunk);
        UNREFERENCED_PARAMETER(dataChunk);
        UNREFERENCED_PARAMETER(injected);
        UNREFERENCED_PARAMETER(blocked);
#ifdef HEADSUP_DEV
        m_DrawDump.SawPacket(id, data, size, Now());
#endif
        if (id == kZoneInPacket)
        {
            m_PlayerStatus.clear(); // target indexes are reused in the next zone
            m_LevelSynced.clear();
            m_Names.ForgetCamera();
        }
        if (id == headsup::kOtherPlayerPacket)
        {
            if (const auto update = headsup::ParseOtherPlayer(data, size))
            {
                if (update->despawn)
                {
                    m_PlayerStatus.erase(update->index);
                    m_LevelSynced.erase(update->index);
                }
                else if (update->status)
                    m_PlayerStatus[update->index] = *update->status;
            }
            return false;
        }
        if (id == headsup::kCharSyncPacket)
        {
            if (const auto sync = headsup::ParseCharSync(data, size))
            {
                if (sync->synced)
                    m_LevelSynced.insert(sync->index);
                else
                    m_LevelSynced.erase(sync->index);
            }
            return false;
        }
        if (id == headsup::kOwnStatusPacket)
        {
            if (const auto own = headsup::ParseOwnStatus(data, size)) m_OwnStatus = *own;
            return false;
        }
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
#ifdef HEADSUP_DEV
        if (m_DrawDump.NextFrame(Now(), m_Names, m_Tracker, m_Nameplates)) WriteDrawDump();
#endif
        m_Outline.NewFrame();
        m_Names.NewFrame();
        if (m_Outline.TakeStencilWarning())
            Print("a mob could not be outlined: the game's depth buffer has no stencil bits.");
        // A frame whose nameplates were not drawn keeps none of the last layout.
        if (!m_PlatesPlaced) m_Nameplates.Clear();
        m_PlatesPlaced = false;
        if (!headsup::NameplatesOn(m_Settings)) m_Nameplates.ReleasePlates();
        if (const std::string failure = m_Nameplates.TakeFailure(); !failure.empty())
            Print("nameplates are off until HeadsUp reloads, and the game's names are back: " + failure + ".");
        if (const std::string failure = m_Nameplates.TakeIconFailure(); !failure.empty())
            Print("icons are off until HeadsUp reloads, and the game's are back: " + failure + ".");
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
        UpdateCursorTargets();
        const auto& letters = m_Names.TextStatsLastFrame();
        const headsup::MenuStatus status{m_Tracker.OutlinedCount(), m_Outline.MeshesLastFrame(),
            static_cast<uint32_t>(m_Nameplates.LastShown().size()), m_FrameMs, m_Outline.StencilAvailable(), drewInScene,
            letters.inScene, letters.fromMobs, letters.hidden, m_Player.level, m_Player.sitting};
        if (m_Menu.Draw(m_AshitaCore->GetGuiManager(), m_Settings, status))
            SaveSettings();
        if (m_Menu.TakeDebugRequest()) m_DebugPending = true;
        UpdatePointer();
    }

    // The game draws nameplates into its scene image, copies it to the back buffer and ends that scene before Present.
    // Unless they were drawn into the scene, our nameplates go on top here, in the same frame as the game's names.
    void Direct3DEndScene(bool isRenderingBackBuffer) override
    {
        if (Ours() || !isRenderingBackBuffer || m_PlatesPlaced || !m_Names.NamesThisFrame()) return;
        m_Names.FinishText();
        WithSavedState([&] { DrawNameplates(1.0f, 1.0f, false); });
    }

    bool Direct3DDrawPrimitive(D3DPRIMITIVETYPE type, UINT startVertex, UINT primCount) override
    {
        UNREFERENCED_PARAMETER(startVertex);
        if (Ours()) return false;
        RecordDraw('P', type, primCount, nullptr, 0u, 0u, 0u);
        DrawNameplatesBeforeSceneCopy();
        return false;
    }

    bool Direct3DDrawIndexedPrimitive(D3DPRIMITIVETYPE type, UINT minIndex, UINT numVertices, UINT startIndex, UINT primCount) override
    {
        if (Ours()) return false;
        RecordDraw('I', type, primCount, nullptr, 0u, 0u, 0u);
        DrawNameplatesBeforeSceneCopy();
        const bool outlines = m_Settings.enabled && m_Tracker.OutlinedCount() > 0;
        const bool bodies   = headsup::NameplatesOn(m_Settings) && !m_Tracker.Actors().empty(); // labels wait for a drawn body
        if (!outlines && !bodies) return false;
        const headsup::ActorInfo* owner = m_Outline.CharacterMeshOwner(m_Tracker);
        if (owner == nullptr) return false;
        m_Names.CountMesh(owner->index);
        const bool replaced =
            outlines && owner->outline && m_Outline.DrawOutlined(type, minIndex, numVertices, startIndex, primCount, *owner, m_Settings);
        if (replaced) MarkHidden();
        return replaced;
    }

    bool Direct3DDrawPrimitiveUP(D3DPRIMITIVETYPE type, UINT primCount, const void* vertices, UINT stride) override
    {
        if (Ours()) return false;
        RecordDraw('U', type, primCount, vertices, stride, 0u, headsup::VertexCount(type, primCount));
        DrawNameplatesBeforeSceneCopy();
        // The game's target cursor is not drawn where ours replaces it; the game's names are measured, and those HeadsUp
        // replaces are blocked, except what it can no longer draw itself.
        const bool blocked = BlocksGameCursor(type, primCount, vertices, stride) ||
                             m_Names.OnDrawUP(type, primCount, vertices, stride, m_Tracker, m_Settings,
                                 {!m_Nameplates.NamesFailed(), !m_Nameplates.IconsFailed()});
        if (blocked) MarkHidden();
        return blocked;
    }

    bool Direct3DDrawIndexedPrimitiveUP(D3DPRIMITIVETYPE type, UINT minIndex, UINT numVertices, UINT primCount,
        const void* indices, D3DFORMAT indexFormat, const void* vertices, UINT stride) override
    {
        UNREFERENCED_PARAMETER(indices);
        UNREFERENCED_PARAMETER(indexFormat);
        if (Ours()) return false;
        RecordDraw('X', type, primCount, vertices, stride, minIndex, numVertices);
        DrawNameplatesBeforeSceneCopy();
        return false;
    }

private:
    // HeadsUp's own draws, which come back through the hooks.
    bool Ours() const { return m_Drawing || m_Outline.Drawing(); }

    bool BlocksGameCursor(D3DPRIMITIVETYPE type, UINT primCount, const void* vertices, UINT stride)
    {
        return m_Settings.enabled && m_Settings.replaceCursor &&
               m_Names.IsGameCursorDraw(type, primCount, vertices, stride, m_Tracker, m_Nameplates.CursorNames(), m_Picking,
                   m_AshitaCore->GetMemoryManager()->GetTarget());
    }

    // Developer builds (dev/) record each draw, and whether HeadsUp blocked or replaced it, for /hu drawdump.
    template <typename... Args>
    void RecordDraw([[maybe_unused]] Args... args)
    {
#ifdef HEADSUP_DEV
        m_DrawDump.Record(m_Device, m_Tracker, args...);
#endif
    }
    void MarkHidden()
    {
#ifdef HEADSUP_DEV
        m_DrawDump.MarkHidden();
#endif
    }

    // Saves the game's render states, render target and depth surface around draw, which may change any of them.
    template <typename Draw>
    void WithSavedState(Draw draw)
    {
        DWORD saved = 0;
        if (m_Device == nullptr) return;
        if (FAILED(m_Device->CreateStateBlock(D3DSBT_ALL, &saved)))
        {
            if (!std::exchange(m_StateWarned, true)) Print("nameplates are not drawn: Direct3D could not save the game's render states.");
            return;
        }
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
    }

    // Lays out and draws the nameplates into the bound render target, toX and toY pixels per back-buffer pixel.
    void DrawNameplates(float toX, float toY, bool depthTest)
    {
        m_Nameplates.Update(m_Tracker, m_Names, m_Settings, toX, toY, m_CursorTargets, Now());
        m_Nameplates.Draw(depthTest);
        m_PlatesPlaced = true;
    }

    // The game draws its 3D scene, names included, into an off-screen image and then copies that to the back buffer.
    // Drawn into the image just before the copy, at the name's depth, our nameplates are hidden by walls like the
    // game's names, and sit under the game's menus. The copy is the first draw to the back buffer after the names.
    void DrawNameplatesBeforeSceneCopy()
    {
        if (!headsup::NameplatesOn(m_Settings) || !m_Names.SceneCopyStarting()) return;
        m_Names.FinishText();
        WithSavedState([&] {
            if (FAILED(m_Device->SetRenderTarget(m_Names.SceneTarget(), m_Names.SceneDepth()))) return;
            DrawNameplates(1.0f / m_Names.TargetScaleX(), 1.0f / m_Names.TargetScaleY(), true);
            m_DrewInScene = true;
        });
    }

#ifdef HEADSUP_DEV
    void WriteDrawDump()
    {
        std::string path;
        std::FILE* out = OpenLog(m_DrawDump.FilePrefix().c_str(), path);
        if (out == nullptr) return;
        const uint32_t self = m_AshitaCore->GetMemoryManager()->GetParty()->GetMemberTargetIndex(0);
        const size_t draws  = m_DrawDump.Write(out, headsup::DrawDump::Sources{m_AshitaCore, m_Names, m_Nameplates.CursorNames(),
                                                        m_Nameplates.LastShown(), [&](uint32_t index) { return PlayerStatusOf(index, self); }});
        std::fclose(out);
        m_DrawDump.Wrote(path, draws);
        if (!m_DrawDump.Watching()) Print("wrote " + std::to_string(draws) + " draw calls to " + path);
    }
#endif

    void Print(const std::string& message)
    {
        const std::string line = Ashita::Chat::Header(kName) + Ashita::Chat::Message(message);
        m_AshitaCore->GetChatManager()->Write(kChatMode, false, line.c_str());
    }

    // Ashita's folder (config or logs) with HeadsUp's own folder in it, made when missing.
    std::string OwnFolder(const char* ashitaFolder)
    {
        const std::string parent = std::string(m_AshitaCore->GetInstallPath()) + ashitaFolder;
        CreateDirectoryA(parent.c_str(), nullptr);
        const std::string folder = parent + "\\" + kFolder;
        CreateDirectoryA(folder.c_str(), nullptr);
        return folder;
    }

    // A new logs/headsup/<prefix>-<time>.txt; nullptr, after saying so, when it cannot be written.
    std::FILE* OpenLog(const char* prefix, std::string& path)
    {
        char stamp[32];
        const std::time_t now = std::time(nullptr);
        std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", std::localtime(&now));
        path           = OwnFolder("logs") + "\\" + prefix + "-" + stamp + ".txt";
        std::FILE* out = std::fopen(path.c_str(), "w");
        if (out == nullptr) Print("could not write " + path + "; nothing was recorded");
        return out;
    }

    // Settings live in config/headsup/settings.ini; Ashita's configuration manager takes the path inside config.
    std::string SettingsPath() const { return std::string(kFolder) + "/" + kSettingsFile; }

    void LoadSettings()
    {
        IConfigurationManager* config = m_AshitaCore->GetConfigurationManager();
        const std::string file        = OwnFolder("config") + "\\" + kSettingsFile;
        // A missing file is fine: every value keeps its default.
        if (!config->Load(kConfigAlias, SettingsPath().c_str()) && GetFileAttributesA(file.c_str()) != INVALID_FILE_ATTRIBUTES)
            Print("could not read " + file + "; using the defaults, and a change in the menu will overwrite that file");
        AshitaStore store(config);
        m_Settings = headsup::LoadSettings(store);
    }

    void SaveSettings()
    {
        const std::string file        = OwnFolder("config") + "\\" + kSettingsFile;
        IConfigurationManager* config = m_AshitaCore->GetConfigurationManager();
        AshitaStore store(config);
        headsup::SaveSettings(m_Settings, store);
        if (!config->Save(kConfigAlias, SettingsPath().c_str()))
            Print("could not save " + file + "; the change lasts until Ashita closes");
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

    // A player's status from the game's memory, known at once, with what only the packets carry once seen.
    std::optional<headsup::PlayerStatus> CurrentStatus(IEntity* entity, uint32_t index, headsup::EntityKind kind, uint32_t self)
    {
        if (kind != headsup::EntityKind::Player) return std::nullopt;
        const headsup::PlayerStatus memory = headsup::StatusFromRender(entity->GetRenderFlags1(index), entity->GetRenderFlags2(index),
            entity->GetLinkshellColor(index));
        headsup::PlayerStatus status = headsup::WithPacketStatus(memory, PlayerStatusOf(index, self));
        status.levelSync             = m_LevelSynced.count(static_cast<uint16_t>(index)) != 0 || m_BuffSynced.count(index) != 0;
        return status;
    }

    const headsup::PlayerStatus* PlayerStatusOf(uint32_t index, uint32_t self) const
    {
        if (index == self) return m_OwnStatus ? &*m_OwnStatus : nullptr;
        const auto it = m_PlayerStatus.find(static_cast<uint16_t>(index));
        return it != m_PlayerStatus.end() ? &it->second : nullptr;
    }

    // The chocobo mouse pointer follows its setting.
    void UpdatePointer()
    {
        if (!m_Settings.enabled || !m_Settings.chocoboPointer)
        {
            m_PointerFailed = false;
            if (m_Pointer.Running()) m_Pointer.Stop();
            return;
        }
        if (!m_Pointer.Running() && !m_PointerFailed)
        {
            if (const std::string failure = m_Pointer.Start(); !failure.empty())
            {
                m_PointerFailed = true;
                Print("the chocobo pointer is off: " + failure + ".");
            }
        }
        m_Pointer.Animate();
    }

    void UpdateCursorTargets()
    {
        ITarget* target = m_AshitaCore->GetMemoryManager()->GetTarget();
        m_Picking       = target->GetIsSubTargetActive() != 0;
        m_CursorTargets = headsup::TargetsFromSlots(m_Picking, target->GetTargetIndex(0), target->GetTargetIndex(1),
            (target->GetLockedOnFlags() & kLockedOn) != 0, m_AshitaCore->GetMemoryManager()->GetEntity()->GetEntityMapSize());
        if (!m_Picking) m_Names.ForgetPickRange();
        m_CursorTargets.outOfRange = m_Picking && m_Names.PickOutOfRange();
        m_Names.SetEnlarged(m_Picking ? m_CursorTargets.subTarget : uint16_t{0});
        IConfigurationManager* config = m_AshitaCore->GetConfigurationManager();
        const float menuWidth = config->GetFloat(kBootConfig, kRegistry, kMenuWidth, 0.0f);
        const float menuHeight = config->GetFloat(kBootConfig, kRegistry, kMenuHeight, 0.0f);
        const Ashita::FFXI::targetwindow_t* window = target->GetRawStructureWindow();
        if (window != nullptr && menuWidth > 0.0f && menuHeight > 0.0f && m_Names.BackBufferWidth() > 0.0f)
        {
            const float x = m_Names.BackBufferWidth() / menuWidth, y = m_Names.BackBufferHeight() / menuHeight;
            m_CursorTargets.anchored   = true;
            m_CursorTargets.anchorX    = static_cast<float>(window->m_AnkX) * x;
            m_CursorTargets.anchorY    = static_cast<float>(window->m_AnkY) * y;
            m_CursorTargets.subAnchorX = static_cast<float>(window->m_SubAnkX) * x;
            m_CursorTargets.subAnchorY = static_cast<float>(window->m_SubAnkY) * y;
        }
    }

    void UpdateTracker(double now)
    {
        IEntity* entity = m_AshitaCore->GetMemoryManager()->GetEntity();
        IParty* party   = m_AshitaCore->GetMemoryManager()->GetParty();
        IPlayer* player = m_AshitaCore->GetMemoryManager()->GetPlayer();
        m_Inputs.clear();
        m_PartyIds.clear();
        for (uint32_t member = 0; member < kPartyMembers; ++member)
            if (party->GetMemberIsActive(member) != 0) m_PartyIds.push_back(party->GetMemberServerId(member));
        m_BuffSynced.clear();
        if (headsup::LevelSyncInBuffs(player->GetBuffs())) m_BuffSynced.insert(party->GetMemberTargetIndex(0));
        for (uint32_t member = 0; member < kPartyIconMembers; ++member)
            if (party->GetStatusIconsServerId(member) != 0 &&
                headsup::LevelSyncInPartyIcons(party->GetStatusIcons(member), party->GetStatusIconsBitMask(member)))
                m_BuffSynced.insert(party->GetStatusIconsTargetIndex(member));
        const uint32_t count = std::min<uint32_t>(entity->GetEntityMapSize(), headsup::kMaxEntities);
        const uint32_t self  = party->GetMemberTargetIndex(0);
        for (uint32_t i = 0; i < count; ++i)
        {
            if (entity->GetRawEntity(i) == nullptr) continue;
            const uintptr_t actor = entity->GetActorPointer(i);
            if (actor == 0) continue;
            const uint32_t serverId = entity->GetServerId(i);
            const uint32_t flags    = entity->GetSpawnFlags(i);
            const auto kind         = headsup::KindFromSpawnFlags(flags);
            const bool isMob        = kind == headsup::EntityKind::Mob;
            const bool isPlayer     = kind == headsup::EntityKind::Player;
            const bool alive        = entity->GetHPPercent(i) > 0;
            if (isMob && !alive) m_Checks.Forget(serverId); // the next spawn rolls a new level
            m_Inputs.push_back(headsup::ActorInput{static_cast<headsup::ActorPtr>(actor), static_cast<uint16_t>(i),
                serverId, kind, alive, headsup::DistanceFromSquared(entity->GetDistance(i)), EntityName(entity, i),
                isMob ? m_Checks.Result(serverId, now) : nullptr, CurrentStatus(entity, i, kind, self),
                isPlayer ? headsup::PoseFromStatus(entity->GetStatus(i)) : headsup::Pose::Standing,
                headsup::FromEntityPosition(entity->GetLocalPositionX(i), entity->GetLocalPositionY(i), entity->GetLocalPositionZ(i)),
                isMob && headsup::ClaimedByParty(entity->GetClaimStatus(i), m_PartyIds)});
        }
        m_Player.level   = player->GetMainJobLevel();
        m_Player.sitting = headsup::IsSittingStatus(entity->GetStatus(party->GetMemberTargetIndex(0)));
        m_Tracker.Update(m_Inputs, m_Player, m_Settings);
    }

    // /hu debug: what each outline and nameplate showed in the last frame, written to logs/headsup/.
    void WriteDebugReport()
    {
        std::string path;
        std::FILE* out = OpenLog("debug", path);
        if (out == nullptr) return;

        IEntity* entity = m_AshitaCore->GetMemoryManager()->GetEntity();
        std::fprintf(out, "headsup %.2f: back buffer %.0fx%.0f, player level %d%s, %u outlined, %u nameplates, replace %s\n",
            GetVersion(), m_Names.BackBufferWidth(), m_Names.BackBufferHeight(), m_Player.level,
            m_Player.sitting ? " (sitting)" : "", m_Tracker.OutlinedCount(),
            static_cast<unsigned>(m_Nameplates.LastShown().size()), m_Settings.replaceMobNames ? "on" : "off");
        for (const headsup::ActorPtr actor : m_Tracker.Actors())
        {
            const headsup::ActorInfo* info = m_Tracker.Find(actor);
            if (info == nullptr || (!info->outline && m_Names.NameplateBox(info->index) == nullptr)) continue;
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
            if (const headsup::ScreenBox* plate = m_Names.NameplateBox(info->index))
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
        m_Capture     = "\nframe  dt(ms)  name glyphs: in-scene from-mobs from-others no-owner hidden | nameplates shown, S if drawn into the scene, s<scene to screen scale> | per entity "
                    "with a nameplate: index r<frames in row> m<body meshes> g<glyphs> c<name color> (nameplate box)\n";
        m_CaptureLeft = kCaptureFrames;
        m_CaptureLast = Now();
    }

    // One line per frame after /hu debug: where nameplate draws went and what the labels did.
    void CaptureFrame()
    {
        if (m_CaptureLeft <= 0) return;
        const double now = Now();
        const auto& stats = m_Names.TextStatsLastFrame();
        char line[200];
        std::snprintf(line, sizeof(line), "f%03d %6.1f  %3u %3u %3u %3u %3u | %2u %c s%.3f |", kCaptureFrames - m_CaptureLeft,
            (now - m_CaptureLast) * 1000.0, stats.inScene, stats.fromMobs, stats.fromOthers, stats.noOwner, stats.hidden,
            static_cast<unsigned>(m_Nameplates.LastShown().size()), m_DrewInScene ? 'S' : '-', m_Names.TargetScaleY());
        m_Capture += line;
        int listed = 0;
        for (const headsup::ActorPtr actor : m_Tracker.Actors())
        {
            const headsup::ActorInfo* info = m_Tracker.Find(actor);
            const headsup::ScreenBox* plate = info != nullptr ? m_Names.NameplateBox(info->index) : nullptr;
            if (plate == nullptr || ++listed > kCaptureMobs) continue;
            std::snprintf(line, sizeof(line), " %u r%u m%u g%u c%06X (%.0f-%.0f,%.0f-%.0f)", info->index,
                m_Names.PlateFramesInRow(info->index), m_Names.MeshDraws(info->index), m_Names.GlyphsLastFrame(info->index),
                static_cast<unsigned>(m_Names.NameplateColor(info->index) & 0xFFFFFF), plate->minX, plate->maxX, plate->minY,
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
            else
                Print("could not add the frame capture to " + m_CapturePath + "; the report before it is complete");
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
