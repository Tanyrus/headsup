#include "plate_compose.h"
#include "setup_scene.h"
#include "test.h"

#include <cmath>
#include <string>

using namespace headsup;

namespace
{
    std::string LineText(const ComposedLine& line)
    {
        std::string text;
        for (const PlateItem& item : line.items)
            text += item.text;
        return text;
    }
    struct Tracked
    {
        Tracker tracker;
        SampleScene scene;
        explicit Tracked(bool others, const Settings& settings = {})
            : scene(SetupScene(others, "Tanyrus"))
        {
            tracker.Update(scene.actors, scene.player, settings, scene.targets);
        }
        const ActorInfo& Of(SampleActor actor) const { return *tracker.FindIndex(SampleIndex(actor)); }
    };
}

TEST(the_sample_tonberry_is_a_tonberry_cutter_of_the_temple)
{
    const MobRecord* mob = MobById(kSampleTonberryId);
    CHECK(mob != nullptr);
    CHECK_EQ(std::string(mob->name), std::string("Tonberry Cutter"));
}

// From the mob data: aggressive, 52 to 55, sight; so at level 55 it would attack, and its plate reads that.
TEST(the_sample_tonberry_would_attack_you_and_its_plate_shows_its_level_and_name)
{
    const Tracked t(false);
    const ActorInfo& tonberry = t.Of(SampleActor::Tonberry);
    CHECK(tonberry.state.kind == EntityKind::Mob && tonberry.state.row == Category::WillAttack);
    CHECK(tonberry.outline);
    CHECK(t.scene.targets.target == SampleIndex(SampleActor::Tonberry));
    const ComposedPlate plate = ComposePlate(tonberry, t.scene.player, Settings{}, 0xFFFFFFFF);
    CHECK_EQ(plate.lines.size(), 3u);
    CHECK(!plate.lines[0].items.empty() && !plate.lines[0].items[0].icons.empty()); // its detection icon
    CHECK(LineText(plate.lines[1]).find("52") != std::string::npos && LineText(plate.lines[1]).find("55") != std::string::npos);
    CHECK_EQ(LineText(plate.lines[2]), std::string("Tonberry Cutter"));
}

TEST(you_and_shantotto_stand_by_the_tonberry_only_on_the_step_for_others)
{
    CHECK_EQ(SetupScene(false, "Tanyrus").actors.size(), 1u);
    const Tracked t(true);
    CHECK(t.Of(SampleActor::You).state.self && t.Of(SampleActor::You).state.kind == EntityKind::Player);
    CHECK(t.Of(SampleActor::Shantotto).state.kind == EntityKind::Npc);
    const ComposedPlate you = ComposePlate(t.Of(SampleActor::You), t.scene.player, Settings{}, 0xFFFFFFFF);
    CHECK_EQ(LineText(you.lines.back()), std::string("Tanyrus"));
    const ComposedPlate shantotto = ComposePlate(t.Of(SampleActor::Shantotto), t.scene.player, Settings{}, 0xFFFFFFFF);
    CHECK_EQ(LineText(shantotto.lines.back()), std::string("Shantotto"));
}

TEST(a_sample_name_sits_just_over_the_head_at_the_depth_that_draws_it_whole_size)
{
    const DrawnName name = SampleName(320.0f, 100.0f);
    CHECK(name.box.valid && name.box.maxY <= 100.0f && name.box.minX < 320.0f && name.box.maxX > 320.0f);
    CHECK(std::abs(YalmsAtDepth(name.depth) - kSampleNameYalms) < 0.01f);
    CHECK(std::abs(DepthAtYalms(kSampleNameYalms) - name.depth) < 1e-6f);
}
