#include "game_font.h"
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

TEST(an_adventurer_stands_with_you_and_shows_her_status_icons_before_her_name)
{
    const Tracked t(true);
    const ActorInfo& her = t.Of(SampleActor::Adventurer);
    CHECK(her.state.kind == EntityKind::Player && !her.state.self);
    const ComposedPlate plate = ComposePlate(her, t.scene.player, Settings{}, 0xFFFFFFFF);
    const ComposedLine& name = plate.lines.back();
    CHECK(!name.items.empty() && name.items.front().icons.size() >= 2u); // her linkshell's pearl and more
    CHECK(!name.items.back().text.empty() && name.items.back().icons.empty());
    CHECK(her.name != std::string(t.Of(SampleActor::You).name));
}

// The game's name colors, from the names a /hu debug report read: an unclaimed mob pale yellow, another player white,
// you light blue, an NPC light green.
TEST(a_sample_name_is_in_the_color_the_game_draws_its_kind_in)
{
    CHECK_EQ(SampleName(SampleActor::Tonberry, 0, 0).color, 0xFFFFFFC2u);
    CHECK_EQ(SampleName(SampleActor::Adventurer, 0, 0).color, 0xFFFFFFFFu);
    CHECK_EQ(SampleName(SampleActor::You, 0, 0).color, 0xFFC2C2FFu);
    CHECK_EQ(SampleName(SampleActor::Shantotto, 0, 0).color, 0xFFC2FFC2u);
}

namespace
{
    // The plate the preview draws for an actor.
    ComposedPlate Drawn(const Tracked& t, const Settings& settings, SampleActor actor)
    {
        return ComposePlate(t.Of(actor), t.scene.player, WithGameNames(settings, t.tracker, t.scene.player), 0xFFFFFFFF);
    }
}

TEST(where_the_game_would_draw_a_name_the_preview_draws_that_name_alone_in_the_games_font)
{
    Settings settings;
    for (RowSettings& row : settings.Kind(PlateKind::Self).rows)
        row.plate = PlateMode::Vanilla;
    const Tracked t(true, settings);
    const ComposedPlate you = Drawn(t, settings, SampleActor::You);
    CHECK_EQ(you.lines.size(), 1u);
    CHECK_EQ(LineText(you.lines[0]), std::string("Tanyrus"));
    CHECK(you.lines[0].items.size() == 1u && you.lines[0].items[0].icons.empty());
    CHECK(GameFontVariantOf(you.lines[0].look.font) == GameFontVariant::Game);
    CHECK(you.ornament == OrnamentPlace::Disabled);
    // The others keep their own plates.
    CHECK(LineText(Drawn(t, settings, SampleActor::Tonberry).lines[1]).find("55") != std::string::npos);
    CHECK(Drawn(t, settings, SampleActor::Adventurer).lines.back().items.front().icons.size() >= 2u);
}

TEST(with_nameplates_off_every_name_is_the_games_and_a_hidden_one_stays_hidden)
{
    Settings off;
    off.nameplates = false;
    const Tracked t(true, off);
    for (SampleActor actor : {SampleActor::Tonberry, SampleActor::You, SampleActor::Adventurer, SampleActor::Shantotto})
        CHECK_EQ(Drawn(t, off, actor).lines.size(), 1u);
    Settings hidden;
    for (RowSettings& row : hidden.Kind(PlateKind::Self).rows)
        row.plate = PlateMode::Hidden;
    CHECK(Drawn(Tracked(true, hidden), hidden, SampleActor::You).lines.empty());
    // Plates of HeadsUp's own are left as they are.
    CHECK(WithGameNames(Settings{}, Tracked(true).tracker, Tracked(true).scene.player) == Settings{});
}

TEST(a_sample_name_sits_just_over_the_head_at_the_depth_that_draws_it_whole_size)
{
    const DrawnName name = SampleName(SampleActor::Tonberry, 320.0f, 100.0f);
    CHECK(name.box.valid && name.box.maxY <= 100.0f && name.box.minX < 320.0f && name.box.maxX > 320.0f);
    CHECK(std::abs(YalmsAtDepth(name.depth) - kSampleNameYalms) < 0.01f);
    CHECK(std::abs(DepthAtYalms(kSampleNameYalms) - name.depth) < 1e-6f);
}
