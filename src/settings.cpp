#include "settings.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace aggroglow
{
    namespace
    {
        const char* const kCategoryKeys[kCategoryCount] = {"willAttack", "wontAttack", "unknown"};
        // Colors are opaque: outline copies take alpha from the texture, where it shapes hair and cloth cut-outs.
        const char* const kChannelKeys[3]               = {"R", "G", "B"};

        float ClampFloat(float v, float lo, float hi, float fallback)
        {
            return std::isfinite(v) ? std::clamp(v, lo, hi) : fallback;
        }
    }

    Settings Clamp(Settings s)
    {
        const Settings d;
        s.thickness   = ClampFloat(s.thickness, 1.0f, 16.0f, d.thickness);
        s.smoothness  = std::clamp(s.smoothness, 4, 16);
        s.maxDistance = ClampFloat(s.maxDistance, 5.0f, 60.0f, d.maxDistance);
        for (int c = 0; c < kCategoryCount; ++c)
        {
            for (int i = 0; i < 3; ++i)
                s.color[c].v[i] = ClampFloat(s.color[c].v[i], 0.0f, 1.0f, d.color[c].v[i]);
            s.color[c].v[3] = 1.0f;
        }
        return s;
    }

    Settings LoadSettings(SettingsStore& store)
    {
        Settings s;
        s.enabled        = store.GetBool("enabled", s.enabled);
        s.thickness      = store.GetFloat("thickness", s.thickness);
        const float copies = ClampFloat(store.GetFloat("smoothness", static_cast<float>(s.smoothness)), 4.0f, 16.0f, static_cast<float>(s.smoothness));
        s.smoothness     = static_cast<int>(std::lround(copies));
        s.maxDistance    = store.GetFloat("maxDistance", s.maxDistance);
        s.modernConTable = store.GetBool("modernConTable", s.modernConTable);
        for (int c = 0; c < kCategoryCount; ++c)
        {
            const std::string base = kCategoryKeys[c];
            s.show[c] = store.GetBool((base + "Show").c_str(), s.show[c]);
            for (int i = 0; i < 3; ++i)
                s.color[c].v[i] = store.GetFloat((base + kChannelKeys[i]).c_str(), s.color[c].v[i]);
        }
        return Clamp(s);
    }

    void SaveSettings(const Settings& s, SettingsStore& store)
    {
        auto setBool  = [&](const std::string& key, bool v) { store.Set(key.c_str(), v ? "true" : "false"); };
        auto setFloat = [&](const std::string& key, float v) {
            char text[32];
            std::snprintf(text, sizeof(text), "%.4f", v);
            store.Set(key.c_str(), text);
        };
        setBool("enabled", s.enabled);
        setFloat("thickness", s.thickness);
        setFloat("smoothness", static_cast<float>(s.smoothness));
        setFloat("maxDistance", s.maxDistance);
        setBool("modernConTable", s.modernConTable);
        for (int c = 0; c < kCategoryCount; ++c)
        {
            const std::string base = kCategoryKeys[c];
            setBool(base + "Show", s.show[c]);
            for (int i = 0; i < 3; ++i)
                setFloat(base + kChannelKeys[i], s.color[c].v[i]);
        }
    }

    uint32_t ToArgb(const Color& c)
    {
        auto byte = [](float f) { return static_cast<uint32_t>(std::lround(ClampFloat(f, 0.0f, 1.0f, 0.0f) * 255.0f)); };
        return byte(c.v[3]) << 24 | byte(c.v[0]) << 16 | byte(c.v[1]) << 8 | byte(c.v[2]);
    }
}
