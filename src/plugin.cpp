#include "Ashita.h"

#include "bar_connection.h"
#include "bar_fps.h"
#include "bar_job.h"
#include "bar_render.h"
#include "check.h"
#include "commands.h"
#include "d3d_util.h"
#include "fonts.h"
#include "game_cursor.h"
#include "game_names.h"
#include "game_state.h"
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
#include "pointerwatch.h"
#include "stackdump.h"
#include "stripdump.h"
#endif

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <unordered_map>
#include <cstdio>
#include <ctime>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
    constexpr const char* kName              = "headsup";
    // Ashita takes a plugin's version as one number, so it holds the release tag's major.minor; the release workflow
    // checks the two agree.
    constexpr double kVersion                = 0.6;
    constexpr float kMenuFontPixels          = 18.0f; // the size ImGui first rasterizes the menu's fonts at
    constexpr const char* kConfigAlias       = "headsup";
    constexpr const char* kConfigFolder      = "config";
    constexpr const char* kLogsFolder        = "logs";
    constexpr const char* kFolder            = "headsup"; // inside Ashita's config and logs folders
    constexpr const char* kSettingsFile      = "settings.ini";
    constexpr const char* kSection           = "settings";
    constexpr uint32_t kLockedOn             = 0x01; // ITarget::GetLockedOnFlags
    constexpr int kCaptureFrames             = 120;
    constexpr int kCapturedPlatesPerFrame    = 16;
    constexpr double kFrameTimeWeight        = 0.05;
    constexpr int32_t kChatMode              = 1; // the chat mode Ashita's own plugins print in
    // The game's menu resolution, which its target window's arrow anchors are in, from its registry settings.
    constexpr const char* kBootConfig        = "boot";
    constexpr const char* kRegistry          = "ffxi.registry";
    constexpr const char* kMenuWidth         = "0037";
    constexpr float kWindowClearance         = 4.0f; // back buffer pixels between the help bar and a window it moved
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

    // Set at Initialize and Present, and read by OnMouse, which Ashita calls on the game's thread before the game sees
    // each message.
    bool g_SyncClicks               = false;
    HWND g_GameWindow               = nullptr;
    IGuiManager* g_Gui              = nullptr;
    headsup::BarClicks* g_BarClicks = nullptr;
    bool g_BarOn                    = false;
    float g_BarScaleX               = 1.0f; // back-buffer pixels per client pixel
    float g_BarScaleY               = 1.0f;
    IResourceManager* g_Resources   = nullptr; // for the target popup's drops, from Initialize to Release

    // The device Ashita gives plugins is Direct3D's own, under Ashita's wrapper: its Present runs after Ashita has
    // drawn ImGui, every addon's windows included, so what HeadsUp draws there is over all of them.
    using PresentFn = HRESULT(__attribute__((stdcall))*)(IDirect3DDevice8*, const RECT*, const RECT*, HWND,
        const RGNDATA*);
    constexpr size_t kPresentSlot = 15; // IDirect3DDevice8's vtable: Present follows Reset
    void** g_PresentSlot          = nullptr;
    PresentFn g_RealPresent       = nullptr;
    void (*g_AfterGui)()          = nullptr; // the plugin's draw over ImGui, while it is loaded

    HRESULT __attribute__((stdcall)) HookedPresent(IDirect3DDevice8* device, const RECT* source, const RECT* dest,
        HWND window, const RGNDATA* dirty)
    {
        if (g_AfterGui != nullptr) g_AfterGui();
        return g_RealPresent(device, source, dest, window, dirty);
    }

    // Writes a vtable slot, which lives in read-only memory.
    bool WriteSlot(void** slot, void* value)
    {
        DWORD was = 0;
        if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &was)) return false;
        *slot = value;
        VirtualProtect(slot, sizeof(void*), was, &was);
        return true;
    }

    const char* ItemName(uint16_t item)
    {
        const IItem* found = g_Resources != nullptr ? g_Resources->GetItemById(item) : nullptr;
        return found != nullptr ? found->Name[0] : nullptr;
    }

    BOOL __stdcall OnMouse(uint32_t message, WPARAM wParam, LPARAM lParam, bool blocked)
    {
        // An Ashita window takes the mouse. Asked only when read: the bar ignores it while off or for blocked messages.
        const bool overGui =
            ((g_BarOn && !blocked) || g_SyncClicks) && g_Gui != nullptr && g_Gui->GetIO().WantCaptureMouse;
        if (g_BarClicks != nullptr)
        {
            const float x = static_cast<float>(static_cast<int16_t>(LOWORD(lParam))) * g_BarScaleX;
            const float y = static_cast<float>(static_cast<int16_t>(HIWORD(lParam))) * g_BarScaleY;
            if (g_BarClicks->Take(message, static_cast<uint32_t>(wParam), x, y, g_BarOn, overGui, blocked)) return TRUE;
        }
        if (!g_SyncClicks) return FALSE;
        // The game takes a click where its pointer last was, which is somewhere else after the mouse moved over another
        // window: a mouse move to the click's place goes first.
        if (!blocked && !overGui && g_GameWindow != nullptr && headsup::ClickMessage(message))
            SendMessageA(g_GameWindow, WM_MOUSEMOVE, LOWORD(wParam), lParam);
        // The game hides its pointer for typing until it sees the mouse move, which it never does over an Ashita window.
        if (message == WM_MOUSEMOVE && overGui) headsup::PointerFix::RevealUnderWindow();
        return FALSE;
    }
}

class HeadsUp;
namespace
{
    HeadsUp* g_Plugin = nullptr; // the loaded plugin, for the Present hook to draw with
}

class HeadsUp final : public IPlugin
{
    IAshitaCore* m_AshitaCore = nullptr;
    headsup::Settings m_Settings;
    headsup::Tracker m_Tracker;
    headsup::PhTimers m_PhTimers;
    std::vector<headsup::TimerLine> m_TimerLines;
    uint16_t m_SelfIndex = 0;
    uint16_t m_Zone      = 0; // this frame
    headsup::OutlineRenderer m_Outline;
    headsup::GameNames m_Names;
    headsup::NameplateRenderer m_Nameplates;
    headsup::BarRenderer m_Bar;
    headsup::BarInput m_BarInput;
    headsup::BarClicks m_BarClicks{m_BarInput};
    bool m_BarDrawn = false; // this frame
    headsup::PointerSwap m_Pointer;
    std::vector<HANDLE> m_LoadedFonts;
    headsup::NameHook m_NameHook;
    headsup::ArrowHook m_ArrowHook;
    headsup::WindowHook m_WindowHook;
    bool m_WindowHookTried = false; // it patches the game's code, so it starts only once someone wants it
    headsup::PointerFix m_PointerFix;
    headsup::StripTextReader m_StripText;
    std::string m_StripTextFound = "not looked for"; // Find's result, said by the dev dump
    headsup::StripObject m_HelpBytes{}, m_TitleBytes{}; // this frame's
    headsup::StripObject m_LastHelp{}, m_LastTitle{};   // the bytes m_Strip was converted from
    bool m_StripConverted = false;
    headsup::StripText m_Strip;
    std::string m_MenuFontApplied;                       // the menu font the menu has, "" before it first opens
    std::unordered_map<std::string, ImFont*> m_MenuFonts; // each font loaded into ImGui, by name; null when it failed
    headsup::WeatherReader m_WeatherReader;
    bool m_WeatherTried = false; // it scans the client's code, so once
    headsup::NetStatsReader m_NetStats;
    bool m_NetStatsTried = false; // it scans the client's code, so once
    std::optional<headsup::NetCounters> m_Counters; // the last read, kept between samples
    std::optional<double> m_CountersReadAt;
    headsup::ConnectionMeter m_Connection;
    bool m_HooksTried = false;
    headsup::ScreenBox m_GameNameBox;
    bool m_PointerFailed = false; // said once; tried again when the setting changes
    headsup::Menu m_Menu;
    headsup::PlayerState m_Player;
    headsup::CheckResults m_Checks;
    headsup::GameState m_GameState;
    std::unordered_set<uint16_t> m_BuffSynced; // you and your party, from the buffs the game keeps, this frame
    std::vector<uint32_t> m_PartyIds;           // server IDs of you, your party and alliance, this frame
    bool m_DebugPending = false;
    bool m_PlatesPlaced = false; // this frame, in the scene or at the back-buffer EndScene

#ifdef HEADSUP_DEV
    headsup::DrawDump m_DrawDump;
    headsup::PointerWatch m_PointerWatch;
#endif
    bool m_Drawing      = false; // drawing nameplates: our own draws come back through the hooks
    bool m_DrewInScene  = false; // this frame
    // Said once each: Direct3D could not save the game's render states, so nameplates or the bar were not drawn.
    bool m_PlatesUnsaved = false;
    bool m_BarUnsaved    = false;
    bool m_RestoreWarned = false; // said once that Direct3D did not put the game's states back
    std::optional<DWORD> m_QuadStates; // the bar's (RecordQuadStates), freed at unload
    bool m_QuadStatesTried = false;
    IDirect3DDevice8* m_Device = nullptr;
    headsup::CursorTargets m_CursorTargets;
    float m_MenuWidth   = 0.0f;
    // The windows the hook moves, as last given it (none until the first frame), and those it has seen.
    std::optional<headsup::WindowMoves> m_WindowMoves;
    std::optional<std::string> m_WindowLog;   // /hu windowlog's file, while it runs
    std::vector<std::string> m_LastOpen;      // the open windows it wrote last // what the window hook has: the settings' windows and groups
    std::vector<std::string> m_SeenWindows;
    float m_MenuHeight  = 0.0f;
    int m_CaptureLeft   = 0;
    std::string m_CapturePath;
    std::string m_Capture;
    double m_CaptureLast = 0.0;
    LARGE_INTEGER m_QpcFrequency{};
    double m_LastPresent = 0.0;
    double m_FrameMs     = 0.0;
    headsup::FrameStats m_FrameStats;
    headsup::ExpTracker m_Exp;

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
        g_Plugin     = this;
        g_Resources  = core->GetResourceManager();
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
        g_Gui       = core->GetGuiManager();
        g_BarClicks = &m_BarClicks;
        core->GetInputManager()->GetMouse()->AddCallback(kName, OnMouse);
        return true;
    }

    void Release(void) override
    {
        m_AshitaCore->GetInputManager()->GetMouse()->RemoveCallback(kName);
        g_SyncClicks = false;
        g_Gui        = nullptr;
        g_Resources  = nullptr;
        if (const std::string failure = m_NameHook.Stop(); !failure.empty()) Print(failure + ".");
        if (const std::string failure = m_ArrowHook.Stop(); !failure.empty()) Print(failure + ".");
        if (const std::string failure = m_WindowHook.Stop(); !failure.empty()) Print(failure + ".");
        if (const std::string failure = m_PointerFix.Stop(); !failure.empty()) Print(failure + ".");
        m_Pointer.Stop();
        m_Nameplates.Release();
        UnhookPresent();
        g_Plugin = nullptr;
        m_Bar.Release();
        // After the renderers, which hold fonts made from these until they release.
        for (HANDLE handle : m_LoadedFonts)
            RemoveFontMemResourceEx(handle);
        m_LoadedFonts.clear();
        if (m_QuadStates) m_Device->DeleteStateBlock(*m_QuadStates);
        g_BarClicks = nullptr;
        g_BarOn     = false;
    }

    bool Direct3DInitialize(IDirect3DDevice8* device) override
    {
        m_Device = device;
        HookPresent(device);
        m_Outline.SetDevice(device);
        m_Names.SetDevice(device);
        m_Nameplates.SetDevice(device);
        m_Bar.SetDevice(device);
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
            Print(std::string("HeadsUp ") + (m_Settings.enabled ? "on" : "off"));
        }
        else if (args[1] == "debug")
            m_DebugPending = true;
        else if (args[1] == "popup" && args.size() > 2)
        {
            // Pins a widget's popup by the name the settings file gives it, in lower case, or none.
            std::optional<headsup::BarWidget> pinned;
            for (int w = 0; w < headsup::kBarWidgetCount; ++w)
            {
                std::string name = headsup::BarWidgetName(static_cast<headsup::BarWidget>(w));
                for (char& c : name)
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (name == args[2]) pinned = static_cast<headsup::BarWidget>(w);
            }
            if (!pinned && args[2] != "off")
                Print("/hu popup: no widget is named " + args[2] + ".");
            else
                m_BarInput.Pin(pinned);
        }
        else if (args[1] == "windowlog")
        {
            // Every change in which game windows are open, so windows that open together can be told apart.
            if (args.size() > 2 && args[2] == "stop")
            {
                m_WindowLog.reset();
                Print("windowlog stopped.");
            }
            else
            {
                m_WindowLog = OwnFolder(kLogsFolder) + "\\windows.log";
                m_LastOpen.clear();
                Print("windowlog: writing which game windows are open to " + *m_WindowLog + "; /hu windowlog stop ends it.");
            }
        }
        else if (args[1] == "menu" && args.size() > 2)
        {
            if (!m_Menu.OpenPage(args[2], args.size() > 3 && args[3] == "colors"))
                Print("/hu menu: no page is named " + args[2] + ".");
#ifdef HEADSUP_DEV
            // A number last scrolls the page that many pixels down.
            char* end          = nullptr;
            const float pixels = std::strtof(args.back().c_str(), &end);
            if (args.size() > 3 && end != args.back().c_str() && *end == '\0') m_Menu.ScrollTo(pixels);
#endif
        }
#ifdef HEADSUP_DEV
        else if (args[1] == "drawdump")
            Print(m_DrawDump.Start(args.size() > 2 ? args[2] : "", Now(), m_AshitaCore, OwnFolder(kLogsFolder)));
        else if (args[1] == "codedump")
            Print(headsup::WriteCodeDump(OwnFolder(kLogsFolder)));
        else if (args[1] == "namedump")
            Print(headsup::ArmNameDump(m_NameHook.Running()));
        else if (args[1] == "stackdump")
            Print(headsup::ArmStackDump());
        else if (args[1] == "pointerwatch")
            Print(args.size() > 2 && args[2] == "stop" ? m_PointerWatch.Stop() : m_PointerWatch.Start(OwnFolder(kLogsFolder), Now()));
        else if (args[1] == "stripdump")
        {
            FindStripText();
            Print(headsup::WriteStripDump(m_StripText, m_StripTextFound, OwnFolder(kLogsFolder)));
        }
#endif
        else
        {
            Print("/headsup or /hu: open or close the settings window");
            Print("/hu on | /hu off: turn all of HeadsUp (outlines, nameplates, the top bar and the rest) on or off");
            Print("/hu debug: write what every outline and nameplate show to logs/headsup");
            Print("/hu menu <page> [colors]: open the settings at a page, such as topbar or mobs");
            Print("/hu popup <widget> | off: pin a widget's popup open, such as zone or vanatime");
            Print("/hu windowlog [stop]: write which game windows are open, as it changes, to logs/headsup/windows.log");
#ifdef HEADSUP_DEV
            Print("/hu drawdump [seconds | watch | stop]: write every draw call of one frame to logs/headsup after the delay, or watch every frame for name glitches until stopped");
            Print("/hu codedump: write the client's loaded image to logs/headsup");
            Print("/hu namedump: write every call into the game's name routine during one frame to logs/headsup");
            Print("/hu stackdump: write the stack at the character draws of one frame to logs/headsup");
            Print("/hu pointerwatch [stop]: log every change in the mouse pointer and the game's own to logs/headsup until stopped");
            Print("/hu stripdump: write the game's top-strip text, every level, to logs/headsup");
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
        UNREFERENCED_PARAMETER(blocked);
#ifdef HEADSUP_DEV
        m_DrawDump.SawPacket(id, data, size, Now());
#endif
        if (!injected) m_Connection.Received(Now()); // an addon's injected packet never came from the server
        const headsup::PacketFollowUp next = m_GameState.Received(id, data, size);
        if (next.zonedIn) m_Names.ForgetCamera();
        if (next.check) OnCheckReply(*next.check);
        return false;
    }

    // HeadsUp never changes or blocks the game's packets: every one still goes to the server.
    bool HandleOutgoingPacket(uint16_t id, uint32_t size, const uint8_t* data, uint8_t* modified, uint32_t sizeChunk,
        const uint8_t* dataChunk, bool injected, bool blocked) override
    {
        UNREFERENCED_PARAMETER(id);
        UNREFERENCED_PARAMETER(size);
        UNREFERENCED_PARAMETER(data);
        UNREFERENCED_PARAMETER(modified);
        UNREFERENCED_PARAMETER(sizeChunk);
        UNREFERENCED_PARAMETER(dataChunk);
        UNREFERENCED_PARAMETER(injected);
        if (!blocked) m_Connection.Sent(Now());
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
        m_PointerWatch.Frame(m_AshitaCore, Now());
#endif
        StartHooks();
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
        g_GameWindow = m_AshitaCore->GetProperties()->GetFinalFantasyHwnd();
        const uint8_t loginStatus = m_AshitaCore->GetMemoryManager()->GetPlayer()->GetLoginStatus();
        // A popup left open at logout would come back with the next character.
        if (loginStatus != headsup::kLoginInGame) m_BarInput.Reset();
        g_BarOn = headsup::BarRuns(m_Settings, loginStatus) && !m_Bar.Failed();
        // Opening Region Info asks the server for the conquest standings; HeadsUp asks the same once per zone-in.
        const bool wantsConquest = m_Settings.barWidgets[static_cast<int>(headsup::BarWidget::Zone)] ||
                                   m_Settings.barWidgets[static_cast<int>(headsup::BarWidget::Conquest)];
        if (m_GameState.ConquestRequestDue(now, g_BarOn && wantsConquest &&
                                                    m_AshitaCore->GetMemoryManager()->GetPlayer()->GetIsZoning() == 0))
        {
            auto request = headsup::kConquestRequest; // Ashita takes the bytes as non-const
            m_AshitaCore->GetPacketManager()->AddOutgoingPacket(headsup::kConquestRequestPacket,
                static_cast<uint32_t>(request.size()), request.data());
            m_GameState.ConquestRequested(now);
        }
        // Opening the Currencies menu asks for them; HeadsUp asks the same, as for the conquest standings.
        const bool gilOpen = m_BarInput.Open() == headsup::BarWidget::Gil;
        if (m_GameState.CurrencyRequestDue(now,
                g_BarOn && m_Settings.barWidgets[static_cast<int>(headsup::BarWidget::Gil)] &&
                    m_AshitaCore->GetMemoryManager()->GetPlayer()->GetIsZoning() == 0,
                gilOpen))
        {
            auto first = headsup::kCurrencies1RequestBytes, second = headsup::kCurrencies2RequestBytes;
            m_AshitaCore->GetPacketManager()->AddOutgoingPacket(headsup::kCurrencies1Request,
                static_cast<uint32_t>(first.size()), first.data());
            m_AshitaCore->GetPacketManager()->AddOutgoingPacket(headsup::kCurrencies2Request,
                static_cast<uint32_t>(second.size()), second.data());
            m_GameState.CurrencyRequested(now);
        }
        if (RECT client{}; g_BarOn && g_GameWindow != nullptr && GetClientRect(g_GameWindow, &client) &&
                           client.right > 0 && client.bottom > 0)
        {
            g_BarScaleX = m_Names.BackBufferWidth() / static_cast<float>(client.right);
            g_BarScaleY = m_Names.BackBufferHeight() / static_cast<float>(client.bottom);
            // No message says the pointer left the client area, and in windowed modes the game never moves Windows'.
            if (POINT pointer{}; GetCursorPos(&pointer) && ScreenToClient(g_GameWindow, &pointer) &&
                                 !PtInRect(&client, pointer))
                m_BarInput.Leave();
        }
        // Off, the bar is given nothing to show, and releases what it made.
        m_Bar.Update(g_BarOn ? BarStateNow(now) : headsup::BarState{}, m_Settings, m_Names.BackBufferWidth(),
            m_Names.BackBufferHeight(), m_BarInput);
        if (const std::string failure = m_Bar.TakeFailure(); !failure.empty())
            Print("the top bar is off until HeadsUp reloads: " + failure + ".");
        // Over ImGui from Present; through ImGui, under addons that draw over its windows, when Present is not hooked.
        if (g_PresentSlot != nullptr)
            g_AfterGui = [] { if (g_Plugin != nullptr) g_Plugin->DrawOverGui(); };
        else if (IGuiManager* gui = m_AshitaCore->GetGuiManager(); gui != nullptr && m_Names.BackBufferWidth() > 0.0f)
            m_Bar.DrawPopup(gui, gui->GetIO().DisplaySize.x / m_Names.BackBufferWidth());
        m_BarDrawn = false;
        m_ArrowHook.Hide(m_Settings.enabled && m_Settings.replaceCursor);
        const headsup::HiddenWindows hidden{.helpBar = m_Settings.enabled && m_Settings.hideHelpBar,
            .netstat = m_Settings.enabled && m_Settings.hideNetstat};
        const std::optional<headsup::HelpBand> band = HelpBandNow();
        const bool movesWindows = (band && band->bottom != 0) || !m_Settings.windowOffsets.empty() ||
                                  !m_Settings.windowGroups.empty() || m_Settings.allUnderBar || m_Settings.moveGameHelp;
        if ((hidden.Any() || movesWindows) && !std::exchange(m_WindowHookTried, true))
        {
            if (const std::string failure = m_WindowHook.Start(); !failure.empty())
                Print("could not hide or move the game's windows: " + failure + ".");
            if (const std::string failure = m_WindowHook.TakeMoveProblem(); !failure.empty())
                Print("the game's windows stay under the help bar: " + failure + ".");
        }
        m_WindowHook.Hide(hidden);
        m_WindowHook.NewFrame();
        m_WindowHook.MoveFrom(band);
        if (headsup::WindowMoves moves =
                headsup::CombinedMoves(m_Settings.movedWindows, m_Settings.windowOffsets, m_Settings.windowGroups);
            moves != m_WindowMoves)
        {
            m_WindowHook.Move(moves.underBar);
            m_WindowHook.Offset(moves.offsets);
            m_WindowMoves = std::move(moves);
        }
        if (m_WindowHook.SeenCount() != m_SeenWindows.size()) m_SeenWindows = m_WindowHook.SeenWindows();
        if (std::vector<std::string> open = m_WindowHook.TakeOpen(); m_WindowLog && open != m_LastOpen)
        {
            if (FILE* log = std::fopen(m_WindowLog->c_str(), "a"))
            {
                std::fprintf(log, "%.1f", now);
                for (const std::string& name : open)
                    std::fprintf(log, " %s", name.c_str());
                std::fputc('\n', log);
                std::fclose(log);
            }
            m_LastOpen = std::move(open);
        }
        m_PointerFix.Enable(m_Settings.enabled && m_Settings.fixPointer);
        m_PointerFix.KeepPointerFor(m_Settings.enabled ? m_Settings.keepPointerKeys : std::vector<uint8_t>{});
        g_SyncClicks = m_Settings.enabled && m_Settings.fixPointer;
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
            .barTitlePixels   = m_Bar.TitlePixels(),
            .seenWindows      = &m_SeenWindows,
        };
        if (m_Menu.open) ApplyMenuFont();
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
        if (Ours() || !isRenderingBackBuffer) return;
        bool plates = false;
        if (!m_PlatesPlaced)
        {
            m_Names.UseScene(m_NameHook.SceneTarget(), m_NameHook.SceneDepth());
            plates = m_Names.NamesThisFrame();
        }
        const bool bar = g_BarOn && !std::exchange(m_BarDrawn, true);
        if (plates)
        {
            m_Names.FinishText();
            const bool saved = WithSavedState([&] {
                DrawNameplates(1.0f, 1.0f, false);
                if (bar) m_Bar.Draw();
            });
            if (!saved) PlatesNotDrawn();
            if (!saved && bar) BarNotDrawn();
        }
        else if (bar && !WithQuadStates([&] { m_Bar.Draw(); }))
            BarNotDrawn();
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
        m_Names.OnDrawUP(m_Tracker, m_Settings);
        return false;
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
    // The help bar's tallest reach in the game's menu resolution, with clear space below it, while the bar shows it.
    std::optional<headsup::HelpBand> HelpBandNow() const
    {
        // The screen in the game's menu resolution, and the help bar's bottom in it while the bar shows it.
        const std::optional<headsup::BarRect> reach = g_BarOn ? m_Bar.HelpBarReach() : std::nullopt;
        const float height = m_Names.BackBufferHeight();
        if (m_MenuHeight <= 0.0f || m_MenuWidth <= 0.0f || height <= 0.0f) return std::nullopt;
        const float down = m_MenuHeight / height;
        // In menu units: the game's menu resolution over the back buffer's. Without the help bar, windows go under
        // the top bar itself.
        const float barBottom = g_BarOn ? m_Bar.BarBottomNow() : 0.0f;
        const auto menuUnits  = [&](float pixels) {
            return pixels > 0.0f ? static_cast<int>(std::ceil((pixels + kWindowClearance) * down)) : 0;
        };
        return headsup::HelpBand{
            .bottom       = menuUnits(reach ? reach->y + reach->height : barBottom),
            .screenRight  = static_cast<int>(m_MenuWidth),
            .screenBottom = static_cast<int>(m_MenuHeight),
            .topBar       = menuUnits(barBottom),
            .allUnderBar  = m_Settings.allUnderBar,
            .moveGameHelp = m_Settings.moveGameHelp && !m_Settings.hideHelpBar,
        };
    }

    // HeadsUp's own draws, which come back through the hooks.
    bool Ours() const { return m_Drawing || m_Outline.Drawing(); }

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

    // Draws, then restore(texture 0 as it was) puts back the game's states. On Windows the game's scene copy came back
    // without its texture and drew its gray vertex color over the whole screen, so the states it draws with are set
    // again every time: a layer under Direct3D 8 can report a binding it no longer has.
    template <typename Draw, typename Restore>
    void DrawThenRestore(Draw draw, Restore restore)
    {
        IDirect3DBaseTexture8* texture = nullptr;
        m_Device->GetTexture(0, &texture);
        const headsup::CopyState before = headsup::ReadCopyState(m_Device);
        m_Drawing = true;
        draw();
        m_Drawing = false;
        restore(texture);
        if (const std::string lost = headsup::StatesNotRestored(before, headsup::ReadCopyState(m_Device)); !lost.empty())
            if (!std::exchange(m_RestoreWarned, true))
                Print("Direct3D did not put back the game's " + lost + " after HeadsUp drew; HeadsUp puts them back itself.");
        headsup::RestoreCopyState(m_Device, before, texture);
        if (texture != nullptr) texture->Release();
    }

    // Every state Direct3D keeps, saved around draw. False when Direct3D could not save them, and nothing was drawn.
    template <typename Draw>
    bool WithSavedState(Draw draw)
    {
        DWORD saved = 0;
        if (m_Device == nullptr || FAILED(m_Device->CreateStateBlock(D3DSBT_ALL, &saved))) return false;
        IDirect3DSurface8* target = nullptr;
        IDirect3DSurface8* depth  = nullptr;
        m_Device->GetRenderTarget(&target);
        m_Device->GetDepthStencilSurface(&depth);
        DrawThenRestore(draw, [&](IDirect3DBaseTexture8*) {
            m_Device->SetRenderTarget(target, depth);
            m_Device->ApplyStateBlock(saved); // after SetRenderTarget, which resets the viewport
            m_Device->DeleteStateBlock(saved);
        });
        if (target != nullptr) target->Release();
        if (depth != nullptr) depth->Release();
        return true;
    }

    // Only the states the quads change, which is all the bar changes on the bound target: the block recorded at its
    // first draw, texture 0 and stream 0. Every state, as WithSavedState, when Direct3D cannot record or capture it.
    template <typename Draw>
    bool WithQuadStates(Draw draw)
    {
        if (m_Device == nullptr) return false;
        if (!std::exchange(m_QuadStatesTried, true)) m_QuadStates = headsup::RecordQuadStates(m_Device);
        // One that stops capturing is let go, not deleted: its handle may not be HeadsUp's any more.
        if (m_QuadStates && FAILED(m_Device->CaptureStateBlock(*m_QuadStates))) m_QuadStates.reset();
        if (!m_QuadStates) return WithSavedState(draw);
        IDirect3DVertexBuffer8* stream = nullptr;
        UINT stride                    = 0;
        m_Device->GetStreamSource(0, &stream, &stride);
        DrawThenRestore(draw, [&](IDirect3DBaseTexture8* texture) {
            m_Device->ApplyStateBlock(*m_QuadStates);
            m_Device->SetTexture(0, texture);
            m_Device->SetStreamSource(0, stream, stride);
        });
        if (stream != nullptr) stream->Release();
        return true;
    }

    void NotDrawn(const char* what, bool& said)
    {
        if (!std::exchange(said, true))
            Print(std::string(what) + " not drawn: Direct3D could not save the game's render states.");
    }
    void PlatesNotDrawn() { NotDrawn("nameplates are", m_PlatesUnsaved); }
    void BarNotDrawn() { NotDrawn("the top bar is", m_BarUnsaved); }

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
        const bool saved = WithSavedState([&] {
            if (FAILED(m_Device->SetRenderTarget(m_Names.SceneTarget(), m_Names.SceneDepth()))) return;
            DrawNameplates(1.0f / m_Names.TargetScaleX(), 1.0f / m_Names.TargetScaleY(), true);
            m_DrewInScene = true;
        });
        if (!saved) PlatesNotDrawn();
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

    // At Present, a render boundary: the game is not inside its name routine or its target window's draw.
    void StartHooks()
    {
        if (std::exchange(m_HooksTried, true)) return;
        if (const std::string failure = m_NameHook.Start(); !failure.empty())
            Print("could not hook the game's names: " + failure + ".");
        if (const std::string failure = m_ArrowHook.Start(); !failure.empty())
            Print("could not hook the game's target arrows, so it draws its own and HeadsUp none: " + failure + ".");
        if (const std::string failure = m_PointerFix.Start(); !failure.empty())
            Print("could not fix the game's mouse pointer: " + failure + ".");
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
                                                       m_Nameplates.LastShown(), [&](uint32_t index) { return m_GameState.PlayerStatusOf(index, m_SelfIndex); }});
        std::fclose(out);
        m_DrawDump.Wrote(path, draws);
        if (!m_DrawDump.Watching()) Print("wrote " + std::to_string(draws) + " draw calls to " + path);
    }
#endif

    void OnCheckReply(const headsup::CheckReply& reply)
    {
        IEntity* entity = m_AshitaCore->GetMemoryManager()->GetEntity();
        const headsup::MobRecord* mob = headsup::FindMob(reply.serverId, EntityName(entity, reply.targetIndex));
        m_Checks.Received(reply, headsup::CheckLifetime(mob), Now());
    }

    void Print(const std::string& message)
    {
        const std::string line = Ashita::Chat::Header(kName) + Ashita::Chat::Message(message);
        m_AshitaCore->GetChatManager()->Write(kChatMode, false, line.c_str());
    }

    // It scans the client's image, so once; the bar's widget and the dev dump share it.
    void FindStripText()
    {
        if (m_StripTextFound == "not looked for") m_StripTextFound = m_StripText.Find();
    }

    // The client's own, which a reload does not lose as the last weather packet is; that while it cannot be read.
    std::optional<headsup::Weather> WeatherNow()
    {
        if (!std::exchange(m_WeatherTried, true))
            if (const std::string failure = m_WeatherReader.Find(); !failure.empty())
                Print("the weather widget goes by the last weather change: " + failure + ".");
        if (m_Zone != 0)
            if (const std::optional<headsup::Weather> weather = m_WeatherReader.Read()) return weather;
        return m_GameState.ZoneWeather();
    }

    std::optional<headsup::NetCounters> NetCountersNow()
    {
        if (!std::exchange(m_NetStatsTried, true))
            if (const std::string failure = m_NetStats.Find(); !failure.empty())
                Print("the connection widget shows no S and R: " + failure + ".");
        return m_NetStats.Read();
    }

    // The game's CP932 as UTF-8, through Windows; "" when it cannot be converted.
    static std::string Cp932AsUtf8(const std::string& cp932)
    {
        constexpr UINT kCp932 = 932;
        const int wide = MultiByteToWideChar(kCp932, 0, cp932.data(), static_cast<int>(cp932.size()), nullptr, 0);
        if (wide <= 0) return "";
        std::wstring text(static_cast<size_t>(wide), L'\0');
        MultiByteToWideChar(kCp932, 0, cp932.data(), static_cast<int>(cp932.size()), text.data(), wide);
        const int bytes = WideCharToMultiByte(CP_UTF8, 0, text.data(), wide, nullptr, 0, nullptr, nullptr);
        if (bytes <= 0) return "";
        std::string utf8(static_cast<size_t>(bytes), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), wide, utf8.data(), bytes, nullptr, nullptr);
        return utf8;
    }

    // Converted only when the game's bytes change, so the widget's text cache keeps still.
    std::optional<headsup::StripText> StripTextNow()
    {
        FindStripText();
        if (!m_StripText.Read(headsup::StripWindow::Help, m_HelpBytes) ||
            !m_StripText.Read(headsup::StripWindow::Title, m_TitleBytes))
        {
            m_StripConverted = false;
            return std::nullopt; // hidden; the dev dump says why
        }
        if (!m_StripConverted || m_HelpBytes != m_LastHelp || m_TitleBytes != m_LastTitle)
        {
            m_LastHelp       = m_HelpBytes;
            m_LastTitle      = m_TitleBytes;
            m_StripConverted = true;
            const auto convert = [](const std::string& bytes) { return Cp932AsUtf8(bytes); };
            m_Strip = headsup::StripText{
                headsup::Cp932ToUtf8(headsup::ShownCp932(m_TitleBytes.data(), m_TitleBytes.size()), convert),
                headsup::Cp932ToUtf8(headsup::ShownCp932(m_HelpBytes.data(), m_HelpBytes.size()), convert)};
        }
        return m_Strip;
    }

    // An ImGui font from a file: a bundled font's bytes are written under config/headsup/fonts once, a Windows font
    // is read from its Fonts folder. Each is loaded once and kept, as ImGui cannot drop one.
    ImFont* MenuFontFile(const char* name, const char* file)
    {
        if (const auto known = m_MenuFonts.find(name); known != m_MenuFonts.end()) return known->second;
        std::string path;
        if (file != nullptr)
            path = headsup::WindowsFontPath(file);
        else
        {
            size_t count = 0;
            for (const headsup::BundledFont* font = headsup::BundledFonts(count); count-- > 0; ++font)
            {
                if (std::strcmp(font->family, name) != 0) continue;
                const std::string folder = OwnFolder("config") + "\\fonts";
                CreateDirectoryA(folder.c_str(), nullptr);
                path = folder + "\\" + name + ".ttf";
                if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) break;
                FILE* out = std::fopen(path.c_str(), "wb");
                if (out == nullptr) return nullptr;
                const bool written = std::fwrite(font->data, 1, font->bytes, out) == font->bytes;
                if (std::fclose(out) != 0 || !written) return nullptr;
                break;
            }
        }
        ImFont* loaded = path.empty() || GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES
                             ? nullptr
                             : m_AshitaCore->GetGuiManager()->AddFontFromFileTTF(path.c_str(), kMenuFontPixels);
        m_MenuFonts[name] = loaded;
        return loaded;
    }

    // The menu's text font as the settings pick it, and the bar's title font for its titles.
    void ApplyMenuFont()
    {
        if (m_MenuFontApplied == m_Settings.menuFont) return;
        m_MenuFontApplied             = m_Settings.menuFont;
        const headsup::MenuFont* font = headsup::MenuFontNamed(m_Settings.menuFont);
        ImFont* text  = font != nullptr ? MenuFontFile(font->name, font->file) : nullptr;
        ImFont* title = MenuFontFile(headsup::kMarcellusSC, nullptr);
        if (text == nullptr || title == nullptr)
            Print("the settings window keeps Ashita's font: " + m_Settings.menuFont + " did not load.");
        m_Menu.SetFonts(text, title, font != nullptr ? font->scale : 1.0f);
    }

    void HookPresent(IDirect3DDevice8* device)
    {
        if (g_PresentSlot != nullptr || device == nullptr) return;
        void** slot = *reinterpret_cast<void***>(device) + kPresentSlot;
        if (*slot == reinterpret_cast<void*>(&HookedPresent)) return;
        g_RealPresent = reinterpret_cast<PresentFn>(*slot);
        if (!WriteSlot(slot, reinterpret_cast<void*>(&HookedPresent)))
        {
            Print("the top bar's popups draw under other addons' windows: Present could not be hooked.");
            return;
        }
        g_PresentSlot = slot;
    }

    // Puts Present back, unless something hooked it over HeadsUp, whose calls would then still come through here.
    void UnhookPresent()
    {
        g_AfterGui = nullptr;
        if (g_PresentSlot == nullptr) return;
        if (*g_PresentSlot == reinterpret_cast<void*>(&HookedPresent))
            WriteSlot(g_PresentSlot, reinterpret_cast<void*>(g_RealPresent));
        g_PresentSlot = nullptr;
    }

    // The open popup, over every ImGui window: in a scene of its own, with the bar's quad states.
    void DrawOverGui()
    {
        if (!g_BarOn || m_Device == nullptr || FAILED(m_Device->BeginScene())) return;
        WithQuadStates([&] { m_Bar.DrawPopupQuads(); });
        m_Device->EndScene();
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

    static int64_t UnixMsOf(std::chrono::system_clock::time_point clock)
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(clock.time_since_epoch()).count();
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
            m_FrameStats.Add(now, ms);
        }
        m_LastPresent = now;
    }

    // A player's status from the game's memory, known at once, with what only the packets carry once seen.
    std::optional<headsup::PlayerStatus> CurrentStatus(IEntity* entity, uint16_t index, headsup::EntityKind kind)
    {
        if (kind != headsup::EntityKind::Player) return std::nullopt;
        const headsup::PlayerStatus memory = headsup::StatusFromRender(entity->GetRenderFlags1(index), entity->GetRenderFlags2(index),
            entity->GetLinkshellColor(index));
        headsup::PlayerStatus status = headsup::WithPacketStatus(memory, m_GameState.PlayerStatusOf(index, m_SelfIndex));
        status.levelSync             = m_GameState.LevelSynced(index) || m_BuffSynced.count(index) != 0;
        return status;
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

    headsup::BarState BarStateNow(double now)
    {
        const headsup::BarNeeds needs = headsup::NeedsOf(m_Settings, m_BarInput.Open());
        headsup::BarState s;
        s.weather = WeatherNow();
        s.owners   = m_GameState.Owners();
        s.standing = m_GameState.Standing();
        if (needs.unixTime || needs.localTime || needs.signet)
        {
            const auto clock = std::chrono::system_clock::now();
            s.unixMs         = UnixMsOf(clock);
            const std::time_t seconds = std::chrono::system_clock::to_time_t(clock);
            if (const std::tm* local = needs.localTime ? std::localtime(&seconds) : nullptr; local != nullptr)
            {
                s.earthYear   = local->tm_year + 1900;
                s.earthMonth  = local->tm_mon + 1;
                s.earthDay    = local->tm_mday;
                s.earthHour   = local->tm_hour;
                s.earthMinute = local->tm_min;
                s.earthSecond = local->tm_sec;
            }
        }
        if (needs.zone) s.zone = m_Zone;
        if (needs.frames) s.frames = m_FrameStats.Report();
        if (needs.job)
        {
            IPlayer* player = m_AshitaCore->GetMemoryManager()->GetPlayer();
            const headsup::JobSample sample{player->GetMainJob(), player->GetMainJobLevel(), player->GetSubJob(),
                player->GetSubJobLevel(), player->GetExpCurrent(), player->GetExpNeeded(), player->GetLimitPoints(),
                player->GetMeritPoints(), player->GetMeritPointsMax(), player->GetIsLimitModeEnabled()};
            if (sample.mainJob != 0)
            {
                m_Exp.Update(now, sample, m_AshitaCore->GetMemoryManager()->GetParty()->GetMemberServerId(0));
                s.job = headsup::JobReport{sample, m_Exp.PerHour(now)};
            }
            else
                m_Exp.Reset();
        }
        if (needs.connection)
        {
            if (!m_CountersReadAt || now - *m_CountersReadAt >= headsup::kCounterSampling)
            {
                m_Counters       = NetCountersNow();
                m_CountersReadAt = now;
            }
            m_Connection.Frame(now, m_Counters);
            s.connection = m_Connection.Report(now, m_AshitaCore->GetMemoryManager()->GetPlayer()->GetIsZoning() != 0);
        }
        if (needs.party)
        {
            IParty* party = m_AshitaCore->GetMemoryManager()->GetParty();
            for (uint32_t m = 0; m < static_cast<uint32_t>(headsup::kPartySize); ++m)
            {
                if (party->GetMemberIsActive(m) == 0) continue;
                const char* name    = party->GetMemberName(m);
                const uint16_t zone = party->GetMemberZone(m);
                s.party[m] = headsup::PartyMember{name != nullptr ? name : "", party->GetMemberMainJob(m),
                    party->GetMemberMainJobLevel(m), party->GetMemberSubJob(m), party->GetMemberSubJobLevel(m),
                    party->GetMemberHP(m), party->GetMemberMP(m), party->GetMemberTP(m), party->GetMemberHPPercent(m),
                    party->GetMemberMPPercent(m), zone, zone == m_Zone};
            }
        }
        if (needs.signet)
        {
            IPlayer* player = m_AshitaCore->GetMemoryManager()->GetPlayer();
            s.signet = headsup::SignetFrom(player->GetBuffs(), player->GetStatusIcons(), player->GetStatusTimers(),
                s.unixMs);
        }
        if (needs.target)
        {
            s.target   = m_Tracker.FindIndex(m_CursorTargets.target);
            s.itemName = ItemName;
        }
        if (s.target != nullptr)
            s.targetDistance = headsup::DistanceFromSquared(
                m_AshitaCore->GetMemoryManager()->GetEntity()->GetDistance(s.target->index));
        if (s.target != nullptr && s.target->serverId != 0)
            s.targetPhSeconds = m_PhTimers.SecondsLeft(s.target->serverId, now);
        if (s.target != nullptr && s.target->kind == headsup::EntityKind::Player)
            if (const headsup::PlayerCheck* check = m_GameState.PlayerCheckOf(s.target->serverId))
                s.targetCheck = *check;
        IInventory* inventory = m_AshitaCore->GetMemoryManager()->GetInventory();
        if (needs.strip)
        {
            s.strip = StripTextNow();
            if (const std::optional<int> bag = s.strip ? headsup::ContainerNamed(s.strip->caption) : std::nullopt)
            {
                const auto c = static_cast<uint32_t>(*bag);
                s.stripBag = headsup::BagOf(inventory->GetContainerCount(c), inventory->GetContainerCountMax(c));
            }
        }
        if (needs.gil) s.currencies = m_GameState.Currencies();
        if (!needs.gil && needs.containers == 0) return s;
        const Ashita::FFXI::item_t* gil =
            needs.gil ? inventory->GetContainerItem(headsup::kInventory, headsup::kGilSlot) : nullptr;
        if (gil != nullptr) s.gil = headsup::GilOf(gil->Id, gil->Count);
        for (uint32_t c = 0; c < static_cast<uint32_t>(needs.containers); ++c)
            s.bags[c] = headsup::BagOf(inventory->GetContainerCount(c), inventory->GetContainerCountMax(c));
        return s;
    }

    void UpdateCursorTargets()
    {
        ITarget* target = m_AshitaCore->GetMemoryManager()->GetTarget();
        const bool picking = target->GetIsSubTargetActive() != 0;
        m_CursorTargets    = headsup::TargetsFromSlots(picking, target->GetTargetIndex(0), target->GetTargetIndex(1),
            (target->GetLockedOnFlags() & kLockedOn) != 0, m_AshitaCore->GetMemoryManager()->GetEntity()->GetEntityMapSize());
        if (!picking) m_ArrowHook.ForgetPicked();
        m_CursorTargets.outOfRange      = picking && headsup::PickedOutOfRange(m_ArrowHook.PickedColor()).value_or(false);
        m_CursorTargets.gameArrowsShown = !m_ArrowHook.Running();
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
        m_Zone = static_cast<uint16_t>(party->GetMemberZone(0));
        m_TimerLines.clear();
        if (m_Settings.phTimers) m_TimerLines = m_PhTimers.Lines(m_Zone, now);
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
        if (const std::optional<headsup::NetCounters> net = NetCountersNow())
            std::fprintf(out, "connection: R %d S %d meter %d\n", net->received, net->sent, net->health);
        else
            std::fprintf(out, "connection: counters not read\n");
        const std::vector<uint8_t>& conquest = m_GameState.LastConquestPacket();
        std::fprintf(out, "last 0x05E, %u bytes:", static_cast<unsigned>(conquest.size()));
        for (size_t i = 0; i < conquest.size(); ++i)
            std::fprintf(out, "%s%02X", i % 16 == 0 ? "\n  " : " ", conquest[i]);
        std::fprintf(out, "\n");
        IPlayer* player       = m_AshitaCore->GetMemoryManager()->GetPlayer();
        const int16_t* icons  = player->GetStatusIcons();
        const uint32_t* times = player->GetStatusTimers();
        const int64_t unixMs  = UnixMsOf(std::chrono::system_clock::now());
        bool influence        = false;
        for (int i = 0; icons != nullptr && times != nullptr && i < headsup::kStatusIconSlots; ++i)
            if (icons[i] == headsup::kSignetEffect || icons[i] == headsup::kSanctionEffect)
            {
                const std::optional<double> left = headsup::StatusSecondsLeft(times[i], unixMs);
                std::fprintf(out, "status %d in icon slot %d: timer %08X, %.0f s left\n", icons[i], i,
                    static_cast<unsigned>(times[i]), left ? *left : -1.0);
                influence = true;
            }
        if (!influence) std::fprintf(out, "status %d or %d: in no icon slot\n", headsup::kSignetEffect, headsup::kSanctionEffect);
        {
            IParty* party = m_AshitaCore->GetMemoryManager()->GetParty();
            for (uint32_t m = 0; m < static_cast<uint32_t>(headsup::kPartySize); ++m)
            {
                if (party->GetMemberIsActive(m) == 0) continue;
                const char* name = party->GetMemberName(m);
                std::fprintf(out, "party %u: '%s' %s %u / %s %u HP %u (%u%%) MP %u (%u%%) TP %u zone %u (yours %u)\n", m,
                    name != nullptr ? name : "", headsup::JobName(party->GetMemberMainJob(m)),
                    party->GetMemberMainJobLevel(m), headsup::JobName(party->GetMemberSubJob(m)),
                    party->GetMemberSubJobLevel(m), party->GetMemberHP(m), party->GetMemberHPPercent(m),
                    party->GetMemberMP(m), party->GetMemberMPPercent(m), party->GetMemberTP(m),
                    party->GetMemberZone(m), m_Zone);
            }
        }
        if (const headsup::ActorInfo* t = m_Tracker.FindIndex(m_CursorTargets.target); t != nullptr)
            if (const headsup::PlayerCheck* c = m_GameState.PlayerCheckOf(t->serverId))
                std::fprintf(out, "check of %s: jobs %u/%u levels %u/%u master %u linkshell \"%s\" title %u\n",
                    c->message.name.c_str(), c->general.mainJob, c->general.subJob, c->general.mainLevel,
                    c->general.subLevel, c->general.masterLevel, c->general.linkshell.c_str(), c->message.title);
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
