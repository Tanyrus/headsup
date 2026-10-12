#include "entity_state.h"
#include "test.h"

using namespace headsup;

namespace
{
    EntityState Mob(Nature nature, ClaimState claim = ClaimState::Unclaimed)
    {
        EntityState s;
        s.kind   = EntityKind::Mob;
        s.alive  = true;
        s.nature = nature;
        s.claim  = claim;
        s.row    = MobRowOf(nature, claim == ClaimState::Yours, claim != ClaimState::Unclaimed);
        return s;
    }
}

TEST(a_classified_category_maps_to_its_nature_and_a_placeholder_wins)
{
    CHECK(NatureOf(Category::WillAttack, false) == Nature::Aggressive);
    CHECK(NatureOf(Category::WontAttack, false) == Nature::Passive);
    CHECK(NatureOf(Category::NmWillAttack, false) == Nature::AggressiveNm);
    CHECK(NatureOf(Category::NmWontAttack, false) == Nature::PassiveNm);
    CHECK(NatureOf(Category::Unknown, false) == Nature::NoData);
    CHECK(NatureOf(Category::WillAttack, true) == Nature::Placeholder);
}

TEST(a_claim_wins_over_the_placeholder_which_wins_over_the_nature)
{
    CHECK(MobRowOf(Nature::Aggressive, false, false) == Category::WillAttack);
    CHECK(MobRowOf(Nature::Placeholder, false, false) == Category::Placeholder);
    CHECK(MobRowOf(Nature::Placeholder, false, true) == Category::OtherClaim);
    CHECK(MobRowOf(Nature::Placeholder, true, true) == Category::PartyClaim);
    CHECK(MobRowOf(Nature::NoData, false, false) == Category::Unknown);
}

TEST(a_party_member_outranks_a_friend_who_outranks_a_linkshell_co_member)
{
    CHECK(PlayerRowOf(true, true, true) == PlayerRow::PartyMember);
    CHECK(PlayerRowOf(false, true, true) == PlayerRow::Friend);
    CHECK(PlayerRowOf(false, false, true) == PlayerRow::LinkshellMember);
    CHECK(PlayerRowOf(false, false, false) == PlayerRow::Player);
}

TEST(the_plate_kind_and_row_follow_the_state)
{
    EntityState pet;
    pet.kind = EntityKind::Mob;
    pet.pet  = true;
    CHECK(PlateKindOf(pet) == PlateKind::Pets);
    CHECK_EQ(RowOf(pet), 0);
    EntityState you;
    you.kind = EntityKind::Player;
    you.self = true;
    CHECK(PlateKindOf(you) == PlateKind::Self);
    EntityState friendly;
    friendly.kind      = EntityKind::Player;
    friendly.playerRow = PlayerRow::Friend;
    CHECK(PlateKindOf(friendly) == PlateKind::Players);
    CHECK_EQ(RowOf(friendly), static_cast<int>(PlayerRow::Friend));
    CHECK(TypeOf(friendly) == TargetType::Friend);
    const EntityState mob = Mob(Nature::Aggressive, ClaimState::Yours);
    CHECK(PlateKindOf(mob) == PlateKind::Mobs);
    CHECK_EQ(RowOf(mob), CategoryIndex(Category::PartyClaim));
    CHECK(TypeOf(mob) == TargetType::Mob);
    CHECK(TypeOf(pet) == TargetType::Pet);
    EntityState npc;
    CHECK(TypeOf(npc) == TargetType::Npc);
}

TEST(the_target_state_comes_from_the_slots)
{
    CursorTargets t;
    t.target    = 5;
    t.subTarget = 9;
    t.locked    = true;
    CHECK(TargetStateOf(5, t) == TargetState::Locked);
    CHECK(TargetStateOf(9, t) == TargetState::SubTarget);
    t.outOfRange = true;
    CHECK(TargetStateOf(9, t) == TargetState::OutOfRange);
    CHECK(TargetStateOf(7, t) == TargetState::None);
    CHECK(TargetStateOf(0, t) == TargetState::None);
    t.locked = false;
    CHECK(TargetStateOf(5, t) == TargetState::Targeted);
    CHECK(Targeted(TargetState::Targeted) && Targeted(TargetState::OutOfRange) && !Targeted(TargetState::None));
}

TEST(every_color_source_resolves_from_the_state)
{
    Palette p = DefaultPalette();
    p.aggression[static_cast<int>(Nature::Aggressive)] = {{1.0f, 0.0f, 0.0f}};
    p.toughness[static_cast<int>(LabelShade::Tough)]   = {{0.0f, 1.0f, 0.0f}};
    p.claim[static_cast<int>(ClaimState::Others)]      = {{0.0f, 0.0f, 1.0f}};
    p.target[static_cast<int>(TargetState::Locked)]    = {{1.0f, 1.0f, 0.0f}};
    p.type[static_cast<int>(TargetType::Mob)]          = {{0.0f, 1.0f, 1.0f}};
    EntityState s   = Mob(Nature::Aggressive, ClaimState::Others);
    s.toughness     = LabelShade::Tough;
    s.target        = TargetState::Locked;
    const uint32_t game = 0xFF123456;
    CHECK_EQ(ResolveColor(ColorPick{ColorSource::Custom, {{0.5f, 0.5f, 0.5f}}}, s, p, game), ToArgb(Color{{0.5f, 0.5f, 0.5f}}));
    CHECK_EQ(ResolveColor(ColorPick{ColorSource::Game}, s, p, game), game);
    CHECK_EQ(ResolveColor(ColorPick{ColorSource::Aggression}, s, p, game), ToArgb(Color{{1.0f, 0.0f, 0.0f}}));
    CHECK_EQ(ResolveColor(ColorPick{ColorSource::Toughness}, s, p, game), ToArgb(Color{{0.0f, 1.0f, 0.0f}}));
    CHECK_EQ(ResolveColor(ColorPick{ColorSource::Claim}, s, p, game), ToArgb(Color{{0.0f, 0.0f, 1.0f}}));
    CHECK_EQ(ResolveColor(ColorPick{ColorSource::Target}, s, p, game), ToArgb(Color{{1.0f, 1.0f, 0.0f}}));
    CHECK_EQ(ResolveColor(ColorPick{ColorSource::Type}, s, p, game), ToArgb(Color{{0.0f, 1.0f, 1.0f}}));
}

TEST(always_shows_and_otherwise_any_checked_condition_does)
{
    EntityState s = Mob(Nature::Aggressive, ClaimState::Yours);
    s.target      = TargetState::Targeted;
    s.engaged     = true;
    CHECK(LineShows(LineShow{true, 0}, s));
    CHECK(!LineShows(LineShow{false, 0}, s));
    CHECK(LineShows(LineShow{false, ConditionBit(ShowCondition::YourClaim)}, s));
    CHECK(!LineShows(LineShow{false, ConditionBit(ShowCondition::Unclaimed) | ConditionBit(ShowCondition::OthersClaim)}, s));
    CHECK(LineShows(LineShow{false, ConditionBit(ShowCondition::Unclaimed) | ConditionBit(ShowCondition::Targeted)}, s));
}

TEST(each_condition_holds_for_its_state_alone)
{
    const auto holds = [](ShowCondition c, const EntityState& s) { return LineShows(LineShow{false, ConditionBit(c)}, s); };
    EntityState unclaimed = Mob(Nature::Passive);
    EntityState yours     = Mob(Nature::Aggressive, ClaimState::Yours);
    EntityState others    = Mob(Nature::Aggressive, ClaimState::Others);
    CHECK(holds(ShowCondition::Unclaimed, unclaimed) && !holds(ShowCondition::Unclaimed, yours));
    CHECK(holds(ShowCondition::YourClaim, yours) && !holds(ShowCondition::YourClaim, others));
    CHECK(holds(ShowCondition::OthersClaim, others) && !holds(ShowCondition::OthersClaim, yours));
    CHECK(holds(ShowCondition::Passive, unclaimed) && !holds(ShowCondition::Passive, yours));
    CHECK(holds(ShowCondition::Aggressive, yours) && !holds(ShowCondition::Aggressive, unclaimed));
    EntityState nm = Mob(Nature::AggressiveNm);
    CHECK(holds(ShowCondition::Aggressive, nm));
    unclaimed.tooWeak = true;
    CHECK(holds(ShowCondition::TooWeak, unclaimed) && !holds(ShowCondition::TooWeak, yours));
    yours.engaged = true;
    CHECK(holds(ShowCondition::Engaged, yours) && !holds(ShowCondition::Engaged, others));
    yours.target = TargetState::SubTarget;
    CHECK(holds(ShowCondition::Targeted, yours) && !holds(ShowCondition::Targeted, others));
    EntityState party;
    party.kind      = EntityKind::Player;
    party.playerRow = PlayerRow::PartyMember;
    CHECK(holds(ShowCondition::PartyMember, party) && !holds(ShowCondition::Friend, party));
    party.playerRow = PlayerRow::Friend;
    CHECK(holds(ShowCondition::Friend, party));
    party.playerRow = PlayerRow::LinkshellMember;
    CHECK(holds(ShowCondition::LinkshellMember, party));
    CHECK(!holds(ShowCondition::PartyMember, yours)); // a mob is never a party member
}

TEST(a_friend_is_named_in_the_list_whatever_the_case)
{
    const std::vector<std::string> friends{"Chadmage", "pollux"};
    CHECK(IsFriend(friends, "Chadmage"));
    CHECK(IsFriend(friends, "chadMAGE"));
    CHECK(IsFriend(friends, "Pollux"));
    CHECK(!IsFriend(friends, "Chad"));
    CHECK(!IsFriend(friends, "Chadmages"));
    CHECK(!IsFriend(friends, ""));
    CHECK(!IsFriend({}, "Chadmage"));
}
