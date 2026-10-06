#pragma once

#include "classifier.h"

#include <cstdint>

namespace aggroglow
{
    struct Color
    {
        float v[4]; // r, g, b, a in 0-1 (the layout ImGui's ColorEdit4 edits)
    };

    struct Settings
    {
        bool enabled       = true;
        float thickness    = 4.0f;  // render-target pixels, 1-16
        int smoothness     = 8;     // shifted copies per mesh, 4-16
        float maxDistance  = 40.0f; // yalms, 5-60
        bool modernConTable = false;
        bool show[kCategoryCount]   = {true, true, true}; // indexed by Category
        Color color[kCategoryCount] = {{{1.00f, 0.15f, 0.15f, 1.0f}}, {{0.20f, 1.00f, 0.30f, 1.0f}}, {{0.70f, 0.70f, 0.70f, 1.0f}}};
    };

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

    // D3DCOLOR (A8R8G8B8) for D3DRS_TEXTUREFACTOR.
    uint32_t ToArgb(const Color& c);
}
