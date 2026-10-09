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
    CHECK(passive.glow && passive.glowColor == ToArgb(s.color[CategoryIndex(Category::WontAttack)]));
    const PlateStyle aggressive = StyleFor(Mob(Category::WillAttack), true, s);
    CHECK(aggressive.glowColor == ToArgb(s.color[CategoryIndex(Category::WillAttack)]));
    CHECK(aggressive.ornamentColor == aggressive.glowColor);
    s.color[CategoryIndex(Category::WillAttack)] = Color{{0.0f, 0.0f, 1.0f}};
    CHECK(StyleFor(Mob(Category::WillAttack), true, s).glowColor == ToArgb(Color{{0.0f, 0.0f, 1.0f}}));
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

TEST(the_glow_can_be_turned_off_or_made_stronger)
{
    Settings s;
    s.glowStrength = 150;
    s.glowSize     = 50;
    CHECK_EQ(StyleFor(Mob(Category::WillAttack), true, s).glowStrength, 1.5f);
    CHECK_EQ(StyleFor(Mob(Category::WillAttack), true, s).glowSize, 0.5f);
    s.nameGlow             = false;
    const PlateStyle plain = StyleFor(Mob(Category::WillAttack), true, s);
    CHECK(!plain.glow && plain.ornament); // the ornament stays
}

TEST(the_glow_and_the_ornament_can_take_their_own_colors)
{
    Settings s;
    const uint32_t red = ToArgb(s.color[CategoryIndex(Category::WillAttack)]);
    const Color blue{{0.0f, 0.0f, 1.0f}}, green{{0.0f, 1.0f, 0.0f}};
    s.ownGlowColor = true;
    s.glowColor    = blue;
    PlateStyle style = StyleFor(Mob(Category::WillAttack), true, s);
    CHECK(style.glowColor == ToArgb(blue) && style.ornamentColor == red);
    s.ownOrnamentColor = true;
    s.ornamentColor    = green;
    style              = StyleFor(Mob(Category::WillAttack), true, s);
    CHECK(style.glowColor == ToArgb(blue) && style.ornamentColor == ToArgb(green));
}

TEST(the_ornament_can_be_turned_off_on_its_own)
{
    Settings s;
    s.showOrnament         = false;
    const PlateStyle style = StyleFor(Mob(Category::WillAttack), true, s);
    CHECK(!style.ornament && style.glow);
}

TEST(the_name_and_the_level_line_each_have_a_font_and_a_shadow)
{
    Settings s;
    s.fontName         = "Georgia";
    s.fontBold         = true;
    s.textOutline      = Color{{0.25f, 0.0f, 0.0f}};
    s.nameShadow       = 50;
    s.labelFontName    = "Cinzel";
    s.labelShadowColor = Color{{0.0f, 0.0f, 0.5f}};
    s.labelShadow      = 150;
    CHECK(NameLineStyle(s) == (LineStyle{"Georgia", true, ToArgb(s.textOutline), 0.5f}));
    CHECK(LevelLineStyle(s) == (LineStyle{"Cinzel", false, ToArgb(s.labelShadowColor), 1.5f}));
}

TEST(a_mob_you_or_your_party_fight_can_lose_its_glow)
{
    Settings s;
    ActorInfo yours = Mob(Category::WillAttack);
    yours.claimed = yours.claimedByParty = true;
    ActorInfo theirs = Mob(Category::WillAttack);
    theirs.claimed   = true; // someone outside your party's
    CHECK(!StyleFor(yours, true, s).glow);
    CHECK(StyleFor(yours, true, s).ornament); // only the glow goes
    CHECK(StyleFor(theirs, true, s).glow);
    s.glowOffWhenFighting = false;
    CHECK(StyleFor(yours, true, s).glow);
}
