#include "Ashita.h"

#include "check.h"
#include "commands.h"
#include "d3d_util.h"
#include "fonts.h"
#include "game_cursor.h"
#include "game_names.h"
#include "menu.h"
#include "mobdata.h"
#include "nameplate_render.h"
#include "native_hook.h"
#include "outline.h"
#include "ph_timers.h"
#include "player_status.h"
#include "pointer_swap.h"
#include "settings.h"
#include "tracker.h"
#ifdef HEADSUP_DEV
#include "codedump.h"
#include "drawdump.h"
#include "namedump.h"
#include "stackdump.h"
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
    constexpr const char* kName              = "headsup";
    // Ashita takes a plugin's version as one number, so it holds the release tag's major.minor; the release workflow
    // checks the two agree.
    constexpr double kVersion                = 0.6;
    constexpr const char* kConfigAlias       = "headsup";
    constexpr const char* kConfigFolder      = "config";
    constexpr const char* kLogsFolder        = "logs";
    constexpr const char* kFolder            = "headsup"; // inside Ashita's config and logs folders
    constexpr const char* kSettingsFile      = "settings.ini";
    constexpr const char* kSection           = "settings";
    constexpr uint32_t kLockedOn             = 0x01; // ITarget::GetLockedOnFlags
    constexpr uint16_t kZoneInPacket         = 0x00A;
    constexpr int kCaptureFrames             = 120;
    constexpr int kCapturedPlatesPerFrame    = 16;
    constexpr double kFrameTimeWeight        = 0.05;
    constexpr int32_t kChatMode              = 1; // the chat mode Ashita's own plugins print in
    // The game's menu resolution, which its target window's arrow anchors are in, from its registry settings.
    constexpr const char* kBootConfig        = "boot";
    constexpr const char* kRegistry          = "ffxi.registry";
    constexpr const char* kMenuWidth         = "0037";
    constexpr const char* kMenuHeight        = "0038";
    constexpr uint32_t kPartyIconMembers     = 5;  // the other members of your party, whose buffs the game keeps
    constexpr uint32_t kPartyMembers         = 18; // you, your party and the two other alliance parties
    constexpr uint32_t kEngagedStatus        = 1;  // an entity's status while it fights

    const char* EntityName(IEntity* entity, uint32_t index)
    {
        if (index >= entity->GetEntityMapSize() || entity->GetRawEntity(index) == nullptr) return "";
        const char* name = entity->GetName(index);
        return name != nullptr ? name : "";
    }

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
    headsup::PhTimers m_PhTimers;
    std::vector<headsup::TimerLine> m_TimerLines;
    uint16_t m_SelfIndex = 0;
    headsup::OutlineRenderer m_Outline;
    headsup::GameNames m_Names;
    headsup::NameplateRenderer m_Nameplates;
    headsup::PointerSwap m_Pointer;
    std::vector<HANDLE> m_LoadedFonts;
    headsup::NameHook m_NameHook;
    bool m_NameHookTried = false;
    headsup::ScreenBox m_GameNameBox;
    bool m_PointerFailed = false; // said once; tried again when the setting changes
    headsup::Menu m_Menu;
    headsup::PlayerState m_Player;
    headsup::CheckResults m_Checks;
    std::unordered_map<uint16_t, headsup::PlayerStatus> m_PlayerStatus; // other players, by target index
    std::unordered_set<uint16_t> m_LevelSynced; // players, you included, by target index, from packets
    std::unordered_set<uint16_t> m_BuffSynced;  // you and your party, from the buffs the game keeps, this frame
    std::vector<uint32_t> m_PartyIds;            // server IDs of you, your party and alliance, this frame
    std::optional<headsup::PlayerStatus> m_OwnStatus;
    bool m_DebugPending = false;
    bool m_PlatesPlaced = false; // this frame, in the scene or at the back-buffer EndScene

#ifdef HEADSUP_DEV
    headsup::DrawDump m_DrawDump;
#endif
    bool m_Drawing      = false; // drawing nameplates: our own draws come back through the hooks
    bool m_DrewInScene  = false; // this frame
    bool m_StateWarned  = false; // said once that the game's render states could not be saved
    bool m_RestoreWarned = false; // said once that Direct3D did not put the game's states back
    IDirect3DDevice8* m_Device = nullptr;
    headsup::CursorTargets m_CursorTargets;
    bool m_Picking      = false;
    float m_MenuWidth   = 0.0f;
    float m_MenuHeight  = 0.0f;
    int m_CaptureLeft   = 0;
    std::string m_CapturePath;
    std::string m_Capture;
    double m_CaptureLast = 0.0;
    LARGE_INTEGER m_QpcFrequency{};
    double m_LastPresent = 0.0;
    double m_FrameMs     = 0.0;

public:
    const char* GetName(void) const override { return kName; }
    const char* GetAuthor(void) const override { return "tanyrus"; }
    const char* GetDescription(void) const override
    {
        return "Outlines monsters by whether they will attack you and replaces the game's nameplates with its own.";
    }
    const char* GetLink(void) const override { return ""; }
    double GetVersion(void) const override { return kVersion; }
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
        IConfigurationManager* config = core->GetConfigurationManager();
        m_MenuWidth                   = config->GetFloat(kBootConfig, kRegistry, kMenuWidth, 0.0f);
        m_MenuHeight                  = config->GetFloat(kBootConfig, kRegistry, kMenuHeight, 0.0f);
        size_t fonts = 0;
        for (const headsup::BundledFont* font = headsup::BundledFonts(fonts); fonts-- > 0; ++font)
        {
            DWORD installed = 0;
            // Loaded for this process only: nothing is written to the system's fonts.
            if (HANDLE handle = AddFontMemResourceEx(const_cast<unsigned char*>(font->data), static_cast<DWORD>(font->bytes), nullptr,
                    &installed);
                handle != nullptr)
                m_LoadedFonts.push_back(handle);
        }
        LoadSettings();
        return true;
    }

    void Release(void) override
    {
        for (HANDLE handle : m_LoadedFonts)
            RemoveFontMemResourceEx(handle);
        m_LoadedFonts.clear();
        if (const std::string failure = m_NameHook.Stop(); !failure.empty()) Print(failure + ".");
        m_Pointer.Stop();
        m_Nameplates.Release();
    }

    bool Direct3DInitialize(IDirect3DDevice8* device) override
    {
        m_Device = device;
        m_Outline.SetDevice(device);
        m_Names.SetDevice(device);
        m_Nameplates.SetDevice(device);
        m_NameHook.SetDevice(device);
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
            Print(m_DrawDump.Start(args.size() > 2 ? args[2] : "", Now(), m_AshitaCore, OwnFolder(kLogsFolder)));
        else if (args[1] == "codedump")
            Print(headsup::WriteCodeDump(OwnFolder(kLogsFolder)));
        else if (args[1] == "namedump")
            Print(headsup::ArmNameDump(m_NameHook.Running()));
        else if (args[1] == "stackdump")
            Print(headsup::ArmStackDump());
#endif
        else
        {
            Print("/headsup or /hu: open or close the settings window");
            Print("/hu on | /hu off: turn outlines and nameplates on or off");
            Print("/hu debug: write what every outline and nameplate show to logs/headsup");
#ifdef HEADSUP_DEV
            Print("/hu drawdump [seconds | watch | stop]: write every draw call of one frame to logs/headsup after the delay, or watch every frame for name glitches until stopped");
            Print("/hu codedump: write the client's loaded image to logs/headsup");
            Print("/hu namedump: write every call into the game's name routine during one frame to logs/headsup");
            Print("/hu stackdump: write the stack at the character draws of one frame to logs/headsup");
#endif
        }
        return true;
    }

    // HeadsUp only reads packets: every one still reaches the game.
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
        switch (id)
        {
        case kZoneInPacket:
            m_PlayerStatus.clear(); // target indexes are reused in the next zone
            m_LevelSynced.clear();
            m_Names.ForgetCamera();
            break;
        case headsup::kOtherPlayerPacket:
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
            break;
        case headsup::kCharSyncPacket:
            if (const auto sync = headsup::ParseCharSync(data, size))
            {
                if (sync->synced)
                    m_LevelSynced.insert(sync->index);
                else
                    m_LevelSynced.erase(sync->index);
            }
            break;
        case headsup::kOwnStatusPacket:
            if (const auto own = headsup::ParseOwnStatus(data, size)) m_OwnStatus = *own;
            break;
        case headsup::kCheckReplyPacket:
            OnCheckReply(data, size);
            break;
        }
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
        if (m_DrawDump.NextFrame(Now(), m_Names, m_NameHook, m_Tracker, m_Nameplates)) WriteDrawDump();
        if (headsup::NameDumpReady()) WriteNameDump();
        if (headsup::StackDumpReady()) WriteStackDump();
#endif
        StartNameHook();
        m_Outline.NewFrame();
        m_Names.NewFrame();
        if (m_Outline.TakeStencilWarning())
            Print("a mob could not be outlined: the game's depth buffer has no stencil bits.");
        // A frame whose nameplates were not drawn keeps none of the last layout.
        if (!m_PlatesPlaced) m_Nameplates.Clear();
        m_PlatesPlaced = false;
        if (!headsup::NameplatesOn(m_Settings)) m_Nameplates.ReleasePlates();
        if (const std::string failure = m_Nameplates.TakeNameFailure(); !failure.empty())
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
        const headsup::MenuStatus status{
            .outlinedMobs     = m_Tracker.OutlinedCount(),
            .meshes           = m_Outline.MeshesLastFrame(),
            .nameplates       = static_cast<uint32_t>(m_Nameplates.LastShown().size()),
            .frameMs          = m_FrameMs,
            .stencilAvailable = m_Outline.StencilAvailable(),
            .drewInScene      = drewInScene,
            .hooked           = m_NameHook.Running(),
            .names            = m_NameHook.Stats(),
            .player           = m_Player,
            .version          = kVersion,
        };
        if (m_Menu.Draw(m_AshitaCore->GetGuiManager(), m_Settings, status))
            SaveSettings();
        if (m_Menu.TakeDebugRequest()) m_DebugPending = true;
        UpdatePointer();
        m_NameHook.NewFrame(NameOwners());
    }

    // The game draws nameplates into its scene image, copies it to the back buffer and ends that scene before Present.
    // Unless they were drawn into the scene, our nameplates go on top here, in the same frame as the game's names.
    void Direct3DBeginScene(bool isRenderingBackBuffer) override
    {
        if (!Ours()) RecordDraw(isRenderingBackBuffer ? 'B' : 'b', D3DPT_POINTLIST, 0u, nullptr, 0u, 0u, 0u);
    }

    void Direct3DEndScene(bool isRenderingBackBuffer) override
    {
        if (!Ours()) RecordDraw(isRenderingBackBuffer ? 'E' : 'e', D3DPT_POINTLIST, 0u, nullptr, 0u, 0u, 0u);
        if (Ours() || !isRenderingBackBuffer || m_PlatesPlaced) return;
        m_Names.UseScene(m_NameHook.SceneTarget(), m_NameHook.SceneDepth());
        if (!m_Names.NamesThisFrame()) return;
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
#ifdef HEADSUP_DEV
        headsup::CaptureStack(owner->index);
#endif
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
        m_Names.OnDrawUP(type, primCount, vertices, stride, m_Tracker, m_Settings);
        const bool blocked = BlocksGameCursor(type, primCount, vertices, stride);
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

    // Only developer builds (dev/) record draws, for /hu drawdump.
    void RecordDraw([[maybe_unused]] char hook, [[maybe_unused]] D3DPRIMITIVETYPE type, [[maybe_unused]] UINT count,
        [[maybe_unused]] const void* vertices, [[maybe_unused]] UINT stride, [[maybe_unused]] UINT firstVertex,
        [[maybe_unused]] UINT vertexCount)
    {
#ifdef HEADSUP_DEV
        m_DrawDump.Record(m_Device, m_Tracker, hook, type, count, vertices, stride, firstVertex, vertexCount);
#endif
    }
    void MarkHidden()
    {
#ifdef HEADSUP_DEV
        m_DrawDump.MarkHidden();
#endif
    }

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
        IDirect3DSurface8* target      = nullptr;
        IDirect3DSurface8* depth       = nullptr;
        IDirect3DBaseTexture8* texture = nullptr;
        m_Device->GetRenderTarget(&target);
        m_Device->GetDepthStencilSurface(&depth);
        m_Device->GetTexture(0, &texture);
        const headsup::CopyState before = headsup::ReadCopyState(m_Device);
        m_Drawing = true;
        draw();
        m_Drawing = false;
        m_Device->SetRenderTarget(target, depth);
        m_Device->ApplyStateBlock(saved); // after SetRenderTarget, which resets the viewport
        m_Device->DeleteStateBlock(saved);
        // On Windows the game's scene copy came back without its texture and drew its gray vertex color over the whole
        // screen. Its states are set again every time: a layer under Direct3D 8 can report a binding it no longer has.
        if (const std::string lost = headsup::StatesNotRestored(before, headsup::ReadCopyState(m_Device)); !lost.empty())
            if (!std::exchange(m_RestoreWarned, true))
                Print("Direct3D did not put back the game's " + lost + " after HeadsUp drew; HeadsUp puts them back itself.");
        headsup::RestoreCopyState(m_Device, before, texture);
        if (target != nullptr) target->Release();
        if (depth != nullptr) depth->Release();
        if (texture != nullptr) texture->Release();
    }

    // toX and toY: the bound render target's pixels per back-buffer pixel.
    void DrawNameplates(float toX, float toY, bool depthTest)
    {
        m_Nameplates.Update(m_Tracker, m_Names, m_NameHook, m_Settings, toX, toY, m_CursorTargets, Now(), m_SelfIndex, m_TimerLines);
        m_Nameplates.Draw(depthTest);
        m_PlatesPlaced = true;
    }

    // The game draws its 3D scene, names included, into an off-screen image and then copies that to the back buffer.
    // Drawn into the image just before the copy, at the name's depth, our nameplates are hidden by walls like the
    // game's names, and sit under the game's menus. The copy is the first draw to the back buffer after the names.
    void DrawNameplatesBeforeSceneCopy()
    {
        if (!headsup::NameplatesOn(m_Settings)) return;
        m_Names.UseScene(m_NameHook.SceneTarget(), m_NameHook.SceneDepth());
        if (!m_Names.SceneCopyStarting()) return;
        m_Names.FinishText();
        WithSavedState([&] {
            if (FAILED(m_Device->SetRenderTarget(m_Names.SceneTarget(), m_Names.SceneDepth()))) return;
            DrawNameplates(1.0f / m_Names.TargetScaleX(), 1.0f / m_Names.TargetScaleY(), true);
            m_DrewInScene = true;
        });
    }

    // Whose names the hook may meet next frame. A name is HeadsUp's only while it can still draw it.
    std::vector<headsup::NameOwner> NameOwners() const
    {
        std::vector<headsup::NameOwner> owners;
        const bool replacing = headsup::NameplatesOn(m_Settings) && !m_Nameplates.NamesFailed();
        for (const headsup::ActorPtr actor : m_Tracker.Actors())
            if (const headsup::ActorInfo* info = m_Tracker.Find(actor))
                owners.push_back({actor, info->index, replacing && headsup::ReplacesName(m_Settings, *info)});
        return owners;
    }

    // At Present, a render boundary: the game is not inside its name routine.
    void StartNameHook()
    {
        if (std::exchange(m_NameHookTried, true)) return;
        if (const std::string failure = m_NameHook.Start(); !failure.empty())
            Print("could not hook the game's names: " + failure + ".");
    }

#ifdef HEADSUP_DEV
    void WriteStackDump()
    {
        std::string path;
        std::FILE* out = OpenLog("stackdump", path);
        if (out == nullptr) return;
        const size_t draws = headsup::WriteStackDump(out, m_Tracker, m_AshitaCore);
        std::fclose(out);
        Print("wrote the stack at " + std::to_string(draws) + " character draws to " + path);
    }

    void WriteNameDump()
    {
        std::string path;
        std::FILE* out = OpenLog("namedump", path);
        if (out == nullptr) return;
        const size_t calls = headsup::WriteNameDump(out, m_Tracker, m_NameHook.ModuleBase(), m_NameHook.CallerReturns());
        std::fclose(out);
        Print("wrote " + std::to_string(calls) + " name calls to " + path);
    }

    void WriteDrawDump()
    {
        std::string path;
        std::FILE* out = OpenLog(m_DrawDump.FilePrefix().c_str(), path);
        if (out == nullptr) return;
        const size_t draws = m_DrawDump.Write(out, headsup::DrawDump::Sources{m_AshitaCore, m_Names, m_NameHook, m_Nameplates.CursorNames(),
                                                       m_Nameplates.LastShown(), [&](uint32_t index) { return PlayerStatusOf(index, m_SelfIndex); }});
        std::fclose(out);
        m_DrawDump.Wrote(path, draws);
        if (!m_DrawDump.Watching()) Print("wrote " + std::to_string(draws) + " draw calls to " + path);
    }
#endif

    void OnCheckReply(const uint8_t* data, uint32_t size)
    {
        const auto reply = headsup::ParseCheckReply(data, size);
        if (!reply) return;
        IEntity* entity = m_AshitaCore->GetMemoryManager()->GetEntity();
        const headsup::MobRecord* mob = headsup::FindMob(reply->serverId, EntityName(entity, reply->targetIndex));
        m_Checks.Received(*reply, headsup::CheckLifetime(mob), Now());
    }

    void Print(const std::string& message)
    {
        const std::string line = Ashita::Chat::Header(kName) + Ashita::Chat::Message(message);
        m_AshitaCore->GetChatManager()->Write(kChatMode, false, line.c_str());
    }

    std::string OwnFolder(const char* ashitaFolder)
    {
        const std::string parent = std::string(m_AshitaCore->GetInstallPath()) + ashitaFolder;
        CreateDirectoryA(parent.c_str(), nullptr);
        const std::string folder = parent + "\\" + kFolder;
        CreateDirectoryA(folder.c_str(), nullptr);
        return folder;
    }

    std::FILE* OpenLog(const char* prefix, std::string& path)
    {
        char stamp[32];
        const std::time_t now = std::time(nullptr);
        std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", std::localtime(&now));
        path           = OwnFolder(kLogsFolder) + "\\" + prefix + "-" + stamp + ".txt";
        std::FILE* out = std::fopen(path.c_str(), "w");
        if (out == nullptr) Print("could not write " + path + "; nothing was recorded");
        return out;
    }

    // Ashita's configuration manager takes the settings file's path inside its config folder.
    std::string SettingsPath() const { return std::string(kFolder) + "/" + kSettingsFile; }
    std::string SettingsFile() { return OwnFolder(kConfigFolder) + "\\" + kSettingsFile; }

    void LoadSettings()
    {
        IConfigurationManager* config = m_AshitaCore->GetConfigurationManager();
        const std::string file        = SettingsFile();
        // A missing file is fine: every value keeps its default.
        if (!config->Load(kConfigAlias, SettingsPath().c_str()) && GetFileAttributesA(file.c_str()) != INVALID_FILE_ATTRIBUTES)
            Print("could not read " + file + "; using the defaults, and a change in the menu will overwrite that file");
        AshitaStore store(config);
        m_Settings = headsup::LoadSettings(store);
    }

    void SaveSettings()
    {
        IConfigurationManager* config = m_AshitaCore->GetConfigurationManager();
        AshitaStore store(config);
        headsup::SaveSettings(m_Settings, store);
        if (!config->Save(kConfigAlias, SettingsPath().c_str()))
            Print("could not save " + SettingsFile() + "; the change lasts until Ashita closes");
    }

    double Now() const
    {
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        return m_QpcFrequency.QuadPart != 0 ? static_cast<double>(counter.QuadPart) / static_cast<double>(m_QpcFrequency.QuadPart) : 0.0;
    }

    void MeasureFrame()
    {
        const double now = Now();
        if (m_LastPresent > 0.0)
        {
            const double ms = (now - m_LastPresent) * 1000.0;
            m_FrameMs       = m_FrameMs == 0.0 ? ms : m_FrameMs + (ms - m_FrameMs) * kFrameTimeWeight;
        }
        m_LastPresent = now;
    }

    // A player's status from the game's memory, known at once, with what only the packets carry once seen.
    std::optional<headsup::PlayerStatus> CurrentStatus(IEntity* entity, uint16_t index, headsup::EntityKind kind)
    {
        if (kind != headsup::EntityKind::Player) return std::nullopt;
        const headsup::PlayerStatus memory = headsup::StatusFromRender(entity->GetRenderFlags1(index), entity->GetRenderFlags2(index),
            entity->GetLinkshellColor(index));
        headsup::PlayerStatus status = headsup::WithPacketStatus(memory, PlayerStatusOf(index, m_SelfIndex));
        status.levelSync             = m_LevelSynced.count(index) != 0 || m_BuffSynced.count(index) != 0;
        return status;
    }

    const headsup::PlayerStatus* PlayerStatusOf(uint32_t index, uint32_t self) const
    {
        if (index == self) return m_OwnStatus ? &*m_OwnStatus : nullptr;
        const auto it = m_PlayerStatus.find(static_cast<uint16_t>(index));
        return it != m_PlayerStatus.end() ? &it->second : nullptr;
    }

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
        if (const Ashita::FFXI::targetwindow_t* window = target->GetRawStructureWindow())
            headsup::PlaceAnchors(m_CursorTargets,
                headsup::CursorWindow{static_cast<float>(window->m_AnkX), static_cast<float>(window->m_AnkY),
                    static_cast<float>(window->m_SubAnkX), static_cast<float>(window->m_SubAnkY)},
                m_MenuWidth, m_MenuHeight, m_Names.BackBufferWidth(), m_Names.BackBufferHeight());
    }

    void UpdateParty(IParty* party, IPlayer* player)
    {
        m_PartyIds.clear();
        for (uint32_t member = 0; member < kPartyMembers; ++member)
            if (party->GetMemberIsActive(member) != 0) m_PartyIds.push_back(party->GetMemberServerId(member));
        m_BuffSynced.clear();
        if (headsup::LevelSyncInBuffs(player->GetBuffs())) m_BuffSynced.insert(m_SelfIndex);
        for (uint32_t member = 0; member < kPartyIconMembers; ++member)
            if (party->GetStatusIconsServerId(member) != 0 &&
                headsup::LevelSyncInPartyIcons(party->GetStatusIcons(member), party->GetStatusIconsBitMask(member)))
                m_BuffSynced.insert(static_cast<uint16_t>(party->GetStatusIconsTargetIndex(member)));
    }

    void UpdateTracker(double now)
    {
        IEntity* entity = m_AshitaCore->GetMemoryManager()->GetEntity();
        IParty* party   = m_AshitaCore->GetMemoryManager()->GetParty();
        IPlayer* player = m_AshitaCore->GetMemoryManager()->GetPlayer();
        m_SelfIndex     = static_cast<uint16_t>(party->GetMemberTargetIndex(0));
        UpdateParty(party, player);
        const uint32_t count = std::min<uint32_t>(entity->GetEntityMapSize(), headsup::kMaxEntities);
        std::vector<headsup::ActorInput> inputs;
        std::vector<headsup::PhSighting> sightings;
        for (uint32_t i = 0; i < count; ++i)
        {
            if (entity->GetRawEntity(i) == nullptr) continue;
            const uintptr_t actor = entity->GetActorPointer(i);
            if (actor == 0) continue;
            const auto index        = static_cast<uint16_t>(i);
            const uint32_t serverId = entity->GetServerId(i);
            const auto kind         = headsup::KindFromSpawnFlags(entity->GetSpawnFlags(i));
            const bool isMob        = kind == headsup::EntityKind::Mob;
            const bool alive        = entity->GetHPPercent(i) > 0;
            const char* name        = EntityName(entity, i);
            const uint32_t claim    = entity->GetClaimStatus(i);
            if (isMob && !alive) m_Checks.Forget(serverId); // the next spawn rolls a new level
            if (isMob && m_Settings.phTimers)
            {
                const bool bodyDrawn = m_Names.FramesSinceMesh(index) <= headsup::kMeshGraceFrames;
                if (const auto sighting = headsup::SightingOf(headsup::FindMob(serverId, name), alive, bodyDrawn))
                    sightings.push_back(*sighting);
            }
            inputs.push_back(headsup::ActorInput{
                .actor    = static_cast<headsup::ActorPtr>(actor),
                .index    = index,
                .serverId = serverId,
                .kind     = kind,
                .alive    = alive,
                .distance = headsup::DistanceFromSquared(entity->GetDistance(i)),
                .name     = name,
                .checked  = isMob ? m_Checks.Result(serverId, now) : nullptr,
                .status   = CurrentStatus(entity, index, kind),
                .pose     = kind == headsup::EntityKind::Player ? headsup::PoseFromStatus(entity->GetStatus(i)) : headsup::Pose::Standing,
                .feet = headsup::FromEntityPosition(entity->GetLocalPositionX(i), entity->GetLocalPositionY(i), entity->GetLocalPositionZ(i)),
                .claimedByParty = isMob && headsup::ClaimedByParty(claim, m_PartyIds),
                .claimed        = isMob && headsup::IsClaimed(claim),
            });
        }
        const uint32_t ownStatus = entity->GetStatus(m_SelfIndex);
        m_Player.level           = player->GetMainJobLevel();
        m_Player.sitting         = headsup::IsSittingStatus(ownStatus);
        m_Player.engaged         = ownStatus == kEngagedStatus;
        m_Tracker.Update(inputs, m_Player, m_Settings);
        // With timers off no sightings are passed, so turning them on never takes a placeholder that died meanwhile for
        // one seen dying.
        m_PhTimers.Update(now, sightings);
        m_TimerLines.clear();
        if (m_Settings.phTimers) m_TimerLines = m_PhTimers.Lines(party->GetMemberZone(0), now);
    }

    void WriteDebugReport()
    {
        std::string path;
        std::FILE* out = OpenLog("debug", path);
        if (out == nullptr) return;

        IEntity* entity = m_AshitaCore->GetMemoryManager()->GetEntity();
        auto onOff      = [](bool on) { return on ? "on" : "off"; };
        std::fprintf(out,
            "headsup %.2f: back buffer %.0fx%.0f, player level %d%s, %u outlined, %u nameplates, replace mobs %s players %s npcs %s\n",
            GetVersion(), m_Names.BackBufferWidth(), m_Names.BackBufferHeight(), m_Player.level,
            m_Player.sitting ? " (sitting)" : "", m_Tracker.OutlinedCount(), static_cast<unsigned>(m_Nameplates.LastShown().size()),
            onOff(m_Settings.replaceMobNames), onOff(m_Settings.replacePlayerNames), onOff(m_Settings.replaceNpcNames));
        for (const headsup::ActorPtr actor : m_Tracker.Actors())
        {
            const headsup::ActorInfo* info = m_Tracker.Find(actor);
            if (info == nullptr || (!info->outline && m_NameHook.Drawn(info->index) == nullptr)) continue;
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
                static_cast<unsigned>(info->argb), info->label.text, static_cast<unsigned>(headsup::ToArgb(m_Settings.labelColor[static_cast<int>(info->label.shade)])), info->mobIcons.count);
            if (const headsup::CheckResult* checked = m_Checks.Result(serverId, Now()))
                std::fprintf(out, " | checked Lv %d %s", checked->level, headsup::Abbrev(checked->con));
            if (const headsup::ScreenBox* plate = GameNameBox(info->index))
                std::fprintf(out, " | game name (%.1f,%.1f)-(%.1f,%.1f)", plate->minX, plate->minY, plate->maxX, plate->maxY);
            else
                std::fprintf(out, " | no game name");
            for (const auto& shown : m_Nameplates.LastShown())
                if (shown.index == info->index)
                    std::fprintf(out, " | name %08X %dpx at (%.1f,%.1f), label %dpx at (%.1f,%.1f), %d icons %dpx at (%.1f,%.1f)",
                        static_cast<unsigned>(shown.nameColor), shown.nameHeight, shown.nameX, shown.nameY, shown.labelHeight,
                        shown.labelX, shown.labelY, shown.mobIconCount, shown.iconSize, shown.iconsX, shown.iconsY);
            std::fprintf(out, "\n");
        }
        std::fclose(out);
        Print("wrote " + path + "; recording " + std::to_string(kCaptureFrames) + " frames");
        m_CapturePath = path;
        m_Capture     = "\nframe  dt(ms)  game's names: hooked replaced | nameplates shown, S if drawn into the scene, s<scene to screen scale> | per entity "
                    "with a game name: index m<body meshes> c<name color> (name box: center, top-bottom)\n";
        m_CaptureLeft = kCaptureFrames;
        m_CaptureLast = Now();
    }

    void CaptureFrame()
    {
        if (m_CaptureLeft <= 0) return;
        const double now               = Now();
        const headsup::NameStats stats = m_NameHook.Stats();
        char line[200];
        std::snprintf(line, sizeof(line), "f%03d %6.1f  %3u %3u | %2u %c s%.3f |", kCaptureFrames - m_CaptureLeft,
            (now - m_CaptureLast) * 1000.0, stats.names, stats.replaced, static_cast<unsigned>(m_Nameplates.LastShown().size()),
            m_DrewInScene ? 'S' : '-', m_Names.TargetScaleY());
        m_Capture += line;
        int listed = 0;
        for (const headsup::ActorPtr actor : m_Tracker.Actors())
        {
            const headsup::ActorInfo* info = m_Tracker.Find(actor);
            const headsup::NameFrame* frame = info != nullptr ? m_NameHook.Drawn(info->index) : nullptr;
            if (frame == nullptr || ++listed > kCapturedPlatesPerFrame) continue;
            const headsup::DrawnName name = headsup::NameFromFrame(*frame, m_Names.TargetScaleX(), m_Names.TargetScaleY());
            std::snprintf(line, sizeof(line), " %u m%u c%06X (%.0f,%.0f-%.0f)", info->index, m_Names.MeshDraws(info->index),
                static_cast<unsigned>(name.color & 0xFFFFFF), name.box.CenterX(), name.box.minY, name.box.maxY);
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

    // The box of the name the game drew for this entity this frame, in back-buffer pixels.
    const headsup::ScreenBox* GameNameBox(uint16_t index)
    {
        const headsup::NameFrame* frame = m_NameHook.Drawn(index);
        if (frame == nullptr) return nullptr;
        m_GameNameBox = headsup::NameFromFrame(*frame, m_Names.TargetScaleX(), m_Names.TargetScaleY()).box;
        return &m_GameNameBox;
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
