#include "look.h"
#include "test.h"

using namespace headsup;

namespace
{
    ActorInfo Mob(Category category, bool alive = true)
    {
        ActorInfo info;
        info.kind     = EntityKind::Mob;
        info.alive    = alive;
        info.category = category;
        return info;
    }
}

TEST(a_mobs_name_glows_in_its_outline_color_whether_or_not_the_outline_shows)
{
    Settings s;
    s.show[CategoryIndex(Category::WontAttack)] = false; // off by default
    const PlateStyle passive = StyleFor(Mob(Category::WontAttack), true, s);
    CHECK(passive.glow && passive.accent == ToArgb(s.color[CategoryIndex(Category::WontAttack)]));
    const PlateStyle aggressive = StyleFor(Mob(Category::WillAttack), true, s);
    CHECK(aggressive.accent == ToArgb(s.color[CategoryIndex(Category::WillAttack)]));
    s.color[CategoryIndex(Category::WillAttack)] = Color{{0.0f, 0.0f, 1.0f}};
    CHECK(StyleFor(Mob(Category::WillAttack), true, s).accent == ToArgb(Color{{0.0f, 0.0f, 1.0f}}));
}

TEST(players_and_npcs_have_no_glow_and_no_ornament)
{
    const Settings s;
    ActorInfo player;
    player.kind            = EntityKind::Player;
    const PlateStyle style = StyleFor(player, true, s);
    CHECK(!style.glow && !style.ornament);
    ActorInfo npc;
    npc.kind = EntityKind::Npc;
    CHECK(!StyleFor(npc, true, s).glow);
}

TEST(the_ornament_shows_only_with_a_level_line)
{
    const Settings s;
    CHECK(StyleFor(Mob(Category::WillAttack), true, s).ornament);
    CHECK(!StyleFor(Mob(Category::WillAttack), false, s).ornament);
    CHECK(StyleFor(Mob(Category::WillAttack), true, s).glow); // the glow stays: it belongs to the name
}

TEST(a_dead_mob_keeps_its_name_but_loses_its_glow)
{
    const Settings s;
    const PlateStyle dead = StyleFor(Mob(Category::WillAttack, false), false, s);
    CHECK(!dead.glow && !dead.ornament);
}
