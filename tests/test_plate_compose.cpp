#include "plate_compose.h"
#include "look.h"
#include "test.h"

#include <algorithm>
#include <cstdio>
#include <string>

using namespace headsup;

namespace
{
    // The first line's items joined, texts as they are and icon runs as their count in brackets.
    std::string Rendered(const ComposedLine& line)
    {
        std::string out;
        for (const PlateItem& item : line.items)
            out += item.kind == ItemKind::Text ? item.text : item.kind == ItemKind::Space ? std::string(static_cast<size_t>(item.spaces), ' ') : "[" + std::to_string(item.icons.size()) + "]";
        return out;
    }
    const MobRecord kQuadav{17190918, "Young Quadav", 17, 20, kMobAggressive | kMobLink, 300, kDetectSight | kDetectHearing, 0, 0, 0, 0};
    ActorInfo AggressiveMob()
    {
        ActorInfo info;
        info.index    = 6;
        info.serverId = kQuadav.id;
        info.mob      = &kQuadav;
        std::snprintf(info.name, sizeof(info.name), "%s", kQuadav.name);
        info.state.kind      = EntityKind::Mob;
        info.state.alive     = true;
        info.state.nature    = Nature::Aggressive;
        info.state.row       = Category::WillAttack;
        info.state.toughness = LabelShade::EvenMatch;
        return info;
    }
    ActorInfo LinkshellPlayer()
    {
        ActorInfo info;
        info.index    = 1052;
        info.serverId = 0x00012345;
        std::snprintf(info.name, sizeof(info.name), "Chadmage");
        info.state.kind  = EntityKind::Player;
        info.state.alive = true;
        PlayerStatus status;
        status.linkshell     = true;
        status.seekingParty  = true;
        status.linkshellArgb = 0xFF0000FF;
        info.status          = status;
        info.linkshellArgb   = status.linkshellArgb;
        return info;
    }
    const PlayerState kPlayer{20, false, false};
    constexpr uint32_t kGame = 0xFFC0C0C0;
}

TEST(the_default_mob_design_composes_todays_three_lines)
{
    const Settings s;
    const ComposedPlate plate = ComposePlate(AggressiveMob(), kPlayer, s, kGame);
    CHECK_EQ(plate.lines.size(), size_t{3});
    CHECK_EQ(Rendered(plate.lines[0]), "[4]"); // aggro, link, sight, sound
    CHECK_EQ(Rendered(plate.lines[1]), "Lv 17-20 EP-EM");
    CHECK_EQ(Rendered(plate.lines[2]), "Young Quadav");
    CHECK_EQ(plate.lines[1].look.color, ToArgb(s.palette.toughness[static_cast<int>(LabelShade::EvenMatch)]));
    CHECK_EQ(plate.lines[2].look.color, kGame);
    CHECK_EQ(plate.lines[2].look.glowColor, ToArgb(s.palette.aggression[static_cast<int>(Nature::Aggressive)]));
    CHECK_EQ(plate.ornamentAbove, 2);
    CHECK_EQ(plate.ornamentColor, ToArgb(s.palette.aggression[static_cast<int>(Nature::Aggressive)]));
    CHECK_EQ(plate.lines[2].look.textSize, 15.0f);
    CHECK_EQ(plate.lines[1].look.textSize, BasePixels(s, 87));
    CHECK_EQ(plate.lines[0].look.iconSize, BasePixels(s, 107));
    CHECK_EQ(plate.lines[0].source, 0);
}

TEST(the_default_player_design_puts_the_icons_before_the_name)
{
    const Settings s;
    const ComposedPlate plate = ComposePlate(LinkshellPlayer(), kPlayer, s, kGame);
    CHECK_EQ(plate.lines.size(), size_t{1});
    CHECK_EQ(Rendered(plate.lines[0]), "[2]Chadmage"); // linkshell and seeking join in one run; the rest render nothing
    CHECK_EQ(plate.lines[0].items.size(), size_t{2});
    const PlateItem& icons = plate.lines[0].items[0];
    CHECK(icons.icons[0].icon == Icon::Linkshell);
    CHECK_EQ(icons.icons[0].tint, 0xFF0000FFu);
    CHECK(icons.icons[1].icon == Icon::Invite);
    CHECK(plate.lines[0].items[1].name);
    CHECK(!icons.name);
}

TEST(text_around_a_tag_merges_into_one_run_and_spaces_beside_icons_become_gaps)
{
    Settings s;
    s.Kind(PlateKind::Mobs).lines[0].text = "$detect - $name ($level)";
    const ComposedPlate plate = ComposePlate(AggressiveMob(), kPlayer, s, kGame);
    const ComposedLine& line = plate.lines[0];
    CHECK_EQ(line.items.size(), size_t{3});
    CHECK(line.items[0].kind == ItemKind::Icons);
    CHECK(line.items[1].kind == ItemKind::Space);
    CHECK_EQ(line.items[1].spaces, 1);
    CHECK(line.items[2].kind == ItemKind::Text);
    CHECK_EQ(line.items[2].text, "- Young Quadav (17-20)");
}

TEST(lines_with_nothing_to_show_are_skipped)
{
    Settings s;
    KindSettings& mobs = s.Kind(PlateKind::Mobs);
    mobs.lines[0].text = "   ";
    mobs.lines[1].text = "$ph$weak"; // not a placeholder, no weaknesses
    mobs.lines[2].text = "";
    CHECK(ComposePlate(AggressiveMob(), kPlayer, s, kGame).lines.empty());
    mobs.lines[2].text = "$name";
    CHECK_EQ(ComposePlate(AggressiveMob(), kPlayer, s, kGame).lines.size(), size_t{1});
}

TEST(hidden_vanilla_and_untargeted_when_targeted_rows_compose_nothing)
{
    for (const PlateMode mode : {PlateMode::Hidden, PlateMode::Vanilla, PlateMode::WhenTargeted})
    {
        Settings s;
        s.Kind(PlateKind::Mobs).rows[CategoryIndex(Category::WillAttack)].plate = mode;
        CHECK(ComposePlate(AggressiveMob(), kPlayer, s, kGame).lines.empty());
    }
    Settings s;
    s.Kind(PlateKind::Mobs).rows[CategoryIndex(Category::WillAttack)].plate = PlateMode::WhenTargeted;
    ActorInfo targeted  = AggressiveMob();
    targeted.state.target = TargetState::Targeted;
    CHECK_EQ(ComposePlate(targeted, kPlayer, s, kGame).lines.size(), size_t{3});
}

TEST(expanding_rows_and_dead_mobs_keep_the_name_alone)
{
    Settings s;
    s.Kind(PlateKind::Mobs).rows[CategoryIndex(Category::WillAttack)].plate = PlateMode::Expanding;
    s.Kind(PlateKind::Mobs).lines[2].text = "$name ($level)";
    const ComposedPlate untargeted = ComposePlate(AggressiveMob(), kPlayer, s, kGame);
    CHECK_EQ(untargeted.lines.size(), size_t{1});
    CHECK_EQ(Rendered(untargeted.lines[0]), "Young Quadav");
    ActorInfo targeted    = AggressiveMob();
    targeted.state.target = TargetState::Targeted;
    CHECK_EQ(ComposePlate(targeted, kPlayer, s, kGame).lines.size(), size_t{3});
    Settings d;
    ActorInfo dead  = AggressiveMob();
    dead.state.alive = false;
    const ComposedPlate gone = ComposePlate(dead, kPlayer, d, kGame);
    CHECK_EQ(gone.lines.size(), size_t{1});
    CHECK_EQ(Rendered(gone.lines[0]), "Young Quadav");
    CHECK_EQ(gone.ornamentAbove, kNoOrnament);
}

TEST(the_ornament_needs_both_of_the_lines_it_sits_between)
{
    Settings s;
    KindSettings& mobs = s.Kind(PlateKind::Mobs);
    mobs.lines[1].text = "";
    CHECK_EQ(ComposePlate(AggressiveMob(), kPlayer, s, kGame).ornamentAbove, kNoOrnament);
    mobs.ornament.place = OrnamentPlace::BetweenLines1And2;
    mobs.lines[1].text  = "Lv $level";
    const ComposedPlate plate = ComposePlate(AggressiveMob(), kPlayer, s, kGame);
    CHECK_EQ(plate.ornamentAbove, 1); // the composed index of design line 2
    mobs.ornament.on = false;
    CHECK_EQ(ComposePlate(AggressiveMob(), kPlayer, s, kGame).ornamentAbove, kNoOrnament);
}

TEST(a_line_takes_the_style_of_the_line_it_syncs_with)
{
    Settings s;
    KindSettings& mobs = s.Kind(PlateKind::Mobs);
    mobs.lines[2].textSize = 200;
    mobs.lines[2].bold     = true;
    mobs.lines[1].syncWith = 2;
    mobs.lines[1].textSize = 50;
    const ComposedPlate plate = ComposePlate(AggressiveMob(), kPlayer, s, kGame);
    CHECK_EQ(plate.lines[1].look.textSize, BasePixels(s, 200));
    CHECK(plate.lines[1].look.bold);
    CHECK_EQ(Rendered(plate.lines[1]), "Lv 17-20 EP-EM"); // its own text
}

TEST(a_lines_conditions_can_hide_it)
{
    Settings s;
    s.Kind(PlateKind::Mobs).lines[1].show = LineShow{false, ConditionBit(ShowCondition::YourClaim)};
    CHECK_EQ(ComposePlate(AggressiveMob(), kPlayer, s, kGame).lines.size(), size_t{2});
    ActorInfo claimed   = AggressiveMob();
    claimed.state.claim = ClaimState::Yours;
    claimed.state.row   = Category::PartyClaim;
    CHECK_EQ(ComposePlate(claimed, kPlayer, s, kGame).lines.size(), size_t{3});
}

TEST(each_mob_tag_renders_its_text_or_icons)
{
    Settings s;
    KindSettings& mobs = s.Kind(PlateKind::Mobs);
    mobs.lines[0].text = "$id $serverid $ph";
    mobs.lines[1].text = "$weak|$resist";
    mobs.lines[2].text = "$con";
    ActorInfo ph    = AggressiveMob();
    ph.placeholder  = true;
    const ComposedPlate plate = ComposePlate(ph, kPlayer, s, kGame);
    CHECK_EQ(plate.lines.size(), size_t{3});
    CHECK_EQ(Rendered(plate.lines[0]), "[006] [17190918] [PH]");
    CHECK_EQ(Rendered(plate.lines[1]), "|"); // neutral resistances: the tags render nothing, the literal text stays
    CHECK_EQ(Rendered(plate.lines[2]), "EP-EM");
    CHECK_EQ(Rendered(ComposePlate(AggressiveMob(), kPlayer, s, kGame).lines[0]), "[006] [17190918]");
}

TEST(detection_icons_follow_the_detection_filter)
{
    Settings s;
    s.detectionShown = DetectionBit(DetectionIcon::Link);
    CHECK_EQ(Rendered(ComposePlate(AggressiveMob(), kPlayer, s, kGame).lines[0]), "[1]");
}

TEST(player_tags_render_jobs_and_levels_when_known)
{
    Settings s;
    s.Kind(PlateKind::Players).lines[2].text = "$name $job $level";
    ActorInfo known = LinkshellPlayer();
    known.jobs      = PlayerJobs{1, 75, 2, 37};
    CHECK_EQ(Rendered(ComposePlate(known, kPlayer, s, kGame).lines[0]), "Chadmage WAR/MNK 75");
    CHECK_EQ(Rendered(ComposePlate(LinkshellPlayer(), kPlayer, s, kGame).lines[0]), "Chadmage");
}

TEST(the_hook_keeps_vanilla_names_hides_hidden_ones_and_replaces_the_rest)
{
    Settings s;
    const EntityState mob = AggressiveMob().state;
    CHECK(HookActionFor(mob, s, false) == HookAction::Replace);
    s.Kind(PlateKind::Mobs).rows[CategoryIndex(Category::WillAttack)].plate = PlateMode::Vanilla;
    CHECK(HookActionFor(mob, s, false) == HookAction::Keep);
    s.Kind(PlateKind::Mobs).rows[CategoryIndex(Category::WillAttack)].plate = PlateMode::Hidden;
    CHECK(HookActionFor(mob, s, false) == HookAction::Hide);
    s.Kind(PlateKind::Mobs).rows[CategoryIndex(Category::WillAttack)].plate = PlateMode::WhenTargeted;
    CHECK(HookActionFor(mob, s, false) == HookAction::Hide);
    EntityState targeted = mob;
    targeted.target      = TargetState::Targeted;
    CHECK(HookActionFor(targeted, s, false) == HookAction::Replace);
    s.nameplates = false;
    CHECK(HookActionFor(targeted, s, false) == HookAction::Keep);
    CHECK(HookActionFor(targeted, s, true) == HookAction::Hide);
    s.nameplates = true;
    s.enabled    = false;
    CHECK(HookActionFor(targeted, s, false) == HookAction::Keep);
    CHECK(HookActionFor(targeted, s, true) == HookAction::Hide); // screenshot mode hides names even after /hu off
}

TEST(spaces_at_the_ends_of_a_line_or_beside_text_are_dropped)
{
    Settings s;
    s.Kind(PlateKind::Mobs).lines[2].text = "  $name  ";
    const ComposedPlate plate = ComposePlate(AggressiveMob(), kPlayer, s, kGame);
    CHECK_EQ(plate.lines[2].items.size(), size_t{1});
    CHECK_EQ(plate.lines[2].items[0].text, "Young Quadav");
    s.Kind(PlateKind::Mobs).lines[2].text = "$detect  ";
    const ComposedPlate icons = ComposePlate(AggressiveMob(), kPlayer, s, kGame);
    CHECK_EQ(icons.lines[2].items.size(), size_t{1});
    CHECK(icons.lines[2].items[0].kind == ItemKind::Icons);
}

namespace
{
    ComposedPlate Compose(const ActorInfo& info, const Settings& s) { return ComposePlate(info, kPlayer, s, kGame); }
    ActorInfo PlayerWith(const PlayerStatus& status)
    {
        ActorInfo info = LinkshellPlayer();
        info.status    = status;
        return info;
    }
    ActorInfo PlainPlayer()
    {
        ActorInfo info = LinkshellPlayer();
        info.status.reset();
        return info;
    }
}

TEST(a_synced_line_follows_the_chain_to_the_style_it_shows)
{
    Settings s;
    KindSettings& mobs = s.Kind(PlateKind::Mobs);
    mobs.lines[0].syncWith = 1;
    mobs.lines[1].syncWith = 2;
    mobs.lines[1].textSize = 50;
    mobs.lines[2].bold     = true;
    mobs.lines[2].textSize = 200;
    mobs.lines[0].text     = "$id";
    const ComposedPlate plate = Compose(AggressiveMob(), s);
    CHECK_EQ(plate.lines.size(), size_t{3});
    for (int i = 0; i < 2; ++i)
    {
        CHECK_EQ(plate.lines[i].look.textSize, BasePixels(s, 200));
        CHECK(plate.lines[i].look.bold);
    }
    CHECK_EQ(Rendered(plate.lines[0]), "[006]");
    CHECK_EQ(Rendered(plate.lines[1]), "Lv 17-20 EP-EM");
}

TEST(a_sync_cycle_leaves_each_line_its_own_style)
{
    Settings s;
    KindSettings& mobs = s.Kind(PlateKind::Mobs);
    mobs.lines[0].syncWith = 1;
    mobs.lines[1].syncWith = 0;
    mobs.lines[0].textSize = 60;
    mobs.lines[1].textSize = 90;
    const ComposedPlate plate = Compose(AggressiveMob(), s);
    CHECK_EQ(plate.lines.size(), size_t{3});
    CHECK_EQ(plate.lines[0].look.textSize, BasePixels(s, 60));
    CHECK_EQ(plate.lines[1].look.textSize, BasePixels(s, 90));
}

TEST(a_tag_that_renders_nothing_takes_one_space_with_it)
{
    Settings s;
    KindSettings& mobs = s.Kind(PlateKind::Mobs);
    mobs.lines[2].text = "$name $ph $id";
    CHECK_EQ(Rendered(Compose(AggressiveMob(), s).lines[2]), "Young Quadav [006]");
    ActorInfo ph   = AggressiveMob();
    ph.placeholder = true;
    CHECK_EQ(Rendered(Compose(ph, s).lines[2]), "Young Quadav [PH] [006]");
    mobs.lines[2].text = "$name  $id";
    CHECK_EQ(Rendered(Compose(AggressiveMob(), s).lines[2]), "Young Quadav  [006]");
    mobs.lines[2].text = "a $ph$weak b";
    CHECK_EQ(Rendered(Compose(AggressiveMob(), s).lines[2]), "a b");
    mobs.lines[2].text = "$ph $name";
    CHECK_EQ(Rendered(Compose(AggressiveMob(), s).lines[2]), "Young Quadav");
    mobs.lines[2].text = "$detect $weak $name";
    const ComposedPlate gapPlate = Compose(AggressiveMob(), s);
    const ComposedLine& gap = gapPlate.lines[2];
    CHECK_EQ(gap.items.size(), size_t{3});
    CHECK(gap.items[0].kind == ItemKind::Icons);
    CHECK(gap.items[1].kind == ItemKind::Space);
    CHECK_EQ(gap.items[1].spaces, 1);
    CHECK(gap.items[2].kind == ItemKind::Text);
}

TEST(each_player_icon_tag_draws_its_own_icon_only_when_its_flag_is_set)
{
    struct Case
    {
        const char* tag;
        Icon icon;
        bool PlayerStatus::* flag;
    };
    const Case cases[] = {{"$gm", Icon::Gm, &PlayerStatus::gm}, {"$mentor", Icon::Mentor, &PlayerStatus::mentor},
        {"$new", Icon::NewAdventurer, &PlayerStatus::newAdventurer}, {"$sync", Icon::LevelSync, &PlayerStatus::levelSync},
        {"$away", Icon::Away, &PlayerStatus::away}, {"$linkshell", Icon::Linkshell, &PlayerStatus::linkshell},
        {"$bazaar", Icon::Bazaar, &PlayerStatus::bazaar}, {"$seeking", Icon::Invite, &PlayerStatus::seekingParty}};
    for (const Case& c : cases)
    {
        Settings s;
        s.Kind(PlateKind::Players).lines[2].text = c.tag;
        PlayerStatus status;
        status.*c.flag = true;
        const ComposedPlate on = Compose(PlayerWith(status), s);
        CHECK_EQ(on.lines.size(), size_t{1});
        CHECK_EQ(on.lines[0].items.size(), size_t{1});
        CHECK_EQ(on.lines[0].items[0].icons.size(), size_t{1});
        const IconDraw& draw = on.lines[0].items[0].icons[0];
        CHECK(draw.icon == c.icon);
        CHECK_EQ(draw.tint, c.icon == Icon::Linkshell ? 0xFF0000FFu : kWhite);
        CHECK(Compose(PlayerWith(PlayerStatus{}), s).lines.empty());
        CHECK(Compose(PlainPlayer(), s).lines.empty());
    }
}

TEST(each_kind_composes_from_its_own_design)
{
    Settings s;
    s.Kind(PlateKind::Pets).lines[2].text    = "$name ($owner)";
    s.Kind(PlateKind::Self).lines[2].text    = "me $name";
    s.Kind(PlateKind::Players).lines[2].text = "you $name";
    s.Kind(PlateKind::Npcs).lines[2].text    = "npc $name";
    ActorInfo pet = AggressiveMob();
    pet.state.pet = true;
    std::snprintf(pet.ownerName, sizeof(pet.ownerName), "Carrott");
    const ComposedPlate pets = Compose(pet, s);
    CHECK_EQ(pets.lines.size(), size_t{1});
    CHECK_EQ(Rendered(pets.lines[0]), "Young Quadav (Carrott)");
    ActorInfo self = PlainPlayer();
    self.state.self = true;
    CHECK_EQ(Rendered(Compose(self, s).lines[0]), "me Chadmage");
    CHECK_EQ(Rendered(Compose(PlainPlayer(), s).lines[0]), "you Chadmage");
    ActorInfo npc = AggressiveMob();
    npc.state     = EntityState{};
    npc.state.kind = EntityKind::Npc;
    npc.state.alive = true;
    std::snprintf(npc.name, sizeof(npc.name), "Shami");
    CHECK_EQ(Rendered(Compose(npc, s).lines[0]), "npc Shami");
    s.Kind(PlateKind::Players).lines[2].text = "$name$con$id$detect";
    CHECK_EQ(Rendered(Compose(PlainPlayer(), s).lines[0]), "Chadmage");
    s.Kind(PlateKind::Mobs).lines[2].text = "$name$job$linkshell";
    CHECK_EQ(Rendered(Compose(AggressiveMob(), s).lines[2]), "Young Quadav");
}

TEST(a_checked_mob_shows_the_exact_level_and_con)
{
    ActorInfo info = AggressiveMob();
    info.checked   = CheckResult{19, Con::DecentChallenge};
    CHECK_EQ(Rendered(Compose(info, Settings{}).lines[1]), "Lv 19 DC");
}

TEST(jobs_show_alone_when_there_is_no_sub_job_and_nothing_when_unknown)
{
    Settings s;
    s.Kind(PlateKind::Players).lines[2].text = "$name $job $level";
    ActorInfo info = LinkshellPlayer();
    info.jobs      = PlayerJobs{1, 75, 0, 0};
    CHECK_EQ(Rendered(Compose(info, s).lines[0]), "Chadmage WAR 75");
    info.jobs = PlayerJobs{0, 0, 0, 0};
    CHECK_EQ(Rendered(Compose(info, s).lines[0]), "Chadmage");
}

TEST(a_dead_player_keeps_its_whole_plate)
{
    ActorInfo info   = LinkshellPlayer();
    info.state.alive = false;
    CHECK_EQ(Rendered(Compose(info, Settings{}).lines[0]), "[2]Chadmage");
}

TEST(the_ornament_follows_the_composed_index_when_a_line_above_is_skipped)
{
    Settings s;
    s.Kind(PlateKind::Mobs).lines[0].text = "";
    const ComposedPlate plate = Compose(AggressiveMob(), s);
    CHECK_EQ(plate.lines.size(), size_t{2});
    CHECK_EQ(plate.ornamentAbove, 1);
    Settings d;
    d.Kind(PlateKind::Mobs).lines[1].text = "$name";
    ActorInfo dead   = AggressiveMob();
    dead.state.alive = false;
    const ComposedPlate gone = Compose(dead, d);
    CHECK_EQ(gone.lines.size(), size_t{2});
    CHECK_EQ(gone.ornamentAbove, 1); // two name lines still have the rule between them
}

// Goblin Bounty Hunter (17199648) is weak to light by rank, which the Weak group shows as a rank badge.
TEST(badge_tags_draw_what_the_icon_groups_and_badge_functions_give)
{
    const MobRecord* hunter = MobById(17199648);
    CHECK(hunter != nullptr);
    ActorInfo info = AggressiveMob();
    info.mob       = hunter;
    Settings s;
    KindSettings& mobs = s.Kind(PlateKind::Mobs);
    mobs.lines[0].text = "$weak";
    mobs.lines[1].text = "$resist$status$immune";
    for (StatusBadgeRule& rule : s.statusBadges) rule.weakRank = kMaxWeakRank; // so its status weaknesses show
    BadgeRules rules{.damage = true, .statusRanks = true, .immunities = true, .elementWeakRank = s.elementWeakRank,
        .elementResistRank = s.elementResistRank, .maxElements = s.maxElements};
    std::ranges::copy(s.statusBadges, rules.status);
    const auto expected = [&](bool ranks, std::initializer_list<BadgeGroup> groups)
    {
        std::vector<IconDraw> out;
        for (const BadgeGroup group : groups)
        {
            const InfoIconSet set = InfoIconsOf(&ResistancesOf(*hunter), rules, group);
            for (int i = 0; i < set.count; ++i)
            {
                IconDraw draw;
                draw.icon       = set.icons[i].icon;
                draw.badge      = BadgeText(set.icons[i], ranks);
                draw.badgeColor = BadgeColor(set.icons[i].badge, s);
                out.push_back(draw);
            }
        }
        return out;
    };
    const auto composed = [&](const ComposedLine& line)
    {
        CHECK_EQ(line.items.size(), size_t{1});
        return line.items[0].icons;
    };
    const ComposedPlate plate = Compose(info, s);
    CHECK_EQ(plate.lines.size(), size_t{3});
    const std::vector<IconDraw> weak = expected(false, {BadgeGroup::Weak});
    CHECK(!weak.empty());
    CHECK(composed(plate.lines[0]) == weak);
    const std::vector<IconDraw> rest = expected(false, {BadgeGroup::Resist, BadgeGroup::Status, BadgeGroup::Immune});
    CHECK(!rest.empty());
    CHECK(composed(plate.lines[1]) == rest);
    s.showRanks = true;
    const std::vector<IconDraw> ranked = composed(Compose(info, s).lines[0]);
    CHECK(ranked == expected(true, {BadgeGroup::Weak}));
    CHECK(ranked != weak);
}
