#include "plate_settings.h"
#include "test.h"

#include <algorithm>
#include <array>
#include <vector>

using namespace headsup;

TEST(every_default_color_pick_is_one_its_kinds_page_offers)
{
    const std::array<KindSettings, kPlateKindCount> kinds = DefaultKinds();
    const auto among = [](const std::vector<ColorSource>& offered, const ColorPick& pick) {
        return std::ranges::find(offered, pick.source) != offered.end();
    };
    for (int k = 0; k < kPlateKindCount; ++k)
    {
        const auto kind = static_cast<PlateKind>(k);
        // The Line Style card's picks may take the game's color; an outline's and the ornament's may not.
        const std::vector<ColorSource> text = SourcesFor(kind, true), plain = SourcesFor(kind, false);
        const KindSettings& d = kinds[k];
        for (int r = 0; r < RowCountOf(kind); ++r)
            CHECK(among(plain, d.rows[r].outlineColor));
        for (const LineDesign& line : d.lines)
        {
            CHECK(among(text, line.color));
            CHECK(among(text, line.shadowColor));
            CHECK(among(text, line.glowColor));
        }
        CHECK(among(plain, d.ornament.color));
        if (kind == PlateKind::Mobs) continue;
        // Only a mob has an aggression: every other kind's picks start on what kind of entity it is.
        for (int r = 0; r < RowCountOf(kind); ++r)
            CHECK(d.rows[r].outlineColor.source == ColorSource::Type);
        for (const LineDesign& line : d.lines)
            CHECK(line.glowColor.source == ColorSource::Type);
        CHECK(d.ornament.color.source == ColorSource::Type);
    }
    const KindSettings& mobs = kinds[static_cast<int>(PlateKind::Mobs)];
    CHECK(mobs.rows[CategoryIndex(Category::WillAttack)].outlineColor.source == ColorSource::Aggression);
    CHECK(mobs.rows[CategoryIndex(Category::PartyClaim)].outlineColor.source == ColorSource::Claim);
    CHECK(mobs.lines[2].glowColor.source == ColorSource::Aggression);
    CHECK(mobs.ornament.color.source == ColorSource::Aggression);
}

TEST(by_nature_is_offered_only_on_the_mob_rows_that_can_step_aside)
{
    for (int k = 0; k < kPlateKindCount; ++k)
    {
        const auto kind = static_cast<PlateKind>(k);
        for (int row = 0; row < RowCountOf(kind); ++row)
        {
            const auto category = static_cast<Category>(row);
            const bool stepsAside = kind == PlateKind::Mobs && (category == Category::PartyClaim ||
                                        category == Category::OtherClaim || category == Category::Placeholder);
            // The first modes are offered, so a list of three ends before By Nature.
            CHECK_EQ(OutlineModesFor(kind, row), stepsAside ? 4 : 3);
        }
    }
}

TEST(only_a_drawn_outline_has_a_color_to_pick_and_only_headsups_plates_a_design)
{
    CHECK(!HasOutlineColor(OutlineMode::Disabled));
    CHECK(HasOutlineColor(OutlineMode::Shown));
    CHECK(HasOutlineColor(OutlineMode::WhenTargeted));
    CHECK(!HasOutlineColor(OutlineMode::ByNature)); // the row under it picks the color
    CHECK(!DrawsPlate(PlateMode::Hidden));
    CHECK(DrawsPlate(PlateMode::Shown));
    CHECK(DrawsPlate(PlateMode::WhenTargeted));
    CHECK(!DrawsPlate(PlateMode::Vanilla));
}
