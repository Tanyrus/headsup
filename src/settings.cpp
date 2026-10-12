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
#include <utility>

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
            {"stackNames", &Settings::stackNames},
            {"nameplates", &Settings::nameplates}, {"outlines", &Settings::outlines}};

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
            {"untargetedOpacity", &Settings::untargetedOpacity, kMinOpacity, kMaxOpacity},
            {"baseSize", &Settings::baseSize, kMinTextSize, kMaxTextSize},
            {"scaleMin", &Settings::scaleMin, kMinSizePercent, kMaxSizePercent},
            {"scaleMax", &Settings::scaleMax, kMinSizePercent, kMaxSizePercent},
            {"timerPercent", &Settings::timerPercent, kMinSizePercent, kMaxSizePercent}};

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
            {"immuneBadgeColor", &Settings::immuneBadgeColor},
            {"cursorEdgeColor", &Settings::cursorEdgeColor}};

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
        constexpr int kDesignerVersion     = 3;
        constexpr int kSettingsVersion     = kDesignerVersion;
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

        const char* const kPlateModeNames[kPlateModeCount]       = {"hidden", "shown", "whenTargeted", "expanding", "vanilla"};
        const char* const kOutlineModeNames[kOutlineModeCount]   = {"disabled", "shown", "whenTargeted", "byNature"};
        const char* const kSourceNames[kColorSourceCount]        = {"custom", "game", "aggression", "toughness", "claim", "target", "type"};
        const char* const kAlignNames[kAlignCount]               = {"left", "center", "right"};
        const char* const kOrnamentPlaceNames[kOrnamentPlaceCount] = {"lines12", "lines23"};
        const char* const kNatureKeys[kNatureCount] = {"passive", "aggressive", "passiveNm", "aggressiveNm", "placeholder", "noData"};
        const char* const kClaimKeys[kClaimStateCount]   = {"unclaimed", "others", "yours"};
        const char* const kTargetKeys[kTargetStateCount] = {"none", "targeted", "locked", "subTarget", "outOfRange"};
        const char* const kTypeKeys[kTargetTypeCount]    = {"mob", "npc", "pet", "player", "friend", "partyMember", "linkshell"};
        const char* const kPaletteKeys[]                 = {"paletteAggression", "paletteToughness", "paletteClaim", "paletteTarget", "paletteType"};
        constexpr const char* kLabelShadePrefix = "label"; // kLabelShadeKeys' prefix, which the palette's toughness keys drop
        constexpr const char* kRowInfix         = "Row";
        constexpr const char* kLineInfix        = "Line";
        constexpr const char* kOrnamentInfix    = "Ornament";
        constexpr const char* kDefaultFontKey   = "defaultFont";
        constexpr const char* kFriendsKey       = "friends";
        constexpr const char* kDetectionShownKey = "detectionShown";
        constexpr float kPercentScale = 100.0f;
        // By PlayerIcon.
        const char* const kPlayerIconTags[kPlayerIconCount] = {kTagGm, kTagMentor, kTagNew, kTagSync, kTagAway, kTagLinkshell,
            kTagBazaar, kTagSeeking};
        constexpr int kMobIconLine = 0, kMobLevelLine = 1, kNameLine = kPlateLines - 1;

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

        std::string Capitalized(const char* key)
        {
            std::string text = key;
            if (!text.empty()) text[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(text[0])));
            return text;
        }

        int NameIndex(const char* const* names, int count, const std::string& name, int fallback)
        {
            for (int i = 0; i < count; ++i)
                if (name == names[i]) return i;
            return fallback;
        }

        void StoreFloat(SettingsStore& store, const std::string& key, float v)
        {
            char text[32];
            std::snprintf(text, sizeof(text), "%.4f", v);
            // A value the four places do not hold, such as a color made from a hex code, keeps its precision.
            if (std::strtof(text, nullptr) != v) std::snprintf(text, sizeof(text), "%.9g", v);
            store.Set(key.c_str(), text);
        }

        void StoreColor(SettingsStore& store, const std::string& base, const Color& c)
        {
            for (int i = 0; i < 3; ++i)
                StoreFloat(store, base + kChannelKeys[i], c.v[i]);
        }

        void StoreBool(SettingsStore& store, const std::string& key, bool v) { store.Set(key.c_str(), v ? "true" : "false"); }

        uint32_t LoadHex(SettingsStore& store, const std::string& key, uint32_t fallback)
        {
            const std::string text = store.GetString(key.c_str(), "");
            char* end              = nullptr;
            const unsigned long v  = std::strtoul(text.c_str(), &end, kHex);
            return text.empty() || *end != '\0' ? fallback : static_cast<uint32_t>(v);
        }

        void StoreHex(SettingsStore& store, const std::string& key, uint32_t v)
        {
            char text[16];
            std::snprintf(text, sizeof(text), "%X", v);
            store.Set(key.c_str(), text);
        }

        void LoadPick(SettingsStore& store, const std::string& base, ColorPick& pick)
        {
            pick.source = static_cast<ColorSource>(
                NameIndex(kSourceNames, kColorSourceCount, store.GetString((base + "Source").c_str(), ""), static_cast<int>(pick.source)));
            LoadColor(store, base, pick.custom);
        }

        void SavePick(SettingsStore& store, const std::string& base, const ColorPick& pick)
        {
            store.Set((base + "Source").c_str(), kSourceNames[static_cast<int>(pick.source)]);
            StoreColor(store, base, pick.custom);
        }

        // Where each palette's entries live in Settings::palette, by key.
        struct PaletteEntry
        {
            std::string key;
            Color* color;
        };
        std::vector<PaletteEntry> PaletteEntries(Palette& p)
        {
            std::vector<PaletteEntry> entries;
            auto add = [&](const char* palette, const std::string& entry, Color& color) {
                entries.push_back({std::string(palette) + entry, &color});
            };
            for (int i = 0; i < kNatureCount; ++i)
                add(kPaletteKeys[0], Capitalized(kNatureKeys[i]), p.aggression[i]);
            for (int i = 0; i < kLabelShadeCount; ++i)
                add(kPaletteKeys[1], std::string(kLabelShadeKeys[i]).substr(std::string(kLabelShadePrefix).size()), p.toughness[i]);
            for (int i = 0; i < kClaimStateCount; ++i)
                add(kPaletteKeys[2], Capitalized(kClaimKeys[i]), p.claim[i]);
            for (int i = 0; i < kTargetStateCount; ++i)
                add(kPaletteKeys[3], Capitalized(kTargetKeys[i]), p.target[i]);
            for (int i = 0; i < kTargetTypeCount; ++i)
                add(kPaletteKeys[4], Capitalized(kTypeKeys[i]), p.type[i]);
            return entries;
        }

        std::string LineBase(PlateKind kind, int line) { return std::string(KindKey(kind)) + kLineInfix + std::to_string(line + 1); }

        void LoadLine(SettingsStore& store, const std::string& base, LineDesign& line)
        {
            auto key = [&](const char* name) { return base + name; };
            line.text     = store.GetString(key("Text").c_str(), line.text.c_str());
            line.syncWith = LoadInt(store, key("Sync").c_str(), line.syncWith);
            line.show.always = store.GetBool(key("Always").c_str(), line.show.always);
            line.show.when   = LoadHex(store, key("When"), line.show.when);
            line.align = static_cast<Align>(NameIndex(kAlignNames, kAlignCount, store.GetString(key("Align").c_str(), ""), static_cast<int>(line.align)));
            line.font     = store.GetString(key("Font").c_str(), line.font.c_str());
            line.bold     = store.GetBool(key("Bold").c_str(), line.bold);
            line.textSize = LoadInt(store, key("TextSize").c_str(), line.textSize);
            line.iconSize = LoadInt(store, key("IconSize").c_str(), line.iconSize);
            LoadPick(store, key("Color"), line.color);
            line.shadow = LoadInt(store, key("Shadow").c_str(), line.shadow);
            LoadPick(store, key("ShadowColor"), line.shadowColor);
            line.glow     = LoadInt(store, key("Glow").c_str(), line.glow);
            line.glowSize = LoadInt(store, key("GlowSize").c_str(), line.glowSize);
            LoadPick(store, key("GlowColor"), line.glowColor);
        }

        void SaveLine(SettingsStore& store, const std::string& base, const LineDesign& line)
        {
            auto key = [&](const char* name) { return base + name; };
            store.Set(key("Text").c_str(), line.text.c_str());
            StoreFloat(store, key("Sync"), static_cast<float>(line.syncWith));
            StoreBool(store, key("Always"), line.show.always);
            StoreHex(store, key("When"), line.show.when);
            store.Set(key("Align").c_str(), kAlignNames[static_cast<int>(line.align)]);
            store.Set(key("Font").c_str(), line.font.c_str());
            StoreBool(store, key("Bold"), line.bold);
            StoreFloat(store, key("TextSize"), static_cast<float>(line.textSize));
            StoreFloat(store, key("IconSize"), static_cast<float>(line.iconSize));
            SavePick(store, key("Color"), line.color);
            StoreFloat(store, key("Shadow"), static_cast<float>(line.shadow));
            SavePick(store, key("ShadowColor"), line.shadowColor);
            StoreFloat(store, key("Glow"), static_cast<float>(line.glow));
            StoreFloat(store, key("GlowSize"), static_cast<float>(line.glowSize));
            SavePick(store, key("GlowColor"), line.glowColor);
        }

        std::string RowBase(PlateKind kind, int row)
        {
            return std::string(KindKey(kind)) + kRowInfix + Capitalized(RowKey(kind, row));
        }

        void LoadKind(SettingsStore& store, PlateKind kind, KindSettings& k)
        {
            for (int r = 0; r < RowCountOf(kind); ++r)
            {
                const std::string base = RowBase(kind, r);
                RowSettings& row       = k.rows[r];
                row.plate = static_cast<PlateMode>(NameIndex(kPlateModeNames, kPlateModeCount, store.GetString((base + "Plate").c_str(), ""), static_cast<int>(row.plate)));
                row.outline = static_cast<OutlineMode>(NameIndex(kOutlineModeNames, kOutlineModeCount, store.GetString((base + "Outline").c_str(), ""), static_cast<int>(row.outline)));
                LoadPick(store, base + "Color", row.outlineColor);
            }
            for (int n = 0; n < kPlateLines; ++n)
                LoadLine(store, LineBase(kind, n), k.lines[n]);
            const std::string base = std::string(KindKey(kind)) + kOrnamentInfix;
            OrnamentSettings& o    = k.ornament;
            o.on     = store.GetBool((base + "On").c_str(), o.on);
            o.place  = static_cast<OrnamentPlace>(NameIndex(kOrnamentPlaceNames, kOrnamentPlaceCount, store.GetString((base + "Place").c_str(), ""), static_cast<int>(o.place)));
            o.width  = LoadInt(store, (base + "Width").c_str(), o.width);
            o.height = LoadInt(store, (base + "Height").c_str(), o.height);
            LoadPick(store, base + "Color", o.color);
        }

        void SaveKind(SettingsStore& store, PlateKind kind, const KindSettings& k)
        {
            for (int r = 0; r < RowCountOf(kind); ++r)
            {
                const std::string base = RowBase(kind, r);
                store.Set((base + "Plate").c_str(), kPlateModeNames[static_cast<int>(k.rows[r].plate)]);
                store.Set((base + "Outline").c_str(), kOutlineModeNames[static_cast<int>(k.rows[r].outline)]);
                SavePick(store, base + "Color", k.rows[r].outlineColor);
            }
            for (int n = 0; n < kPlateLines; ++n)
                SaveLine(store, LineBase(kind, n), k.lines[n]);
            const std::string base = std::string(KindKey(kind)) + kOrnamentInfix;
            StoreBool(store, base + "On", k.ornament.on);
            store.Set((base + "Place").c_str(), kOrnamentPlaceNames[static_cast<int>(k.ornament.place)]);
            StoreFloat(store, base + "Width", static_cast<float>(k.ornament.width));
            StoreFloat(store, base + "Height", static_cast<float>(k.ornament.height));
            SavePick(store, base + "Color", k.ornament.color);
        }

        void LoadDesigner(SettingsStore& store, Settings& s)
        {
            for (int k = 0; k < kPlateKindCount; ++k)
                LoadKind(store, static_cast<PlateKind>(k), s.kinds[k]);
            for (const PaletteEntry& e : PaletteEntries(s.palette))
                LoadColor(store, e.key, *e.color);
            s.defaultFont    = store.GetString(kDefaultFontKey, s.defaultFont.c_str());
            s.detectionShown = LoadHex(store, kDetectionShownKey, s.detectionShown);
            std::istringstream names(store.GetString(kFriendsKey, ""));
            for (std::string name; names >> name;)
                s.friends.push_back(name);
        }

        void SaveDesigner(SettingsStore& store, const Settings& s)
        {
            for (int k = 0; k < kPlateKindCount; ++k)
                SaveKind(store, static_cast<PlateKind>(k), s.kinds[k]);
            Palette palette = s.palette;
            for (const PaletteEntry& e : PaletteEntries(palette))
                StoreColor(store, e.key, *e.color);
            store.Set(kDefaultFontKey, s.defaultFont.c_str());
            StoreHex(store, kDetectionShownKey, s.detectionShown);
            std::string names;
            for (const std::string& name : s.friends)
                names += (names.empty() ? "" : " ") + name;
            store.Set(kFriendsKey, names.c_str());
        }

        // The icons beside a name, as the old plate laid them: those on the left in the order's turn, GM always last of
        // them and so nearest the name whatever its side says, then the name, then those on the right.
        std::string PlayerNameText(const Settings& s)
        {
            if (!s.showPlayerIcons) return kTagName;
            std::string left, right;
            for (const PlayerIcon icon : s.playerIconOrder)
            {
                const int i = static_cast<int>(icon);
                if (icon == PlayerIcon::Gm) continue;
                if (s.playerIconSide[i] == IconSide::Left) left += kPlayerIconTags[i];
                else if (s.playerIconSide[i] == IconSide::Right) right += kPlayerIconTags[i];
            }
            return left + kTagGm + kTagName + right;
        }

        // Reads the old fields, which every file loads, into the designer's: the look a version 2 file had.
        void MigrateDesigner(Settings& s)
        {
            KindSettings& mobs      = s.Kind(PlateKind::Mobs);
            KindSettings& players   = s.Kind(PlateKind::Players);
            KindSettings& self      = s.Kind(PlateKind::Self);
            KindSettings& npcs      = s.Kind(PlateKind::Npcs);
            LineDesign& mobIcons    = mobs.lines[kMobIconLine];
            LineDesign& mobLevel    = mobs.lines[kMobLevelLine];
            LineDesign& mobName     = mobs.lines[kNameLine];
            const int base          = std::clamp(s.mobNameSize, kMinTextSize, kMaxTextSize);
            auto percent            = [&](int px) {
                const float pixels = static_cast<float>(std::clamp(px, kMinTextSize, kMaxTextSize));
                return static_cast<int>(std::lround(kPercentScale * pixels / static_cast<float>(base)));
            };
            // The icons are playerIconSize percent of the name they stand beside.
            auto IconPercent        = [&](int nameSize) {
                const float pixels = static_cast<float>(std::clamp(nameSize, kMinTextSize, kMaxTextSize));
                return static_cast<int>(std::lround(static_cast<float>(s.playerIconSize) * pixels / static_cast<float>(base)));
            };
            auto fontOf             = [](const std::string& font) { return font == kDefaultFont ? std::string() : font; };
            auto setVanilla         = [](KindSettings& k, PlateKind kind) {
                for (int r = 0; r < RowCountOf(kind); ++r)
                    k.rows[r].plate = PlateMode::Vanilla;
            };
            if (!s.replaceMobNames) setVanilla(mobs, PlateKind::Mobs);
            if (!s.replacePlayerNames)
            {
                setVanilla(players, PlateKind::Players);
                setVanilla(self, PlateKind::Self);
            }
            if (!s.replaceNpcNames) setVanilla(npcs, PlateKind::Npcs);

            const Category natureCategory[kNatureCount] = {Category::WontAttack, Category::WillAttack, Category::NmWontAttack,
                Category::NmWillAttack, Category::Placeholder, Category::Unknown};
            for (int n = 0; n < kNatureCount; ++n)
            {
                const int c = CategoryIndex(natureCategory[n]);
                const bool steps = CanOutlineByNature(PlateKind::Mobs, c);
                mobs.rows[c].outline = s.show[c] ? OutlineMode::Shown : steps ? OutlineMode::ByNature : OutlineMode::Disabled;
                s.palette.aggression[n] = s.color[c];
            }
            // Switched off, a claimed mob kept the outline its nature gave it.
            for (const Category claim : {Category::PartyClaim, Category::OtherClaim})
            {
                const int c = CategoryIndex(claim);
                mobs.rows[c].outline = s.show[c] ? OutlineMode::Shown : OutlineMode::ByNature;
                mobs.rows[c].outlineColor.source = ColorSource::Claim;
            }
            s.palette.claim[static_cast<int>(ClaimState::Yours)]  = s.color[CategoryIndex(Category::PartyClaim)];
            s.palette.claim[static_cast<int>(ClaimState::Others)] = s.color[CategoryIndex(Category::OtherClaim)];
            std::ranges::copy(s.labelColor, s.palette.toughness);
            s.palette.target[static_cast<int>(TargetState::Targeted)]   = s.cursorColor;
            s.palette.target[static_cast<int>(TargetState::Locked)]     = s.lockedCursorColor;
            s.palette.target[static_cast<int>(TargetState::SubTarget)]  = s.subCursorColor;
            s.palette.target[static_cast<int>(TargetState::OutOfRange)] = s.outOfRangeCursorColor;
            s.cursorEdgeColor = s.textOutline;
            for (KindSettings& k : s.kinds)
                for (LineDesign& line : k.lines)
                    line.shadowColor.custom = s.textOutline;
            mobLevel.shadowColor.custom = s.labelShadowColor;
            mobLevel.shadow             = s.labelShadow;

            mobIcons.font = mobName.font = fontOf(s.fontName);
            mobIcons.bold = mobName.bold = s.fontBold;
            mobLevel.font = fontOf(s.labelFontName);
            mobLevel.bold = s.labelFontBold;
            for (const PlateKind kind : {PlateKind::Players, PlateKind::Self})
            {
                LineDesign& line = s.Kind(kind).lines[kNameLine];
                line.font        = fontOf(s.playerFontName);
                line.bold        = s.playerFontBold;
                line.text        = PlayerNameText(s);
            }
            players.lines[kNameLine].iconSize = IconPercent(s.playerNameSize);
            self.lines[kNameLine].iconSize    = IconPercent(s.selfNameSize);
            npcs.lines[kNameLine].font = fontOf(s.npcFontName);
            npcs.lines[kNameLine].bold = s.npcFontBold;

            s.baseSize                         = base;
            mobLevel.textSize                  = percent(s.labelSize);
            mobIcons.iconSize                  = percent(s.iconSize);
            players.lines[kNameLine].textSize  = percent(s.playerNameSize);
            self.lines[kNameLine].textSize     = percent(s.selfNameSize);
            npcs.lines[kNameLine].textSize     = percent(s.npcNameSize);
            s.timerPercent                     = percent(s.timerSize);

            std::string level;
            auto append = [](std::string& text, const std::string& part) { text += (text.empty() || part.empty() ? "" : " ") + part; };
            if (s.showLabels) append(level, kTagLevelCon);
            if (s.mobId == MobIdFormat::LastThree) append(level, kTagId);
            if (s.mobId == MobIdFormat::Decimal) append(level, kTagServerId);
            if (s.mobId == MobIdFormat::Full) append(level, std::string(kTagServerId) + " " + kTagId);
            if (s.mobId != MobIdFormat::Off && s.markPlaceholders) append(level, kTagPlaceholder);
            mobLevel.text = level;
            std::string badges;
            if (s.showDamageIcons) badges += kTagWeakResist;
            if (s.showStatusRankIcons) badges += kTagStatus;
            if (s.showImmunityIcons) badges += kTagImmune;
            const std::string detect = s.showIcons ? kTagDetect : "";
            const bool inlineRow     = s.mobIconPlace == IconPlace::Inline && !level.empty();
            if (inlineRow)
            {
                const bool detectLeft = s.detectionIconSide == IconSide::Left, badgesLeft = s.badgeSide == IconSide::Left;
                const std::string left  = (badgesLeft ? badges : "") + (detectLeft ? detect : "");
                const std::string right = (detectLeft ? "" : detect) + (badgesLeft ? "" : badges);
                std::string row;
                append(row, left);
                append(row, level);
                append(row, right);
                mobLevel.text = row;
                mobIcons.text.clear();
            }
            else
                mobIcons.text = s.badgeSide == IconSide::Left ? badges + detect : detect + badges;
            auto condition = [&]() -> LineShow {
                if (s.hideClaimed) return LineShow{false, ConditionBit(ShowCondition::Unclaimed)};
                if (s.hideClaimedByParty)
                    return LineShow{false, ConditionBit(ShowCondition::Unclaimed) | ConditionBit(ShowCondition::OthersClaim)};
                return LineShow{};
            };
            mobLevel.show = condition();
            if (!mobIcons.text.empty()) mobIcons.show = mobLevel.show;

            mobName.glow     = s.nameGlow ? s.glowStrength : 0;
            mobName.glowSize = s.glowSize;
            if (s.ownGlowColor) mobName.glowColor = ColorPick{ColorSource::Custom, s.glowColor};
            mobs.ornament.on     = s.showOrnament;
            mobs.ornament.width  = s.ornamentWidth;
            mobs.ornament.height = s.ornamentThickness;
            if (s.ownOrnamentColor) mobs.ornament.color = ColorPick{ColorSource::Custom, s.ornamentColor};
            for (KindSettings& k : s.kinds)
            {
                k.lines[kNameLine].shadow = s.nameShadow;
                if (s.ownNameColor) k.lines[kNameLine].color = ColorPick{ColorSource::Custom, s.nameColor};
            }
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
        return s.enabled && (s.nameplates || s.replaceCursor || s.phTimers);
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

    namespace
    {
        void ClampPick(ColorPick& pick, const ColorPick& fallback)
        {
            if (static_cast<int>(pick.source) >= kColorSourceCount) pick.source = fallback.source;
            pick.custom = ClampColor(pick.custom, fallback.custom);
        }

        void ClampLine(LineDesign& line, const LineDesign& d)
        {
            std::ranges::replace(line.text, '\n', ' ');
            line.text.resize(std::min(line.text.size(), kMaxTemplate));
            if (line.font.size() > kMaxFontName) line.font.clear();
            if (static_cast<int>(line.align) >= kAlignCount) line.align = d.align;
            line.show.when = line.show.when & kAllConditions;
            line.textSize  = std::clamp(line.textSize, kMinSizePercent, kMaxSizePercent);
            line.iconSize  = std::clamp(line.iconSize, kMinSizePercent, kMaxSizePercent);
            line.shadow    = std::clamp(line.shadow, kMinShadow, kMaxShadow);
            line.glow      = std::clamp(line.glow, kMinGlow, kMaxGlow);
            line.glowSize  = std::clamp(line.glowSize, kMinGlowSize, kMaxGlowSize);
            ClampPick(line.color, d.color);
            ClampPick(line.shadowColor, d.shadowColor);
            ClampPick(line.glowColor, d.glowColor);
            if (line.syncWith < 0 || line.syncWith >= kPlateLines) line.syncWith = kNoSync;
        }

        // A cycle of syncs is cut at its highest-numbered line, so the lower line keeps its source.
        void BreakSyncCycles(LineDesign (&lines)[kPlateLines])
        {
            for (int start = 0; start < kPlateLines; ++start)
            {
                int at = start, highest = start;
                for (int step = 0; step < kPlateLines; ++step)
                {
                    at = lines[at].syncWith;
                    if (at == kNoSync) break;
                    highest = std::max(highest, at);
                    if (at == start)
                    {
                        lines[highest].syncWith = kNoSync;
                        break;
                    }
                }
            }
        }

        void ClampDesigner(Settings& s, const Settings& d)
        {
            s.scaleMax = std::max(s.scaleMax, s.scaleMin);
            for (int k = 0; k < kPlateKindCount; ++k)
            {
                KindSettings& kind = s.kinds[k];
                const KindSettings& defaults = d.kinds[k];
                for (int r = 0; r < kMaxRows; ++r)
                {
                    RowSettings& row = kind.rows[r];
                    if (static_cast<int>(row.plate) >= kPlateModeCount) row.plate = defaults.rows[r].plate;
                    if (static_cast<int>(row.outline) >= kOutlineModeCount ||
                        (row.outline == OutlineMode::ByNature && !CanOutlineByNature(static_cast<PlateKind>(k), r)))
                        row.outline = defaults.rows[r].outline;
                    ClampPick(row.outlineColor, defaults.rows[r].outlineColor);
                }
                for (int n = 0; n < kPlateLines; ++n)
                    ClampLine(kind.lines[n], defaults.lines[n]);
                BreakSyncCycles(kind.lines);
                OrnamentSettings& o = kind.ornament;
                if (static_cast<int>(o.place) >= kOrnamentPlaceCount) o.place = defaults.ornament.place;
                o.width  = std::clamp(o.width, kMinOrnamentWidth, kMaxOrnamentWidth);
                o.height = std::clamp(o.height, kMinOrnamentThickness, kMaxOrnamentThickness);
                ClampPick(o.color, defaults.ornament.color);
            }
            Palette fallback = d.palette;
            const std::vector<PaletteEntry> entries = PaletteEntries(s.palette), defaults = PaletteEntries(fallback);
            for (size_t i = 0; i < entries.size(); ++i)
                *entries[i].color = ClampColor(*entries[i].color, *defaults[i].color);
            if (s.defaultFont.empty() || s.defaultFont.size() > kMaxFontName) s.defaultFont = d.defaultFont;
            s.detectionShown &= kAllDetection;
            std::erase_if(s.friends, [](const std::string& name) {
                return name.empty() || std::ranges::any_of(name, [](char c) { return std::isspace(static_cast<unsigned char>(c)); });
            });
        }
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
        ClampDesigner(s, d);
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
        if (saved < kDesignerVersion) MigrateDesigner(s);
        else LoadDesigner(store, s);
        return Clamp(s);
    }

    void SaveSettings(const Settings& s, SettingsStore& store)
    {
        auto setBool  = [&](const std::string& key, bool v) { StoreBool(store, key, v); };
        auto setFloat = [&](const std::string& key, float v) { StoreFloat(store, key, v); };
        auto setColor = [&](const std::string& base, const Color& c) { StoreColor(store, base, c); };
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
        SaveDesigner(store, s);
        setFloat(kVersionKey, static_cast<float>(kSettingsVersion));
    }

    uint32_t ToArgb(const Color& c)
    {
        auto byte = [](float f) { return static_cast<uint8_t>(std::lround(f * 255.0f)); };
        return Opaque(byte(c.v[0]), byte(c.v[1]), byte(c.v[2]));
    }
}
