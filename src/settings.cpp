#include "settings.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>

namespace headsup
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

        // Fonts were once saved by their place in this list.
        const char* const kOldFonts[] = {"Arial", "Tahoma", "Verdana", "Trebuchet MS", "Courier New", "Times New Roman"};
        constexpr int kOldFontCount   = static_cast<int>(sizeof(kOldFonts) / sizeof(kOldFonts[0]));

        std::string LoadFontName(SettingsStore& store)
        {
            const int old = LoadInt(store, "fontIndex", -1, -1, kOldFontCount);
            return store.GetString("fontName", old >= 0 && old < kOldFontCount ? kOldFonts[old] : kDefaultFont);
        }

        bool SameIgnoringCase(const std::string& a, const std::string& b)
        {
            return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
                return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
            });
        }
    }

    bool ReplacesNames(const Settings& s)
    {
        return s.enabled && (s.replaceNameplates || s.replacePlayerNames || s.replaceNpcNames);
    }

    bool NameplatesOn(const Settings& s)
    {
        return s.enabled && (s.showLabels || s.showIcons || s.replaceNameplates || s.replacePlayerNames || s.replaceNpcNames ||
                                s.replaceCursor);
    }

    std::vector<std::string> FontChoices(std::vector<std::string> installed, const std::string& current)
    {
        installed.push_back(current);
        installed.erase(std::remove_if(installed.begin(), installed.end(),
                            [](const std::string& name) { return name.empty() || name[0] == '@'; }),
            installed.end());
        auto lower = [](const std::string& name) {
            std::string l = name;
            for (char& ch : l)
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            return l;
        };
        std::sort(installed.begin(), installed.end(), [&](const std::string& a, const std::string& b) { return lower(a) < lower(b); });
        installed.erase(std::unique(installed.begin(), installed.end(), SameIgnoringCase), installed.end());
        return installed;
    }

    Settings Clamp(Settings s)
    {
        const Settings d;
        s.thickness   = ClampFloat(s.thickness, kMinThickness, kMaxThickness, d.thickness);
        s.smoothness  = std::clamp(s.smoothness, kMinSmoothness, kMaxSmoothness);
        s.maxDistance = ClampFloat(s.maxDistance, kMinOutlineDistance, kMaxOutlineDistance, d.maxDistance);
        if (s.fontName.empty() || s.fontName.size() > kMaxFontName) s.fontName = d.fontName;
        s.nameSize    = std::clamp(s.nameSize, kMinTextSize, kMaxTextSize);
        s.labelSize   = std::clamp(s.labelSize, kMinTextSize, kMaxTextSize);
        s.iconSize    = std::clamp(s.iconSize, kMinTextSize, kMaxTextSize);
        s.cursorSize  = std::clamp(s.cursorSize, kMinTextSize, kMaxTextSize);
        s.nameRaise   = std::clamp(s.nameRaise, kMinNameRaise, kMaxNameRaise);
        s.nameColor = ClampColor(s.nameColor, d.nameColor);
        for (int k = 0; k < kLabelShadeCount; ++k)
            s.labelColor[k] = ClampColor(s.labelColor[k], d.labelColor[k]);
        s.textOutline = ClampColor(s.textOutline, d.textOutline);
        s.iconTint    = ClampColor(s.iconTint, d.iconTint);
        s.cursorColor = ClampColor(s.cursorColor, d.cursorColor);
        s.subCursorColor = ClampColor(s.subCursorColor, d.subCursorColor);
        s.lockedCursorColor = ClampColor(s.lockedCursorColor, d.lockedCursorColor);
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
        s.replaceNameplates = store.GetBool("replaceNameplates", s.replaceNameplates);
        s.showIcons         = store.GetBool("showIcons", s.showIcons);
        s.scaleWithDistance = store.GetBool("scaleWithDistance", s.scaleWithDistance);
        s.fontName          = LoadFontName(store);
        s.fontBold          = store.GetBool("fontBold", s.fontBold);
        s.nameSize          = LoadInt(store, "nameSize", s.nameSize, kMinTextSize, kMaxTextSize);
        s.labelSize         = LoadInt(store, "labelSize", s.labelSize, kMinTextSize, kMaxTextSize);
        s.iconSize          = LoadInt(store, "iconSize", s.iconSize, kMinTextSize, kMaxTextSize);
        s.replacePlayerNames = store.GetBool("replacePlayerNames", s.replacePlayerNames);
        s.replaceNpcNames   = store.GetBool("replaceNpcNames", s.replaceNpcNames);
        s.replaceCursor     = store.GetBool("replaceCursor", s.replaceCursor);
        s.cursorFeather     = store.GetBool("cursorFeather", s.cursorFeather);
        s.showPlayerIcons   = store.GetBool("showPlayerIcons", s.showPlayerIcons);
        s.centerNameAndIcons = store.GetBool("centerNameAndIcons", s.centerNameAndIcons);
        s.cursorSize        = LoadInt(store, "cursorSize", s.cursorSize, kMinTextSize, kMaxTextSize);
        s.nameRaise         = LoadInt(store, "nameRaise", s.nameRaise, kMinNameRaise, kMaxNameRaise);
        LoadColor(store, "cursorColor", s.cursorColor);
        LoadColor(store, "subCursorColor", s.subCursorColor);
        LoadColor(store, "lockedCursorColor", s.lockedCursorColor);
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
        setBool("replaceNameplates", s.replaceNameplates);
        setBool("showIcons", s.showIcons);
        setBool("scaleWithDistance", s.scaleWithDistance);
        store.Set("fontName", s.fontName.c_str());
        setBool("fontBold", s.fontBold);
        setFloat("nameSize", static_cast<float>(s.nameSize));
        setFloat("labelSize", static_cast<float>(s.labelSize));
        setFloat("iconSize", static_cast<float>(s.iconSize));
        auto setColor = [&](const std::string& base, const Color& c) {
            for (int i = 0; i < 3; ++i)
                setFloat(base + kChannelKeys[i], c.v[i]);
        };
        setBool("replacePlayerNames", s.replacePlayerNames);
        setBool("replaceNpcNames", s.replaceNpcNames);
        setBool("replaceCursor", s.replaceCursor);
        setBool("cursorFeather", s.cursorFeather);
        setBool("showPlayerIcons", s.showPlayerIcons);
        setBool("centerNameAndIcons", s.centerNameAndIcons);
        setFloat("cursorSize", static_cast<float>(s.cursorSize));
        setFloat("nameRaise", static_cast<float>(s.nameRaise));
        setColor("cursorColor", s.cursorColor);
        setColor("subCursorColor", s.subCursorColor);
        setColor("lockedCursorColor", s.lockedCursorColor);
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
