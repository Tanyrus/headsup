#pragma once

#include "argb.h"
#include "bar_placement.h"
#include "bar_server_time.h"
#include "bar_types.h"
#include "currencies.h"
#include "fonts.h"
#include "labels.h"
#include "menu_fonts.h"
#include "native_locate.h"
#include "player_status.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace headsup
{
    enum class Category : uint8_t
    {
        WillAttack,
        WontAttack,
        Unknown,
        NmWillAttack,
        NmWontAttack,
        Placeholder, // a lottery placeholder, whatever it would be, while placeholders are shown in their own color
    };
    constexpr int kCategoryCount = static_cast<int>(Category::Placeholder) + 1;
    constexpr int CategoryIndex(Category category) { return static_cast<int>(category); }

    enum class Look : uint8_t
    {
        Fantasy,
    };
    constexpr int kLookCount = static_cast<int>(Look::Fantasy) + 1;

    // Opaque: outline copies take alpha from the mob's texture, where it shapes hair and cloth cut-outs.
    struct Color
    {
        float v[3]; // r, g, b in 0-1, the layout ImGui's ColorEdit3 edits
        bool operator==(const Color&) const = default;
    };
    // From 0xRRGGBB, exactly: ToArgb gives back the same bytes.
    constexpr Color ColorFromRgb(uint32_t rgb)
    {
        constexpr float kFull = 255.0f;
        return {{Channel(rgb, kRedShift) / kFull, Channel(rgb, kGreenShift) / kFull, Channel(rgb, kBlueShift) / kFull}};
    }
    // Phoenix's royal red, the accent of its own theme (phoenix-platform's --phoenix-accent).
    constexpr uint32_t kPhoenixRed = 0xC55151;

    constexpr float kMinThickness = 1.0f, kMaxThickness = 16.0f;            // render-target pixels
    constexpr int kMinSmoothness = 4, kMaxSmoothness = 16;                  // shifted copies per mesh
    constexpr float kMinOutlineDistance = 5.0f, kMaxOutlineDistance = 60.0f; // yalms
    constexpr int kMinTextSize = 8, kMaxTextSize = 48;   // pixels: name, label, icon and cursor sizes
    constexpr int kMinNameRaise = 0, kMaxNameRaise = 40; // pixels
    constexpr int kMinPlayerIconSize = 50, kMaxPlayerIconSize = 150; // percent of the name's letter height
    constexpr int kMinGlowStrength = 25, kMaxGlowStrength = 300;     // percent of the mockup's glow
    constexpr int kMinGlowSize = 25, kMaxGlowSize = 200;             // percent: a wider glow takes longer to draw
    constexpr int kMinShadowStrength = 0, kMaxShadowStrength = 200;  // percent of the mockup's shadow
    constexpr int kMinOrnamentWidth = 20, kMaxOrnamentWidth = 300;   // pixels
    constexpr int kMinOrnamentThickness = 3, kMaxOrnamentThickness = 24; // pixels
    constexpr const char* kDefaultFont = kMarcellusSC;
    constexpr const char* kBadgeFont   = kBarlowCondensed; // plain enough to read a few pixels tall
    constexpr size_t kMaxFontName      = 31; // a Windows font name's longest, without its terminator

    constexpr std::array<bool, kBarWidgetCount> AllOn()
    {
        std::array<bool, kBarWidgetCount> on{};
        on.fill(true);
        return on;
    }

    constexpr int kMinMenuScale = 70, kMaxMenuScale = 160; // percent

    constexpr int kMaxHelpBarOffset = 4000; // pixels either way

    struct Settings
    {
        bool enabled       = true;
        float thickness    = 1.0f;  // render-target pixels
        int smoothness     = 4;     // shifted copies per mesh
        float maxDistance  = 50.0f; // yalms
        bool showLabels    = true;  // level and con above every mob's name
        bool replaceMobNames   = true;
        bool showIcons         = true;  // the MobDB icon row
        bool showDamageIcons     = false; // the badge group, off since the top bar's popup shows the same: weaknesses
        bool showStatusRankIcons = false; // and resistances to damage types and elements; statuses by statusBadges
        bool showImmunityIcons   = false;
        // Each group's side of the plate's center; a side both share has the detection icons nearest the center.
        IconSide detectionIconSide = IconSide::Left;
        IconSide badgeSide         = IconSide::Right;
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
        ClockZone serverClockZone = ClockZone::Japan; // the Server time widget's clock
        std::vector<WindowOffset> windowOffsets; // each game window's move from where the game puts it
        std::vector<WindowGroup> windowGroups;   // windows moved together
        bool allUnderBar  = false; // every game window opening in the screen's upper half goes under the bar
        bool moveGameHelp = false; // the game's own help bar goes under the top bar, and the windows under it
        int helpBarX = 0, helpBarY = 0;          // back-buffer pixels from the help bar's place under the top bar
        bool helpBarBackground = true;           // off, the help text alone
        std::string menuFont = kDefaultMenuFont; // the settings window's text, by its kMenuFonts name
        int menuScale        = 100;              // percent: the settings window's text and everything sized by it
        bool barTargetDetails = false; // a mob's icons after its name and its weaknesses after its level
        std::array<bool, kBarWidgetCount> barSecondLine = AllOn(); // by BarWidget: its second line
        std::array<bool, kBarWidgetCount> barIcons      = AllOn(); // by BarWidget: its icons and gil's coin
        ExpBarPlace expBar = ExpBarPlace::Under;
        SubtitlePlace barSubtitle = SubtitlePlace::After; // a widget's second line on a bar too short for two
        bool jobNames      = true;
        bool clock24       = true; // the Clock and Server time widgets
        bool clockSeconds  = false; // the Clock widget's seconds
        bool serverSeconds = false; // and Server time's
        bool barLatency    = false; // the Connection widget's round trip after its rates
        BarPlacement barSections = DefaultPlacement(); // where each widget sits, by BarSection
        bool hideHelpBar = false; // skip the game's own help bar across the top of the screen
        bool hideNetstat = false; // and its connection readout (S, R and %) at the top right
        // The game's windows moved out from under Game help's bar, by short name (native_locate.h's ShortName).
        std::vector<std::string> movedWindows = DefaultMovedWindows();
        bool hideClaimedByParty = false; // no level line or icons on a mob claimed by you or your party
        bool hideClaimed       = false; // nor on one anyone has claimed
        bool hideTooWeak       = false; // nor on one that cons Too Weak
        bool hideWhileEngaged  = false; // nor on any while you are engaged
        MobIdFormat mobId      = MobIdFormat::Off; // a mob's server ID on its level line
        bool markPlaceholders  = true;  // "[PH]" for a lottery placeholder's ID
        bool phTimers          = false; // above your name, a respawn timer for each placeholder you see die
        bool scaleWithDistance = true;  // sizes follow the game's name size
        bool scaleOwnName      = true;  // your own plate too
        bool scalePlayerNames  = true;  // other players' plates too
        bool scaleCursor       = true;  // the target cursor's size follows the game's name size, apart from the names
        Look look              = Look::Fantasy; // which style the plates are drawn in
        std::vector<uint8_t> keepPointerKeys; // keys that set off a command without hiding the game's pointer
        std::string fontName   = kDefaultFont; // a font family installed in Windows
        bool fontBold          = false;
        bool nameGlow          = true;  // a mob's name glows in its outline color
        bool glowOffWhenFighting = true; // but not once you or your party has claimed it
        int glowStrength       = 100;   // percent of the mockup's glow
        int glowSize           = 100;   // percent of the mockup's glow
        bool ownGlowColor      = false; // the glow in glowColor instead of the outline color
        Color glowColor        = ColorFromRgb(0xFFB45A); // the mockup's warm glow
        bool showOrnament      = true;  // between a mob's level line and its name
        int ornamentWidth      = 90;    // pixels
        int ornamentThickness  = 6;     // pixels
        bool ownOrnamentColor  = false; // the ornament in ornamentColor instead of the outline color
        Color ornamentColor    = ColorFromRgb(0xD9B46A); // the mockup's gold
        int nameShadow         = 100;   // percent of the mockup's shadow; textOutline is its color
        std::string labelFontName = kDefaultFont; // the level line and the timers
        bool labelFontBold     = false;
        Color labelShadowColor = {{0.00f, 0.00f, 0.00f}};
        int labelShadow        = 100;   // percent of the mockup's shadow
        int mobNameSize        = 15;    // pixels
        int playerNameSize     = 15;    // pixels: other players
        int selfNameSize       = 15;    // pixels: you
        int npcNameSize        = 15;    // pixels
        int labelSize          = 13;    // pixels
        int timerSize          = 13;    // pixels: your placeholder timers' lines
        int iconSize           = 16;    // pixels
        int playerIconSize     = 100;   // percent of the name's letter height
        bool replacePlayerNames = true; // you included
        bool replaceNpcNames   = true;
        bool replaceCursor     = true;  // our target cursor above the target's nameplate, instead of the game's
        bool cursorFeather     = true;  // Phoenix's feather instead of the arrow
        bool chocoboPointer    = false; // PlayOnline's chocobo in place of the game's mouse pointer
        bool fixPointer        = true;  // the game reads the mouse in its true client area and syncs its pointer before clicks
        bool showPlayerIcons   = true;  // seeking party, bazaar, linkshell and the rest beside replaced player names
        IconSide playerIconSide[kPlayerIconCount] = {}; // by PlayerIcon: every one left of the name
        bool centerNameAndIcons = true;  // a player's name and icons centered together over them, rather than the name
        Color cursorColor      = ColorFromRgb(kPhoenixRed); // the target
        Color lockedCursorColor = {{0.65f, 0.40f, 1.00f}};
        Color subCursorColor   = {{0.99f, 0.82f, 0.09f}}; // like XIUI's sub-target tint
        Color outOfRangeCursorColor = {{1.00f, 0.25f, 0.25f}}; // the sub-target out of range, red as the game shows it
        int cursorSize         = 20;    // pixels tall
        int nameRaise          = 6;     // pixels the names HeadsUp draws sit above the game's, with all above them
        bool ownNameColor      = false; // names in nameColor instead of the game's color
        Color nameColor        = {{1.00f, 1.00f, 1.00f}};
        Color labelColor[kLabelShadeCount] = {
            {{0.60f, 0.60f, 0.60f}}, // unknown
            {{0.60f, 0.60f, 0.60f}}, // Too Weak
            {{0.40f, 1.00f, 0.40f}}, // Easy Prey
            {{0.45f, 0.70f, 1.00f}}, // Decent Challenge
            {{1.00f, 1.00f, 1.00f}}, // Even Match
            {{1.00f, 1.00f, 0.35f}}, // Tough
            {{1.00f, 0.35f, 0.35f}}, // Very Tough
        };
        Color textOutline = {{0.00f, 0.00f, 0.00f}};
        Color iconTint    = {{1.00f, 1.00f, 1.00f}}; // white keeps the icons' own colors
        Color weakBadgeColor   = {{0.40f, 1.00f, 0.40f}};
        Color resistBadgeColor = {{1.00f, 0.35f, 0.35f}};
        Color rankBadgeColor   = {{1.00f, 0.85f, 0.30f}};
        Color immuneBadgeColor = {{1.00f, 0.25f, 0.25f}};
        bool show[kCategoryCount]   = {true, false, false, true, true, true}; // indexed by Category: no passive or unknown
        Color color[kCategoryCount] = {
            {{1.00f, 0.15f, 0.15f}}, // aggressive
            {{0.20f, 1.00f, 0.30f}}, // passive
            {{0.70f, 0.70f, 0.70f}}, // unknown
            {{1.00f, 0.60f, 0.10f}}, // aggressive NM
            {{1.00f, 0.84f, 0.00f}}, // passive NM
            {{0.70f, 0.30f, 1.00f}}, // lottery placeholder
        };
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
    bool ReplacesAnyName(const Settings& s);

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

    // Opaque D3DCOLOR (A8R8G8B8) for D3DRS_TEXTUREFACTOR, from a color Clamp has kept in 0-1.
    uint32_t ToArgb(const Color& c);
}
