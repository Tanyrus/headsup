#include "settings.h"
#include "test.h"

#include <algorithm>
#include <cstdlib>
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
    constexpr const char* kSavedFile = R"(settingsVersion=1.0000
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
fontBold=true
replacePlayerNames=false
replaceNpcNames=false
replaceCursor=false
cursorFeather=false
chocoboPointer=true
showPlayerIcons=false
centerNameAndIcons=false
ownNameColor=true
smoothness=12.0000
nameSize=12.0000
labelSize=14.0000
timerSize=17.0000
iconSize=24.0000
cursorSize=31.0000
nameRaise=12.0000
playerIconSize=120.0000
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
iconGm=right
iconMentor=hide
iconNewAdventurer=right
iconLevelSync=hide
iconAway=right
iconLinkshell=hide
iconBazaar=right
iconSeekingParty=hide
mobId=lastThree
look=fantasy
)";

    Settings SavedSettings()
    {
        Settings s;
        s.enabled = s.showLabels = s.replaceMobNames = s.showIcons = false;
        s.hideClaimedByParty = s.hideClaimed = s.hideTooWeak = s.hideWhileEngaged = true;
        s.markPlaceholders  = false;
        s.phTimers          = true;
        s.scaleWithDistance = false;
        s.fontBold          = true;
        s.replacePlayerNames = s.replaceNpcNames = s.replaceCursor = s.cursorFeather = false;
        s.chocoboPointer = true;
        s.showPlayerIcons = s.centerNameAndIcons = false;
        s.ownNameColor   = true;
        s.smoothness     = 12;
        s.nameSize       = 12;
        s.labelSize      = 14;
        s.timerSize      = 17;
        s.iconSize       = 24;
        s.cursorSize     = 31;
        s.nameRaise      = 12;
        s.playerIconSize = 120;
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
                 Outline{Category::Placeholder, false, {{0.63f, 0.64f, 0.65f}}}})
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

TEST(a_saved_file_loads_every_setting_and_saves_back_the_same)
{
    MapStore file = Parse(kSavedFile);
    CHECK(LoadSettings(file) == SavedSettings());
    MapStore saved;
    SaveSettings(SavedSettings(), saved);
    CHECK(saved.values == file.values);
}

TEST(a_player_icon_side_that_is_not_a_side_keeps_its_default)
{
    MapStore store;
    store.values["iconGm"] = "middle";
    CHECK(LoadSettings(store).playerIconSide[static_cast<int>(PlayerIcon::Gm)] == IconSide::Left);
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
    CHECK_EQ(s.nameSize, kMaxTextSize);
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
    const std::vector<std::string> expected{"Marcellus SC", "Cinzel", "Cormorant SC", "arial", "Verdana"};
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
    s.nameSize    = 100;
    s.labelSize   = 0;
    s.timerSize   = 0;
    s.iconSize    = 49;
    s.playerIconSize = 10;
    s.cursorSize  = 2;
    s.nameRaise   = 99;
    const Color wild{{2.0f, -1.0f, 0.5f}};
    s.nameColor = s.textOutline = s.iconTint = s.cursorColor = s.subCursorColor = s.lockedCursorColor = wild;
    for (Color& c : s.labelColor)
        c = wild;
    for (Color& c : s.color)
        c = wild;
    const Settings c = Clamp(s);
    const Color clamped{{1.0f, 0.0f, 0.5f}};
    for (const Color& each : {c.nameColor, c.textOutline, c.iconTint, c.cursorColor, c.subCursorColor, c.lockedCursorColor})
        CHECK(each == clamped);
    for (const Color& each : c.labelColor)
        CHECK(each == clamped);
    for (const Color& each : c.color)
        CHECK(each == clamped);
    CHECK_EQ(c.thickness, kMinThickness);
    CHECK_EQ(c.smoothness, kMaxSmoothness);
    CHECK_EQ(c.playerIconSize, kMinPlayerIconSize);
    CHECK_EQ(c.maxDistance, kMaxOutlineDistance);
    CHECK(c.fontName == kDefaultFont);
    CHECK_EQ(c.nameSize, kMaxTextSize);
    CHECK_EQ(c.labelSize, kMinTextSize);
    CHECK_EQ(c.timerSize, kMinTextSize);
    CHECK_EQ(c.iconSize, kMaxTextSize);
    CHECK_EQ(c.cursorSize, kMinTextSize);
    CHECK_EQ(c.nameRaise, kMaxNameRaise);
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
    CHECK(!NameplatesOn(none));
    auto onlyWithTheSwitch = [](Settings s) {
        CHECK(NameplatesOn(s));
        s.enabled = false;
        CHECK(!NameplatesOn(s));
    };
    for (bool Settings::*part : {&Settings::showLabels, &Settings::showIcons, &Settings::replaceMobNames,
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
