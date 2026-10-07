#pragma once

#include "classifier.h"
#include "labels.h"
#include "player_status.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace headsup
{
    // Opaque: outline copies take alpha from the mob's texture, where it shapes hair and cloth cut-outs.
    struct Color
    {
        float v[3]; // r, g, b in 0-1, the layout ImGui's ColorEdit3 edits
    };

    constexpr float kMinThickness = 1.0f, kMaxThickness = 16.0f;            // render-target pixels
    constexpr int kMinSmoothness = 4, kMaxSmoothness = 16;                  // shifted copies per mesh
    constexpr float kMinOutlineDistance = 5.0f, kMaxOutlineDistance = 60.0f; // yalms
    constexpr int kMinTextSize = 8, kMaxTextSize = 48;   // pixels: name, label, icon and cursor sizes
    constexpr int kMinNameRaise = 0, kMaxNameRaise = 40; // pixels
    constexpr int kMinPlayerIconSize = 50, kMaxPlayerIconSize = 150; // percent of the name's letter height
    constexpr const char* kDefaultFont = "Trebuchet MS";
    constexpr size_t kMaxFontName      = 31; // a Windows font name's longest, without its terminator

    struct Settings
    {
        bool enabled       = true;
        float thickness    = 1.0f;  // render-target pixels
        int smoothness     = 4;     // shifted copies per mesh
        float maxDistance  = 50.0f; // yalms
        bool showLabels    = true;  // level and con above every mob's name
        bool replaceMobNames   = true;  // hide the game's mob names and draw HeadsUp's
        bool showIcons         = true;  // the MobDB icon row
        bool hideInCombat      = false; // no level line or icons on a mob claimed by you or your party
        bool hideClaimed       = false; // nor on one anyone has claimed
        bool hideTooWeak       = false; // nor on one that cons Too Weak
        bool hideWhileEngaged  = false; // nor on any while you are engaged
        MobIdFormat mobId      = MobIdFormat::Off; // a mob's server ID on its level line
        bool markPlaceholders  = true;  // "[PH]" for a lottery placeholder's ID
        bool phTimers          = false; // above your name, a respawn timer for each placeholder you see die
        bool scaleWithDistance = true;  // sizes follow the game's name size, like the game's names do
        std::string fontName   = kDefaultFont; // a font family installed in Windows
        bool fontBold          = false;
        int nameSize           = 15;    // pixels
        int labelSize          = 13;    // pixels
        int timerSize          = 13;    // pixels: your placeholder timers' lines
        int iconSize           = 16;    // pixels
        int playerIconSize     = 100;   // percent of the name's letter height
        bool replacePlayerNames = true; // players' names, you included, in the font below
        bool replaceNpcNames   = true;
        bool replaceCursor     = true;  // our target cursor above the target's nameplate, instead of the game's
        bool cursorFeather     = true;  // Phoenix's feather instead of the arrow
        bool chocoboPointer    = false; // PlayOnline's chocobo in place of the game's mouse pointer
        bool showPlayerIcons   = true;  // seeking party, bazaar, linkshell and the rest beside replaced player names
        IconSide playerIconSide[kPlayerIconCount] = {}; // by PlayerIcon: every one left of the name
        bool centerNameAndIcons = true;  // a player's name and icons centered together over them, rather than the name
        Color cursorColor      = {{1.00f, 1.00f, 1.00f}}; // the target: white
        Color lockedCursorColor = {{0.65f, 0.40f, 1.00f}}; // locked on: purple
        Color subCursorColor   = {{0.99f, 0.82f, 0.09f}}; // the sub-target: gold, like XIUI's sub-target tint
        Color outOfRangeCursorColor = {{1.00f, 0.25f, 0.25f}}; // the sub-target out of range: red, as the game shows it
        int cursorSize         = 20;    // pixels tall
        int nameRaise          = 6;     // pixels the names HeadsUp draws sit above the game's, with all above them
        bool ownNameColor      = false; // names in nameColor instead of the game's color
        Color nameColor        = {{1.00f, 1.00f, 1.00f}};
        Color labelColor[kLabelShadeCount] = {
            {{0.60f, 0.60f, 0.60f}}, // unknown: gray
            {{0.60f, 0.60f, 0.60f}}, // Too Weak: gray
            {{0.40f, 1.00f, 0.40f}}, // Easy Prey: green
            {{0.45f, 0.70f, 1.00f}}, // Decent Challenge: blue
            {{1.00f, 1.00f, 1.00f}}, // Even Match: white
            {{1.00f, 1.00f, 0.35f}}, // Tough: yellow
            {{1.00f, 0.35f, 0.35f}}, // Very Tough: red
        };
        Color textOutline = {{0.00f, 0.00f, 0.00f}};
        Color iconTint    = {{1.00f, 1.00f, 1.00f}}; // white keeps the icons' own colors
        bool show[kCategoryCount]   = {true, false, false, true, true, true}; // indexed by Category: no passive or unknown
        Color color[kCategoryCount] = {
            {{1.00f, 0.15f, 0.15f}}, // aggressive: red
            {{0.20f, 1.00f, 0.30f}}, // passive: green
            {{0.70f, 0.70f, 0.70f}}, // unknown: gray
            {{1.00f, 0.60f, 0.10f}}, // aggressive NM: gold-orange
            {{1.00f, 0.84f, 0.00f}}, // passive NM: gold
            {{0.70f, 0.30f, 1.00f}}, // lottery placeholder: purple
        };
    };

    // Whether anything of the nameplate is drawn: the master switch, and labels, icons, names or the cursor.
    bool NameplatesOn(const Settings& s);
    // Whether HeadsUp draws any kind of name (mobs, players or NPCs), hiding the game's.
    bool ReplacesNames(const Settings& s);

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
