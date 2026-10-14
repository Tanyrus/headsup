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
        s.row    = MobRowOf(nature, claim == ClaimState::Yours, claim != ClaimState::Unclaimed, true);
        s.classified = MobRowOf(nature, false, false, true);
        return s;
    }

    // A lottery placeholder, whose nature says only that: classified is what its aggression makes it.
    EntityState Placeholder(Category classified)
    {
        EntityState s = Mob(Nature::Placeholder);
        s.classified  = classified;
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
    CHECK(MobRowOf(Nature::Aggressive, false, false, true) == Category::WillAttack);
    CHECK(MobRowOf(Nature::Placeholder, false, false, true) == Category::Placeholder);
    CHECK(MobRowOf(Nature::Placeholder, false, true, true) == Category::OtherClaim);
    CHECK(MobRowOf(Nature::Placeholder, true, true, true) == Category::PartyClaim);
    CHECK(MobRowOf(Nature::NoData, false, false, true) == Category::Unknown);
    // A pet's row is its owner's standing with you, the nearest first.
    CHECK(PetRowOf(true, false, false) == PetRow::Yours);
    CHECK(PetRowOf(false, true, true) == PetRow::Party);
    CHECK(PetRowOf(false, false, true) == PetRow::Friend);
    CHECK(PetRowOf(false, false, false) == PetRow::Others);
    EntityState friendsPet;
    friendsPet.kind   = EntityKind::Mob;
    friendsPet.pet    = true;
    friendsPet.petRow = PetRow::Friend;
    CHECK(PlateKindOf(friendsPet) == PlateKind::Pets);
    CHECK_EQ(RowOf(friendsPet), static_cast<int>(PetRow::Friend));
    // Only your own pet's lines show by "When Owner is You", and no player's or mob's.
    const LineShow yours{false, ConditionBit(ShowCondition::OwnerIsYou)};
    CHECK(!LineShows(yours, friendsPet, false));
    friendsPet.petRow = PetRow::Yours;
    CHECK(LineShows(yours, friendsPet, false));
    friendsPet.pet = false;
    CHECK(!LineShows(yours, friendsPet, false));
    // Dead, it is in the Dead row whatever it was and whoever claimed it.
    CHECK(MobRowOf(Nature::Aggressive, false, false, false) == Category::Dead);
    CHECK(MobRowOf(Nature::Placeholder, true, true, false) == Category::Dead);
    CHECK(MobRowOf(Nature::NoData, false, true, false) == Category::Dead);
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
    CHECK_EQ(RowOf(pet), static_cast<int>(PetRow::Others)); // nobody's it is known to be
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

TEST(your_target_keeps_its_own_cursor_under_the_candidates_while_you_pick_it)
{
    CursorTargets t;
    t.target = t.subTarget = 5;
    CHECK(TargetStateOf(5, t) == TargetState::SubTarget); // the candidate's, on top
    CHECK(TargetUnderPick(5, t) == TargetState::Targeted);
    t.locked = true;
    CHECK(TargetUnderPick(5, t) == TargetState::Locked);
    CHECK(TargetUnderPick(7, t) == TargetState::None);
    t.subTarget = 9; // picking someone else: each has one cursor
    CHECK(TargetUnderPick(5, t) == TargetState::None);
    CHECK(TargetUnderPick(9, t) == TargetState::None);
    t.subTarget = 0;
    CHECK(TargetUnderPick(5, t) == TargetState::None);
    CHECK(TargetUnderPick(0, CursorTargets{}) == TargetState::None);
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
    CHECK(LineShows(LineShow{true, 0}, s, false));
    CHECK(!LineShows(LineShow{false, 0}, s, false));
    CHECK(LineShows(LineShow{false, ConditionBit(ShowCondition::YourClaim)}, s, false));
    CHECK(!LineShows(LineShow{false, ConditionBit(ShowCondition::Unclaimed) | ConditionBit(ShowCondition::OthersClaim)}, s, false));
    CHECK(LineShows(LineShow{false, ConditionBit(ShowCondition::Unclaimed) | ConditionBit(ShowCondition::Targeted)}, s, false));
}

TEST(each_condition_holds_for_its_state_alone)
{
    const auto holds = [](ShowCondition c, const EntityState& s, bool youFight = false) { return LineShows(LineShow{false, ConditionBit(c)}, s, youFight); };
    EntityState unclaimed = Mob(Nature::Passive);
    EntityState yours     = Mob(Nature::Aggressive, ClaimState::Yours);
    EntityState others    = Mob(Nature::Aggressive, ClaimState::Others);
    CHECK(holds(ShowCondition::Unclaimed, unclaimed) && !holds(ShowCondition::Unclaimed, yours));
    CHECK(holds(ShowCondition::YourClaim, yours) && !holds(ShowCondition::YourClaim, others));
    CHECK(holds(ShowCondition::OthersClaim, others) && !holds(ShowCondition::OthersClaim, yours));
    CHECK(holds(ShowCondition::Passive, unclaimed) && !holds(ShowCondition::Passive, yours));
    CHECK(holds(ShowCondition::Aggressive, yours) && !holds(ShowCondition::Aggressive, unclaimed));
    EntityState nm = Mob(Nature::AggressiveNm);
    CHECK(holds(ShowCondition::Aggressive, nm) && !holds(ShowCondition::Passive, nm));
    const EntityState passiveNm = Mob(Nature::PassiveNm);
    CHECK(holds(ShowCondition::Passive, passiveNm) && !holds(ShowCondition::Aggressive, passiveNm));
    for (const Category attacks : {Category::WillAttack, Category::NmWillAttack})
        CHECK(holds(ShowCondition::Aggressive, Placeholder(attacks)) && !holds(ShowCondition::Passive, Placeholder(attacks)));
    for (const Category ignores : {Category::WontAttack, Category::NmWontAttack})
        CHECK(holds(ShowCondition::Passive, Placeholder(ignores)) && !holds(ShowCondition::Aggressive, Placeholder(ignores)));
    unclaimed.tooWeak = true;
    CHECK(holds(ShowCondition::TooWeak, unclaimed) && !holds(ShowCondition::TooWeak, yours));
    // Your own combat, as its tip says, whatever this entity does.
    CHECK(!holds(ShowCondition::Engaged, yours));
    CHECK(holds(ShowCondition::Engaged, others, true) && holds(ShowCondition::Engaged, unclaimed, true));
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

TEST(generally_holds_while_the_entity_is_in_none_of_its_kinds_special_states)
{
    const LineShow generally{false, ConditionBit(ShowCondition::Generally)};
    const LineShow orTargeted{false, ConditionBit(ShowCondition::Generally) | ConditionBit(ShowCondition::Targeted)};
    // The user's example, a mob line with Generally and Targeted.
    const EntityState mob = Mob(Nature::Aggressive);
    EntityState target    = mob;
    target.target         = TargetState::Targeted;
    EntityState weak      = mob;
    weak.tooWeak          = true;
    CHECK(LineShows(orTargeted, mob, false));
    CHECK(LineShows(orTargeted, target, false));
    CHECK(!LineShows(orTargeted, Mob(Nature::Aggressive, ClaimState::Others), false));
    CHECK(!LineShows(orTargeted, weak, false));
    CHECK(!LineShows(generally, target, false));
    CHECK(!LineShows(generally, Mob(Nature::Passive, ClaimState::Yours), false));
    CHECK(!LineShows(generally, Mob(Nature::Passive), true));
    // Every mob is unclaimed or claimed and aggressive or passive: those say nothing against it.
    CHECK(LineShows(generally, Mob(Nature::Passive), false));
    CHECK(LineShows(generally, Placeholder(Category::WillAttack), false));
    EntityState player;
    player.kind  = EntityKind::Player;
    player.alive = true;
    CHECK(LineShows(generally, player, false));
    for (const PlayerRow row : {PlayerRow::PartyMember, PlayerRow::Friend, PlayerRow::LinkshellMember})
    {
        player.playerRow = row;
        CHECK(!LineShows(generally, player, false));
    }
    EntityState self = player;
    self.self        = true;
    self.playerRow   = PlayerRow::Player;
    CHECK(LineShows(generally, self, false));
    CHECK(!LineShows(generally, self, true));
    EntityState pet = Mob(Nature::Passive);
    pet.pet         = true;
    pet.petRow      = PetRow::Party;
    CHECK(LineShows(generally, pet, false));
    pet.petRow = PetRow::Yours;
    CHECK(!LineShows(generally, pet, false));
    EntityState npc;
    npc.kind = EntityKind::Npc;
    CHECK(LineShows(generally, npc, false));
    npc.target = TargetState::Targeted;
    CHECK(!LineShows(generally, npc, false));
}

TEST(the_cursor_takes_its_picks_color_and_the_targeting_color_where_the_pick_has_nothing_to_read)
{
    Settings s;
    const Palette& p = s.palette;
    EntityState mob;
    mob.kind   = EntityKind::Mob;
    mob.nature = Nature::Passive;
    mob.claim  = ClaimState::Others;
    mob.target = TargetState::Locked;
    const uint32_t locked = ToArgb(p.target[static_cast<int>(TargetState::Locked)]);
    CHECK_EQ(CursorColorOf(ColorPick{ColorSource::Target}, mob, p), locked);
    CHECK_EQ(CursorColorOf(ColorPick{ColorSource::Aggression}, mob, p), ToArgb(p.aggression[static_cast<int>(Nature::Passive)]));
    CHECK_EQ(CursorColorOf(ColorPick{ColorSource::Claim}, mob, p), ToArgb(p.claim[static_cast<int>(ClaimState::Others)]));
    CHECK_EQ(CursorColorOf(ColorPick{ColorSource::Custom, {{0.0f, 0.0f, 1.0f}}}, mob, p), 0xFF0000FFu);
    // A player has no aggression, toughness or claim to read: the cursor over one is the Targeting color.
    EntityState player;
    player.kind   = EntityKind::Player;
    player.target = TargetState::SubTarget;
    const uint32_t picking = ToArgb(p.target[static_cast<int>(TargetState::SubTarget)]);
    for (const ColorSource mobsOnly : {ColorSource::Aggression, ColorSource::Toughness, ColorSource::Claim})
        CHECK_EQ(CursorColorOf(ColorPick{mobsOnly}, player, p), picking);
    CHECK_EQ(CursorColorOf(ColorPick{ColorSource::Type}, player, p), ToArgb(p.type[static_cast<int>(TargetType::Player)]));
    // A pet is a mob in the game's eyes and has its own page, which offers no aggression either.
    EntityState pet = mob;
    pet.pet         = true;
    CHECK_EQ(CursorColorOf(ColorPick{ColorSource::Aggression}, pet, p), locked);
    // Every source the cursor offers is one it can resolve, and the game's name color is not among them.
    const std::vector<ColorSource> offered = CursorSources();
    CHECK(std::ranges::find(offered, ColorSource::Game) == offered.end());
    CHECK(std::ranges::find(offered, ColorSource::Aggression) != offered.end());
    CHECK(std::ranges::find(offered, ColorSource::Target) != offered.end());
}

TEST(a_rows_color_is_what_its_outline_pick_resolves_to)
{
    Settings s;
    EntityState aggressive;
    aggressive.kind   = EntityKind::Mob;
    aggressive.alive  = true;
    aggressive.nature = Nature::Aggressive;
    aggressive.row    = Category::WillAttack;
    CHECK_EQ(RowColor(aggressive, s), ToArgb(s.palette.aggression[static_cast<int>(Nature::Aggressive)]));
    EntityState claimed = aggressive;
    claimed.row         = Category::PartyClaim;
    claimed.claim       = ClaimState::Yours;
    claimed.classified  = Category::WillAttack;
    CHECK_EQ(RowColor(claimed, s), ToArgb(s.palette.aggression[static_cast<int>(Nature::Aggressive)])); // by nature by default
    s.Kind(PlateKind::Mobs).rows[CategoryIndex(Category::PartyClaim)].outline = OutlineMode::Shown;
    CHECK_EQ(RowColor(claimed, s), ToArgb(s.palette.claim[static_cast<int>(ClaimState::Yours)]));
    s.Kind(PlateKind::Mobs).rows[CategoryIndex(Category::WillAttack)].outlineColor = ColorPick{ColorSource::Custom, {{0.4f, 0.5f, 0.6f}}};
    CHECK_EQ(RowColor(aggressive, s), ToArgb(Color{{0.4f, 0.5f, 0.6f}}));
}

TEST(the_row_that_decides_follows_by_nature_down_the_chain)
{
    Settings s;
    EntityState claimed;
    claimed.kind       = EntityKind::Mob;
    claimed.alive      = true;
    claimed.nature     = Nature::Passive;
    claimed.classified = Category::WontAttack;
    claimed.row        = Category::PartyClaim;
    claimed.claim      = ClaimState::Yours;
    CHECK(OutlineModeOf(claimed, s) == OutlineMode::Disabled); // the Passive row
    CHECK_EQ(RowColor(claimed, s), ToArgb(s.palette.aggression[static_cast<int>(Nature::Passive)]));
    s.Kind(PlateKind::Mobs).rows[CategoryIndex(Category::WontAttack)].outline = OutlineMode::ByNature; // set by hand, unclamped
    CHECK(OutlineModeOf(claimed, s) == OutlineMode::Disabled);
    EntityState placeholder = claimed;
    placeholder.nature      = Nature::Placeholder;
    CHECK(OutlineModeOf(placeholder, s) == OutlineMode::Shown); // claim steps aside to the Placeholder row
    s.Kind(PlateKind::Mobs).rows[CategoryIndex(Category::Placeholder)].outline = OutlineMode::ByNature;
    CHECK(OutlineModeOf(placeholder, s) == OutlineMode::Disabled); // ... which steps aside to the Passive row, also by hand
}

TEST(a_placeholder_that_falls_through_takes_its_aggressions_color)
{
    Settings s;
    s.Kind(PlateKind::Mobs).rows[CategoryIndex(Category::Placeholder)].outline = OutlineMode::ByNature;
    EntityState funguar;
    funguar.kind       = EntityKind::Mob;
    funguar.alive      = true;
    funguar.nature     = Nature::Placeholder;
    funguar.classified = Category::WillAttack;
    funguar.row        = Category::Placeholder;
    CHECK(OutlineModeOf(funguar, s) == OutlineMode::Shown);
    CHECK_EQ(RowColor(funguar, s), ToArgb(s.palette.aggression[static_cast<int>(Nature::Aggressive)]));
}

TEST(a_players_row_decides_its_own_outline)
{
    Settings s;
    s.Kind(PlateKind::Players).rows[static_cast<int>(PlayerRow::Friend)].outline = OutlineMode::WhenTargeted;
    EntityState friendly;
    friendly.kind      = EntityKind::Player;
    friendly.playerRow = PlayerRow::Friend;
    CHECK(OutlineModeOf(friendly, s) == OutlineMode::WhenTargeted);
    friendly.playerRow = PlayerRow::Player;
    CHECK(OutlineModeOf(friendly, s) == OutlineMode::Disabled);
}

TEST(a_pet_is_the_entity_a_player_names_and_that_player_its_owner)
{
    // A dragoon at 1052 with a wyvern at 1840, a player with no pet out, and a summoner at 1054 with Carbuncle at 1841.
    const PetLink links[] = {{1052, EntityKind::Player, 1840}, {1053, EntityKind::Player, 0},
        {1054, EntityKind::Player, 1841}};
    const std::unordered_map<uint16_t, uint16_t> owners = PetOwners(links);
    CHECK_EQ(owners.size(), size_t{2});
    CHECK(owners.contains(1840) && owners.at(1840) == 1052);
    CHECK(owners.contains(1841) && owners.at(1841) == 1054);
    CHECK(!owners.contains(0));    // no pet out is not entity 0's owner
    CHECK(!owners.contains(1052)); // and an owner is no one's pet
    CHECK(PetOwners({}).empty());
}

TEST(only_a_player_owns_a_pet_and_the_first_to_name_it_keeps_it)
{
    // Two players naming entity 1840: the one listed first, the lower target index as the plugin lists them.
    const PetLink shared[] = {{1052, EntityKind::Player, 1840}, {1060, EntityKind::Player, 1840}};
    CHECK_EQ(PetOwners(shared).at(1840), 1052);
    const PetLink sharedBackwards[] = {{1060, EntityKind::Player, 1840}, {1052, EntityKind::Player, 1840}};
    CHECK_EQ(PetOwners(sharedBackwards).at(1840), 1060);
    // A mob's or an NPC's pet index names no pet of a player's, and neither takes one from the player after it.
    const PetLink others[] = {{300, EntityKind::Mob, 301}, {40, EntityKind::Npc, 41}, {300, EntityKind::Mob, 1840},
        {1052, EntityKind::Player, 1840}};
    const std::unordered_map<uint16_t, uint16_t> owners = PetOwners(others);
    CHECK_EQ(owners.size(), size_t{1});
    CHECK_EQ(owners.at(1840), 1052);
    // A player whose pet index is their own is nobody's pet.
    const PetLink own[] = {{1052, EntityKind::Player, 1052}};
    CHECK(PetOwners(own).empty());
    const PetLink none[] = {{1052, EntityKind::Player, 0}};
    CHECK(PetOwners(none).empty());
}

TEST(a_party_or_alliance_member_is_anyone_on_the_member_list_but_you)
{
    const uint32_t members[] = {0x00012345, 0x01098ABC, 0x0100BEEF}; // yours first, as the party list has them
    CHECK(InYourParty(0x01098ABC, false, members));
    CHECK(InYourParty(0x0100BEEF, false, members));
    CHECK(!InYourParty(0x00012345, true, members));  // you are not a member of your own party
    CHECK(!InYourParty(0x01011111, false, members)); // a stranger
    CHECK(!InYourParty(0x01098ABC, false, {}));
}

namespace
{
    // RDM 52 / BLM 26, as a check's general part lists them: both jobs, then both levels.
    InspectGeneral CheckedRedMage()
    {
        InspectGeneral g;
        g.mainJob   = 5;
        g.subJob    = 4;
        g.mainLevel = 52;
        g.subLevel  = 26;
        return g;
    }
}

TEST(a_players_jobs_come_from_the_game_then_the_party_list_then_the_last_check)
{
    const PlayerJobs own{1, 75, 13, 37}, member{4, 60, 3, 30};
    const InspectGeneral checked = CheckedRedMage();
    CHECK(KnownJobs(&own, &member, &checked, 0) == own);
    CHECK(KnownJobs(nullptr, &member, &checked, 0) == member);
    CHECK(KnownJobs(nullptr, nullptr, &checked, 0) == (PlayerJobs{5, 52, 4, 26}));
    CHECK(!KnownJobs(nullptr, nullptr, nullptr, 0).has_value());
    // A level the server sent, last: it has no job.
    CHECK(KnownJobs(nullptr, nullptr, nullptr, 75) == (PlayerJobs{0, 75, 0, 0}));
    CHECK(KnownJobs(nullptr, nullptr, &checked, 75) == (PlayerJobs{5, 52, 4, 26}));
    CHECK(KnownJobs(nullptr, &member, nullptr, 75) == member);
}

TEST(a_players_level_the_server_sends_shows_with_auto_check_on_unless_they_are_anonymous)
{
    PlayerStatus open, hidden;
    hidden.anonymous = true;
    CHECK_EQ(int{SeenLevel(true, 75, &open)}, 75);
    CHECK_EQ(int{SeenLevel(false, 75, &open)}, 0);
    CHECK_EQ(int{SeenLevel(true, 75, &hidden)}, 0);
    CHECK_EQ(int{SeenLevel(true, 75, nullptr)}, 0); // whether they are anonymous is not known yet
    CHECK_EQ(int{SeenLevel(true, 0, &open)}, 0);
}

TEST(a_main_job_of_0_knows_nothing_so_the_next_source_is_asked)
{
    const PlayerJobs unknown{}, member{4, 60, 3, 30};
    const InspectGeneral checked = CheckedRedMage(), anonymous{};
    CHECK(KnownJobs(&unknown, &member, nullptr, 0) == member);                      // you, while zoning
    CHECK(KnownJobs(nullptr, &unknown, &checked, 0) == (PlayerJobs{5, 52, 4, 26})); // a member in another zone
    CHECK(!KnownJobs(nullptr, nullptr, &anonymous, 0).has_value());                 // an anonymous player's check
    CHECK(!KnownJobs(&unknown, &unknown, &anonymous, 0).has_value());
}
