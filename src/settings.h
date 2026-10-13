#pragma once

#include "argb.h"
#include "bar_placement.h"
#include "bar_server_time.h"
#include "bar_types.h"
#include "color.h"
#include "currencies.h"
#include "fonts.h"
#include "icons.h"
#include "ini_text.h"
#include "menu_document.h"
#include "menu_fonts.h"
#include "native_locate.h"
#include "plate_settings.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace headsup
{
    constexpr float kMinThickness = 1.0f, kMaxThickness = 16.0f;            // render-target pixels
    constexpr int kMinSmoothness = 4, kMaxSmoothness = 16;                  // shifted copies per mesh
    constexpr float kMinOutlineDistance = 5.0f, kMaxOutlineDistance = 50.0f; // yalms: the game draws no mob past 50
    constexpr int kMinTextSize = 8, kMaxTextSize = 48;   // pixels: the base and cursor sizes
    constexpr int kMinNameRaise = 0, kMaxNameRaise = 40; // pixels
    constexpr const char* kDefaultFont = kMarcellusSC;
    constexpr const char* kBadgeFont   = kBarlowCondensed; // plain enough to read a few pixels tall
    constexpr size_t kMaxFontName      = 31; // a Windows font name's longest, without its terminator

    constexpr std::array<bool, kBarWidgetCount> AllOn()
    {
        std::array<bool, kBarWidgetCount> on{};
        on.fill(true);
        return on;
    }

    constexpr int kMinCursorOutline = 25, kMaxCursorOutline = 200; // percent
    constexpr int kMinOpacity = 10, kMaxOpacity = 100; // percent

    constexpr int kMinMenuScale = 70, kMaxMenuScale = 160; // percent
    constexpr int kMaxMenuSide  = 16384; // pixels: more than any screen is across

    constexpr int kMaxHelpBarOffset = 4000; // pixels either way

    struct Settings
    {
        bool enabled       = true;
        float thickness    = 1.0f;  // render-target pixels
        int smoothness     = 4;     // shifted copies per mesh
        float maxDistance  = 50.0f; // yalms
        // Which elements and statuses get badges, as BadgeRules has them.
        int elementWeakRank   = BadgeRules{}.elementWeakRank;
        int elementResistRank = BadgeRules{}.elementResistRank;
        int maxElements       = BadgeRules{}.maxElements;
        StatusBadgeRule statusBadges[kStatusRankCount] = {}; // by StatusRank
        bool showRanks = false; // rank badges as ranks rather than the wikis' percents
        std::string badgeFontName = kBadgeFont;
        bool showBar       = true;
        BarTheme barTheme  = BarTheme::PhoenixGold;
        BarSeparators barSeparators = BarSeparators::Theme;
        int barHeight      = 45; // pixels: the least at which titles are 13 px over 11 px detail lines
        int barTextSize    = kAutoBarTextSize; // the bar's title size, or Auto
        std::string barTitleFont; // widget titles and popup headings; empty for the theme's
        std::string barTextFont;  // details, figures and popup text; empty for the theme's
        int popupTextSize  = kAutoPopupTextSize; // the popups' text size, or Auto
        std::array<bool, kBarWidgetCount> barWidgets = kBarWidgetDefaults; // by BarWidget
        int barCurrency = kGilCurrency; // the Gil widget's, by kCurrencyTable
        std::optional<ClockZone> clockZone;           // the Clock widget's; none for the computer's own
        std::vector<WindowOffset> windowOffsets; // each game window's move from where the game puts it
        std::vector<WindowGroup> windowGroups;   // windows moved together
        bool allUnderBar  = false; // every game window opening in the screen's upper half goes under the bar
        bool moveGameHelp = false; // the game's own help bar goes under the top bar, and the windows under it
        int helpBarX = 0, helpBarY = 0;          // back-buffer pixels from the help bar's place under the top bar
        bool helpBarBackground = true;           // off, the help text alone
        std::string menuFont = kDefaultMenuFont; // the settings window's text, by its kMenuFonts name
        int menuScale        = 100;              // percent: the settings window's text and everything sized by it
        MenuTheme menuTheme  = MenuTheme::Phoenix;
        // Where the player left the settings window, in pixels; no width for one never moved, which opens in the
        // screen's middle.
        int menuX = 0, menuY = 0, menuWidth = 0, menuHeight = 0;
        bool barTargetDetails = false; // a mob's icons after its name and its weaknesses after its level
        std::array<bool, kBarWidgetCount> barSecondLine = AllOn(); // by BarWidget: its second line
        std::array<bool, kBarWidgetCount> barIcons      = AllOn(); // by BarWidget: its icons and gil's coin
        ExpBarPlace expBar = ExpBarPlace::Under;
        JobLayout jobLayout = JobLayout::JobsFirst;
        SubtitlePlace barSubtitle = SubtitlePlace::After; // a widget's second line on a bar too short for two
        bool jobNames      = true;
        bool clock24       = true;  // the Clock widget's two lines
        bool clockSeconds  = false; // its own line's seconds
        bool clockServerTime = false; // the server's time under its own
        bool barLatency    = false; // the Connection widget's round trip after its rates
        BarPlacement barSections = DefaultPlacement(); // where each widget sits, by BarSection
        bool hideHelpBar = false; // skip the game's own help bar across the top of the screen
        bool hideNetstat = false; // and its connection readout (S, R and %) at the top right
        int screenshotKey        = 0;    // the virtual-key code that turns screenshot mode on and off, 0 for none
        bool screenshotHidesGame = true; // screenshot mode takes the game's own windows and chat too
        // The game's windows moved out from under Game help's bar, by short name (native_locate.h's ShortName).
        std::vector<std::string> movedWindows = DefaultMovedWindows();
        int untargetedOpacity = 100; // percent: other mobs' plates while you have a target
        bool glowOffWhenFighting = true; // a mob's lines lose their glow once you or your party has claimed it
        bool phTimers          = false; // above your name, a respawn timer for each placeholder you see die
        bool scaleWithDistance = true;  // sizes follow how far a name's owner is from you
        bool scaleOwnName      = true;  // your own plate too
        bool scaleCursor       = true;  // the target cursor's size follows the distance too, apart from the names
        std::vector<uint8_t> keepPointerKeys; // keys that set off a command without hiding the game's pointer
        bool replaceCursor     = true;  // our target cursor above the target's nameplate, instead of the game's
        bool cursorFeather     = true;  // Phoenix's feather instead of the arrow
        bool chocoboPointer    = false; // PlayOnline's chocobo in place of the game's mouse pointer
        bool fixPointer        = true;  // the game reads the mouse in its true client area and syncs its pointer before clicks
        int cursorSize         = 20;    // pixels tall
        int cursorOutline      = 50;    // percent of the outline's first thickness
        int nameRaise          = 6;     // pixels the names HeadsUp draws sit above the game's, with all above them
        bool stackNames        = false; // names HeadsUp draws that would cover each other rise over one another
        Color weakBadgeColor   = {{0.40f, 1.00f, 0.40f}};
        Color resistBadgeColor = {{1.00f, 0.35f, 0.35f}};
        Color rankBadgeColor   = {{1.00f, 0.85f, 0.30f}};
        Color immuneBadgeColor = {{1.00f, 0.25f, 0.25f}};
        bool nameplates = true;
        bool outlines   = true;
        int baseSize    = 15; // pixels: a line at 100 percent
        std::string defaultFont = kDefaultFont;
        int scaleMin = 50, scaleMax = 250; // percent: the distance scale's limits
        int timerPercent = 87;
        std::array<KindSettings, kPlateKindCount> kinds = DefaultKinds();
        Palette palette = DefaultPalette();
        uint32_t detectionShown = kAllDetection;
        std::vector<std::string> friends;
        Color cursorEdgeColor = ColorFromRgb(0x000000);
        const KindSettings& Kind(PlateKind k) const { return kinds[static_cast<int>(k)]; }
        KindSettings& Kind(PlateKind k) { return kinds[static_cast<int>(k)]; }
        bool operator==(const Settings&) const = default;
    };

    bool NameplatesOn(const Settings& s);
    // How game windows move: HeadsUp's way, every window under the bar in the groups the game opens them in, or the
    // game's, where it puts them. Each clears the windows' own offsets.
    void UseHeadsUpWindowDefaults(Settings& s);
    void UseGameWindowDefaults(Settings& s);
    // HeadsUp's help text in place of the game's help bar, which is hidden, and every window under it as the HeadsUp
    // defaults have them.
    void UseHelpTextDefaults(Settings& s);
    bool BarOn(const Settings& s);
    // IPlayer::GetLoginStatus() once the character is in the world; 0 and 1 are the title screen and character select.
    inline constexpr uint8_t kLoginInGame = 2;
    // The bar's settings are on and the character is in game.
    bool BarRuns(const Settings& s, uint8_t loginStatus);

    // The fonts to offer: the installed families (as GDI lists them, once per character set and with vertical variants
    // named @...), each once and sorted, with current added when it is not installed.
    std::vector<std::string> FontChoices(std::vector<std::string> installed, const std::string& current);

    // Key/value persistence: Ashita's configuration manager in the plugin, an in-memory map in tests.
    class SettingsStore
    {
    public:
        virtual ~SettingsStore()                                 = default;
        virtual bool GetBool(const char* key, bool fallback)     = 0;
        virtual float GetFloat(const char* key, float fallback)  = 0;
        virtual std::string GetString(const char* key, const char* fallback) = 0;
        virtual void Set(const char* key, const char* value)     = 0;
    };

    // Out-of-range values become the nearest limit; NaN and infinities become the default.
    Settings Clamp(Settings s);
    Settings LoadSettings(SettingsStore& store);
    void SaveSettings(const Settings& s, SettingsStore& store);

    // What a save has to tell the player: a failure once for a run of them, and the save that ends such a run.
    enum class SaveNotice : uint8_t { None, Failed, Saved };

    // A store that passes a Set on only when the key's text is not the text the store holds: a Set is Ashita writing
    // the key into the file, about a third of a millisecond, and a save makes one for every key.
    class ChangedKeys final : public SettingsStore
    {
    public:
        explicit ChangedKeys(SettingsStore& store)
            : m_Store(store)
        {}
        bool GetBool(const char* key, bool fallback) override { return m_Store.GetBool(key, fallback); }
        float GetFloat(const char* key, float fallback) override { return m_Store.GetFloat(key, fallback); }
        std::string GetString(const char* key, const char* fallback) override { return m_Store.GetString(key, fallback); }
        void Set(const char* key, const char* value) override;
        // LoadSettings, after asking the store for the text it holds for every key a save writes, so the next save
        // passes on exactly the keys the file lacks or holds otherwise than a save writes them.
        Settings Load();
        // Whether the file loaded is owed a save without waiting for a change: it holds settings, and lacks the
        // designs' marker or has an older build's version.
        bool Outdated() const { return m_Outdated; }
        // SaveSettings. Returns every key passed on that the file is not known to hold, with its latest text: those
        // of this save, and of earlier ones that ReadBack was not told the file holds.
        const std::vector<KeyText>& Save(const Settings& s);
        // After a save, whether the file as read back holds the keys Save returned: only then are they owed no more.
        SaveNotice ReadBack(bool fileHoldsThem);
        bool Unwritten() const { return !m_Unwritten.empty(); }

    private:
        SettingsStore& m_Store;
        std::unordered_map<std::string, std::string> m_Held; // each key's text as the store holds it
        std::vector<KeyText> m_Unwritten;
        bool m_Passing  = true; // off while Load lists the keys of a save
        bool m_Outdated = false;
        bool m_Failing  = false; // the last read-back did not hold the keys, which the player was told
    };

    // Opaque D3DCOLOR (A8R8G8B8) for D3DRS_TEXTUREFACTOR, from a color Clamp has kept in 0-1.
    uint32_t ToArgb(const Color& c);
}
