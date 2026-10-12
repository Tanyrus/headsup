#include "pointer_keys.h"
#include "settings.h"
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

        bool GetBool(const char* key, bool fallback) override
        {
            const auto it = values.find(key);
            return it == values.end() ? fallback : it->second == "true";
        }
        float GetFloat(const char* key, float fallback) override
        {
            const auto it = values.find(key);
            return it == values.end() ? fallback : std::strtof(it->second.c_str(), nullptr);
        }
        std::string GetString(const char* key, const char* fallback) override
        {
            const auto it = values.find(key);
            return it == values.end() ? fallback : it->second;
        }
        void Set(const char* key, const char* value) override { values[key] = value; }
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
barConquest=true
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
barConquestLine2=true
barConquestIcons=true
barSignetLine2=true
barSignetIcons=true
barDistanceLine2=true
barDistanceIcons=true
expBar=beside
jobNames=false
clock24=false
stackNames=true
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
barLeft=zone weather vanaTime job party conquest signet serverTime distance
barCenter=
barRight=target gil bags connection fps earthTime
hideHelpBar=true
hideNetstat=true
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
        s.enabled = s.showLabels = s.replaceMobNames = s.showIcons = false;
        s.hideClaimedByParty = s.hideClaimed = s.hideTooWeak = s.hideWhileEngaged = true;
        s.markPlaceholders  = false;
        s.phTimers          = true;
        s.scaleWithDistance = false;
        s.scaleCursor       = true; // apart from the names': a dropped key would take theirs
        s.fontBold          = true;
        s.replacePlayerNames = s.replaceNpcNames = s.replaceCursor = s.cursorFeather = false;
        s.chocoboPointer = true;
        s.fixPointer     = false;
        s.showPlayerIcons = s.centerNameAndIcons = false;
        s.ownNameColor   = true;
        s.smoothness     = 12;
        s.mobNameSize    = 12;
        s.playerNameSize = 13;
        s.selfNameSize   = 17;
        s.npcNameSize    = 16;
        s.labelSize      = 14;
        s.timerSize      = 17;
        s.iconSize       = 24;
        s.cursorSize     = 31;
        s.nameRaise      = 12;
        s.playerIconSize = 120;
        s.glowStrength   = 150;
        s.nameGlow       = false;
        s.glowOffWhenFighting = false;
        s.scaleOwnName = s.scalePlayerNames = false;
        s.labelFontBold    = false; // the name's is on: a dropped key would take it
        s.showOrnament     = false;
        s.ownGlowColor     = true;
        s.ownOrnamentColor = true;
        s.labelFontName    = "Cinzel";
        s.glowSize         = 150;
        s.ornamentWidth    = 120;
        s.ornamentThickness = 9;
        s.nameShadow       = 50;
        s.labelShadow      = 150;
        s.glowColor        = Color{{0.1f, 0.2f, 0.3f}};
        s.ornamentColor    = Color{{0.4f, 0.5f, 0.6f}};
        s.labelShadowColor = Color{{0.7f, 0.8f, 0.9f}};
        s.thickness      = 6.5f;
        s.maxDistance    = 25.0f;
        s.cursorColor           = Color{{0.77f, 0.32f, 0.31f}};
        s.subCursorColor        = Color{{0.5f, 0.25f, 0.75f}};
        s.outOfRangeCursorColor = Color{{0.75f, 0.12f, 0.25f}};
        s.lockedCursorColor     = Color{{0.12f, 0.25f, 0.5f}};
        s.nameColor             = Color{{0.5f, 0.25f, 0.12f}};
        s.textOutline           = Color{{0.25f, 0.24f, 0.5f}};
        s.iconTint              = Color{{0.75f, 0.5f, 0.9f}};
        s.fontName              = "Georgia";
        struct Shade
        {
            LabelShade shade;
            Color color;
        };
        for (const Shade& l : {Shade{LabelShade::Unknown, {{0.1f, 0.11f, 0.12f}}}, Shade{LabelShade::TooWeak, {{0.2f, 0.21f, 0.22f}}},
                 Shade{LabelShade::EasyPrey, {{0.3f, 0.31f, 0.32f}}}, Shade{LabelShade::DecentChallenge, {{0.4f, 0.41f, 0.42f}}},
                 Shade{LabelShade::EvenMatch, {{0.5f, 0.51f, 0.52f}}}, Shade{LabelShade::Tough, {{0.6f, 0.61f, 0.62f}}},
                 Shade{LabelShade::VeryTough, {{0.7f, 0.71f, 0.72f}}}})
            s.labelColor[static_cast<int>(l.shade)] = l.color;
        struct Outline
        {
            Category category;
            bool show;
            Color color;
        };
        for (const Outline& o : {Outline{Category::WillAttack, false, {{0.13f, 0.14f, 0.15f}}},
                 Outline{Category::WontAttack, true, {{0.23f, 0.24f, 0.25f}}}, Outline{Category::Unknown, true, {{0.33f, 0.34f, 0.35f}}},
                 Outline{Category::NmWillAttack, false, {{0.43f, 0.44f, 0.45f}}},
                 Outline{Category::NmWontAttack, false, {{0.53f, 0.54f, 0.55f}}},
                 Outline{Category::Placeholder, false, {{0.63f, 0.64f, 0.65f}}},
                 Outline{Category::PartyClaim, true, {{0.73f, 0.74f, 0.75f}}},
                 Outline{Category::OtherClaim, true, {{0.83f, 0.84f, 0.85f}}}})
        {
            s.show[CategoryIndex(o.category)]  = o.show;
            s.color[CategoryIndex(o.category)] = o.color;
        }
        for (const PlayerIcon icon : {PlayerIcon::Gm, PlayerIcon::NewAdventurer, PlayerIcon::Away, PlayerIcon::Bazaar})
            s.playerIconSide[static_cast<int>(icon)] = IconSide::Right;
        for (const PlayerIcon icon : {PlayerIcon::Mentor, PlayerIcon::LevelSync, PlayerIcon::Linkshell, PlayerIcon::SeekingParty})
            s.playerIconSide[static_cast<int>(icon)] = IconSide::Hidden;
        s.mobId = MobIdFormat::LastThree;
        s.look  = Look::Fantasy;
        s.keepPointerKeys = {0x11, 0x1B, 0x51};
        s.showDamageIcons     = true;
        s.showStatusRankIcons = true;
        s.showImmunityIcons   = true;
        s.detectionIconSide   = IconSide::Right;
        s.badgeSide           = IconSide::Left;
        s.elementWeakRank     = -3;
        s.elementResistRank   = 6;
        s.maxElements         = 3;
        s.showRanks           = true;
        s.badgeFontName       = "Tahoma";
        s.showBar      = false;
        s.barTheme     = BarTheme::UmbraDark;
        s.barSeparators = BarSeparators::None;
        s.barCurrency   = *CurrencyOfKey("allied_notes");
        s.serverClockZone = ClockZone::UsPacific;
        s.barTargetDetails = true;
        s.menuFont         = "Tahoma";
        s.menuScale        = 125;
        s.windowOffsets    = {WindowOffset{"equip", 10, -20}};
        s.windowGroups     = {WindowGroup{"Bags", 0, 30, true, {"inventor", "iteminfo"}}};
        s.allUnderBar      = true;
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
        s.barTitleFont = "Cinzel";
        s.barSubtitle = SubtitlePlace::Before;
        s.playerIconOrder = {PlayerIcon::SeekingParty, PlayerIcon::Gm, PlayerIcon::Mentor, PlayerIcon::NewAdventurer,
            PlayerIcon::LevelSync, PlayerIcon::Away, PlayerIcon::Linkshell, PlayerIcon::Bazaar};
        s.playerIconShare[static_cast<int>(PlayerIcon::Bazaar)] = true;
        s.playerIconMax = 4;
        s.playerFontName = "Verdana";
        s.playerFontBold = false;
        s.npcFontName    = "Cinzel";
        s.npcFontBold    = true;
        s.mobIconPlace = IconPlace::Inline;
        s.cursorOutline = 60;
        s.untargetedOpacity = 40;
        s.clockSeconds  = true;
        s.serverSeconds = true;
        s.barLatency = true;
        s.barHeight    = 48;
        s.barTextSize   = 16;
        s.popupTextSize = 20;
        s.hideHelpBar  = true;
        s.hideNetstat  = true;
        s.movedWindows = {"equip", "levmerit"};
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

    MapStore Parse(const char* file)
    {
        MapStore store;
        std::istringstream lines(file);
        for (std::string line; std::getline(lines, line);)
            if (const size_t equals = line.find('='); equals != std::string::npos)
                store.values[line.substr(0, equals)] = line.substr(equals + 1);
        return store;
    }
}

TEST(players_and_npcs_take_the_mob_name_font_from_a_file_saved_before_they_had_their_own)
{
    MapStore old;
    old.values = {{"settingsVersion", "2.0000"}, {"fontName", "Georgia"}, {"fontBold", "true"}};
    const Settings s = LoadSettings(old);
    CHECK(s.playerFontName == "Georgia" && s.npcFontName == "Georgia" && s.playerFontBold && s.npcFontBold);
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
    expected.friends          = loaded.friends;
    expected.cursorEdgeColor  = loaded.cursorEdgeColor;
    CHECK(loaded == expected);
    MapStore saved;
    SaveSettings(loaded, saved);
    for (const auto& [key, value] : file.values)
        if (key != "settingsVersion") CHECK_EQ(saved.values[key], value);
    CHECK(LoadSettings(saved) == loaded);
}

TEST(a_player_icon_side_that_is_not_a_side_keeps_its_default)
{
    MapStore store;
    store.values["iconGm"] = "middle";
    CHECK(LoadSettings(store).playerIconSide[static_cast<int>(PlayerIcon::Gm)] == IconSide::Left);
}

TEST(a_group_side_that_is_not_left_or_right_keeps_its_default)
{
    MapStore store;
    store.values["detectionIconSide"] = "hide";
    store.values["badgeSide"]         = "middle";
    const Settings s                  = LoadSettings(store);
    CHECK(s.detectionIconSide == IconSide::Left && s.badgeSide == IconSide::Right);
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
    store.values["nameSize"]   = "1e10";
    store.values["labelSize"]  = "-1e10";
    store.values["cursorSize"] = "3e9";
    const Settings s = LoadSettings(store);
    CHECK_EQ(s.mobNameSize, kMaxTextSize);
    CHECK_EQ(s.labelSize, kMinTextSize);
    CHECK_EQ(s.cursorSize, kMaxTextSize);
}

TEST(a_font_name_windows_cannot_use_falls_back_to_the_default)
{
    MapStore store;
    store.values["fontName"] = "";
    CHECK(LoadSettings(store).fontName == kDefaultFont);
    store.values["fontName"] = std::string(kMaxFontName + 1, 'x');
    CHECK(LoadSettings(store).fontName == kDefaultFont);
    store.values["fontName"] = std::string(kMaxFontName, 'x');
    CHECK(LoadSettings(store).fontName == std::string(kMaxFontName, 'x'));
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
    s.thickness   = 0.0f;
    s.smoothness  = 99;
    s.maxDistance = 1000.0f;
    s.fontName    = "";
    s.mobNameSize = s.playerNameSize = 100;
    s.selfNameSize = s.npcNameSize = 0;
    s.labelSize   = 0;
    s.timerSize   = 0;
    s.iconSize    = 49;
    s.playerIconSize = 10;
    s.glowStrength   = 1000;
    s.glowSize       = 1000;
    s.ornamentWidth  = 0;
    s.ornamentThickness = 1000;
    s.nameShadow     = -5;
    s.labelShadow    = 1000;
    s.labelFontName  = "";
    s.cursorSize  = 2;
    s.nameRaise   = 99;
    s.elementWeakRank   = 5;
    s.elementResistRank = -9;
    s.maxElements       = 99;
    s.statusBadges[2]   = StatusBadgeRule{true, 3, 40};
    s.statusBadges[7]   = StatusBadgeRule{true, -40, -3};
    s.detectionIconSide = s.badgeSide = IconSide::Hidden;
    s.badgeFontName     = "";
    s.barHeight         = 200;
    s.barTheme          = static_cast<BarTheme>(9);
    s.barCurrency       = kCurrencyCount;
    const Color wild{{2.0f, -1.0f, 0.5f}};
    s.nameColor = s.textOutline = s.iconTint = s.cursorColor = s.subCursorColor = s.lockedCursorColor = wild;
    s.glowColor = s.ornamentColor = s.labelShadowColor = wild;
    for (Color& c : s.labelColor)
        c = wild;
    for (Color& c : s.color)
        c = wild;
    const Settings c = Clamp(s);
    const Color clamped{{1.0f, 0.0f, 0.5f}};
    for (const Color& each : {c.nameColor, c.textOutline, c.iconTint, c.cursorColor, c.subCursorColor, c.lockedCursorColor,
             c.glowColor, c.ornamentColor, c.labelShadowColor})
        CHECK(each == clamped);
    for (const Color& each : c.labelColor)
        CHECK(each == clamped);
    for (const Color& each : c.color)
        CHECK(each == clamped);
    CHECK_EQ(c.thickness, kMinThickness);
    CHECK_EQ(c.smoothness, kMaxSmoothness);
    CHECK_EQ(c.playerIconSize, kMinPlayerIconSize);
    CHECK_EQ(c.glowStrength, kMaxGlowStrength);
    CHECK_EQ(c.glowSize, kMaxGlowSize);
    CHECK_EQ(c.ornamentWidth, kMinOrnamentWidth);
    CHECK_EQ(c.ornamentThickness, kMaxOrnamentThickness);
    CHECK_EQ(c.nameShadow, kMinShadowStrength);
    CHECK_EQ(c.labelShadow, kMaxShadowStrength);
    CHECK(c.labelFontName == kDefaultFont);
    CHECK_EQ(c.maxDistance, kMaxOutlineDistance);
    CHECK(c.fontName == kDefaultFont);
    CHECK(c.mobNameSize == kMaxTextSize && c.playerNameSize == kMaxTextSize);
    CHECK(c.selfNameSize == kMinTextSize && c.npcNameSize == kMinTextSize);
    CHECK_EQ(c.labelSize, kMinTextSize);
    CHECK_EQ(c.timerSize, kMinTextSize);
    CHECK_EQ(c.iconSize, kMaxTextSize);
    CHECK_EQ(c.cursorSize, kMinTextSize);
    CHECK_EQ(c.nameRaise, kMaxNameRaise);
    CHECK(c.elementWeakRank == -1 && c.elementResistRank == 1 && c.maxElements == kElementCount);
    CHECK(c.statusBadges[2] == (StatusBadgeRule{true, -1, kNoResistRank}));
    CHECK(c.statusBadges[7] == (StatusBadgeRule{true, kNoWeakRank, 1}));
    CHECK(c.detectionIconSide == IconSide::Left && c.badgeSide == IconSide::Right); // a group cannot hide
    CHECK(c.badgeFontName == kBadgeFont);
    CHECK_EQ(c.barHeight, kMaxBarHeight);
    CHECK(c.barTheme == BarTheme::PhoenixGold && c.barCurrency == kGilCurrency);
}

TEST(non_finite_values_fall_back_to_defaults)
{
    MapStore store;
    store.values["thickness"]   = "nan";
    store.values["smoothness"]  = "nan";
    store.values["maxDistance"] = "inf";
    store.values["iconSize"]    = "nan";
    store.values["unknownG"]    = "nan";
    CHECK(LoadSettings(store) == Settings{});
}

TEST(argb_packing)
{
    CHECK_EQ(ToArgb(Color{{1.0f, 0.0f, 0.0f}}), 0xFFFF0000u);
    CHECK_EQ(ToArgb(Color{{0.2f, 1.0f, 0.3f}}), 0xFF33FF4Du);
}

TEST(any_replaced_kind_of_name_counts)
{
    Settings s;
    s.replaceMobNames = s.replacePlayerNames = s.replaceNpcNames = false;
    CHECK(!ReplacesAnyName(s));
    for (bool Settings::*kind : {&Settings::replaceMobNames, &Settings::replacePlayerNames, &Settings::replaceNpcNames})
    {
        Settings one = s;
        one.*kind    = true;
        CHECK(ReplacesAnyName(one));
        one.enabled = false;
        CHECK(!ReplacesAnyName(one));
    }
}

TEST(nameplates_need_the_master_switch_and_a_part)
{
    CHECK(NameplatesOn(Settings{}));
    Settings none;
    none.showLabels = none.showIcons = false;
    none.replaceMobNames = none.replacePlayerNames = none.replaceNpcNames = none.replaceCursor = false;
    none.showDamageIcons = none.showStatusRankIcons = none.showImmunityIcons = false;
    CHECK(!NameplatesOn(none));
    auto onlyWithTheSwitch = [](Settings s) {
        CHECK(NameplatesOn(s));
        s.enabled = false;
        CHECK(!NameplatesOn(s));
    };
    for (bool Settings::*part : {&Settings::showLabels, &Settings::showIcons, &Settings::showDamageIcons,
             &Settings::showStatusRankIcons, &Settings::showImmunityIcons, &Settings::replaceMobNames,
             &Settings::replacePlayerNames, &Settings::replaceNpcNames, &Settings::replaceCursor, &Settings::phTimers})
    {
        Settings s = none;
        s.*part    = true;
        onlyWithTheSwitch(s);
    }
    Settings ids = none;
    ids.mobId    = MobIdFormat::LastThree;
    onlyWithTheSwitch(ids);
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
        store.values = {{"cursorColorR", r}, {"cursorColorG", g}, {"cursorColorB", b}};
        return store;
    }
}

TEST(a_white_target_cursor_saved_before_it_was_red_turns_red)
{
    // Every setting is saved whenever one changes, so a file from before 0.53 holds the old default, white, as if chosen.
    MapStore old = TargetCursor("1.0000", "1.0000", "1.0000");
    CHECK(LoadSettings(old).cursorColor == ColorFromRgb(kPhoenixRed));
}

TEST(a_white_target_cursor_chosen_since_stays_white)
{
    Settings white;
    white.cursorColor = Color{{1.0f, 1.0f, 1.0f}};
    MapStore saved;
    SaveSettings(white, saved);
    CHECK(LoadSettings(saved).cursorColor == white.cursorColor);
}

TEST(an_old_target_cursor_of_any_other_color_is_kept)
{
    MapStore old = TargetCursor("0.7700", "0.3200", "0.3200");
    CHECK(LoadSettings(old).cursorColor == (Color{{0.77f, 0.32f, 0.32f}}));
}

TEST(a_file_from_before_the_level_line_had_its_own_font_and_shadow_keeps_the_names)
{
    MapStore old;
    old.values = {{"fontName", "Georgia"}, {"fontBold", "true"}, {"textOutlineR", "0.2500"}, {"textOutlineG", "0.2400"},
        {"textOutlineB", "0.5000"}};
    const Settings s = LoadSettings(old);
    CHECK(s.labelFontName == "Georgia" && s.labelFontBold);
    CHECK(s.labelShadowColor == (Color{{0.25f, 0.24f, 0.5f}}));
}

TEST(a_file_from_before_each_kind_had_a_name_size_gives_all_of_them_its_one)
{
    MapStore old;
    old.values = {{"nameSize", "20.0000"}};
    const Settings s = LoadSettings(old);
    CHECK(s.mobNameSize == 20 && s.playerNameSize == 20 && s.selfNameSize == 20 && s.npcNameSize == 20);
}

TEST(a_file_from_before_the_cursor_scaled_on_its_own_scales_it_like_the_names)
{
    MapStore old;
    old.values = {{"scaleWithDistance", "false"}};
    CHECK(!LoadSettings(old).scaleCursor);
    old.values = {{"scaleWithDistance", "true"}};
    CHECK(LoadSettings(old).scaleCursor);
}

TEST(an_old_install_on_the_old_default_font_takes_the_bundled_one_once)
{
    // Every setting is saved whenever one changes, so a file from before 0.6 holds Trebuchet MS as if chosen.
    MapStore old;
    old.values = {{"settingsVersion", "1.0000"}, {"fontName", "Trebuchet MS"}, {"labelFontName", "Trebuchet MS"}};
    const Settings s = LoadSettings(old);
    CHECK(s.fontName == kDefaultFont && s.labelFontName == kDefaultFont);
    MapStore older; // from before settings had a version, or the level line its own font
    older.values = {{"fontName", "Trebuchet MS"}};
    CHECK(LoadSettings(older).labelFontName == kDefaultFont);
}

TEST(a_font_picked_before_or_since_is_kept)
{
    MapStore picked;
    picked.values = {{"settingsVersion", "1.0000"}, {"fontName", "Georgia"}, {"labelFontName", "Cinzel"}};
    CHECK(LoadSettings(picked).fontName == "Georgia" && LoadSettings(picked).labelFontName == "Cinzel");
    MapStore since;
    since.values = {{"settingsVersion", "2.0000"}, {"fontName", "Trebuchet MS"}};
    CHECK(LoadSettings(since).fontName == "Trebuchet MS");
}

TEST(a_white_target_cursor_saved_after_it_turned_red_stays_white)
{
    MapStore chosen = TargetCursor("1.0000", "1.0000", "1.0000");
    chosen.values["settingsVersion"] = "1.0000";
    CHECK(LoadSettings(chosen).cursorColor == (Color{{1.0f, 1.0f, 1.0f}}));
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

TEST(a_file_from_before_the_badge_group_takes_its_defaults)
{
    MapStore old;
    old.values = {{"showIcons", "true"}};
    const Settings s = LoadSettings(old);
    CHECK(!s.showDamageIcons && !s.showStatusRankIcons && !s.showImmunityIcons); // the top bar's popup shows them
    CHECK(s.detectionIconSide == IconSide::Left && s.badgeSide == IconSide::Right);
    CHECK(s.elementWeakRank == -1 && s.elementResistRank == 4 && s.maxElements == kElementCount && !s.showRanks);
    CHECK(s.badgeFontName == kBadgeFont);
    for (const StatusBadgeRule& status : s.statusBadges)
        CHECK(status == (StatusBadgeRule{true, kNoWeakRank, 4}));
    CHECK(s.weakBadgeColor == Settings{}.weakBadgeColor && s.immuneBadgeColor == Settings{}.immuneBadgeColor);
}

TEST(a_file_from_before_windows_moved_moves_the_equipment_group_and_an_empty_list_moves_none)
{
    MapStore old;
    CHECK(LoadSettings(old).movedWindows == DefaultMovedWindows());
    MapStore none;
    none.values["movedWindows"] = "";
    CHECK(LoadSettings(none).movedWindows.empty());
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
             Named{"phoenixRedPills", BarTheme::PhoenixRedPills}})
    {
        MapStore store;
        store.values["barTheme"] = t.name;
        CHECK(LoadSettings(store).barTheme == t.theme);
        MapStore saved;
        SaveSettings(LoadSettings(store), saved);
        CHECK(saved.values["barTheme"] == t.name);
    }
    const char* const widgetKeys[] = {"barZone", "barWeather", "barVanaTime", "barTarget", "barGil", "barBags",
        "barEarthTime", "barGameHelp", "barServerTime", "barFps", "barJob", "barConnection", "barParty", "barConquest", "barSignet", "barDistance"}; // in BarWidget's order
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

TEST(a_menu_font_the_list_lacks_or_a_size_past_its_ends_keeps_the_defaults)
{
    MapStore store;
    store.values["menuFont"]  = "Comic Sans";
    store.values["menuScale"] = "400";
    const Settings s          = LoadSettings(store);
    CHECK(s.menuFont == kDefaultMenuFont && s.menuScale == kMaxMenuScale);
    CHECK(MenuFontNamed("Segoe UI") != nullptr && MenuFontNamed("Barlow Condensed")->file == nullptr);
}

TEST(the_window_defaults_set_every_window_under_the_bar_or_leave_them_all_to_the_game)
{
    Settings s;
    s.windowOffsets = {WindowOffset{"equip", 10, -20}};
    UseHeadsUpWindowDefaults(s);
    CHECK(s.allUnderBar && s.moveGameHelp && s.windowOffsets.empty() && s.windowGroups == DefaultWindowGroups());
    CHECK(s.movedWindows == DefaultMovedWindows());
    UseGameWindowDefaults(s);
    CHECK(!s.allUnderBar && !s.moveGameHelp && s.movedWindows.empty() && s.windowGroups.empty());
    s.hideHelpBar = false;
    s.barWidgets[static_cast<int>(BarWidget::GameHelp)] = false;
    UseHelpTextDefaults(s);
    CHECK(s.allUnderBar && !s.moveGameHelp && s.hideHelpBar && s.barWidgets[static_cast<int>(BarWidget::GameHelp)]);
}

TEST(the_default_designs_reproduce_todays_look)
{
    const Settings d;
    const KindSettings& mobs = d.Kind(PlateKind::Mobs);
    CHECK_EQ(mobs.lines[0].text, "$detect");
    CHECK_EQ(mobs.lines[0].iconSize, 107);
    CHECK_EQ(mobs.lines[1].text, "Lv $level $con");
    CHECK_EQ(mobs.lines[1].textSize, 87);
    CHECK(mobs.lines[1].color.source == ColorSource::Toughness);
    CHECK_EQ(mobs.lines[2].text, "$name");
    CHECK(mobs.lines[2].color.source == ColorSource::Game);
    CHECK_EQ(mobs.lines[2].glow, 100);
    CHECK(mobs.lines[2].glowColor.source == ColorSource::Aggression);
    CHECK(mobs.ornament.on);
    CHECK(mobs.ornament.place == OrnamentPlace::BetweenLines2And3);
    CHECK_EQ(mobs.ornament.width, 90);
    CHECK(mobs.ornament.color.source == ColorSource::Aggression);
    CHECK(!d.Kind(PlateKind::Players).ornament.on);
    CHECK(mobs.rows[CategoryIndex(Category::WillAttack)].outline == OutlineMode::Shown);
    CHECK(mobs.rows[CategoryIndex(Category::WontAttack)].outline == OutlineMode::Disabled);
    CHECK(mobs.rows[CategoryIndex(Category::Unknown)].outline == OutlineMode::Disabled);
    CHECK(mobs.rows[CategoryIndex(Category::PartyClaim)].outline == OutlineMode::Shown);
    CHECK(mobs.rows[CategoryIndex(Category::PartyClaim)].outlineColor.source == ColorSource::Aggression);
    for (const RowSettings& row : mobs.rows)
        CHECK(row.plate == PlateMode::Shown);
    const KindSettings& players = d.Kind(PlateKind::Players);
    CHECK_EQ(players.lines[2].text, "$mentor$new$sync$away$linkshell$bazaar$seeking$gm$name"); // today's order, GM nearest the name
    CHECK(players.lines[2].glow == 0);
    CHECK(players.lines[0].text.empty() && players.lines[1].text.empty());
    CHECK(d.Kind(PlateKind::Self) == players);
    CHECK_EQ(d.Kind(PlateKind::Npcs).lines[2].text, "$name");
    CHECK_EQ(d.Kind(PlateKind::Pets).lines[2].text, "$name");
    for (PlateKind kind : {PlateKind::Players, PlateKind::Self, PlateKind::Npcs, PlateKind::Pets})
        for (int r = 0; r < RowCountOf(kind); ++r)
            CHECK(d.Kind(kind).rows[r].outline == OutlineMode::Disabled);
    CHECK_EQ(d.baseSize, 15);
    CHECK_EQ(d.scaleMin, 50);
    CHECK_EQ(d.scaleMax, 250);
    CHECK_EQ(d.detectionShown, kAllDetection);
}

TEST(the_palettes_keep_todays_colors)
{
    const Palette p = DefaultPalette();
    CHECK(p.aggression[static_cast<int>(Nature::Aggressive)] == (Color{{1.00f, 0.15f, 0.15f}}));
    CHECK(p.aggression[static_cast<int>(Nature::Placeholder)] == (Color{{0.70f, 0.30f, 1.00f}}));
    CHECK(p.toughness[static_cast<int>(LabelShade::EasyPrey)] == (Color{{0.40f, 1.00f, 0.40f}}));
    CHECK(p.target[static_cast<int>(TargetState::Targeted)] == ColorFromRgb(kPhoenixRed));
    CHECK(p.target[static_cast<int>(TargetState::OutOfRange)] == (Color{{1.00f, 0.25f, 0.25f}}));
    CHECK(p.claim[static_cast<int>(ClaimState::Yours)] == (Color{{0.30f, 0.65f, 1.00f}}));
    CHECK(p.claim[static_cast<int>(ClaimState::Others)] == (Color{{0.55f, 0.45f, 0.40f}}));
}

TEST(every_designer_key_round_trips)
{
    Settings s;
    KindSettings& mobs = s.Kind(PlateKind::Mobs);
    mobs.rows[CategoryIndex(Category::WontAttack)].plate   = PlateMode::Expanding;
    mobs.rows[CategoryIndex(Category::WontAttack)].outline = OutlineMode::WhenTargeted;
    mobs.rows[CategoryIndex(Category::WontAttack)].outlineColor = ColorPick{ColorSource::Custom, {{0.1f, 0.2f, 0.3f}}};
    LineDesign& line = mobs.lines[1];
    line.text     = "Lv $level $con $$ $id";
    line.syncWith = 2;
    line.show     = LineShow{false, ConditionBit(ShowCondition::Targeted) | ConditionBit(ShowCondition::YourClaim)};
    line.align    = Align::Right;
    line.font     = "Cinzel";
    line.bold     = true;
    line.textSize = 150;
    line.iconSize = 60;
    line.color    = ColorPick{ColorSource::Claim};
    line.shadow   = 40;
    line.shadowColor   = ColorPick{ColorSource::Custom, {{0.4f, 0.5f, 0.6f}}};
    line.glow          = 120;
    line.glowSize      = 70;
    line.glowColor     = ColorPick{ColorSource::Type};
    mobs.ornament      = OrnamentSettings{true, OrnamentPlace::BetweenLines1And2, 120, 9, ColorPick{ColorSource::Target}};
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
    s.friends    = {"Tanyrus", "Chadmage"};
    s.cursorEdgeColor = {{0.2f, 0.2f, 0.2f}};
    MapStore store;
    SaveSettings(s, store);
    CHECK(LoadSettings(store) == s);
    CHECK_EQ(store.values.at("mobsRowPassivePlate"), "expanding");
    CHECK_EQ(store.values.at("mobsLine2Text"), "Lv $level $con $$ $id");
    CHECK_EQ(store.values.at("mobsLine2When"), "5");
    CHECK_EQ(store.values.at("petsLine1Text"), "$owner's $name");
    CHECK_EQ(store.values.at("friends"), "Tanyrus Chadmage");
    CHECK_EQ(store.values.at("settingsVersion"), "3.0000");
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
    s.friends = {"", "Two Words", "Fine"};
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
    CHECK_EQ(c.friends.size(), size_t{1});
    CHECK_EQ(c.friends[0], "Fine");
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
    CHECK(s.Kind(PlateKind::Pets).rows[0].plate == PlateMode::Shown); // no old key covered pets
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
    CHECK(mobs.lines[2].bold); // fontBold=true
    CHECK_EQ(mobs.lines[1].font, store.values.at("labelFontName"));
    CHECK_EQ(s.Kind(PlateKind::Players).lines[2].font, store.values.at("playerFontName"));
    CHECK_EQ(s.Kind(PlateKind::Npcs).lines[2].font, store.values.at("npcFontName"));
    const int base = static_cast<int>(std::strtof(store.values.at("mobNameSize").c_str(), nullptr));
    CHECK_EQ(s.baseSize, base);
    CHECK_EQ(mobs.lines[2].textSize, 100);
    CHECK_EQ(mobs.lines[1].textSize, std::lround(100.0f * std::strtof(store.values.at("labelSize").c_str(), nullptr) / static_cast<float>(base)));
    CHECK_EQ(mobs.lines[0].iconSize, std::lround(100.0f * std::strtof(store.values.at("iconSize").c_str(), nullptr) / static_cast<float>(base)));
    CHECK_EQ(s.Kind(PlateKind::Self).lines[2].textSize, std::lround(100.0f * std::strtof(store.values.at("selfNameSize").c_str(), nullptr) / static_cast<float>(base)));
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
    CHECK_EQ(mobs.lines[2].glow, 0); // nameGlow=false
    CHECK(!mobs.ornament.on);        // showOrnament=false
    CHECK(mobs.ornament.color.source == ColorSource::Custom); // ownOrnamentColor=true
    CHECK_EQ(mobs.ornament.width, static_cast<int>(std::strtof(store.values.at("ornamentWidth").c_str(), nullptr)));
    CHECK_EQ(mobs.ornament.height, static_cast<int>(std::strtof(store.values.at("ornamentThickness").c_str(), nullptr)));
    CHECK_EQ(mobs.lines[2].shadow, static_cast<int>(std::strtof(store.values.at("nameShadow").c_str(), nullptr)));
    // ownNameColor, nameShadow and the player icon size reach the players' name line too.
    const LineDesign& playerName = s.Kind(PlateKind::Players).lines[2];
    CHECK(mobs.lines[2].color.source == ColorSource::Custom && playerName.color.source == ColorSource::Custom);
    CHECK_EQ(playerName.shadow, mobs.lines[2].shadow);
    CHECK_EQ(playerName.iconSize, 130); // playerIconSize=120 of playerNameSize=13 over mobNameSize=12
    CHECK_EQ(s.Kind(PlateKind::Self).lines[2].iconSize, 170); // of selfNameSize=17
    CHECK_EQ(playerName.text, "$name"); // showPlayerIcons=false
    // Saved again, the file carries the new version and the new keys.
    MapStore again;
    SaveSettings(s, again);
    CHECK_EQ(again.values.at("settingsVersion"), "3.0000");
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

TEST(claim_rows_are_outlined_by_their_own_switch_or_any_nature_switch)
{
    const Settings none = Load2({});
    CHECK(PartyRow(none).outline == OutlineMode::Shown && PartyRow(none).outlineColor.source == ColorSource::Aggression);
    CHECK(OthersRow(none).outline == OutlineMode::Shown && OthersRow(none).outlineColor.source == ColorSource::Aggression);
    const Settings party = Load2({{"partyClaimShow", "true"}});
    CHECK(PartyRow(party).outline == OutlineMode::Shown && PartyRow(party).outlineColor.source == ColorSource::Claim);
    CHECK(OthersRow(party).outlineColor.source == ColorSource::Aggression);
    const Settings others = Load2({{"otherClaimShow", "true"}});
    CHECK(OthersRow(others).outlineColor.source == ColorSource::Claim);
    CHECK(PartyRow(others).outlineColor.source == ColorSource::Aggression);
    const Settings off = Load2WithNaturesOff({});
    CHECK(PartyRow(off).outline == OutlineMode::Disabled && OthersRow(off).outline == OutlineMode::Disabled);
    const Settings onlyOthers = Load2WithNaturesOff({{"otherClaimShow", "true"}});
    CHECK(OthersRow(onlyOthers).outline == OutlineMode::Shown && OthersRow(onlyOthers).outlineColor.source == ColorSource::Claim);
    CHECK(PartyRow(onlyOthers).outline == OutlineMode::Disabled);
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
    CHECK_EQ(s.Kind(PlateKind::Players).lines[2].iconSize, 70);
    CHECK_EQ(s.Kind(PlateKind::Self).lines[2].iconSize, 120);
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
    CHECK_EQ(mobs.lines[1].shadow, 40);
    CHECK(mobs.lines[1].shadowColor.custom == (Color{{0.1f, 0.2f, 0.3f}}));
    CHECK_EQ(mobs.lines[2].shadow, 170);
    CHECK_EQ(shadow.Kind(PlateKind::Players).lines[2].shadow, 170);
    const Settings outline = Load2({{"textOutlineR", "0.6"}, {"textOutlineG", "0.7"}, {"textOutlineB", "0.8"}});
    const Color edge{{0.6f, 0.7f, 0.8f}};
    CHECK(outline.cursorEdgeColor == edge);
    CHECK(outline.Kind(PlateKind::Mobs).lines[0].shadowColor.custom == edge);
    CHECK(outline.Kind(PlateKind::Mobs).lines[2].shadowColor.custom == edge);
    CHECK(outline.Kind(PlateKind::Pets).lines[1].shadowColor.custom == edge);
    CHECK(outline.Kind(PlateKind::Mobs).lines[1].shadowColor.custom == edge); // labelShadowColor follows textOutline when unset
}
