#include "settings.h"

#include "argb.h"
#include "fonts.h"
#include "pointer_keys.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <string>

namespace headsup
{
    namespace
    {
        // Whole numbers are stored as floats, like every other number in settings.ini.
        struct BoolKey
        {
            const char* key;
            bool Settings::*field;
        };
        constexpr BoolKey kBools[] = {{"enabled", &Settings::enabled}, {"showLabels", &Settings::showLabels},
            {"replaceMobNames", &Settings::replaceMobNames}, {"showIcons", &Settings::showIcons},
            // The setting's first name, which saved files carry.
            {"hideInCombat", &Settings::hideClaimedByParty}, {"hideClaimed", &Settings::hideClaimed},
            {"hideTooWeak", &Settings::hideTooWeak}, {"hideWhileEngaged", &Settings::hideWhileEngaged},
            {"markPlaceholders", &Settings::markPlaceholders}, {"phTimers", &Settings::phTimers},
            {"scaleWithDistance", &Settings::scaleWithDistance}, {"fontBold", &Settings::fontBold},
            {"nameGlow", &Settings::nameGlow}, {"glowOffWhenFighting", &Settings::glowOffWhenFighting}, {"scaleOwnName", &Settings::scaleOwnName},
            {"scalePlayerNames", &Settings::scalePlayerNames}, {"showOrnament", &Settings::showOrnament},
            {"ownGlowColor", &Settings::ownGlowColor}, {"ownOrnamentColor", &Settings::ownOrnamentColor},
            {"replacePlayerNames", &Settings::replacePlayerNames}, {"replaceNpcNames", &Settings::replaceNpcNames},
            {"replaceCursor", &Settings::replaceCursor}, {"cursorFeather", &Settings::cursorFeather},
            {"chocoboPointer", &Settings::chocoboPointer}, {"fixPointer", &Settings::fixPointer},
            {"showPlayerIcons", &Settings::showPlayerIcons}, {"centerNameAndIcons", &Settings::centerNameAndIcons},
            {"ownNameColor", &Settings::ownNameColor}, {"showDamageIcons", &Settings::showDamageIcons},
            {"showStatusRankIcons", &Settings::showStatusRankIcons}, {"showImmunityIcons", &Settings::showImmunityIcons},
            {"showRanks", &Settings::showRanks}, {"showBar", &Settings::showBar},
            {"hideHelpBar", &Settings::hideHelpBar}, {"hideNetstat", &Settings::hideNetstat},
            {"clockSeconds", &Settings::clockSeconds}, {"serverSeconds", &Settings::serverSeconds},
            {"stackNames", &Settings::stackNames}};

        struct IntKey
        {
            const char* key;
            int Settings::*field;
            int lo, hi;
        };
        constexpr IntKey kInts[] = {{"smoothness", &Settings::smoothness, kMinSmoothness, kMaxSmoothness},
            {"mobNameSize", &Settings::mobNameSize, kMinTextSize, kMaxTextSize},
            {"playerNameSize", &Settings::playerNameSize, kMinTextSize, kMaxTextSize},
            {"selfNameSize", &Settings::selfNameSize, kMinTextSize, kMaxTextSize},
            {"npcNameSize", &Settings::npcNameSize, kMinTextSize, kMaxTextSize},
            {"labelSize", &Settings::labelSize, kMinTextSize, kMaxTextSize},
            {"timerSize", &Settings::timerSize, kMinTextSize, kMaxTextSize},
            {"iconSize", &Settings::iconSize, kMinTextSize, kMaxTextSize},
            {"cursorSize", &Settings::cursorSize, kMinTextSize, kMaxTextSize},
            {"nameRaise", &Settings::nameRaise, kMinNameRaise, kMaxNameRaise},
            {"playerIconSize", &Settings::playerIconSize, kMinPlayerIconSize, kMaxPlayerIconSize},
            {"glowStrength", &Settings::glowStrength, kMinGlowStrength, kMaxGlowStrength},
            {"glowSize", &Settings::glowSize, kMinGlowSize, kMaxGlowSize},
            {"ornamentWidth", &Settings::ornamentWidth, kMinOrnamentWidth, kMaxOrnamentWidth},
            {"ornamentThickness", &Settings::ornamentThickness, kMinOrnamentThickness, kMaxOrnamentThickness},
            {"nameShadow", &Settings::nameShadow, kMinShadowStrength, kMaxShadowStrength},
            {"labelShadow", &Settings::labelShadow, kMinShadowStrength, kMaxShadowStrength},
            {"elementWeakRank", &Settings::elementWeakRank, kNoWeakRank, kMaxWeakRank},
            {"elementResistRank", &Settings::elementResistRank, kMinResistRank, kNoResistRank},
            {"maxElements", &Settings::maxElements, 0, kElementCount},
            {"barHeight", &Settings::barHeight, kMinBarHeight, kMaxBarHeight},
            {"barTextSize", &Settings::barTextSize, kAutoBarTextSize, kMaxBarTextSize},
            {"popupTextSize", &Settings::popupTextSize, kAutoPopupTextSize, kMaxBarTextSize},
            {"cursorOutline", &Settings::cursorOutline, kMinCursorOutline, kMaxCursorOutline},
            {"untargetedOpacity", &Settings::untargetedOpacity, kMinOpacity, kMaxOpacity}};

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
            {"textOutline", &Settings::textOutline}, {"iconTint", &Settings::iconTint}, {"glowColor", &Settings::glowColor},
            {"ornamentColor", &Settings::ornamentColor}, {"weakBadgeColor", &Settings::weakBadgeColor},
            {"resistBadgeColor", &Settings::resistBadgeColor}, {"rankBadgeColor", &Settings::rankBadgeColor},
            {"immuneBadgeColor", &Settings::immuneBadgeColor}};

        constexpr const char* kFontKey = "fontName";
        constexpr const char* kBadgeFontKey = "badgeFontName";
        // Saved files from before players and NPCs had their own name fonts take the mobs'.
        constexpr const char* kBarTitleFontKey = "barTitleFont";
        constexpr const char* kBarTextFontKey  = "barTextFont";
        constexpr const char* kPlayerFontKey = "playerFontName";
        constexpr const char* kPlayerBoldKey = "playerFontBold";
        constexpr const char* kNpcFontKey    = "npcFontName";
        constexpr const char* kNpcBoldKey    = "npcFontBold";
        // Saved files from before the level line had its own font and shadow take the name's.
        constexpr const char* kLabelFontKey        = "labelFontName";
        constexpr const char* kLabelBoldKey        = "labelFontBold";
        constexpr const char* kLabelShadowColorKey = "labelShadowColor";
        // And from before each kind of name had its own size, one for all of them.
        constexpr const char* kOneNameSizeKey = "nameSize";
        // And from before the cursor scaled on its own, when it scaled with the names.
        constexpr const char* kScaleCursorKey = "scaleCursor";
        constexpr const char* kKeepPointerKey = "keepPointerKeys"; // hex virtual-key codes, a space apart
        constexpr const char* kMovedWindowsKey = "movedWindows";   // short window names, a space apart
        constexpr const char* kWindowOffsetsKey = "windowOffsets"; // "equip:10,-20 statcom:0,40"
        constexpr const char* kWindowGroupsKey  = "windowGroups";  // "Equipment:20,-10,1:equip+inventor|..."
        constexpr const char* kAllUnderBarKey   = "allUnderBar";
        constexpr const char* kMoveGameHelpKey  = "moveGameHelp";
        constexpr const char* kHelpBarXKey = "helpBarX";
        constexpr const char* kHelpBarBackgroundKey = "helpBarBackground";
        constexpr const char* kHelpBarYKey = "helpBarY";
        constexpr int kHex                    = 16;
        // Every setting is saved whenever one changes, so a file keeps the defaults of its day, and each default that
        // changed is put right once in files from before. 1: the target cursor's became Phoenix's red. 2: the font's
        // became the bundled Marcellus SC.
        constexpr const char* kVersionKey = "settingsVersion";
        constexpr int kRedCursorVersion    = 1;
        constexpr int kBundledFontVersion  = 2;
        constexpr int kSettingsVersion     = kBundledFontVersion;
        constexpr Color kOldCursorColor    = {{1.0f, 1.0f, 1.0f}};
        constexpr const char* kOldFont     = "Trebuchet MS";
        constexpr const char* kShowSuffix = "Show";
        const char* const kCategoryKeys[kCategoryCount] = {"willAttack", "wontAttack", "unknown", "nmWillAttack", "nmWontAttack",
            "placeholder", "partyClaim", "otherClaim"};
        constexpr const char* kMobIdKey                  = "mobId";
        const char* const kMobIdNames[kMobIdFormatCount] = {"off", "lastThree", "decimal", "full"};
        constexpr const char* kLookKey                   = "look";
        const char* const kLookNames[kLookCount]         = {"fantasy"};
        constexpr const char* kBarThemeKey = "barTheme";
        const char* const kBarThemeNames[kBarThemeCount] = {"phoenixGold", "umbraDark", "phoenixRedOrnate",
            "phoenixRedPills"};
        constexpr const char* kBarCurrencyKey = "barCurrency";
        constexpr const char* kClockZoneKey   = "serverClockZone";
        constexpr const char* kTargetDetailsKey = "barTargetDetails";
        constexpr const char* kMenuFontKey      = "menuFont";
        constexpr const char* kMenuScaleKey     = "menuScale";
        constexpr const char* kSecondLineSuffix = "Line2";
        constexpr const char* kIconsSuffix      = "Icons";
        constexpr const char* kExpBarKey        = "expBar";
        const char* const kExpBarNames[kExpBarPlaceCount] = {"under", "beside", "off"};
        constexpr const char* kJobNamesKey  = "jobNames";
        constexpr const char* kSubtitleKey  = "barSubtitle";
        const char* const kSubtitleNames[kSubtitlePlaceCount] = {"hide", "before", "after"};
        constexpr const char* kClock24Key   = "clock24";
        constexpr const char* kLatencyKey   = "barLatency";
        const char* const kClockZoneNames[kClockZoneCount] = {"japan", "utc", "usEastern", "usCentral", "usMountain",
            "usArizona", "usPacific", "usAlaska", "usHawaii", "ukIreland", "centralEurope", "easternEurope",
            "australiaEastern"};
        constexpr const char* kBarSeparatorsKey                    = "barSeparators";
        const char* const kBarSeparatorsNames[kBarSeparatorsCount] = {"theme", "diamonds", "none"};
        const char* const kBarWidgetKeys[] = {"barZone", "barWeather", "barVanaTime", "barTarget", "barGil", "barBags",
            "barEarthTime", "barGameHelp", "barServerTime", "barFps", "barJob", "barConnection", "barParty", "barConquest", "barSignet", "barDistance"};
        static_assert(std::size(kBarWidgetKeys) == kBarWidgetCount);
        const char* const kBarSectionKeys[] = {"barLeft", "barCenter", "barRight"};
        static_assert(std::size(kBarSectionKeys) == kBarSectionCount);
        const char* const kChannelKeys[3]               = {"R", "G", "B"};
        const char* const kIconSideNames[kIconSideCount] = {"left", "right", "hide"};
        constexpr const char* kDetectionSideKey          = "detectionIconSide";
        constexpr const char* kBadgeSideKey              = "badgeSide";
        constexpr const char* kIconPlaceKey              = "mobIconPlace";
        const char* const kIconPlaceNames[kIconPlaceCount] = {"edges", "center", "inline"};
        // By StatusRank, each followed by kStatusShowSuffix and the cutoffs' suffixes.
        const char* const kStatusKeys[kStatusRankCount] = {"paralyze", "bind", "silence", "slow", "poison", "lightSleep",
            "darkSleep", "blind", "stun", "gravity"};
        constexpr const char* kStatusShowSuffix   = "Badge";
        constexpr const char* kPlayerIconOrderKey = "playerIconOrder"; // kPlayerIconKeys, a space apart
        constexpr const char* kPlayerIconShareSuffix = "Share";
        constexpr const char* kPlayerIconMaxKey   = "playerIconMax";
        constexpr const char* kWeakRankSuffix     = "WeakRank";
        constexpr const char* kResistRankSuffix   = "ResistRank";
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

    bool ReplacesAnyName(const Settings& s)
    {
        return s.enabled && (s.replaceMobNames || s.replacePlayerNames || s.replaceNpcNames);
    }

    bool NameplatesOn(const Settings& s)
    {
        return s.enabled && (s.showLabels || s.mobId != MobIdFormat::Off || s.showIcons || s.showDamageIcons ||
                                s.showStatusRankIcons || s.showImmunityIcons || s.replaceMobNames ||
                                s.replacePlayerNames || s.replaceNpcNames || s.replaceCursor || s.phTimers);
    }

    void UseHeadsUpWindowDefaults(Settings& s)
    {
        s.allUnderBar   = true;
        s.moveGameHelp  = true;
        s.movedWindows  = DefaultMovedWindows();
        s.windowOffsets = {};
        s.windowGroups  = DefaultWindowGroups();
    }

    void UseGameWindowDefaults(Settings& s)
    {
        s.allUnderBar   = false;
        s.moveGameHelp  = false;
        s.movedWindows  = {};
        s.windowOffsets = {};
        s.windowGroups  = {};
    }

    void UseHelpTextDefaults(Settings& s)
    {
        UseHeadsUpWindowDefaults(s);
        s.moveGameHelp = false;
        s.hideHelpBar  = true;
        s.barWidgets[static_cast<int>(BarWidget::GameHelp)] = true;
    }

    bool BarOn(const Settings& s)
    {
        return s.enabled && s.showBar;
    }

    bool BarRuns(const Settings& s, uint8_t loginStatus)
    {
        return BarOn(s) && loginStatus == kLoginInGame;
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
        std::vector<std::string> choices = BundledFamilies();
        for (const std::string& name : installed)
            if (std::none_of(choices.begin(), choices.end(), [&](const std::string& had) { return SameIgnoringCase(had, name); }))
                choices.push_back(name);
        return choices;
    }

    Settings Clamp(Settings s)
    {
        const Settings d;
        for (const FloatKey& f : kFloats)
            s.*f.field = ClampFloat(s.*f.field, f.lo, f.hi, d.*f.field);
        for (const IntKey& i : kInts)
            s.*i.field = std::clamp(s.*i.field, i.lo, i.hi);
        if (s.fontName.empty() || s.fontName.size() > kMaxFontName) s.fontName = d.fontName;
        if (s.labelFontName.empty() || s.labelFontName.size() > kMaxFontName) s.labelFontName = d.labelFontName;
        if (s.badgeFontName.empty() || s.badgeFontName.size() > kMaxFontName) s.badgeFontName = d.badgeFontName;
        if (s.playerFontName.empty() || s.playerFontName.size() > kMaxFontName) s.playerFontName = d.playerFontName;
        if (s.npcFontName.empty() || s.npcFontName.size() > kMaxFontName) s.npcFontName = d.npcFontName;
        if (s.barTitleFont.size() > kMaxFontName) s.barTitleFont = d.barTitleFont;
        if (s.barTextFont.size() > kMaxFontName) s.barTextFont = d.barTextFont;
        s.labelShadowColor = ClampColor(s.labelShadowColor, d.labelShadowColor);
        std::vector<uint8_t> keys;
        for (const uint8_t vk : s.keepPointerKeys)
        {
            const uint8_t key = NormalizeKey(vk);
            if (key != 0 && !MouseButton(key) && std::ranges::find(keys, key) == keys.end() && keys.size() < kMaxKeptKeys)
                keys.push_back(key);
        }
        s.keepPointerKeys = std::move(keys);
        s.movedWindows    = MovedWindowsFrom(MovedWindowNames(s.movedWindows));
        s.windowOffsets   = WindowOffsetsFrom(WindowOffsetText(s.windowOffsets));
        s.windowGroups    = WindowGroupsFrom(WindowGroupsText(s.windowGroups));
        s.helpBarX        = std::clamp(s.helpBarX, -kMaxHelpBarOffset, kMaxHelpBarOffset);
        s.helpBarY        = std::clamp(s.helpBarY, -kMaxHelpBarOffset, kMaxHelpBarOffset);
        for (const ColorKey& c : kColors)
            s.*c.field = ClampColor(s.*c.field, d.*c.field);
        for (int k = 0; k < kLabelShadeCount; ++k)
            s.labelColor[k] = ClampColor(s.labelColor[k], d.labelColor[k]);
        for (int c = 0; c < kCategoryCount; ++c)
            s.color[c] = ClampColor(s.color[c], d.color[c]);
        if (s.detectionIconSide == IconSide::Hidden) s.detectionIconSide = d.detectionIconSide;
        if (s.badgeSide == IconSide::Hidden) s.badgeSide = d.badgeSide;
        if (static_cast<int>(s.mobIconPlace) >= kIconPlaceCount) s.mobIconPlace = d.mobIconPlace;
        {
            bool seen[kPlayerIconCount] = {};
            bool whole                  = true;
            for (const PlayerIcon icon : s.playerIconOrder)
            {
                const int i = static_cast<int>(icon);
                if (i >= kPlayerIconCount || seen[i]) whole = false;
                else seen[i] = true;
            }
            if (!whole) s.playerIconOrder = d.playerIconOrder;
        }
        s.playerIconMax = std::clamp(s.playerIconMax, 1, kMaxPlayerIcons);
        if (static_cast<int>(s.barTheme) >= kBarThemeCount) s.barTheme = d.barTheme;
        if (s.barCurrency < 0 || s.barCurrency >= kCurrencyCount) s.barCurrency = d.barCurrency;
        if (static_cast<int>(s.serverClockZone) >= kClockZoneCount) s.serverClockZone = d.serverClockZone;
        if (static_cast<int>(s.expBar) >= kExpBarPlaceCount) s.expBar = d.expBar;
        if (static_cast<int>(s.barSubtitle) >= kSubtitlePlaceCount) s.barSubtitle = d.barSubtitle;
        if (MenuFontNamed(s.menuFont) == nullptr) s.menuFont = d.menuFont;
        s.menuScale = std::clamp(s.menuScale, kMinMenuScale, kMaxMenuScale);
        if (static_cast<int>(s.barSeparators) >= kBarSeparatorsCount) s.barSeparators = d.barSeparators;
        for (StatusBadgeRule& status : s.statusBadges)
        {
            status.weakRank   = std::clamp(status.weakRank, kNoWeakRank, kMaxWeakRank);
            status.resistRank = std::clamp(status.resistRank, kMinResistRank, kNoResistRank);
        }
        return s;
    }

    Settings LoadSettings(SettingsStore& store)
    {
        Settings s;
        for (const BoolKey& b : kBools)
            s.*b.field = store.GetBool(b.key, s.*b.field);
        s.mobNameSize = s.playerNameSize = s.selfNameSize = s.npcNameSize = LoadInt(store, kOneNameSizeKey, s.mobNameSize);
        for (const IntKey& i : kInts)
            s.*i.field = LoadInt(store, i.key, s.*i.field);
        for (const FloatKey& f : kFloats)
            s.*f.field = store.GetFloat(f.key, s.*f.field);
        for (const ColorKey& c : kColors)
            LoadColor(store, c.key, s.*c.field);
        s.fontName = store.GetString(kFontKey, kDefaultFont);
        s.labelFontName    = store.GetString(kLabelFontKey, s.fontName.c_str());
        s.badgeFontName    = store.GetString(kBadgeFontKey, kBadgeFont);
        s.barTitleFont     = store.GetString(kBarTitleFontKey, "");
        s.barTextFont      = store.GetString(kBarTextFontKey, "");
        s.labelFontBold    = store.GetBool(kLabelBoldKey, s.fontBold);
        s.scaleCursor      = store.GetBool(kScaleCursorKey, s.scaleWithDistance);
        std::istringstream keys(store.GetString(kKeepPointerKey, ""));
        for (std::string key; keys >> key;)
        {
            char* end        = nullptr;
            const long value = std::strtol(key.c_str(), &end, kHex);
            if (*end == '\0' && value > 0 && value <= UINT8_MAX) s.keepPointerKeys.push_back(static_cast<uint8_t>(value));
        }
        s.labelShadowColor = s.textOutline;
        LoadColor(store, kLabelShadowColorKey, s.labelShadowColor);
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
        {
            std::istringstream order(store.GetString(kPlayerIconOrderKey, ""));
            std::vector<PlayerIcon> icons;
            for (std::string name; order >> name;)
                for (int i = 0; i < kPlayerIconCount; ++i)
                    if (name == kPlayerIconKeys[i] && std::ranges::find(icons, static_cast<PlayerIcon>(i)) == icons.end())
                        icons.push_back(static_cast<PlayerIcon>(i));
            if (icons.size() == kPlayerIconCount) std::ranges::copy(icons, s.playerIconOrder.begin());
            for (int i = 0; i < kPlayerIconCount; ++i)
                s.playerIconShare[i] =
                    store.GetBool((std::string(kPlayerIconKeys[i]) + kPlayerIconShareSuffix).c_str(), s.playerIconShare[i]);
            s.playerIconMax = LoadInt(store, kPlayerIconMaxKey, s.playerIconMax);
        }
        // A group takes a side, never "hide".
        auto loadSide = [&](const char* key, IconSide& side) {
            const std::string name = store.GetString(key, "");
            for (const IconSide each : {IconSide::Left, IconSide::Right})
                if (name == kIconSideNames[static_cast<int>(each)]) side = each;
        };
        loadSide(kDetectionSideKey, s.detectionIconSide);
        loadSide(kBadgeSideKey, s.badgeSide);
        const std::string iconPlace = store.GetString(kIconPlaceKey, "");
        for (int i = 0; i < kIconPlaceCount; ++i)
            if (iconPlace == kIconPlaceNames[i]) s.mobIconPlace = static_cast<IconPlace>(i);
        for (int i = 0; i < kStatusRankCount; ++i)
        {
            const std::string base = kStatusKeys[i];
            StatusBadgeRule& rule  = s.statusBadges[i];
            rule.show              = store.GetBool((base + kStatusShowSuffix).c_str(), rule.show);
            rule.weakRank          = LoadInt(store, (base + kWeakRankSuffix).c_str(), rule.weakRank);
            rule.resistRank        = LoadInt(store, (base + kResistRankSuffix).c_str(), rule.resistRank);
        }
        const std::string mobId = store.GetString(kMobIdKey, "");
        for (int f = 0; f < kMobIdFormatCount; ++f)
            if (mobId == kMobIdNames[f]) s.mobId = static_cast<MobIdFormat>(f);
        const std::string look = store.GetString(kLookKey, "");
        for (int l = 0; l < kLookCount; ++l)
            if (look == kLookNames[l]) s.look = static_cast<Look>(l);
        const std::string theme = store.GetString(kBarThemeKey, "");
        for (int t = 0; t < kBarThemeCount; ++t)
            if (theme == kBarThemeNames[t]) s.barTheme = static_cast<BarTheme>(t);
        if (const std::optional<int> currency = CurrencyOfKey(store.GetString(kBarCurrencyKey, "")))
            s.barCurrency = *currency;
        s.barTargetDetails = store.GetBool(kTargetDetailsKey, s.barTargetDetails);
        s.menuFont         = store.GetString(kMenuFontKey, s.menuFont.c_str());
        s.menuScale        = LoadInt(store, kMenuScaleKey, s.menuScale);
        const std::string clockZone = store.GetString(kClockZoneKey, "");
        for (int z = 0; z < kClockZoneCount; ++z)
            if (clockZone == kClockZoneNames[z]) s.serverClockZone = static_cast<ClockZone>(z);
        const std::string separators = store.GetString(kBarSeparatorsKey, "");
        for (int t = 0; t < kBarSeparatorsCount; ++t)
            if (separators == kBarSeparatorsNames[t]) s.barSeparators = static_cast<BarSeparators>(t);
        for (int w = 0; w < kBarWidgetCount; ++w)
        {
            const std::string key = kBarWidgetKeys[w];
            s.barWidgets[w]       = store.GetBool(key.c_str(), s.barWidgets[w]);
            s.barSecondLine[w]    = store.GetBool((key + kSecondLineSuffix).c_str(), s.barSecondLine[w]);
            s.barIcons[w]         = store.GetBool((key + kIconsSuffix).c_str(), s.barIcons[w]);
        }
        const std::string expBar = store.GetString(kExpBarKey, "");
        for (int e = 0; e < kExpBarPlaceCount; ++e)
            if (expBar == kExpBarNames[e]) s.expBar = static_cast<ExpBarPlace>(e);
        s.jobNames   = store.GetBool(kJobNamesKey, s.jobNames);
        const std::string subtitle = store.GetString(kSubtitleKey, "");
        for (int i = 0; i < kSubtitlePlaceCount; ++i)
            if (subtitle == kSubtitleNames[i]) s.barSubtitle = static_cast<SubtitlePlace>(i);
        s.clock24    = store.GetBool(kClock24Key, s.clock24);
        s.barLatency = store.GetBool(kLatencyKey, s.barLatency);
        std::array<std::string, kBarSectionCount> sections;
        for (int i = 0; i < kBarSectionCount; ++i)
            sections[i] = store.GetString(kBarSectionKeys[i], "");
        s.barSections = PlacementFrom(sections);
        s.movedWindows = MovedWindowsFrom(store.GetString(kMovedWindowsKey, MovedWindowNames(s.movedWindows).c_str()));
        s.windowOffsets = WindowOffsetsFrom(store.GetString(kWindowOffsetsKey, ""));
        s.windowGroups  = WindowGroupsFrom(store.GetString(kWindowGroupsKey, ""));
        s.allUnderBar   = store.GetBool(kAllUnderBarKey, s.allUnderBar);
        s.moveGameHelp  = store.GetBool(kMoveGameHelpKey, s.moveGameHelp);
        s.helpBarX      = LoadInt(store, kHelpBarXKey, s.helpBarX);
        s.helpBarBackground = store.GetBool(kHelpBarBackgroundKey, s.helpBarBackground);
        s.helpBarY      = LoadInt(store, kHelpBarYKey, s.helpBarY);
        const float saved = store.GetFloat(kVersionKey, 0.0f);
        if (saved < kRedCursorVersion && s.cursorColor == kOldCursorColor) s.cursorColor = Settings{}.cursorColor;
        if (saved < kBundledFontVersion)
        {
            if (s.fontName == kOldFont) s.fontName = kDefaultFont;
            if (s.labelFontName == kOldFont) s.labelFontName = kDefaultFont;
        }
        s.playerFontName = store.GetString(kPlayerFontKey, s.fontName.c_str());
        s.playerFontBold = store.GetBool(kPlayerBoldKey, s.fontBold);
        s.npcFontName    = store.GetString(kNpcFontKey, s.fontName.c_str());
        s.npcFontBold    = store.GetBool(kNpcBoldKey, s.fontBold);
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
        store.Set(kLabelFontKey, s.labelFontName.c_str());
        store.Set(kBadgeFontKey, s.badgeFontName.c_str());
        store.Set(kBarTitleFontKey, s.barTitleFont.c_str());
        store.Set(kBarTextFontKey, s.barTextFont.c_str());
        store.Set(kPlayerFontKey, s.playerFontName.c_str());
        setBool(kPlayerBoldKey, s.playerFontBold);
        store.Set(kNpcFontKey, s.npcFontName.c_str());
        setBool(kNpcBoldKey, s.npcFontBold);
        setBool(kLabelBoldKey, s.labelFontBold);
        setBool(kScaleCursorKey, s.scaleCursor);
        std::string keys;
        for (const uint8_t vk : s.keepPointerKeys)
        {
            char hex[4];
            std::snprintf(hex, sizeof(hex), "%02X", vk);
            keys += (keys.empty() ? "" : " ") + std::string(hex);
        }
        store.Set(kKeepPointerKey, keys.c_str());
        setColor(kLabelShadowColorKey, s.labelShadowColor);
        for (int k = 0; k < kLabelShadeCount; ++k)
            setColor(kLabelShadeKeys[k], s.labelColor[k]);
        for (int c = 0; c < kCategoryCount; ++c)
        {
            const std::string base = kCategoryKeys[c];
            setBool(base + kShowSuffix, s.show[c]);
            setColor(base, s.color[c]);
        }
        std::string order;
        for (int i = 0; i < kPlayerIconCount; ++i)
        {
            store.Set(kPlayerIconKeys[i], kIconSideNames[static_cast<int>(s.playerIconSide[i])]);
            setBool(std::string(kPlayerIconKeys[i]) + kPlayerIconShareSuffix, s.playerIconShare[i]);
            order += (order.empty() ? "" : " ") + std::string(kPlayerIconKeys[static_cast<int>(s.playerIconOrder[i])]);
        }
        store.Set(kPlayerIconOrderKey, order.c_str());
        setFloat(kPlayerIconMaxKey, static_cast<float>(s.playerIconMax));
        store.Set(kDetectionSideKey, kIconSideNames[static_cast<int>(s.detectionIconSide)]);
        store.Set(kBadgeSideKey, kIconSideNames[static_cast<int>(s.badgeSide)]);
        store.Set(kIconPlaceKey, kIconPlaceNames[static_cast<int>(s.mobIconPlace)]);
        for (int i = 0; i < kStatusRankCount; ++i)
        {
            const std::string base = kStatusKeys[i];
            setBool(base + kStatusShowSuffix, s.statusBadges[i].show);
            setFloat(base + kWeakRankSuffix, static_cast<float>(s.statusBadges[i].weakRank));
            setFloat(base + kResistRankSuffix, static_cast<float>(s.statusBadges[i].resistRank));
        }
        store.Set(kMobIdKey, kMobIdNames[static_cast<int>(s.mobId)]);
        store.Set(kLookKey, kLookNames[static_cast<int>(s.look)]);
        store.Set(kBarThemeKey, kBarThemeNames[static_cast<int>(s.barTheme)]);
        store.Set(kBarSeparatorsKey, kBarSeparatorsNames[static_cast<int>(s.barSeparators)]);
        store.Set(kBarCurrencyKey, kCurrencyTable[s.barCurrency].key);
        store.Set(kClockZoneKey, kClockZoneNames[static_cast<int>(s.serverClockZone)]);
        setBool(kTargetDetailsKey, s.barTargetDetails);
        store.Set(kMenuFontKey, s.menuFont.c_str());
        setFloat(kMenuScaleKey, static_cast<float>(s.menuScale));
        for (int w = 0; w < kBarWidgetCount; ++w)
        {
            const std::string key = kBarWidgetKeys[w];
            setBool(key, s.barWidgets[w]);
            setBool(key + kSecondLineSuffix, s.barSecondLine[w]);
            setBool(key + kIconsSuffix, s.barIcons[w]);
        }
        store.Set(kExpBarKey, kExpBarNames[static_cast<int>(s.expBar)]);
        setBool(kJobNamesKey, s.jobNames);
        store.Set(kSubtitleKey, kSubtitleNames[static_cast<int>(s.barSubtitle)]);
        setBool(kClock24Key, s.clock24);
        setBool(kLatencyKey, s.barLatency);
        for (int i = 0; i < kBarSectionCount; ++i)
            store.Set(kBarSectionKeys[i], PlacementNames(s.barSections[i]).c_str());
        store.Set(kMovedWindowsKey, MovedWindowNames(s.movedWindows).c_str());
        store.Set(kWindowOffsetsKey, WindowOffsetText(s.windowOffsets).c_str());
        store.Set(kWindowGroupsKey, WindowGroupsText(s.windowGroups).c_str());
        setBool(kAllUnderBarKey, s.allUnderBar);
        setBool(kMoveGameHelpKey, s.moveGameHelp);
        setFloat(kHelpBarXKey, static_cast<float>(s.helpBarX));
        setBool(kHelpBarBackgroundKey, s.helpBarBackground);
        setFloat(kHelpBarYKey, static_cast<float>(s.helpBarY));
        setFloat(kVersionKey, static_cast<float>(kSettingsVersion));
    }

    uint32_t ToArgb(const Color& c)
    {
        auto byte = [](float f) { return static_cast<uint8_t>(std::lround(f * 255.0f)); };
        return Opaque(byte(c.v[0]), byte(c.v[1]), byte(c.v[2]));
    }
}
