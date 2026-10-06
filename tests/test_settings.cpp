#include "settings.h"
#include "test.h"

#include <cstdlib>
#include <map>
#include <string>

using namespace aggroglow;

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
}

TEST(empty_store_gives_defaults)
{
    MapStore store;
    const Settings s = LoadSettings(store);
    CHECK(s.enabled);
    CHECK_EQ(s.thickness, 4.0f);
    CHECK_EQ(s.smoothness, 8);
    CHECK_EQ(s.maxDistance, 40.0f);
    CHECK(s.showLabels);
    CHECK(!s.autoExamine);
    CHECK(s.show[0] && s.show[1] && s.show[2] && s.show[kNmWillAttack] && s.show[kNmWontAttack]);
    CHECK_EQ(s.color[0].v[0], 1.0f);
    CHECK_EQ(s.color[kNmWillAttack].v[1], 0.60f); // gold-orange
    CHECK_EQ(s.color[kNmWontAttack].v[1], 0.84f); // gold
}

TEST(settings_round_trip)
{
    MapStore store;
    Settings s;
    s.enabled        = false;
    s.thickness      = 6.5f;
    s.smoothness     = 12;
    s.maxDistance    = 25.0f;
    s.showLabels     = false;
    s.autoExamine    = true;
    s.show[1]        = false;
    s.show[kNmWontAttack]        = false;
    s.color[2].v[0]              = 0.25f;
    s.color[kNmWillAttack].v[2]  = 0.5f;
    SaveSettings(s, store);
    const Settings r = LoadSettings(store);
    CHECK(!r.enabled);
    CHECK_EQ(r.thickness, 6.5f);
    CHECK_EQ(r.smoothness, 12);
    CHECK_EQ(r.maxDistance, 25.0f);
    CHECK(!r.showLabels);
    CHECK(r.autoExamine);
    CHECK(!r.show[1]);
    CHECK(!r.show[kNmWontAttack]);
    CHECK_EQ(r.color[2].v[0], 0.25f);
    CHECK_EQ(r.color[kNmWillAttack].v[2], 0.5f);
    CHECK(store.values.count("nmWillAttackB") == 1);
    CHECK(store.values.count("nmWontAttackShow") == 1);
}

TEST(out_of_range_values_are_clamped)
{
    MapStore store;
    store.values["thickness"]   = "99";
    store.values["smoothness"]  = "2";
    store.values["maxDistance"] = "-5";
    store.values["willAttackA"] = "3";
    const Settings s = LoadSettings(store);
    CHECK_EQ(s.thickness, 16.0f);
    CHECK_EQ(s.smoothness, 4);
    CHECK_EQ(s.maxDistance, 5.0f);
    CHECK_EQ(s.color[0].v[3], 1.0f);
}

TEST(non_finite_values_fall_back_to_defaults)
{
    MapStore store;
    store.values["thickness"]   = "nan";
    store.values["smoothness"]  = "nan";
    store.values["maxDistance"] = "inf";
    store.values["unknownG"]    = "nan";
    const Settings s = LoadSettings(store);
    CHECK_EQ(s.thickness, 4.0f);
    CHECK_EQ(s.smoothness, 8);
    CHECK_EQ(s.maxDistance, 40.0f);
    CHECK_EQ(s.color[2].v[1], 0.70f);
}

TEST(argb_packing)
{
    CHECK_EQ(ToArgb(Color{{1.0f, 0.0f, 0.0f, 1.0f}}), 0xFFFF0000u);
    CHECK_EQ(ToArgb(Color{{0.2f, 1.0f, 0.3f, 0.5f}}), (128u << 24) | (51u << 16) | (255u << 8) | 77u);
}

TEST(colors_are_always_opaque)
{
    // Outline copies take alpha from the texture (it shapes cut-outs), so a color's alpha has no effect; keep it 1.
    MapStore store;
    store.values["wontAttackA"] = "0.2";
    const Settings s = LoadSettings(store);
    CHECK_EQ(s.color[1].v[3], 1.0f);
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
