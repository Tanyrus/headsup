#pragma once

#include "argb.h"
#include "labels.h"
#include "player_status.h"

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
    constexpr const char* kDefaultFont = "Marcellus SC";
    constexpr size_t kMaxFontName      = 31; // a Windows font name's longest, without its terminator

    struct Settings
    {
        bool enabled       = true;
        float thickness    = 1.0f;  // render-target pixels
        int smoothness     = 4;     // shifted copies per mesh
        float maxDistance  = 50.0f; // yalms
        bool showLabels    = true;  // level and con above every mob's name
        bool replaceMobNames   = true;
        bool showIcons         = true;  // the MobDB icon row
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
