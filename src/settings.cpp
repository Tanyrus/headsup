#include "settings.h"

#include "argb.h"
#include "cursor_picture.h"
#include "fonts.h"
#include "pointer_keys.h"
#include "utf16.h"

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
        constexpr BoolKey kBools[] = {{"enabled", &Settings::enabled}, {"phTimers", &Settings::phTimers},
            {"scaleWithDistance", &Settings::scaleWithDistance}, {"scaleOwnName", &Settings::scaleOwnName},
            {"chocoboPointer", &Settings::chocoboPointer},
            {"fixPointer", &Settings::fixPointer}, {"showRanks", &Settings::showRanks}, {"showBar", &Settings::showBar},
            {"hideHelpBar", &Settings::hideHelpBar}, {"hideNetstat", &Settings::hideNetstat},
            {"screenshotHidesGame", &Settings::screenshotHidesGame},
            {"clockSeconds", &Settings::clockSeconds},
            {"stackNames", &Settings::stackNames},
            {"nameplates", &Settings::nameplates}, {"outlines", &Settings::outlines},
            {"outlineFadeIn", &Settings::outlineFadeIn}, {"cursorFloat", &Settings::cursorFloat},
            {"autoCheck", &Settings::autoCheck}};

        struct IntKey
        {
            const char* key;
            int Settings::*field;
            int lo, hi;
        };
        constexpr IntKey kInts[] = {{"smoothness", &Settings::smoothness, kMinSmoothness, kMaxSmoothness},
            {"outlineThickness", &Settings::outlineThickness, kMinOutlineThickness, kMaxOutlineThickness},
            {"outlineOpacity", &Settings::outlineOpacity, kMinOpacity, kMaxOpacity},
            {"menuOpacity", &Settings::menuOpacity, kMinMenuOpacity, kMaxOpacity},
            {"cursorRaise", &Settings::cursorRaise, -kMaxCursorRaise, kMaxCursorRaise},
            {"nameRaise", &Settings::nameRaise, kMinNameRaise, kMaxNameRaise},
            {"elementWeakRank", &Settings::elementWeakRank, kNoWeakRank, kMaxWeakRank},
            {"elementResistRank", &Settings::elementResistRank, kMinResistRank, kNoResistRank},
            {"maxElements", &Settings::maxElements, 0, kElementCount},
            {"barHeight", &Settings::barHeight, kMinBarHeight, kMaxBarHeight},
            {"barTextSize", &Settings::barTextSize, kAutoBarTextSize, kMaxBarTextSize},
            {"popupTextSize", &Settings::popupTextSize, kAutoPopupTextSize, kMaxBarTextSize},
            {"baseSize", &Settings::baseSize, kMinTextSize, kMaxTextSize},
            {"baseIconSize", &Settings::baseIconSize, kMinTextSize, kMaxTextSize},
            {"scaleMin", &Settings::scaleMin, kMinSizePercent, kMaxSizePercent},
            {"scaleMax", &Settings::scaleMax, kMinSizePercent, kMaxSizePercent},
            {"timerPercent", &Settings::timerPercent, kMinSizePercent, kMaxSizePercent},
            {"screenshotKey", &Settings::screenshotKey, 0, UINT8_MAX}};

        // Numbers whose meaning changed and whose key changed with it: saved and bounded as the rest, and read through
        // Rebased, since a typed read of the new key would have Ashita's store hold its default before the old is asked.
        constexpr IntKey kRebasedInts[] = {{"cursorWidth", &Settings::cursorWidth, kMinCursorSide, kMaxCursorSide},
            {"cursorHeight", &Settings::cursorHeight, kMinCursorSide, kMaxCursorSide},
            {"cursorOutlineThickness", &Settings::cursorOutlineThickness, kMinCursorEdge, kMaxCursorEdge}};
        constexpr const char* kCursorWidthKey  = kRebasedInts[0].key;
        constexpr const char* kCursorHeightKey = kRebasedInts[1].key;
        constexpr const char* kCursorEdgeKey   = kRebasedInts[2].key;
        constexpr const char* kDashMarkKey = "dashMark";
        const char* const kDashMarkNames[kDashMarkCount] = {"no", "dot", "square", "diamond"};
        constexpr const char* kCursorStyleKey = "cursorStyle";
        constexpr const char* kCursorFileKey  = "cursorFile";
        const char* const kCursorStyleNames[kCursorStyleCount] = {"vanilla", "triangle", "feather", "chocobo", "file"};
        // Before it had a style the cursor was on or off, and Phoenix's feather or an arrow.
        constexpr const char* kOldReplaceCursorKey = "replaceCursor";
        constexpr const char* kOldCursorFeatherKey = "cursorFeather";
        constexpr const char* kOldCursorSizeKey    = "cursorSize";    // its height in pixels, its width its shape's
        constexpr const char* kOldCursorOutlineKey = "cursorOutline"; // percent, of twice the edge
        constexpr float kCursorEdgeRebase          = 2.0f;
        constexpr size_t kMaxCursorFileName        = 64;
        constexpr const char* kPathCharacters      = "\\/:";

        struct FloatKey
        {
            const char* key;
            float Settings::*field;
            float lo, hi;
        };
        constexpr FloatKey kFloats[] = {{"maxDistance", &Settings::maxDistance, kMinOutlineDistance, kMaxOutlineDistance}};

        struct ColorKey
        {
            const char* key;
            Color Settings::*field;
        };
        constexpr ColorKey kColors[] = {{"weakBadgeColor", &Settings::weakBadgeColor},
            {"resistBadgeColor", &Settings::resistBadgeColor}, {"rankBadgeColor", &Settings::rankBadgeColor},
            {"immuneBadgeColor", &Settings::immuneBadgeColor}};
        // The edge's custom color is under the keys the edge's one color had. The cursor's own cannot be under
        // cursorColor, where a file from before the designer holds the target's color.
        constexpr const char* kCursorEdgePickKey = "cursorEdgeColor";
        constexpr const char* kCursorPickKey     = "cursorPick";

        constexpr const char* kBadgeFontKey = "badgeFontName";
        constexpr const char* kBarTitleFontKey = "barTitleFont";
        constexpr const char* kBarTextFontKey  = "barTextFont";
        // Saved files from before the cursor scaled on its own, when it scaled with the names, take the names'.
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
        // A file's first save writes every key it lacks, so it keeps the defaults of its day, and each default that
        // changed is put right once in files from before. 1: the target cursor's became Phoenix's red. 2: the font's
        // became the bundled Marcellus SC. 3: the nameplate designer's settings took the place of the fixed plate's.
        // 4: the review's settings. A number whose meaning changed there has a new key, read through Rebased, and its
        // old key is never written again: the build before goes on reading its own.
        constexpr const char* kVersionKey = "settingsVersion";
        constexpr int kRedCursorVersion    = 1;
        constexpr int kBundledFontVersion  = 2;
        constexpr int kDesignerVersion     = 3;
        constexpr int kReviewVersion       = 4;
        constexpr int kSettingsVersion     = kReviewVersion;
        // Saved with the designer's keys. An older build's save takes the version back to its own and leaves this and
        // those keys in the file, so a file that has it is not migrated over them.
        constexpr const char* kDesignsSavedKey = "nameplateDesigns";
        constexpr const char* kBarThemeKey = "barTheme";
        const char* const kBarThemeNames[kBarThemeCount] = {"phoenixGold", "umbraDark", "phoenixRedOrnate",
            "phoenixRedPills", "aurora"};
        constexpr const char* kBarCurrencyKey = "barCurrency";
        constexpr const char* kClockServerTimeKey = "clockServerTime";
        // Server time was a widget of its own, with this switch.
        constexpr const char* kServerTimeWidgetKey = "barServerTime";
        constexpr const char* kTrue                = "true";
        constexpr const char* kAshitaTrue          = "1";
        constexpr const char* kClockZoneKey       = "clockZone"; // the Clock widget's: a kClockZoneNames or kLocalZone
        constexpr const char* kLocalZone          = "local";
        constexpr const char* kJobLayoutKey       = "jobLayout";
        const char* const kJobLayoutNames[kJobLayoutCount] = {"jobsFirst", "expFirst", "expOnly"};
        constexpr const char* kTargetDetailsKey = "barTargetDetails";
        constexpr const char* kMenuFontKey      = "menuFont";
        constexpr const char* kMenuScaleKey     = "menuScale";
        constexpr const char* kMenuThemeKey     = "menuTheme";
        const char* const kMenuThemeNames[kMenuThemeCount] = {"phoenix", "aurora"};
        constexpr const char* kMenuXKey = "menuX", *kMenuYKey = "menuY", *kMenuWidthKey = "menuWidth",
                              *kMenuHeightKey = "menuHeight";
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
            "barEarthTime", "barGameHelp", "barFps", "barJob", "barConnection", "barParty", "barSignet", "barDistance"};
        static_assert(std::size(kBarWidgetKeys) == kBarWidgetCount);
        const char* const kBarSectionKeys[] = {"barLeft", "barCenter", "barRight"};
        static_assert(std::size(kBarSectionKeys) == kBarSectionCount);
        const char* const kChannelKeys[3]               = {"R", "G", "B"};
        // By StatusRank, each followed by kStatusShowSuffix and the cutoffs' suffixes.
        const char* const kStatusKeys[kStatusRankCount] = {"paralyze", "bind", "silence", "slow", "poison", "lightSleep",
            "darkSleep", "blind", "stun", "gravity"};
        constexpr const char* kStatusShowSuffix   = "Badge";
        constexpr const char* kWeakRankSuffix     = "WeakRank";
        constexpr const char* kResistRankSuffix   = "ResistRank";
        const char* const kLabelShadeKeys[kLabelShadeCount] = {"labelUnknown", "labelTooWeak", "labelEasyPrey",
            "labelDecentChallenge", "labelEvenMatch", "labelTough", "labelVeryTough"};

        const char* const kPlateModeNames[kPlateModeCount]       = {"hidden", "shown", "whenTargeted", "vanilla"};
        // A mode that is gone: its name alone, and the whole nameplate for a target. Such a row is Shown.
        constexpr const char* kExpandingName = "expanding";
        const char* const kOutlineModeNames[kOutlineModeCount]   = {"disabled", "shown", "whenTargeted", "byNature"};
        const char* const kSourceNames[kColorSourceCount]        = {"custom", "game", "aggression", "toughness", "claim", "target", "type"};
        const char* const kAlignNames[kAlignCount]               = {"left", "center", "right"};
        const char* const kOrnamentPlaceNames[kOrnamentPlaceCount] = {"disabled", "top", "aboveMiddle", "belowMiddle", "bottom"};
        // Before it had a place over or under the plate, an ornament was a switch and one of two places between lines.
        constexpr const char* kOldOrnamentOnSuffix    = "On";
        constexpr const char* kOldOrnamentPlaceSuffix = "Place";
        constexpr const char* kOldOrnamentUpperPlace  = "lines12";
        const char* const kNatureKeys[kNatureCount] = {"passive", "aggressive", "passiveNm", "aggressiveNm", "placeholder", "noData"};
        const char* const kClaimKeys[kClaimStateCount]   = {"unclaimed", "others", "yours"};
        const char* const kTargetKeys[kTargetStateCount] = {"none", "targeted", "locked", "subTarget", "outOfRange"};
        const char* const kTypeKeys[kTargetTypeCount]    = {"mob", "player", "friend", "partyMember", "linkshell", "npc", "pet"};
        const char* const kPaletteKeys[]                 = {"paletteAggression", "paletteToughness", "paletteClaim", "paletteTarget", "paletteType"};
        constexpr const char* kLabelShadePrefix = "label"; // kLabelShadeKeys' prefix, which the palette's toughness keys drop
        constexpr const char* kRowInfix         = "Row";
        constexpr const char* kOpacityInfix     = "Opacity";
        constexpr const char* kGlowOffSuffix    = "GlowOffInCombat";
        constexpr const char* kOldUntargetedKey = "untargetedOpacity";   // every mob's that was not your target
        constexpr const char* kOldGlowOffKey    = "glowOffWhenFighting"; // every line of a mob your party had claimed
        constexpr const char* kOldPetRow        = "Pet"; // the pets' one row of before, after kRowInfix
        constexpr const char* kLineInfix        = "Line";
        constexpr const char* kOrnamentInfix    = "Ornament";
        constexpr const char* kDefaultFontKey   = "defaultFont";
        constexpr const char* kFriendsKey       = "friends";
        constexpr const char* kDetectionShownKey = "detectionShown";
        constexpr float kPercentScale = 100.0f;
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

        int Whole(float v) { return static_cast<int>(std::clamp<double>(std::round(v), INT_MIN, INT_MAX)); }

        // What a key the store lacks reads as: no line of a file holds a line break, so it differs from every text.
        constexpr const char* kNoText = "\n";
        // A key's number as the store holds it; nothing for a key it lacks or that holds no number. Asked as text: a
        // typed read of a key Ashita's store lacks adds the key with the default it was given.
        std::optional<float> HeldNumber(SettingsStore& store, const std::string& key)
        {
            const std::string text = store.GetString(key.c_str(), kNoText);
            char* end              = nullptr;
            const float v          = std::strtof(text.c_str(), &end);
            return text == kNoText || end == text.c_str() || !std::isfinite(v) ? std::nullopt : std::optional<float>(v);
        }

        // A switch as the store holds it, asked as text for the same reason; nothing for a key it lacks. A build
        // writes true or false, and Ashita's own default for a key it added is 1 or 0.
        std::optional<bool> HeldSwitch(SettingsStore& store, const char* key)
        {
            const std::string text = store.GetString(key, kNoText);
            if (text == kNoText) return std::nullopt;
            return text == kTrue || text == kAshitaTrue;
        }

        // A number whose meaning changed: the file's own under its new key, else what the value under its old key
        // comes to, so a look from before is kept, else the default.
        template <typename Convert>
        int Rebased(SettingsStore& store, const std::string& key, const std::string& oldKey, int fallback, Convert convert)
        {
            if (const std::optional<float> held = HeldNumber(store, key)) return Whole(*held);
            if (const std::optional<float> old = HeldNumber(store, oldKey)) return Whole(convert(*old));
            return fallback;
        }

        // A shadow of the unit before, where 100 was the mockup's shadow, as a strength of this one.
        float ShadowStrengthOf(float before) { return before / kShadowUnit; }

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

        void StoreBool(SettingsStore& store, const std::string& key, bool v) { store.Set(key.c_str(), v ? kTrue : "false"); }

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

        // iconShare: what a file from before, whose icon sizes were percents of its base font size, has them
        // multiplied by to be percents of the base icon size.
        void LoadLine(SettingsStore& store, const std::string& base, LineDesign& line, float iconShare)
        {
            auto key = [&](const char* name) { return base + name; };
            line.text     = store.GetString(key("Text").c_str(), line.text.c_str());
            line.syncWith = LoadInt(store, key("Sync").c_str(), line.syncWith);
            line.show.always = store.GetBool(key("Always").c_str(), line.show.always);
            line.show.when   = LoadHex(store, key("When"), line.show.when);
            line.align = static_cast<Align>(NameIndex(kAlignNames, kAlignCount, store.GetString(key("Align").c_str(), ""), static_cast<int>(line.align)));
            line.font     = store.GetString(key("Font").c_str(), line.font.c_str());
            // Bold was a switch of its own before a line had styles.
            if (store.GetString(key("Style").c_str(), kNoText) != kNoText)
                line.style = static_cast<uint8_t>(LoadHex(store, key("Style"), line.style));
            else if (store.GetString(key("Bold").c_str(), "") == kTrue)
                line.style = StyleBit(FontStyle::Bold);
            line.textSize = LoadInt(store, key("TextSize").c_str(), line.textSize);
            line.iconSize = Rebased(store, key("IconScale"), key("IconSize"), line.iconSize,
                [&](float before) { return before * iconShare; });
            line.padding = LoadInt(store, key("Padding").c_str(), line.padding);
            LoadPick(store, key("Color"), line.color);
            line.shadow = Rebased(store, key("ShadowStrength"), key("Shadow"), line.shadow, ShadowStrengthOf);
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
            StoreHex(store, key("Style"), line.style);
            StoreFloat(store, key("TextSize"), static_cast<float>(line.textSize));
            StoreFloat(store, key("IconScale"), static_cast<float>(line.iconSize));
            StoreFloat(store, key("Padding"), static_cast<float>(line.padding));
            SavePick(store, key("Color"), line.color);
            StoreFloat(store, key("ShadowStrength"), static_cast<float>(line.shadow));
            SavePick(store, key("ShadowColor"), line.shadowColor);
            StoreFloat(store, key("Glow"), static_cast<float>(line.glow));
            StoreFloat(store, key("GlowSize"), static_cast<float>(line.glowSize));
            SavePick(store, key("GlowColor"), line.glowColor);
            StoreBool(store, key(kGlowOffSuffix), line.glowOffInCombat);
        }

        std::string RowBase(PlateKind kind, int row)
        {
            return std::string(KindKey(kind)) + kRowInfix + Capitalized(RowKey(kind, row));
        }

        void LoadKind(SettingsStore& store, PlateKind kind, KindSettings& k, float iconShare)
        {
            for (int r = 0; r < RowCountOf(kind); ++r)
            {
                // Pets had one row before they had four: a row the file has no keys for takes that row's.
                std::string base = RowBase(kind, r);
                if (kind == PlateKind::Pets && store.GetString((base + "Plate").c_str(), kNoText) == kNoText)
                    base = std::string(KindKey(kind)) + kRowInfix + kOldPetRow;
                RowSettings& row       = k.rows[r];
                const std::string plate = store.GetString((base + "Plate").c_str(), "");
                row.plate = plate == kExpandingName ? PlateMode::Shown
                                                    : static_cast<PlateMode>(NameIndex(kPlateModeNames, kPlateModeCount, plate, static_cast<int>(row.plate)));
                row.outline = static_cast<OutlineMode>(NameIndex(kOutlineModeNames, kOutlineModeCount, store.GetString((base + "Outline").c_str(), ""), static_cast<int>(row.outline)));
                LoadPick(store, base + "Color", row.outlineColor);
            }
            for (int n = 0; n < kPlateLines; ++n)
                LoadLine(store, LineBase(kind, n), k.lines[n], iconShare);
            const std::string base = std::string(KindKey(kind)) + kOrnamentInfix;
            OrnamentSettings& o    = k.ornament;
            const std::string visibility = store.GetString((base + "Visibility").c_str(), kNoText);
            const std::string wasOn      = store.GetString((base + kOldOrnamentOnSuffix).c_str(), kNoText);
            if (visibility != kNoText)
                o.place = static_cast<OrnamentPlace>(NameIndex(kOrnamentPlaceNames, kOrnamentPlaceCount, visibility, static_cast<int>(o.place)));
            else if (wasOn != kNoText)
                o.place = wasOn != kTrue ? OrnamentPlace::Disabled
                          : store.GetString((base + kOldOrnamentPlaceSuffix).c_str(), "") == kOldOrnamentUpperPlace ? OrnamentPlace::AboveMiddle
                                                                                                                     : OrnamentPlace::BelowMiddle;
            o.width  = LoadInt(store, (base + "Width").c_str(), o.width);
            o.height = LoadInt(store, (base + "Height").c_str(), o.height);
            LoadPick(store, base + "Color", o.color);
        }

        // A kind's opacities and its lines' glow in combat, from a file of any age: the kind's own keys, else what
        // the two settings of before held. Those covered the mobs: one opacity for every mob that was not your target,
        // pets among them, and one switch for the glow of a mob your party had claimed.
        void LoadOpacity(SettingsStore& store, PlateKind kind, KindSettings& k)
        {
            const std::string base = std::string(KindKey(kind)) + kOpacityInfix;
            OpacitySettings& o     = k.opacity;
            const std::string notTargeted = base + "NotTargeted";
            const bool faded              = kind == PlateKind::Mobs || kind == PlateKind::Pets;
            o.noTarget    = LoadInt(store, (base + "NoTarget").c_str(), o.noTarget);
            o.notTargeted = Rebased(store, notTargeted, faded ? kOldUntargetedKey : notTargeted, o.notTargeted,
                [](float before) { return before; });
            o.targeted    = LoadInt(store, (base + "Targeted").c_str(), o.targeted);
            o.fadeIn      = store.GetBool((base + "FadeIn").c_str(), o.fadeIn);
            o.range       = store.GetFloat((base + "Range").c_str(), o.range);
            const std::optional<bool> before = kind == PlateKind::Mobs ? HeldSwitch(store, kOldGlowOffKey) : std::nullopt;
            for (int n = 0; n < kPlateLines; ++n)
            {
                LineDesign& line = k.lines[n];
                line.glowOffInCombat =
                    HeldSwitch(store, (LineBase(kind, n) + kGlowOffSuffix).c_str()).value_or(before.value_or(line.glowOffInCombat));
            }
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
            store.Set((base + "Visibility").c_str(), kOrnamentPlaceNames[static_cast<int>(k.ornament.place)]);
            StoreFloat(store, base + "Width", static_cast<float>(k.ornament.width));
            StoreFloat(store, base + "Height", static_cast<float>(k.ornament.height));
            SavePick(store, base + "Color", k.ornament.color);
            const std::string opacity = std::string(KindKey(kind)) + kOpacityInfix;
            StoreFloat(store, opacity + "NoTarget", static_cast<float>(k.opacity.noTarget));
            StoreFloat(store, opacity + "NotTargeted", static_cast<float>(k.opacity.notTargeted));
            StoreFloat(store, opacity + "Targeted", static_cast<float>(k.opacity.targeted));
            StoreBool(store, opacity + "FadeIn", k.opacity.fadeIn);
            StoreFloat(store, opacity + "Range", k.opacity.range);
        }

        void LoadDesigner(SettingsStore& store, Settings& s)
        {
            const float iconShare = static_cast<float>(s.baseSize) / kDefaultIconSize;
            for (int k = 0; k < kPlateKindCount; ++k)
                LoadKind(store, static_cast<PlateKind>(k), s.kinds[k], iconShare);
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
            StoreBool(store, kDesignsSavedKey, true);
        }

        // A file from before the designer (kDesignerVersion): what the migration reads of it, each field with the default
        // of its day, under the key such a file carries.
        enum class OldIconSide : uint8_t { Left, Right, Hidden };
        const char* const kOldIconSideNames[] = {"left", "right", "hide"};
        constexpr int kOldSideCount           = static_cast<int>(std::size(kOldIconSideNames));
        // A group of mob icons takes a side, never "hide": the names before it.
        constexpr int kOldGroupSideCount      = static_cast<int>(OldIconSide::Hidden);
        enum class OldMobId : uint8_t { Off, LastThree, Decimal, Full };
        const char* const kOldMobIdNames[] = {"off", "lastThree", "decimal", "full"};
        // The player icons as such a file numbers them, GM first: each one's key and the tag that stands for it.
        struct OldPlayerIcon
        {
            const char* key;
            const char* tag;
        };
        constexpr OldPlayerIcon kOldPlayerIcons[] = {{"iconGm", kTagGm}, {"iconMentor", kTagMentor},
            {"iconNewAdventurer", kTagNew}, {"iconLevelSync", kTagSync}, {"iconAway", kTagAway},
            {"iconLinkshell", kTagLinkshell}, {"iconBazaar", kTagBazaar}, {"iconSeekingParty", kTagSeeking}};
        constexpr int kOldPlayerIconCount = static_cast<int>(std::size(kOldPlayerIcons));
        constexpr int kOldGmIcon          = 0;
        constexpr std::array<int, kOldPlayerIconCount> OldPlayerIconOrder()
        {
            std::array<int, kOldPlayerIconCount> order{};
            for (int i = 0; i < kOldPlayerIconCount; ++i)
                order[i] = i;
            return order;
        }

        constexpr int kOldCategoryCount = static_cast<int>(Category::OtherClaim) + 1;
        struct OldSettings
        {
            bool showLabels = true, replaceMobNames = true, replacePlayerNames = true, replaceNpcNames = true;
            bool showIcons = true, showDamageIcons = false, showStatusRankIcons = false, showImmunityIcons = false;
            OldIconSide detectionIconSide = OldIconSide::Left, badgeSide = OldIconSide::Right;
            bool mobIconsInline     = false; // the mob icons on the level line, each group at its side
            bool hideClaimedByParty = false, hideClaimed = false;
            OldMobId mobId          = OldMobId::Off;
            bool markPlaceholders   = true;
            std::string fontName = kDefaultFont, playerFontName = kDefaultFont, npcFontName = kDefaultFont;
            std::string labelFontName = kDefaultFont;
            bool fontBold = false, playerFontBold = false, npcFontBold = false, labelFontBold = false;
            bool nameGlow = true, ownGlowColor = false;
            int glowStrength = 100, glowSize = 100;
            Color glowColor = ColorFromRgb(0xFFB45A);
            bool showOrnament = true, ownOrnamentColor = false;
            int ornamentWidth = 90, ornamentThickness = 6;
            Color ornamentColor = ColorFromRgb(0xD9B46A);
            int nameShadow = 100, labelShadow = 100; // of the mockup's shadow
            Color labelShadowColor = {{0.00f, 0.00f, 0.00f}};
            int mobNameSize = 15, playerNameSize = 15, selfNameSize = 15, npcNameSize = 15;
            int labelSize = 13, timerSize = 13, iconSize = 16;
            int playerIconSize   = 100; // percent of the name's letter height
            bool showPlayerIcons = true;
            OldIconSide playerIconSide[kOldPlayerIconCount] = {};
            std::array<int, kOldPlayerIconCount> playerIconOrder = OldPlayerIconOrder();
            Color cursorColor           = ColorFromRgb(kPhoenixRed);
            Color lockedCursorColor     = {{0.65f, 0.40f, 1.00f}};
            Color subCursorColor        = {{0.99f, 0.82f, 0.09f}};
            Color outOfRangeCursorColor = {{1.00f, 0.25f, 0.25f}};
            bool ownNameColor = false;
            Color nameColor   = {{1.00f, 1.00f, 1.00f}};
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
            // By Category, the eight such a file knew: whether its mobs were outlined, and in what color.
            bool show[kOldCategoryCount]   = {true, false, false, true, true, true, false, false};
            Color color[kOldCategoryCount] = {
                {{1.00f, 0.15f, 0.15f}}, // aggressive
                {{0.20f, 1.00f, 0.30f}}, // passive
                {{0.70f, 0.70f, 0.70f}}, // unknown
                {{1.00f, 0.60f, 0.10f}}, // aggressive NM
                {{1.00f, 0.84f, 0.00f}}, // passive NM
                {{0.70f, 0.30f, 1.00f}}, // lottery placeholder
                {{0.30f, 0.65f, 1.00f}}, // claimed by your party
                {{0.55f, 0.45f, 0.40f}}, // claimed by someone else
            };
        };

        struct OldBoolKey
        {
            const char* key;
            bool OldSettings::*field;
        };
        constexpr OldBoolKey kOldBools[] = {{"showLabels", &OldSettings::showLabels},
            {"replaceMobNames", &OldSettings::replaceMobNames}, {"showIcons", &OldSettings::showIcons},
            // The setting's first name, which saved files carry.
            {"hideInCombat", &OldSettings::hideClaimedByParty}, {"hideClaimed", &OldSettings::hideClaimed},
            {"markPlaceholders", &OldSettings::markPlaceholders}, {"fontBold", &OldSettings::fontBold},
            {"nameGlow", &OldSettings::nameGlow}, {"showOrnament", &OldSettings::showOrnament},
            {"ownGlowColor", &OldSettings::ownGlowColor}, {"ownOrnamentColor", &OldSettings::ownOrnamentColor},
            {"replacePlayerNames", &OldSettings::replacePlayerNames}, {"replaceNpcNames", &OldSettings::replaceNpcNames},
            {"showPlayerIcons", &OldSettings::showPlayerIcons}, {"ownNameColor", &OldSettings::ownNameColor},
            {"showDamageIcons", &OldSettings::showDamageIcons}, {"showStatusRankIcons", &OldSettings::showStatusRankIcons},
            {"showImmunityIcons", &OldSettings::showImmunityIcons}};

        struct OldIntKey
        {
            const char* key;
            int OldSettings::*field;
        };
        constexpr OldIntKey kOldInts[] = {{"mobNameSize", &OldSettings::mobNameSize},
            {"playerNameSize", &OldSettings::playerNameSize}, {"selfNameSize", &OldSettings::selfNameSize},
            {"npcNameSize", &OldSettings::npcNameSize}, {"labelSize", &OldSettings::labelSize},
            {"timerSize", &OldSettings::timerSize}, {"iconSize", &OldSettings::iconSize},
            {"playerIconSize", &OldSettings::playerIconSize}, {"glowStrength", &OldSettings::glowStrength},
            {"glowSize", &OldSettings::glowSize}, {"ornamentWidth", &OldSettings::ornamentWidth},
            {"ornamentThickness", &OldSettings::ornamentThickness}, {"nameShadow", &OldSettings::nameShadow},
            {"labelShadow", &OldSettings::labelShadow}};

        struct OldColorKey
        {
            const char* key;
            Color OldSettings::*field;
        };
        constexpr OldColorKey kOldColors[] = {{"cursorColor", &OldSettings::cursorColor},
            {"subCursorColor", &OldSettings::subCursorColor}, {"outOfRangeCursorColor", &OldSettings::outOfRangeCursorColor},
            {"lockedCursorColor", &OldSettings::lockedCursorColor}, {"nameColor", &OldSettings::nameColor},
            {"textOutline", &OldSettings::textOutline}, {"glowColor", &OldSettings::glowColor},
            {"ornamentColor", &OldSettings::ornamentColor}};

        constexpr const char* kOldFontKey = "fontName";
        // Files from before players and NPCs had their own name fonts take the mobs'.
        constexpr const char* kOldPlayerFontKey = "playerFontName";
        constexpr const char* kOldPlayerBoldKey = "playerFontBold";
        constexpr const char* kOldNpcFontKey    = "npcFontName";
        constexpr const char* kOldNpcBoldKey    = "npcFontBold";
        // Files from before the level line had its own font and shadow take the name's.
        constexpr const char* kOldLabelFontKey        = "labelFontName";
        constexpr const char* kOldLabelBoldKey        = "labelFontBold";
        constexpr const char* kOldLabelShadowColorKey = "labelShadowColor";
        // And from before each kind of name had its own size, one for all of them.
        constexpr const char* kOldOneNameSizeKey = "nameSize";
        constexpr Color kOldCursorColor          = {{1.0f, 1.0f, 1.0f}};
        constexpr const char* kOldFont           = "Trebuchet MS";
        constexpr const char* kOldShowSuffix     = "Show";
        const char* const kOldCategoryKeys[kOldCategoryCount] = {"willAttack", "wontAttack", "unknown", "nmWillAttack",
            "nmWontAttack", "placeholder", "partyClaim", "otherClaim"};
        constexpr const char* kOldMobIdKey           = "mobId";
        constexpr const char* kOldDetectionSideKey   = "detectionIconSide";
        constexpr const char* kOldBadgeSideKey       = "badgeSide";
        constexpr const char* kOldIconPlaceKey       = "mobIconPlace";
        constexpr const char* kOldInlineIcons        = "inline";
        constexpr const char* kOldPlayerIconOrderKey = "playerIconOrder"; // kOldPlayerIcons' keys, a space apart

        // saved is the file's version: the defaults that changed before it are put right, as kVersionKey says.
        OldSettings LoadOld(SettingsStore& store, float saved)
        {
            OldSettings o;
            for (const OldBoolKey& b : kOldBools)
                o.*b.field = store.GetBool(b.key, o.*b.field);
            o.mobNameSize = o.playerNameSize = o.selfNameSize = o.npcNameSize = LoadInt(store, kOldOneNameSizeKey, o.mobNameSize);
            for (const OldIntKey& i : kOldInts)
                o.*i.field = LoadInt(store, i.key, o.*i.field);
            for (const OldColorKey& c : kOldColors)
                LoadColor(store, c.key, o.*c.field);
            o.fontName         = store.GetString(kOldFontKey, kDefaultFont);
            o.labelFontName    = store.GetString(kOldLabelFontKey, o.fontName.c_str());
            o.labelFontBold    = store.GetBool(kOldLabelBoldKey, o.fontBold);
            o.labelShadowColor = o.textOutline;
            LoadColor(store, kOldLabelShadowColorKey, o.labelShadowColor);
            for (int k = 0; k < kLabelShadeCount; ++k)
                LoadColor(store, kLabelShadeKeys[k], o.labelColor[k]);
            for (int c = 0; c < kOldCategoryCount; ++c)
            {
                const std::string base = kOldCategoryKeys[c];
                o.show[c] = store.GetBool((base + kOldShowSuffix).c_str(), o.show[c]);
                LoadColor(store, base, o.color[c]);
            }
            auto side = [&](const char* key, int sides, OldIconSide fallback) {
                const int named = NameIndex(kOldIconSideNames, sides, store.GetString(key, ""), static_cast<int>(fallback));
                return static_cast<OldIconSide>(named);
            };
            for (int i = 0; i < kOldPlayerIconCount; ++i)
                o.playerIconSide[i] = side(kOldPlayerIcons[i].key, kOldSideCount, o.playerIconSide[i]);
            // An order that does not name every icon once is none.
            std::istringstream order(store.GetString(kOldPlayerIconOrderKey, ""));
            std::vector<int> icons;
            for (std::string name; order >> name;)
                for (int i = 0; i < kOldPlayerIconCount; ++i)
                    if (name == kOldPlayerIcons[i].key && std::ranges::find(icons, i) == icons.end()) icons.push_back(i);
            if (std::ssize(icons) == kOldPlayerIconCount) std::ranges::copy(icons, o.playerIconOrder.begin());
            o.detectionIconSide = side(kOldDetectionSideKey, kOldGroupSideCount, o.detectionIconSide);
            o.badgeSide         = side(kOldBadgeSideKey, kOldGroupSideCount, o.badgeSide);
            o.mobIconsInline    = store.GetString(kOldIconPlaceKey, "") == kOldInlineIcons;
            o.mobId = static_cast<OldMobId>(NameIndex(kOldMobIdNames, static_cast<int>(std::size(kOldMobIdNames)),
                store.GetString(kOldMobIdKey, ""), static_cast<int>(o.mobId)));
            if (saved < kRedCursorVersion && o.cursorColor == kOldCursorColor) o.cursorColor = OldSettings{}.cursorColor;
            if (saved < kBundledFontVersion)
            {
                if (o.fontName == kOldFont) o.fontName = kDefaultFont;
                if (o.labelFontName == kOldFont) o.labelFontName = kDefaultFont;
            }
            o.playerFontName = store.GetString(kOldPlayerFontKey, o.fontName.c_str());
            o.playerFontBold = store.GetBool(kOldPlayerBoldKey, o.fontBold);
            o.npcFontName    = store.GetString(kOldNpcFontKey, o.fontName.c_str());
            o.npcFontBold    = store.GetBool(kOldNpcBoldKey, o.fontBold);
            return o;
        }

        // The icons beside a name, as the fixed plate laid them: those on the left in the order's turn, GM always last
        // of them and so nearest the name whatever its side says, then the name, then those on the right.
        std::string PlayerNameText(const OldSettings& o)
        {
            if (!o.showPlayerIcons) return kTagName;
            std::string left, right;
            for (const int icon : o.playerIconOrder)
            {
                if (icon == kOldGmIcon) continue;
                if (o.playerIconSide[icon] == OldIconSide::Left) left += kOldPlayerIcons[icon].tag;
                else if (o.playerIconSide[icon] == OldIconSide::Right) right += kOldPlayerIcons[icon].tag;
            }
            return left + kTagGm + kTagName + right;
        }

        // The look such a file had, as the designer's settings.
        void MigrateDesigner(const OldSettings& o, Settings& s)
        {
            KindSettings& mobs      = s.Kind(PlateKind::Mobs);
            KindSettings& players   = s.Kind(PlateKind::Players);
            KindSettings& self      = s.Kind(PlateKind::Self);
            KindSettings& npcs      = s.Kind(PlateKind::Npcs);
            KindSettings& pets      = s.Kind(PlateKind::Pets); // drawn as mobs then: the mobs' keys covered them
            s.detectionShown        = kAllDetection;           // such a file showed every icon, the aggression one too
            LineDesign& mobIcons    = mobs.lines[kMobIconLine];
            LineDesign& mobLevel    = mobs.lines[kMobLevelLine];
            LineDesign& mobName     = mobs.lines[kNameLine];
            const int base          = std::clamp(o.mobNameSize, kMinTextSize, kMaxTextSize);
            auto percent            = [&](int px) {
                const float pixels = static_cast<float>(std::clamp(px, kMinTextSize, kMaxTextSize));
                return static_cast<int>(std::lround(kPercentScale * pixels / static_cast<float>(base)));
            };
            // An icon's pixels as a percent of the base icon size, which such a file leaves at its default.
            auto iconScale          = [](float pixels) { return static_cast<int>(std::lround(kPercentScale * pixels / kDefaultIconSize)); };
            // The icons are playerIconSize percent of the name they stand beside.
            auto IconPercent        = [&](int nameSize) {
                const float pixels = static_cast<float>(std::clamp(nameSize, kMinTextSize, kMaxTextSize));
                return iconScale(static_cast<float>(o.playerIconSize) / kPercentScale * pixels);
            };
            auto fontOf             = [](const std::string& font) { return font == kDefaultFont ? std::string() : font; };
            auto boldOf             = [](bool bold) { return bold ? StyleBit(FontStyle::Bold) : uint8_t{0}; };
            auto setVanilla         = [](KindSettings& k, PlateKind kind) {
                for (int r = 0; r < RowCountOf(kind); ++r)
                    k.rows[r].plate = PlateMode::Vanilla;
            };
            // Such a file drew a pet's name as any mob's, where a pet row of today shows it when targeted.
            for (int r = 0; r < RowCountOf(PlateKind::Pets); ++r)
                pets.rows[r].plate = PlateMode::Shown;
            if (!o.replaceMobNames)
            {
                setVanilla(mobs, PlateKind::Mobs);
                setVanilla(pets, PlateKind::Pets);
            }
            if (!o.replacePlayerNames)
            {
                setVanilla(players, PlateKind::Players);
                setVanilla(self, PlateKind::Self);
            }
            if (!o.replaceNpcNames) setVanilla(npcs, PlateKind::Npcs);

            const Category natureCategory[kNatureCount] = {Category::WontAttack, Category::WillAttack, Category::NmWontAttack,
                Category::NmWillAttack, Category::Placeholder, Category::Unknown};
            for (int n = 0; n < kNatureCount; ++n)
            {
                const int c = CategoryIndex(natureCategory[n]);
                const bool steps = CanOutlineByNature(PlateKind::Mobs, c);
                mobs.rows[c].outline = o.show[c] ? OutlineMode::Shown : steps ? OutlineMode::ByNature : OutlineMode::Disabled;
                s.palette.aggression[n] = o.color[c];
            }
            // Switched off, a claimed mob kept the outline its nature gave it.
            for (const Category claim : {Category::PartyClaim, Category::OtherClaim})
            {
                const int c = CategoryIndex(claim);
                mobs.rows[c].outline = o.show[c] ? OutlineMode::Shown : OutlineMode::ByNature;
                mobs.rows[c].outlineColor.source = ColorSource::Claim;
            }
            s.palette.claim[static_cast<int>(ClaimState::Yours)]  = o.color[CategoryIndex(Category::PartyClaim)];
            s.palette.claim[static_cast<int>(ClaimState::Others)] = o.color[CategoryIndex(Category::OtherClaim)];
            std::ranges::copy(o.labelColor, s.palette.toughness);
            s.palette.target[static_cast<int>(TargetState::Targeted)]   = o.cursorColor;
            s.palette.target[static_cast<int>(TargetState::Locked)]     = o.lockedCursorColor;
            s.palette.target[static_cast<int>(TargetState::SubTarget)]  = o.subCursorColor;
            s.palette.target[static_cast<int>(TargetState::OutOfRange)] = o.outOfRangeCursorColor;
            s.cursorEdgeColor.custom = o.textOutline;
            // Every shadow such a file drew was of the unit before, a line's it never set included.
            auto strength = [](int before) { return Whole(ShadowStrengthOf(static_cast<float>(before))); };
            for (KindSettings& k : s.kinds)
                for (LineDesign& line : k.lines)
                {
                    line.shadowColor.custom = o.textOutline;
                    line.shadow             = strength(OldSettings{}.nameShadow);
                }
            mobLevel.shadowColor.custom = o.labelShadowColor;
            mobLevel.shadow             = strength(o.labelShadow);

            mobIcons.font = mobName.font = pets.lines[kNameLine].font = fontOf(o.fontName);
            mobIcons.style = mobName.style = pets.lines[kNameLine].style = boldOf(o.fontBold);
            mobLevel.font = fontOf(o.labelFontName);
            mobLevel.style = boldOf(o.labelFontBold);
            for (const PlateKind kind : {PlateKind::Players, PlateKind::Self})
            {
                LineDesign& line = s.Kind(kind).lines[kNameLine];
                line.font        = fontOf(o.playerFontName);
                line.style       = boldOf(o.playerFontBold);
                line.text        = PlayerNameText(o);
            }
            players.lines[kNameLine].iconSize = IconPercent(o.playerNameSize);
            self.lines[kNameLine].iconSize    = IconPercent(o.selfNameSize);
            npcs.lines[kNameLine].font = fontOf(o.npcFontName);
            npcs.lines[kNameLine].style = boldOf(o.npcFontBold);

            s.baseSize                         = base;
            mobLevel.textSize                  = percent(o.labelSize);
            mobIcons.iconSize                  = iconScale(static_cast<float>(std::clamp(o.iconSize, kMinTextSize, kMaxTextSize)));
            players.lines[kNameLine].textSize  = percent(o.playerNameSize);
            self.lines[kNameLine].textSize     = percent(o.selfNameSize);
            npcs.lines[kNameLine].textSize     = percent(o.npcNameSize);
            s.timerPercent                     = percent(o.timerSize);

            std::string level;
            auto append = [](std::string& text, const std::string& part) { text += (text.empty() || part.empty() ? "" : " ") + part; };
            if (o.showLabels) append(level, kTagLevelCon);
            if (o.mobId == OldMobId::LastThree) append(level, kTagId);
            if (o.mobId == OldMobId::Decimal) append(level, kTagServerId);
            if (o.mobId == OldMobId::Full) append(level, std::string(kTagServerId) + " " + kTagId);
            if (o.mobId != OldMobId::Off && o.markPlaceholders) append(level, kTagPlaceholder);
            mobLevel.text = level;
            std::string badges;
            if (o.showDamageIcons) badges += kTagWeakResist;
            if (o.showStatusRankIcons) badges += kTagStatus;
            if (o.showImmunityIcons) badges += kTagImmune;
            const std::string detect = o.showIcons ? kTagDetect : "";
            const bool badgesLeft    = o.badgeSide == OldIconSide::Left;
            if (o.mobIconsInline && !level.empty())
            {
                const bool detectLeft   = o.detectionIconSide == OldIconSide::Left;
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
                mobIcons.text = badgesLeft ? badges + detect : detect + badges;
            auto condition = [&]() -> LineShow {
                if (o.hideClaimed) return LineShow{false, ConditionBit(ShowCondition::Unclaimed)};
                if (o.hideClaimedByParty)
                    return LineShow{false, ConditionBit(ShowCondition::Unclaimed) | ConditionBit(ShowCondition::OthersClaim)};
                return LineShow{};
            };
            mobLevel.show = condition();
            if (!mobIcons.text.empty()) mobIcons.show = mobLevel.show;

            mobName.glow     = o.nameGlow ? o.glowStrength : 0;
            mobName.glowSize = o.glowSize;
            if (o.ownGlowColor) mobName.glowColor = ColorPick{ColorSource::Custom, o.glowColor};
            mobs.ornament.place  = o.showOrnament ? OrnamentPlace::BelowMiddle : OrnamentPlace::Disabled;
            mobs.ornament.width  = o.ornamentWidth;
            mobs.ornament.height = o.ornamentThickness;
            if (o.ownOrnamentColor) mobs.ornament.color = ColorPick{ColorSource::Custom, o.ornamentColor};
            for (KindSettings& k : s.kinds)
            {
                k.lines[kNameLine].shadow = strength(o.nameShadow);
                if (o.ownNameColor) k.lines[kNameLine].color = ColorPick{ColorSource::Custom, o.nameColor};
            }
        }

        bool SameIgnoringCase(const std::string& a, const std::string& b)
        {
            return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
                return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
            });
        }
    }

    bool NameplatesOn(const Settings& s)
    {
        return s.enabled && (s.nameplates || s.cursorStyle != CursorStyle::Vanilla || s.phTimers);
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
        // A source the pick's own dropdown does not offer, as a file from before may hold, goes back to the default.
        void ClampPick(ColorPick& pick, const ColorPick& fallback, const std::vector<ColorSource>& offered)
        {
            if (std::ranges::find(offered, pick.source) == offered.end()) pick.source = fallback.source;
            pick.custom = ClampColor(pick.custom, fallback.custom);
        }

        // conditions holds the bits of the conditions the line's kind offers: one it does not could show the line
        // while the menu, which lists only those, reads Never.
        void ClampLine(LineDesign& line, const LineDesign& d, const std::vector<ColorSource>& offered, uint32_t conditions)
        {
            std::ranges::replace(line.text, '\n', ' ');
            if (line.text.size() > kMaxTemplate)
            {
                size_t end = kMaxTemplate;
                while (end > 0 && ContinuesCharacter(line.text[end])) --end;
                line.text.resize(end);
            }
            if (line.font.size() > kMaxFontName) line.font.clear();
            if (static_cast<int>(line.align) >= kAlignCount) line.align = d.align;
            line.style &= kAllFontStyles;
            line.show.when &= conditions;
            line.textSize  = std::clamp(line.textSize, kMinSizePercent, kMaxSizePercent);
            line.iconSize  = std::clamp(line.iconSize, kMinSizePercent, kMaxSizePercent);
            line.padding   = std::clamp(line.padding, 0, kMaxLinePadding);
            line.shadow    = std::clamp(line.shadow, kMinShadow, kMaxShadow);
            line.glow      = std::clamp(line.glow, kMinGlow, kMaxGlow);
            line.glowSize  = std::clamp(line.glowSize, kMinGlowSize, kMaxGlowSize);
            ClampPick(line.color, d.color, offered);
            ClampPick(line.shadowColor, d.shadowColor, offered);
            ClampPick(line.glowColor, d.glowColor, offered);
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
                const std::vector<ColorSource> text = SourcesFor(static_cast<PlateKind>(k), true);
                const std::vector<ColorSource> plain = SourcesFor(static_cast<PlateKind>(k), false);
                uint32_t conditions = 0;
                for (const ShowCondition condition : ConditionsFor(static_cast<PlateKind>(k)))
                    conditions |= ConditionBit(condition);
                for (int r = 0; r < kMaxRows; ++r)
                {
                    RowSettings& row = kind.rows[r];
                    if (static_cast<int>(row.plate) >= kPlateModeCount) row.plate = defaults.rows[r].plate;
                    if (static_cast<int>(row.outline) >= kOutlineModeCount ||
                        (row.outline == OutlineMode::ByNature && !CanOutlineByNature(static_cast<PlateKind>(k), r)))
                        row.outline = defaults.rows[r].outline;
                    ClampPick(row.outlineColor, defaults.rows[r].outlineColor, plain);
                }
                for (int n = 0; n < kPlateLines; ++n)
                    ClampLine(kind.lines[n], defaults.lines[n], text, conditions);
                BreakSyncCycles(kind.lines);
                OrnamentSettings& o = kind.ornament;
                if (static_cast<int>(o.place) >= kOrnamentPlaceCount) o.place = defaults.ornament.place;
                o.width  = std::clamp(o.width, kMinOrnamentWidth, kMaxOrnamentWidth);
                o.height = std::clamp(o.height, kMinOrnamentThickness, kMaxOrnamentThickness);
                ClampPick(o.color, defaults.ornament.color, plain);
                OpacitySettings& opacity = kind.opacity;
                for (int OpacitySettings::*percent : {&OpacitySettings::noTarget, &OpacitySettings::notTargeted, &OpacitySettings::targeted})
                    opacity.*percent = std::clamp(opacity.*percent, kMinOpacity, kMaxOpacity);
                opacity.range = ClampFloat(opacity.range, kMinOutlineDistance, kMaxOutlineDistance, defaults.opacity.range);
                // Your own nameplate is never far from you.
                if (static_cast<PlateKind>(k) == PlateKind::Self) opacity.fadeIn = false;
            }
            Palette fallback = d.palette;
            const std::vector<PaletteEntry> entries = PaletteEntries(s.palette), defaults = PaletteEntries(fallback);
            for (size_t i = 0; i < entries.size(); ++i)
                *entries[i].color = ClampColor(*entries[i].color, *defaults[i].color);
            if (s.defaultFont.empty() || s.defaultFont.size() > kMaxFontName) s.defaultFont = d.defaultFont;
            s.detectionShown &= kAllDetection;
            std::vector<std::string> friends;
            for (std::string& name : s.friends)
                if (IsCharacterName(name) &&
                    std::ranges::none_of(friends, [&](const std::string& kept) { return SameIgnoringCase(kept, name); }))
                    friends.push_back(std::move(name));
            s.friends = std::move(friends);
        }
    }

    int DefaultCursorWidth() { return NaturalCursorWidth(kDefaultCursorStyle, kDefaultCursorHeight); }

    const char* CursorStyleName(CursorStyle style) { return kCursorStyleNames[static_cast<int>(style)]; }

    Settings Clamp(Settings s)
    {
        const Settings d;
        for (const FloatKey& f : kFloats)
            s.*f.field = ClampFloat(s.*f.field, f.lo, f.hi, d.*f.field);
        for (const IntKey& i : kInts)
            s.*i.field = std::clamp(s.*i.field, i.lo, i.hi);
        for (const IntKey& i : kRebasedInts)
            s.*i.field = std::clamp(s.*i.field, i.lo, i.hi);
        s.smoothness = SmoothnessCopies(SmoothnessLevelOf(s.smoothness));
        if (s.badgeFontName.size() > kMaxFontName) s.badgeFontName = d.badgeFontName;
        if (s.barTitleFont.size() > kMaxFontName) s.barTitleFont = d.barTitleFont;
        if (s.barTextFont.size() > kMaxFontName) s.barTextFont = d.barTextFont;
        std::vector<uint8_t> keys;
        for (const uint8_t vk : s.keepPointerKeys)
        {
            const uint8_t key = BindableKey(vk);
            if (key != 0 && std::ranges::find(keys, key) == keys.end() && keys.size() < kMaxKeptKeys) keys.push_back(key);
        }
        s.keepPointerKeys = std::move(keys);
        s.screenshotKey   = BindableKey(s.screenshotKey);
        s.movedWindows    = MovedWindowsFrom(MovedWindowNames(s.movedWindows));
        s.windowOffsets   = WindowOffsetsFrom(WindowOffsetText(s.windowOffsets));
        s.windowGroups    = WindowGroupsFrom(WindowGroupsText(s.windowGroups));
        s.helpBarX        = std::clamp(s.helpBarX, -kMaxHelpBarOffset, kMaxHelpBarOffset);
        s.helpBarY        = std::clamp(s.helpBarY, -kMaxHelpBarOffset, kMaxHelpBarOffset);
        for (const ColorKey& c : kColors)
            s.*c.field = ClampColor(s.*c.field, d.*c.field);
        // The file's name goes into a path under the cursors folder: one that would lead out of it is no name.
        const bool named = !s.cursorFile.empty() && s.cursorFile.size() <= kMaxCursorFileName &&
                           s.cursorFile.find_first_of(kPathCharacters) == std::string::npos;
        if (!named) s.cursorFile.clear();
        if (static_cast<int>(s.cursorStyle) >= kCursorStyleCount || (s.cursorStyle == CursorStyle::File && !named))
            s.cursorStyle = d.cursorStyle;
        if (static_cast<int>(s.dashMark) >= kDashMarkCount) s.dashMark = d.dashMark;
        ClampPick(s.cursorColor, d.cursorColor, CursorSources());
        ClampPick(s.cursorEdgeColor, d.cursorEdgeColor, CursorSources());
        if (static_cast<int>(s.barTheme) >= kBarThemeCount) s.barTheme = d.barTheme;
        if (s.barCurrency < 0 || s.barCurrency >= kCurrencyCount) s.barCurrency = d.barCurrency;
        if (s.clockZone && static_cast<int>(*s.clockZone) >= kClockZoneCount) s.clockZone = d.clockZone;
        if (static_cast<int>(s.expBar) >= kExpBarPlaceCount) s.expBar = d.expBar;
        if (static_cast<int>(s.jobLayout) >= kJobLayoutCount) s.jobLayout = d.jobLayout;
        if (static_cast<int>(s.barSubtitle) >= kSubtitlePlaceCount) s.barSubtitle = d.barSubtitle;
        if (MenuFontNamed(s.menuFont) == nullptr) s.menuFont = d.menuFont;
        s.menuScale = std::clamp(s.menuScale, kMinMenuScale, kMaxMenuScale);
        if (static_cast<int>(s.menuTheme) >= kMenuThemeCount) s.menuTheme = d.menuTheme;
        for (int* side : {&s.menuX, &s.menuY, &s.menuWidth, &s.menuHeight})
            *side = std::clamp(*side, 0, kMaxMenuSide);
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
        // Asked before anything is read: a typed read adds its key. Every build's file holds these two, and a store
        // with neither is a new install, which has today's defaults and no look from before to keep.
        const bool file = store.GetString(kVersionKey, kNoText) != kNoText || store.GetString(kBools[0].key, kNoText) != kNoText;
        Settings s;
        for (const BoolKey& b : kBools)
            s.*b.field = store.GetBool(b.key, s.*b.field);
        for (const IntKey& i : kInts)
            s.*i.field = LoadInt(store, i.key, s.*i.field);
        // The shape a file from before had, which also gave its one size a width.
        const bool wasArrow = !HeldSwitch(store, kOldCursorFeatherKey).value_or(true);
        const std::string style = store.GetString(kCursorStyleKey, kNoText);
        if (style != kNoText)
            s.cursorStyle = static_cast<CursorStyle>(NameIndex(kCursorStyleNames, kCursorStyleCount, style, static_cast<int>(s.cursorStyle)));
        else if (!HeldSwitch(store, kOldReplaceCursorKey).value_or(true))
            s.cursorStyle = CursorStyle::Vanilla;
        else if (wasArrow)
            s.cursorStyle = CursorStyle::Triangle;
        s.cursorFile   = store.GetString(kCursorFileKey, "");
        s.dashMark     = static_cast<DashMark>(
            NameIndex(kDashMarkNames, kDashMarkCount, store.GetString(kDashMarkKey, ""), static_cast<int>(s.dashMark)));
        s.cursorHeight = Rebased(store, kCursorHeightKey, kOldCursorSizeKey, s.cursorHeight, [](float size) { return size; });
        s.cursorWidth  = Rebased(store, kCursorWidthKey, kOldCursorSizeKey, s.cursorWidth, [&](float size) {
            return static_cast<float>(NaturalCursorWidth(wasArrow ? CursorStyle::Triangle : CursorStyle::Feather, Whole(size)));
        });
        s.cursorOutlineThickness = Rebased(store, kCursorEdgeKey, kOldCursorOutlineKey, s.cursorOutlineThickness,
            [](float before) { return before * kCursorEdgeRebase; });
        for (const FloatKey& f : kFloats)
            s.*f.field = store.GetFloat(f.key, s.*f.field);
        for (const ColorKey& c : kColors)
            LoadColor(store, c.key, s.*c.field);
        LoadPick(store, kCursorPickKey, s.cursorColor);
        LoadPick(store, kCursorEdgePickKey, s.cursorEdgeColor);
        // A file from before badges had a font of their own to name keeps the one they were set in.
        s.badgeFontName = store.GetString(kBadgeFontKey, file ? kBadgeFont : s.badgeFontName.c_str());
        s.barTitleFont  = store.GetString(kBarTitleFontKey, "");
        s.barTextFont   = store.GetString(kBarTextFontKey, "");
        s.scaleCursor   = store.GetBool(kScaleCursorKey, file ? s.scaleWithDistance : s.scaleCursor);
        std::istringstream keys(store.GetString(kKeepPointerKey, ""));
        for (std::string key; keys >> key;)
        {
            char* end        = nullptr;
            const long value = std::strtol(key.c_str(), &end, kHex);
            if (*end == '\0' && value > 0 && value <= UINT8_MAX) s.keepPointerKeys.push_back(static_cast<uint8_t>(value));
        }
        for (int i = 0; i < kStatusRankCount; ++i)
        {
            const std::string base = kStatusKeys[i];
            StatusBadgeRule& rule  = s.statusBadges[i];
            rule.show              = store.GetBool((base + kStatusShowSuffix).c_str(), rule.show);
            rule.weakRank          = LoadInt(store, (base + kWeakRankSuffix).c_str(), rule.weakRank);
            rule.resistRank        = LoadInt(store, (base + kResistRankSuffix).c_str(), rule.resistRank);
        }
        const std::string theme = store.GetString(kBarThemeKey, "");
        for (int t = 0; t < kBarThemeCount; ++t)
            if (theme == kBarThemeNames[t]) s.barTheme = static_cast<BarTheme>(t);
        if (const std::optional<int> currency = CurrencyOfKey(store.GetString(kBarCurrencyKey, "")))
            s.barCurrency = *currency;
        s.barTargetDetails = store.GetBool(kTargetDetailsKey, s.barTargetDetails);
        s.menuFont         = store.GetString(kMenuFontKey, s.menuFont.c_str());
        s.menuScale        = LoadInt(store, kMenuScaleKey, s.menuScale);
        const std::string menuTheme = store.GetString(kMenuThemeKey, "");
        for (int t = 0; t < kMenuThemeCount; ++t)
            if (menuTheme == kMenuThemeNames[t]) s.menuTheme = static_cast<MenuTheme>(t);
        s.menuX      = LoadInt(store, kMenuXKey, s.menuX);
        s.menuY      = LoadInt(store, kMenuYKey, s.menuY);
        s.menuWidth  = LoadInt(store, kMenuWidthKey, s.menuWidth);
        s.menuHeight = LoadInt(store, kMenuHeightKey, s.menuHeight);
        const std::string clockZone = store.GetString(kClockZoneKey, "");
        for (int z = 0; z < kClockZoneCount; ++z)
            if (clockZone == kClockZoneNames[z]) s.clockZone = static_cast<ClockZone>(z);
        // A file from when Server time was a widget, and shown, has the Clock stack it.
        s.clockServerTime =
            store.GetBool(kClockServerTimeKey, store.GetString(kServerTimeWidgetKey, "") == kTrue);
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
        const std::string jobLayout = store.GetString(kJobLayoutKey, "");
        for (int l = 0; l < kJobLayoutCount; ++l)
            if (jobLayout == kJobLayoutNames[l]) s.jobLayout = static_cast<JobLayout>(l);
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
        if (saved >= kDesignerVersion || store.GetBool(kDesignsSavedKey, false)) LoadDesigner(store, s);
        else if (file) MigrateDesigner(LoadOld(store, saved), s);
        for (int k = 0; k < kPlateKindCount; ++k)
            LoadOpacity(store, static_cast<PlateKind>(k), s.kinds[k]);
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
        for (const IntKey& i : kRebasedInts)
            setFloat(i.key, static_cast<float>(s.*i.field));
        for (const FloatKey& f : kFloats)
            setFloat(f.key, s.*f.field);
        for (const ColorKey& c : kColors)
            setColor(c.key, s.*c.field);
        store.Set(kCursorStyleKey, CursorStyleName(s.cursorStyle));
        store.Set(kCursorFileKey, s.cursorFile.c_str());
        store.Set(kDashMarkKey, kDashMarkNames[static_cast<int>(s.dashMark)]);
        SavePick(store, kCursorPickKey, s.cursorColor);
        SavePick(store, kCursorEdgePickKey, s.cursorEdgeColor);
        store.Set(kBadgeFontKey, s.badgeFontName.c_str());
        store.Set(kBarTitleFontKey, s.barTitleFont.c_str());
        store.Set(kBarTextFontKey, s.barTextFont.c_str());
        setBool(kScaleCursorKey, s.scaleCursor);
        std::string keys;
        for (const uint8_t vk : s.keepPointerKeys)
        {
            char hex[4];
            std::snprintf(hex, sizeof(hex), "%02X", vk);
            keys += (keys.empty() ? "" : " ") + std::string(hex);
        }
        store.Set(kKeepPointerKey, keys.c_str());
        for (int i = 0; i < kStatusRankCount; ++i)
        {
            const std::string base = kStatusKeys[i];
            setBool(base + kStatusShowSuffix, s.statusBadges[i].show);
            setFloat(base + kWeakRankSuffix, static_cast<float>(s.statusBadges[i].weakRank));
            setFloat(base + kResistRankSuffix, static_cast<float>(s.statusBadges[i].resistRank));
        }
        store.Set(kBarThemeKey, kBarThemeNames[static_cast<int>(s.barTheme)]);
        store.Set(kBarSeparatorsKey, kBarSeparatorsNames[static_cast<int>(s.barSeparators)]);
        store.Set(kBarCurrencyKey, kCurrencyTable[s.barCurrency].key);
        store.Set(kClockZoneKey, s.clockZone ? kClockZoneNames[static_cast<int>(*s.clockZone)] : kLocalZone);
        setBool(kTargetDetailsKey, s.barTargetDetails);
        store.Set(kMenuFontKey, s.menuFont.c_str());
        setFloat(kMenuScaleKey, static_cast<float>(s.menuScale));
        store.Set(kMenuThemeKey, kMenuThemeNames[static_cast<int>(s.menuTheme)]);
        setFloat(kMenuXKey, static_cast<float>(s.menuX));
        setFloat(kMenuYKey, static_cast<float>(s.menuY));
        setFloat(kMenuWidthKey, static_cast<float>(s.menuWidth));
        setFloat(kMenuHeightKey, static_cast<float>(s.menuHeight));
        for (int w = 0; w < kBarWidgetCount; ++w)
        {
            const std::string key = kBarWidgetKeys[w];
            setBool(key, s.barWidgets[w]);
            setBool(key + kSecondLineSuffix, s.barSecondLine[w]);
            setBool(key + kIconsSuffix, s.barIcons[w]);
        }
        store.Set(kExpBarKey, kExpBarNames[static_cast<int>(s.expBar)]);
        store.Set(kJobLayoutKey, kJobLayoutNames[static_cast<int>(s.jobLayout)]);
        setBool(kJobNamesKey, s.jobNames);
        store.Set(kSubtitleKey, kSubtitleNames[static_cast<int>(s.barSubtitle)]);
        setBool(kClock24Key, s.clock24);
        setBool(kClockServerTimeKey, s.clockServerTime);
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

    void ChangedKeys::Set(const char* key, const char* value)
    {
        const auto [held, added] = m_Held.try_emplace(key, value);
        if (!added)
        {
            if (held->second == value) return;
            held->second = value;
        }
        if (!m_Passing) return;
        m_Store.Set(key, value);
        if (const auto waiting = std::ranges::find(m_Unwritten, key, &KeyText::key); waiting != m_Unwritten.end())
            waiting->text = value;
        else
            m_Unwritten.push_back(KeyText{key, value});
    }

    Settings ChangedKeys::Load()
    {
        m_Held.clear();
        m_Unwritten.clear();
        m_Passing = false;
        SaveSettings(Settings{}, *this);
        m_Passing = true;
        const std::string marker = m_Held.at(kDesignsSavedKey);
        // Asked before LoadSettings reads anything: Ashita's typed getters set a key it lacks to the default they
        // were given, and the store would then seem to hold what a new install's file, not yet written, does not.
        bool any = false;
        for (auto& [key, text] : m_Held)
        {
            text = m_Store.GetString(key.c_str(), kNoText);
            any  = any || text != kNoText;
        }
        m_Outdated = any && (m_Held.at(kDesignsSavedKey) != marker ||
                                std::strtof(m_Held.at(kVersionKey).c_str(), nullptr) < kSettingsVersion);
        return LoadSettings(m_Store);
    }

    const std::vector<KeyText>& ChangedKeys::Save(const Settings& s)
    {
        SaveSettings(s, *this);
        return m_Unwritten;
    }

    SaveNotice ChangedKeys::ReadBack(bool fileHoldsThem)
    {
        if (fileHoldsThem) m_Unwritten.clear();
        const bool wasFailing = std::exchange(m_Failing, !fileHoldsThem);
        if (wasFailing == m_Failing) return SaveNotice::None;
        return m_Failing ? SaveNotice::Failed : SaveNotice::Saved;
    }

    uint32_t ToArgb(const Color& c)
    {
        auto byte = [](float f) { return static_cast<uint8_t>(std::lround(f * 255.0f)); };
        return Opaque(byte(c.v[0]), byte(c.v[1]), byte(c.v[2]));
    }
}
