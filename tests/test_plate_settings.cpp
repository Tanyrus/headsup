#include "menu_state.h"
#include "plate_settings.h"
#include "test.h"

#include <algorithm>
#include <array>
#include <initializer_list>
#include <set>
#include <string>
#include <vector>

using namespace headsup;

namespace
{
    // Each of a table's entries has a label of its own, written as a row of the menu would show it.
    template <typename Label>
    void CheckLabels(int count, Label label)
    {
        std::set<std::string> seen;
        for (int i = 0; i < count; ++i)
        {
            const char* text = label(i);
            CHECK(text != nullptr && text[0] != '\0');
            CHECK_EQ(TitleCaseText(text), std::string(text));
            CHECK(seen.insert(text).second);
        }
    }
}

TEST(every_entry_of_every_label_table_has_a_title_case_label_of_its_own)
{
    CheckLabels(kPlateModeCount, [](int i) { return PlateModeLabel(static_cast<PlateMode>(i)); });
    CheckLabels(kOutlineModeCount, [](int i) { return OutlineModeLabel(static_cast<OutlineMode>(i)); });
    CheckLabels(kNatureCount, [](int i) { return NatureLabel(static_cast<Nature>(i)); });
    CheckLabels(kLabelShadeCount, [](int i) { return ToughnessLabel(static_cast<LabelShade>(i)); });
    CheckLabels(kClaimStateCount, [](int i) { return ClaimLabel(static_cast<ClaimState>(i)); });
    CheckLabels(kTargetStateCount, [](int i) { return TargetStateLabel(static_cast<TargetState>(i)); });
    CheckLabels(kTargetTypeCount, [](int i) { return TargetTypeLabel(static_cast<TargetType>(i)); });
    CheckLabels(kShowConditionCount, [](int i) { return ConditionLabel(static_cast<ShowCondition>(i)); });
    CheckLabels(kColorSourceCount, [](int i) { return SourceLabel(static_cast<ColorSource>(i)); });
    CheckLabels(kDetectionIconCount, [](int i) { return DetectionLabel(static_cast<DetectionIcon>(i)); });
    CheckLabels(kStatusRankCount, [](int i) { return StatusRankLabel(static_cast<StatusRank>(i)); });
    CheckLabels(kAlignCount, [](int i) { return kAlignLabels[i]; });
    CheckLabels(kOrnamentPlaceCount, [](int i) { return kOrnamentPlaceLabels[i]; });
    CheckLabels(kPlateLines, [](int i) { return kLineLabels[i]; });
    CheckLabels(kPlateKindCount, [](int i) { return KindLabel(static_cast<PlateKind>(i)); });
    for (int k = 0; k < kPlateKindCount; ++k)
    {
        const auto kind = static_cast<PlateKind>(k);
        CheckLabels(RowCountOf(kind), [kind](int i) { return RowLabel(kind, i); });
    }
}

namespace
{
    // A table's labels, entry by entry in its enum's order, against a list written out by hand.
    template <typename Label>
    void CheckOrder(int count, Label label, std::initializer_list<const char*> expected)
    {
        CHECK_EQ(static_cast<int>(expected.size()), count);
        int i = 0;
        for (const char* text : expected)
            CHECK_EQ(label(i++), text);
    }
}

TEST(every_label_names_its_own_enumerator)
{
    CheckOrder(kPlateModeCount, [](int i) { return PlateModeLabel(static_cast<PlateMode>(i)); },
        {"Hide Nameplate", "Show Nameplate", "When Targeted", "Vanilla Nameplate"});
    CHECK_EQ(PlateModeLabel(PlateMode::Hidden), "Hide Nameplate");
    CHECK_EQ(PlateModeLabel(PlateMode::Vanilla), "Vanilla Nameplate");
    CheckOrder(kOutlineModeCount, [](int i) { return OutlineModeLabel(static_cast<OutlineMode>(i)); },
        {"No Outline", "Show Outline", "When Targeted", "Inherit"});
    CHECK_EQ(OutlineModeLabel(OutlineMode::ByNature), "Inherit");
    CheckOrder(kNatureCount, [](int i) { return NatureLabel(static_cast<Nature>(i)); },
        {"Passive", "Aggressive", "NM (Passive)", "NM (Aggressive)", "NM (Placeholder)", "No Data"});
    CHECK_EQ(NatureLabel(Nature::Aggressive), "Aggressive");
    CHECK_EQ(NatureLabel(Nature::NoData), "No Data");
    CheckOrder(kLabelShadeCount, [](int i) { return ToughnessLabel(static_cast<LabelShade>(i)); },
        {"Unknown Level", "Too Weak", "Easy Prey", "Decent Challenge", "Even Match", "Tough", "Very Tough"});
    CHECK_EQ(ToughnessLabel(LabelShade::VeryTough), "Very Tough");
    CheckOrder(kClaimStateCount, [](int i) { return ClaimLabel(static_cast<ClaimState>(i)); },
        {"Unclaimed", "Claimed (Others')", "Claimed (Yours)"});
    CHECK_EQ(ClaimLabel(ClaimState::Yours), "Claimed (Yours)");
    CheckOrder(kTargetStateCount, [](int i) { return TargetStateLabel(static_cast<TargetState>(i)); },
        {"Not Targeted", "Targeted", "Locked On", "Sub-Target", "Out of Range"});
    CHECK_EQ(TargetStateLabel(TargetState::Locked), "Locked On");
    CHECK_EQ(TargetStateLabel(TargetState::OutOfRange), "Out of Range");
    CheckOrder(kTargetTypeCount, [](int i) { return TargetTypeLabel(static_cast<TargetType>(i)); },
        {"Mob", "Player", "Friend", "Party Member", "Linkshell Member", "NPC or Object", "Pet"});
    CHECK_EQ(TargetTypeLabel(TargetType::LinkshellMember), "Linkshell Member");
    CheckOrder(kOrnamentPlaceCount, [](int i) { return kOrnamentPlaceLabels[i]; }, {"Lines 1-2", "Lines 2-3"});
    CHECK_EQ(kOrnamentPlaceLabels[static_cast<int>(OrnamentPlace::BetweenLines2And3)], "Lines 2-3");
    CheckOrder(kAlignCount, [](int i) { return kAlignLabels[i]; }, {"Left", "Center", "Right"});
    CHECK_EQ(kAlignLabels[static_cast<int>(Align::Right)], "Right");
    CheckOrder(kPlateLines, [](int i) { return kLineLabels[i]; }, {"Top Line", "Middle Line", "Bottom Line"});
    CheckOrder(kShowConditionCount, [](int i) { return ConditionLabel(static_cast<ShowCondition>(i)); },
        {"When Targeted", "When Unclaimed", "When Claimed by You", "When Claimed by Others", "When Engaged in Combat",
            "When Too Weak", "When Aggressive", "When Passive", "When Party Member", "When Friend", "When Linkshell Member"});
    CHECK_EQ(ConditionLabel(ShowCondition::LinkshellMember), "When Linkshell Member");
    CheckOrder(kColorSourceCount, [](int i) { return SourceLabel(static_cast<ColorSource>(i)); },
        {"Custom", "Vanilla Color", "Color by Aggression", "Color by Toughness", "Color by Claim Status", "Color by Targeting",
            "Color by Entity Type"});
    CHECK_EQ(SourceLabel(ColorSource::Type), "Color by Entity Type");
    CheckOrder(kDetectionIconCount, [](int i) { return DetectionLabel(static_cast<DetectionIcon>(i)); },
        {"Aggression", "Links", "Sight", "True Sight", "Sound", "Scent", "Use of Spells", "Use of Abilities", "Low Player HP"});
    CHECK_EQ(DetectionLabel(DetectionIcon::Blood), "Low Player HP");
    CheckOrder(kStatusRankCount, [](int i) { return StatusRankLabel(static_cast<StatusRank>(i)); },
        {"Paralyze", "Bind", "Silence", "Slow", "Poison", "Light Sleep", "Dark Sleep", "Blind", "Stun", "Gravity"});
    CHECK_EQ(StatusRankLabel(StatusRank::Gravity), "Gravity");
    CheckOrder(kCategoryCount, [](int i) { return RowLabel(PlateKind::Mobs, i); },
        {"Aggressive", "Passive", "No Data", "NM (Aggressive)", "NM (Passive)", "NM (Placeholder)", "Claimed (Yours)",
            "Claimed (Others')"});
    CHECK_EQ(RowLabel(PlateKind::Mobs, CategoryIndex(Category::OtherClaim)), "Claimed (Others')");
    CheckOrder(kPlayerRowCount, [](int i) { return RowLabel(PlateKind::Players, i); },
        {"Players", "Party Members", "Friends", "Linkshell Members"});
    CHECK_EQ(RowLabel(PlateKind::Players, static_cast<int>(PlayerRow::LinkshellMember)), "Linkshell Members");
    CheckOrder(kPlateKindCount, [](int i) { return KindLabel(static_cast<PlateKind>(i)); },
        {"Mobs", "Players", "Self", "NPCs", "Pets"});
    CHECK_EQ(KindLabel(PlateKind::Pets), "Pets");
}

TEST(only_the_sub_target_states_of_the_target_palette_say_when_they_apply)
{
    CHECK(TargetStateTip(TargetState::None) == nullptr);
    CHECK(TargetStateTip(TargetState::Targeted) == nullptr);
    CHECK(TargetStateTip(TargetState::Locked) == nullptr);
    CHECK_EQ(TargetStateTip(TargetState::SubTarget), "Picking for a spell, ability or trade.");
    CHECK_EQ(TargetStateTip(TargetState::OutOfRange), "A sub-target out of the spell or ability's range.");
}

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

TEST(a_character_name_is_three_to_fifteen_letters)
{
    CHECK(IsCharacterName("Abc"));
    CHECK(IsCharacterName("Tanyrus"));
    CHECK(IsCharacterName("Abcdefghijklmno")); // 15
    CHECK(!IsCharacterName("Ab"));
    CHECK(!IsCharacterName("Abcdefghijklmnop")); // 16
    CHECK(!IsCharacterName(""));
    CHECK(!IsCharacterName("Chad1"));
    CHECK(!IsCharacterName("Two Words"));
    CHECK(!IsCharacterName("Tom's"));
    CHECK(!IsCharacterName("Chad-mage"));
    CHECK(!IsCharacterName("Ren\xC3\xA9" "e")); // letters past the alphabet are not the game's
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
