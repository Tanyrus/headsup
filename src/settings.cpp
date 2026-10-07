#include "settings.h"

#include "argb.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdio>
#include <string>

namespace headsup
{
    namespace
    {
        // Every setting's key in config/headsup/settings.ini. Whole numbers are stored as floats, like every other
        // number in the file.
        struct BoolKey
        {
            const char* key;
            bool Settings::*field;
        };
        constexpr BoolKey kBools[] = {{"enabled", &Settings::enabled}, {"showLabels", &Settings::showLabels},
            {"replaceMobNames", &Settings::replaceMobNames}, {"showIcons", &Settings::showIcons},
            {"hideInCombat", &Settings::hideInCombat}, {"hideClaimed", &Settings::hideClaimed},
            {"hideTooWeak", &Settings::hideTooWeak}, {"hideWhileEngaged", &Settings::hideWhileEngaged},
            {"markPlaceholders", &Settings::markPlaceholders}, {"phTimers", &Settings::phTimers},
            {"scaleWithDistance", &Settings::scaleWithDistance}, {"fontBold", &Settings::fontBold},
            {"replacePlayerNames", &Settings::replacePlayerNames}, {"replaceNpcNames", &Settings::replaceNpcNames},
            {"replaceCursor", &Settings::replaceCursor}, {"cursorFeather", &Settings::cursorFeather},
            {"chocoboPointer", &Settings::chocoboPointer},
            {"showPlayerIcons", &Settings::showPlayerIcons}, {"centerNameAndIcons", &Settings::centerNameAndIcons},
            {"ownNameColor", &Settings::ownNameColor}};

        struct IntKey
        {
            const char* key;
            int Settings::*field;
            int lo, hi;
        };
        constexpr IntKey kInts[] = {{"smoothness", &Settings::smoothness, kMinSmoothness, kMaxSmoothness},
            {"nameSize", &Settings::nameSize, kMinTextSize, kMaxTextSize},
            {"labelSize", &Settings::labelSize, kMinTextSize, kMaxTextSize},
            {"timerSize", &Settings::timerSize, kMinTextSize, kMaxTextSize},
            {"iconSize", &Settings::iconSize, kMinTextSize, kMaxTextSize},
            {"cursorSize", &Settings::cursorSize, kMinTextSize, kMaxTextSize},
            {"nameRaise", &Settings::nameRaise, kMinNameRaise, kMaxNameRaise},
            {"playerIconSize", &Settings::playerIconSize, kMinPlayerIconSize, kMaxPlayerIconSize}};

        struct FloatKey
        {
            const char* key;
            float Settings::*field;
            float lo, hi;
        };
        constexpr FloatKey kFloats[] = {{"thickness", &Settings::thickness, kMinThickness, kMaxThickness},
            {"maxDistance", &Settings::maxDistance, kMinOutlineDistance, kMaxOutlineDistance}};

        struct ColorKey
        {
            const char* key;
            Color Settings::*field;
        };
        constexpr ColorKey kColors[] = {{"cursorColor", &Settings::cursorColor}, {"subCursorColor", &Settings::subCursorColor},
            {"outOfRangeCursorColor", &Settings::outOfRangeCursorColor},
            {"lockedCursorColor", &Settings::lockedCursorColor}, {"nameColor", &Settings::nameColor},
            {"textOutline", &Settings::textOutline}, {"iconTint", &Settings::iconTint}};

        constexpr const char* kFontKey = "fontName";
        constexpr const char* kShowSuffix = "Show";
        const char* const kCategoryKeys[kCategoryCount] = {"willAttack", "wontAttack", "unknown", "nmWillAttack", "nmWontAttack",
            "placeholder"};
        constexpr const char* kMobIdKey                  = "mobId";
        const char* const kMobIdNames[kMobIdFormatCount] = {"off", "lastThree", "full"};
        const char* const kChannelKeys[3]               = {"R", "G", "B"};
        const char* const kIconSideNames[kIconSideCount] = {"left", "right", "hide"};
        const char* const kPlayerIconKeys[kPlayerIconCount] = {"iconGm", "iconMentor", "iconNewAdventurer", "iconLevelSync",
            "iconAway", "iconLinkshell", "iconBazaar", "iconSeekingParty"};
        const char* const kLabelShadeKeys[kLabelShadeCount] = {"labelUnknown", "labelTooWeak", "labelEasyPrey",
            "labelDecentChallenge", "labelEvenMatch", "labelTough", "labelVeryTough"};

        float ClampFloat(float v, float lo, float hi, float fallback)
        {
            return std::isfinite(v) ? std::clamp(v, lo, hi) : fallback;
        }

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

        // Saturates rather than overflowing, so Clamp then takes a huge value to the nearest limit.
        int LoadInt(SettingsStore& store, const char* key, int fallback)
        {
            const float v = store.GetFloat(key, static_cast<float>(fallback));
            if (!std::isfinite(v)) return fallback;
            return static_cast<int>(std::clamp<double>(std::round(v), INT_MIN, INT_MAX));
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
        return s.enabled && (s.replaceMobNames || s.replacePlayerNames || s.replaceNpcNames);
    }

    bool NameplatesOn(const Settings& s)
    {
        return s.enabled && (s.showLabels || s.mobId != MobIdFormat::Off || s.showIcons || s.replaceMobNames ||
                                s.replacePlayerNames || s.replaceNpcNames || s.replaceCursor || s.phTimers);
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
        for (const FloatKey& f : kFloats)
            s.*f.field = ClampFloat(s.*f.field, f.lo, f.hi, d.*f.field);
        for (const IntKey& i : kInts)
            s.*i.field = std::clamp(s.*i.field, i.lo, i.hi);
        if (s.fontName.empty() || s.fontName.size() > kMaxFontName) s.fontName = d.fontName;
        for (const ColorKey& c : kColors)
            s.*c.field = ClampColor(s.*c.field, d.*c.field);
        for (int k = 0; k < kLabelShadeCount; ++k)
            s.labelColor[k] = ClampColor(s.labelColor[k], d.labelColor[k]);
        for (int c = 0; c < kCategoryCount; ++c)
            s.color[c] = ClampColor(s.color[c], d.color[c]);
        return s;
    }

    Settings LoadSettings(SettingsStore& store)
    {
        Settings s;
        for (const BoolKey& b : kBools)
            s.*b.field = store.GetBool(b.key, s.*b.field);
        for (const IntKey& i : kInts)
            s.*i.field = LoadInt(store, i.key, s.*i.field);
        for (const FloatKey& f : kFloats)
            s.*f.field = store.GetFloat(f.key, s.*f.field);
        for (const ColorKey& c : kColors)
            LoadColor(store, c.key, s.*c.field);
        s.fontName = store.GetString(kFontKey, kDefaultFont);
        for (int k = 0; k < kLabelShadeCount; ++k)
            LoadColor(store, kLabelShadeKeys[k], s.labelColor[k]);
        for (int c = 0; c < kCategoryCount; ++c)
        {
            const std::string base = kCategoryKeys[c];
            s.show[c] = store.GetBool((base + kShowSuffix).c_str(), s.show[c]);
            LoadColor(store, base, s.color[c]);
        }
        for (int i = 0; i < kPlayerIconCount; ++i)
        {
            const std::string name = store.GetString(kPlayerIconKeys[i], "");
            for (int side = 0; side < kIconSideCount; ++side)
                if (name == kIconSideNames[side]) s.playerIconSide[i] = static_cast<IconSide>(side);
        }
        const std::string mobId = store.GetString(kMobIdKey, "");
        for (int f = 0; f < kMobIdFormatCount; ++f)
            if (mobId == kMobIdNames[f]) s.mobId = static_cast<MobIdFormat>(f);
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
        auto setColor = [&](const std::string& base, const Color& c) {
            for (int i = 0; i < 3; ++i)
                setFloat(base + kChannelKeys[i], c.v[i]);
        };
        for (const BoolKey& b : kBools)
            setBool(b.key, s.*b.field);
        for (const IntKey& i : kInts)
            setFloat(i.key, static_cast<float>(s.*i.field));
        for (const FloatKey& f : kFloats)
            setFloat(f.key, s.*f.field);
        for (const ColorKey& c : kColors)
            setColor(c.key, s.*c.field);
        store.Set(kFontKey, s.fontName.c_str());
        for (int k = 0; k < kLabelShadeCount; ++k)
            setColor(kLabelShadeKeys[k], s.labelColor[k]);
        for (int c = 0; c < kCategoryCount; ++c)
        {
            const std::string base = kCategoryKeys[c];
            setBool(base + kShowSuffix, s.show[c]);
            setColor(base, s.color[c]);
        }
        for (int i = 0; i < kPlayerIconCount; ++i)
            store.Set(kPlayerIconKeys[i], kIconSideNames[static_cast<int>(s.playerIconSide[i])]);
        store.Set(kMobIdKey, kMobIdNames[static_cast<int>(s.mobId)]);
    }

    uint32_t ToArgb(const Color& c)
    {
        auto byte = [](float f) { return static_cast<uint8_t>(std::lround(f * 255.0f)); };
        return Opaque(byte(c.v[0]), byte(c.v[1]), byte(c.v[2]));
    }
}
