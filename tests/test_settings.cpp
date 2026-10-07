#include "settings.h"
#include "test.h"

#include <cstdlib>
#include <map>
#include <string>

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
        void Set(const char* key, const char* value) override { values[key] = value; }
    };

    constexpr int kNmWillAttack = static_cast<int>(Category::NmWillAttack);
    constexpr int kNmWontAttack = static_cast<int>(Category::NmWontAttack);

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
        CHECK(a.replaceNameplates == b.replaceNameplates);
        CHECK(a.showIcons == b.showIcons);
        CHECK(a.scaleWithDistance == b.scaleWithDistance);
        CHECK_EQ(a.fontIndex, b.fontIndex);
        CHECK(a.fontBold == b.fontBold);
        CHECK_EQ(a.nameSize, b.nameSize);
        CHECK_EQ(a.labelSize, b.labelSize);
        CHECK_EQ(a.iconSize, b.iconSize);
        CHECK(a.ownNameColor == b.ownNameColor);
        CHECK(a.hideBehindWalls == b.hideBehindWalls);
        CHECK(a.replacePlayerNames == b.replacePlayerNames);
        CHECK(a.replaceNpcNames == b.replaceNpcNames);
        CHECK(a.replaceCursor == b.replaceCursor);
        CHECK(a.cursorFeather == b.cursorFeather);
        CHECK(a.showPlayerIcons == b.showPlayerIcons);
        CHECK(a.centerNameAndIcons == b.centerNameAndIcons);
        CheckSameColor(a.cursorColor, b.cursorColor);
        CheckSameColor(a.lockedCursorColor, b.lockedCursorColor);
        CheckSameColor(a.subCursorColor, b.subCursorColor);
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
    }
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
    s.replaceNameplates = true;
    s.showIcons         = false;
    s.scaleWithDistance = true;
    s.fontIndex         = 3;
    s.fontBold          = false;
    s.nameSize          = 20;
    s.labelSize         = 9;
    s.iconSize          = 24;
    s.ownNameColor      = true;
    s.hideBehindWalls   = false;
    s.replacePlayerNames = true;
    s.replaceNpcNames   = true;
    s.replaceCursor     = true;
    s.cursorFeather     = true;
    s.showPlayerIcons   = false;
    s.centerNameAndIcons = false;
    s.lockedCursorColor = Color{{0.125f, 0.25f, 0.5f}};
    s.cursorColor       = Color{{0.25f, 0.75f, 0.5f}};
    s.subCursorColor    = Color{{0.5f, 0.25f, 0.75f}};
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
        s.color[c] = Color{{0.25f, 0.5f, 0.75f}};
    }
    SaveSettings(s, store);
    CheckSame(LoadSettings(store), s);
    CHECK(store.values.count("nmWillAttackB") == 1);
    CHECK(store.values.count("nmWontAttackShow") == 1);
    CHECK(store.values.count("labelVeryToughR") == 1);
}

TEST(out_of_range_values_are_clamped)
{
    MapStore store;
    store.values["thickness"]   = "99";
    store.values["smoothness"]  = "2";
    store.values["maxDistance"] = "-5";
    store.values["fontIndex"]   = "9";
    store.values["nameSize"]    = "100";
    store.values["labelSize"]   = "2";
    const Settings s = LoadSettings(store);
    CHECK_EQ(s.thickness, kMaxThickness);
    CHECK_EQ(s.smoothness, kMinSmoothness);
    CHECK_EQ(s.maxDistance, kMinOutlineDistance);
    CHECK_EQ(s.fontIndex, kFontCount - 1);
    CHECK_EQ(s.nameSize, kMaxTextSize);
    CHECK_EQ(s.labelSize, kMinTextSize);
    CHECK(std::string(FontFamily(-1)) == FontFamily(0));
    CHECK(std::string(FontFamily(kFontCount)) == FontFamily(0));
}

TEST(clamp_bounds_every_field)
{
    // The menu relies on Clamp, not on the loader's own bounds.
    Settings s;
    s.thickness   = 0.0f;
    s.smoothness  = 99;
    s.maxDistance = 1000.0f;
    s.fontIndex   = -3;
    s.nameSize    = 100;
    s.labelSize   = 0;
    s.iconSize    = 49;
    s.cursorSize  = 2;
    s.nameRaise   = 99;
    s.color[0]    = Color{{2.0f, -1.0f, 0.5f}};
    s.nameColor   = s.color[0];
    s.labelColor[kLabelShadeCount - 1] = s.color[0];
    s.textOutline = s.color[0];
    s.iconTint    = s.color[0];
    s.cursorColor = s.color[0];
    s.subCursorColor = s.color[0];
    s.lockedCursorColor = s.color[0];
    const Settings c = Clamp(s);
    for (const Color& clamped : {c.nameColor, c.labelColor[kLabelShadeCount - 1], c.textOutline, c.iconTint, c.cursorColor,
             c.subCursorColor, c.lockedCursorColor})
        CheckSameColor(clamped, Color{{1.0f, 0.0f, 0.5f}});
    CHECK_EQ(c.thickness, kMinThickness);
    CHECK_EQ(c.smoothness, kMaxSmoothness);
    CHECK_EQ(c.maxDistance, kMaxOutlineDistance);
    CHECK_EQ(c.fontIndex, 0);
    CHECK_EQ(c.nameSize, kMaxTextSize);
    CHECK_EQ(c.labelSize, kMinTextSize);
    CHECK_EQ(c.iconSize, kMaxTextSize);
    CHECK_EQ(c.cursorSize, kMinTextSize);
    CHECK_EQ(c.nameRaise, kMaxNameRaise);
    CHECK_EQ(c.color[0].v[0], 1.0f);
    CHECK_EQ(c.color[0].v[1], 0.0f);
    CHECK_EQ(c.color[0].v[2], 0.5f);
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

TEST(v1_settings_files_still_load)
{
    // A v1 file has no NM keys and may still carry the removed modernConTable key.
    MapStore store;
    store.values["willAttackR"]    = "0.5000";
    store.values["modernConTable"] = "true";
    const Settings s = LoadSettings(store);
    CHECK_EQ(s.color[0].v[0], 0.5f);
    CHECK(s.show[kNmWillAttack] && s.show[kNmWontAttack]);
    CHECK_EQ(s.color[kNmWontAttack].v[0], 1.0f);
}

TEST(any_replaced_kind_of_name_counts)
{
    Settings s;
    CHECK(!ReplacesNames(s)); // nothing is replaced by default
    for (bool Settings::*kind : {&Settings::replaceNameplates, &Settings::replacePlayerNames, &Settings::replaceNpcNames})
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
    CHECK(NameplatesOn(Settings{})); // labels and icons are on by default
    Settings none;
    none.showLabels = none.showIcons = false;
    CHECK(!NameplatesOn(none));
    for (bool Settings::*part : {&Settings::showLabels, &Settings::showIcons, &Settings::replaceNameplates,
             &Settings::replacePlayerNames, &Settings::replaceNpcNames, &Settings::replaceCursor})
    {
        Settings s  = none;
        s.*part     = true;
        CHECK(NameplatesOn(s));
        s.enabled = false;
        CHECK(!NameplatesOn(s));
    }
}
