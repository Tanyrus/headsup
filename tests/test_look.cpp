#include "look.h"
#include "test.h"

using namespace headsup;

TEST(each_kind_of_badge_takes_its_color)
{
    Settings s;
    s.weakBadgeColor   = ColorFromRgb(0x112233);
    s.resistBadgeColor = ColorFromRgb(0x445566);
    s.rankBadgeColor   = ColorFromRgb(0x778899);
    s.immuneBadgeColor = ColorFromRgb(0xAABBCC);
    CHECK_EQ(BadgeColor(BadgeKind::Weak, s), 0xFF112233u);
    CHECK_EQ(BadgeColor(BadgeKind::WeakRank, s), 0xFF112233u);
    CHECK_EQ(BadgeColor(BadgeKind::Resist, s), 0xFF445566u);
    CHECK_EQ(BadgeColor(BadgeKind::ResistRank, s), 0xFF445566u);
    CHECK_EQ(BadgeColor(BadgeKind::StatusWeak, s), 0xFF112233u);
    CHECK_EQ(BadgeColor(BadgeKind::StatusRank, s), 0xFF778899u);
    CHECK_EQ(BadgeColor(BadgeKind::Immune, s), 0xFFAABBCCu);
}

TEST(badges_are_bold_in_their_own_font_with_a_crisp_black_edge)
{
    Settings s;
    s.badgeFontName = "Tahoma";
    CHECK(BadgeLineStyle(s) == (LineStyle{"Tahoma", StyleBit(FontStyle::Bold), 0xFF000000u, 0.0f, true}));
    // With no font of their own they are set in the default font, as a line is.
    s.badgeFontName = "";
    s.defaultFont   = "Cinzel";
    CHECK_EQ(BadgeLineStyle(s).font, "Cinzel");
    CHECK_EQ(BadgeLineStyle(Settings{}).font, Settings{}.defaultFont); // a new install's
}
