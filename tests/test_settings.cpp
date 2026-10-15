#include "pointer_keys.h"
#include "settings.h"
#include "release_file.h"
#include "test.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace headsup;

namespace
{
    class MapStore final : public SettingsStore
    {
    public:
        std::map<std::string, std::string> values;
        std::vector<std::string> sets; // the key of every Set, in order

        bool addsDefaults = false; // as Ashita's store: a typed read of a key it lacks adds the key with the default

        bool GetBool(const char* key, bool fallback) override
        {
            const auto it = values.find(key);
            if (it != values.end()) return it->second == "true" || it->second == "1";
            if (addsDefaults) values[key] = fallback ? "1" : "0"; // as Ashita writes a switch it adds
            return fallback;
        }
        float GetFloat(const char* key, float fallback) override
        {
            const auto it = values.find(key);
            if (it != values.end()) return std::strtof(it->second.c_str(), nullptr);
            if (addsDefaults) values[key] = std::to_string(fallback);
            return fallback;
        }
        std::string GetString(const char* key, const char* fallback) override
        {
            const auto it = values.find(key);
            return it == values.end() ? fallback : it->second;
        }
        void Set(const char* key, const char* value) override
        {
            values[key] = value;
            sets.push_back(key);
        }
    };

    // A settings.ini as HeadsUp writes it, every value off its default so a renamed or dropped key shows. Saved files
    // carry these keys: a field may be renamed, its key never.
    constexpr const char* kSavedFile = R"(settingsVersion=2.0000
enabled=false
showLabels=false
replaceMobNames=false
showIcons=false
hideInCombat=true
hideClaimed=true
hideTooWeak=true
hideWhileEngaged=true
markPlaceholders=false
phTimers=true
scaleWithDistance=false
scaleCursor=true
fontBold=true
nameGlow=false
glowOffWhenFighting=false
scaleOwnName=false
scalePlayerNames=false
labelFontBold=false
showOrnament=false
ownGlowColor=true
ownOrnamentColor=true
labelFontName=Cinzel
glowSize=150.0000
ornamentWidth=120.0000
ornamentThickness=9.0000
nameShadow=50.0000
labelShadow=150.0000
glowColorR=0.1000
glowColorG=0.2000
glowColorB=0.3000
ornamentColorR=0.4000
ornamentColorG=0.5000
ornamentColorB=0.6000
labelShadowColorR=0.7000
labelShadowColorG=0.8000
labelShadowColorB=0.9000
replacePlayerNames=false
replaceNpcNames=false
replaceCursor=false
cursorFeather=false
chocoboPointer=true
fixPointer=false
showPlayerIcons=false
centerNameAndIcons=false
ownNameColor=true
smoothness=12.0000
mobNameSize=12.0000
playerNameSize=13.0000
selfNameSize=17.0000
npcNameSize=16.0000
labelSize=14.0000
timerSize=17.0000
iconSize=24.0000
cursorSize=31.0000
nameRaise=12.0000
playerIconSize=120.0000
glowStrength=150.0000
thickness=6.5000
maxDistance=25.0000
cursorColorR=0.7700
cursorColorG=0.3200
cursorColorB=0.3100
subCursorColorR=0.5000
subCursorColorG=0.2500
subCursorColorB=0.7500
outOfRangeCursorColorR=0.7500
outOfRangeCursorColorG=0.1200
outOfRangeCursorColorB=0.2500
lockedCursorColorR=0.1200
lockedCursorColorG=0.2500
lockedCursorColorB=0.5000
nameColorR=0.5000
nameColorG=0.2500
nameColorB=0.1200
textOutlineR=0.2500
textOutlineG=0.2400
textOutlineB=0.5000
iconTintR=0.7500
iconTintG=0.5000
iconTintB=0.9000
showDamageIcons=true
showStatusRankIcons=true
showImmunityIcons=true
detectionIconSide=right
badgeSide=left
elementWeakRank=-3.0000
elementResistRank=6.0000
maxElements=3.0000
showRanks=true
badgeFontName=Tahoma
paralyzeBadge=false
paralyzeWeakRank=-1.0000
paralyzeResistRank=1.0000
bindBadge=false
bindWeakRank=-2.0000
bindResistRank=2.0000
silenceBadge=false
silenceWeakRank=-3.0000
silenceResistRank=3.0000
slowBadge=false
slowWeakRank=-1.0000
slowResistRank=5.0000
poisonBadge=false
poisonWeakRank=-2.0000
poisonResistRank=6.0000
lightSleepBadge=false
lightSleepWeakRank=-3.0000
lightSleepResistRank=7.0000
darkSleepBadge=false
darkSleepWeakRank=-1.0000
darkSleepResistRank=8.0000
blindBadge=false
blindWeakRank=-2.0000
blindResistRank=9.0000
stunBadge=false
stunWeakRank=-3.0000
stunResistRank=10.0000
gravityBadge=false
gravityWeakRank=-1.0000
gravityResistRank=12.0000
weakBadgeColorR=0.1000
weakBadgeColorG=0.2000
weakBadgeColorB=0.3000
resistBadgeColorR=0.4000
resistBadgeColorG=0.5000
resistBadgeColorB=0.6000
rankBadgeColorR=0.7000
rankBadgeColorG=0.8000
rankBadgeColorB=0.9000
immuneBadgeColorR=0.9000
immuneBadgeColorG=0.1000
immuneBadgeColorB=0.2000
fontName=Georgia
labelUnknownR=0.1000
labelUnknownG=0.1100
labelUnknownB=0.1200
labelTooWeakR=0.2000
labelTooWeakG=0.2100
labelTooWeakB=0.2200
labelEasyPreyR=0.3000
labelEasyPreyG=0.3100
labelEasyPreyB=0.3200
labelDecentChallengeR=0.4000
labelDecentChallengeG=0.4100
labelDecentChallengeB=0.4200
labelEvenMatchR=0.5000
labelEvenMatchG=0.5100
labelEvenMatchB=0.5200
labelToughR=0.6000
labelToughG=0.6100
labelToughB=0.6200
labelVeryToughR=0.7000
labelVeryToughG=0.7100
labelVeryToughB=0.7200
willAttackShow=false
willAttackR=0.1300
willAttackG=0.1400
willAttackB=0.1500
wontAttackShow=true
wontAttackR=0.2300
wontAttackG=0.2400
wontAttackB=0.2500
unknownShow=true
unknownR=0.3300
unknownG=0.3400
unknownB=0.3500
nmWillAttackShow=false
nmWillAttackR=0.4300
nmWillAttackG=0.4400
nmWillAttackB=0.4500
nmWontAttackShow=false
nmWontAttackR=0.5300
nmWontAttackG=0.5400
nmWontAttackB=0.5500
placeholderShow=false
placeholderR=0.6300
placeholderG=0.6400
placeholderB=0.6500
partyClaimShow=true
partyClaimR=0.7300
partyClaimG=0.7400
partyClaimB=0.7500
otherClaimShow=true
otherClaimR=0.8300
otherClaimG=0.8400
otherClaimB=0.8500
iconGm=right
iconMentor=hide
iconNewAdventurer=right
iconLevelSync=hide
iconAway=right
iconLinkshell=hide
iconBazaar=right
iconSeekingParty=hide
iconGmShare=false
iconMentorShare=false
iconNewAdventurerShare=false
iconLevelSyncShare=false
iconAwayShare=false
iconLinkshellShare=false
iconBazaarShare=true
iconSeekingPartyShare=false
playerIconOrder=iconSeekingParty iconGm iconMentor iconNewAdventurer iconLevelSync iconAway iconLinkshell iconBazaar
playerIconMax=4.0000
mobId=lastThree
look=fantasy
keepPointerKeys=11 1B 51
showBar=false
barTheme=umbraDark
barSeparators=none
barCurrency=allied_notes
serverClockZone=usPacific
barTargetDetails=true
menuFont=Tahoma
menuScale=125.0000
barHeight=48.0000
barTextSize=16.0000
popupTextSize=20.0000
barZone=false
barWeather=false
barVanaTime=false
barTarget=false
barGil=false
barBags=false
barEarthTime=false
barGameHelp=false
barServerTime=true
barFps=false
barJob=false
barConnection=false
barParty=true
barSignet=true
barDistance=false
barZoneLine2=true
barZoneIcons=true
barWeatherLine2=true
barWeatherIcons=true
barVanaTimeLine2=true
barVanaTimeIcons=true
barTargetLine2=false
barTargetIcons=true
barGilLine2=true
barGilIcons=false
barBagsLine2=true
barBagsIcons=true
barEarthTimeLine2=true
barEarthTimeIcons=true
barGameHelpLine2=true
barGameHelpIcons=true
barServerTimeLine2=true
barServerTimeIcons=true
barFpsLine2=true
barFpsIcons=true
barJobLine2=true
barJobIcons=true
barConnectionLine2=true
barConnectionIcons=true
barPartyLine2=true
barPartyIcons=true
barSignetLine2=true
barSignetIcons=true
barDistanceLine2=true
barDistanceIcons=true
expBar=beside
jobNames=false
clock24=false
stackNames=true
hideInEvent=true
debugMode=true
barTitleFont=Cinzel
barTextFont=
barSubtitle=before
playerFontName=Verdana
playerFontBold=false
npcFontName=Cinzel
npcFontBold=true
mobIconPlace=inline
cursorOutline=60.0000
untargetedOpacity=40.0000
clockSeconds=true
serverSeconds=true
barLatency=true
barLeft=zone weather vanaTime job party signet distance
barCenter=
barRight=target gil bags connection fps earthTime
hideHelpBar=true
hideNetstat=true
screenshotKey=121.0000
screenshotHidesGame=false
movedWindows=equip levmerit
windowOffsets=equip:10,-20
windowGroups=Bags:0,30,1:inventor+iteminfo
allUnderBar=true
moveGameHelp=true
helpBarBackground=false
helpBarX=12.0000
helpBarY=8.0000
)";

    Settings SavedSettings()
    {
        Settings s;
        s.enabled           = false;
        s.phTimers          = true;
        s.scaleWithDistance = false;
        s.scaleOwnName      = false;
        s.scaleCursor       = true; // apart from the names': a dropped key would take theirs
        s.cursorStyle = CursorStyle::Vanilla; // the cursor was switched off
        s.pointerIcon    = PointerIcon::Chocobo;
        s.fixPointer     = false;
        s.smoothness     = 12;
        s.cursorHeight   = 31;
        s.cursorWidth    = 39; // the arrow it had, 0.8 as tall as wide
        s.nameRaise      = 12;
        s.maxDistance    = 25.0f;
        s.keepPointerKeys = {0x11, 0x1B, 0x51};
        s.elementWeakRank     = -3;
        s.elementResistRank   = 6;
        s.maxElements         = 3;
        s.showRanks           = true;
        s.badgeFontName       = "Tahoma";
        s.showBar      = false;
        s.barTheme     = BarTheme::UmbraDark;
        s.barSeparators = BarSeparators::None;
        s.barCurrency   = *CurrencyOfKey("allied_notes");
        s.clockServerTime = true; // the file's Server time widget was on
        s.barTargetDetails = true;
        s.menuFont         = "Tahoma";
        s.menuFontSize     = 21; // 125 percent of Ashita's 18 pixels, in Tahoma, which was drawn at 0.95 of its size
        s.windowOffsets    = {WindowOffset{"equip", 10, -20}};
        s.moveGameHelp     = true;
        s.helpBarBackground = false;
        s.helpBarX         = 12;
        s.helpBarY         = 8;
        s.barSecondLine[static_cast<int>(BarWidget::Target)] = false;
        s.barIcons[static_cast<int>(BarWidget::Gil)]         = false;
        s.expBar     = ExpBarPlace::Beside;
        s.jobNames   = false;
        s.clock24    = false;
        s.stackNames = true;
        s.hideInEvent = true;
        s.debugMode = true;
        s.barTitleFont = "Cinzel";
        s.barSubtitle = SubtitlePlace::Before;
        s.cursorOutlineThickness = 120; // 60 of before
        s.clockSeconds  = true;
        s.barLatency = true;
        s.barHeight    = 48;
        s.barTextSize   = 16;
        s.popupTextSize = 20;
        s.hideHelpBar  = true;
        s.hideNetstat  = true;
        s.screenshotKey       = 0x79; // F10
        s.screenshotHidesGame = false;
        for (int w = 0; w < kBarWidgetCount; ++w)
            s.barWidgets[w] = !kBarWidgetDefaults[w];
        s.barSections = DefaultPlacement();
        MoveWidget(s.barSections, BarSection::Center, 0, BarSection::Right, 0); // the target first on the right
        const int weak[kStatusRankCount]   = {-1, -2, -3, -1, -2, -3, -1, -2, -3, -1};
        const int resist[kStatusRankCount] = {1, 2, 3, 5, 6, 7, 8, 9, 10, 12};
        for (int i = 0; i < kStatusRankCount; ++i)
            s.statusBadges[i] = StatusBadgeRule{false, weak[i], resist[i]};
        s.weakBadgeColor      = Color{{0.1f, 0.2f, 0.3f}};
        s.resistBadgeColor    = Color{{0.4f, 0.5f, 0.6f}};
        s.rankBadgeColor      = Color{{0.7f, 0.8f, 0.9f}};
        s.immuneBadgeColor    = Color{{0.9f, 0.1f, 0.2f}};
        return s;
    }

    // The keys of kSavedFile that only an older build's file carries: a migration reads them, or nothing does, and
    // nothing writes them.
    const char* const kOldKeys[] = {"movedWindows", "windowGroups", "allUnderBar", "menuScale", "chocoboPointer", "untargetedOpacity", "glowOffWhenFighting", "thickness", "cursorSize", "cursorOutline", "replaceCursor", "cursorFeather", "barServerTime", "barServerTimeLine2", "barServerTimeIcons", "serverSeconds",
        "serverClockZone", "showLabels", "replaceMobNames", "showIcons", "hideInCombat", "hideClaimed", "hideTooWeak",
        "hideWhileEngaged", "markPlaceholders", "fontBold", "nameGlow", "scalePlayerNames",
        "labelFontBold", "showOrnament", "ownGlowColor", "ownOrnamentColor", "labelFontName", "glowSize",
        "ornamentWidth", "ornamentThickness", "nameShadow", "labelShadow", "glowColorR", "glowColorG", "glowColorB",
        "ornamentColorR", "ornamentColorG", "ornamentColorB", "labelShadowColorR", "labelShadowColorG", "labelShadowColorB",
        "replacePlayerNames", "replaceNpcNames", "showPlayerIcons", "centerNameAndIcons", "ownNameColor", "mobNameSize",
        "playerNameSize", "selfNameSize", "npcNameSize", "labelSize", "timerSize", "iconSize", "playerIconSize",
        "glowStrength", "cursorColorR", "cursorColorG", "cursorColorB", "subCursorColorR", "subCursorColorG",
        "subCursorColorB", "outOfRangeCursorColorR", "outOfRangeCursorColorG", "outOfRangeCursorColorB",
        "lockedCursorColorR", "lockedCursorColorG", "lockedCursorColorB", "nameColorR", "nameColorG", "nameColorB",
        "textOutlineR", "textOutlineG", "textOutlineB", "iconTintR", "iconTintG", "iconTintB", "showDamageIcons",
        "showStatusRankIcons", "showImmunityIcons", "detectionIconSide", "badgeSide", "fontName", "labelUnknownR",
        "labelUnknownG", "labelUnknownB", "labelTooWeakR", "labelTooWeakG", "labelTooWeakB", "labelEasyPreyR",
        "labelEasyPreyG", "labelEasyPreyB", "labelDecentChallengeR", "labelDecentChallengeG", "labelDecentChallengeB",
        "labelEvenMatchR", "labelEvenMatchG", "labelEvenMatchB", "labelToughR", "labelToughG", "labelToughB",
        "labelVeryToughR", "labelVeryToughG", "labelVeryToughB", "willAttackShow", "willAttackR", "willAttackG",
        "willAttackB", "wontAttackShow", "wontAttackR", "wontAttackG", "wontAttackB", "unknownShow", "unknownR",
        "unknownG", "unknownB", "nmWillAttackShow", "nmWillAttackR", "nmWillAttackG", "nmWillAttackB", "nmWontAttackShow",
        "nmWontAttackR", "nmWontAttackG", "nmWontAttackB", "placeholderShow", "placeholderR", "placeholderG",
        "placeholderB", "partyClaimShow", "partyClaimR", "partyClaimG", "partyClaimB", "otherClaimShow", "otherClaimR",
        "otherClaimG", "otherClaimB", "iconGm", "iconMentor", "iconNewAdventurer", "iconLevelSync", "iconAway",
        "iconLinkshell", "iconBazaar", "iconSeekingParty", "iconGmShare", "iconMentorShare", "iconNewAdventurerShare",
        "iconLevelSyncShare", "iconAwayShare", "iconLinkshellShare", "iconBazaarShare", "iconSeekingPartyShare",
        "playerIconOrder", "playerIconMax", "mobId", "look", "playerFontName", "playerFontBold", "npcFontName",
        "npcFontBold", "mobIconPlace"};

    MapStore Parse(const char* file)
    {
        MapStore store;
        std::istringstream lines(file);
        for (std::string line; std::getline(lines, line);)
            if (const size_t equals = line.find('='); equals != std::string::npos)
                store.values[line.substr(0, equals)] = line.substr(equals + 1);
        return store;
    }

    // The file release 0.65 wrote for a new install, with these of its keys changed: what a player's file holds
    // when this build first reads it. As Ashita's store, a typed read of a key it lacks adds the key.
    MapStore ReleaseFile(std::initializer_list<std::pair<const char*, const char*>> changed = {})
    {
        MapStore file = Parse(kReleaseFile);
        for (const auto& [key, value] : changed)
            file.values[key] = value;
        file.addsDefaults = true;
        return file;
    }
    // This build's own file of these settings, which still holds keys of before with these values.
    MapStore OwnFileWith(const Settings& s, std::initializer_list<std::pair<const char*, const char*>> old)
    {
        MapStore file;
        SaveSettings(s, file);
        for (const auto& [key, value] : old)
            file.values[key] = value;
        file.sets.clear();
        return file;
    }
}

TEST(players_and_npcs_take_the_mob_name_font_from_a_file_saved_before_they_had_their_own)
{
    MapStore old;
    old.values = {{"settingsVersion", "2.0000"}, {"fontName", "Georgia"}, {"fontBold", "true"}};
    const Settings s = LoadSettings(old);
    for (const PlateKind kind : {PlateKind::Players, PlateKind::Self, PlateKind::Npcs})
        CHECK(s.Kind(kind).lines[2].font == "Georgia" && s.Kind(kind).lines[2].style == StyleBit(FontStyle::Bold));
}

TEST(a_file_from_before_incredibly_tough_had_its_own_color_gains_no_key_for_it)
{
    // No build ever wrote labelIncrediblyTough: a typed read of it would add it to a version 2 file for nothing.
    MapStore file     = Parse(kSavedFile);
    file.addsDefaults = true;
    const Settings s  = LoadSettings(file);
    for (const char* channel : {"R", "G", "B"})
        CHECK(!file.values.contains(std::string("labelIncrediblyTough") + channel));
    const int deep = static_cast<int>(LabelShade::IncrediblyTough);
    CHECK(s.palette.toughness[deep] == Settings{}.palette.toughness[deep]);
}

TEST(a_saved_file_loads_every_setting_and_saves_back_the_same)
{
    MapStore file         = Parse(kSavedFile);
    const Settings loaded = LoadSettings(file);
    Settings expected     = SavedSettings(); // the migration test owns the designer fields
    expected.kinds            = loaded.kinds;
    expected.palette          = loaded.palette;
    expected.nameplates       = loaded.nameplates;
    expected.outlines         = loaded.outlines;
    expected.baseSize         = loaded.baseSize;
    expected.defaultFont      = loaded.defaultFont;
    expected.scaleMin         = loaded.scaleMin;
    expected.scaleMax         = loaded.scaleMax;
    expected.timerPercent     = loaded.timerPercent;
    expected.detectionShown   = loaded.detectionShown;
    expected.cursorEdgeColor  = loaded.cursorEdgeColor;
    CHECK(loaded == expected);
    MapStore saved;
    SaveSettings(loaded, saved);
    for (const auto& [key, value] : file.values)
    {
        if (std::ranges::find(kOldKeys, key) != std::end(kOldKeys))
            CHECK(!saved.values.contains(key));
        else if (key != "settingsVersion")
            CHECK_EQ(saved.values[key], value);
    }
    CHECK(LoadSettings(saved) == loaded);
}

TEST(a_player_icon_side_that_is_not_a_side_keeps_its_default)
{
    MapStore store;
    store.values["enabled"]       = "true"; // a file: with neither this nor a version, a store is a new install
    store.values["iconLinkshell"] = "middle";
    CHECK_EQ(LoadSettings(store).Kind(PlateKind::Players).lines[2].text, "$mentor$new$sync$away$linkshell$bazaar$seeking$gm$name");
}

TEST(a_group_side_that_is_not_left_or_right_keeps_its_default)
{
    // On the level line the detection icons lead and the badges trail, each at its default side.
    MapStore store;
    store.values = {{"enabled", "true"}, {"showDamageIcons", "true"}, {"mobIconPlace", "inline"}, {"detectionIconSide", "hide"},
        {"badgeSide", "middle"}};
    CHECK_EQ(LoadSettings(store).Kind(PlateKind::Mobs).lines[1].text, "$detect Lv $level $con $weak$resist");
}

TEST(empty_store_gives_defaults)
{
    MapStore store;
    CHECK(LoadSettings(store) == Settings{});
}

TEST(a_whole_number_too_large_for_an_int_is_clamped_like_any_other)
{
    // A hand-edited file: the value saturates and Clamp takes it to the nearest limit, not past the other end.
    MapStore store;
    store.values["enabled"]    = "true"; // which every build's file holds: without it there is no file to read
    store.values["nameSize"]   = "1e10";
    store.values["labelSize"]  = "-1e10";
    store.values["cursorHeight"] = "3e9";
    const Settings s = LoadSettings(store);
    CHECK_EQ(s.baseSize, kMaxTextSize);
    CHECK_EQ(s.Kind(PlateKind::Mobs).lines[1].textSize, kMinSizePercent); // the smallest size of the largest base
    CHECK_EQ(s.cursorHeight, kMaxCursorSide);
}

TEST(the_font_list_puts_the_bundled_fonts_first_then_the_installed_ones_sorted_once_each)
{
    // GDI lists a family once per character set, vertical variants with an @, and a bundled font may be installed too.
    const std::vector<std::string> choices = FontChoices({"Verdana", "arial", "Arial", "@Meiryo", "", "Cinzel"}, "Verdana");
    const std::vector<std::string> expected{"Marcellus SC", "Cinzel", "Cormorant SC", "Barlow Condensed", "arial", "Verdana"};
    CHECK(choices == expected);
}

TEST(a_font_that_is_not_installed_is_still_offered_so_it_can_be_kept)
{
    const std::vector<std::string> choices = FontChoices({"Verdana"}, "Papyrus");
    CHECK(std::ranges::find(choices, "Papyrus") != choices.end());
    CHECK(choices[0] == "Marcellus SC"); // the bundled ones still lead
}

TEST(clamp_bounds_every_field)
{
    // The menu relies on Clamp, not on the loader.
    Settings s;
    s.outlineThickness = 0;
    s.smoothness  = 99;
    s.maxDistance = 1000.0f;
    s.cursorHeight = 2;
    s.cursorWidth  = 900;
    s.cursorRaise  = -50;
    s.cursorOutlineThickness = 1;
    s.nameRaise   = 99;
    s.elementWeakRank   = 5;
    s.elementResistRank = -9;
    s.maxElements       = 99;
    s.statusBadges[2]   = StatusBadgeRule{true, 3, 40};
    s.statusBadges[7]   = StatusBadgeRule{true, -40, -3};
    s.badgeFontName     = "";
    s.barHeight         = 200;
    s.barTheme          = static_cast<BarTheme>(9);
    s.barCurrency       = kCurrencyCount;
    const Color wild{{2.0f, -1.0f, 0.5f}};
    s.weakBadgeColor = s.resistBadgeColor = s.rankBadgeColor = s.immuneBadgeColor = s.cursorEdgeColor.custom = wild;
    s.cursorColor     = ColorPick{ColorSource::Game, wild}; // the game's name color is not the cursor's to take
    s.cursorEdgeColor.source = ColorSource::Game;
    const Settings c = Clamp(s);
    const Color clamped{{1.0f, 0.0f, 0.5f}};
    for (const Color& each : {c.weakBadgeColor, c.resistBadgeColor, c.rankBadgeColor, c.immuneBadgeColor, c.cursorEdgeColor.custom,
             c.cursorColor.custom})
        CHECK(each == clamped);
    CHECK(c.cursorColor.source == ColorSource::Target && c.cursorEdgeColor.source == ColorSource::Custom);
    CHECK_EQ(c.outlineThickness, kMinOutlineThickness);
    CHECK_EQ(c.smoothness, kMaxSmoothness);
    CHECK_EQ(c.maxDistance, kMaxOutlineDistance);
    // The smoothness is one of the four levels, and an outline's range no nearer than 15 yalms.
    Settings between;
    between.smoothness     = 9;
    between.maxDistance    = 5.0f;
    between.outlineOpacity = 0;
    between.menuOpacity    = 5; // a window too faint to find again could not be set back
    CHECK_EQ(Clamp(between).menuOpacity, kMinMenuOpacity);
    CHECK_EQ(Clamp(between).smoothness, 8);
    CHECK_EQ(Clamp(between).maxDistance, 15.0f);
    CHECK_EQ(Clamp(between).outlineOpacity, kMinOpacity);
    CHECK_EQ(c.cursorHeight, kMinCursorSide);
    CHECK_EQ(c.cursorWidth, kMaxCursorSide);
    CHECK_EQ(c.cursorRaise, -kMaxCursorRaise);
    CHECK_EQ(c.cursorOutlineThickness, kMinCursorEdge);
    CHECK_EQ(c.nameRaise, kMaxNameRaise);
    CHECK(c.elementWeakRank == -1 && c.elementResistRank == 1 && c.maxElements == kElementCount);
    CHECK(c.statusBadges[2] == (StatusBadgeRule{true, -1, kNoResistRank}));
    CHECK(c.statusBadges[7] == (StatusBadgeRule{true, kNoWeakRank, 1}));
    CHECK(c.badgeFontName.empty()); // no font of its own: the default font
    CHECK_EQ(c.barHeight, kMaxBarHeight);
    CHECK(c.barTheme == BarTheme::PhoenixGold && c.barCurrency == kGilCurrency);
}

TEST(the_target_cursor_rises_or_sinks_up_to_twenty_pixels)
{
    MapStore up = ReleaseFile({{"cursorRaise", "15.0000"}}), down = ReleaseFile({{"cursorRaise", "-18.0000"}});
    CHECK_EQ(LoadSettings(up).cursorRaise, 15);
    CHECK_EQ(LoadSettings(down).cursorRaise, -18);
    Settings s;
    s.cursorRaise = 25;
    CHECK_EQ(Clamp(s).cursorRaise, 20);
}

TEST(a_file_from_before_hide_during_conversation_keeps_plates_in_conversations)
{
    MapStore file = ReleaseFile();
    CHECK(!LoadSettings(file).hideInEvent);
    CHECK(!LoadSettings(file).debugMode); // nor had it a debug mode
}

TEST(called_for_help_has_its_own_color_orange_until_one_is_saved)
{
    const int help = static_cast<int>(ClaimState::CalledForHelp);
    MapStore release = ReleaseFile();
    CHECK_EQ(ToArgb(LoadSettings(release).palette.claim[help]), 0xFFFF9900u);
    Settings s;
    s.palette.claim[help] = {{0.25f, 0.5f, 0.75f}};
    MapStore file;
    SaveSettings(s, file);
    CHECK(LoadSettings(file).palette.claim[help] == (Color{{0.25f, 0.5f, 0.75f}}));
    CHECK(LoadSettings(file).palette.claim[static_cast<int>(ClaimState::Yours)] == Settings{}.palette.claim[static_cast<int>(ClaimState::Yours)]);
}

TEST(percents_load_and_clamp_onto_steps_of_five)
{
    Settings s;
    s.timerPercent           = 87;
    s.scaleMin               = 88;
    s.outlineOpacity         = 12;
    s.menuOpacity            = 33;
    s.cursorOutlineThickness = 52;
    LineDesign& line = s.Kind(PlateKind::Players).lines[0];
    line.textSize    = 87;
    line.iconSize    = 112;
    line.shadow      = 3;
    line.glow        = 298;
    line.glowSize    = 26;
    OpacitySettings& o = s.Kind(PlateKind::Mobs).opacity;
    o.noTarget         = 87;
    o.notTargeted      = 41;
    o.targeted         = 99;
    const Settings c = Clamp(s);
    CHECK(c.timerPercent == 85 && c.scaleMin == 90 && c.outlineOpacity == 10 && c.menuOpacity == 35);
    CHECK_EQ(c.cursorOutlineThickness, 50);
    const LineDesign& l = c.Kind(PlateKind::Players).lines[0];
    CHECK(l.textSize == 85 && l.iconSize == 110 && l.shadow == 5 && l.glow == 300 && l.glowSize == 25);
    const OpacitySettings& co = c.Kind(PlateKind::Mobs).opacity;
    CHECK(co.noTarget == 85 && co.notTargeted == 40 && co.targeted == 100);
    CHECK(Clamp(Settings{}) == Settings{}); // every default is on the steps already
    MapStore file = ReleaseFile({{"timerPercent", "87.0000"}, {"mobsLine2TextSize", "87.0000"}});
    const Settings loaded = LoadSettings(file);
    CHECK(loaded.timerPercent == 85 && loaded.Kind(PlateKind::Mobs).lines[1].textSize == 85);
}

TEST(non_finite_values_fall_back_to_defaults)
{
    // A number that is none reads as a key the file lacks: the file loads as it would without them.
    MapStore plain;
    plain.values["enabled"] = "true";
    MapStore store          = plain;
    store.values["thickness"]   = "nan";
    store.values["outlineThickness"] = "inf";
    store.values["smoothness"]  = "nan";
    store.values["maxDistance"] = "inf";
    store.values["iconSize"]    = "nan";
    store.values["unknownG"]    = "nan";
    store.values["nameSize"]    = "inf";
    CHECK(LoadSettings(store) == LoadSettings(plain));
    // The same of a number whose key the review gave it: the value under the key of before is taken.
    MapStore rebased = ReleaseFile({{"mobsLine3ShadowStrength", "nan"}, {"mobsLine3Shadow", "90.0000"}});
    CHECK_EQ(LoadSettings(rebased).Kind(PlateKind::Mobs).lines[2].shadow, 60);
}

TEST(argb_packing)
{
    CHECK_EQ(ToArgb(Color{{1.0f, 0.0f, 0.0f}}), 0xFFFF0000u);
    CHECK_EQ(ToArgb(Color{{0.2f, 1.0f, 0.3f}}), 0xFF33FF4Du);
}

TEST(nameplates_need_the_master_switch_and_a_part)
{
    CHECK(NameplatesOn(Settings{}));
    Settings none;
    none.nameplates = none.phTimers = false;
    none.cursorStyle = CursorStyle::Vanilla;
    CHECK(!NameplatesOn(none));
    for (bool Settings::*part : {&Settings::nameplates, &Settings::phTimers})
    {
        Settings s = none;
        s.*part    = true;
        CHECK(NameplatesOn(s));
        s.enabled = false;
        CHECK(!NameplatesOn(s));
    }
    // Any cursor of HeadsUp's own is drawn by the nameplates' pass.
    for (const CursorStyle style : {CursorStyle::Triangle, CursorStyle::Feather, CursorStyle::Chocobo, CursorStyle::File})
    {
        Settings cursorOnly = none;
        cursorOnly.cursorStyle = style;
        CHECK(NameplatesOn(cursorOnly));
    }
}

TEST(a_color_from_hex_draws_as_the_same_hex)
{
    // Phoenix's red and a color whose bytes all differ: 0.77 would have drawn C4, not C5.
    CHECK_EQ(ToArgb(ColorFromRgb(0xC55151)), 0xFFC55151u);
    CHECK_EQ(ToArgb(ColorFromRgb(0x01FE80)), 0xFF01FE80u);
}

namespace
{
    MapStore TargetCursor(const char* r, const char* g, const char* b)
    {
        MapStore store;
        store.values = {{"enabled", "true"}, {"cursorColorR", r}, {"cursorColorG", g}, {"cursorColorB", b}};
        return store;
    }

    const Color& TargetColor(const Settings& s) { return s.palette.target[static_cast<int>(TargetState::Targeted)]; }
}

TEST(a_white_target_cursor_saved_before_it_was_red_turns_red)
{
    // Every setting is saved whenever one changes, so a file from before 0.53 holds the old default, white, as if chosen.
    MapStore old = TargetCursor("1.0000", "1.0000", "1.0000");
    CHECK(TargetColor(LoadSettings(old)) == ColorFromRgb(kPhoenixRed));
}

TEST(a_white_target_cursor_chosen_since_stays_white)
{
    Settings white;
    white.palette.target[static_cast<int>(TargetState::Targeted)] = Color{{1.0f, 1.0f, 1.0f}};
    MapStore saved;
    SaveSettings(white, saved);
    CHECK(TargetColor(LoadSettings(saved)) == (Color{{1.0f, 1.0f, 1.0f}}));
}

TEST(an_old_target_cursor_of_any_other_color_is_kept)
{
    MapStore old = TargetCursor("0.7700", "0.3200", "0.3200");
    CHECK(TargetColor(LoadSettings(old)) == (Color{{0.77f, 0.32f, 0.32f}}));
}

TEST(a_file_from_before_the_level_line_had_its_own_font_and_shadow_keeps_the_names)
{
    MapStore old;
    old.values = {{"enabled", "true"}, {"fontName", "Georgia"}, {"fontBold", "true"}, {"textOutlineR", "0.2500"},
        {"textOutlineG", "0.2400"}, {"textOutlineB", "0.5000"}};
    const Settings s        = LoadSettings(old);
    const LineDesign& level = s.Kind(PlateKind::Mobs).lines[1];
    CHECK(level.font == "Georgia" && level.style == StyleBit(FontStyle::Bold));
    CHECK(level.shadowColor.custom == (Color{{0.25f, 0.24f, 0.5f}}));
}

TEST(a_file_from_before_each_kind_had_a_name_size_gives_all_of_them_its_one)
{
    MapStore old;
    old.values = {{"enabled", "true"}, {"nameSize", "20.0000"}};
    const Settings s = LoadSettings(old);
    CHECK_EQ(s.baseSize, 20);
    for (const PlateKind kind : {PlateKind::Players, PlateKind::Self, PlateKind::Npcs})
        CHECK_EQ(s.Kind(kind).lines[2].textSize, 100);
}

TEST(a_file_from_before_the_cursor_scaled_on_its_own_scales_it_like_the_names)
{
    MapStore old;
    old.values = {{"enabled", "true"}, {"scaleWithDistance", "false"}};
    CHECK(!LoadSettings(old).scaleCursor);
    old.values = {{"enabled", "true"}, {"scaleWithDistance", "true"}};
    CHECK(LoadSettings(old).scaleCursor);
    // A new install's cursor stays one size, though its names scale.
    MapStore none;
    CHECK(LoadSettings(none).scaleWithDistance && !LoadSettings(none).scaleCursor);
}

TEST(an_old_install_on_the_old_default_font_takes_the_bundled_one_once)
{
    // Every setting is saved whenever one changes, so a file from before 0.6 holds Trebuchet MS as if chosen.
    MapStore old;
    old.values = {{"settingsVersion", "1.0000"}, {"fontName", "Trebuchet MS"}, {"labelFontName", "Trebuchet MS"}};
    // A line with no font of its own takes the default font, the bundled one.
    const Settings s = LoadSettings(old);
    CHECK(s.defaultFont == kDefaultFont);
    CHECK(s.Kind(PlateKind::Mobs).lines[2].font.empty() && s.Kind(PlateKind::Mobs).lines[1].font.empty());
    MapStore older; // from before settings had a version, or the level line its own font
    older.values = {{"enabled", "true"}, {"fontName", "Trebuchet MS"}};
    CHECK(LoadSettings(older).Kind(PlateKind::Mobs).lines[1].font.empty());
}

TEST(a_font_picked_before_or_since_is_kept)
{
    MapStore picked;
    picked.values = {{"settingsVersion", "1.0000"}, {"fontName", "Georgia"}, {"labelFontName", "Cinzel"}};
    const KindSettings mobs = LoadSettings(picked).Kind(PlateKind::Mobs);
    CHECK(mobs.lines[2].font == "Georgia" && mobs.lines[1].font == "Cinzel");
    MapStore since;
    since.values = {{"settingsVersion", "2.0000"}, {"fontName", "Trebuchet MS"}};
    CHECK(LoadSettings(since).Kind(PlateKind::Mobs).lines[2].font == "Trebuchet MS");
}

TEST(a_white_target_cursor_saved_after_it_turned_red_stays_white)
{
    MapStore chosen = TargetCursor("1.0000", "1.0000", "1.0000");
    chosen.values["settingsVersion"] = "1.0000";
    CHECK(TargetColor(LoadSettings(chosen)) == (Color{{1.0f, 1.0f, 1.0f}}));
}

TEST(the_keys_that_keep_the_pointer_are_real_keys_once_each_and_not_too_many)
{
    Settings s;
    s.keepPointerKeys = {0xA2, 0x11, 0x00, 0x01, 0x51}; // left Ctrl is Ctrl; nothing and the left button are not keys
    CHECK(Clamp(s).keepPointerKeys == (std::vector<uint8_t>{0x11, 0x51}));
    s.keepPointerKeys.clear();
    for (int vk = 0x30; vk < 0x30 + 40; ++vk)
        s.keepPointerKeys.push_back(static_cast<uint8_t>(vk));
    CHECK_EQ(Clamp(s).keepPointerKeys.size(), kMaxKeptKeys);
    MapStore odd;
    odd.values = {{"keepPointerKeys", "11 zz 1B 999"}};
    CHECK(LoadSettings(odd).keepPointerKeys == (std::vector<uint8_t>{0x11, 0x1B}));
}

TEST(the_screenshot_key_is_one_real_key_or_none)
{
    Settings s;
    CHECK_EQ(s.screenshotKey, 0);
    CHECK(s.screenshotHidesGame);
    s.screenshotKey = 0xA3; // right Ctrl
    CHECK_EQ(Clamp(s).screenshotKey, 0x11);
    s.screenshotKey = 0x02; // the right mouse button
    CHECK_EQ(Clamp(s).screenshotKey, 0);
    s.screenshotKey = 999;
    CHECK_EQ(Clamp(s).screenshotKey, 0);
    MapStore odd;
    odd.values = {{"screenshotKey", "zz"}};
    CHECK_EQ(LoadSettings(odd).screenshotKey, 0);
}

TEST(a_file_from_before_the_badge_group_takes_its_defaults)
{
    MapStore old;
    old.values = {{"enabled", "true"}, {"showIcons", "true"}};
    const Settings s = LoadSettings(old);
    CHECK_EQ(s.Kind(PlateKind::Mobs).lines[0].text, "$detect"); // no badges: the top bar's popup shows them
    CHECK(s.elementWeakRank == -1 && s.elementResistRank == 4 && s.maxElements == kElementCount && !s.showRanks);
    CHECK_EQ(s.badgeFontName, "Barlow Condensed"); // the font badges had, where a new install's are in the default font
    CHECK_EQ(s.detectionShown, kAllDetection); // and every icon, the aggression one too, which a new install hides
    for (const StatusBadgeRule& status : s.statusBadges)
        CHECK(status == (StatusBadgeRule{true, kNoWeakRank, 4}));
    CHECK(s.weakBadgeColor == Settings{}.weakBadgeColor && s.immuneBadgeColor == Settings{}.immuneBadgeColor);
}

TEST(the_window_placement_saves_and_loads_back_its_switches_places_and_each_modes_shifts)
{
    const std::string_view first = WindowViews()[0].id, second = WindowViews()[1].id;
    Settings s;
    s.moveWindows = false;
    s.keepClear   = false;
    s.placedUnder = {30, 70};
    s.viewShifts[static_cast<int>(HelpMode::On)]  = {ViewShift{std::string(first), 20}};
    s.viewShifts[static_cast<int>(HelpMode::Off)] = {
        ViewShift{std::string(second), -6}, ViewShift{std::string(first), 3}};
    MapStore file;
    SaveSettings(s, file);
    CHECK(LoadSettings(file) == s);
    file.values["viewShiftsHelpOn"]   = "nowhere:4|" + std::string(first) + ":5000";
    file.values["placedUnderHelpOff"] = "99999.0000";
    const Settings read = LoadSettings(file);
    const std::vector<ViewShift> clamped = {{std::string(first), kMaxWindowOffset}};
    CHECK(read.viewShifts[static_cast<int>(HelpMode::On)] == clamped);
    CHECK_EQ(read.placedUnder[static_cast<int>(HelpMode::Off)], kMaxWindowOffset);
}

TEST(the_windows_help_mode_follows_the_panels_help_line_and_the_games_own_strip)
{
    Settings s;
    s.barWidgets[static_cast<int>(BarWidget::GameHelp)] = true;
    s.hideHelpBar = true;
    CHECK(WindowHelpMode(s) == HelpMode::On);
    s.showBar = false; // no panel, no help line on it
    CHECK(WindowHelpMode(s) == HelpMode::Off);
    s.hideHelpBar = false;
    CHECK(WindowHelpMode(s) == HelpMode::On);
}

TEST(auto_adjust_places_the_views_under_the_band_of_now_and_drops_the_players_shifts_for_it)
{
    Settings s;
    s.barWidgets[static_cast<int>(BarWidget::GameHelp)] = true;
    s.hideHelpBar = true;
    const std::string id(WindowViews()[0].id);
    s.viewShifts = {std::vector<ViewShift>{{id, 4}}, std::vector<ViewShift>{{id, 9}}};
    AutoAdjustWindows(s, 80, 50); // a taller panel: its help line ends at 80, the panel itself at 50
    CHECK(s.placedUnder[static_cast<int>(HelpMode::On)] == 80 && s.placedUnder[static_cast<int>(HelpMode::Off)] == 50);
    CHECK(s.viewShifts[0].empty() && s.viewShifts[1].empty());
    s.hideHelpBar = false;
    s.barWidgets[static_cast<int>(BarWidget::GameHelp)] = false;
    s.viewShifts[static_cast<int>(HelpMode::Off)] = {{id, 2}};
    AutoAdjustWindows(s, 71, 38); // the game's own strip moved under the panel, under which the windows go
    CHECK(s.placedUnder[static_cast<int>(HelpMode::On)] == 71 && s.placedUnder[static_cast<int>(HelpMode::Off)] == 38);
    CHECK(s.viewShifts[static_cast<int>(HelpMode::Off)].empty());
    const Settings before = s;
    AutoAdjustWindows(s, 0, 0); // the panel off: nothing to place under
    CHECK(s == before);
}

TEST(a_new_install_places_the_windows_under_the_panel_at_its_reference_band_and_keeps_them_clear)
{
    MapStore fresh;
    const Settings s = LoadSettings(fresh);
    CHECK(s.moveWindows && s.keepClear);
    CHECK_EQ(s.placedUnder[static_cast<int>(HelpMode::On)], ReferenceBand(HelpMode::On));
    CHECK_EQ(s.placedUnder[static_cast<int>(HelpMode::Off)], ReferenceBand(HelpMode::Off));
    CHECK(s.viewShifts[0].empty() && s.viewShifts[1].empty());
}

TEST(a_theme_that_is_not_one_keeps_the_default_and_old_files_get_the_bar)
{
    MapStore store;
    store.values["barTheme"]    = "purple";
    store.values["barCurrency"] = "dollars";
    const Settings s         = LoadSettings(store);
    CHECK(s.barTheme == BarTheme::PhoenixGold && s.barCurrency == kGilCurrency);
    CHECK(s.showBar);
    for (int w = 0; w < kBarWidgetCount; ++w) // a widget added to BarWidget takes its default from kBarWidgetDefaults
        CHECK(s.barWidgets[w] == kBarWidgetDefaults[w]);
}

TEST(each_theme_and_bar_widget_has_its_own_key)
{
    struct Named
    {
        const char* name;
        BarTheme theme;
    };
    for (const Named& t : {Named{"phoenixGold", BarTheme::PhoenixGold}, Named{"umbraDark", BarTheme::UmbraDark},
             Named{"phoenixRedOrnate", BarTheme::PhoenixRedOrnate},
             Named{"phoenixRedPills", BarTheme::PhoenixRedPills}, Named{"aurora", BarTheme::Aurora}})
    {
        MapStore store;
        store.values["barTheme"] = t.name;
        CHECK(LoadSettings(store).barTheme == t.theme);
        MapStore saved;
        SaveSettings(LoadSettings(store), saved);
        CHECK(saved.values["barTheme"] == t.name);
    }
    const char* const widgetKeys[] = {"barZone", "barWeather", "barVanaTime", "barTarget", "barGil", "barBags",
        "barEarthTime", "barGameHelp", "barFps", "barJob", "barConnection", "barParty", "barSignet", "barDistance"}; // in BarWidget's order
    static_assert(std::size(widgetKeys) == kBarWidgetCount);
    for (int w = 0; w < kBarWidgetCount; ++w)
    {
        MapStore store;
        store.values[widgetKeys[w]] = kBarWidgetDefaults[w] ? "false" : "true";
        const Settings s = LoadSettings(store);
        for (int other = 0; other < kBarWidgetCount; ++other)
            CHECK(s.barWidgets[other] == (kBarWidgetDefaults[other] != (other == w)));
    }
}

TEST(the_bar_height_is_clamped_at_both_ends)
{
    Settings s;
    s.barHeight = 0;
    CHECK_EQ(Clamp(s).barHeight, kMinBarHeight);
    s.barHeight = kMaxBarHeight + 1;
    CHECK_EQ(Clamp(s).barHeight, kMaxBarHeight);
}

TEST(the_bar_needs_the_master_switch_and_its_own)
{
    Settings s;
    CHECK(BarOn(s));
    s.showBar = false;
    CHECK(!BarOn(s));
    s.showBar = true;
    s.enabled = false;
    CHECK(!BarOn(s));
}

TEST(the_bar_runs_only_with_a_character_in_game)
{
    Settings s;
    CHECK(BarRuns(s, kLoginInGame));
    for (const uint8_t status : {uint8_t{0}, uint8_t{1}, uint8_t{3}, uint8_t{255}})
        CHECK(!BarRuns(s, status));
    s.showBar = false;
    CHECK(!BarRuns(s, kLoginInGame));
    s.showBar = true;
    s.enabled = false;
    CHECK(!BarRuns(s, kLoginInGame));
}

TEST(the_bar_sections_load_by_name_and_keep_every_widget)
{
    MapStore store;
    store.values["barLeft"]   = "target zone";
    store.values["barCenter"] = "";
    const Settings s          = LoadSettings(store);
    const auto& left          = s.barSections[static_cast<int>(BarSection::Left)];
    CHECK(left.size() >= 2 && left[0] == BarWidget::Target && left[1] == BarWidget::Zone);
    CHECK(s.barSections[static_cast<int>(BarSection::Center)].empty()); // the target is placed already
    MapStore saved;
    SaveSettings(s, saved);
    CHECK(saved.values["barCenter"].empty() && saved.values["barLeft"].rfind("target zone", 0) == 0);
}

TEST(the_job_layout_and_the_clocks_zone_are_saved_by_name_and_an_unknown_name_keeps_the_default)
{
    MapStore store;
    store.values    = {{"jobLayout", "expOnly"}, {"clockZone", "usPacific"}};
    const Settings s = LoadSettings(store);
    CHECK(s.jobLayout == JobLayout::ExpOnly && s.clockZone == ClockZone::UsPacific);
    MapStore saved;
    SaveSettings(s, saved);
    CHECK(saved.values["jobLayout"] == "expOnly" && saved.values["clockZone"] == "usPacific");
    CHECK(LoadSettings(saved) == s);
    MapStore fresh;
    SaveSettings(Settings{}, fresh);
    CHECK(fresh.values["jobLayout"] == "jobsFirst" && fresh.values["clockZone"] == "local");
    store.values       = {{"jobLayout", "sideways"}, {"clockZone", "mars"}};
    const Settings odd = LoadSettings(store);
    CHECK(odd.jobLayout == JobLayout::JobsFirst && !odd.clockZone.has_value());
    store.values = {{"jobLayout", "expFirst"}, {"clockZone", "local"}};
    CHECK(LoadSettings(store).jobLayout == JobLayout::ExpFirst && !LoadSettings(store).clockZone.has_value());
}

TEST(a_file_that_showed_server_time_as_a_widget_of_its_own_has_the_clock_stack_it)
{
    MapStore fresh;
    CHECK(!LoadSettings(fresh).clockServerTime);
    MapStore shown;
    shown.values = {{"barServerTime", "true"}};
    CHECK(LoadSettings(shown).clockServerTime);
    MapStore hidden;
    hidden.values = {{"barServerTime", "false"}};
    CHECK(!LoadSettings(hidden).clockServerTime);
    MapStore decided; // the option's own key, once saved, is what counts
    decided.values = {{"barServerTime", "true"}, {"clockServerTime", "false"}};
    CHECK(!LoadSettings(decided).clockServerTime);
    MapStore saved;
    SaveSettings(LoadSettings(shown), saved);
    CHECK(saved.values["clockServerTime"] == "true" && !saved.values.contains("barServerTime"));
    CHECK(!saved.values.contains("serverClockZone") && !saved.values.contains("serverSeconds"));
}

TEST(separators_that_are_not_a_choice_keep_the_themes)
{
    MapStore store;
    store.values["barSeparators"] = "stars";
    CHECK(LoadSettings(store).barSeparators == BarSeparators::Theme);
}

TEST(text_sizes_start_at_auto_and_clamp_to_their_range)
{
    CHECK(Settings{}.barTextSize == kAutoBarTextSize && Settings{}.popupTextSize == kAutoPopupTextSize);
    MapStore store;
    store.values["barTextSize"]   = "3";
    store.values["popupTextSize"] = "90";
    const Settings s = LoadSettings(store);
    CHECK(s.barTextSize == kAutoBarTextSize && s.popupTextSize == kMaxBarTextSize);
    // A 9 px popup puts its icons 2 px from their numbers, which reads as touching: set popup sizes start at 10.
    store.values["popupTextSize"] = "3";
    CHECK_EQ(LoadSettings(store).popupTextSize, kAutoPopupTextSize);
    store.values["popupTextSize"] = "10";
    CHECK_EQ(LoadSettings(store).popupTextSize, 10);
    CHECK_EQ(kAutoPopupTextSize + 1, 10);
}

TEST(the_settings_window_keeps_its_theme_and_its_place)
{
    MapStore store;
    store.values["menuTheme"]  = "aurora";
    store.values["menuX"]      = "120";
    store.values["menuY"]      = "80";
    store.values["menuWidth"]  = "900";
    store.values["menuHeight"] = "700";
    const Settings s           = LoadSettings(store);
    CHECK(s.menuTheme == MenuTheme::Aurora && s.menuX == 120 && s.menuY == 80 && s.menuWidth == 900 && s.menuHeight == 700);
    MapStore saved;
    SaveSettings(s, saved);
    CHECK(saved.values["menuTheme"] == "aurora" && saved.values["menuWidth"] == "900.0000" &&
          saved.values["menuY"] == "80.0000");
    // A theme the list lacks keeps the default, and a size of nonsense is no place at all.
    store.values["menuTheme"]  = "neon";
    store.values["menuWidth"]  = "-5";
    store.values["menuHeight"] = "1e9";
    const Settings odd         = LoadSettings(store);
    CHECK(odd.menuTheme == MenuTheme::Phoenix && odd.menuWidth == 0 && odd.menuHeight == kMaxMenuSide);
    // A new file has neither: the window opens in the screen's middle, in the Phoenix look.
    MapStore empty;
    const Settings fresh = LoadSettings(empty);
    CHECK(fresh.menuTheme == MenuTheme::Phoenix && fresh.menuWidth == 0 && fresh.menuHeight == 0);
}

TEST(a_menu_font_the_list_lacks_or_a_size_past_its_ends_keeps_the_defaults)
{
    MapStore store;
    store.values["menuFont"]  = "Comic Sans";
    store.values["menuFontSize"] = "400";
    const Settings s             = LoadSettings(store);
    CHECK(s.menuFont == kDefaultMenuFont && s.menuFontSize == kMaxMenuFontSize);
    CHECK(MenuFontNamed("Segoe UI") != nullptr && MenuFontNamed("Barlow Condensed")->file == nullptr);
}

TEST(the_windows_font_size_is_pixels_and_an_old_percent_becomes_the_pixels_it_drew)
{
    // A percent of Ashita's own font size, 18 pixels, times the font's own scale for faces that run small.
    const auto loaded = [](std::initializer_list<std::pair<const char*, const char*>> keys) {
        MapStore file = ReleaseFile(keys);
        return LoadSettings(file).menuFontSize;
    };
    CHECK_EQ(loaded({{"menuFont", "Segoe UI"}, {"menuScale", "125.0000"}}), 23);
    CHECK_EQ(loaded({{"menuFont", "Barlow Condensed"}, {"menuScale", "125.0000"}}), 26);
    CHECK_EQ(loaded({{"menuFont", "Segoe UI"}, {"menuScale", "100.0000"}}), 18);
    // A percent edited past the old range was drawn at its end: 160 percent, 28.8 pixels.
    CHECK_EQ(loaded({{"menuFont", "Segoe UI"}, {"menuScale", "400.0000"}}), 29);
    CHECK_EQ(loaded({{"menuFont", "Segoe UI"}, {"menuScale", "10.0000"}}), 13); // 70 percent
    // Its own key wins, and the percent is never written again.
    CHECK_EQ(loaded({{"menuScale", "150.0000"}, {"menuFontSize", "20.0000"}}), 20);
    MapStore fresh; // a new install has no percent to convert
    CHECK_EQ(LoadSettings(fresh).menuFontSize, Settings{}.menuFontSize);
    Settings s;
    s.menuFontSize = 30;
    MapStore file  = OwnFileWith(s, {{"menuScale", "80.0000"}});
    CHECK_EQ(LoadSettings(file).menuFontSize, 30);
    SaveSettings(s, file);
    CHECK(std::ranges::find(file.sets, "menuScale") == file.sets.end());
}

TEST(the_window_defaults_place_every_view_by_headsups_defaults_or_leave_them_all_to_the_game)
{
    Settings s;
    s.windowOffsets = {WindowOffset{"equip", 10, -20}};
    s.viewShifts[static_cast<int>(HelpMode::On)] = {ViewShift{std::string(WindowViews()[0].id), 20}};
    s.placedUnder = {1, 2};
    s.keepClear   = false;
    s.moveWindows = false;
    UseHeadsUpWindowDefaults(s);
    CHECK(s.moveWindows && s.keepClear && s.windowOffsets.empty() && s.viewShifts == Settings{}.viewShifts);
    CHECK(s.placedUnder == Settings{}.placedUnder);
    s.windowOffsets = {WindowOffset{"equip", 10, -20}};
    s.moveGameHelp  = true;
    UseGameWindowDefaults(s);
    CHECK(!s.moveWindows && !s.moveGameHelp && s.windowOffsets.empty());
    s.hideHelpBar = false;
    s.barWidgets[static_cast<int>(BarWidget::GameHelp)] = false;
    UseHelpTextDefaults(s);
    CHECK(s.moveWindows && !s.moveGameHelp && s.hideHelpBar && s.barWidgets[static_cast<int>(BarWidget::GameHelp)]);
}

TEST(a_new_install_looks_as_the_release_did_for_one_but_for_the_reviews_four_changes)
{
    MapStore file = ReleaseFile();
    const Settings release = LoadSettings(file);
    Settings fresh;
    const auto iconPixels = [](const Settings& s, const LineDesign& line) {
        return static_cast<float>(s.baseIconSize) * static_cast<float>(line.iconSize) / kPercent;
    };
    for (int k = 0; k < kPlateKindCount; ++k)
    {
        KindSettings& now        = fresh.kinds[static_cast<size_t>(k)];
        const KindSettings& then = release.kinds[static_cast<size_t>(k)];
        for (int l = 0; l < kPlateLines; ++l)
        {
            // A shadow half again as strong as the release's, on the steps of five, and icons of 16 px where a name's
            // were of its 15 px.
            CHECK(std::abs(static_cast<float>(now.lines[l].shadow) - 1.5f * static_cast<float>(then.lines[l].shadow)) <= 2.5f);
            const float grown = iconPixels(fresh, now.lines[l]) - iconPixels(release, then.lines[l]);
            CHECK(grown >= 0.0f && grown < 1.0f);
            now.lines[l].shadow   = then.lines[l].shadow;
            now.lines[l].iconSize = then.lines[l].iconSize;
        }
        // Pets, one row in the release, show their nameplates only when targeted.
        if (k == static_cast<int>(PlateKind::Pets))
            for (int r = 0; r < kPetRowCount; ++r)
            {
                CHECK(then.rows[r].plate == PlateMode::Shown && now.rows[r].plate == PlateMode::WhenTargeted);
                now.rows[r].plate = then.rows[r].plate;
            }
    }
    CHECK(fresh.kinds == release.kinds);
    CHECK(fresh.palette == release.palette);
    // The aggression icon is not shown: the outline's color says it.
    CHECK_EQ(fresh.detectionShown, release.detectionShown & ~DetectionBit(DetectionIcon::Aggro));
    CHECK(fresh.baseSize == release.baseSize && fresh.baseIconSize == release.baseIconSize);
    CHECK(fresh.scaleMin == release.scaleMin && fresh.scaleMax == release.scaleMax);
}

TEST(an_entity_types_color_is_saved_under_its_own_name)
{
    // The menu lists the types in another order than their keys: a saved file's keys are by name.
    MapStore file;
    SaveSettings(Settings{}, file);
    file.values["paletteTypePetR"]         = "0.1250";
    file.values["paletteTypeNpcG"]         = "0.2500";
    file.values["paletteTypeLinkshellB"]   = "0.3750";
    file.values["paletteTypePartyMemberR"] = "0.5000";
    file.values["paletteTypeFriendG"]      = "0.6250";
    file.values["paletteTypePlayerB"]      = "0.7500";
    const Palette p = LoadSettings(file).palette;
    CHECK_EQ(p.type[static_cast<int>(TargetType::Pet)].v[0], 0.125f);
    CHECK_EQ(p.type[static_cast<int>(TargetType::Npc)].v[1], 0.25f);
    CHECK_EQ(p.type[static_cast<int>(TargetType::LinkshellMember)].v[2], 0.375f);
    CHECK_EQ(p.type[static_cast<int>(TargetType::PartyMember)].v[0], 0.5f);
    CHECK_EQ(p.type[static_cast<int>(TargetType::Friend)].v[1], 0.625f);
    CHECK_EQ(p.type[static_cast<int>(TargetType::Player)].v[2], 0.75f);
}

TEST(incredibly_tough_has_a_color_of_its_own_which_a_file_without_it_takes_from_the_defaults)
{
    const Palette defaults = Settings{}.palette;
    const Color& deep = defaults.toughness[static_cast<int>(LabelShade::IncrediblyTough)];
    const Color& very = defaults.toughness[static_cast<int>(LabelShade::VeryTough)];
    // Deep red: red alone, darker than Very Tough's.
    CHECK(deep.v[0] > 4 * deep.v[1] && deep.v[0] > 4 * deep.v[2] && deep.v[0] < very.v[0]);
    Settings s;
    s.palette.toughness[static_cast<int>(LabelShade::VeryTough)] = {{0.25f, 0.5f, 0.75f}};
    MapStore file;
    SaveSettings(s, file);
    CHECK(file.values.contains("paletteToughnessIncrediblyToughR"));
    for (const char* key : {"paletteToughnessIncrediblyToughR", "paletteToughnessIncrediblyToughG", "paletteToughnessIncrediblyToughB"})
        file.values.erase(key);
    const Palette loaded = LoadSettings(file).palette;
    CHECK(loaded.toughness[static_cast<int>(LabelShade::IncrediblyTough)] == deep);
    CHECK(loaded.toughness[static_cast<int>(LabelShade::VeryTough)] == (Color{{0.25f, 0.5f, 0.75f}}));
    MapStore release = ReleaseFile();
    CHECK(LoadSettings(release).palette.toughness[static_cast<int>(LabelShade::IncrediblyTough)] == deep);
}

TEST(every_designer_key_round_trips)
{
    Settings s;
    KindSettings& mobs = s.Kind(PlateKind::Mobs);
    mobs.rows[CategoryIndex(Category::WontAttack)].plate   = PlateMode::WhenTargeted;
    mobs.rows[CategoryIndex(Category::WontAttack)].outline = OutlineMode::WhenTargeted;
    mobs.rows[CategoryIndex(Category::WontAttack)].outlineColor = ColorPick{ColorSource::Custom, {{0.1f, 0.2f, 0.3f}}};
    mobs.rows[CategoryIndex(Category::Dead)] = RowSettings{PlateMode::Hidden, OutlineMode::Shown, ColorPick{ColorSource::Toughness}};
    LineDesign& line = mobs.lines[1];
    line.text     = "Lv $level $con $$ $id";
    line.syncWith = 2;
    line.show     = LineShow{false, ConditionBit(ShowCondition::Targeted) | ConditionBit(ShowCondition::YourClaim)};
    line.align    = Align::Right;
    line.font     = "Cinzel";
    line.style    = StyleBit(FontStyle::Italic) | StyleBit(FontStyle::Underline);
    line.textSize = 150;
    line.iconSize = 60;
    line.color    = ColorPick{ColorSource::Claim};
    line.shadow   = 40;
    line.padding  = 7;
    line.shadowColor   = ColorPick{ColorSource::Custom, {{0.4f, 0.5f, 0.6f}}};
    line.glow          = 120;
    line.glowSize      = 70;
    line.glowColor     = ColorPick{ColorSource::Type};
    line.glowOffInCombat = false;
    mobs.ornament      = OrnamentSettings{OrnamentPlace::Top, 120, 9, ColorPick{ColorSource::Target}};
    mobs.opacity       = OpacitySettings{70, 35, 95, true, 22.0f};
    s.Kind(PlateKind::Self).opacity.noTarget = 55;
    s.Kind(PlateKind::Pets).lines[0].text = "$owner's $name";
    s.palette.claim[static_cast<int>(ClaimState::Yours)] = {{0.7f, 0.8f, 0.9f}};
    s.nameplates = false;
    s.outlines   = false;
    s.baseSize   = 22;
    s.defaultFont = "Cinzel";
    s.scaleMin   = 80;
    s.scaleMax   = 120;
    s.timerPercent = 50;
    s.detectionShown = DetectionBit(DetectionIcon::Aggro) | DetectionBit(DetectionIcon::Blood);
    s.cursorEdgeColor = ColorPick{ColorSource::Type, {{0.2f, 0.2f, 0.2f}}};
    s.cursorColor     = ColorPick{ColorSource::Custom, {{0.9f, 0.8f, 0.1f}}};
    s.cursorStyle     = CursorStyle::File;
    s.cursorFile      = "My Cursor";
    s.dashMark        = DashMark::Square;
    s.pointerIcon     = PointerIcon::File;
    s.pointerFile     = "Gold Arrow";
    MapStore store;
    SaveSettings(s, store);
    CHECK(LoadSettings(store) == s);
    CHECK_EQ(store.values.at("mobsRowPassivePlate"), "whenTargeted");
    CHECK_EQ(store.values.at("mobsLine2Text"), "Lv $level $con $$ $id");
    CHECK_EQ(store.values.at("mobsLine2When"), "5");
    CHECK_EQ(store.values.at("petsLine1Text"), "$owner's $name");
    CHECK(!store.values.contains("friends")); // the list of friends kept by hand is gone: the game's own is read
    CHECK_EQ(store.values.at("settingsVersion"), "4.0000");
}

TEST(clamp_breaks_sync_cycles_keeping_the_lower_line)
{
    Settings s;
    KindSettings& mobs = s.Kind(PlateKind::Mobs);
    mobs.lines[1].syncWith = 2;
    mobs.lines[2].syncWith = 1;
    const Settings c = Clamp(s);
    CHECK_EQ(c.Kind(PlateKind::Mobs).lines[1].syncWith, 2);
    CHECK_EQ(c.Kind(PlateKind::Mobs).lines[2].syncWith, kNoSync);
    Settings self;
    self.Kind(PlateKind::Mobs).lines[0].syncWith = 0;
    CHECK_EQ(Clamp(self).Kind(PlateKind::Mobs).lines[0].syncWith, kNoSync);
    Settings chain;
    chain.Kind(PlateKind::Mobs).lines[0].syncWith = 1;
    chain.Kind(PlateKind::Mobs).lines[1].syncWith = 2;
    chain.Kind(PlateKind::Mobs).lines[2].syncWith = 0;
    const Settings c2 = Clamp(chain);
    CHECK_EQ(c2.Kind(PlateKind::Mobs).lines[0].syncWith, 1);
    CHECK_EQ(c2.Kind(PlateKind::Mobs).lines[1].syncWith, 2);
    CHECK_EQ(c2.Kind(PlateKind::Mobs).lines[2].syncWith, kNoSync);
}

TEST(clamp_keeps_designer_values_in_range)
{
    Settings s;
    s.baseSize = 999;
    s.scaleMin = 300;
    s.scaleMax = 10;
    s.Kind(PlateKind::Mobs).lines[0].textSize = 1;
    s.Kind(PlateKind::Mobs).lines[0].glow     = 999;
    s.Kind(PlateKind::Mobs).lines[0].syncWith = 7;
    s.Kind(PlateKind::Mobs).lines[0].text     = std::string(kMaxTemplate + 40, 'x');
    s.Kind(PlateKind::Mobs).lines[0].font     = std::string(kMaxFontName + 1, 'f');
    s.Kind(PlateKind::Mobs).lines[1].text     = "Lv $level\n$con";
    s.Kind(PlateKind::Mobs).ornament.width    = -5;
    s.Kind(PlateKind::Mobs).ornament.height   = 0;
    s.Kind(PlateKind::Players).ornament.width  = 9999;
    s.Kind(PlateKind::Players).ornament.height = 9999;
    s.palette.toughness[static_cast<int>(LabelShade::Tough)].v[1] = std::nanf("");
    s.palette.claim[static_cast<int>(ClaimState::Yours)]          = {{2.0f, -1.0f, 0.5f}};
    const Settings c = Clamp(s);
    CHECK_EQ(c.baseSize, kMaxTextSize);
    CHECK_EQ(c.scaleMin, 300);
    CHECK_EQ(c.scaleMax, 300); // the maximum is never below the minimum
    CHECK_EQ(c.Kind(PlateKind::Mobs).lines[0].textSize, kMinSizePercent);
    CHECK_EQ(c.Kind(PlateKind::Mobs).lines[0].glow, kMaxGlow);
    CHECK_EQ(c.Kind(PlateKind::Mobs).lines[0].syncWith, kNoSync);
    CHECK_EQ(c.Kind(PlateKind::Mobs).lines[0].text.size(), kMaxTemplate);
    CHECK(c.Kind(PlateKind::Mobs).lines[0].font.empty());
    CHECK_EQ(c.Kind(PlateKind::Mobs).lines[1].text, "Lv $level $con");
    CHECK_EQ(c.Kind(PlateKind::Mobs).ornament.width, kMinOrnamentWidth);
    CHECK_EQ(c.Kind(PlateKind::Mobs).ornament.height, kMinOrnamentThickness);
    CHECK_EQ(c.Kind(PlateKind::Players).ornament.width, kMaxOrnamentWidth);
    CHECK_EQ(c.Kind(PlateKind::Players).ornament.height, kMaxOrnamentThickness);
    // The channel that was no number takes the default's.
    CHECK(c.palette.toughness[static_cast<int>(LabelShade::Tough)] == (Color{{1.00f, 1.00f, 0.35f}}));
    CHECK(c.palette.claim[static_cast<int>(ClaimState::Yours)] == (Color{{1.0f, 0.0f, 0.5f}}));
}

TEST(clamp_puts_a_color_source_its_kind_does_not_offer_back_to_the_default)
{
    const Settings d;
    Settings s;
    const Color mine{{0.1f, 0.2f, 0.3f}};
    KindSettings& players = s.Kind(PlateKind::Players);
    players.rows[static_cast<int>(PlayerRow::Friend)].outlineColor = ColorPick{ColorSource::Aggression, mine};
    players.lines[0].glowColor  = ColorPick{ColorSource::Toughness, mine};
    players.lines[2].color      = ColorPick{ColorSource::Claim, mine};
    players.ornament.color      = ColorPick{ColorSource::Game, mine}; // the game's color is a text line's alone
    s.Kind(PlateKind::Pets).rows[0].outlineColor = ColorPick{ColorSource::Target, mine}; // offered: kept
    KindSettings& mobs = s.Kind(PlateKind::Mobs);
    mobs.rows[CategoryIndex(Category::WontAttack)].outlineColor = ColorPick{ColorSource::Aggression, mine};
    mobs.rows[CategoryIndex(Category::Unknown)].outlineColor    = ColorPick{ColorSource::Game, mine};
    mobs.rows[CategoryIndex(Category::OtherClaim)].outlineColor = ColorPick{ColorSource::Game, mine};
    mobs.lines[0].glowColor = ColorPick{ColorSource::Toughness, mine};
    mobs.lines[0].color     = ColorPick{ColorSource::Game, mine};
    const Settings c = Clamp(s);
    const KindSettings& p = c.Kind(PlateKind::Players);
    CHECK((p.rows[static_cast<int>(PlayerRow::Friend)].outlineColor == ColorPick{ColorSource::Type, mine}));
    CHECK((p.lines[0].glowColor == ColorPick{ColorSource::Type, mine}));
    CHECK((p.lines[2].color == ColorPick{d.Kind(PlateKind::Players).lines[2].color.source, mine}));
    CHECK(p.lines[2].color.source == ColorSource::Game);
    CHECK((p.ornament.color == ColorPick{ColorSource::Type, mine}));
    CHECK((c.Kind(PlateKind::Pets).rows[0].outlineColor == ColorPick{ColorSource::Target, mine}));
    const KindSettings& m = c.Kind(PlateKind::Mobs);
    CHECK((m.rows[CategoryIndex(Category::WontAttack)].outlineColor == ColorPick{ColorSource::Aggression, mine}));
    CHECK((m.rows[CategoryIndex(Category::Unknown)].outlineColor == ColorPick{ColorSource::Aggression, mine}));
    CHECK((m.rows[CategoryIndex(Category::OtherClaim)].outlineColor == ColorPick{ColorSource::Claim, mine}));
    CHECK((m.lines[0].glowColor == ColorPick{ColorSource::Toughness, mine}));
    CHECK((m.lines[0].color == ColorPick{ColorSource::Game, mine}));
    CHECK(Clamp(d) == d);
}

TEST(a_file_that_saved_aggression_on_a_kind_without_one_loads_as_target_type)
{
    MapStore saved;
    SaveSettings(Settings{}, saved);
    for (const char* key : {"playersRowPlayerColorSource", "selfLine3GlowColorSource", "npcsOrnamentColorSource",
             "petsRowOthersPetColorSource"})
    {
        CHECK(saved.values.contains(key));
        saved.values[key] = "aggression";
    }
    saved.values["mobsRowPassiveColorSource"] = "aggression";
    const Settings s = LoadSettings(saved);
    CHECK(s.Kind(PlateKind::Players).rows[static_cast<int>(PlayerRow::Player)].outlineColor.source == ColorSource::Type);
    CHECK(s.Kind(PlateKind::Self).lines[2].glowColor.source == ColorSource::Type);
    CHECK(s.Kind(PlateKind::Npcs).ornament.color.source == ColorSource::Type);
    CHECK(s.Kind(PlateKind::Pets).rows[static_cast<int>(PetRow::Others)].outlineColor.source == ColorSource::Type);
    CHECK(s.Kind(PlateKind::Mobs).rows[CategoryIndex(Category::WontAttack)].outlineColor.source == ColorSource::Aggression);
    CHECK(s == Settings{});
}

TEST(an_old_file_migrates_field_by_field)
{
    MapStore store;
    std::istringstream file(kSavedFile); // the version 2 file at the top of this test, every value off its default
    for (std::string line; std::getline(file, line);)
        if (const size_t eq = line.find('='); eq != std::string::npos)
            store.values[line.substr(0, eq)] = line.substr(eq + 1);
    const Settings s = LoadSettings(store);
    // replaceMobNames=false, replacePlayerNames=false, replaceNpcNames=false: every row Vanilla.
    for (PlateKind kind : {PlateKind::Mobs, PlateKind::Players, PlateKind::Self, PlateKind::Npcs})
        for (int r = 0; r < RowCountOf(kind); ++r)
            CHECK(s.Kind(kind).rows[r].plate == PlateMode::Vanilla);
    CHECK(s.Kind(PlateKind::Pets).rows[0].plate == PlateMode::Vanilla); // a pet was drawn as a mob
    // willAttackShow and friends set the outline modes; the claim keys set the claim rows' color source.
    const KindSettings& mobs = s.Kind(PlateKind::Mobs);
    CHECK(mobs.rows[CategoryIndex(Category::WillAttack)].outline ==
          OutlineMode::Disabled); // willAttackShow=false
    CHECK(mobs.rows[CategoryIndex(Category::PartyClaim)].outlineColor.source ==
          ColorSource::Claim); // partyClaimShow=true
    // The category and label colors fill the palettes.
    CHECK(s.palette.aggression[static_cast<int>(Nature::Aggressive)] ==
          (Color{{std::strtof(store.values.at("willAttackR").c_str(), nullptr), std::strtof(store.values.at("willAttackG").c_str(), nullptr),
                 std::strtof(store.values.at("willAttackB").c_str(), nullptr)}}));
    CHECK(s.palette.claim[static_cast<int>(ClaimState::Yours)] ==
          (Color{{std::strtof(store.values.at("partyClaimR").c_str(), nullptr), std::strtof(store.values.at("partyClaimG").c_str(), nullptr),
                 std::strtof(store.values.at("partyClaimB").c_str(), nullptr)}}));
    CHECK(s.palette.toughness[static_cast<int>(LabelShade::Tough)] ==
          (Color{{std::strtof(store.values.at("labelToughR").c_str(), nullptr), std::strtof(store.values.at("labelToughG").c_str(), nullptr),
                 std::strtof(store.values.at("labelToughB").c_str(), nullptr)}}));
    CHECK(s.palette.target[static_cast<int>(TargetState::Locked)] ==
          (Color{{std::strtof(store.values.at("lockedCursorColorR").c_str(), nullptr), std::strtof(store.values.at("lockedCursorColorG").c_str(), nullptr),
                 std::strtof(store.values.at("lockedCursorColorB").c_str(), nullptr)}}));
    // Fonts and sizes.
    CHECK_EQ(mobs.lines[2].font, store.values.at("fontName"));
    CHECK_EQ(mobs.lines[2].style, StyleBit(FontStyle::Bold)); // fontBold=true
    CHECK_EQ(s.Kind(PlateKind::Pets).lines[2].font, store.values.at("fontName"));
    CHECK_EQ(s.Kind(PlateKind::Pets).lines[2].style, StyleBit(FontStyle::Bold));
    CHECK_EQ(mobs.lines[1].font, store.values.at("labelFontName"));
    CHECK_EQ(s.Kind(PlateKind::Players).lines[2].font, store.values.at("playerFontName"));
    CHECK_EQ(s.Kind(PlateKind::Npcs).lines[2].font, store.values.at("npcFontName"));
    const int base = static_cast<int>(std::strtof(store.values.at("mobNameSize").c_str(), nullptr));
    CHECK_EQ(s.baseSize, base);
    CHECK_EQ(mobs.lines[2].textSize, 100);
    CHECK_EQ(mobs.lines[1].textSize, Stepped(static_cast<int>(std::lround(100.0f * std::strtof(store.values.at("labelSize").c_str(), nullptr) / static_cast<float>(base))), kPercentStep));
    CHECK_EQ(mobs.lines[0].iconSize, 150); // iconSize=24 px, of the 16 px base icon
    CHECK_EQ(s.Kind(PlateKind::Self).lines[2].textSize, Stepped(static_cast<int>(std::lround(100.0f * std::strtof(store.values.at("selfNameSize").c_str(), nullptr) / static_cast<float>(base))), kPercentStep));
    // showLabels=false with mobId=lastThree leaves the ID alone on line 2; showIcons=false with the three badge
    // switches on leaves the badge tags on line 1; hideClaimed shows both lines on unclaimed mobs only.
    // mobIconPlace=inline with the level text "$id": the icons move onto line 2, left of the ID (badgeSide=left;
    // detectionIconSide=right, but showIcons=false has no detection tag).
    CHECK_EQ(mobs.lines[0].text, "");
    CHECK_EQ(mobs.lines[1].text, "$weak$resist$status$immune $id");
    CHECK(mobs.lines[0].show == LineShow{}); // no text on line 1, so hideClaimed=true leaves it alone
    CHECK(mobs.lines[1].show == (LineShow{false, ConditionBit(ShowCondition::Unclaimed)}));
    CHECK(mobs.lines[2].show == LineShow{});
    CHECK(mobs.rows[CategoryIndex(Category::PartyClaim)].outline == OutlineMode::Shown); // its own switch was on
    CHECK(mobs.rows[CategoryIndex(Category::PartyClaim)].outlineColor.source == ColorSource::Claim);
    CHECK_EQ(mobs.lines[2].glow, 0); // nameGlow=false
    CHECK(mobs.ornament.place == OrnamentPlace::Disabled); // showOrnament=false
    CHECK(mobs.ornament.color.source == ColorSource::Custom); // ownOrnamentColor=true
    CHECK_EQ(mobs.ornament.width, static_cast<int>(std::strtof(store.values.at("ornamentWidth").c_str(), nullptr)));
    CHECK_EQ(mobs.ornament.height, static_cast<int>(std::strtof(store.values.at("ornamentThickness").c_str(), nullptr)));
    CHECK_EQ(mobs.lines[2].shadow, 35); // nameShadow=50, of a unit that is now one and a half of it: 33, on the steps of five
    // ownNameColor, nameShadow and the player icon size reach the players' name line too.
    const LineDesign& playerName = s.Kind(PlateKind::Players).lines[2];
    CHECK(mobs.lines[2].color.source == ColorSource::Custom && playerName.color.source == ColorSource::Custom);
    CHECK_EQ(playerName.shadow, mobs.lines[2].shadow);
    CHECK_EQ(playerName.iconSize, 100); // playerIconSize=120 of playerNameSize=13 is 15.6 px, 98 of the 16 px base icon
    CHECK_EQ(s.Kind(PlateKind::Self).lines[2].iconSize, 130); // of selfNameSize=17: 20.4 px, 128 of 16
    CHECK_EQ(playerName.text, "$name"); // showPlayerIcons=false
    // Saved again, the file carries the new version and the new keys.
    MapStore again;
    SaveSettings(s, again);
    CHECK_EQ(again.values.at("settingsVersion"), "4.0000");
    CHECK(LoadSettings(again) == s);
}

TEST(a_version_2_file_with_mob_ids_puts_the_id_tags_on_line_2)
{
    MapStore store;
    store.values["settingsVersion"]  = "2.0000";
    store.values["mobId"]            = "lastThree";
    store.values["markPlaceholders"] = "true";
    CHECK_EQ(LoadSettings(store).Kind(PlateKind::Mobs).lines[1].text, "Lv $level $con $id $ph");
    store.values["mobId"] = "full";
    CHECK_EQ(LoadSettings(store).Kind(PlateKind::Mobs).lines[1].text, "Lv $level $con $serverid $id $ph");
    store.values["mobId"] = "off";
    CHECK_EQ(LoadSettings(store).Kind(PlateKind::Mobs).lines[1].text, "Lv $level $con");
}

namespace
{
    Settings Load2(std::initializer_list<std::pair<const char*, const char*>> keys)
    {
        MapStore store;
        store.values["settingsVersion"] = "2.0000";
        for (const auto& [key, value] : keys)
            store.values[key] = value;
        return LoadSettings(store);
    }

    const std::initializer_list<std::pair<const char*, const char*>> kNaturesOff = {{"willAttackShow", "false"},
        {"wontAttackShow", "false"}, {"unknownShow", "false"}, {"nmWillAttackShow", "false"}, {"nmWontAttackShow", "false"},
        {"placeholderShow", "false"}};

    Settings Load2WithNaturesOff(std::initializer_list<std::pair<const char*, const char*>> more)
    {
        MapStore store;
        store.values["settingsVersion"] = "2.0000";
        for (const auto& [key, value] : kNaturesOff)
            store.values[key] = value;
        for (const auto& [key, value] : more)
            store.values[key] = value;
        return LoadSettings(store);
    }

    const RowSettings& PartyRow(const Settings& s) { return s.Kind(PlateKind::Mobs).rows[CategoryIndex(Category::PartyClaim)]; }
    const RowSettings& OthersRow(const Settings& s) { return s.Kind(PlateKind::Mobs).rows[CategoryIndex(Category::OtherClaim)]; }
}

TEST(a_claim_row_is_shown_in_the_claim_color_only_by_its_own_switch_else_by_nature)
{
    for (const Settings& s : {Load2({}), Load2WithNaturesOff({})})
    {
        CHECK(PartyRow(s).outline == OutlineMode::ByNature && OthersRow(s).outline == OutlineMode::ByNature);
        CHECK(PartyRow(s).outlineColor.source == ColorSource::Claim);
    }
    const Settings party = Load2({{"partyClaimShow", "true"}});
    CHECK(PartyRow(party).outline == OutlineMode::Shown && PartyRow(party).outlineColor.source == ColorSource::Claim);
    CHECK(OthersRow(party).outline == OutlineMode::ByNature);
    const Settings others = Load2WithNaturesOff({{"otherClaimShow", "true"}});
    CHECK(OthersRow(others).outline == OutlineMode::Shown && OthersRow(others).outlineColor.source == ColorSource::Claim);
    CHECK(PartyRow(others).outline == OutlineMode::ByNature);
}

TEST(the_placeholder_row_is_shown_by_its_switch_else_by_nature)
{
    const Settings on  = Load2({{"placeholderShow", "true"}});
    const Settings off = Load2({{"placeholderShow", "false"}});
    CHECK(on.Kind(PlateKind::Mobs).rows[CategoryIndex(Category::Placeholder)].outline == OutlineMode::Shown);
    CHECK(off.Kind(PlateKind::Mobs).rows[CategoryIndex(Category::Placeholder)].outline == OutlineMode::ByNature);
}

TEST(by_nature_round_trips_on_a_claim_row_and_is_reset_elsewhere)
{
    Settings s;
    s.Kind(PlateKind::Mobs).rows[CategoryIndex(Category::OtherClaim)].outline = OutlineMode::ByNature;
    MapStore store;
    SaveSettings(s, store);
    CHECK(LoadSettings(store).Kind(PlateKind::Mobs).rows[CategoryIndex(Category::OtherClaim)].outline == OutlineMode::ByNature);
    s.Kind(PlateKind::Mobs).rows[CategoryIndex(Category::WillAttack)].outline = OutlineMode::ByNature;
    s.Kind(PlateKind::Players).rows[static_cast<int>(PlayerRow::Player)].outline = OutlineMode::ByNature;
    const Settings c = Clamp(s);
    CHECK(c.Kind(PlateKind::Mobs).rows[CategoryIndex(Category::WillAttack)].outline == OutlineMode::Shown);
    CHECK(c.Kind(PlateKind::Players).rows[static_cast<int>(PlayerRow::Player)].outline == OutlineMode::Disabled);
    CHECK(c.Kind(PlateKind::Mobs).rows[CategoryIndex(Category::OtherClaim)].outline == OutlineMode::ByNature);
}

TEST(the_mob_icon_row_is_written_as_the_old_plate_laid_it_out)
{
    auto lines = [](std::initializer_list<std::pair<const char*, const char*>> keys) {
        const Settings s = Load2(keys);
        return std::pair<std::string, std::string>{s.Kind(PlateKind::Mobs).lines[0].text, s.Kind(PlateKind::Mobs).lines[1].text};
    };
    using Pair = std::pair<std::string, std::string>;
    CHECK(lines({{"showDamageIcons", "true"}}) == (Pair{"$detect$weak$resist", "Lv $level $con"}));
    CHECK(lines({{"showDamageIcons", "true"}, {"badgeSide", "left"}}) == (Pair{"$weak$resist$detect", "Lv $level $con"}));
    CHECK(lines({{"showDamageIcons", "true"}, {"detectionIconSide", "right"}, {"badgeSide", "left"}}) ==
          (Pair{"$weak$resist$detect", "Lv $level $con"}));
    CHECK(lines({{"showDamageIcons", "true"}, {"detectionIconSide", "right"}}) == (Pair{"$detect$weak$resist", "Lv $level $con"}));
    CHECK(lines({{"showStatusRankIcons", "true"}, {"showImmunityIcons", "true"}}) == (Pair{"$detect$status$immune", "Lv $level $con"}));
    CHECK(lines({{"mobIconPlace", "center"}, {"showDamageIcons", "true"}}) == (Pair{"$detect$weak$resist", "Lv $level $con"}));
    CHECK(lines({{"mobIconPlace", "inline"}, {"showDamageIcons", "true"}}) == (Pair{"", "$detect Lv $level $con $weak$resist"}));
    CHECK(lines({{"mobIconPlace", "inline"}, {"showDamageIcons", "true"}, {"detectionIconSide", "right"}, {"badgeSide", "right"}}) ==
          (Pair{"", "Lv $level $con $detect$weak$resist"}));
    CHECK(lines({{"mobIconPlace", "inline"}, {"showDamageIcons", "true"}, {"detectionIconSide", "left"}, {"badgeSide", "left"}}) ==
          (Pair{"", "$weak$resist$detect Lv $level $con"}));
    CHECK(lines({{"mobIconPlace", "inline"}, {"showDamageIcons", "true"}, {"showLabels", "false"}, {"mobId", "off"}}) ==
          (Pair{"$detect$weak$resist", ""}));
}

TEST(a_players_name_line_carries_the_icons_the_old_settings_showed_on_each_side)
{
    auto name = [](std::initializer_list<std::pair<const char*, const char*>> keys) {
        const Settings s = Load2(keys);
        CHECK(s.Kind(PlateKind::Players).lines[2].text == s.Kind(PlateKind::Self).lines[2].text);
        return s.Kind(PlateKind::Players).lines[2].text;
    };
    CHECK_EQ(name({{"showPlayerIcons", "false"}}), "$name");
    CHECK_EQ(name({{"iconLinkshell", "right"}, {"iconBazaar", "hide"}}), "$mentor$new$sync$away$seeking$gm$name$linkshell");
    CHECK_EQ(name({{"playerIconOrder", "iconSeekingParty iconGm iconMentor iconNewAdventurer iconLevelSync iconAway iconLinkshell iconBazaar"}}),
        "$seeking$mentor$new$sync$away$linkshell$bazaar$gm$name");
    CHECK_EQ(name({{"iconGm", "hide"}}), "$mentor$new$sync$away$linkshell$bazaar$seeking$gm$name"); // GM shows whatever its side
}

TEST(a_players_icons_are_a_percent_of_that_players_own_name_size)
{
    const Settings s = Load2({{"mobNameSize", "20"}, {"playerNameSize", "14"}, {"selfNameSize", "24"}, {"playerIconSize", "100"}});
    CHECK_EQ(s.Kind(PlateKind::Players).lines[2].iconSize, 90); // 14 px of the 16 px base icon, on the steps of five
    CHECK_EQ(s.Kind(PlateKind::Self).lines[2].iconSize, 150); // 24 px
}

TEST(the_hide_switches_that_have_a_condition_become_one_and_the_others_are_dropped)
{
    const LineShow always;
    const LineShow party = LineShow{false, ConditionBit(ShowCondition::Unclaimed) | ConditionBit(ShowCondition::OthersClaim)};
    const LineShow all   = LineShow{false, ConditionBit(ShowCondition::Unclaimed)};
    const Settings byPartyFile = Load2({{"hideInCombat", "true"}});
    const KindSettings& byParty = byPartyFile.Kind(PlateKind::Mobs);
    CHECK(byParty.lines[0].show == party && byParty.lines[1].show == party && byParty.lines[2].show == always);
    const Settings claimedFile = Load2({{"hideInCombat", "true"}, {"hideClaimed", "true"}});
    const KindSettings& claimed = claimedFile.Kind(PlateKind::Mobs);
    CHECK(claimed.lines[0].show == all && claimed.lines[1].show == all && claimed.lines[2].show == always);
    for (const char* dropped : {"hideTooWeak", "hideWhileEngaged"})
    {
        const Settings file = Load2({{dropped, "true"}});
        const KindSettings& k = file.Kind(PlateKind::Mobs);
        CHECK(k.lines[0].show == always && k.lines[1].show == always && k.lines[2].show == always);
    }
}

TEST(the_old_glow_and_shadow_settings_reach_the_mob_lines_and_the_cursor_edge)
{
    const Settings plainFile = Load2({});
    const KindSettings& plain = plainFile.Kind(PlateKind::Mobs);
    CHECK(plain.lines[2].glowColor.source == ColorSource::Aggression);
    const Settings own = Load2({{"ownGlowColor", "true"}, {"glowColorR", "0.25"}, {"glowColorG", "0.5"}, {"glowColorB", "0.75"}});
    CHECK(own.Kind(PlateKind::Mobs).lines[2].glowColor.source == ColorSource::Custom);
    CHECK(own.Kind(PlateKind::Mobs).lines[2].glowColor.custom == (Color{{0.25f, 0.5f, 0.75f}}));
    const Settings glowFile = Load2({{"nameGlow", "true"}, {"glowStrength", "150"}, {"glowSize", "70"}});
    const KindSettings& glow = glowFile.Kind(PlateKind::Mobs);
    CHECK_EQ(glow.lines[2].glow, 150);
    CHECK_EQ(glow.lines[2].glowSize, 70);
    const Settings shadow = Load2({{"labelShadow", "40"}, {"labelShadowColorR", "0.1"}, {"labelShadowColorG", "0.2"},
        {"labelShadowColorB", "0.3"}, {"nameShadow", "170"}});
    const KindSettings& mobs = shadow.Kind(PlateKind::Mobs);
    CHECK_EQ(mobs.lines[1].shadow, 25); // 40 of the unit before, which was two thirds of this one, on the steps of five
    CHECK(mobs.lines[1].shadowColor.custom == (Color{{0.1f, 0.2f, 0.3f}}));
    CHECK_EQ(mobs.lines[2].shadow, 115); // 170 likewise
    CHECK_EQ(shadow.Kind(PlateKind::Players).lines[2].shadow, 115);
    const Settings outline = Load2({{"textOutlineR", "0.6"}, {"textOutlineG", "0.7"}, {"textOutlineB", "0.8"}});
    const Color edge{{0.6f, 0.7f, 0.8f}};
    CHECK((outline.cursorEdgeColor == ColorPick{ColorSource::Custom, edge}));
    CHECK(outline.Kind(PlateKind::Mobs).lines[0].shadowColor.custom == edge);
    CHECK(outline.Kind(PlateKind::Mobs).lines[2].shadowColor.custom == edge);
    CHECK(outline.Kind(PlateKind::Pets).lines[1].shadowColor.custom == edge);
    CHECK(outline.Kind(PlateKind::Mobs).lines[1].shadowColor.custom == edge); // labelShadowColor follows textOutline when unset
}

namespace
{
    Settings MigratedSavedFile()
    {
        MapStore old = Parse(kSavedFile);
        return LoadSettings(old);
    }

    // A file this build's save wrote, every value off its default.
    MapStore SavedByThisBuild()
    {
        MapStore file;
        SaveSettings(MigratedSavedFile(), file);
        file.sets.clear();
        return file;
    }

    // What a save of every key writes.
    std::map<std::string, std::string> WholeSave(const Settings& s)
    {
        MapStore whole;
        SaveSettings(s, whole);
        return whole.values;
    }
}

TEST(a_save_after_loading_this_builds_file_passes_on_only_the_key_that_changed)
{
    MapStore file = SavedByThisBuild();
    ChangedKeys keys(file);
    Settings s = keys.Load();
    s.Kind(PlateKind::Mobs).lines[2].glow = 150; // the Glow slider of the mobs' third line
    CHECK((keys.Save(s) == std::vector<KeyText>{{"mobsLine3Glow", "150.0000"}}));
    CHECK(file.sets == std::vector<std::string>{"mobsLine3Glow"});
    keys.ReadBack(true);
    file.sets.clear();
    s.Kind(PlateKind::Pets).lines[0].text = "of $owner";
    CHECK((keys.Save(s) == std::vector<KeyText>{{"petsLine1Text", "of $owner"}}));
    CHECK(file.sets == std::vector<std::string>{"petsLine1Text"});
    CHECK(file.values == WholeSave(s)); // the store is what a save of every key would have made it
}

TEST(a_save_with_nothing_changed_passes_nothing_on)
{
    MapStore file = SavedByThisBuild();
    ChangedKeys keys(file);
    const Settings s = keys.Load();
    CHECK(keys.Save(s).empty());
    CHECK(file.sets.empty());
    CHECK(!keys.Unwritten());
}

TEST(the_first_save_after_a_migration_passes_on_every_key_the_file_lacks_and_none_of_the_old_ones)
{
    MapStore file = Parse(kSavedFile);
    const std::map<std::string, std::string> before = file.values;
    ChangedKeys keys(file);
    const Settings s = keys.Load();
    CHECK(file.sets.empty()); // loading sets nothing
    const std::map<std::string, std::string> whole = WholeSave(s);
    std::map<std::string, std::string> expected; // what a whole save holds that the old file lacks or holds otherwise
    for (const auto& [key, text] : whole)
        if (!before.contains(key) || before.at(key) != text) expected[key] = text;
    std::map<std::string, std::string> passed;
    for (const KeyText& k : keys.Save(s))
        passed[k.key] = k.text;
    CHECK(passed == expected);
    CHECK_EQ(file.sets.size(), passed.size()); // each key once
    for (const char* designer : {"mobsRowAggressivePlate", "mobsLine3Text", "petsLine1TextSize", "selfOrnamentVisibility",
             "paletteAggressionPassiveR", "defaultFont", "nameplateDesigns", "settingsVersion"})
        CHECK(passed.contains(designer));
    for (const char* old : kOldKeys)
        CHECK(!passed.contains(old));
    CHECK(!passed.contains("smoothness") && !passed.contains("barTheme")); // held already as a save writes them
    for (const auto& [key, text] : whole)
        CHECK_EQ(file.values.at(key), text);
    keys.ReadBack(true);
    file.sets.clear();
    CHECK(keys.Save(s).empty());
    CHECK(file.sets.empty());
}

TEST(a_new_installs_first_save_passes_on_every_key)
{
    MapStore fresh;
    ChangedKeys keys(fresh);
    const Settings s = keys.Load();
    CHECK(s == Settings{});
    CHECK_EQ(keys.Save(s).size(), WholeSave(s).size());
    CHECK(fresh.values == WholeSave(s));
}

TEST(a_value_changed_and_changed_back_is_passed_on_both_times)
{
    MapStore file = SavedByThisBuild();
    ChangedKeys keys(file);
    Settings s = keys.Load();
    const int was = s.nameRaise;
    s.nameRaise   = was + 5;
    CHECK((keys.Save(s) == std::vector<KeyText>{{"nameRaise", "17.0000"}})); // kSavedFile's 12 and 5
    keys.ReadBack(true);
    s.nameRaise = was;
    CHECK((keys.Save(s) == std::vector<KeyText>{{"nameRaise", "12.0000"}}));
    CHECK((file.sets == std::vector<std::string>{"nameRaise", "nameRaise"}));
}

TEST(the_first_save_passes_on_what_the_file_holds_otherwise_than_a_save_writes_it)
{
    // A maximum below its minimum, which Clamp raises: the window shows 300 and the file must come to hold it.
    MapStore file = SavedByThisBuild();
    file.values["scaleMin"] = "300.0000";
    file.values["scaleMax"] = "10.0000";
    file.values["maxDistance"] = "25.5"; // typed by hand, as a save does not spell it
    file.values.erase("barHeight");   // and a key taken out
    ChangedKeys keys(file);
    const Settings s = keys.Load();
    CHECK_EQ(s.scaleMax, 300);
    CHECK(file.sets.empty());
    std::map<std::string, std::string> passed;
    for (const KeyText& k : keys.Save(s))
        passed[k.key] = k.text;
    CHECK((passed == std::map<std::string, std::string>{{"scaleMax", "300.0000"}, {"maxDistance", "25.5000"},
                         {"barHeight", "45.0000"}})); // the default height, as the file had none
    CHECK(file.values == WholeSave(s));
}

namespace
{
    bool OutdatedWhenLoaded(MapStore file)
    {
        ChangedKeys keys(file);
        keys.Load();
        return keys.Outdated();
    }
}

TEST(a_file_an_older_or_earlier_build_saved_is_owed_a_save_that_gives_it_the_version_and_the_marker)
{
    // An older build's save takes the version back, and an earlier build of the designer wrote no marker.
    MapStore olderBuild = SavedByThisBuild(), earlierBuild = SavedByThisBuild();
    olderBuild.values["settingsVersion"] = "2.0000";
    earlierBuild.values.erase("nameplateDesigns");
    {
        ChangedKeys keys(olderBuild);
        const Settings s = keys.Load();
        CHECK(keys.Outdated());
        CHECK((keys.Save(s) == std::vector<KeyText>{{"settingsVersion", "4.0000"}}));
    }
    {
        ChangedKeys keys(earlierBuild);
        const Settings s = keys.Load();
        CHECK(keys.Outdated());
        CHECK((keys.Save(s) == std::vector<KeyText>{{"nameplateDesigns", "true"}}));
    }
    CHECK(OutdatedWhenLoaded(Parse(kSavedFile))); // the release's file: migrated, then saved without waiting
    MapStore unversioned = Parse(kSavedFile);
    unversioned.values.erase("settingsVersion");
    CHECK(OutdatedWhenLoaded(unversioned));
    // Nothing is owed to this build's own file, to a newer build's, or where there is no file.
    CHECK(!OutdatedWhenLoaded(SavedByThisBuild()));
    MapStore newerBuild = SavedByThisBuild();
    newerBuild.values["settingsVersion"] = "5.0000";
    CHECK(!OutdatedWhenLoaded(newerBuild));
    CHECK(!OutdatedWhenLoaded(MapStore{}));
    // A value edited by hand waits for a save like any other.
    MapStore edited = SavedByThisBuild();
    edited.values["maxDistance"] = "99.0000";
    CHECK(!OutdatedWhenLoaded(edited));
}

TEST(the_stores_texts_are_asked_before_a_typed_read_can_add_a_default_to_it)
{
    // Ashita's typed getters add a key the file lacks to its memory with the default they were given. Asked after
    // them, the store would seem to hold nameplates=true, the file would not, and no save would ever write it.
    MapStore file = Parse(kSavedFile); // the release's file: no nameplates, outlines or baseSize
    file.addsDefaults = true;
    ChangedKeys keys(file);
    const Settings s = keys.Load();
    CHECK_EQ(file.values.at("nameplates"), "1"); // the stand-in did add it, as Ashita writes a switch
    std::map<std::string, std::string> passed;
    for (const KeyText& k : keys.Save(s))
        passed[k.key] = k.text;
    for (const char* added : {"nameplates", "outlines"})
        CHECK(passed.contains(added) && passed.at(added) == "true");
    CHECK(passed.contains("baseSize"));
}

TEST(keys_passed_on_are_owed_until_the_file_is_read_back_holding_them)
{
    MapStore file = SavedByThisBuild();
    ChangedKeys keys(file);
    Settings s = keys.Load();
    CHECK(!keys.Unwritten());
    s.nameRaise = 20;
    CHECK((keys.Save(s) == std::vector<KeyText>{{"nameRaise", "20.0000"}}));
    CHECK(keys.Unwritten());
    // The file does not hold it: the key stays owed, and the next save gives it again with nothing changed and sets
    // nothing more.
    keys.ReadBack(false);
    CHECK(keys.Unwritten());
    file.sets.clear();
    CHECK((keys.Save(s) == std::vector<KeyText>{{"nameRaise", "20.0000"}}));
    CHECK(file.sets.empty());
    // Another change joins it, and a newer text takes the place of the one that waited.
    keys.ReadBack(false);
    s.cursorHeight = 40;
    s.nameRaise    = 21;
    CHECK((keys.Save(s) == std::vector<KeyText>{{"nameRaise", "21.0000"}, {"cursorHeight", "40.0000"}}));
    CHECK(keys.Unwritten());
    keys.ReadBack(true);
    CHECK(!keys.Unwritten());
    CHECK(keys.Save(s).empty());
}

TEST(a_run_of_failed_saves_is_said_once_and_so_is_the_save_that_ends_it)
{
    MapStore file = SavedByThisBuild();
    ChangedKeys keys(file);
    Settings s = keys.Load();
    const auto change = [&] {
        ++s.nameRaise;
        keys.Save(s);
    };
    change();
    CHECK(keys.ReadBack(true) == SaveNotice::None); // the usual save says nothing
    change();
    CHECK(keys.ReadBack(false) == SaveNotice::Failed);
    change();
    CHECK(keys.ReadBack(false) == SaveNotice::None); // still failing: said already
    keys.Save(s);
    CHECK(keys.ReadBack(false) == SaveNotice::None);
    CHECK(keys.ReadBack(true) == SaveNotice::Saved); // the file has them after all
    CHECK(!keys.Unwritten());
    change();
    CHECK(keys.ReadBack(true) == SaveNotice::None);
    change();
    CHECK(keys.ReadBack(false) == SaveNotice::Failed); // a new run of failures is said again
    CHECK(keys.ReadBack(true) == SaveNotice::Saved);
}

TEST(reads_go_straight_through_to_the_store_behind)
{
    MapStore file = SavedByThisBuild();
    file.values["maxDistance"] = "99.0000"; // edited by hand: Load clamps what it returns, not what the store holds
    ChangedKeys keys(file);
    CHECK_EQ(keys.Load().maxDistance, kMaxOutlineDistance);
    CHECK_EQ(keys.GetFloat("maxDistance", 1.0f), 99.0f);
    CHECK(!keys.GetBool("enabled", true)); // kSavedFile's
    CHECK_EQ(keys.GetString("badgeFontName", "none"), "Tahoma");
    CHECK(keys.GetBool("missing", true) && !keys.GetBool("missing", false));
    CHECK_EQ(keys.GetFloat("missing", 2.5f), 2.5f);
    CHECK_EQ(keys.GetString("missing", "none"), "none");
    CHECK(file.sets.empty());
}

namespace
{
    // Designs no migration of kSavedFile's old keys would give.
    Settings WithOwnDesigns(Settings s)
    {
        s.Kind(PlateKind::Mobs).rows[CategoryIndex(Category::WontAttack)].plate = PlateMode::WhenTargeted;
        s.Kind(PlateKind::Mobs).lines[1].text      = "$name ($level)";
        s.Kind(PlateKind::Mobs).lines[2].font      = "Cinzel";
        s.Kind(PlateKind::Pets).lines[1].text      = "($owner)";
        s.Kind(PlateKind::Players).lines[2].glow   = 80;
        s.palette.aggression[static_cast<int>(Nature::Passive)] = {{0.25f, 0.5f, 0.75f}};
        return s;
    }
}

TEST(designs_survive_an_older_builds_save_which_takes_the_version_back)
{
    // The older build writes its own keys and version 2; Ashita keeps the designer's keys and the marker in the file.
    const Settings designed = WithOwnDesigns(MigratedSavedFile());
    MapStore file = Parse(kSavedFile);
    SaveSettings(designed, file);
    file.values["settingsVersion"] = "2.0000";
    file.values["fontName"]        = "Verdana"; // changed in the older build: not migrated a second time
    CHECK_EQ(file.values.at("nameplateDesigns"), "true");
    CHECK(LoadSettings(file) == designed);
}

TEST(a_version_2_file_without_the_marker_migrates)
{
    const Settings designed = WithOwnDesigns(MigratedSavedFile());
    MapStore file = Parse(kSavedFile);
    SaveSettings(designed, file);
    file.values["settingsVersion"] = "2.0000";
    file.values.erase("nameplateDesigns");
    CHECK(LoadSettings(file) == MigratedSavedFile()); // the old keys alone, the designer's ignored
    CHECK(!(LoadSettings(file) == designed));
}

TEST(a_version_3_file_from_before_the_marker_loads_its_designs)
{
    const Settings designed = WithOwnDesigns(Settings{});
    MapStore file;
    SaveSettings(designed, file);
    file.values.erase("nameplateDesigns");
    CHECK_EQ(file.values.at("settingsVersion"), "4.0000");
    CHECK(LoadSettings(file) == designed);
}

TEST(a_row_saved_as_expanding_shows_its_nameplate)
{
    // The Expanding mode is gone: its row drew a nameplate, whole for a target, so it is Shown.
    // And not the row's default, which a name no build knows falls back on: a pet row's is When Targeted.
    const auto yours = [](const Settings& s) { return s.Kind(PlateKind::Pets).rows[static_cast<int>(PetRow::Yours)].plate; };
    MapStore file;
    SaveSettings(Settings{}, file);
    CHECK(yours(LoadSettings(file)) == PlateMode::WhenTargeted);
    file.values["petsRowYourPetPlate"] = "expanding";
    CHECK(yours(LoadSettings(file)) == PlateMode::Shown);
    file.values["petsRowYourPetPlate"] = "nonsense";
    CHECK(yours(LoadSettings(file)) == PlateMode::WhenTargeted);
}

TEST(a_shadow_saved_before_its_unit_grew_keeps_its_look)
{
    // 100 percent is now one and a half of the shadow it was: 150 from before is 100, and 100 is 67, or 65 on the
    // steps of five.
    MapStore file = ReleaseFile({{"mobsLine3Shadow", "150.0000"}, {"mobsLine2Shadow", "100.0000"}});
    const Settings loaded = LoadSettings(file);
    CHECK_EQ(loaded.Kind(PlateKind::Mobs).lines[2].shadow, 100);
    CHECK_EQ(loaded.Kind(PlateKind::Mobs).lines[1].shadow, 65);
    // Such a file is owed a save, which gives it the new keys and leaves the old ones as they were.
    ChangedKeys keys(file);
    const Settings again = keys.Load();
    CHECK(keys.Outdated() && again == loaded);
    keys.Save(again);
    CHECK_EQ(file.values.at("mobsLine2ShadowStrength"), "65.0000");
    CHECK_EQ(file.values.at("mobsLine2Shadow"), "100.0000");
    CHECK(std::ranges::find(file.sets, "mobsLine2Shadow") == file.sets.end());
    CHECK_EQ(file.values.at("settingsVersion"), "4.0000");
    // The build before, run again, writes its own key: this build goes by its own.
    file.values["mobsLine2Shadow"] = "200.0000";
    CHECK_EQ(LoadSettings(file).Kind(PlateKind::Mobs).lines[1].shadow, 65);
    // A file with neither key has the default.
    file.values.erase("mobsLine2Shadow");
    file.values.erase("mobsLine2ShadowStrength");
    CHECK_EQ(LoadSettings(file).Kind(PlateKind::Mobs).lines[1].shadow, 100);
}

TEST(an_icon_size_saved_as_a_share_of_the_font_size_keeps_its_pixels)
{
    // A line's icons were a percent of the base font size, and are one of the base icon size, 16 px: 107 percent of a
    // 20 px font was 21.4 px, which is 134 percent of 16, and 135 on the steps of five.
    MapStore file = ReleaseFile({{"baseSize", "20.0000"}, {"mobsLine1IconSize", "107.0000"}, {"playersLine3IconSize", "50.0000"}});
    const Settings loaded = LoadSettings(file);
    CHECK_EQ(loaded.baseIconSize, 16);
    CHECK_EQ(loaded.Kind(PlateKind::Mobs).lines[0].iconSize, 135);
    CHECK_EQ(loaded.Kind(PlateKind::Players).lines[2].iconSize, 65); // 10 px
    // Saved under its own key, it is read back as it is, whatever the base sizes become.
    Settings changed    = loaded;
    changed.baseSize    = 30;
    changed.baseIconSize = 24;
    SaveSettings(changed, file);
    CHECK(std::ranges::find(file.sets, "mobsLine1IconSize") == file.sets.end());
    CHECK(LoadSettings(file) == changed);
}

TEST(an_ornament_saved_as_a_switch_and_two_lines_is_where_it_was)
{
    // Before, an ornament was on or off and sat between lines 1 and 2 or between 2 and 3.
    const auto place = [](const char* on, const char* lines) {
        MapStore file = ReleaseFile({{"npcsOrnamentOn", on}, {"npcsOrnamentPlace", lines}});
        return LoadSettings(file).Kind(PlateKind::Npcs).ornament.place;
    };
    CHECK(place("true", "lines12") == OrnamentPlace::AboveMiddle);
    CHECK(place("true", "lines23") == OrnamentPlace::BelowMiddle);
    CHECK(place("false", "lines12") == OrnamentPlace::Disabled);
    CHECK(place("false", "lines23") == OrnamentPlace::Disabled);
    // Its own key stands whatever the switch from before says: that key is never written again.
    Settings top;
    top.Kind(PlateKind::Npcs).ornament.place = OrnamentPlace::Top;
    MapStore file = OwnFileWith(top, {{"npcsOrnamentOn", "false"}, {"npcsOrnamentPlace", "lines23"}});
    CHECK(LoadSettings(file).Kind(PlateKind::Npcs).ornament.place == OrnamentPlace::Top);
    SaveSettings(top, file);
    CHECK(std::ranges::find(file.sets, "npcsOrnamentOn") == file.sets.end());
    CHECK(std::ranges::find(file.sets, "npcsOrnamentPlace") == file.sets.end());
}

TEST(a_number_of_before_is_converted_in_a_store_that_adds_what_a_typed_read_misses)
{
    // Ashita's typed getters add a key the file lacks with the default they were given. A number with a new key
    // read through one would then be held as that default, and the look of before under its old key never read.
    MapStore file = ReleaseFile({{"mobsLine3Shadow", "90.0000"}, {"mobsLine1IconSize", "30.0000"}, {"npcsOrnamentOn", "true"},
        {"npcsOrnamentPlace", "lines12"}, {"petsRowPetPlate", "hidden"}});
    const Settings loaded = LoadSettings(file);
    CHECK_EQ(loaded.Kind(PlateKind::Mobs).lines[2].shadow, 60); // 90 of the unit that was two thirds of today's
    CHECK_EQ(loaded.Kind(PlateKind::Mobs).lines[0].iconSize, 30); // 30 percent of a 15 px font is 4.5 px, 28 of 16
    CHECK(loaded.Kind(PlateKind::Npcs).ornament.place == OrnamentPlace::AboveMiddle);
    for (int row = 0; row < kPetRowCount; ++row)
        CHECK(loaded.Kind(PlateKind::Pets).rows[row].plate == PlateMode::Hidden);
}

TEST(a_cursors_edge_color_of_before_is_its_custom_color_and_its_own_color_follows_the_targeting_colors)
{
    // The edge was one color. It is a pick now, whose custom color is under the keys that color had.
    MapStore file = ReleaseFile({{"cursorEdgeColorR", "0.2500"}, {"cursorEdgeColorG", "0.5000"}, {"cursorEdgeColorB", "0.7500"}});
    const Settings loaded = LoadSettings(file);
    CHECK((loaded.cursorEdgeColor == ColorPick{ColorSource::Custom, {{0.25f, 0.5f, 0.75f}}}));
    CHECK(loaded.cursorColor.source == ColorSource::Target);
}

TEST(the_one_opacity_and_glow_switch_of_before_go_to_the_mobs_and_pets_they_were_for)
{
    // Other mobs' plates had one opacity while you had a target, pets among them, and one switch took the glow
    // off every line of a mob your party had claimed.
    MapStore file = ReleaseFile({{"untargetedOpacity", "40.0000"}, {"glowOffWhenFighting", "false"}});
    const Settings loaded = LoadSettings(file);
    CHECK_EQ(loaded.Kind(PlateKind::Mobs).opacity.notTargeted, 40);
    CHECK_EQ(loaded.Kind(PlateKind::Pets).opacity.notTargeted, 40);
    for (const PlateKind kind : {PlateKind::Players, PlateKind::Self, PlateKind::Npcs})
        CHECK_EQ(loaded.Kind(kind).opacity.notTargeted, 100);
    CHECK_EQ(loaded.Kind(PlateKind::Mobs).opacity.targeted, 100);
    for (const LineDesign& line : loaded.Kind(PlateKind::Mobs).lines)
        CHECK(!line.glowOffInCombat);
    // The switch as the release had it, on, takes the glow from the mobs' lines alone.
    MapStore asReleased     = ReleaseFile();
    const Settings released = LoadSettings(asReleased);
    for (int k = 0; k < kPlateKindCount; ++k)
        for (const LineDesign& line : released.kinds[k].lines)
            CHECK_EQ(line.glowOffInCombat, static_cast<PlateKind>(k) == PlateKind::Mobs);
    // A new install's mob lines lose their glow in combat, and no other kind's do.
    const Settings fresh;
    for (int k = 0; k < kPlateKindCount; ++k)
        for (const LineDesign& line : fresh.kinds[k].lines)
            CHECK_EQ(line.glowOffInCombat, static_cast<PlateKind>(k) == PlateKind::Mobs);
    // Keys of their own stand, and the two of before are never written again.
    Settings own;
    own.Kind(PlateKind::Mobs).opacity.notTargeted     = 75;
    own.Kind(PlateKind::Mobs).lines[0].glowOffInCombat = false;
    MapStore saved = OwnFileWith(own, {{"untargetedOpacity", "40.0000"}, {"glowOffWhenFighting", "true"}});
    CHECK(LoadSettings(saved) == own);
    SaveSettings(own, saved);
    CHECK(std::ranges::find(saved.sets, "untargetedOpacity") == saved.sets.end());
    CHECK(std::ranges::find(saved.sets, "glowOffWhenFighting") == saved.sets.end());
}

TEST(a_kinds_opacity_is_bounded_and_your_own_nameplate_never_fades_with_distance)
{
    Settings s;
    s.Kind(PlateKind::Npcs).opacity = OpacitySettings{0, 500, -3, true, 900.0f};
    s.Kind(PlateKind::Self).opacity.fadeIn = true;
    const Settings c = Clamp(s);
    CHECK((c.Kind(PlateKind::Npcs).opacity == OpacitySettings{kMinOpacity, kMaxOpacity, kMinOpacity, true, kMaxOutlineDistance}));
    CHECK(!c.Kind(PlateKind::Self).opacity.fadeIn);
    s.Kind(PlateKind::Npcs).opacity.range = 1.0f;
    CHECK_EQ(Clamp(s).Kind(PlateKind::Npcs).opacity.range, kMinOutlineDistance);
}

TEST(a_kinds_icon_limit_is_kept_for_the_kinds_that_show_status_icons_and_is_bounded)
{
    Settings s;
    s.Kind(PlateKind::Players).maxIcons = 3;
    s.Kind(PlateKind::Self).maxIcons    = 5;
    s.Kind(PlateKind::Mobs).maxIcons    = 2; // no page sets a mob's, so no file holds it
    MapStore store;
    SaveSettings(s, store);
    CHECK(!store.values.contains("mobsMaxIcons"));
    const Settings loaded = LoadSettings(store);
    CHECK_EQ(loaded.Kind(PlateKind::Players).maxIcons, 3);
    CHECK_EQ(loaded.Kind(PlateKind::Self).maxIcons, 5);
    CHECK_EQ(loaded.Kind(PlateKind::Mobs).maxIcons, kCountedIcons);
    s.Kind(PlateKind::Players).maxIcons = 0;
    s.Kind(PlateKind::Self).maxIcons    = 99;
    const Settings bounded = Clamp(s);
    CHECK_EQ(bounded.Kind(PlateKind::Players).maxIcons, 1);
    CHECK_EQ(bounded.Kind(PlateKind::Self).maxIcons, kCountedIcons);
}

TEST(the_cursors_two_switches_of_before_become_its_style)
{
    // The cursor was on or off, and a feather or an arrow.
    const auto style = [](const char* replace, const char* feather) {
        MapStore file = ReleaseFile({{"replaceCursor", replace}, {"cursorFeather", feather}});
        return LoadSettings(file).cursorStyle;
    };
    CHECK(style("true", "true") == CursorStyle::Feather);
    CHECK(style("true", "false") == CursorStyle::Triangle);
    CHECK(style("false", "true") == CursorStyle::Vanilla);
    CHECK(style("false", "false") == CursorStyle::Vanilla);
    // Its own key stands, and the switches are never written again.
    Settings chocobo;
    chocobo.cursorStyle = CursorStyle::Chocobo;
    MapStore file = OwnFileWith(chocobo, {{"replaceCursor", "false"}, {"cursorFeather", "false"}});
    CHECK(LoadSettings(file).cursorStyle == CursorStyle::Chocobo);
    SaveSettings(chocobo, file);
    CHECK(std::ranges::find(file.sets, "replaceCursor") == file.sets.end());
    CHECK(std::ranges::find(file.sets, "cursorFeather") == file.sets.end());
}

TEST(a_cursor_file_is_a_name_and_never_a_path)
{
    // The name goes into a path under the cursors folder: one that would lead out of it is no name.
    for (const char* path : {"..\\..\\system32\\x", "a/b", "c:evil", ""})
    {
        Settings s;
        s.cursorStyle = CursorStyle::File;
        s.cursorFile  = path;
        const Settings c = Clamp(s);
        CHECK(c.cursorStyle == CursorStyle::Feather && c.cursorFile.empty());
    }
    Settings named;
    named.cursorStyle = CursorStyle::File;
    named.cursorFile  = "My Cursor 2";
    CHECK(Clamp(named) == named);
    // The longest name a file may have is kept, and one letter more is no name.
    named.cursorFile = std::string(kMaxCursorFileName, 'n');
    CHECK(Clamp(named) == named);
    named.cursorFile += 'n';
    CHECK(Clamp(named).cursorStyle == CursorStyle::Feather);
    // A style no build knows is the default, and so is a dash mark.
    Settings wild;
    wild.cursorStyle = static_cast<CursorStyle>(9);
    wild.dashMark    = static_cast<DashMark>(9);
    CHECK(Clamp(wild).cursorStyle == CursorStyle::Feather);
    CHECK(Clamp(wild).dashMark == DashMark::Diamond);
    // A file from before dashes had marks draws them as diamonds, as a new install does.
    MapStore file = ReleaseFile();
    CHECK(LoadSettings(file).dashMark == DashMark::Diamond);
    file.values["dashMark"] = "no";
    CHECK(LoadSettings(file).dashMark == DashMark::None);
}

TEST(the_mouse_pointer_is_one_icon_and_the_chocobo_switch_of_before_picks_the_chocobo)
{
    const auto loaded = [](std::initializer_list<std::pair<const char*, const char*>> keys) {
        MapStore file = ReleaseFile(keys);
        return LoadSettings(file);
    };
    CHECK(loaded({{"chocoboPointer", "true"}}).pointerIcon == PointerIcon::Chocobo);
    CHECK(loaded({{"chocoboPointer", "false"}}).pointerIcon == PointerIcon::Vanilla);
    MapStore fresh;
    CHECK(LoadSettings(fresh).pointerIcon == PointerIcon::Vanilla);
    // The key of its own wins over the switch of before.
    const Settings file = loaded({{"chocoboPointer", "true"}, {"pointerIcon", "file"}, {"pointerFile", "sword"}});
    CHECK(file.pointerIcon == PointerIcon::File && file.pointerFile == "sword");
    CHECK(loaded({{"chocoboPointer", "true"}, {"pointerIcon", "vanilla"}}).pointerIcon == PointerIcon::Vanilla);
    // A file that cannot be in the folder is no pick: the game's own pointer.
    Settings wild;
    wild.pointerIcon = PointerIcon::File;
    wild.pointerFile = "..\\outside";
    CHECK(Clamp(wild).pointerIcon == PointerIcon::Vanilla && Clamp(wild).pointerFile.empty());
    wild.pointerIcon = static_cast<PointerIcon>(9);
    CHECK(Clamp(wild).pointerIcon == PointerIcon::Vanilla);
    // The switch of before is never written again.
    Settings chocobo;
    chocobo.pointerIcon = PointerIcon::Chocobo;
    MapStore saved      = OwnFileWith(chocobo, {{"chocoboPointer", "false"}});
    CHECK(LoadSettings(saved) == chocobo);
    SaveSettings(chocobo, saved);
    CHECK(std::ranges::find(saved.sets, "chocoboPointer") == saved.sets.end());
    CHECK_EQ(saved.values.at("pointerIcon"), "chocobo");
}

TEST(a_cursor_saved_as_one_size_keeps_its_shape_and_its_edge)
{
    // The cursor had a height and took its width from its shape. It has both now, and its edge's 100 percent is
    // what 50 was.
    const auto loaded = [](const char* feather) {
        MapStore file = ReleaseFile({{"cursorSize", "31.0000"}, {"cursorFeather", feather}, {"cursorOutline", "75.0000"}});
        return LoadSettings(file);
    };
    CHECK_EQ(loaded("true").cursorHeight, 31);
    CHECK_EQ(loaded("true").cursorWidth, 14);  // the feather is 2.155 times as tall as wide
    CHECK_EQ(loaded("false").cursorWidth, 39); // and the arrow 0.8
    CHECK_EQ(loaded("true").cursorOutlineThickness, 150);
    // The keys of its own stand, and the size and edge of before are never written again.
    Settings wide;
    wide.cursorWidth = 40;
    MapStore file = OwnFileWith(wide, {{"cursorSize", "31.0000"}, {"cursorOutline", "75.0000"}});
    CHECK(LoadSettings(file) == wide);
    SaveSettings(wide, file);
    CHECK(std::ranges::find(file.sets, "cursorSize") == file.sets.end());
    CHECK(std::ranges::find(file.sets, "cursorOutline") == file.sets.end());
}

TEST(an_outlines_thickness_in_pixels_is_left_behind_for_the_percent)
{
    // The thickness was pixels, one by default, and is a percent of four pixels now. What a file held in pixels is
    // not carried over: every file starts at 100 percent, and the pixels are never read or written again.
    for (const char* pixels : {"1.0000", "6.5000", "16.0000"})
    {
        MapStore file = ReleaseFile({{"thickness", pixels}});
        CHECK_EQ(LoadSettings(file).outlineThickness, 100);
    }
    Settings thick;
    thick.outlineThickness = 250;
    MapStore file = OwnFileWith(thick, {{"thickness", "1.0000"}});
    CHECK_EQ(LoadSettings(file).outlineThickness, 250);
    SaveSettings(thick, file);
    CHECK(std::ranges::find(file.sets, "thickness") == file.sets.end());
}

TEST(a_switch_reads_as_headsup_or_ashita_wrote_it)
{
    // HeadsUp writes true and false. Ashita, adding a switch a file lacked, writes 1 and 0.
    MapStore file = ReleaseFile({{"mobsLine3Bold", "1"}, {"mobsLine2Bold", "0"}, {"npcsOrnamentOn", "1"},
        {"npcsOrnamentPlace", "lines12"}, {"mobsOrnamentOn", "0"}, {"glowOffWhenFighting", "0"}});
    const Settings loaded = LoadSettings(file);
    CHECK_EQ(loaded.Kind(PlateKind::Mobs).lines[2].style, StyleBit(FontStyle::Bold));
    CHECK_EQ(loaded.Kind(PlateKind::Mobs).lines[1].style, 0);
    CHECK(loaded.Kind(PlateKind::Npcs).ornament.place == OrnamentPlace::AboveMiddle);
    CHECK(loaded.Kind(PlateKind::Mobs).ornament.place == OrnamentPlace::Disabled);
    for (const LineDesign& line : loaded.Kind(PlateKind::Mobs).lines)
        CHECK(!line.glowOffInCombat);
}

TEST(a_kinds_lines_and_ornament_are_bounded_and_a_condition_its_page_does_not_offer_is_dropped)
{
    Settings s;
    LineDesign& pets   = s.Kind(PlateKind::Pets).lines[0];
    pets.padding       = kMaxLinePadding + 5;
    pets.show.when     = ConditionBit(ShowCondition::OwnerIsYou) | ConditionBit(ShowCondition::TooWeak);
    LineDesign& mobs   = s.Kind(PlateKind::Mobs).lines[0];
    mobs.padding       = -4;
    mobs.show.when     = ConditionBit(ShowCondition::OwnerIsYou) | ConditionBit(ShowCondition::TooWeak);
    s.Kind(PlateKind::Npcs).ornament.place = static_cast<OrnamentPlace>(42);
    const Settings bounded = Clamp(s);
    CHECK_EQ(bounded.Kind(PlateKind::Pets).lines[0].padding, kMaxLinePadding);
    CHECK_EQ(bounded.Kind(PlateKind::Mobs).lines[0].padding, 0);
    // A pet's page asks whose it is and a mob's whether it is too weak, and neither asks the other's.
    CHECK_EQ(bounded.Kind(PlateKind::Pets).lines[0].show.when, ConditionBit(ShowCondition::OwnerIsYou));
    CHECK_EQ(bounded.Kind(PlateKind::Mobs).lines[0].show.when, ConditionBit(ShowCondition::TooWeak));
    CHECK(bounded.Kind(PlateKind::Npcs).ornament.place == Settings{}.Kind(PlateKind::Npcs).ornament.place);
    // The base icon size has the font size's bounds.
    s.baseIconSize = kMaxTextSize + 30;
    CHECK_EQ(Clamp(s).baseIconSize, kMaxTextSize);
    s.baseIconSize = 0;
    CHECK_EQ(Clamp(s).baseIconSize, kMinTextSize);
}

TEST(a_file_from_before_dead_mobs_had_a_row_gives_that_row_what_every_mob_row_had)
{
    // The release had no row for dead mobs: a dead one went by its own row. A player who had every mob on the
    // game's own names, or on none, must not find HeadsUp's nameplate on corpses alone.
    const auto all = [](const char* mode) {
        return ReleaseFile({{"mobsRowAggressivePlate", mode}, {"mobsRowPassivePlate", mode}, {"mobsRowNoDataPlate", mode},
            {"mobsRowAggressiveNmPlate", mode}, {"mobsRowPassiveNmPlate", mode}, {"mobsRowPlaceholderPlate", mode},
            {"mobsRowPartyClaimPlate", mode}, {"mobsRowOthersClaimPlate", mode}});
    };
    const auto dead = [](MapStore file) { return LoadSettings(file).Kind(PlateKind::Mobs).rows[static_cast<int>(Category::Dead)]; };
    CHECK(dead(all("vanilla")).plate == PlateMode::Vanilla);
    CHECK(dead(all("hidden")).plate == PlateMode::Hidden);
    CHECK(dead(all("whenTargeted")).plate == PlateMode::WhenTargeted);
    CHECK(dead(all("vanilla")).outline == OutlineMode::Disabled); // nothing dead was outlined
    // Rows that differ say nothing for the dead: they show, as on a new install.
    MapStore mixed = all("vanilla");
    mixed.values["mobsRowPassivePlate"] = "hidden";
    CHECK(dead(mixed).plate == PlateMode::Shown);
    // The row's own key stands, and a save writes it.
    MapStore own = all("vanilla");
    own.values["mobsRowDeadPlate"] = "shown";
    CHECK(dead(own).plate == PlateMode::Shown);
    MapStore saved = all("hidden");
    SaveSettings(LoadSettings(saved), saved);
    CHECK_EQ(saved.values.at("mobsRowDeadPlate"), "hidden");
    saved.values["mobsRowAggressivePlate"] = "shown";
    CHECK(dead(saved).plate == PlateMode::Hidden);
}

TEST(the_one_pet_row_of_before_becomes_all_four)
{
    // Pets had one row. A file from then gives what that row held to each of the four, and saves them as four.
    MapStore file = ReleaseFile({{"petsRowPetPlate", "hidden"}, {"petsRowPetOutline", "shown"}, {"petsRowPetColorSource", "custom"},
        {"petsRowPetColorR", "0.2500"}, {"petsRowPetColorG", "0.5000"}, {"petsRowPetColorB", "0.7500"}});
    const Settings loaded = LoadSettings(file);
    for (int row = 0; row < kPetRowCount; ++row)
    {
        const RowSettings& pets = loaded.Kind(PlateKind::Pets).rows[row];
        CHECK(pets.plate == PlateMode::Hidden && pets.outline == OutlineMode::Shown);
    }
    const RowSettings& yours = loaded.Kind(PlateKind::Pets).rows[static_cast<int>(PetRow::Yours)];
    CHECK((yours.outlineColor == ColorPick{ColorSource::Custom, {{0.25f, 0.5f, 0.75f}}}));
    // A row with keys of its own goes by them, whatever the one row of before says.
    Settings changed = loaded;
    changed.Kind(PlateKind::Pets).rows[static_cast<int>(PetRow::Party)].plate = PlateMode::Shown;
    SaveSettings(changed, file);
    CHECK(std::ranges::find(file.sets, "petsRowPetPlate") == file.sets.end());
    CHECK(LoadSettings(file) == changed);
    // A new install's four rows show a pet's nameplate when it is targeted, and never outline it.
    const Settings fresh;
    for (int row = 0; row < kPetRowCount; ++row)
    {
        const RowSettings& pets = fresh.Kind(PlateKind::Pets).rows[row];
        CHECK(pets.plate == PlateMode::WhenTargeted && pets.outline == OutlineMode::Disabled);
    }
}

TEST(a_line_saved_as_bold_is_bold_among_its_styles)
{
    // Bold was a switch of its own before a line had styles.
    MapStore file = ReleaseFile({{"mobsLine3Bold", "true"}, {"mobsLine2Bold", "false"}});
    const Settings loaded = LoadSettings(file);
    CHECK_EQ(loaded.Kind(PlateKind::Mobs).lines[2].style, StyleBit(FontStyle::Bold));
    CHECK_EQ(loaded.Kind(PlateKind::Mobs).lines[1].style, 0);
    // Its styles are saved under their own key, which then stands whatever the switch says.
    Settings styled = loaded;
    styled.Kind(PlateKind::Mobs).lines[2].style = StyleBit(FontStyle::Italic) | StyleBit(FontStyle::Underline);
    SaveSettings(styled, file);
    CHECK(std::ranges::find(file.sets, "mobsLine3Bold") == file.sets.end());
    CHECK_EQ(LoadSettings(file).Kind(PlateKind::Mobs).lines[2].style, StyleBit(FontStyle::Italic) | StyleBit(FontStyle::Underline));
    // A style no build knows is dropped.
    file.values["mobsLine3Style"] = "FF";
    CHECK_EQ(LoadSettings(file).Kind(PlateKind::Mobs).lines[2].style, kAllFontStyles);
}

TEST(the_ornament_switch_of_a_version_2_file_puts_it_between_the_level_line_and_the_name)
{
    CHECK(Load2({{"showOrnament", "true"}}).Kind(PlateKind::Mobs).ornament.place == OrnamentPlace::BelowMiddle);
    CHECK(Load2({{"showOrnament", "false"}}).Kind(PlateKind::Mobs).ornament.place == OrnamentPlace::Disabled);
}

TEST(pets_migrate_as_the_mobs_they_were_drawn_as)
{
    const auto petRow = [](const Settings& s) { return s.Kind(PlateKind::Pets).rows[0].plate; };
    CHECK(petRow(Load2({{"replaceMobNames", "false"}})) == PlateMode::Vanilla);
    CHECK(petRow(Load2({{"replaceMobNames", "true"}})) == PlateMode::Shown);
    CHECK(petRow(Load2({{"replaceNpcNames", "false"}, {"replacePlayerNames", "false"}})) == PlateMode::Shown);
    // The mob name's font and bold, whatever players and NPCs had.
    const Settings fonts = Load2({{"fontName", "Georgia"}, {"fontBold", "true"}, {"playerFontName", "Verdana"},
        {"playerFontBold", "false"}, {"npcFontName", "Cinzel"}, {"npcFontBold", "false"}});
    const LineDesign& petName = fonts.Kind(PlateKind::Pets).lines[2];
    CHECK_EQ(petName.font, "Georgia");
    CHECK_EQ(petName.style, StyleBit(FontStyle::Bold));
    CHECK(fonts.Kind(PlateKind::Pets).lines[0].font.empty() && fonts.Kind(PlateKind::Pets).lines[0].style == 0);
    // But for its shadows, which were of the unit before, such a file's pets are today's.
    // And it shows their nameplates, as it showed any mob's.
    const Settings plain   = Load2({});
    KindSettings expected  = Settings{}.Kind(PlateKind::Pets);
    for (LineDesign& line : expected.lines)
        line.shadow = 65; // 67, on the steps of five
    for (int row = 0; row < kPetRowCount; ++row)
        expected.rows[row].plate = PlateMode::Shown;
    CHECK(plain.Kind(PlateKind::Pets) == expected);
}

TEST(a_line_too_long_is_cut_between_characters)
{
    const std::string e = "\xC3\xA9"; // e with an acute accent: two bytes
    Settings s;
    LineDesign* lines = s.Kind(PlateKind::Mobs).lines;
    lines[0].text = std::string(kMaxTemplate - 1, 'x') + e + "yy"; // the letter's second byte is the first one over
    lines[1].text = std::string(kMaxTemplate - 2, 'x') + e + "yy"; // the letter ends at the limit
    lines[2].text = std::string(kMaxTemplate - 2, 'x') + "\xE2\x82\xAC" + "y"; // the euro sign's three bytes, one over
    const Settings c = Clamp(s);
    CHECK_EQ(c.Kind(PlateKind::Mobs).lines[0].text, std::string(kMaxTemplate - 1, 'x'));
    CHECK_EQ(c.Kind(PlateKind::Mobs).lines[1].text, std::string(kMaxTemplate - 2, 'x') + e);
    CHECK_EQ(c.Kind(PlateKind::Mobs).lines[2].text, std::string(kMaxTemplate - 2, 'x'));
}

TEST(a_lines_conditions_are_the_ones_its_kind_offers)
{
    Settings s;
    s.Kind(PlateKind::Pets).lines[2].show = LineShow{false, ConditionBit(ShowCondition::Unclaimed)};
    s.Kind(PlateKind::Pets).lines[1].show =
        LineShow{false, ConditionBit(ShowCondition::Targeted) | ConditionBit(ShowCondition::Engaged)};
    s.Kind(PlateKind::Players).lines[2].show =
        LineShow{false, ConditionBit(ShowCondition::Friend) | ConditionBit(ShowCondition::Aggressive)};
    s.Kind(PlateKind::Mobs).lines[2].show =
        LineShow{false, ConditionBit(ShowCondition::Passive) | ConditionBit(ShowCondition::PartyMember) | (1u << 20)};
    const Settings c = Clamp(s);
    CHECK(c.Kind(PlateKind::Pets).lines[2].show == (LineShow{false, 0}));
    CHECK(c.Kind(PlateKind::Pets).lines[1].show == (LineShow{false, ConditionBit(ShowCondition::Targeted)}));
    CHECK(c.Kind(PlateKind::Players).lines[2].show == (LineShow{false, ConditionBit(ShowCondition::Friend)}));
    CHECK(c.Kind(PlateKind::Mobs).lines[2].show == (LineShow{false, ConditionBit(ShowCondition::Passive)}));
    // Every kind's lines offer Generally, and it is saved with the rest.
    for (int kind = 0; kind < kPlateKindCount; ++kind)
        s.Kind(static_cast<PlateKind>(kind)).lines[0].show = LineShow{false, ConditionBit(ShowCondition::Generally)};
    MapStore store;
    SaveSettings(Clamp(s), store);
    const Settings loaded = LoadSettings(store);
    for (int kind = 0; kind < kPlateKindCount; ++kind)
        CHECK(loaded.Kind(static_cast<PlateKind>(kind)).lines[0].show == (LineShow{false, ConditionBit(ShowCondition::Generally)}));
}

TEST(a_font_name_of_31_characters_is_kept_and_one_of_32_refused)
{
    // A Windows font name's room, LOGFONT's 32 characters, ends in a terminator.
    const std::string fits(31, 'f'), over(32, 'f');
    Settings s;
    s.Kind(PlateKind::Mobs).lines[0].font = fits;
    s.Kind(PlateKind::Mobs).lines[1].font = over;
    s.defaultFont = s.badgeFontName = s.barTitleFont = s.barTextFont = fits;
    const Settings kept = Clamp(s);
    CHECK_EQ(kept.Kind(PlateKind::Mobs).lines[0].font, fits);
    CHECK(kept.Kind(PlateKind::Mobs).lines[1].font.empty()); // the default font
    CHECK(kept.defaultFont == fits && kept.badgeFontName == fits && kept.barTitleFont == fits && kept.barTextFont == fits);
    s.defaultFont = s.badgeFontName = s.barTitleFont = s.barTextFont = over;
    const Settings refused = Clamp(s), d;
    CHECK(refused.defaultFont == d.defaultFont && refused.badgeFontName == d.badgeFontName);
    CHECK(refused.barTitleFont == d.barTitleFont && refused.barTextFont == d.barTextFont);
}
