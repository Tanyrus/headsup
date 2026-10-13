#include "Ashita.h"

#include "bar_connection.h"
#include "bar_document.h"
#include "bar_fps.h"
#include "bar_job.h"
#include "check.h"
#include "commands.h"
#include "copy_state.h"
#include "cursor_picture.h"
#include "d3d_util.h"
#include "debug_report.h"
#include "fonts.h"
#include "game_cursor.h"
#include "game_names.h"
#include "game_state.h"
#include "installed_fonts.h"
#include "menu_state.h"
#include "mobdata.h"
#include "nameplate_render.h"
#include "native_hook.h"
#include "outline.h"
#include "ph_timers.h"
#include "player_status.h"
#include "pointer_keys.h"
#include "pointer_swap.h"
#include "rml_bar.h"
#include "rml_host.h"
#include "rml_menu.h"
#include "settings.h"
#include "shot_watch.h"
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
#include <deque>
#include <fstream>
#include <iterator>
#include <mutex>
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
    bool KeyDown(uint8_t vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

    constexpr const char* kName              = "headsup";
    // Ashita takes a plugin's version as one number, so it holds the release tag's major.minor; the release workflow
    // checks the two agree.
    constexpr double kVersion                = 0.65;
    constexpr const char* kConfigAlias       = "headsup";
    constexpr const char* kConfigFolder      = "config";
    constexpr const char* kLogsFolder        = "logs";
    constexpr const char* kFolder            = "headsup"; // inside Ashita's config and logs folders
    constexpr const char* kCursorsFolder     = "cursors"; // inside HeadsUp's config folder: the player's cursor files
    constexpr const char* kSettingsFile      = "settings.ini";
    constexpr const char* kNewFileSuffix     = ".new";          // after the process id: the file while it is written
    constexpr long kMaxSettingsBytes         = 4 * 1024 * 1024; // past this the file is not HeadsUp's to rewrite
    constexpr double kLinkshellReadSeconds   = 1.0;             // between two reads of the linkshells you wear
    constexpr const char* kSection           = "settings";
    constexpr uint32_t kLockedOn             = 0x01; // ITarget::GetLockedOnFlags
    constexpr int kCaptureFrames             = 120;
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
    // Every container the client keeps items of, the Recycle Bin after Wardrobe 8 included.
    constexpr auto kContainers               = static_cast<uint32_t>(Ashita::FFXI::Enums::Container::Max);
    static_assert(headsup::kContainerSlots == sizeof(Ashita::FFXI::items_t::Items) / sizeof(Ashita::FFXI::item_t));

    class AshitaStore final : public headsup::SettingsStore
    {
    public:
        IConfigurationManager* config = nullptr; // from Initialize on

        bool GetBool(const char* key, bool fallback) override { return config->GetBool(kConfigAlias, kSection, key, fallback); }
        float GetFloat(const char* key, float fallback) override { return config->GetFloat(kConfigAlias, kSection, key, fallback); }
        std::string GetString(const char* key, const char* fallback) override
        {
            const char* value = config->GetString(kConfigAlias, kSection, key);
            return value != nullptr ? value : fallback;
        }
        void Set(const char* key, const char* value) override { config->SetValue(kConfigAlias, kSection, key, value); }
    };

    // Set at Initialize and Present, and read by OnMouse, which Ashita calls on the game's thread before the game sees
    // each message.
    bool g_SyncClicks               = false;
    HWND g_GameWindow               = nullptr;
    IGuiManager* g_Gui              = nullptr;
    headsup::BarClicks* g_BarClicks = nullptr;
    bool g_BarOn                    = false;
    bool g_BarHidden                = false;
    float g_BarScaleX               = 1.0f; // back-buffer pixels per client pixel
    float g_BarScaleY               = 1.0f;
    IResourceManager* g_Resources   = nullptr; // for the target popup's drops, from Initialize to Release
    headsup::RmlHost* g_Rml         = nullptr; // for OnMouse and OnKeyboard, from Initialize to Release

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
        const float x = static_cast<float>(static_cast<int16_t>(LOWORD(lParam))) * g_BarScaleX;
        const float y = static_cast<float>(static_cast<int16_t>(HIWORD(lParam))) * g_BarScaleY;
        // RmlUi's overlay lies over every window: what it takes, nothing under it sees.
        const bool overRml = !blocked && g_Rml != nullptr && g_Rml->Running() &&
                             g_Rml->Mouse(message, x, y, static_cast<int16_t>(HIWORD(wParam)));
        // An Ashita window takes the mouse. Asked only when read: the bar ignores it while off or for blocked messages.
        const bool overGui = overRml || (((g_BarOn && !g_BarHidden && !blocked) || g_SyncClicks) && g_Gui != nullptr &&
                                            g_Gui->GetIO().WantCaptureMouse);
        if (g_BarClicks != nullptr &&
            g_BarClicks->Take(message, static_cast<uint32_t>(wParam), x, y, g_BarOn && !g_BarHidden, overGui, blocked))
            return TRUE;
        if (overRml) return TRUE;
        if (!g_SyncClicks) return FALSE;
        // The game takes a click where its pointer last was, which is somewhere else after the mouse moved over another
        // window: a mouse move to the click's place goes first.
        if (!blocked && !overGui && g_GameWindow != nullptr && headsup::ClickMessage(message))
            SendMessageA(g_GameWindow, WM_MOUSEMOVE, LOWORD(wParam), lParam);
        // The game hides its pointer for typing until it sees the mouse move, which it never does over an Ashita window.
        if (message == WM_MOUSEMOVE && overGui) headsup::PointerFix::RevealUnderWindow();
        return FALSE;
    }

    // Ashita's keyboard hook: wParam the virtual key, lParam the message's flags, whose top bit says the key went up.
    constexpr uint32_t kKeyUpFlag = 0x80000000;
    BOOL __stdcall OnKeyboard(WPARAM wParam, LPARAM lParam, bool blocked)
    {
        if (blocked || g_Rml == nullptr || !g_Rml->Running()) return FALSE;
        return g_Rml->Key(static_cast<uint32_t>(wParam), (static_cast<uint32_t>(lParam) & kKeyUpFlag) == 0) ? TRUE : FALSE;
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
    AshitaStore m_Store;
    headsup::ChangedKeys m_SavedKeys{m_Store};
    std::optional<double> m_SettingsChangedAt;
    double m_LastSaveMs   = 0.0;
    size_t m_LastSaveKeys = 0; // how many keys the last save had for the file
    headsup::Tracker m_Tracker;
    headsup::PhTimers m_PhTimers;
    std::vector<headsup::TimerLine> m_TimerLines;
    uint16_t m_SelfIndex = 0;
    uint16_t m_Zone      = 0; // this frame
    headsup::OutlineRenderer m_Outline;
    headsup::GameNames m_Names;
    headsup::NameplateRenderer m_Nameplates;
    headsup::BarInput m_BarInput;
    headsup::RmlHost m_Rml;
    headsup::RmlBar m_RmlBar;
    headsup::RmlMenu m_RmlMenu;
    bool m_RmlTried = false;
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
    headsup::WeatherReader m_WeatherReader;
    bool m_WeatherTried = false; // it scans the client's code, so once
    headsup::NetStatsReader m_NetStats;
    bool m_NetStatsTried = false; // it scans the client's code, so once
    std::optional<headsup::NetCounters> m_Counters; // the last read, kept between samples
    std::optional<double> m_CountersReadAt;
    headsup::ConnectionMeter m_Connection;
    bool m_HooksTried = false;
    bool m_PointerFailed = false; // said once; tried again when the setting changes
    headsup::InstalledFonts m_Fonts;
    headsup::PlayerState m_Player;
    headsup::CheckResults m_Checks;
    headsup::GameState m_GameState;
    std::unordered_set<uint16_t> m_BuffSynced; // you and your party, from the buffs the game keeps, this frame
    std::vector<uint32_t> m_PartyIds;           // server IDs of you, your party and alliance, this frame
    std::vector<std::string> m_Linkshells;      // the linkshells you wear, as last read
    std::optional<double> m_LinkshellsReadAt;
    std::unordered_map<uint32_t, headsup::PlayerJobs> m_PartyJobs; // theirs as the party list has them, by server ID
    headsup::PlayerJobs m_OwnJobs;                                // yours as the game has them, this frame
    bool m_DebugPending = false;
    bool m_Screenshot = false;
    std::mutex m_CommandLock;
    std::vector<std::string> m_Commands; // /hu lines as they came, until the frame that runs them
    headsup::ShotWatch m_Shot;
    headsup::KeyPress m_ShotKey;
    bool m_SceneCopied      = false; // this frame: the first draw to cover the back buffer came
    bool m_LoneCursorsDrawn = false; // this frame
    bool m_UiHidden = false;
    float m_GuiAlpha = 1.0f;
    int m_GuiFlags = 0;
    bool m_FontsWereVisible = true, m_PrimitivesWereVisible = true;
    bool m_PlatesPlaced = false; // this frame, in the scene or at the back-buffer EndScene

#ifdef HEADSUP_DEV
    headsup::DrawDump m_DrawDump;
    headsup::PointerWatch m_PointerWatch;
    // Input played into HeadsUp's own mouse and keyboard handlers as Ashita would hand it over, a step a frame:
    // what /hu play queued, for trying the windows where no real pointer is.
    struct PlayedInput
    {
        bool key;
        uint32_t message;
        WPARAM wParam;
        LPARAM lParam;
    };
    std::deque<PlayedInput> m_Played;
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
    std::vector<std::string> m_CursorFiles;      // the cursors folder's, as the Cursor Style list offers them
    std::optional<std::string> m_CursorFileRead; // the File cursor read last, whether or not it was one
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
        m_Store.config                = config;
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
        ScanCursors();
        g_Gui       = core->GetGuiManager();
        g_BarClicks = &m_BarClicks;
        core->GetInputManager()->GetMouse()->AddCallback(kName, OnMouse);
        core->GetInputManager()->GetKeyboard()->AddCallback(kName, nullptr, nullptr, OnKeyboard);
        g_Rml = &m_Rml;
        return true;
    }

    void Release(void) override
    {
        if (m_SettingsChangedAt || m_SavedKeys.Unwritten()) SaveSettings();
        SetScreenshot(false);
        m_AshitaCore->GetInputManager()->GetMouse()->RemoveCallback(kName);
        m_AshitaCore->GetInputManager()->GetKeyboard()->RemoveCallback(kName);
        g_Rml = nullptr;
        g_SyncClicks = false;
        g_Gui        = nullptr;
        g_Resources  = nullptr;
        if (const std::string failure = m_NameHook.Stop(); !failure.empty()) Print(failure + ".");
        if (const std::string failure = m_ArrowHook.Stop(); !failure.empty()) Print(failure + ".");
        if (const std::string failure = m_WindowHook.Stop(); !failure.empty()) Print(failure + ".");
        if (const std::string failure = m_PointerFix.Stop(); !failure.empty()) Print(failure + ".");
        m_Pointer.Stop();
        m_Fonts.Wait();
        m_Nameplates.Release();
        UnhookPresent();
        g_Plugin = nullptr;
        m_RmlBar.Release();
        m_RmlMenu.Close();
        m_Rml.Stop();
        // After the renderers, which hold fonts made from these until they release.
        for (HANDLE handle : m_LoadedFonts)
            RemoveFontMemResourceEx(handle);
        m_LoadedFonts.clear();
        if (m_QuadStates) m_Device->DeleteStateBlock(*m_QuadStates);
        g_BarClicks = nullptr;
        g_BarOn     = false;
        // Ashita would keep the settings' values loaded after this, for the next load to find even with the file gone.
        m_AshitaCore->GetConfigurationManager()->Delete(kConfigAlias);
    }

    bool Direct3DInitialize(IDirect3DDevice8* device) override
    {
        m_Device = device;
        HookPresent(device);
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
        // A queued command (a script's, another plugin's) comes on a thread of Ashita's, not the game's: what it
        // does waits for the next frame, on the thread everything else here runs on.
        const std::lock_guard lock(m_CommandLock);
        m_Commands.emplace_back(command);
        return true;
    }

    void RunCommands()
    {
        std::vector<std::string> commands;
        {
            const std::lock_guard lock(m_CommandLock);
            commands.swap(m_Commands);
        }
        for (const std::string& command : commands)
            RunCommand(headsup::SplitLower(command.c_str()), command.c_str());
    }

    void RunCommand(const std::vector<std::string>& args, [[maybe_unused]] const char* command)
    {
        if (args.size() == 1)
        {
            if (m_Rml.Running())
                m_RmlMenu.open = !m_RmlMenu.open;
            else
                Print("the settings window cannot open: RmlUi is not running.");
        }
        else if (args[1] == "on" || args[1] == "off")
        {
            m_Settings.enabled = args[1] == "on";
            SaveSettings();
            Print(std::string("HeadsUp ") + (m_Settings.enabled ? "on" : "off"));
        }
        else if (args[1] == "screenshot")
            SetScreenshot(!m_Screenshot);
        else if (args[1] == "debug")
            m_DebugPending = true;
        else if (args[1] == "rml")
        {
            if (!m_Rml.Running())
                Print("RmlUi is not running.");
            else if (args.size() > 2 && args[2] == "debug")
                m_Rml.SetDebugger(!m_Rml.DebuggerVisible());
            else
                Print("/hu rml debug opens RmlUi's debugger.");
        }
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
            if (!m_RmlMenu.OpenPage(args[2])) Print("/hu menu: no page is named " + args[2] + ".");
#ifdef HEADSUP_DEV
            // A number last scrolls the page that many pixels down.
            char* end          = nullptr;
            const float pixels = std::strtof(args.back().c_str(), &end);
            if (args.size() > 3 && end != args.back().c_str() && *end == '\0') m_RmlMenu.ScrollTo(pixels);
#endif
        }
#ifdef HEADSUP_DEV
        else if (args[1] == "play")
            Play(args, command);
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
            Print("/hu on | /hu off: turn all of HeadsUp (outlines, nameplates, the top panel and the rest) on or off");
            Print("/hu screenshot: hide every name, HeadsUp's bar and menu and every addon window until it is run again; bind it with /bind");
            Print("/hu debug: write what every outline and nameplate show to logs/headsup");
            Print("/hu menu <page>: open the settings at a page, such as entities, mobs or toppanel");
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
        RunCommands();
#ifdef HEADSUP_DEV
        if (m_DrawDump.NextFrame(Now(), m_Names, m_NameHook, m_Tracker, m_Nameplates)) WriteDrawDump();
        if (headsup::NameDumpReady()) WriteNameDump();
        if (headsup::StackDumpReady()) WriteStackDump();
        m_PointerWatch.Frame(m_AshitaCore, Now());
        if (!m_Played.empty())
        {
            const PlayedInput step = m_Played.front();
            m_Played.pop_front();
            if (step.key)
                OnKeyboard(step.wParam, step.lParam, false);
            else
                OnMouse(step.message, step.wParam, step.lParam, false);
        }
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
        UpdateCursorTargets();
        UpdateTracker(now);
        g_GameWindow = m_AshitaCore->GetProperties()->GetFinalFantasyHwnd();
        if (m_ShotKey.Pressed(static_cast<uint8_t>(m_Settings.screenshotKey), ShotKeyDown())) SetScreenshot(!m_Screenshot);
        const uint8_t loginStatus = m_AshitaCore->GetMemoryManager()->GetPlayer()->GetLoginStatus();
        // A popup left open at logout would come back with the next character.
        if (loginStatus != headsup::kLoginInGame) m_BarInput.Reset();
        m_UiHidden = m_Screenshot || m_Player.inEvent;
        StartRml();
        g_BarOn     = headsup::BarRuns(m_Settings, loginStatus) && m_Rml.Running();
        g_BarHidden = m_UiHidden;
        // Opening Region Info asks the server for the conquest standings; HeadsUp asks the same once per zone-in, and
        // again while the Zone popup shows them.
        if (m_GameState.ConquestRequestDue(now,
                g_BarOn && m_Settings.barWidgets[static_cast<int>(headsup::BarWidget::Zone)] &&
                    m_AshitaCore->GetMemoryManager()->GetPlayer()->GetIsZoning() == 0,
                m_BarInput.Open() == headsup::BarWidget::Zone))
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
        if (m_Rml.Running())
            m_RmlBar.Update(m_Rml, g_BarOn ? BarStateNow(now) : headsup::BarState{}, m_Settings, m_Names.BackBufferWidth(),
                m_Names.BackBufferHeight(), m_BarInput, m_UiHidden);
        // Over ImGui from Present; with the bar, under ImGui, when Present is not hooked.
        if (g_PresentSlot != nullptr)
            g_AfterGui = [] { if (g_Plugin != nullptr) g_Plugin->DrawOverGui(); };
        m_BarDrawn = false;
        m_ArrowHook.Hide(m_Settings.enabled && m_Settings.cursorStyle != headsup::CursorStyle::Vanilla);
        LoadCursorFile();
        // The game's help bar is the only help left while an event hides HeadsUp's bar; a screenshot wants neither.
        const bool helpGone = m_Settings.hideHelpBar && (m_Screenshot || !m_Player.inEvent);
        const headsup::HiddenWindows hidden{.helpBar = m_Settings.enabled && helpGone,
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
        m_Shot.EndFrame();
        m_SceneCopied      = false;
        m_LoneCursorsDrawn = false;
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
        if (m_Rml.Running())
        {
            const headsup::NameStats names = m_NameHook.Stats();
            const headsup::MenuReadings readings{
                .outlinedMobs     = m_Tracker.OutlinedCount(),
                .meshes           = m_Outline.MeshesLastFrame(),
                .nameplates       = static_cast<uint32_t>(m_Nameplates.LastShown().size()),
                .frameMs          = m_FrameMs,
                .lastSaveMs       = m_LastSaveMs,
                .lastSaveKeys     = m_LastSaveKeys,
                .stencilAvailable = m_Outline.StencilAvailable(),
                .drewInScene      = drewInScene,
                .hooked           = m_NameHook.Running(),
                .names            = names.names,
                .replaced         = names.replaced,
                .hidden           = names.hidden,
                .level            = m_Player.level,
                .sitting          = m_Player.sitting,
                .barTitlePixels   = m_RmlBar.TitlePixels(),
                .seenWindows      = &m_SeenWindows,
                .cursorFiles      = &m_CursorFiles,
            };
            if (m_RmlMenu.Update(MenuHostNow(), m_Settings, readings, m_Fonts.Choices(m_Settings.defaultFont),
                    m_AshitaCore->GetGuiManager()->GetFontSize(), kVersion))
                m_SettingsChangedAt = now;
            if (m_RmlMenu.TakeDebugRequest()) m_DebugPending = true;
            if (m_RmlMenu.TakeCursorsRequest()) ScanCursors();
        }
        if (headsup::SaveDue(m_SettingsChangedAt, now, m_RmlMenu.open)) SaveSettings();
        UpdatePointer();
        m_NameHook.NewFrame(NameOwners());
    }

    // The game draws nameplates into its scene image, copies it to the back buffer and ends that scene before Present.
    // Unless they were drawn into the scene, our nameplates go on top here, in the same frame as the game's names.
    void Direct3DBeginScene(bool isRenderingBackBuffer) override
    {
        if (!Ours()) WatchForShot();
        if (!Ours()) RecordDraw(isRenderingBackBuffer ? 'B' : 'b', D3DPT_POINTLIST, 0u, nullptr, 0u, 0u, 0u);
    }

    void Direct3DEndScene(bool isRenderingBackBuffer) override
    {
        if (!Ours()) RecordDraw(isRenderingBackBuffer ? 'E' : 'e', D3DPT_POINTLIST, 0u, nullptr, 0u, 0u, 0u);
        if (Ours() || !isRenderingBackBuffer) return;
        DrawLoneCursors();
        bool plates = false;
        if (!m_PlatesPlaced)
        {
            m_Names.UseScene(m_NameHook.SceneTarget(), m_NameHook.SceneDepth());
            plates = m_Names.NamesThisFrame();
        }
        const bool bar = g_BarOn && !g_BarHidden && !m_Shot.Hiding() && !std::exchange(m_BarDrawn, true);
        if (plates)
        {
            m_Names.FinishText();
            const bool saved = WithSavedState([&] {
                DrawNameplates(1.0f, 1.0f, false);
                if (bar) DrawBar();
            });
            if (!saved) PlatesNotDrawn();
            if (!saved && bar) BarNotDrawn();
        }
        else if (bar && !WithQuadStates([&] { DrawBar(); }))
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
        if (outlines && owner->outline) m_Outline.DrawOutlined(type, minIndex, numVertices, startIndex, primCount, *owner, m_Settings);
        return false;
    }

    bool Direct3DDrawPrimitiveUP(D3DPRIMITIVETYPE type, UINT primCount, const void* vertices, UINT stride) override
    {
        if (Ours()) return false;
        RecordDraw('U', type, primCount, vertices, stride, 0u, headsup::VertexCount(type, primCount));
        DrawNameplatesBeforeSceneCopy();
        m_Names.OnDrawUP(m_Tracker, m_Settings);
        if (!InterfaceCopy(type, primCount, vertices, stride)) return false;
        DrawLoneCursors();
        if (!Unseen() || !m_Settings.screenshotHidesGame) return false;
        MarkHidden();
        return true;
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
        const std::optional<headsup::BarRect> reach = g_BarOn ? m_RmlBar.HelpBarReach() : std::nullopt;
        const float height = m_Names.BackBufferHeight();
        if (m_MenuHeight <= 0.0f || m_MenuWidth <= 0.0f || height <= 0.0f) return std::nullopt;
        const float down = m_MenuHeight / height;
        // In menu units: the game's menu resolution over the back buffer's. Without the help bar, windows go under
        // the top bar itself.
        const float barBottom = g_BarOn ? m_RmlBar.BarBottomNow() : 0.0f;
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
    void BarNotDrawn() { NotDrawn("the top panel is", m_BarUnsaved); }

    // Ashita's screenshot plugin, asked to hide, takes its fonts away for the frame it captures (shot_watch.h): that
    // frame is drawn as screenshot mode draws. HeadsUp's own hiding of the fonts is not a screenshot.
    void WatchForShot()
    {
        if (!m_Settings.enabled) return;
        const bool was = m_Shot.Hiding();
        if (m_Shot.Seen(m_Screenshot || m_AshitaCore->GetFontManager()->GetVisible()) && !was) m_NameHook.HideAll();
    }
    bool Unseen() const { return m_Screenshot || m_Shot.Hiding(); }

    // The game draws its interface, every window, the chat and its own pointer, into one image and lays that over
    // the scene's copy: the second draw of a frame to cover the back buffer, each a single quad. A screenshot that
    // hides the game's windows blocks it, which leaves the scene.
    bool InterfaceCopy(D3DPRIMITIVETYPE type, UINT primCount, const void* vertices, UINT stride)
    {
        constexpr UINT kQuadTriangles = 2;
        if (primCount != kQuadTriangles ||
            !headsup::CoversTarget(vertices, headsup::VertexCount(type, primCount), stride, m_Names.BackBufferWidth(),
                m_Names.BackBufferHeight()))
            return false;
        DWORD fvf = 0;
        if (!headsup::BoundFvf(m_Device, fvf) || (fvf & D3DFVF_XYZRHW) == 0 || !m_Names.DrawingToBackBuffer())
            return false;
        return std::exchange(m_SceneCopied, true);
    }

    // The key is the player's own while they type; in screenshot mode it always answers, so nothing keeps it on.
    bool ShotKeyDown() const
    {
        if (m_Settings.screenshotKey == 0 || GetForegroundWindow() != g_GameWindow) return false;
        const bool typing = m_AshitaCore->GetChatManager()->IsInputOpen() != 0 ||
                            m_AshitaCore->GetGuiManager()->GetIO().WantTextInput;
        return (m_Screenshot || !typing) && KeyDown(static_cast<uint8_t>(m_Settings.screenshotKey));
    }

    // toX and toY: the bound render target's pixels per back-buffer pixel.
    void DrawNameplates(float toX, float toY, bool depthTest)
    {
        if (Unseen())
        {
            m_Nameplates.Clear();
            m_PlatesPlaced = true;
            return;
        }
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
        if (Unseen())
        {
            m_Nameplates.Clear();
            m_PlatesPlaced = true;
            return;
        }
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

    // Every name, HeadsUp's bar and menu, and every addon's ImGui, font and primitive drawing go, and come back when it
    // is run again or HeadsUp unloads. Outlines stay, and the game's own windows unless the settings take them too.
    void SetScreenshot(bool on)
    {
        if (on == m_Screenshot) return;
        m_Screenshot                  = on;
        IGuiManager* gui              = m_AshitaCore->GetGuiManager();
        IFontManager* fonts           = m_AshitaCore->GetFontManager();
        IPrimitiveManager* primitives = m_AshitaCore->GetPrimitiveManager();
        if (on)
        {
            m_GuiAlpha = gui->GetStyle().Alpha;
            m_GuiFlags = gui->GetIO().ConfigFlags;
            gui->GetStyle().Alpha = 0.0f;
            gui->GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
            m_FontsWereVisible      = fonts->GetVisible();
            m_PrimitivesWereVisible = primitives->GetVisible();
            fonts->SetVisible(false);
            primitives->SetVisible(false);
            Print("screenshot mode: names, the bar, the menu and every addon window are hidden; /hu screenshot brings them back.");
        }
        else
        {
            gui->GetStyle().Alpha    = m_GuiAlpha;
            gui->GetIO().ConfigFlags = m_GuiFlags;
            fonts->SetVisible(m_FontsWereVisible);
            primitives->SetVisible(m_PrimitivesWereVisible);
            Print("screenshot mode off.");
        }
    }

    // Whose names the hook may meet next frame, and what it does with each.
    std::vector<headsup::NameOwner> NameOwners() const
    {
        std::vector<headsup::NameOwner> owners;
        for (const headsup::ActorPtr actor : m_Tracker.Actors())
            if (const headsup::ActorInfo* info = m_Tracker.Find(actor))
                owners.push_back({actor, info->index,
                    headsup::HookActionFor(info->state, m_Settings, m_Screenshot, m_Nameplates.NamesFailed())});
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
        m_Checks.Received(reply, headsup::CheckLifetime(headsup::MobById(reply.serverId)), Now());
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

    void HookPresent(IDirect3DDevice8* device)
    {
        if (g_PresentSlot != nullptr || device == nullptr) return;
        void** slot = *reinterpret_cast<void***>(device) + kPresentSlot;
        if (*slot == reinterpret_cast<void*>(&HookedPresent)) return;
        g_RealPresent = reinterpret_cast<PresentFn>(*slot);
        if (!WriteSlot(slot, reinterpret_cast<void*>(&HookedPresent)))
        {
            Print("the top panel's popups draw under other addons' windows: Present could not be hooked.");
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

    // RmlUi's overlay (the popups and the settings window), over every ImGui window: in a scene of its own.
    void DrawOverGui()
    {
        if (m_Device == nullptr || FAILED(m_Device->BeginScene())) return;
        if (m_Rml.Running() && !m_Screenshot) WithQuadStates([&] { m_Rml.RenderOverlay(); });
        m_Device->EndScene();
    }

    // What the RmlUi settings window runs on: the window's own font, or a bundled one where Windows lacks it.
    headsup::MenuHost MenuHostNow()
    {
        headsup::MenuHost host{m_Rml.Overlay(), headsup::kBarlowCondensed, {}, KeyDown};
        const headsup::MenuFont* font = headsup::MenuFontNamed(m_Settings.menuFont);
        if (font != nullptr && (font->file == nullptr ? m_Rml.EnsureFont(font->name)
                                                      : m_Rml.EnsureFontFile(font->name, headsup::WindowsFontPath(font->file))))
            host.textFont = font->name;
        for (const headsup::MenuFont& choice : headsup::kMenuFonts)
            if (headsup::MenuFontInstalled(choice)) host.menuFonts.emplace_back(choice.name);
        return host;
    }

#ifdef HEADSUP_DEV
    // /hu play click X Y [right] | move X Y | drag X Y TOX TOY | wheel X Y NOTCHES | key NAME | type TEXT
    // Points are the back buffer's pixels; a wheel turns toward the user for positive notches.
    void Play(const std::vector<std::string>& args, const char* command)
    {
        constexpr int kDragSteps  = 6;
        constexpr int kWheelDelta = 120;
        constexpr uint32_t kUp    = 0xC0000001; // a key message's flags as the key goes up
        static const std::unordered_map<std::string, uint32_t> kKeys = {{"enter", VK_RETURN}, {"escape", VK_ESCAPE},
            {"tab", VK_TAB}, {"backspace", VK_BACK}, {"delete", VK_DELETE}, {"left", VK_LEFT}, {"right", VK_RIGHT},
            {"up", VK_UP}, {"down", VK_DOWN}, {"home", VK_HOME}, {"end", VK_END}, {"space", VK_SPACE}, {"a", 'A'}};
        auto number = [&](size_t at) { return at < args.size() ? std::strtof(args[at].c_str(), nullptr) : 0.0f; };
        auto at = [&](float x, float y) {
            return MAKELPARAM(static_cast<int>(x / g_BarScaleX), static_cast<int>(y / g_BarScaleY));
        };
        auto mouse = [&](uint32_t message, float x, float y, WPARAM wParam = 0) {
            m_Played.push_back(PlayedInput{false, message, wParam, at(x, y)});
        };
        std::string what = args.size() > 2 ? args[2] : "";
        float x = number(3), y = number(4);
        float toX = number(5), toY = number(6);
        // id NAME clicks the settings window's control of that id, and id NAME NAME drags the first onto the second.
        if (what == "id" && args.size() > 3)
        {
            // Both ends scrolled into view before either is placed: the second scroll would move the first.
            if (args.size() > 4) m_RmlMenu.CenterOf(args[4]);
            m_RmlMenu.CenterOf(args[3]);
            const std::optional<Rml::Vector2f> from = m_RmlMenu.CenterOf(args[3], false);
            const std::optional<Rml::Vector2f> to   = args.size() > 4 ? m_RmlMenu.CenterOf(args[4], false) : std::nullopt;
            if (!from || (args.size() > 4 && !to))
            {
                Print("/hu play id: the settings window has no such control.");
                return;
            }
            what = to ? "drag" : "click";
            x = from->x, y = from->y;
            if (to) toX = to->x, toY = to->y;
        }
        if (what == "move")
            mouse(WM_MOUSEMOVE, x, y);
        else if (what == "click")
        {
            const bool right = args.size() > 5 && args[5] == "right" && args[2] == "click";
            mouse(WM_MOUSEMOVE, x, y);
            mouse(right ? WM_RBUTTONDOWN : WM_LBUTTONDOWN, x, y, right ? MK_RBUTTON : MK_LBUTTON);
            mouse(right ? WM_RBUTTONUP : WM_LBUTTONUP, x, y);
        }
        else if (what == "drag")
        {
            mouse(WM_MOUSEMOVE, x, y);
            mouse(WM_LBUTTONDOWN, x, y, MK_LBUTTON);
            for (int step = 1; step <= kDragSteps; ++step)
            {
                const float share = static_cast<float>(step) / kDragSteps;
                mouse(WM_MOUSEMOVE, x + (toX - x) * share, y + (toY - y) * share, MK_LBUTTON);
            }
            mouse(WM_LBUTTONUP, toX, toY);
        }
        else if (what == "wheel")
        {
            mouse(WM_MOUSEMOVE, x, y);
            mouse(WM_MOUSEWHEEL, x, y, MAKEWPARAM(0, static_cast<int16_t>(-number(5) * kWheelDelta)));
        }
        else if (what == "key" && args.size() > 3 && kKeys.count(args[3]) != 0)
        {
            m_Played.push_back(PlayedInput{true, 0, kKeys.at(args[3]), 1});
            m_Played.push_back(PlayedInput{true, 0, kKeys.at(args[3]), static_cast<LPARAM>(kUp)});
        }
        else if (what == "theme" || what == "height" || what == "seps" || what == "text" || what == "popuptext")
        {
            // A bar setting by number, saved as a change in the window is.
            const int value = static_cast<int>(number(3));
            if (what == "theme") m_Settings.barTheme = static_cast<headsup::BarTheme>(value);
            if (what == "height") m_Settings.barHeight = value;
            if (what == "seps") m_Settings.barSeparators = static_cast<headsup::BarSeparators>(value);
            if (what == "text") m_Settings.barTextSize = value;
            if (what == "popuptext") m_Settings.popupTextSize = value;
            m_Settings          = headsup::Clamp(m_Settings);
            m_SettingsChangedAt = Now();
        }
        else if (what == "type" && m_Rml.Running())
        {
            // The text as typed, which the lower-cased arguments have lost.
            const char* text = std::strstr(command, " type ");
            if (text == nullptr) text = std::strstr(command, " TYPE ");
            if (text != nullptr) m_Rml.Overlay()->ProcessTextInput(Rml::String(text + std::strlen(" type ")));
        }
        else
            Print("/hu play: click X Y [right], move X Y, drag X Y TOX TOY, wheel X Y NOTCHES, key NAME, type TEXT, id NAME [NAME].");
    }
#endif

    void DrawBar()
    {
        m_Rml.RenderBar();
        // Without Present hooked nothing draws over ImGui: the overlay goes under it, with the bar.
        if (g_PresentSlot == nullptr) m_Rml.RenderOverlay();
    }

    // RmlUi starts once the back buffer's size is known, and follows it after.
    void StartRml()
    {
        const int width  = static_cast<int>(m_Names.BackBufferWidth());
        const int height = static_cast<int>(m_Names.BackBufferHeight());
        if (width <= 0 || height <= 0 || m_Device == nullptr) return;
        if (m_Rml.Running())
        {
            m_Rml.Resize(width, height);
            if (const std::string failure = m_Rml.TakeFailure(); !failure.empty()) Print("RmlUi: " + failure + ".");
            return;
        }
        if (std::exchange(m_RmlTried, true)) return;
        const bool started = m_Rml.Start(m_Device, width, height,
            [this](const std::string& line) { Print("RmlUi: " + line); },
            [](const std::string& name) { return headsup::HostImage(name); });
        if (!started) Print("RmlUi did not start: the top panel and the settings window are off until HeadsUp reloads.");
    }


    std::string OwnFolder(const char* ashitaFolder)
    {
        const std::string parent = std::string(m_AshitaCore->GetInstallPath()) + ashitaFolder;
        CreateDirectoryA(parent.c_str(), nullptr);
        const std::string folder = parent + "\\" + kFolder;
        CreateDirectoryA(folder.c_str(), nullptr);
        return folder;
    }

    std::string CursorsFolder() { return OwnFolder(kConfigFolder) + "\\" + kCursorsFolder + "\\"; }

    void ScanCursors()
    {
        const std::string folder = CursorsFolder();
        CreateDirectoryA(folder.c_str(), nullptr);
        std::vector<std::string> entries;
        WIN32_FIND_DATAA found{};
        const HANDLE search = FindFirstFileA((folder + "*").c_str(), &found);
        if (search != INVALID_HANDLE_VALUE)
        {
            do
                if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) entries.emplace_back(found.cFileName);
            while (FindNextFileA(search, &found) != 0);
            FindClose(search);
        }
        m_CursorFiles = headsup::CursorFileNames(entries);
    }

    // The File cursor's picture, read when the settings come to name another file.
    void LoadCursorFile()
    {
        const bool wanted = m_Settings.cursorStyle == headsup::CursorStyle::File;
        if (!wanted || m_CursorFileRead == m_Settings.cursorFile) return;
        m_CursorFileRead = m_Settings.cursorFile;
        std::optional<headsup::Image> picture;
        for (const char* extension : headsup::kCursorExtensions)
        {
            std::ifstream in(CursorsFolder() + m_Settings.cursorFile + extension, std::ios::binary);
            const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
            picture = headsup::CursorFilePicture(bytes.data(), bytes.size());
            if (picture) break;
        }
        if (!picture) Print("could not read the cursor " + m_Settings.cursorFile + " in " + CursorsFolder() + "; the feather stands in for it");
        m_Nameplates.SetCursorFile(std::move(picture));
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
    // The settings file's name. A developer's build takes another from HEADSUP_SETTINGS in the game's environment, so
    // a test client changes a copy and not the file the player's own game reads.
    static const char* SettingsName()
    {
#ifdef HEADSUP_DEV
        static const std::string named = [] {
            const char* name = std::getenv("HEADSUP_SETTINGS");
            return std::string(name != nullptr && *name != '\0' ? name : kSettingsFile);
        }();
        return named.c_str();
#else
        return kSettingsFile;
#endif
    }
    std::string SettingsPath() const { return std::string(kFolder) + "/" + SettingsName(); }
    std::string SettingsFile() { return OwnFolder(kConfigFolder) + "\\" + SettingsName(); }

    void LoadSettings()
    {
        IConfigurationManager* config = m_AshitaCore->GetConfigurationManager();
        const std::string file        = SettingsFile();
        // Ashita keeps a loaded file's values after the plugin unloads: without this, a file deleted since would load
        // as it was. A missing file is fine: every value keeps its default.
        config->Delete(kConfigAlias);
        if (!config->Load(kConfigAlias, SettingsPath().c_str()) && GetFileAttributesA(file.c_str()) != INVALID_FILE_ATTRIBUTES)
            Print("could not read " + file + "; using the defaults, which a change in the menu will write into that file");
        m_Settings = m_SavedKeys.Load();
        if (m_SavedKeys.Outdated()) m_SettingsChangedAt = Now();
    }

    // The settings file's bytes: none for a missing file. False for one that cannot be read or is too large.
    bool ReadSettingsFile(std::string& text)
    {
        const std::string path = SettingsFile();
        std::FILE* in          = std::fopen(path.c_str(), "rb");
        if (in == nullptr) return GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES;
        const long size = std::fseek(in, 0, SEEK_END) == 0 ? std::ftell(in) : -1;
        const bool fits = size >= 0 && size <= kMaxSettingsBytes && std::fseek(in, 0, SEEK_SET) == 0;
        if (fits) text.resize(static_cast<size_t>(size));
        const bool read = fits && std::fread(text.data(), 1, text.size(), in) == text.size();
        std::fclose(in);
        return read;
    }

    bool FileHolds(const std::vector<headsup::KeyText>& keys)
    {
        std::string text;
        return ReadSettingsFile(text) && headsup::HoldsIniValues(text, kSection, keys);
    }

    // The file with the keys set, written whole beside it under this process's own name, flushed to disk and put in
    // its place in one step.
    bool WriteSettingsFile(const std::vector<headsup::KeyText>& keys)
    {
        std::string text;
        if (!ReadSettingsFile(text)) return false;
        const std::string next = headsup::WithIniValues(text, kSection, keys);
        const std::string path  = SettingsFile();
        const std::string fresh = path + "." + std::to_string(GetCurrentProcessId()) + kNewFileSuffix;
        // Over one this process left at an earlier save, and never another client's.
        const HANDLE out =
            CreateFileA(fresh.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (out == INVALID_HANDLE_VALUE) return false;
        DWORD wrote        = 0;
        const bool flushed = WriteFile(out, next.data(), static_cast<DWORD>(next.size()), &wrote, nullptr) != 0 &&
                             wrote == next.size() && FlushFileBuffers(out) != 0;
        if (CloseHandle(out) != 0 && flushed &&
            MoveFileExA(fresh.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0)
            return true;
        DeleteFileA(fresh.c_str());
        return false;
    }

    void SaveSettings()
    {
        const double before = Now();
        const std::vector<headsup::KeyText>& keys = m_SavedKeys.Save(m_Settings);
        m_LastSaveKeys                            = keys.size();
        if (!keys.empty())
        {
            // Ashita's set has already written each key into a file that loaded, and its Save cannot report a failed
            // write: the file is read back, and written here when it lacks them, as a new install's does.
            const bool held                  = FileHolds(keys) || (WriteSettingsFile(keys) && FileHolds(keys));
            const headsup::SaveNotice notice = m_SavedKeys.ReadBack(held);
            if (notice == headsup::SaveNotice::Failed)
                Print("could not save " + SettingsFile() + "; the change lasts until Ashita closes");
            if (notice == headsup::SaveNotice::Saved)
                Print("saved " + SettingsFile() + ", with the changes it could not take before");
        }
        m_LastSaveMs        = (Now() - before) * 1000.0;
        m_SettingsChangedAt = std::nullopt;
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
        s.shares   = m_GameState.Shares();
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
        IEntity* entity = m_AshitaCore->GetMemoryManager()->GetEntity();
        const Ashita::FFXI::targetwindow_t* window = target->GetRawStructureWindow();
        const auto held = [&](uint32_t slot) {
            return headsup::SlotTarget(target->GetTargetIndex(slot), target->GetIsActive(slot) != 0,
                target->GetIsModelActor(slot) != 0, entity->GetEntityMapSize());
        };
        const uint16_t slot0 = held(0);
        const bool picking =
            headsup::Picking(target->GetIsSubTargetActive() != 0, window != nullptr && window->m_Sub != 0, slot0);
        m_CursorTargets = headsup::TargetsFromSlots(picking, slot0, held(1), (target->GetLockedOnFlags() & kLockedOn) != 0);
        if (!picking) m_ArrowHook.ForgetPicked();
        uint32_t color = 0;
        if (picking)
            color = m_ArrowHook.CandidateColor(window,
                entity->GetActorPointer(m_AshitaCore->GetMemoryManager()->GetParty()->GetMemberTargetIndex(0)),
                m_CursorTargets.subTarget == headsup::kModelTarget ? 0 : entity->GetActorPointer(m_CursorTargets.subTarget));
        if (color == 0) color = m_ArrowHook.PickedColor();
        m_CursorTargets.outOfRange      = picking && headsup::PickedOutOfRange(color).value_or(false);
        m_CursorTargets.gameArrowsShown = !m_ArrowHook.Running();
        PlaceCursorAnchors();
    }

    void PlaceCursorAnchors()
    {
        const Ashita::FFXI::targetwindow_t* window = m_AshitaCore->GetMemoryManager()->GetTarget()->GetRawStructureWindow();
        if (window == nullptr) return;
        const headsup::MenuSize size =
            headsup::MenuSizeOf(m_MenuWidth, m_MenuHeight, m_Names.BackBufferWidth(), m_Names.BackBufferHeight());
        headsup::PlaceAnchors(m_CursorTargets,
            headsup::CursorWindow{static_cast<float>(window->m_AnkX), static_cast<float>(window->m_AnkY),
                static_cast<float>(window->m_SubAnkX), static_cast<float>(window->m_SubAnkY)},
            size.width, size.height, m_Names.BackBufferWidth(), m_Names.BackBufferHeight());
    }

    // The cursor of a target without a nameplate sits on the target window's anchor, which the game moves only once
    // its scene is copied: read any earlier it is the frame before's, and the cursor trailed a moving camera. So it
    // is drawn under the game's interface, where the game's own arrow is, or at the scene's end where no interface
    // was laid over.
    void DrawLoneCursors()
    {
        if (std::exchange(m_LoneCursorsDrawn, true) || Unseen() || m_Device == nullptr) return;
        PlaceCursorAnchors();
        const float width = m_Names.BackBufferWidth(), height = m_Names.BackBufferHeight();
        const std::vector<headsup::LoneCursor> lone = m_Nameplates.LoneCursorsDue(m_Settings, m_CursorTargets, width, height);
        if (lone.empty()) return;
        if (!WithSavedState([&] { m_Nameplates.DrawLoneCursors(lone, m_Settings, Now(), width, height); })) PlatesNotDrawn();
    }

    void UpdateParty(IParty* party, IPlayer* player)
    {
        m_PartyIds.clear();
        m_PartyJobs.clear();
        for (uint32_t member = 0; member < kPartyMembers; ++member)
        {
            if (party->GetMemberIsActive(member) == 0) continue;
            const uint32_t serverId = party->GetMemberServerId(member);
            m_PartyIds.push_back(serverId);
            m_PartyJobs[serverId] = headsup::PlayerJobs{party->GetMemberMainJob(member), party->GetMemberMainJobLevel(member),
                party->GetMemberSubJob(member), party->GetMemberSubJobLevel(member)};
        }
        m_OwnJobs = headsup::PlayerJobs{player->GetMainJob(), player->GetMainJobLevel(), player->GetSubJob(),
            player->GetSubJobLevel()};
        m_BuffSynced.clear();
        if (headsup::LevelSyncInBuffs(player->GetBuffs())) m_BuffSynced.insert(m_SelfIndex);
        for (uint32_t member = 0; member < kPartyIconMembers; ++member)
            if (party->GetStatusIconsServerId(member) != 0 &&
                headsup::LevelSyncInPartyIcons(party->GetStatusIcons(member), party->GetStatusIconsBitMask(member)))
                m_BuffSynced.insert(static_cast<uint16_t>(party->GetStatusIconsTargetIndex(member)));
    }

    // The names of the linkshells you wear, from their items: Phoenix equips a linkshell from any container.
    std::vector<std::string> WornLinkshells() const
    {
        IInventory* inventory = m_AshitaCore->GetMemoryManager()->GetInventory();
        std::vector<std::string> names;
        for (uint32_t container = 0; container < kContainers; ++container)
        {
            const uint32_t last = headsup::LastItemSlot(inventory->GetContainerCountMax(container));
            for (uint32_t slot = headsup::kGilSlot + 1; slot <= last; ++slot)
            {
                const Ashita::FFXI::item_t* item = inventory->GetContainerItem(container, slot);
                if (item == nullptr) continue;
                std::string name = headsup::WornLinkshellName(item->Flags, item->Extra, sizeof(item->Extra));
                if (!name.empty()) names.push_back(std::move(name));
            }
        }
        return names;
    }

    std::optional<headsup::PlayerJobs> JobsOf(uint16_t index, uint32_t serverId) const
    {
        const auto member                 = m_PartyJobs.find(serverId);
        const headsup::PlayerCheck* check = m_GameState.PlayerCheckOf(serverId);
        return headsup::KnownJobs(index == m_SelfIndex ? &m_OwnJobs : nullptr,
            member != m_PartyJobs.end() ? &member->second : nullptr, check != nullptr ? &check->general : nullptr);
    }

    // Your own title is the client's; another player's comes with a check of them.
    uint32_t TitleOf(bool self, uint32_t serverId, IPlayer* player) const
    {
        if (self) return player->GetTitle();
        const headsup::PlayerCheck* check = m_GameState.PlayerCheckOf(serverId);
        return check != nullptr ? check->message.title : 0;
    }

    void UpdateTracker(double now)
    {
        IEntity* entity = m_AshitaCore->GetMemoryManager()->GetEntity();
        IParty* party   = m_AshitaCore->GetMemoryManager()->GetParty();
        IPlayer* player = m_AshitaCore->GetMemoryManager()->GetPlayer();
        m_SelfIndex     = static_cast<uint16_t>(party->GetMemberTargetIndex(0));
        UpdateParty(party, player);
        if (!m_LinkshellsReadAt || now - *m_LinkshellsReadAt >= kLinkshellReadSeconds)
        {
            m_Linkshells       = WornLinkshells();
            m_LinkshellsReadAt = now;
        }
        const uint32_t count = std::min<uint32_t>(entity->GetEntityMapSize(), headsup::kMaxEntities);
        std::vector<headsup::ActorInput> inputs;
        std::vector<headsup::PhSighting> sightings;
        std::vector<headsup::PetLink> pets;
        for (uint32_t i = 0; i < count; ++i)
        {
            if (entity->GetRawEntity(i) == nullptr) continue;
            const auto index    = static_cast<uint16_t>(i);
            const auto kind     = headsup::KindFromSpawnFlags(entity->GetSpawnFlags(i));
            const bool isMob    = kind == headsup::EntityKind::Mob;
            const bool isPlayer = kind == headsup::EntityKind::Player;
            // Before the actor is asked for: a player without one still owns their pet.
            pets.push_back(headsup::PetLink{index, kind, entity->GetPetTargetIndex(i)});
            const uintptr_t actor = entity->GetActorPointer(i);
            if (actor == 0) continue;
            const uint32_t serverId = entity->GetServerId(i);
            const bool self         = index == m_SelfIndex;
            const bool alive        = entity->GetHPPercent(i) > 0;
            const char* name        = headsup::EntityName(entity, i);
            const uint32_t claim    = entity->GetClaimStatus(i);
            const uint32_t state    = entity->GetStatus(i);
            if (isMob && !alive) m_Checks.Forget(serverId); // the next spawn rolls a new level
            if (isMob && m_Settings.phTimers)
            {
                const bool bodyDrawn = m_Names.FramesSinceMesh(index) <= headsup::kMeshGraceFrames;
                if (const auto sighting = headsup::SightingOf(headsup::MobById(serverId), alive, bodyDrawn))
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
                .pose     = isPlayer ? headsup::PoseFromStatus(state) : headsup::Pose::Standing,
                .feet = headsup::FromEntityPosition(entity->GetLocalPositionX(i), entity->GetLocalPositionY(i), entity->GetLocalPositionZ(i)),
                .claimedByParty = isMob && headsup::ClaimedByParty(claim, m_PartyIds),
                .claimed        = isMob && headsup::IsClaimed(claim),
                .party          = isPlayer && headsup::InYourParty(serverId, self, m_PartyIds),
                .isFriend       = isPlayer && headsup::IsFriend(m_Settings.friends, name),
                .linkshell      = isPlayer && headsup::SharesLinkshell(m_GameState.PlayerCheckOf(serverId), m_Linkshells),
                .engaged        = state == kEngagedStatus,
                .self           = self,
                .jobs           = isPlayer ? JobsOf(index, serverId) : std::nullopt,
                .title          = isPlayer ? TitleOf(self, serverId, player) : 0,
            });
        }
        const std::unordered_map<uint16_t, uint16_t> petOwners = headsup::PetOwners(pets);
        for (headsup::ActorInput& input : inputs)
            if (const auto owner = petOwners.find(input.index); owner != petOwners.end())
            {
                const uint16_t ownerIndex = owner->second;
                const bool yours          = ownerIndex == m_SelfIndex;
                input.pet                 = true;
                input.ownerName           = headsup::EntityName(entity, ownerIndex);
                input.petRow = headsup::PetRowOf(yours, headsup::InYourParty(entity->GetServerId(ownerIndex), yours, m_PartyIds),
                    headsup::IsFriend(m_Settings.friends, input.ownerName));
            }
        m_Player.level   = player->GetMainJobLevel();
        m_Player.sitting = headsup::IsSittingStatus(entity->GetStatus(m_SelfIndex));
        // The status the server gave you says an event runs: your own stayed 0 through an NPC's dialog, a cutscene
        // and one begun at zone-in.
        m_Player.inEvent = headsup::IsEventStatus(entity->GetStatusServer(m_SelfIndex));
        m_Tracker.Update(inputs, m_Player, m_Settings, m_CursorTargets);
        // With timers off no sightings are passed, so turning them on never takes a placeholder that died meanwhile for
        // one seen dying.
        m_PhTimers.Update(now, sightings);
        m_Zone = static_cast<uint16_t>(party->GetMemberZone(0));
        m_TimerLines.clear();
        if (m_Settings.phTimers) m_TimerLines = m_PhTimers.Lines(m_Zone, now);
    }

    headsup::DebugSources DebugSourcesNow() const
    {
        return headsup::DebugSources{m_AshitaCore, m_Tracker, m_NameHook, m_Names, m_Nameplates, m_GameState, m_Checks,
            m_CursorTargets, m_Player, m_Settings, m_MenuWidth, m_MenuHeight, m_Zone, Now(), kVersion};
    }

    void WriteDebugReport()
    {
        std::string path;
        std::FILE* out = OpenLog("debug", path);
        if (out == nullptr) return;
        headsup::WriteDebugReport(DebugSourcesNow(), NetCountersNow(), out);
        std::fclose(out);
        Print("wrote " + path + "; recording " + std::to_string(kCaptureFrames) + " frames");
        m_CapturePath = path;
        m_Capture     = headsup::kCaptureHeading;
        m_CaptureLeft = kCaptureFrames;
        m_CaptureLast = Now();
    }

    void CaptureFrame()
    {
        if (m_CaptureLeft <= 0) return;
        const double now = Now();
        m_Capture += headsup::CaptureFrameLine(DebugSourcesNow(), kCaptureFrames - m_CaptureLeft, (now - m_CaptureLast) * 1000.0,
            m_DrewInScene);
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
