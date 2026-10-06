#pragma once

#include "classifier.h"
#include "labels.h"

#include <cstdint>

namespace aggroglow
{
    // Opaque: outline copies take alpha from the mob's texture, where it shapes hair and cloth cut-outs.
    struct Color
    {
        float v[3]; // r, g, b in 0-1, the layout ImGui's ColorEdit3 edits
    };

    constexpr float kMinThickness = 1.0f, kMaxThickness = 16.0f;            // render-target pixels
    constexpr int kMinSmoothness = 4, kMaxSmoothness = 16;                  // shifted copies per mesh
    constexpr float kMinOutlineDistance = 5.0f, kMaxOutlineDistance = 60.0f; // yalms
    constexpr int kMinTextSize = 8, kMaxTextSize = 48;                      // pixels: name, label and icon sizes

    struct Settings
    {
        bool enabled       = true;
        float thickness    = 4.0f;  // render-target pixels
        int smoothness     = 8;     // shifted copies per mesh
        float maxDistance  = 40.0f; // yalms
        bool showLabels    = true;  // level and con above every mob's name
        bool autoExamine   = false; // /check the targeted mob once per respawn when it would give exp
        bool replaceNameplates = false; // hide the game's mob nameplates and draw AggroGlow's (with the name)
        bool showIcons         = true;  // the MobDB icon row
        bool scaleWithDistance = false; // sizes follow the game's name size, like the game's names do
        int fontIndex          = 0;     // into FontFamily's list
        bool fontBold          = true;
        int nameSize           = 15;    // pixels
        int labelSize          = 13;    // pixels
        int iconSize           = 16;    // pixels
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
        bool show[kCategoryCount]   = {true, true, true, true, true}; // indexed by Category
        Color color[kCategoryCount] = {
            {{1.00f, 0.15f, 0.15f}}, // will attack: red
            {{0.20f, 1.00f, 0.30f}}, // won't attack: green
            {{0.70f, 0.70f, 0.70f}}, // unknown: gray
            {{1.00f, 0.60f, 0.10f}}, // NM will attack: gold-orange
            {{1.00f, 0.84f, 0.00f}}, // NM won't attack: gold
        };
    };

    // Whether anything of the nameplate is drawn: the master switch, and labels, icons or the replacement.
    bool NameplatesOn(const Settings& s);

    constexpr int kFontCount = 6;
    // Arial, Tahoma, Verdana, Trebuchet MS, Courier New, Times New Roman; Arial for anything out of range.
    const char* FontFamily(int index);

    // Key/value persistence: Ashita's configuration manager in the plugin, an in-memory map in tests.
    class SettingsStore
    {
    public:
        virtual ~SettingsStore()                                 = default;
        virtual bool GetBool(const char* key, bool fallback)     = 0;
        virtual float GetFloat(const char* key, float fallback)  = 0;
        virtual void Set(const char* key, const char* value)     = 0;
    };

    // Out-of-range values become the nearest limit; NaN and infinities become the default.
    Settings Clamp(Settings s);
    Settings LoadSettings(SettingsStore& store);
    void SaveSettings(const Settings& s, SettingsStore& store);

    // Opaque D3DCOLOR (A8R8G8B8) for D3DRS_TEXTUREFACTOR.
    uint32_t ToArgb(const Color& c);
}
