#include "plate_compose.h"
#include "look.h"
#include "test.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace headsup;

namespace
{
    // The first line's items joined, texts as they are and icon runs as their count in brackets.
    std::string Rendered(const ComposedLine& line)
    {
        std::string out;
        for (const PlateItem& item : line.items)
            out += item.kind == ItemKind::Text ? WithDashes(item.text) : item.kind == ItemKind::Space ? std::string(static_cast<size_t>(item.spaces), ' ') : "[" + std::to_string(item.icons.size()) + "]";
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
        info.state.classified = Category::WillAttack;
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
    const PlayerState kPlayer{20, false};
    constexpr uint32_t kGame = 0xFFC0C0C0;
}

TEST(the_default_mob_design_composes_todays_three_lines)
{
    const Settings s;
    const ComposedPlate plate = ComposePlate(AggressiveMob(), kPlayer, s, kGame);
    CHECK_EQ(plate.lines.size(), size_t{3});
    CHECK_EQ(Rendered(plate.lines[0]), "[3]"); // link, sight, sound
    CHECK_EQ(Rendered(plate.lines[1]), "Lv 17-20 EP-EM");
    CHECK_EQ(Rendered(plate.lines[2]), "Young Quadav");
    CHECK_EQ(plate.lines[1].look.color, ToArgb(s.palette.toughness[static_cast<int>(LabelShade::EvenMatch)]));
    CHECK_EQ(plate.lines[2].look.color, kGame);
    CHECK_EQ(plate.lines[2].look.glowColor, ToArgb(s.palette.aggression[static_cast<int>(Nature::Aggressive)]));
    CHECK_EQ(OrnamentLineOf(plate).line, 2);
    CHECK_EQ(plate.ornamentColor, ToArgb(s.palette.aggression[static_cast<int>(Nature::Aggressive)]));
    CHECK_EQ(plate.lines[2].look.textSize, 15.0f);
    CHECK_EQ(plate.lines[1].look.textSize, BasePixels(s, 87));
    CHECK_EQ(plate.lines[0].look.iconSize, 16.0f); // the whole of the base icon size
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
    CHECK_EQ(WithDashes(line.items[2].text), "- Young Quadav (17-20)");
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

TEST(a_dead_mob_keeps_its_name_alone)
{
    Settings d;
    d.Kind(PlateKind::Mobs).lines[2].text = "$name ($level)";
    CHECK_EQ(ComposePlate(AggressiveMob(), kPlayer, d, kGame).lines.size(), size_t{3});
    ActorInfo dead  = AggressiveMob();
    dead.state.alive = false;
    const ComposedPlate gone = ComposePlate(dead, kPlayer, d, kGame);
    CHECK_EQ(gone.lines.size(), size_t{1});
    CHECK_EQ(Rendered(gone.lines[0]), "Young Quadav");
    CHECK_EQ(OrnamentLineOf(gone).line, kNoOrnament);
    // The Dead row decides whether it has a nameplate at all.
    dead.state.row = Category::Dead;
    d.Kind(PlateKind::Mobs).rows[CategoryIndex(Category::Dead)].plate = PlateMode::Hidden;
    CHECK(ComposePlate(dead, kPlayer, d, kGame).lines.empty());
    CHECK(HookActionFor(dead.state, d, false, false) == HookAction::Hide);
}

TEST(a_dead_entitys_text_is_dark_grey_whatever_its_colors)
{
    Settings s;
    s.Kind(PlateKind::Mobs).lines[2].color    = ColorPick{ColorSource::Custom, {{1.0f, 0.0f, 0.0f}}};
    s.Kind(PlateKind::Players).lines[2].color = ColorPick{ColorSource::Type};
    s.Kind(PlateKind::Players).lines[1].text  = "$job";
    ActorInfo mob   = AggressiveMob();
    const uint32_t living = ComposePlate(mob, kPlayer, s, kGame).lines.back().look.color;
    mob.state.alive = false;
    const ComposedPlate dead = ComposePlate(mob, kPlayer, s, kGame);
    CHECK(living != kDeadTextArgb && dead.lines.back().look.color == kDeadTextArgb);
    CHECK_EQ(dead.lines.back().look.shadowColor, kBlack); // its shadow is as it was
    // A dead player keeps every line, each in the grey.
    ActorInfo player   = LinkshellPlayer();
    player.jobs        = PlayerJobs{1, 75, 2, 37};
    player.state.alive = false;
    const ComposedPlate fallen = ComposePlate(player, kPlayer, s, kGame);
    CHECK_EQ(fallen.lines.size(), size_t{2});
    for (const ComposedLine& line : fallen.lines)
        CHECK_EQ(line.look.color, kDeadTextArgb);
    // An NPC has no life to lose: whatever its health reads, its colors are its own.
    ActorInfo npc;
    npc.state.kind = EntityKind::Npc;
    std::snprintf(npc.name, sizeof(npc.name), "Signpost");
    CHECK_EQ(ComposePlate(npc, kPlayer, s, kGame).lines.back().look.color, kGame);
}

TEST(in_a_cutscene_or_a_dialog_every_plate_keeps_its_name_alone)
{
    const Settings s; // the default designs
    PlayerState watching = kPlayer;
    watching.inEvent     = true;
    ActorInfo targeted    = AggressiveMob();
    targeted.state.target = TargetState::Targeted;
    CHECK_EQ(ComposePlate(targeted, kPlayer, s, kGame).lines.size(), size_t{3});
    const ComposedPlate mob = ComposePlate(targeted, watching, s, kGame);
    CHECK_EQ(mob.lines.size(), size_t{1});
    CHECK_EQ(Rendered(mob.lines[0]), "Young Quadav");
    CHECK_EQ(OrnamentLineOf(mob).line, kNoOrnament);
    // A player's icons go with the rest: the linkshell and seeking-party marks beside the name.
    CHECK(Rendered(ComposePlate(LinkshellPlayer(), kPlayer, s, kGame).lines.back()) != "Chadmage");
    const ComposedPlate player = ComposePlate(LinkshellPlayer(), watching, s, kGame);
    CHECK_EQ(player.lines.size(), size_t{1});
    CHECK_EQ(Rendered(player.lines[0]), "Chadmage");
}

TEST(a_dead_mobs_name_does_not_glow)
{
    const Settings s; // the default design: the name glows at full strength
    CHECK_EQ(ComposePlate(AggressiveMob(), kPlayer, s, kGame).lines[2].look.glow, 1.0f);
    ActorInfo dead   = AggressiveMob();
    dead.state.alive = false;
    const ComposedPlate gone = ComposePlate(dead, kPlayer, s, kGame);
    CHECK_EQ(gone.lines.size(), size_t{1});
    CHECK_EQ(gone.lines[0].look.glow, 0.0f);
    CHECK_EQ(gone.lines[0].look.shadow, kShadowUnit); // the rest of its look stays
    // A dead player's name keeps its glow.
    Settings glowing;
    glowing.Kind(PlateKind::Players).lines[2].glow = 150;
    ActorInfo player   = LinkshellPlayer();
    player.state.alive = false;
    CHECK_EQ(ComposePlate(player, kPlayer, glowing, kGame).lines[0].look.glow, 1.5f);
}

TEST(a_mob_your_party_has_claimed_can_lose_its_glow)
{
    Settings s; // the default design's name glows at full strength; a second line glows here too
    s.Kind(PlateKind::Mobs).lines[1].glow = 50;
    ActorInfo yours   = AggressiveMob();
    yours.state.claim = ClaimState::Yours;
    yours.state.row   = Category::PartyClaim;
    const ComposedPlate fought = ComposePlate(yours, kPlayer, s, kGame);
    CHECK_EQ(fought.lines.size(), size_t{3});
    CHECK_EQ(fought.lines[1].look.glow, 0.0f);
    CHECK_EQ(fought.lines[2].look.glow, 0.0f);
    CHECK_EQ(fought.lines[2].look.shadow, kShadowUnit); // the rest of its look stays
    // Another party's mob, and one nobody has touched, keep theirs.
    ActorInfo theirs   = AggressiveMob();
    theirs.state.claim = ClaimState::Others;
    theirs.state.row   = Category::OtherClaim;
    CHECK_EQ(ComposePlate(theirs, kPlayer, s, kGame).lines[2].look.glow, 1.0f);
    CHECK_EQ(ComposePlate(AggressiveMob(), kPlayer, s, kGame).lines[1].look.glow, 0.5f);
    // Each line has the switch for itself.
    s.Kind(PlateKind::Mobs).lines[1].glowOffInCombat = false;
    const ComposedPlate kept = ComposePlate(yours, kPlayer, s, kGame);
    CHECK_EQ(kept.lines[1].look.glow, 0.5f);
    CHECK_EQ(kept.lines[2].look.glow, 0.0f);
    // A line that takes its style from another takes that line's switch with it.
    s.Kind(PlateKind::Mobs).lines[2].syncWith = 1;
    CHECK_EQ(ComposePlate(yours, kPlayer, s, kGame).lines[2].look.glow, 0.5f);
    // A player's line has it too, off by default: with it on, their name stops glowing while their weapon is out.
    Settings glowing;
    LineDesign& name = glowing.Kind(PlateKind::Players).lines[2];
    name.glow        = 150;
    ActorInfo fighter      = LinkshellPlayer();
    fighter.state.engaged  = true;
    CHECK_EQ(ComposePlate(fighter, kPlayer, glowing, kGame).lines[0].look.glow, 1.5f);
    name.glowOffInCombat = true;
    CHECK_EQ(ComposePlate(fighter, kPlayer, glowing, kGame).lines[0].look.glow, 0.0f);
    fighter.state.engaged = false;
    CHECK_EQ(ComposePlate(fighter, kPlayer, glowing, kGame).lines[0].look.glow, 1.5f);
}

TEST(the_ornament_needs_both_of_the_lines_it_sits_between)
{
    Settings s;
    KindSettings& mobs = s.Kind(PlateKind::Mobs);
    mobs.lines[1].text = "";
    CHECK_EQ(OrnamentLineOf(ComposePlate(AggressiveMob(), kPlayer, s, kGame)).line, kNoOrnament);
    mobs.ornament.place = OrnamentPlace::AboveMiddle;
    mobs.lines[1].text  = "Lv $level";
    const ComposedPlate plate = ComposePlate(AggressiveMob(), kPlayer, s, kGame);
    CHECK_EQ(OrnamentLineOf(plate).line, 1); // the composed index of the middle line, which it stands over
    CHECK(!OrnamentLineOf(plate).under);
    mobs.ornament.place = OrnamentPlace::Disabled;
    CHECK_EQ(OrnamentLineOf(ComposePlate(AggressiveMob(), kPlayer, s, kGame)).line, kNoOrnament);
}

TEST(an_ornament_at_the_top_or_the_bottom_goes_with_whichever_line_is_first_or_last)
{
    Settings s;
    KindSettings& mobs = s.Kind(PlateKind::Mobs);
    mobs.ornament.place = OrnamentPlace::Top;
    OrnamentAt at = OrnamentLineOf(ComposePlate(AggressiveMob(), kPlayer, s, kGame));
    CHECK(at.line == 0 && !at.under);
    mobs.ornament.place = OrnamentPlace::Bottom;
    at = OrnamentLineOf(ComposePlate(AggressiveMob(), kPlayer, s, kGame));
    CHECK(at.line == 2 && at.under);
    // With the bottom line empty the middle line is the plate's last, and the ornament goes under that.
    mobs.lines[2].text = "";
    at = OrnamentLineOf(ComposePlate(AggressiveMob(), kPlayer, s, kGame));
    CHECK(at.line == 1 && at.under);
    // A plate that loses its icons keeps the ornament at the line that is left.
    ComposedPlate plate = ComposePlate(AggressiveMob(), kPlayer, s, kGame);
    CHECK_EQ(plate.lines.size(), size_t{2});
    DropIcons(plate);
    CHECK_EQ(plate.lines.size(), size_t{1});
    CHECK(OrnamentLineOf(plate).line == 0 && OrnamentLineOf(plate).under);
    plate.ornament = OrnamentPlace::Top;
    CHECK(OrnamentLineOf(plate).line == 0 && !OrnamentLineOf(plate).under);
    plate.lines.clear();
    CHECK_EQ(OrnamentLineOf(plate).line, kNoOrnament);
}

TEST(a_line_takes_the_style_of_the_line_it_syncs_with)
{
    Settings s;
    KindSettings& mobs = s.Kind(PlateKind::Mobs);
    mobs.lines[2].textSize = 200;
    mobs.lines[2].style    = StyleBit(FontStyle::Bold) | StyleBit(FontStyle::Underline);
    mobs.lines[1].syncWith = 2;
    mobs.lines[1].textSize = 50;
    const ComposedPlate plate = ComposePlate(AggressiveMob(), kPlayer, s, kGame);
    CHECK_EQ(plate.lines[1].look.textSize, BasePixels(s, 200));
    CHECK_EQ(plate.lines[1].look.style, StyleBit(FontStyle::Bold) | StyleBit(FontStyle::Underline));
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

TEST(only_a_dash_typed_in_a_design_is_drawn_as_the_mark_chosen)
{
    Settings s; // a new install's dashes are diamonds
    s.Kind(PlateKind::Mobs).lines[1].text = "Lv $level - $con";
    ActorInfo mob = AggressiveMob();
    std::snprintf(mob.name, sizeof(mob.name), "Ga-Bhu");
    const auto level = [&] { return ComposePlate(mob, kPlayer, s, kGame).lines[1].items[0].text; };
    const std::string diamond(1, DashChar(DashMark::Diamond)), dot(1, DashChar(DashMark::Dot));
    // The dashes between a range's ends are the tags' own text, not the design's.
    CHECK_EQ(level(), "Lv 17-20 " + diamond + " EP-EM");
    CHECK_EQ(ComposePlate(mob, kPlayer, s, kGame).lines[2].items[0].text, "Ga-Bhu");
    s.dashMark = DashMark::Dot;
    CHECK_EQ(level(), "Lv 17-20 " + dot + " EP-EM");
    s.dashMark = DashMark::None;
    CHECK_EQ(level(), "Lv 17-20 - EP-EM");
    // Each mark has a character of its own, none of them a dash, and a report reads them as dashes.
    CHECK(diamond != dot && diamond != "-" && dot != "-");
    CHECK(MarkOfChar(diamond[0]) == DashMark::Diamond && MarkOfChar(dot[0]) == DashMark::Dot && !MarkOfChar('-'));
    CHECK_EQ(WithDashes("Lv 17" + diamond + "20 " + dot), "Lv 17-20 -");
}

TEST(a_text_is_split_at_its_marks_into_what_is_drawn_in_turn)
{
    const std::string diamond(1, DashChar(DashMark::Diamond)), square(1, DashChar(DashMark::Square));
    CHECK((SplitMarks("Lv 17" + diamond + "20") ==
           std::vector<TextPiece>{{"Lv 17", DashMark::None}, {"", DashMark::Diamond}, {"20", DashMark::None}}));
    // A mark at either end, and two side by side, leave no empty stretch of letters between them.
    CHECK((SplitMarks(square + square + "a" + diamond) ==
           std::vector<TextPiece>{{"", DashMark::Square}, {"", DashMark::Square}, {"a", DashMark::None}, {"", DashMark::Diamond}}));
    CHECK((SplitMarks("Ga-Bhu") == std::vector<TextPiece>{{"Ga-Bhu", DashMark::None}}));
    CHECK(SplitMarks("").empty());
}

TEST(detection_icons_follow_the_detection_filter)
{
    Settings s;
    s.detectionShown = DetectionBit(DetectionIcon::Link);
    CHECK_EQ(Rendered(ComposePlate(AggressiveMob(), kPlayer, s, kGame).lines[0]), "[1]");
    // A new install leaves out the aggression icon, which the outline's color already tells.
    const auto icons = [](const Settings& with) {
        return ComposePlate(AggressiveMob(), kPlayer, with, kGame).lines[0].items[0].icons.size();
    };
    Settings every;
    every.detectionShown = kAllDetection;
    CHECK_EQ(icons(Settings{}) + 1, icons(every));
    CHECK(ComposePlate(AggressiveMob(), kPlayer, Settings{}, kGame).lines[0].items[0].icons[0].icon != Icon::AggroNQ);
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
    CHECK(HookActionFor(mob, s, false, false) == HookAction::Replace);
    s.Kind(PlateKind::Mobs).rows[CategoryIndex(Category::WillAttack)].plate = PlateMode::Vanilla;
    CHECK(HookActionFor(mob, s, false, false) == HookAction::Keep);
    s.Kind(PlateKind::Mobs).rows[CategoryIndex(Category::WillAttack)].plate = PlateMode::Hidden;
    CHECK(HookActionFor(mob, s, false, false) == HookAction::Hide);
    s.Kind(PlateKind::Mobs).rows[CategoryIndex(Category::WillAttack)].plate = PlateMode::WhenTargeted;
    CHECK(HookActionFor(mob, s, false, false) == HookAction::Hide);
    EntityState targeted = mob;
    targeted.target      = TargetState::Targeted;
    CHECK(HookActionFor(targeted, s, false, false) == HookAction::Replace);
    s.nameplates = false;
    CHECK(HookActionFor(targeted, s, false, false) == HookAction::Keep);
    CHECK(HookActionFor(targeted, s, true, false) == HookAction::Hide);
    s.nameplates = true;
    s.enabled    = false;
    CHECK(HookActionFor(targeted, s, false, false) == HookAction::Keep);
    CHECK(HookActionFor(targeted, s, true, false) == HookAction::Hide); // screenshot mode hides names even after /hu off
}

TEST(a_name_headsup_cannot_draw_goes_back_to_the_game_and_a_hidden_one_stays_hidden)
{
    Settings s;
    PlateMode& row        = s.Kind(PlateKind::Mobs).rows[CategoryIndex(Category::WillAttack)].plate;
    const EntityState mob = AggressiveMob().state;
    EntityState targeted  = mob;
    targeted.target       = TargetState::Targeted;
    row = PlateMode::Shown;
    CHECK(HookActionFor(mob, s, false, true) == HookAction::Keep);
    row = PlateMode::WhenTargeted;
    CHECK(HookActionFor(targeted, s, false, true) == HookAction::Keep);
    CHECK(HookActionFor(mob, s, false, true) == HookAction::Hide); // hidden while it is not your target
    row = PlateMode::Hidden;
    CHECK(HookActionFor(mob, s, false, true) == HookAction::Hide);
    row = PlateMode::Vanilla;
    CHECK(HookActionFor(mob, s, false, true) == HookAction::Keep);
    row = PlateMode::Shown;
    CHECK(HookActionFor(mob, s, true, true) == HookAction::Hide); // screenshot mode still hides every name
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
    mobs.lines[2].style    = StyleBit(FontStyle::Bold) | StyleBit(FontStyle::Underline);
    mobs.lines[2].textSize = 200;
    mobs.lines[0].text     = "$id";
    const ComposedPlate plate = Compose(AggressiveMob(), s);
    CHECK_EQ(plate.lines.size(), size_t{3});
    for (int i = 0; i < 2; ++i)
    {
        CHECK_EQ(plate.lines[i].look.textSize, BasePixels(s, 200));
        CHECK_EQ(plate.lines[i].look.style, StyleBit(FontStyle::Bold) | StyleBit(FontStyle::Underline));
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
    CHECK(Compose(pet, s).lines.empty()); // a pet's nameplate shows when it is targeted
    pet.state.target = TargetState::Targeted;
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

TEST(a_players_title_shows_once_known_and_takes_its_space_with_it_when_not)
{
    Settings s;
    s.Kind(PlateKind::Players).lines[2].text = "$name, $title";
    s.Kind(PlateKind::Mobs).lines[2].text    = "$name $title";
    ActorInfo info = LinkshellPlayer();
    info.status.reset();
    CHECK_EQ(Rendered(Compose(info, s).lines[0]), "Chadmage,"); // unknown until they are checked
    info.title = 266;
    CHECK_EQ(Rendered(Compose(info, s).lines[0]), "Chadmage, Looks Sublime in a Subligar");
    info.title = 60000; // a title the data does not have
    CHECK_EQ(Rendered(Compose(info, s).lines[0]), "Chadmage,");
    // A mob has no title, whatever number its entity carries.
    ActorInfo mob = AggressiveMob();
    mob.title     = 266;
    CHECK_EQ(Rendered(Compose(mob, s).lines.back()), "Young Quadav");
}

TEST(a_slot_draws_the_first_of_its_tags_that_has_something_to_show)
{
    Settings s;
    std::string& line = s.Kind(PlateKind::Players).lines[2].text;
    line              = "${sync|bazaar} $name";
    const auto icons  = [&](const PlayerStatus& status) {
        const ComposedPlate plate = Compose(PlayerWith(status), s);
        std::vector<Icon> drawn;
        for (const PlateItem& item : plate.lines[0].items)
            for (const IconDraw& icon : item.icons) drawn.push_back(icon.icon);
        return drawn;
    };
    PlayerStatus status;
    status.bazaar = true;
    CHECK(icons(status) == std::vector<Icon>{Icon::Bazaar}); // not synced: the next in the slot
    status.levelSync = true;
    CHECK(icons(status) == std::vector<Icon>{Icon::LevelSync}); // one place, so never both
    // Neither, and the slot takes its space with it as a tag with nothing to show does.
    CHECK_EQ(Rendered(Compose(PlayerWith(PlayerStatus{}), s).lines[0]), "Chadmage");
    // Text and icons share a slot alike: a title when it is known, and the linkshell's icon until then.
    line           = "$name ${title|linkshell}";
    ActorInfo info = LinkshellPlayer();
    CHECK_EQ(Rendered(Compose(info, s).lines[0]), "Chadmage [1]");
    info.title = 266;
    CHECK_EQ(Rendered(Compose(info, s).lines[0]), "Chadmage Looks Sublime in a Subligar");
    // Where a plate keeps its name alone, a slot that holds the name shows it and any other shows nothing.
    line = "${title|name} ${sync|bazaar}";
    PlayerState watching = kPlayer;
    watching.inEvent     = true;
    info.status->bazaar  = true;
    const ComposedPlate alone = ComposePlate(info, watching, s, kGame);
    CHECK_EQ(alone.lines.size(), size_t{1});
    CHECK_EQ(Rendered(alone.lines[0]), "Chadmage");
}

TEST(a_plate_draws_no_more_status_icons_than_its_kind_allows_and_gms_besides)
{
    Settings s;
    KindSettings& players = s.Kind(PlateKind::Players);
    players.lines[1].text = "$away$bazaar";
    players.lines[2].text = "$gm$mentor$seeking $name ${sync|linkshell}";
    PlayerStatus all;
    all.gm = all.mentor = all.seekingParty = all.away = all.bazaar = all.linkshell = all.levelSync = all.newAdventurer = true;
    const auto icons = [&](const PlayerStatus& status) {
        std::vector<Icon> drawn;
        for (const ComposedLine& line : Compose(PlayerWith(status), s).lines)
            for (const PlateItem& item : line.items)
                for (const IconDraw& icon : item.icons) drawn.push_back(icon.icon);
        return drawn;
    };
    // A new install's limit is every icon there is.
    CHECK((icons(all) == std::vector<Icon>{Icon::Away, Icon::Bazaar, Icon::Gm, Icon::Mentor, Icon::Invite, Icon::LevelSync}));
    // Counted in reading order, the upper line first; GM's is never counted and always drawn.
    players.maxIcons = 3;
    CHECK((icons(all) == std::vector<Icon>{Icon::Away, Icon::Bazaar, Icon::Gm, Icon::Mentor}));
    players.maxIcons = 1;
    CHECK((icons(all) == std::vector<Icon>{Icon::Away, Icon::Gm}));
    // Only what is drawn counts: an icon the player lacks leaves its place in the count to the next, and a slot is one.
    PlayerStatus some;
    some.mentor = some.linkshell = some.levelSync = true;
    players.maxIcons = 2;
    CHECK((icons(some) == std::vector<Icon>{Icon::Mentor, Icon::LevelSync}));
    players.maxIcons = 1;
    CHECK((icons(some) == std::vector<Icon>{Icon::Mentor}));
    CHECK_EQ(Rendered(Compose(PlayerWith(some), s).lines[0]), "[1] Chadmage"); // what is left out leaves no gap at the end
    // Each kind has its own limit.
    s.Kind(PlateKind::Mobs).maxIcons = 1;
    CHECK_EQ(Rendered(Compose(AggressiveMob(), s).lines[0]), "[3]");
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
    CHECK_EQ(OrnamentLineOf(plate).line, 1);
    Settings d;
    d.Kind(PlateKind::Mobs).lines[1].text = "$name";
    ActorInfo dead   = AggressiveMob();
    dead.state.alive = false;
    const ComposedPlate gone = Compose(dead, d);
    CHECK_EQ(gone.lines.size(), size_t{2});
    CHECK_EQ(OrnamentLineOf(gone).line, 1); // two name lines still have the rule between them
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
    BadgeRules rules{.elementWeakRank = s.elementWeakRank, .elementResistRank = s.elementResistRank,
        .maxElements = s.maxElements};
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

TEST(the_settings_badge_cutoffs_decide_which_badges_the_tags_draw)
{
    // The Goblin Bounty Hunter's ranks: light -3, the other elements but dark -2; dark sleep -3, gravity -2.
    ActorInfo info = AggressiveMob();
    info.mob       = MobById(17199648);
    CHECK(info.mob != nullptr);
    Settings s;
    s.Kind(PlateKind::Mobs).lines[0].text = "$weak$resist$status$immune";
    const auto drawn = [&]
    {
        const ComposedPlate plate = Compose(info, s);
        std::vector<Icon> icons;
        for (const IconDraw& draw : plate.lines[0].items[0].icons)
            icons.push_back(draw.icon);
        return icons;
    };
    s.elementWeakRank = -3;
    CHECK(drawn() == std::vector<Icon>{Icon::Light});
    s             = Settings{};
    s.Kind(PlateKind::Mobs).lines[0].text = "$weak$resist$status$immune";
    s.maxElements = 2;
    CHECK(drawn() == (std::vector<Icon>{Icon::Light, Icon::Fire}));
    s.statusBadges[static_cast<int>(StatusRank::Gravity)].weakRank = -2;
    CHECK(drawn() == (std::vector<Icon>{Icon::Light, Icon::Fire, Icon::Gravity}));
    s.statusBadges[static_cast<int>(StatusRank::Gravity)].show = false;
    CHECK(drawn() == (std::vector<Icon>{Icon::Light, Icon::Fire}));
}

TEST(a_plate_without_icons_keeps_its_texts_and_the_ornament_between_two_lines_still_shown)
{
    Settings s;
    ComposedPlate plate = Compose(AggressiveMob(), s);
    CHECK_EQ(plate.lines.size(), size_t{3});
    DropIcons(plate);
    CHECK_EQ(plate.lines.size(), size_t{2}); // the icon line is gone
    CHECK_EQ(plate.lines[0].source, 1);
    CHECK_EQ(Rendered(plate.lines[0]), "Lv 17-20 EP-EM");
    CHECK_EQ(plate.lines[1].source, 2);
    CHECK_EQ(OrnamentLineOf(plate).line, 1); // over the name, which is one line higher in the plate

    ComposedPlate player = Compose(LinkshellPlayer(), s);
    DropIcons(player);
    CHECK_EQ(player.lines.size(), size_t{1});
    CHECK_EQ(player.lines[0].items.size(), size_t{1});
    CHECK_EQ(Rendered(player.lines[0]), "Chadmage");
    CHECK_EQ(OrnamentLineOf(player).line, kNoOrnament);

    KindSettings& mobs = s.Kind(PlateKind::Mobs);
    mobs.lines[1].text = "$detect $name - $detect";
    ComposedPlate gaps = Compose(AggressiveMob(), s);
    CHECK_EQ(gaps.lines[1].items.size(), size_t{5}); // icons, a gap, text, a gap, icons
    DropIcons(gaps);
    CHECK_EQ(gaps.lines[0].items.size(), size_t{1}); // the gaps go with their icons
    CHECK_EQ(Rendered(gaps.lines[0]), "Young Quadav -");
}

TEST(the_ornament_goes_with_either_line_that_was_only_icons)
{
    Settings s;
    KindSettings& mobs  = s.Kind(PlateKind::Mobs);
    mobs.ornament.place = OrnamentPlace::AboveMiddle;
    ComposedPlate above = Compose(AggressiveMob(), s);
    CHECK_EQ(OrnamentLineOf(above).line, 1);
    DropIcons(above); // line 1, the icons, is the upper of its two
    CHECK_EQ(above.lines.size(), size_t{2});
    CHECK_EQ(OrnamentLineOf(above).line, kNoOrnament);

    mobs.lines[0].text = "Lv $level";
    mobs.lines[1].text = "$detect";
    ComposedPlate below = Compose(AggressiveMob(), s);
    CHECK_EQ(OrnamentLineOf(below).line, 1);
    DropIcons(below); // and here the lower
    CHECK_EQ(below.lines.size(), size_t{2});
    CHECK_EQ(OrnamentLineOf(below).line, kNoOrnament);

    mobs.lines[1].text = "$detect$id";
    ComposedPlate kept = Compose(AggressiveMob(), s);
    DropIcons(kept);
    CHECK_EQ(kept.lines.size(), size_t{3});
    CHECK_EQ(OrnamentLineOf(kept).line, 1);
}

TEST(a_lines_texts_join_with_spaces_for_the_debug_report)
{
    Settings s;
    KindSettings& mobs = s.Kind(PlateKind::Mobs);
    mobs.lines[1].text = "$name$detect($level)";
    const ComposedPlate plate = Compose(AggressiveMob(), s);
    std::string text = "what the last plate left";
    JoinTexts(plate.lines[1], text);
    CHECK_EQ(text, "Young Quadav (17-20)");
    JoinTexts(plate.lines[0], text); // only icons
    CHECK_EQ(text, "");
    JoinTexts(plate.lines[2], text);
    CHECK_EQ(text, "Young Quadav");
}

TEST(a_lines_icons_are_a_share_of_the_base_icon_size_whatever_the_font_size)
{
    Settings s;
    s.baseSize     = 30;
    s.baseIconSize = 20;
    s.Kind(PlateKind::Mobs).lines[0].iconSize = 50;
    const ComposedPlate plate = ComposePlate(AggressiveMob(), kPlayer, s, kGame);
    CHECK_EQ(plate.lines[0].look.iconSize, 10.0f);
    CHECK_EQ(plate.lines.back().look.textSize, 30.0f);
}

TEST(a_line_carries_its_padding_and_a_synced_line_the_one_it_borrows)
{
    Settings s;
    KindSettings& mobs = s.Kind(PlateKind::Mobs);
    mobs.lines[1].padding  = 6;
    mobs.lines[2].syncWith = 1;
    const ComposedPlate plate = ComposePlate(AggressiveMob(), kPlayer, s, kGame);
    CHECK_EQ(plate.lines[0].look.padding, 0.0f);
    CHECK_EQ(plate.lines[1].look.padding, 6.0f);
    CHECK_EQ(plate.lines[2].look.padding, 6.0f);
}

TEST(a_lines_shadow_at_100_percent_is_one_and_a_half_of_the_mockups)
{
    Settings s;
    LineDesign& name = s.Kind(PlateKind::Mobs).lines[2];
    CHECK_EQ(name.shadow, 100);
    CHECK_EQ(ComposePlate(AggressiveMob(), kPlayer, s, kGame).lines.back().look.shadow, 1.5f);
    name.shadow = 200;
    CHECK_EQ(ComposePlate(AggressiveMob(), kPlayer, s, kGame).lines.back().look.shadow, 3.0f);
    name.shadow = 0;
    CHECK_EQ(ComposePlate(AggressiveMob(), kPlayer, s, kGame).lines.back().look.shadow, 0.0f);
}

TEST(placeholder_timers_take_the_default_font_and_a_plain_lines_shadow)
{
    Settings s;
    s.baseSize     = 20;
    s.timerPercent = 50;
    s.defaultFont  = "Tahoma";
    // No line's design reaches them.
    LineDesign& name = s.Kind(PlateKind::Self).lines[2];
    name.font        = "Cinzel";
    name.style       = StyleBit(FontStyle::Italic);
    name.shadow      = 0;
    name.glow        = 100;
    name.color       = ColorPick{ColorSource::Custom, {{1.0f, 0.0f, 0.0f}}};
    const LineLook look = TimerLook(s);
    CHECK_EQ(look.font, "Tahoma");
    CHECK_EQ(look.style, 0);
    CHECK_EQ(look.textSize, 10.0f);
    CHECK_EQ(look.color, kWhite);
    CHECK_EQ(look.shadow, kShadowUnit);
    CHECK_EQ(look.shadowColor, kBlack);
    CHECK_EQ(look.glow, 0.0f);
}

TEST(badge_tags_join_each_other_and_start_a_run_of_their_own_beside_other_icons)
{
    const MobRecord* hunter = MobById(17199648);
    CHECK(hunter != nullptr);
    ActorInfo info = AggressiveMob();
    info.mob       = hunter;
    Settings s;
    for (StatusBadgeRule& rule : s.statusBadges) rule.weakRank = kMaxWeakRank;
    KindSettings& mobs = s.Kind(PlateKind::Mobs);
    const auto oneRun  = [&](const char* text)
    {
        mobs.lines[0].text        = text;
        const ComposedPlate plate = Compose(info, s);
        CHECK_EQ(plate.lines[0].items.size(), size_t{1});
        return plate.lines[0].items[0].icons;
    };
    const std::vector<IconDraw> detection = oneRun("$detect");
    const std::vector<IconDraw> badges    = oneRun("$weak$resist$status$immune");
    CHECK(!detection.empty() && std::ranges::none_of(detection, HasBadge));
    CHECK(!badges.empty() && std::ranges::all_of(badges, HasBadge));
    CHECK(oneRun("$resist$status$immune").size() < badges.size()); // the hunter has a weakness; the rest join without it

    mobs.lines[0].text       = "$detect$weak$resist$status$immune";
    const ComposedPlate both = Compose(info, s);
    CHECK_EQ(both.lines[0].items.size(), size_t{2}); // nothing between them: the layout sets them an item gap apart
    CHECK(both.lines[0].items[0].kind == ItemKind::Icons && both.lines[0].items[0].icons == detection);
    CHECK(both.lines[0].items[1].kind == ItemKind::Icons && both.lines[0].items[1].icons == badges);

    mobs.lines[0].text          = "$weak$resist$status$immune$detect";
    const ComposedPlate flipped = Compose(info, s);
    CHECK_EQ(flipped.lines[0].items.size(), size_t{2});
    CHECK(flipped.lines[0].items[0].icons == badges);
    CHECK(flipped.lines[0].items[1].icons == detection);

    // A mob with nothing to badge has the detection run alone, and a player's icons make one run.
    mobs.lines[0].text          = "$detect$weak$resist$status$immune";
    const ComposedPlate neutral = Compose(AggressiveMob(), s);
    CHECK_EQ(neutral.lines[0].items.size(), size_t{1});
    CHECK_EQ(Rendered(neutral.lines[0]), "[3]");
    const ComposedPlate player = Compose(LinkshellPlayer(), s);
    CHECK_EQ(player.lines[0].items.size(), size_t{2});
    CHECK_EQ(Rendered(player.lines[0]), "[2]Chadmage");
}

// Para (16785716) has something in each of the four badge groups under the default rules.
TEST(each_badge_tag_keeps_apart_from_the_detection_icons)
{
    const MobRecord* para = MobById(16785716);
    CHECK(para != nullptr);
    ActorInfo info = AggressiveMob();
    info.mob       = para;
    Settings s;
    std::string& text = s.Kind(PlateKind::Mobs).lines[0].text;
    for (const char* tag : {"$weak", "$resist", "$status", "$immune"})
    {
        text                      = std::string("$detect") + tag;
        const ComposedPlate plate = Compose(info, s);
        CHECK_EQ(plate.lines[0].items.size(), size_t{2});
        CHECK(std::ranges::none_of(plate.lines[0].items[0].icons, HasBadge));
        CHECK(!plate.lines[0].items[1].icons.empty() && std::ranges::all_of(plate.lines[0].items[1].icons, HasBadge));
    }
    text = "$weak$resist$status$immune";
    CHECK_EQ(Compose(info, s).lines[0].items.size(), size_t{1});
}

TEST(a_text_left_out_takes_its_item_and_an_emptied_line_takes_the_ornament_beside_it)
{
    const Settings s;
    // The default mob plate: icons, the level line, the name, and the ornament over the name.
    ComposedPlate level = Compose(AggressiveMob(), s);
    DropTexts(level, std::vector<uint8_t>{1, 0});
    CHECK_EQ(level.lines.size(), size_t{2});
    CHECK_EQ(level.lines[0].source, 0);
    CHECK_EQ(level.lines[1].source, 2);
    CHECK_EQ(OrnamentLineOf(level).line, kNoOrnament); // it sat between the level line and the name

    ComposedPlate name = Compose(AggressiveMob(), s);
    DropTexts(name, std::vector<uint8_t>{0, 1});
    CHECK_EQ(name.lines.size(), size_t{2});
    CHECK_EQ(Rendered(name.lines[1]), "Lv 17-20 EP-EM");
    CHECK_EQ(OrnamentLineOf(name).line, kNoOrnament);
    for (const ComposedLine& line : name.lines)
        CHECK(std::ranges::none_of(line.items, [](const PlateItem& item) { return item.name; }));

    ComposedPlate whole = Compose(AggressiveMob(), s);
    DropTexts(whole, std::vector<uint8_t>{0, 0});
    CHECK_EQ(whole.lines.size(), size_t{3});
    CHECK_EQ(OrnamentLineOf(whole).line, 2);
}

TEST(a_text_left_out_beside_icons_takes_the_gap_that_set_it_apart)
{
    Settings s;
    KindSettings& mobs = s.Kind(PlateKind::Mobs);
    mobs.lines[0].text = "$name $detect";         // text, gap, icons
    mobs.lines[1].text = "$detect $name $detect"; // icons, gap, text, gap, icons
    mobs.lines[2].text = "$id$detect$name";       // text, icons, text
    ComposedPlate plate = Compose(AggressiveMob(), s);
    CHECK_EQ(plate.lines[1].items.size(), size_t{5});
    DropTexts(plate, std::vector<uint8_t>{1, 1, 0, 1}); // every name; the ID stays
    CHECK_EQ(plate.lines.size(), size_t{3});
    CHECK_EQ(Rendered(plate.lines[0]), "[3]");    // no gap before the icons
    CHECK_EQ(Rendered(plate.lines[1]), "[3] [3]"); // one gap between the two runs, not two
    CHECK_EQ(Rendered(plate.lines[2]), "[006][3]");
    CHECK_EQ(OrnamentLineOf(plate).line, 2);
    mobs.lines[1].text = "$detect $name";
    ComposedPlate last = Compose(AggressiveMob(), s);
    DropTexts(last, std::vector<uint8_t>{0, 1, 0, 0});
    CHECK_EQ(Rendered(last.lines[0]), "Young Quadav [3]");
    CHECK_EQ(Rendered(last.lines[1]), "[3]"); // nor a gap after them
}
