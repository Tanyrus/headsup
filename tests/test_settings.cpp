#include "settings.h"
#include "test.h"

#include <cstdlib>
#include <map>
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

    // Every field, so a loader or saver that drops one is caught.
    void CheckSameColor(const Color& a, const Color& b)
    {
        for (int i = 0; i < 3; ++i)
            CHECK_EQ(a.v[i], b.v[i]);
    }

    void CheckSame(const Settings& a, const Settings& b)
    {
        CHECK(a.enabled == b.enabled);
        CHECK_EQ(a.thickness, b.thickness);
        CHECK_EQ(a.smoothness, b.smoothness);
        CHECK_EQ(a.maxDistance, b.maxDistance);
        CHECK(a.showLabels == b.showLabels);
        CHECK(a.replaceMobNames == b.replaceMobNames);
        CHECK(a.showIcons == b.showIcons);
        CHECK(a.scaleWithDistance == b.scaleWithDistance);
        CHECK(a.fontName == b.fontName);
        CHECK(a.fontBold == b.fontBold);
        CHECK_EQ(a.nameSize, b.nameSize);
        CHECK_EQ(a.labelSize, b.labelSize);
        CHECK_EQ(a.iconSize, b.iconSize);
        CHECK_EQ(a.playerIconSize, b.playerIconSize);
        CHECK(a.ownNameColor == b.ownNameColor);
        CHECK(a.replacePlayerNames == b.replacePlayerNames);
        CHECK(a.replaceNpcNames == b.replaceNpcNames);
        CHECK(a.replaceCursor == b.replaceCursor);
        CHECK(a.cursorFeather == b.cursorFeather);
        CHECK(a.chocoboPointer == b.chocoboPointer);
        CHECK(a.showPlayerIcons == b.showPlayerIcons);
        CHECK(a.centerNameAndIcons == b.centerNameAndIcons);
        CheckSameColor(a.cursorColor, b.cursorColor);
        CheckSameColor(a.lockedCursorColor, b.lockedCursorColor);
        CheckSameColor(a.subCursorColor, b.subCursorColor);
        CheckSameColor(a.outOfRangeCursorColor, b.outOfRangeCursorColor);
        CHECK_EQ(a.cursorSize, b.cursorSize);
        CHECK_EQ(a.nameRaise, b.nameRaise);
        CheckSameColor(a.nameColor, b.nameColor);
        for (int k = 0; k < kLabelShadeCount; ++k)
            CheckSameColor(a.labelColor[k], b.labelColor[k]);
        CheckSameColor(a.textOutline, b.textOutline);
        CheckSameColor(a.iconTint, b.iconTint);
        for (int c = 0; c < kCategoryCount; ++c)
        {
            CHECK(a.show[c] == b.show[c]);
            CheckSameColor(a.color[c], b.color[c]);
        }
        for (int i = 0; i < kPlayerIconCount; ++i)
            CHECK(a.playerIconSide[i] == b.playerIconSide[i]);
    }
}

TEST(the_game_keeps_its_own_mouse_pointer_by_default)
{
    CHECK(!Settings{}.chocoboPointer);
}

TEST(every_player_icon_shows_left_of_the_name_by_default)
{
    const Settings s;
    for (int i = 0; i < kPlayerIconCount; ++i)
        CHECK(s.playerIconSide[i] == IconSide::Left);
}

TEST(a_player_icons_side_is_saved_by_name)
{
    MapStore store;
    Settings s;
    s.playerIconSide[PlayerIconIndex(PlayerIcon::LevelSync)] = IconSide::Right;
    s.playerIconSide[PlayerIconIndex(PlayerIcon::Away)]      = IconSide::Hidden;
    SaveSettings(s, store);
    CHECK(store.values["iconLevelSync"] == "right");
    CHECK(store.values["iconAway"] == "hide");
    CHECK(store.values["iconGm"] == "left");
    store.values["iconGm"] = "middle"; // not a side: the default
    CHECK(LoadSettings(store).playerIconSide[PlayerIconIndex(PlayerIcon::Gm)] == IconSide::Left);
}

TEST(empty_store_gives_defaults)
{
    MapStore store;
    CheckSame(LoadSettings(store), Settings{});
}

TEST(settings_round_trip)
{
    MapStore store;
    Settings s;
    s.enabled           = false;
    s.thickness         = 6.5f;
    s.smoothness        = 12;
    s.maxDistance       = 25.0f;
    s.showLabels        = false;
    s.replaceMobNames = false;
    s.showIcons         = false;
    s.scaleWithDistance = false;
    s.fontName          = "Georgia";
    s.fontBold          = true;
    s.nameSize          = 20;
    s.labelSize         = 9;
    s.iconSize          = 24;
    s.playerIconSize    = 120;
    s.ownNameColor      = true;
    s.replacePlayerNames = false;
    s.replaceNpcNames   = false;
    s.replaceCursor     = false;
    s.cursorFeather     = false;
    s.chocoboPointer    = true;
    s.showPlayerIcons   = false;
    s.centerNameAndIcons = false;
    s.lockedCursorColor = Color{{0.125f, 0.25f, 0.5f}};
    s.cursorColor       = Color{{0.25f, 0.75f, 0.5f}};
    s.subCursorColor    = Color{{0.5f, 0.25f, 0.75f}};
    s.outOfRangeCursorColor = Color{{0.75f, 0.125f, 0.25f}};
    s.cursorSize        = 31;
    s.nameRaise         = 12;
    s.nameColor         = Color{{0.5f, 0.25f, 0.125f}};
    for (int k = 0; k < kLabelShadeCount; ++k)
        s.labelColor[k] = Color{{0.0625f * static_cast<float>(k), 0.5f, 0.25f}};
    s.textOutline = Color{{0.25f, 0.25f, 0.5f}};
    s.iconTint    = Color{{0.75f, 0.5f, 1.0f}};
    for (int c = 0; c < kCategoryCount; ++c)
    {
        s.show[c]  = c % 2 == 0;
        s.color[c] = Color{{0.125f * static_cast<float>(c), 0.5f, 0.75f}}; // each its own, so a swapped key shows
    }
    for (int i = 0; i < kPlayerIconCount; ++i)
        s.playerIconSide[i] = static_cast<IconSide>((i + 1) % kIconSideCount); // a swapped key shows
    SaveSettings(s, store);
    CheckSame(LoadSettings(store), s);
    CHECK(store.values.count("nmWillAttackB") == 1);
    CHECK(store.values.count("nmWontAttackShow") == 1);
    CHECK(store.values.count("labelVeryToughR") == 1);
    CHECK(store.values.count("iconLevelSync") == 1);
    CHECK(store.values.count("outOfRangeCursorColorR") == 1);
    CHECK(store.values.count("iconSeekingParty") == 1);
}

TEST(out_of_range_values_are_clamped)
{
    MapStore store;
    store.values["thickness"]   = "99";
    store.values["smoothness"]  = "2";
    store.values["maxDistance"] = "-5";
    store.values["nameSize"]    = "100";
    store.values["labelSize"]   = "2";
    const Settings s = LoadSettings(store);
    CHECK_EQ(s.thickness, kMaxThickness);
    CHECK_EQ(s.smoothness, kMinSmoothness);
    CHECK_EQ(s.maxDistance, kMinOutlineDistance);
    CHECK_EQ(s.nameSize, kMaxTextSize);
    CHECK_EQ(s.labelSize, kMinTextSize);
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

TEST(the_font_list_is_the_installed_families_sorted_once_each)
{
    // GDI lists a family once per character set, and vertical variants with an @.
    const std::vector<std::string> choices =
        FontChoices({"Verdana", "@MS Gothic", "arial", "Verdana", "Trebuchet MS", "MS Gothic", ""}, "Trebuchet MS");
    CHECK((choices == std::vector<std::string>{"arial", "MS Gothic", "Trebuchet MS", "Verdana"}));
    // The chosen font stays in the list when it is not installed, so the dropdown can show it.
    const std::vector<std::string> missing = FontChoices({"Verdana"}, "Gill Sans");
    CHECK((missing == std::vector<std::string>{"Gill Sans", "Verdana"}));
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
        CheckSameColor(each, clamped);
    for (const Color& each : c.labelColor)
        CheckSameColor(each, clamped);
    for (const Color& each : c.color)
        CheckSameColor(each, clamped);
    CHECK_EQ(c.thickness, kMinThickness);
    CHECK_EQ(c.smoothness, kMaxSmoothness);
    CHECK_EQ(c.playerIconSize, kMinPlayerIconSize);
    CHECK_EQ(c.maxDistance, kMaxOutlineDistance);
    CHECK(c.fontName == kDefaultFont);
    CHECK_EQ(c.nameSize, kMaxTextSize);
    CHECK_EQ(c.labelSize, kMinTextSize);
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
    CheckSame(LoadSettings(store), Settings{});
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
    CHECK(!ReplacesNames(s));
    for (bool Settings::*kind : {&Settings::replaceMobNames, &Settings::replacePlayerNames, &Settings::replaceNpcNames})
    {
        Settings one = s;
        one.*kind    = true;
        CHECK(ReplacesNames(one));
        one.enabled = false;
        CHECK(!ReplacesNames(one));
    }
}

TEST(nameplates_need_the_master_switch_and_a_part)
{
    CHECK(NameplatesOn(Settings{}));
    Settings none;
    none.showLabels = none.showIcons = false;
    none.replaceMobNames = none.replacePlayerNames = none.replaceNpcNames = none.replaceCursor = false;
    CHECK(!NameplatesOn(none));
    for (bool Settings::*part : {&Settings::showLabels, &Settings::showIcons, &Settings::replaceMobNames,
             &Settings::replacePlayerNames, &Settings::replaceNpcNames, &Settings::replaceCursor})
    {
        Settings s  = none;
        s.*part     = true;
        CHECK(NameplatesOn(s));
        s.enabled = false;
        CHECK(!NameplatesOn(s));
    }
}
