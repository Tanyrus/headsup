#include "Ashita.h"

#include "examine.h"
#include "nameplate_render.h"
#include "menu.h"
#include "mobdata.h"
#include "outline.h"
#include "player_status.h"
#include "settings.h"
#include "tracker.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <unordered_map>
#include <optional>
#include <vector>

namespace
{
    constexpr const char* kName          = "headsup";
    constexpr const char* kConfigAlias   = "headsup";
    constexpr const char* kConfigFile    = "headsup/settings.ini";   // relative to Ashita's config folder
    constexpr const char* kOldConfigFile = "aggroglow\\settings.ini"; // the plugin's settings under its old name
    constexpr const char* kSection       = "settings";
    constexpr uint32_t kSpawnFlagPlayer = 0x01; // IEntity::GetSpawnFlags
    constexpr uint32_t kSpawnFlagMob   = 0x10;
    constexpr uint32_t kLockedOn       = 0x01;  // ITarget::GetLockedOnFlags
    constexpr uint16_t kZoneInPacket   = 0x00A;
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
        std::string GetString(const char* key, const char* fallback) override
        {
            const char* value = m_Config->GetString(kConfigAlias, kSection, key);
            return value != nullptr ? value : fallback;
        }
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
    std::unordered_map<uint16_t, headsup::PlayerStatus> m_PlayerStatus; // other players, by target index
    std::optional<headsup::PlayerStatus> m_OwnStatus;
    std::vector<headsup::PlayerStatus> m_Statuses; // this frame's, for the tracker's inputs
    std::vector<headsup::CursorAnchor> m_Anchors;   // see GameCursorAnchors
    bool m_DebugPending = false;
    bool m_PlatesPlaced = false; // placed this frame, in the scene or at the back-buffer EndScene

    // /hu drawdump [seconds]: every draw call of one frame, after the given delay, with the target window, the players'
    // status and render flags, the scene camera and the players' poses, to logs/headsup/drawdump-<time>.txt.
    struct DrawRecord
    {
        char hook; // P DrawPrimitive, I DrawIndexedPrimitive, U DrawPrimitiveUP, X DrawIndexedPrimitiveUP
        uint32_t type, count;
        DWORD shader, zenable, alphablend, srcblend, destblend;
        uintptr_t texture, target;
        uint32_t targetWidth, targetHeight;
        bool hasBox;
        bool hidden; // HeadsUp blocked it
        float x0, y0, x1, y1, z0, z1; // pretransformed vertices
        float wx, wy, wz;             // the world matrix's translation
        int owner;
    };
    enum class DumpState
    {
        Idle,
        Waiting,
        Recording,
    };
    DumpState m_DumpState = DumpState::Idle;
    double m_DumpAt       = 0.0;
    std::vector<DrawRecord> m_DrawLog;
    D3DMATRIX m_DumpView{}, m_DumpProj{}; // the camera, from a fixed-function scene draw
    D3DVIEWPORT8 m_DumpViewport{};
    bool m_DumpHaveCamera = false;
    bool m_Drawing      = false; // drawing nameplates: our own draws come back through the hooks
    bool m_DrewInScene  = false; // this frame, for /hu debug
    IDirect3DDevice8* m_Device = nullptr;
    headsup::NameplateRenderer::CursorTargets m_CursorTargets;
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
        else if (args[1] == "drawdump")
        {
            const double delay = args.size() > 2 ? std::atof(args[2].c_str()) : 0.0;
            m_DumpAt           = Now() + delay;
            m_DumpState        = DumpState::Waiting;
            Print("recording one frame of draw calls" + std::string(delay > 0.0 ? " in " + args[2] + " seconds" : ""));
        }
        else
        {
            Print("/headsup or /hu: open or close the settings window");
            Print("/hu on | /hu off: turn outlines and nameplates on or off");
            Print("/hu debug: write what every mob's outline and nameplate show to logs/headsup");
            Print("/hu drawdump [seconds]: write every draw call of one frame, after the delay, to logs/headsup");
        }
        return true;
    }

    // Players' statuses, for the icons beside their names, and the reply to the player's own /check: the mob's exact
    // level, for its label until it respawns. Every packet still reaches the game.
    bool HandleIncomingPacket(uint16_t id, uint32_t size, const uint8_t* data, uint8_t* modified, uint32_t sizeChunk,
        const uint8_t* dataChunk, bool injected, bool blocked) override
    {
        UNREFERENCED_PARAMETER(modified);
        UNREFERENCED_PARAMETER(sizeChunk);
        UNREFERENCED_PARAMETER(dataChunk);
        UNREFERENCED_PARAMETER(injected);
        UNREFERENCED_PARAMETER(blocked);
        if (id == kZoneInPacket) m_PlayerStatus.clear(); // target indexes are reused in the next zone
        if (id == headsup::kOtherPlayerPacket)
        {
            if (const auto update = headsup::ParseOtherPlayer(data, size))
            {
                if (update->despawn)
                    m_PlayerStatus.erase(update->index);
                else if (update->status)
                    m_PlayerStatus[update->index] = *update->status;
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
        if (m_DumpState == DumpState::Recording)
            WriteDrawDump();
        else if (m_DumpState == DumpState::Waiting && Now() >= m_DumpAt)
        {
            m_DrawLog.clear();
            m_DumpHaveCamera = false;
            m_DumpState      = DumpState::Recording;
        }
        m_Outline.NewFrame();
        if (m_Outline.TakeStencilWarning())
            Print("a mob could not be outlined: the game's depth buffer has no stencil bits.");
        // A frame whose nameplates were not drawn keeps none of the last layout.
        if (!m_PlatesPlaced) m_Nameplates.Clear();
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
        UpdateCursorTargets();
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
        if (m_Drawing) return false;
        RecordDraw('P', type, primCount, nullptr, 0, 0);
        DrawNameplatesBeforeSceneCopy();
        return false;
    }

    bool Direct3DDrawIndexedPrimitive(D3DPRIMITIVETYPE type, UINT minIndex, UINT numVertices, UINT startIndex, UINT primCount) override
    {
        if (m_Drawing) return false;
        RecordDraw('I', type, primCount, nullptr, 0, 0);
        DrawNameplatesBeforeSceneCopy();
        return m_Outline.OnDrawIndexed(type, minIndex, numVertices, startIndex, primCount, m_Tracker, m_Settings);
    }

    bool Direct3DDrawPrimitiveUP(D3DPRIMITIVETYPE type, UINT primCount, const void* vertices, UINT stride) override
    {
        if (m_Drawing) return false;
        RecordDraw('U', type, primCount, vertices, stride, headsup::VertexCount(type, primCount));
        DrawNameplatesBeforeSceneCopy();
        // The game's target cursor is not drawn where ours replaces it.
        bool blocked = m_Settings.enabled && m_Settings.replaceCursor &&
                       m_Outline.IsGameCursorDraw(type, primCount, vertices, stride, m_Tracker, m_Nameplates.CursorNames(),
                           GameCursorAnchors());
        // The game's names are measured, and those HeadsUp replaces are blocked.
        if (!blocked) blocked = m_Outline.OnDrawUP(type, primCount, vertices, stride, m_Tracker, m_Settings);
        if (m_DumpState == DumpState::Recording && !m_DrawLog.empty()) m_DrawLog.back().hidden = blocked;
        return blocked;
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
        if (m_Drawing) return false;
        RecordDraw('X', type, primCount, vertices, stride, numVertices);
        DrawNameplatesBeforeSceneCopy();
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
        m_Nameplates.Update(m_Tracker, m_Outline, m_Settings, toX, toY, m_CursorTargets, Now());
        m_Nameplates.Draw(depthTest);
        m_PlatesPlaced = true;
    }

    // The game draws its 3D scene, names included, into an off-screen image and then copies that to the back buffer.
    // Drawn into the image just before the copy, at the name's depth, our nameplates are hidden by walls like the
    // game's names, and sit under the game's menus. The copy is the first draw to the back buffer after the names.
    void DrawNameplatesBeforeSceneCopy()
    {
        if (!m_Settings.enabled || !headsup::NameplatesOn(m_Settings) ||
            m_Outline.TargetScaleX() <= 0.0f || m_Outline.TargetScaleY() <= 0.0f || !m_Outline.SceneCopyStarting())
            return;
        m_Outline.FinishText();
        WithSavedState([&] {
            if (FAILED(m_Device->SetRenderTarget(m_Outline.SceneTarget(), m_Outline.SceneDepth()))) return;
            DrawNameplates(1.0f / m_Outline.TargetScaleX(), 1.0f / m_Outline.TargetScaleY(), true);
            m_DrewInScene = true;
        });
    }

    void RecordDraw(char hook, D3DPRIMITIVETYPE type, UINT count, const void* vertices, UINT stride, uint32_t vertexCount)
    {
        if (m_DumpState != DumpState::Recording || m_Device == nullptr) return;
        DrawRecord r{};
        r.hook  = hook;
        r.type  = static_cast<uint32_t>(type);
        r.count = count;
        m_Device->GetVertexShader(&r.shader);
        m_Device->GetRenderState(D3DRS_ZENABLE, &r.zenable);
        m_Device->GetRenderState(D3DRS_ALPHABLENDENABLE, &r.alphablend);
        m_Device->GetRenderState(D3DRS_SRCBLEND, &r.srcblend);
        m_Device->GetRenderState(D3DRS_DESTBLEND, &r.destblend);
        IDirect3DBaseTexture8* texture = nullptr;
        if (SUCCEEDED(m_Device->GetTexture(0, &texture)) && texture != nullptr)
        {
            r.texture = reinterpret_cast<uintptr_t>(texture);
            texture->Release();
        }
        IDirect3DSurface8* target = nullptr;
        if (SUCCEEDED(m_Device->GetRenderTarget(&target)) && target != nullptr)
        {
            r.target = reinterpret_cast<uintptr_t>(target);
            D3DSURFACE_DESC desc{};
            if (SUCCEEDED(target->GetDesc(&desc))) r.targetWidth = desc.Width, r.targetHeight = desc.Height;
            target->Release();
        }
        const bool pretransformed = r.shader < 0x10000 && (r.shader & D3DFVF_POSITION_MASK) == D3DFVF_XYZRHW;
        if (pretransformed && vertices != nullptr && stride >= 3 * sizeof(float) && vertexCount > 0 && vertexCount <= 4096)
        {
            r.hasBox = true;
            r.x0 = r.y0 = r.z0 = 1e9f;
            r.x1 = r.y1 = r.z1 = -1e9f;
            for (uint32_t i = 0; i < vertexCount; ++i)
            {
                float xyz[3];
                std::memcpy(xyz, static_cast<const uint8_t*>(vertices) + static_cast<size_t>(i) * stride, sizeof(xyz));
                r.x0 = std::min(r.x0, xyz[0]), r.x1 = std::max(r.x1, xyz[0]);
                r.y0 = std::min(r.y0, xyz[1]), r.y1 = std::max(r.y1, xyz[1]);
                r.z0 = std::min(r.z0, xyz[2]), r.z1 = std::max(r.z1, xyz[2]);
            }
        }
        else if (!pretransformed)
        {
            D3DMATRIX world{};
            m_Device->GetTransform(D3DTS_WORLD, &world);
            r.wx = world._41, r.wy = world._42, r.wz = world._43;
        }
        const headsup::ActorInfo* owner = m_Outline.DrawOwner(m_Tracker);
        r.owner                           = owner != nullptr ? owner->index : -1;
        if (!m_DumpHaveCamera && !pretransformed && r.shader < 0x10000 && owner != nullptr && r.targetWidth > 2000)
        {
            m_Device->GetTransform(D3DTS_VIEW, &m_DumpView);
            m_Device->GetTransform(D3DTS_PROJECTION, &m_DumpProj);
            m_Device->GetViewport(&m_DumpViewport);
            m_DumpHaveCamera = true;
        }
        m_DrawLog.push_back(r);
    }

    void WriteDrawDump()
    {
        m_DumpState = DumpState::Idle;
        const std::string logs = std::string(m_AshitaCore->GetInstallPath()) + "logs";
        CreateDirectoryA(logs.c_str(), nullptr);
        CreateDirectoryA((logs + "\\headsup").c_str(), nullptr);
        char file[48];
        const std::time_t now = std::time(nullptr);
        std::strftime(file, sizeof(file), "\\headsup\\drawdump-%Y%m%d-%H%M%S.txt", std::localtime(&now));
        const std::string path = logs + file;
        std::FILE* out         = std::fopen(path.c_str(), "w");
        if (out == nullptr)
        {
            Print("could not write " + path);
            return;
        }
        IEntity* entity = m_AshitaCore->GetMemoryManager()->GetEntity();
        ITarget* target = m_AshitaCore->GetMemoryManager()->GetTarget();
        IConfigurationManager* config = m_AshitaCore->GetConfigurationManager();
        std::fprintf(out, "back buffer %.0fx%.0f, menu %.0fx%.0f, sub-target active %u\n", m_Outline.BackBufferWidth(),
            m_Outline.BackBufferHeight(), config->GetFloat("boot", "ffxi.registry", "0037", 0.0f),
            config->GetFloat("boot", "ffxi.registry", "0038", 0.0f), target->GetIsSubTargetActive());
        if (const Ashita::FFXI::targetwindow_t* window = target->GetRawStructureWindow())
            std::fprintf(out, "window: sub %u ank num %u ank (%d,%d) sub ank (%d,%d)\n", window->m_Sub, window->m_AnkNum,
                window->m_AnkX, window->m_AnkY, window->m_SubAnkX, window->m_SubAnkY);
        for (uint32_t t = 0; t < 2; ++t)
        {
            const uint32_t index = target->GetTargetIndex(t);
            std::fprintf(out, "target %u: index %u '%s' active %u arrow active %u arrow (%.2f,%.2f,%.2f,%.2f)", t, index,
                EntityName(entity, index), target->GetIsActive(t), target->GetIsArrowActive(t),
                target->GetArrowPositionX(t), target->GetArrowPositionY(t), target->GetArrowPositionZ(t),
                target->GetArrowPositionW(t));
            if (index != 0 && index < entity->GetEntityMapSize() && entity->GetRawEntity(index) != nullptr)
            {
                std::fprintf(out, " entity at (%.2f,%.2f,%.2f)", entity->GetLocalPositionX(index),
                    entity->GetLocalPositionY(index), entity->GetLocalPositionZ(index));
                if (const headsup::ScreenBox* plate = m_Outline.NameplateBox(static_cast<uint16_t>(index)))
                    std::fprintf(out, " name box (%.0f,%.0f)-(%.0f,%.0f)", plate->minX, plate->minY, plate->maxX, plate->maxY);
            }
            std::fprintf(out, "\n");
        }
        for (const headsup::CursorName& n : m_Nameplates.CursorNames())
            std::fprintf(out, "our cursor over %u, name (%.1f,%.1f)-(%.1f,%.1f)\n", n.index, n.name.minX, n.name.minY, n.name.maxX,
                n.name.maxY);
        // Players: their status from the packets beside the render flags the game keeps, to find where it keeps them.
        IParty* party       = m_AshitaCore->GetMemoryManager()->GetParty();
        const uint32_t self = party->GetMemberTargetIndex(0);
        std::fprintf(out, "players: index name | packet seek bazaar ls away mentor new gm lscolor | memory flags0-8 lscolor\n");
        for (uint32_t i = 0; i < std::min<uint32_t>(entity->GetEntityMapSize(), kMaxEntities); ++i)
        {
            if (entity->GetRawEntity(i) == nullptr || (entity->GetSpawnFlags(i) & kSpawnFlagPlayer) == 0) continue;
            const headsup::PlayerStatus* st = PlayerStatusOf(i, self);
            std::fprintf(out, "player %u '%s'%s |", i, EntityName(entity, i), i == self ? " (you)" : "");
            if (st != nullptr)
                std::fprintf(out, " %d %d %d %d %d %d %d %08X |", st->seekingParty, st->bazaar, st->linkshell, st->away, st->mentor,
                    st->newAdventurer, st->gm, static_cast<unsigned>(st->linkshellArgb));
            else
                std::fprintf(out, " unknown |");
            std::fprintf(out, " %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X\n", static_cast<unsigned>(entity->GetRenderFlags0(i)),
                static_cast<unsigned>(entity->GetRenderFlags1(i)), static_cast<unsigned>(entity->GetRenderFlags2(i)),
                static_cast<unsigned>(entity->GetRenderFlags3(i)), static_cast<unsigned>(entity->GetRenderFlags4(i)),
                static_cast<unsigned>(entity->GetRenderFlags5(i)), static_cast<unsigned>(entity->GetRenderFlags6(i)),
                static_cast<unsigned>(entity->GetRenderFlags7(i)), static_cast<unsigned>(entity->GetRenderFlags8(i)),
                static_cast<unsigned>(entity->GetLinkshellColor(i)));
        }
        // Poses: where the game puts each player's name against where they stand, with the camera to project them.
        if (m_DumpHaveCamera)
        {
            auto matrix = [&](const char* name, const D3DMATRIX& m) {
                std::fprintf(out, "%s", name);
                for (int row = 0; row < 4; ++row)
                    for (int col = 0; col < 4; ++col)
                        std::fprintf(out, " %.6g", m.m[row][col]);
                std::fprintf(out, "\n");
            };
            matrix("view", m_DumpView);
            matrix("projection", m_DumpProj);
            std::fprintf(out, "viewport %lu %lu %lu %lu\n", static_cast<unsigned long>(m_DumpViewport.X),
                static_cast<unsigned long>(m_DumpViewport.Y), static_cast<unsigned long>(m_DumpViewport.Width),
                static_cast<unsigned long>(m_DumpViewport.Height));
        }
        else
            std::fprintf(out, "no camera seen\n");
        std::fprintf(out, "poses: index name | status modelSize hitboxSize | position x y z | whole name box\n");
        for (uint32_t i = 0; i < std::min<uint32_t>(entity->GetEntityMapSize(), kMaxEntities); ++i)
        {
            if (entity->GetRawEntity(i) == nullptr || (entity->GetSpawnFlags(i) & kSpawnFlagPlayer) == 0) continue;
            std::fprintf(out, "pose %u '%s' | %lu %.4f %.4f | %.3f %.3f %.3f |", i, EntityName(entity, i),
                static_cast<unsigned long>(entity->GetStatus(i)), entity->GetModelSize(i), entity->GetModelHitboxSize(i),
                entity->GetLocalPositionX(i), entity->GetLocalPositionY(i), entity->GetLocalPositionZ(i));
            if (const headsup::ScreenBox* whole = m_Outline.WholeNameplate(static_cast<uint16_t>(i)))
                std::fprintf(out, " (%.1f,%.1f)-(%.1f,%.1f)\n", whole->minX, whole->minY, whole->maxX, whole->maxY);
            else
                std::fprintf(out, " none\n");
        }
        std::fprintf(out, "seq hook type count shader z blend src dst texture target(size) owner | box (x0,y0)-(x1,y1) z z0-z1 or world (x,y,z)\n");
        for (size_t i = 0; i < m_DrawLog.size(); ++i)
        {
            const DrawRecord& r = m_DrawLog[i];
            std::fprintf(out, "%4u %c%c %u %4u %08lX %lu %lu %2lu %2lu %08X %08X(%ux%u) %4d |", static_cast<unsigned>(i), r.hook,
                r.hidden ? 'H' : ' ',
                r.type, r.count, static_cast<unsigned long>(r.shader), static_cast<unsigned long>(r.zenable),
                static_cast<unsigned long>(r.alphablend), static_cast<unsigned long>(r.srcblend),
                static_cast<unsigned long>(r.destblend), static_cast<unsigned>(r.texture), static_cast<unsigned>(r.target),
                r.targetWidth, r.targetHeight, r.owner);
            if (r.hasBox)
                std::fprintf(out, " box (%.1f,%.1f)-(%.1f,%.1f) z %.5f-%.5f\n", r.x0, r.y0, r.x1, r.y1, r.z0, r.z1);
            else
                std::fprintf(out, " world (%.2f,%.2f,%.2f)\n", r.wx, r.wy, r.wz);
        }
        std::fclose(out);
        Print(std::string("wrote ") + std::to_string(m_DrawLog.size()) + " draw calls to " + path);
        m_DrawLog.clear();
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

    // A player's status from the game's memory, known at once, with what only the packets carry once seen.
    const headsup::PlayerStatus* CurrentStatus(IEntity* entity, uint32_t index, headsup::EntityKind kind, uint32_t self)
    {
        if (kind != headsup::EntityKind::Player) return nullptr;
        const headsup::PlayerStatus memory = headsup::StatusFromRender(entity->GetRenderFlags1(index), entity->GetRenderFlags2(index),
            entity->GetLinkshellColor(index));
        m_Statuses.push_back(headsup::WithPacketStatus(memory, PlayerStatusOf(index, self)));
        return &m_Statuses.back();
    }

    const headsup::PlayerStatus* PlayerStatusOf(uint32_t index, uint32_t self) const
    {
        if (index == self) return m_OwnStatus ? &*m_OwnStatus : nullptr;
        const auto it = m_PlayerStatus.find(static_cast<uint16_t>(index));
        return it != m_PlayerStatus.end() ? &it->second : nullptr;
    }

    // While a sub-target is being picked, slot 1 holds the target and slot 0 the candidate under the sub-target cursor.
    // Where the game is drawing its target cursors this frame, for either of our cursor targets.
    const std::vector<headsup::CursorAnchor>& GameCursorAnchors()
    {
        m_Anchors.clear();
        const Ashita::FFXI::targetwindow_t* window = m_AshitaCore->GetMemoryManager()->GetTarget()->GetRawStructureWindow();
        if (window == nullptr) return m_Anchors;
        for (const uint16_t index : {m_CursorTargets.target, m_CursorTargets.subTarget})
        {
            if (index == 0) continue;
            m_Anchors.push_back({index, static_cast<float>(window->m_AnkX), static_cast<float>(window->m_AnkY)});
            m_Anchors.push_back({index, static_cast<float>(window->m_SubAnkX), static_cast<float>(window->m_SubAnkY)});
        }
        return m_Anchors;
    }

    void UpdateCursorTargets()
    {
        ITarget* target   = m_AshitaCore->GetMemoryManager()->GetTarget();
        IEntity* entity   = m_AshitaCore->GetMemoryManager()->GetEntity();
        const bool picking = target->GetIsSubTargetActive() != 0;
        auto index         = [&](uint32_t slot) {
            const uint32_t i = target->GetTargetIndex(slot);
            return i < entity->GetEntityMapSize() ? static_cast<uint16_t>(i) : uint16_t{0};
        };
        m_CursorTargets = {index(picking ? 1 : 0), picking ? index(0) : uint16_t{0}, (target->GetLockedOnFlags() & kLockedOn) != 0};
    }

    void UpdateTracker(double now)
    {
        IEntity* entity = m_AshitaCore->GetMemoryManager()->GetEntity();
        IParty* party   = m_AshitaCore->GetMemoryManager()->GetParty();
        IPlayer* player = m_AshitaCore->GetMemoryManager()->GetPlayer();
        m_Inputs.clear();
        const uint32_t count = std::min<uint32_t>(entity->GetEntityMapSize(), kMaxEntities);
        const uint32_t self  = party->GetMemberTargetIndex(0);
        m_Statuses.clear();
        m_Statuses.reserve(count); // the inputs point into it, so it must not grow
        for (uint32_t i = 0; i < count; ++i)
        {
            if (entity->GetRawEntity(i) == nullptr) continue;
            const uintptr_t actor = entity->GetActorPointer(i);
            if (actor == 0) continue;
            const uint32_t serverId = entity->GetServerId(i);
            const uint32_t flags    = entity->GetSpawnFlags(i);
            const auto kind         = (flags & kSpawnFlagMob) != 0      ? headsup::EntityKind::Mob
                                      : (flags & kSpawnFlagPlayer) != 0 ? headsup::EntityKind::Player
                                                                        : headsup::EntityKind::Npc;
            const bool isMob        = kind == headsup::EntityKind::Mob;
            const bool isPlayer     = kind == headsup::EntityKind::Player;
            const bool alive        = entity->GetHPPercent(i) > 0;
            if (isMob && !alive) m_Checks.Forget(serverId); // the next spawn rolls a new level
            m_Inputs.push_back(headsup::ActorInput{static_cast<headsup::ActorPtr>(actor), static_cast<uint16_t>(i),
                serverId, kind, alive, headsup::DistanceFromSquared(entity->GetDistance(i)), EntityName(entity, i),
                isMob ? m_Checks.Result(serverId, now) : nullptr, CurrentStatus(entity, i, kind, self),
                isPlayer ? headsup::PoseFromStatus(entity->GetStatus(i)) : headsup::Pose::Standing,
                headsup::FromEntityPosition(entity->GetLocalPositionX(i), entity->GetLocalPositionY(i), entity->GetLocalPositionZ(i))});
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
        for (const headsup::ActorPtr actor : m_Tracker.Actors())
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
        for (const headsup::ActorPtr actor : m_Tracker.Actors())
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
