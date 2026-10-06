#include "settings.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace aggroglow
{
    namespace
    {
        const char* const kCategoryKeys[kCategoryCount] = {"willAttack", "wontAttack", "unknown", "nmWillAttack", "nmWontAttack"};
        const char* const kChannelKeys[3]               = {"R", "G", "B"};
        const char* const kLabelShadeKeys[kLabelShadeCount] = {"labelUnknown", "labelTooWeak", "labelEasyPrey",
            "labelDecentChallenge", "labelEvenMatch", "labelTough", "labelVeryTough"};

        float ClampFloat(float v, float lo, float hi, float fallback)
        {
            return std::isfinite(v) ? std::clamp(v, lo, hi) : fallback;
        }

        // Whole numbers are stored as floats, like every other value in the file.
        Color ClampColor(Color c, const Color& fallback)
        {
            for (int i = 0; i < 3; ++i)
                c.v[i] = ClampFloat(c.v[i], 0.0f, 1.0f, fallback.v[i]);
            return c;
        }

        void LoadColor(SettingsStore& store, const std::string& base, Color& c)
        {
            for (int i = 0; i < 3; ++i)
                c.v[i] = store.GetFloat((base + kChannelKeys[i]).c_str(), c.v[i]);
        }

        int LoadInt(SettingsStore& store, const char* key, int fallback, int lo, int hi)
        {
            const float v = store.GetFloat(key, static_cast<float>(fallback));
            return std::isfinite(v) ? std::clamp(static_cast<int>(std::lround(v)), lo, hi) : fallback;
        }

        const char* const kFontFamilies[kFontCount] = {"Arial", "Tahoma", "Verdana", "Trebuchet MS", "Courier New",
            "Times New Roman"};
    }

    bool NameplatesOn(const Settings& s)
    {
        return s.enabled && (s.showLabels || s.showIcons || s.replaceNameplates);
    }

    const char* FontFamily(int index)
    {
        return index >= 0 && index < kFontCount ? kFontFamilies[index] : kFontFamilies[0];
    }

    Settings Clamp(Settings s)
    {
        const Settings d;
        s.thickness   = ClampFloat(s.thickness, kMinThickness, kMaxThickness, d.thickness);
        s.smoothness  = std::clamp(s.smoothness, kMinSmoothness, kMaxSmoothness);
        s.maxDistance = ClampFloat(s.maxDistance, kMinOutlineDistance, kMaxOutlineDistance, d.maxDistance);
        s.fontIndex   = std::clamp(s.fontIndex, 0, kFontCount - 1);
        s.nameSize    = std::clamp(s.nameSize, kMinTextSize, kMaxTextSize);
        s.labelSize   = std::clamp(s.labelSize, kMinTextSize, kMaxTextSize);
        s.iconSize    = std::clamp(s.iconSize, kMinTextSize, kMaxTextSize);
        s.nameColor = ClampColor(s.nameColor, d.nameColor);
        for (int k = 0; k < kLabelShadeCount; ++k)
            s.labelColor[k] = ClampColor(s.labelColor[k], d.labelColor[k]);
        s.textOutline = ClampColor(s.textOutline, d.textOutline);
        s.iconTint    = ClampColor(s.iconTint, d.iconTint);
        for (int c = 0; c < kCategoryCount; ++c)
            s.color[c] = ClampColor(s.color[c], d.color[c]);
        return s;
    }

    Settings LoadSettings(SettingsStore& store)
    {
        Settings s;
        s.enabled        = store.GetBool("enabled", s.enabled);
        s.thickness      = store.GetFloat("thickness", s.thickness);
        s.smoothness     = LoadInt(store, "smoothness", s.smoothness, kMinSmoothness, kMaxSmoothness);
        s.maxDistance    = store.GetFloat("maxDistance", s.maxDistance);
        s.showLabels     = store.GetBool("showLabels", s.showLabels);
        s.autoExamine    = store.GetBool("autoExamine", s.autoExamine);
        s.replaceNameplates = store.GetBool("replaceNameplates", s.replaceNameplates);
        s.showIcons         = store.GetBool("showIcons", s.showIcons);
        s.scaleWithDistance = store.GetBool("scaleWithDistance", s.scaleWithDistance);
        s.fontIndex         = LoadInt(store, "fontIndex", s.fontIndex, 0, kFontCount - 1);
        s.fontBold          = store.GetBool("fontBold", s.fontBold);
        s.nameSize          = LoadInt(store, "nameSize", s.nameSize, kMinTextSize, kMaxTextSize);
        s.labelSize         = LoadInt(store, "labelSize", s.labelSize, kMinTextSize, kMaxTextSize);
        s.iconSize          = LoadInt(store, "iconSize", s.iconSize, kMinTextSize, kMaxTextSize);
        s.ownNameColor      = store.GetBool("ownNameColor", s.ownNameColor);
        LoadColor(store, "nameColor", s.nameColor);
        for (int k = 0; k < kLabelShadeCount; ++k)
            LoadColor(store, kLabelShadeKeys[k], s.labelColor[k]);
        LoadColor(store, "textOutline", s.textOutline);
        LoadColor(store, "iconTint", s.iconTint);
        for (int c = 0; c < kCategoryCount; ++c)
        {
            const std::string base = kCategoryKeys[c];
            s.show[c] = store.GetBool((base + "Show").c_str(), s.show[c]);
            LoadColor(store, base, s.color[c]);
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
        setBool("showLabels", s.showLabels);
        setBool("autoExamine", s.autoExamine);
        setBool("replaceNameplates", s.replaceNameplates);
        setBool("showIcons", s.showIcons);
        setBool("scaleWithDistance", s.scaleWithDistance);
        setFloat("fontIndex", static_cast<float>(s.fontIndex));
        setBool("fontBold", s.fontBold);
        setFloat("nameSize", static_cast<float>(s.nameSize));
        setFloat("labelSize", static_cast<float>(s.labelSize));
        setFloat("iconSize", static_cast<float>(s.iconSize));
        auto setColor = [&](const std::string& base, const Color& c) {
            for (int i = 0; i < 3; ++i)
                setFloat(base + kChannelKeys[i], c.v[i]);
        };
        setBool("ownNameColor", s.ownNameColor);
        setColor("nameColor", s.nameColor);
        for (int k = 0; k < kLabelShadeCount; ++k)
            setColor(kLabelShadeKeys[k], s.labelColor[k]);
        setColor("textOutline", s.textOutline);
        setColor("iconTint", s.iconTint);
        for (int c = 0; c < kCategoryCount; ++c)
        {
            const std::string base = kCategoryKeys[c];
            setBool(base + "Show", s.show[c]);
            setColor(base, s.color[c]);
        }
    }

    uint32_t ToArgb(const Color& c)
    {
        auto byte = [](float f) { return static_cast<uint32_t>(std::lround(ClampFloat(f, 0.0f, 1.0f, 0.0f) * 255.0f)); };
        return 0xFF000000u | byte(c.v[0]) << 16 | byte(c.v[1]) << 8 | byte(c.v[2]);
    }
}
