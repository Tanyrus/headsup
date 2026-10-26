#include "bar_widgets.h"
#include "labels.h"
#include "test.h"
#include "vanadiel.h"
#include "weather.h"

#include <cmath>

#include <algorithm>
#include <cstdio>
#include <map>
#include <string>
#include <utility>

using namespace headsup;

namespace
{
    constexpr uint32_t kBountyHunter = 17199648; // Goblin Bounty Hunter, zone 103: light -3, dark 0, the rest -2

    ActorInfo Goblin()
    {
        ActorInfo info;
        info.kind     = EntityKind::Mob;
        info.alive    = true;
        info.serverId = kBountyHunter;
        info.index    = kBountyHunter & 0xFFF;
        info.mob      = MobById(kBountyHunter);
        info.level    = MakeLabel(info.mob, nullptr, 20);
        info.state.kind       = EntityKind::Mob;
        info.state.alive      = true;
        info.state.nature     = Nature::Aggressive;
        info.state.classified = Category::WillAttack;
        info.state.row        = Category::WillAttack;
        info.state.toughness  = info.level.shade;
        std::snprintf(info.name, sizeof(info.name), "%s", "Goblin Bounty Hunter");
        return info;
    }

    std::vector<Icon> EveryIcon(const ActorInfo& mob)
    {
        const IconSet icons = IconsFor(mob.mob, kAllDetection);
        return std::vector<Icon>(icons.icons, icons.icons + icons.count);
    }

    std::string Joined(const std::vector<Span>& spans)
    {
        std::string text;
        for (const Span& s : spans)
            text += s.text;
        return text;
    }

    const PopupSection* Section(const PopupContent& popup, const std::string& heading)
    {
        for (const PopupSection& s : popup.sections)
            if (s.heading == heading) return &s;
        return nullptr;
    }

    // Each row of a section as one line of text: its cells' text joined.
    std::vector<std::string> Lines(const PopupSection& section)
    {
        std::vector<std::string> lines;
        for (const PopupRow& row : section.rows)
        {
            std::string line;
            for (const PopupCell& cell : row.cells)
                line += Joined(cell.text);
            lines.push_back(line);
        }
        return lines;
    }

    std::vector<std::string> TagsOf(const PopupContent& popup)
    {
        std::vector<std::string> tags;
        for (const PopupRow& row : popup.sections[0].rows)
            if (row.kind == RowKind::Tags)
                for (const PopupCell& cell : row.cells)
                    tags.push_back(Joined(cell.text));
        return tags;
    }

    ActorInfo MobTarget(uint32_t serverId)
    {
        ActorInfo target = Goblin();
        target.mob       = MobById(serverId);
        target.serverId  = serverId;
        return target;
    }
}

TEST(the_target_widget_shows_name_level_and_con_in_their_colors)
{
    // The Aggressive row's outline pick is the Aggression palette's; at 20 a 17-20 mob's harder end is an Even Match.
    Settings settings;
    settings.palette.aggression[static_cast<int>(Nature::Aggressive)] = ColorFromRgb(0x112233);
    settings.palette.toughness[static_cast<int>(LabelShade::EvenMatch)] = ColorFromRgb(0x445566);
    const ActorInfo goblin = Goblin();
    BarState state;
    state.target          = &goblin;
    const WidgetContent w = BarWidgetContent(BarWidget::Target, state, settings);
    CHECK(w.shown && w.hasPopup);
    CHECK(w.icons == std::vector<Icon>{Icon::AggroNQ});
    CHECK(w.title == (std::vector<Span>{{"Goblin Bounty Hunter", Tone::Own, 0xFF112233u}}));
    CHECK(w.subtitle == (std::vector<Span>{{"Lv 17-20 EP-EM", Tone::Own, 0xFF445566u}}));
}

TEST(target_details_put_a_mobs_icons_after_its_name_and_its_weaknesses_after_its_level)
{
    Settings settings;
    settings.Widget(BarWidget::Target).targetDetails = true;
    const ActorInfo goblin    = Goblin();
    BarState state;
    state.target          = &goblin;
    const WidgetContent w = BarWidgetContent(BarWidget::Target, state, settings);
    CHECK(w.icons.empty()); // none before the name: they follow it
    CHECK(w.titleIcons.size() > 1 && w.titleIcons == EveryIcon(goblin));
    std::vector<Icon> weak;
    const InfoIconSet all = InfoIconsFor(&ResistancesOf(*goblin.mob), BadgeRules{});
    for (int i = 0; i < all.count; ++i)
        if (all.icons[i].badge == BadgeKind::Weak || all.icons[i].badge == BadgeKind::WeakRank)
            weak.push_back(all.icons[i].icon);
    CHECK(!weak.empty() && w.subtitleIcons == weak);
    CHECK(Joined(w.subtitle) == goblin.level.text);
}

TEST(no_target_reads_no_target_and_a_player_gets_only_a_name)
{
    Settings settings;
    BarState state;
    CHECK(BarWidgetContent(BarWidget::Target, state, settings).title == (std::vector<Span>{{"No target", Tone::Dim}}));
    CHECK(!BarWidgetContent(BarWidget::Target, state, settings).hasPopup);
    CHECK(BarPopupContent(BarWidget::Target, state, settings).title == (std::vector<Span>{{"No target", Tone::Dim}}));
    ActorInfo player;
    player.kind = EntityKind::Player;
    std::snprintf(player.name, sizeof(player.name), "%s", "Carrott");
    state.target          = &player;
    const WidgetContent w = BarWidgetContent(BarWidget::Target, state, settings);
    CHECK(Joined(w.title) == "Carrott" && w.subtitle.empty() && w.icons.empty() && !w.hasPopup);
    ActorInfo stranger = Goblin(); // a mob the Phoenix data does not have
    stranger.mob       = nullptr;
    stranger.level     = MakeLabel(nullptr, nullptr, 20);
    state.target       = &stranger;
    const WidgetContent s = BarWidgetContent(BarWidget::Target, state, settings);
    CHECK(Joined(s.title) == "Goblin Bounty Hunter" && Joined(s.subtitle) == stranger.level.text && !s.hasPopup);
    CHECK(s.icons.empty());
}

TEST(a_dead_mob_with_data_keeps_its_aggro_icon_and_popup_but_has_no_level)
{
    Settings settings;
    ActorInfo dead = Goblin(); // the tracker fills the level only for a living mob
    dead.alive = dead.state.alive = false;
    dead.level                    = Label{};
    dead.state.toughness          = LabelShade::Unknown;
    BarState state;
    state.target          = &dead;
    const WidgetContent w = BarWidgetContent(BarWidget::Target, state, settings);
    CHECK(w.icons == std::vector<Icon>{Icon::AggroNQ});
    const uint32_t nameColor = ToArgb(settings.palette.aggression[static_cast<int>(Nature::Aggressive)]);
    CHECK(w.title == (std::vector<Span>{{"Goblin Bounty Hunter", Tone::Own, nameColor}}));
    CHECK(w.subtitle.empty());
    CHECK(w.hasPopup);
    const PopupContent popup = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(popup.icon == Icon::AggroNQ);
    const PopupRow& top = popup.sections[0].rows[0];
    CHECK(top.cells.size() == 2);
    CHECK(Joined(top.cells[0].text) == "WAR/WAR" && Joined(top.cells[1].text) == "ID 220");
    CHECK(Section(popup, "Spawn") != nullptr);
}

TEST(the_target_popup_has_every_element_and_status_with_its_percent)
{
    Settings settings;
    const ActorInfo goblin = Goblin();
    BarState state;
    state.target             = &goblin;
    const PopupContent popup = BarPopupContent(BarWidget::Target, state, settings);
    std::vector<std::string> headings;
    for (const PopupSection& s : popup.sections)
        headings.push_back(s.heading);
    CHECK(headings == (std::vector<std::string>{"", "Damage taken", "Elements", "Statuses", "Immune", "Stats at Lv 20",
                                                "TP moves", "Spawn"}));
    const PopupRow& elements = Section(popup, "Elements")->rows[0];
    CHECK(elements.kind == RowKind::Grid && elements.columns == 4 && elements.cells.size() == 8);
    CHECK(elements.cells[6].icon == Icon::Light);
    // Unchecked, at the top of its range: magic evasion 58 at 20 (rank C+, Phoenix's sql/skill_caps.sql), light -3
    // takes floor(58 * 0.95) = 55 off it, fire -2 floor(58 * 0.96019) = 55; dark is 0.
    CHECK(elements.cells[6].text == (std::vector<Span>{{"+3%", Tone::Weak}}));
    CHECK(elements.cells[7].text == (std::vector<Span>{{"0%", Tone::Neutral}}));
    CHECK(elements.cells[0].text == (std::vector<Span>{{"+3%", Tone::Weak}}));
    // Its combat stats at 20 from data/phoenix_mob_info.tsv, then STR to CHR and its magic evasion.
    const PopupRow& combat = Section(popup, "Stats at Lv 20")->rows[0];
    CHECK(combat.kind == RowKind::Grid && combat.cells.size() == 6);
    CHECK(combat.cells[0].text == (std::vector<Span>{{"HP", Tone::Dim}, {" 414", Tone::Normal}}));
    CHECK(combat.cells[3].text == (std::vector<Span>{{"DEF", Tone::Dim}, {" 85", Tone::Normal}}));
    const PopupRow& stats = Section(popup, "Stats at Lv 20")->rows[1];
    CHECK(stats.kind == RowKind::Grid && stats.cells.size() == 8);
    CHECK(stats.cells[7].text == (std::vector<Span>{{"MEva", Tone::Dim}, {" 58", Tone::Normal}}));
    const PopupRow& moves = Section(popup, "TP moves")->rows[0];
    CHECK(moves.kind == RowKind::Grid && moves.columns == 3 && moves.cells.size() == 3);
    CHECK(Joined(moves.cells[0].text) == "Goblin Rush" && Joined(moves.cells[2].text) == "Bomb Toss Suicide");
    const PopupRow& statuses = Section(popup, "Statuses")->rows[0];
    CHECK(statuses.columns == 5 && statuses.cells.size() == 10 && statuses.cells[0].icon == Icon::Paralyze);
    CHECK(Section(popup, "Immune")->rows[0].cells[0].text == (std::vector<Span>{{"None", Tone::Neutral}}));
    CHECK(Joined(Section(popup, "Spawn")->rows[0].cells[0].text) == "Respawn 5:00");
    CHECK(Joined(popup.sections[0].rows[0].cells[1].text) == "WAR/WAR");
    CHECK(Joined(popup.sections[0].rows[0].cells[2].text) == "ID 220"); // 17199648 is 0x1067220
    settings.rankDisplay = RankDisplay::Rank;
    CHECK(Section(BarPopupContent(BarWidget::Target, state, settings), "Elements")->rows[0].cells[6].text ==
          (std::vector<Span>{{"-3", Tone::Weak}}));
}

TEST(a_placeholder_names_its_nm_and_counts_down)
{
    // East Ronfaure: two Carrion Worms are Bigmouth Billy's placeholders (data/phoenix_mobs.tsv).
    Settings settings;
    ActorInfo target = Goblin();
    target.mob       = MobById(17191194);
    target.serverId  = 17191194;
    BarState state;
    state.target          = &target;
    state.targetPhSeconds = 299.0;
    const PopupContent popup  = BarPopupContent(BarWidget::Target, state, settings);
    const PopupSection* spawn = Section(popup, "Spawn");
    CHECK(Joined(spawn->rows[1].cells[0].text) == "Placeholder for Bigmouth Billy (15%)"); // his slot's chance
    CHECK(Joined(spawn->rows[2].cells[0].text) == "Back in 4:59");
    target.mob      = MobById(17191196);
    target.serverId = 17191196;
    const PopupContent nmPopup = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(Joined(Section(nmPopup, "Spawn")->rows.back().cells[0].text) == "2 placeholders in this zone");
}

TEST(a_placeholders_last_second_reads_as_the_nameplates_timer_does)
{
    Settings settings;
    ActorInfo target = Goblin();
    target.mob       = MobById(17191194); // a Carrion Worm, Bigmouth Billy's placeholder
    target.serverId  = 17191194;
    BarState state;
    state.target          = &target;
    state.targetPhSeconds = 0.4;
    const PopupContent popup = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(Joined(Section(popup, "Spawn")->rows[2].cells[0].text) == "Back in 0:01"); // never 0:00 before it is due
}

TEST(the_tags_say_how_a_mob_aggros_links_and_detects)
{
    // data/phoenix_mobs.tsv: the Goblin Bounty Hunter has flags 33 (aggressive, links) and detects 1 (sight); the
    // Brontotaur, 16797799, flags 97 (aggressive, links, true detection) and detects 1, which true sight replaces.
    Settings settings;
    ActorInfo target = Goblin();
    BarState state;
    state.target              = &target;
    const PopupContent goblin = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(TagsOf(goblin) == (std::vector<std::string>{"Aggressive", "Links", "Sight 15y"}));
    target                        = MobTarget(16797799);
    const PopupContent brontotaur = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(TagsOf(brontotaur) == (std::vector<std::string>{"Aggressive", "Links", "True sight 15y", "Counter"}));
    // Qufim Island's Clipper, a crab: it hears from 8 yalms and a beastmaster can charm it.
    target = MobTarget(17293331);
    CHECK(TagsOf(BarPopupContent(BarWidget::Target, state, settings)) == (std::vector<std::string>{"Passive", "Sound 8y", "Charmable"}));
}

TEST(a_tags_words_take_the_tag_tone_and_nothing_else_in_the_popup_does)
{
    // The words sit in their frames, drawn in the tag's text color; the ID beside them and the rows after keep theirs.
    // data/phoenix_mobs.tsv: the Brontotaur, 16797799, has three tags (Aggressive, Links, True sight) and counters
    // (a fourth, data/phoenix_mob_info.tsv); Bloodtear Baldurf, 17195318, flags 11 and detects 257, four (Aggressive,
    // Sight, Scent, NM).
    Settings settings;
    for (const auto& [id, tags] : {std::pair{16797799u, size_t{4}}, {17195318u, size_t{4}}})
    {
        ActorInfo target = MobTarget(id);
        BarState state;
        state.target             = &target;
        const PopupContent popup = BarPopupContent(BarWidget::Target, state, settings);
        size_t tagged = 0, others = 0;
        for (const PopupSection& section : popup.sections)
            for (const PopupRow& row : section.rows)
                for (const PopupCell& cell : row.cells)
                    for (const Span& span : cell.text)
                    {
                        const bool tag = row.kind == RowKind::Tags;
                        CHECK((span.tone == Tone::Tag) == tag);
                        ++(tag ? tagged : others);
                    }
        CHECK(tagged == tags && others > 0);
    }
}

namespace
{
    const char* SpellNumber(uint16_t spell)
    {
        static std::map<uint16_t, std::string> names;
        return names.try_emplace(spell, "S" + std::to_string(spell)).first->second.c_str();
    }
}

TEST(a_lottery_nm_shows_its_jobs_window_chance_and_cooldown_and_its_placeholder_its_chance)
{
    // Attohwa Chasm (data/zones/attohwa_chasm/mobs.yaml): Citipati is THF/BLM, out from 20:00 to 04:00, 15% from his
    // slot with three hours out after his kill, and takes a quarter less magic damage and half breath damage; the
    // Corse is his placeholder.
    Settings settings;
    BarState state;
    const ActorInfo citipati = MobTarget(16806162);
    state.target             = &citipati;
    const PopupContent popup = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(Joined(popup.sections[0].rows[0].cells[1].text) == "THF/BLM");
    CHECK(Lines(*Section(popup, "Spawn")) == (std::vector<std::string>{"Respawn 5:00", "Spawns 20:00 to 04:00",
                                                 "15% pop chance", "3:00:00 cooldown after its kill",
                                                 "1 placeholder in this zone"}));
    const PopupRow& kinds = Section(popup, "Damage taken")->rows[1];
    CHECK(kinds.cells.size() == 2);
    CHECK(kinds.cells[0].text == (std::vector<Span>{{"Magic", Tone::Dim}, {" ", Tone::Normal}, {"-25%", Tone::Resist}}));
    CHECK(kinds.cells[1].text == (std::vector<Span>{{"Breath", Tone::Dim}, {" ", Tone::Normal}, {"-50%", Tone::Resist}}));
    const ActorInfo corse = MobTarget(16806161);
    state.target          = &corse;
    CHECK(Lines(*Section(BarPopupContent(BarWidget::Target, state, settings), "Spawn"))[1] ==
          "Placeholder for Citipati (15%)");
    // A mob with no other kind of damage taken has only its SDT row.
    const ActorInfo goblin = Goblin();
    state.target           = &goblin;
    CHECK(Section(BarPopupContent(BarWidget::Target, state, settings), "Damage taken")->rows.size() == 1);
}

TEST(a_casters_spells_are_the_ones_it_casts_at_its_level)
{
    // The Corse's list (data/phoenix_spells.tsv, list 8), at 67, the top of its range: each spell whose levels
    // hold 67, in the list's order.
    Settings settings;
    BarState state;
    const ActorInfo corse = MobTarget(16806161);
    state.target          = &corse;
    state.spellName       = SpellNumber;
    std::vector<std::string> expected;
    for (const int spell : {254, 258, 245, 250, 253, 216, 225, 247, 231, 259, 252, 206, 210, 274, 146, 191, 151, 166, 186})
        expected.push_back("S" + std::to_string(spell));
    const PopupContent popup   = BarPopupContent(BarWidget::Target, state, settings);
    const PopupSection* spells = Section(popup, "Spells");
    CHECK(spells != nullptr);
    if (spells == nullptr) return;
    CHECK(spells->rows[0].kind == RowKind::Grid && spells->rows[0].columns == 3);
    std::vector<std::string> names;
    for (const PopupCell& cell : spells->rows[0].cells)
        names.push_back(Joined(cell.text));
    CHECK(names == expected);
    state.spellName = nullptr; // without the game's names there is nothing to list
    CHECK(Section(BarPopupContent(BarWidget::Target, state, settings), "Spells") == nullptr);
}

TEST(an_element_it_absorbs_and_what_it_does_by_itself)
{
    // data/phoenix_mob_info.tsv: Puk WW (Windurst Waters' Puk, 16986223) absorbs wind; the Stubborn Dredvodd,
    // 16781327, regenerates; a Water Elemental of zone 37, 16928885, poisons with its hits.
    Settings settings;
    BarState state;
    const ActorInfo puk = MobTarget(16986223);
    state.target        = &puk;
    const PopupContent pukPopup = BarPopupContent(BarWidget::Target, state, settings);
    const PopupRow& elements    = Section(pukPopup, "Elements")->rows[0];
    CHECK(elements.cells[static_cast<int>(Element::Wind)].text == (std::vector<Span>{{"Absorbs", Tone::Immune}}));
    const ActorInfo dredvodd = MobTarget(16781327);
    state.target             = &dredvodd;
    CHECK(Lines(BarPopupContent(BarWidget::Target, state, settings).sections[0]).back() == "Regen");
    const ActorInfo elemental = MobTarget(16928885);
    state.target              = &elemental;
    CHECK(Lines(BarPopupContent(BarWidget::Target, state, settings).sections[0]).back() == "Additional effect: Poison");
}

TEST(a_resisted_damage_type_shows_its_percent_in_the_resist_tone)
{
    // Protozoan, 16781317: slashing and piercing sdt -5000, impact and hand-to-hand -7500 (data/phoenix_mobs.tsv).
    Settings settings;
    const ActorInfo target = MobTarget(16781317);
    BarState state;
    state.target             = &target;
    const PopupContent popup = BarPopupContent(BarWidget::Target, state, settings);
    const PopupRow& damage   = Section(popup, "Damage taken")->rows[0];
    CHECK(damage.cells[0].icon == Icon::Slashing);
    CHECK(damage.cells[0].text == (std::vector<Span>{{"-50%", Tone::Resist}}));
    CHECK(damage.cells[3].text == (std::vector<Span>{{"-75%", Tone::Resist}}));
}

TEST(the_spawn_section_says_when_there_is_no_timer_no_placeholder_or_one_is_due)
{
    Settings settings;
    BarState state;
    const ActorInfo goblin = Goblin(); // respawn 300, no placeholder of an NM, no placeholders of its own
    state.target           = &goblin;
    const PopupContent goblinPopup = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(Lines(*Section(goblinPopup, "Spawn")) == (std::vector<std::string>{"Respawn 5:00", "Not a placeholder"}));
    const ActorInfo snipper = MobTarget(16781313); // respawn 0
    state.target            = &snipper;
    const PopupContent snipperPopup = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(Lines(*Section(snipperPopup, "Spawn")).front() == "No respawn timer");
    const ActorInfo worm  = MobTarget(17191194); // Bigmouth Billy's placeholder
    state.target          = &worm;
    state.targetPhSeconds = 0.0;
    const PopupContent wormPopup = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(Section(wormPopup, "Spawn")->rows[2].cells[0].text == (std::vector<Span>{{"Due now", Tone::Weak}}));
}

TEST(an_immunity_shows_a_cross_in_its_status_and_an_icon_under_immune)
{
    Settings settings;
    const MobRecord* mob = nullptr;
    for (uint32_t id = 0x1000000; id < 0x1200000 && mob == nullptr; ++id)
    {
        const MobRecord* m = MobById(id);
        if (m != nullptr && (ResistancesOf(*m).immunity & kImmuneParalyze) != 0) mob = m;
    }
    CHECK(mob != nullptr);
    ActorInfo target = Goblin();
    target.mob       = mob;
    target.serverId  = mob->id;
    BarState state;
    state.target             = &target;
    const PopupContent popup = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(Section(popup, "Statuses")->rows[0].cells[0].text == (std::vector<Span>{{"\xC3\x97", Tone::Immune}}));
    const PopupRow& immune = Section(popup, "Immune")->rows[0];
    CHECK(!immune.cells.empty());
    for (const PopupCell& cell : immune.cells)
        CHECK(cell.immune && cell.icon.has_value() && cell.text.empty());
}

TEST(vanadiel_time_names_the_day_in_its_elements_color_and_the_moon)
{
    Settings settings;
    BarState state;
    state.unixMs          = 1791417600000LL; // Darksday 09:00, 64% waxing
    const WidgetContent w = BarWidgetContent(BarWidget::VanaTime, state, settings);
    CHECK(w.title ==
          (std::vector<Span>{{"Darksday", Tone::Own, ElementColor(Element::Dark)}, {" 09:00", Tone::Normal}}));
    CHECK(w.subtitle == (std::vector<Span>{{"Waxing Gibbous 64%", Tone::Dim}}));
    const PopupContent popup = BarPopupContent(BarWidget::VanaTime, state, settings);
    CHECK(Joined(popup.title) == "1514/03/20");
    std::string all;
    for (const PopupRow& row : popup.sections[0].rows)
        for (const PopupCell& cell : row.cells)
            all += Joined(cell.text) + "|";
    CHECK(all.find("Waxing Gibbous (64%)") != std::string::npos);
    CHECK(all.find("Next day in 36:00") != std::string::npos);
    CHECK(all.find("Next phase in 10:12:00") != std::string::npos);
}

TEST(the_zone_widget_names_the_zone_and_its_region)
{
    Settings settings;
    BarState state;
    state.zone = 103;
    WidgetContent w = BarWidgetContent(BarWidget::Zone, state, settings);
    CHECK(Joined(w.title) == "Valkurm Dunes" && Joined(w.subtitle) == "Zulkheim" && w.hasPopup);
    state.zone = 999;
    w          = BarWidgetContent(BarWidget::Zone, state, settings);
    CHECK(Joined(w.title) == "Zone 999" && w.subtitle.empty());
    state.zone = 0;
    w          = BarWidgetContent(BarWidget::Zone, state, settings);
    CHECK(Joined(w.title) == "-" && !w.hasPopup);
}

TEST(gil_bags_and_earth_time)
{
    Settings settings;
    BarState state;
    CHECK(Joined(BarWidgetContent(BarWidget::Gil, state, settings).title) == "-");
    state.gil             = 1234567;
    const WidgetContent g = BarWidgetContent(BarWidget::Gil, state, settings);
    CHECK(g.coin && Joined(g.title) == "1,234,567" && g.subtitle.empty() && g.hasPopup);
    state.bags[0]         = BagCount{45, 80};
    state.bags[5]         = BagCount{30, 30}; // a full satchel
    const WidgetContent b = BarWidgetContent(BarWidget::Bags, state, settings);
    CHECK(Joined(b.title) == "45 / 80" && Joined(b.subtitle) == "Bags" && b.hasPopup);
    const PopupContent bagsPopup = BarPopupContent(BarWidget::Bags, state, settings);
    const PopupRow& rows         = bagsPopup.sections[0].rows[0];
    CHECK(rows.kind == RowKind::Grid && rows.columns == 2 && rows.cells.size() == 4);
    CHECK(Joined(rows.cells[2].text) == "Mog Satchel");
    CHECK(rows.cells[3].text == (std::vector<Span>{{"30 / 30", Tone::Resist}}));
    state.earthHour   = 21;
    state.earthMinute = 7;
    state.earthYear   = 2026;
    state.earthMonth  = 10;
    state.earthDay    = 8;
    CHECK(Joined(BarWidgetContent(BarWidget::EarthTime, state, settings).title) == "21:07");
    CHECK(Joined(BarPopupContent(BarWidget::EarthTime, state, settings).title) == "2026-10-08");
}

// As Ashita reports the inventory: gil is item 0xFFFF in its first slot, and a bag the character cannot use has no
// room.
TEST(gil_is_only_the_gil_item_and_a_bag_without_room_is_none)
{
    CHECK(GilOf(0xFFFF, 1234567) == 1234567u);
    CHECK(GilOf(0xFFFF, 0) == 0u);
    CHECK(!GilOf(4096, 12).has_value()); // a stack of fire crystals
    CHECK(!GilOf(0, 0).has_value());     // nothing there yet
    const std::optional<BagCount> satchel = BagOf(30, 80);
    CHECK(satchel.has_value() && satchel->used == 30 && satchel->max == 80);
    CHECK(BagOf(0, 30).has_value()); // empty, with room
    CHECK(!BagOf(0, 0).has_value()); // locked
}

TEST(each_widget_follows_its_switch_and_a_switched_off_one_builds_nothing)
{
    Settings settings;
    BarState state;
    state.zone = 103;
    CHECK(BarWidgetContent(BarWidget::Gil, state, settings).shown);
    for (WidgetOptions& each : settings.widgets)
        each.on = false;
    for (int i = 0; i < kBarWidgetCount; ++i)
    {
        const WidgetContent w = BarWidgetContent(static_cast<BarWidget>(i), state, settings);
        CHECK(!w.shown && w.title.empty() && w.subtitle.empty() && w.icons.empty() && !w.coin && !w.hasPopup);
    }
}

TEST(before_a_weather_is_known_the_widget_shows_the_days_likely_weather_and_its_chance)
{
    Settings settings;
    BarState state;
    state.zone   = 103;              // Valkurm Dunes, day 0: sunshine 50%, dust storm 50%
    state.unixMs = kVanaEpochUnixMs; // day 0
    const WidgetContent w = BarWidgetContent(BarWidget::Weather, state, settings);
    CHECK(w.shown && w.hasPopup && w.icons.empty());
    CHECK(Joined(w.title) == "Sunshine" && Joined(w.subtitle) == "50%");
}

TEST(a_known_weather_shows_its_element_once_or_twice_and_only_its_chance_today)
{
    Settings settings;
    BarState state;
    state.zone    = 103;
    state.unixMs  = kVanaEpochUnixMs;
    state.weather = Weather::DustStorm;
    WidgetContent w = BarWidgetContent(BarWidget::Weather, state, settings);
    CHECK(Joined(w.title) == "Dust Storm" && w.icons == (std::vector<Icon>{Icon::Earth}) &&
          Joined(w.subtitle) == "50%");
    state.weather = Weather::SandStorm;
    w             = BarWidgetContent(BarWidget::Weather, state, settings);
    CHECK(w.icons == (std::vector<Icon>{Icon::Earth, Icon::Earth}));
}

TEST(the_forecast_covers_today_and_four_more_days)
{
    Settings settings;
    BarState state;
    state.zone   = 103;
    state.unixMs = kVanaEpochUnixMs;
    const PopupContent popup = BarPopupContent(BarWidget::Weather, state, settings);
    const PopupSection* forecast = Section(popup, "Forecast");
    CHECK(forecast != nullptr);
    CHECK(forecast->rows[0].kind == RowKind::Grid && forecast->rows[0].columns == 4);
    const std::vector<PopupCell>& cells = forecast->rows[0].cells;
    CHECK(cells.size() == 20);
    CHECK(Joined(cells[0].text) == "Today");
    CHECK(Joined(cells[1].text) == "Sunshine 50%" && Joined(cells[2].text) == "Dust Storm 50%");
    CHECK(cells[3].text.empty()); // the day has two chances
    CHECK(Joined(cells[4].text) == "Earthsday");     // day 1
    CHECK(Joined(cells[8].text) == "Watersday");     // day 2: calendar 0x0421, Sunshine 100%
    CHECK(Joined(cells[9].text) == "Sunshine 100%");
    CHECK(cells[10].text.empty() && cells[11].text.empty());
    CHECK(Joined(cells[16].text) == "Iceday");       // day 4: calendar 0x0442, Sunshine 50% and Clouds 50%
    CHECK(Joined(cells[17].text) == "Sunshine 50%" && Joined(cells[18].text) == "Clouds 50%");
    CHECK(cells[19].text.empty());
}

TEST(the_forecast_names_the_weekdays_past_the_weeks_end)
{
    Settings settings;
    BarState state;
    state.zone   = 103;
    state.unixMs = 1009831536000LL; // epoch day 6, a Lightsday: the epoch and six days of 3456 Earth seconds
    const PopupContent popup = BarPopupContent(BarWidget::Weather, state, settings);
    std::vector<std::string> days;
    const std::vector<PopupCell>& cells = Section(popup, "Forecast")->rows[0].cells;
    for (size_t i = 0; i < cells.size(); i += 4) // the day cells come first in each row of four
        days.push_back(Joined(cells[i].text));
    CHECK(days == (std::vector<std::string>{"Today", "Darksday", "Firesday", "Earthsday", "Watersday"}));
}

TEST(the_reroll_note_is_in_earth_minutes_and_left_out_where_the_weather_never_changes)
{
    // data/phoenix_weather.tsv: Temenos (zone 37) has calendar 11, one day of Stellar Glare three times, which
    // Phoenix sets once and never rolls again; Valkurm Dunes rolls between Sunshine and Dust Storm on day 0.
    Settings settings;
    BarState state;
    state.unixMs = kVanaEpochUnixMs;
    state.zone   = 103;
    const PopupContent dunes = BarPopupContent(BarWidget::Weather, state, settings);
    CHECK(Lines(*Section(dunes, "Forecast")).back() == "Phoenix rolls the weather again every 3 to 30 Earth minutes.");
    state.zone = 37;
    const PopupContent temenos   = BarPopupContent(BarWidget::Weather, state, settings);
    const PopupSection* forecast = Section(temenos, "Forecast");
    CHECK_EQ(forecast->rows.size(), static_cast<size_t>(1));
    CHECK(Joined(forecast->rows[0].cells[1].text) == "Stellar Glare 100%");
}

TEST(a_zone_without_a_calendar_or_weather_hides_the_widget)
{
    Settings settings;
    BarState state;
    state.zone = 999;
    CHECK(!BarWidgetContent(BarWidget::Weather, state, settings).shown);
}

TEST(an_unknown_zone_has_no_weather_odds_of_its_own)
{
    Settings settings;
    BarState state;
    state.zone   = 0; // the data gives zone 0 calendar 0, which says Sunshine
    state.unixMs = kVanaEpochUnixMs;
    CHECK(!BarWidgetContent(BarWidget::Weather, state, settings).shown);
    state.weather = Weather::DustStorm;
    const WidgetContent w = BarWidgetContent(BarWidget::Weather, state, settings);
    CHECK(w.shown && Joined(w.title) == "Dust Storm" && w.icons == (std::vector<Icon>{Icon::Earth}));
    CHECK(w.subtitle.empty() && !w.hasPopup);
    const PopupContent popup = BarPopupContent(BarWidget::Weather, state, settings);
    const PopupSection* forecast = Section(popup, "Forecast");
    CHECK(forecast != nullptr);
    const std::vector<PopupCell>& cells = forecast->rows[0].cells;
    for (size_t i = 0; i < cells.size(); ++i)
        if (i % 4 != 0) CHECK(cells[i].text.empty()); // the day cells come first in each row of four
}

TEST(the_current_row_says_likely_until_the_server_gives_the_weather)
{
    Settings settings;
    BarState state;
    state.zone   = 103;
    state.unixMs = kVanaEpochUnixMs;
    const PopupContent before = BarPopupContent(BarWidget::Weather, state, settings);
    CHECK(before.sections[0].rows[0].cells[0].text ==
          (std::vector<Span>{{"Sunshine", Tone::Normal}, {" (likely)", Tone::Dim}}));
    state.weather = Weather::Sunshine;
    const PopupContent after = BarPopupContent(BarWidget::Weather, state, settings);
    CHECK(after.sections[0].rows[0].cells[0].text == (std::vector<Span>{{"Sunshine", Tone::Normal}}));
}

TEST(a_live_weather_outside_the_days_chances_still_shows)
{
    Settings settings;
    BarState state;
    state.zone    = 103;
    state.unixMs  = kVanaEpochUnixMs;
    state.weather = Weather::Rain; // day 0 is Sunshine or Dust Storm
    const WidgetContent w = BarWidgetContent(BarWidget::Weather, state, settings);
    CHECK(Joined(w.title) == "Rain" && w.icons == (std::vector<Icon>{Icon::Water}));
    CHECK(w.subtitle.empty()); // no chance of it today to give
    const PopupContent popup = BarPopupContent(BarWidget::Weather, state, settings);
    CHECK(Joined(popup.sections[0].rows[0].cells[0].text) == "Rain");
    CHECK(Joined(Section(popup, "Forecast")->rows[0].cells[1].text) == "Sunshine 50%");
}

TEST(the_zone_says_who_holds_its_region_once_known)
{
    Settings settings;
    BarState state;
    state.zone = 103; // Valkurm Dunes, Zulkheim (region 1)
    CHECK(Joined(BarWidgetContent(BarWidget::Zone, state, settings).subtitle) == "Zulkheim");
    RegionOwners owners{};
    owners[1]    = Nation::Bastok;
    state.owners = owners;
    const WidgetContent w = BarWidgetContent(BarWidget::Zone, state, settings);
    CHECK(Joined(w.subtitle) == "Zulkheim \xC2\xB7 Bastok");
    CHECK(w.subtitle.back() == (Span{"Bastok", Tone::Own, NationColor(Nation::Bastok)}));
    state.zone = 230; // Southern San d'Oria: a city region, not in the conquest packet
    CHECK(Joined(BarWidgetContent(BarWidget::Zone, state, settings).subtitle) == "San d'Oria");
}

TEST(the_zone_popup_has_the_zones_number_beside_its_name_over_its_region)
{
    Settings settings;
    BarState state;
    state.zone = 103; // Valkurm Dunes, Zulkheim (region 1)
    const PopupContent dunes = BarPopupContent(BarWidget::Zone, state, settings);
    CHECK(dunes.title == ListOf(Text("Valkurm Dunes"), Text(" 103", Tone::Dim)));
    CHECK(dunes.sections.size() == 1 && Lines(dunes.sections[0]) == (std::vector<std::string>{"Region Zulkheim"}));
    RegionOwners owners{};
    owners[1]    = Nation::Bastok;
    state.owners = owners; // who holds it is on the bar, and in the popup only beside its share
    CHECK(BarPopupContent(BarWidget::Zone, state, settings) == dunes);
    state.zone = 230; // Southern San d'Oria: a city region, outside the conquest
    const PopupContent city = BarPopupContent(BarWidget::Zone, state, settings);
    CHECK(Joined(city.title) == "Southern San d'Oria 230");
    CHECK(city.sections.size() == 1 && Lines(city.sections[0]) == (std::vector<std::string>{"Region San d'Oria"}));
    state.zone = 999; // past the data: its number is its title already, and it has no region
    const PopupContent unknown = BarPopupContent(BarWidget::Zone, state, settings);
    CHECK(Joined(unknown.title) == "Zone 999" && unknown.sections.empty());
}

TEST(the_zone_popup_ranks_the_influence_in_its_region_and_stars_who_holds_it)
{
    Settings settings;
    BarState state;
    state.zone   = 100;           // West Ronfaure, Ronfaure (region 0)
    state.unixMs = 1791527400000; // Friday 15:30 JST: 203,400 s to Monday 00:00 JST
    RegionOwners owners{};
    owners[0]    = Nation::SanDoria;
    state.owners = owners;
    CHECK(Section(BarPopupContent(BarWidget::Zone, state, settings), "Influence") == nullptr); // not known yet
    state.shares =
        RegionShares{{{Nation::SanDoria, 61}, {Nation::Bastok, 21}, {Nation::Windurst, 11}, {Nation::Beastmen, 5}}};
    const PopupContent popup       = BarPopupContent(BarWidget::Zone, state, settings);
    const PopupSection* influence = Section(popup, "Influence");
    CHECK(influence != nullptr);
    CHECK(Lines(popup.sections[0]) == (std::vector<std::string>{"Region Ronfaure"}));
    CHECK(Lines(*influence) == (std::vector<std::string>{"San d'Oria 61%", "Bastok 21%", "Windurst 11%", "Beastmen 5%",
                                   "Tally in 2d 8h"}));
    for (const PopupRow& row : influence->rows)
        CHECK(row.kind == RowKind::Pair); // each figure at the right edge
    CHECK(influence->rows[1].cells[0].text == ListOf(Colored("Bastok ", NationColor(Nation::Bastok))));
    // Which lines carry the holder's star, top to bottom.
    const auto starred = [&] {
        std::vector<bool> stars;
        const PopupContent now = BarPopupContent(BarWidget::Zone, state, settings); // named: the loop reads its rows
        for (const PopupRow& row : Section(now, "Influence")->rows)
            stars.push_back(row.cells[0].star);
        return stars;
    };
    CHECK(starred() == (std::vector<bool>{true, false, false, false, false}));
    (*state.owners)[0] = Nation::Beastmen; // a holder behind in the race keeps its place in it
    CHECK(starred() == (std::vector<bool>{false, false, false, true, false}));
    (*state.owners)[0] = Nation::None; // no one holds it
    CHECK(starred() == (std::vector<bool>{false, false, false, false, false}));
    (*state.owners)[0] = Nation::Windurst; // a new week: no influence yet, in the nations' order
    state.shares =
        RegionShares{{{Nation::SanDoria, 0}, {Nation::Bastok, 0}, {Nation::Windurst, 0}, {Nation::Beastmen, 0}}};
    CHECK(Lines(*Section(BarPopupContent(BarWidget::Zone, state, settings), "Influence")) ==
          (std::vector<std::string>{"San d'Oria 0%", "Bastok 0%", "Windurst 0%", "Beastmen 0%", "Tally in 2d 8h"}));
    CHECK(starred() == (std::vector<bool>{false, false, true, false, false}));
    state.zone = 230; // Southern San d'Oria: a city region, outside the conquest
    CHECK(Section(BarPopupContent(BarWidget::Zone, state, settings), "Influence") == nullptr);
}

namespace
{
    // You, a healer low on HP and MP, a red mage, and a black mage in Aht Urhgan Whitegate.
    Party FourMembers()
    {
        Party p{};
        p[0] = PartyMember{"Tanyrus", 1, 75, 13, 37, 1800, 0, 1200, 90, 0, 103, true};
        p[1] = PartyMember{"Healer", 3, 75, 4, 37, 520, 180, 0, 42, 18, 103, true};
        p[2] = PartyMember{"Redmage", 5, 75, 3, 37, 1000, 600, 300, 100, 60, 103, true};
        p[3] = PartyMember{"Away", 4, 75, 3, 37, 100, 50, 0, 10, 5, 50, false};
        return p;
    }

    std::string CellText(const PopupCell& cell) { return Joined(cell.text); }

    ConnectionReport SteadyLink()
    {
        ConnectionReport r;
        r.counters       = NetCounters{227, 175, 100};
        r.silence        = 0.3;
        r.latency        = Latency{48.0, 40.0, 66.0};
        r.receivedBytes  = 35328.0;
        r.sentBytes      = 1258291.0;
        r.countedSeconds = 4320.0;
        return r;
    }

    // Every part of the bar's state set, each to something its widgets show.
    struct FullState
    {
        ActorInfo target = Goblin();
        BarState state;
        FullState()
        {
            // East Ronfaure's Carrion Worm, one of Bigmouth Billy's placeholders: its popup counts down.
            target.mob      = MobById(17191194);
            target.serverId = 17191194;
            state.unixMs    = 1791533220000; // 2026-10-09 08:07 UTC, a day Valkurm Dunes may have clouds
            state.earthYear = 2026, state.earthMonth = 10, state.earthDay = 8;
            state.earthHour = 23, state.earthMinute = 7;
            state.zone            = 103; // Valkurm Dunes, Zulkheim
            state.target          = &target;
            state.targetPhSeconds = 299.0;
            state.targetDistance  = 8.5f;
            state.gil             = 1234567;
            for (int c = 0; c < kContainerCount; ++c)
                state.bags[c] = BagCount{static_cast<uint32_t>(c + 1), 80};
            RegionOwners owners{};
            owners[1]     = Nation::Bastok;
            state.owners  = owners;
            state.shares =
                RegionShares{{{Nation::Bastok, 52}, {Nation::SanDoria, 30}, {Nation::Windurst, 10}, {Nation::Beastmen, 8}}};
            state.weather = Weather::DustStorm;
            state.strip    = StripText{"Mog Wardrobe 8", "Help on the selected item"};
            state.stripBag = BagCount{3, 80};
            state.frames   = FrameReport{1000.0 / 30.0, 120.0};
            JobSample job;
            job.mainJob = 1, job.mainLevel = 30, job.subJob = 13, job.subLevel = 15;
            job.exp = 4140, job.expNeeded = 7600;
            state.job = JobReport{job, 18000.0};
            state.connection = SteadyLink();
            state.party      = FourMembers();
            state.signet     = SignetState{Influence::Signet, 6130.0};
            state.memory     = MemoryReport{1532, 2563, 4095};
            state.cpuPercent = 18.0;
            LinuxXIReport report;
            report.flags      = kPoolHooked | kPoolOn | kGraceActive | kGraceOn;
            report.copiesHeld = 588;
            report.addonCount = 1;
            report.lua[0]     = LinuxXIAddon{"xiui", 41250};
            state.linuxxi.report = report;
        }
    };

    // What the plugin gathers for needs: the rest stays as a BarState starts.
    BarState Gathered(const BarState& full, const BarNeeds& needs)
    {
        BarState s;
        s.weather = full.weather;
        s.owners  = full.owners;
        s.shares   = full.shares;
        if (needs.unixTime) s.unixMs = full.unixMs;
        if (needs.localTime)
        {
            s.earthYear   = full.earthYear;
            s.earthMonth  = full.earthMonth;
            s.earthDay    = full.earthDay;
            s.earthHour   = full.earthHour;
            s.earthMinute = full.earthMinute;
        }
        if (needs.zone) s.zone = full.zone;
        if (needs.target)
        {
            s.target          = full.target;
            s.targetPhSeconds = full.targetPhSeconds;
            s.targetDistance  = full.targetDistance;
            s.targetCheck     = full.targetCheck;
        }
        if (needs.gil) s.gil = full.gil;
        for (int c = 0; c < needs.containers; ++c)
            s.bags[c] = full.bags[c];
        if (needs.strip)
        {
            s.strip    = full.strip;
            s.stripBag = full.stripBag;
        }
        if (needs.frames) s.frames = full.frames;
        if (needs.job) s.job = full.job;
        if (needs.connection) s.connection = full.connection;
        if (needs.party) s.party = full.party;
        if (needs.signet) s.signet = full.signet;
        if (needs.memory) s.memory = full.memory;
        if (needs.cpu) s.cpuPercent = full.cpuPercent;
        if (needs.linuxxi) s.linuxxi = full.linuxxi;
        return s;
    }

    // needs with one of its parts taken away, for each part it has.
    std::vector<BarNeeds> EachOneLess(const BarNeeds& needs)
    {
        std::vector<BarNeeds> less;
        for (bool BarNeeds::*part :
            {&BarNeeds::unixTime, &BarNeeds::localTime, &BarNeeds::zone, &BarNeeds::target, &BarNeeds::gil,
            &BarNeeds::strip, &BarNeeds::frames, &BarNeeds::job, &BarNeeds::connection,
            &BarNeeds::party, &BarNeeds::signet, &BarNeeds::memory, &BarNeeds::cpu, &BarNeeds::linuxxi})
            if (needs.*part)
            {
                less.push_back(needs);
                less.back().*part = false;
            }
        if (needs.containers > 0)
        {
            less.push_back(needs);
            less.back().containers = needs.containers == kContainerCount ? 1 : 0;
        }
        return less;
    }

    // What the bar shows of widget from state: the widget, or Game help's own bar, and its popup when open.
    bool SameShown(BarWidget widget, std::optional<PanelWidget> open, const BarState& a, const BarState& b,
        const Settings& settings)
    {
        const bool helpBar = widget != BarWidget::GameHelp || (a.strip == b.strip && StripBagFill(a) == StripBagFill(b));
        return helpBar && BarWidgetContent(widget, a, settings) == BarWidgetContent(widget, b, settings) &&
               (!open || BarPopupContent(*open, a, settings) == BarPopupContent(*open, b, settings));
    }
}

TEST(the_bar_gathers_what_each_shown_widget_and_open_popup_reads_and_nothing_more)
{
    const FullState full;
    for (int i = 0; i < kBarWidgetCount; ++i)
    {
        const auto widget = static_cast<BarWidget>(i);
        Settings settings;
        for (WidgetOptions& each : settings.widgets)
            each.on = false;
        settings.Widget(widget).on = true;
        const bool hasPopup = BarWidgetContent(widget, full.state, settings).hasPopup;
        for (const std::optional<PanelWidget> open : {std::optional<PanelWidget>(), std::optional<PanelWidget>(widget)})
        {
            if (open && !hasPopup) continue;
            const BarNeeds needs = NeedsOf(settings, open);
            CHECK(SameShown(widget, open, full.state, Gathered(full.state, needs), settings));
            for (const BarNeeds& less : EachOneLess(needs))
                CHECK(!SameShown(widget, open, full.state, Gathered(full.state, less), settings));
        }
    }
}

TEST(the_other_bags_are_gathered_only_while_their_popup_is_open)
{
    Settings settings;
    CHECK(NeedsOf(settings, std::nullopt).containers == 1);
    CHECK(NeedsOf(settings, BarWidget::Target).containers == 1);
    CHECK(NeedsOf(settings, BarWidget::Bags).containers == kContainerCount);
    for (WidgetOptions& each : settings.widgets)
        each.on = false;
    const BarNeeds none = NeedsOf(settings, BarWidget::Bags); // a popup open as its widget is switched off
    CHECK(!none.unixTime && !none.localTime && !none.zone && !none.target && !none.gil && none.containers == 0);
}

TEST(a_caption_names_a_bag_only_when_it_is_that_bags_name)
{
    CHECK(ContainerNamed("Inventory") == 0 && ContainerNamed("Mog Wardrobe 8") == 16);
    CHECK(ContainerNamed("Items") == 0); // the Items menu as it opens: its list is the inventory's
    // The Mog House's own short captions for some of them.
    CHECK(ContainerNamed("Locker") == 4 && ContainerNamed("M.Ward.") == 8 && ContainerNamed("M.Ward. 2") == 10);
    CHECK(ContainerNamed("M.Ward. 8") == 16 && !ContainerNamed("M.Ward. 9").has_value());
    CHECK(ContainerNamed("Satchel") == 5 && ContainerNamed("Mog Sack") == 6); // as View House's lists title them
    CHECK(!ContainerNamed("Inventory 1/30").has_value()); // the game's own count, if it is there, stays the game's
    CHECK(!ContainerNamed("Commands").has_value() && !ContainerNamed("").has_value());
}

TEST(game_help_is_never_on_the_bar_and_its_popup_has_the_caption_over_the_help)
{
    Settings settings;
    BarState state;
    state.strip = StripText{"Commands", "Select a target to auto-attack."};
    CHECK(!BarWidgetContent(BarWidget::GameHelp, state, settings).shown); // it has its own bar
    const PopupContent p = BarPopupContent(BarWidget::GameHelp, state, settings);
    CHECK(Joined(p.title) == "Commands" && Lines(p.sections[0]) == (std::vector<std::string>{"Select a target to auto-attack."}));
}

TEST(a_bag_caption_gains_that_bags_count)
{
    Settings settings;
    BarState state;
    state.strip    = StripText{"Inventory", "Bronze Sword"};
    state.stripBag = BagCount{12, 30};
    CHECK(StripBagFill(state) && StripBagFill(state)->text == "12 / 30");
    CHECK(Lines(BarPopupContent(BarWidget::GameHelp, state, settings).sections[0]) ==
          (std::vector<std::string>{"Bronze Sword", "Items 12 / 30"}));
}

TEST(a_help_line_without_a_caption_wraps_in_the_popup_under_a_fixed_title)
{
    Settings settings;
    BarState state;
    const std::string help = "Select a target to auto-attack, or choose an ally to cast on, then confirm with the enter key "
                             "to begin. Press cancel to return to the previous menu.";
    state.strip = StripText{"", help};
    const PopupContent p = BarPopupContent(BarWidget::GameHelp, state, settings);
    CHECK(Joined(p.title) == "Game help");
    const std::vector<std::string> lines = Lines(p.sections[0]);
    CHECK(lines.size() == 2);
    for (const std::string& line : lines)
        CHECK(line.size() <= 80);
    CHECK(lines[0] + " " + lines[1] == help);
}

TEST(the_clock_can_stack_the_servers_time_under_its_own_and_its_popup_has_the_resets)
{
    Settings settings;
    BarState state;
    state.unixMs    = 1791527400000 + 42000; // Friday 15:30:42 JST, Thursday 23:30:42 on the US Pacific coast
    state.earthYear = 2026, state.earthMonth = 10, state.earthDay = 9; // the computer's own: 08:30:42 in Berlin
    state.earthHour = 8, state.earthMinute = 30, state.earthSecond = 42;
    WidgetContent w = BarWidgetContent(BarWidget::EarthTime, state, settings);
    CHECK(w.shown && w.hasPopup && Joined(w.title) == "08:30" && w.subtitle.empty()); // not stacked unless asked
    settings.Widget(BarWidget::EarthTime).clockServerTime = true;
    w                        = BarWidgetContent(BarWidget::EarthTime, state, settings);
    CHECK(Joined(w.title) == "08:30" && w.subtitle == ListOf(Text("JST 15:30", Tone::Dim)));
    settings.Widget(BarWidget::EarthTime).clockSeconds = true; // the seconds are the upper line's
    settings.Widget(BarWidget::EarthTime).clock24      = false;
    w                     = BarWidgetContent(BarWidget::EarthTime, state, settings);
    CHECK(Joined(w.title) == "8:30:42 AM" && Joined(w.subtitle) == "JST 3:30 PM");
    settings.Widget(BarWidget::EarthTime).clockZone = ClockZone::UsPacific;
    w                  = BarWidgetContent(BarWidget::EarthTime, state, settings);
    CHECK(Joined(w.title) == "PDT 11:30:42 PM" && Joined(w.subtitle) == "JST 3:30 PM");
    // The popup has the server's clock and its resets whether stacked or not: 8h 29m 18s to Saturday 00:00 JST.
    for (const bool stacked : {true, false})
    {
        settings.Widget(BarWidget::EarthTime).clockServerTime = stacked;
        const PopupContent p     = BarPopupContent(BarWidget::EarthTime, state, settings);
        CHECK(Joined(p.title) == "2026-10-08"); // the Pacific coast's date
        CHECK(p.sections.size() == 1 && Lines(p.sections[0]) == (std::vector<std::string>{"Server time Friday 15:30 JST",
                                                                    "Daily reset in 8h 29m", "Conquest tally in 2d 8h"}));
    }
}

TEST(the_fps_widget_shows_frames_a_second_and_its_popup_the_frame_times)
{
    Settings settings;
    BarState state;
    CHECK(Joined(BarWidgetContent(BarWidget::Fps, state, settings).title) == "-");
    CHECK(!BarWidgetContent(BarWidget::Fps, state, settings).hasPopup);
    state.frames = FrameReport{1000.0 / 30.0, 120.0};
    const WidgetContent w = BarWidgetContent(BarWidget::Fps, state, settings);
    CHECK(w.shown && w.hasPopup && w.figures && Joined(w.title) == "30 fps" && w.subtitle.empty());
    const PopupContent p = BarPopupContent(BarWidget::Fps, state, settings);
    CHECK(Joined(p.title) == "Frame rate");
    CHECK(Lines(p.sections[0]) == (std::vector<std::string>{"Frame time 33.3 ms", "Slowest frame 120.0 ms"}));
}

TEST(the_job_widget_shows_jobs_and_exp_to_next_or_merits_at_the_cap_over_a_bar_of_either)
{
    Settings settings;
    BarState state;
    CHECK(Joined(BarWidgetContent(BarWidget::Job, state, settings).title) == "-"); // no job: the title screen
    JobSample s;
    s.mainJob = 1, s.mainLevel = 30, s.subJob = 13, s.subLevel = 15;
    s.exp = 4140, s.expNeeded = 7600;
    state.job = JobReport{s, 18000.0};
    const WidgetContent w = BarWidgetContent(BarWidget::Job, state, settings);
    CHECK(w.shown && w.hasPopup && w.figures && Joined(w.title) == "WAR 30 / NIN 15" &&
          Joined(w.subtitle) == "3,460 EXP to next");
    CHECK(w.progress && std::fabs(*w.progress - 4140.0f / 7600.0f) < 1e-6f); // the bar under it
    CHECK(Joined(BarPopupContent(BarWidget::Job, state, settings).title) == "Job and EXP"); // words: the title font
    CHECK(Lines(BarPopupContent(BarWidget::Job, state, settings).sections[0]) ==
          (std::vector<std::string>{"EXP 4,140 / 7,600", "To next 3,460", "Per hour 18,000", "Next level in 11m 32s",
              "Limit points 0 / 10,000"})); // 7,600 - 4,140 = 3,460; at 18,000 an hour that is 692 s

    s.mainLevel = 75, s.subLevel = 37, s.merits = 7, s.meritsMax = 10, s.limitPoints = 4210;
    state.job = JobReport{s, std::nullopt};
    CHECK(Joined(BarWidgetContent(BarWidget::Job, state, settings).subtitle) == "7 merits");
    CHECK(BarWidgetContent(BarWidget::Job, state, settings).progress == 0.421f); // limit points to the next merit
    CHECK(Lines(BarPopupContent(BarWidget::Job, state, settings).sections[0]) ==
          (std::vector<std::string>{"Merits 7 / 10", "Limit points 4,210 / 10,000", "Per hour -",
              "Limit Mode is off, so EXP does not become merits."}));
}

TEST(the_job_widget_says_one_merit_and_promises_no_merit_without_limit_mode)
{
    Settings settings;
    BarState state;
    JobSample s;
    s.mainJob = 1, s.mainLevel = 75, s.merits = 1, s.meritsMax = 10, s.limitPoints = 100;
    state.job = JobReport{s, 5000.0};
    CHECK(Joined(BarWidgetContent(BarWidget::Job, state, settings).subtitle) == "1 merit");
    for (const std::string& line : Lines(BarPopupContent(BarWidget::Job, state, settings).sections[0]))
        CHECK(line.find("Next merit") == std::string::npos); // Limit Mode is off
    state.job->sample.limitMode = true;
    const auto lines = Lines(BarPopupContent(BarWidget::Job, state, settings).sections[0]);
    CHECK(std::any_of(lines.begin(), lines.end(), [](const std::string& l) { return l.find("Next merit in") == 0; }));
}

TEST(the_connection_widget_shows_the_games_rates_on_one_line_and_turns_amber_then_red)
{
    Settings settings;
    BarState state;
    state.connection = SteadyLink();
    WidgetContent w  = BarWidgetContent(BarWidget::Connection, state, settings);
    CHECK(w.shown && w.hasPopup && w.figures && Joined(w.title) == "S 175  R 227  100%" && w.subtitle.empty());
    CHECK(w.title[1] == Text("175"));

    state.connection->silence = 3.0;
    w = BarWidgetContent(BarWidget::Connection, state, settings);
    CHECK(w.title[1] == Text("175")); // the game's counters still see data arrive
    state.connection->counters->received = 0;
    w = BarWidgetContent(BarWidget::Connection, state, settings);
    CHECK(w.title[1] == Colored("175", kWarningColor) && w.title[3] == Colored("0", kWarningColor));

    state.connection->silence          = 13.0;
    state.connection->counters->health = 94;
    w = BarWidgetContent(BarWidget::Connection, state, settings);
    CHECK(w.title[1] == Colored("175", kAlarmColor) && w.subtitle.empty());

    state.connection->counters.reset(); // the chain could not be read
    state.connection->silence = 0.3;
    CHECK(Joined(BarWidgetContent(BarWidget::Connection, state, settings).title) == "S -  R -");
}

TEST(the_connection_popup_gives_the_rates_latency_last_data_and_session_totals)
{
    Settings settings;
    BarState state;
    state.connection     = SteadyLink();
    const PopupContent p = BarPopupContent(BarWidget::Connection, state, settings);
    CHECK(Joined(p.title) == "Connection" && p.sections.size() == 2);
    // Short lines only: no sentence on where S and R come from (the menu tip says it).
    CHECK(Lines(p.sections[0]) == (std::vector<std::string>{"Sent 175 B/s", "Received 227 B/s", "Latency 48 ms",
                                      "40 to 66 ms over recent replies", "Last server data 1s"})); // 0.3 s, rounded up
    CHECK(p.sections[0].rows[4] == Labeled("Last server data", Text("1s")));
    CHECK(p.sections[1].heading == "This session");
    CHECK(Lines(p.sections[1]) == (std::vector<std::string>{"Sent 1.2 MB", "Received 34.5 KB", "Over 1h 12m"}));
}

TEST(the_connection_popup_colors_the_last_server_data_as_the_bar_and_names_the_meter_only_under_100)
{
    Settings settings;
    BarState state;
    state.connection          = SteadyLink();
    state.connection->silence = 17.4;
    state.connection->counters->received = 0;
    PopupContent p            = BarPopupContent(BarWidget::Connection, state, settings);
    CHECK(p.sections[0].rows[4] ==
          Line(Cell(ListOf(Colored("Last server data ", kWarningColor), Colored("18s", kWarningColor)))));
    CHECK(p.sections[0].rows.size() == 5); // the meter is still full

    state.connection->counters->health = 94;
    p = BarPopupContent(BarWidget::Connection, state, settings);
    CHECK(p.sections[0].rows[4] ==
          Line(Cell(ListOf(Colored("Last server data ", kAlarmColor), Colored("18s", kAlarmColor)))));
    CHECK(Lines(p.sections[0]).back() == "The game's own meter is at 94%: it falls once 10 s pass without server data.");

    state.connection->zoning = true; // no color while zoning; the meter line stays while the meter is under 100
    p = BarPopupContent(BarWidget::Connection, state, settings);
    CHECK(p.sections[0].rows[4] == Labeled("Last server data", Text("18s")) && p.sections[0].rows.size() == 6);
}

TEST(the_party_widget_counts_members_and_shows_the_lowest_hp_and_mp_in_your_zone)
{
    Settings settings;
    settings.Widget(BarWidget::Party).on = true;
    BarState state;
    state.party           = FourMembers();
    const WidgetContent w = BarWidgetContent(BarWidget::Party, state, settings);
    CHECK(w.shown && w.hasPopup && w.figures && Joined(w.title) == "Party 4" &&
          Joined(w.subtitle) == "HP 42% \xC2\xB7 MP 18%");
    CHECK(w.subtitle[1] == Colored("42%", kWarningColor) && w.subtitle[4] == Colored("18%", kAlarmColor));

    state.party[1] = PartyMember{"Samurai", 12, 75, 1, 37, 1500, 0, 0, 100, 0, 103, true};
    state.party[2].reset();
    state.party[3].reset();
    CHECK(Joined(BarWidgetContent(BarWidget::Party, state, settings).subtitle) == "HP 90%"); // no MP among them

    state.party[1].reset();
    const WidgetContent solo = BarWidgetContent(BarWidget::Party, state, settings);
    CHECK(Joined(solo.title) == "Solo" && solo.subtitle.empty() && !solo.hasPopup);
}

TEST(the_party_popup_lists_each_member_here_and_gives_one_elsewhere_a_row_of_its_own)
{
    Settings settings;
    BarState state;
    state.party          = FourMembers();
    const PopupContent p = BarPopupContent(BarWidget::Party, state, settings);
    CHECK(Joined(p.title) == "Party" && p.sections[0].rows.size() == 2);
    const PopupRow& grid = p.sections[0].rows[0];
    CHECK(grid.kind == RowKind::Grid && grid.columns == 5 && grid.cells.size() == 20); // a heading row, then three
    CHECK(CellText(grid.cells[2]) == "HP" && CellText(grid.cells[4]) == "TP");
    CHECK(CellText(grid.cells[5]) == "Tanyrus" && CellText(grid.cells[6]) == "WAR 75 / NIN 37");
    CHECK(grid.cells[8].text == ListOf(Text("-", Tone::Dim)));          // a warrior has no MP
    CHECK(grid.cells[9].text == ListOf(Colored("1,200", kReadyColor))); // TP enough for a weapon skill
    CHECK(grid.cells[12].text == ListOf(Colored("520", kWarningColor))); // the healer's HP at 42%
    CHECK(grid.cells[13].text == ListOf(Colored("180", kAlarmColor)));   // and MP at 18%
    CHECK(CellText(grid.cells[15]) == "Redmage");
    // The member elsewhere: their name and zone on one row, after the grid, their stale figures left out.
    const PopupRow& away = p.sections[0].rows[1];
    CHECK(away.kind == RowKind::Line && away.cells.size() == 2);
    CHECK(away.cells[0].text == ListOf(Text("Away")) &&
          away.cells[1].text == ListOf(Text("In Aht Urhgan Whitegate", Tone::Dim)));
}

TEST(a_member_zoning_or_in_another_zone_never_counts_toward_the_lowest_figures)
{
    Settings settings;
    settings.Widget(BarWidget::Party).on = true;
    BarState state;
    state.party[0] = PartyMember{"Tanyrus", 4, 75, 3, 37, 1800, 900, 0, 80, 70, 103, true};
    state.party[1] = PartyMember{"Zoning", 3, 75, 4, 37, 0, 0, 0, 0, 0, 0, false};
    state.party[2] = PartyMember{"Away", 3, 75, 4, 37, 10, 10, 0, 1, 1, 50, false};
    const WidgetContent w = BarWidgetContent(BarWidget::Party, state, settings);
    CHECK(Joined(w.title) == "Party 3" && Joined(w.subtitle) == "HP 80% \xC2\xB7 MP 70%");
    PopupContent popup     = BarPopupContent(BarWidget::Party, state, settings);
    const PopupRow& zoning = popup.sections[0].rows[1];
    CHECK(zoning.cells[0].text == ListOf(Text("Zoning")) && zoning.cells[1].text == ListOf(Text("Zoning", Tone::Dim)));
    state.party[1]->zone = 9999; // a zone the tables lack
    popup = BarPopupContent(BarWidget::Party, state, settings);
    const PopupRow& unknown = popup.sections[0].rows[1];
    CHECK(unknown.cells[1].text == ListOf(Text("Zone 9999", Tone::Dim)));
}

TEST(a_dead_member_counts_and_turns_the_face_red)
{
    Settings settings;
    settings.Widget(BarWidget::Party).on = true;
    BarState state;
    state.party[0] = PartyMember{"Tanyrus", 4, 75, 3, 37, 1800, 900, 0, 80, 70, 103, true};
    state.party[1] = PartyMember{"Dead", 1, 75, 0, 0, 0, 0, 0, 0, 0, 103, true};
    const WidgetContent w = BarWidgetContent(BarWidget::Party, state, settings);
    CHECK(Joined(w.subtitle) == "HP 0% \xC2\xB7 MP 70%" && w.subtitle[1] == Colored("0%", kAlarmColor));
}

TEST(the_figures_turn_amber_below_half_and_red_below_a_quarter)
{
    Settings settings;
    settings.Widget(BarWidget::Party).on = true;
    BarState state;
    state.party[0] = PartyMember{"Tanyrus", 4, 75, 3, 37, 1, 1, 0, 50, 25, 103, true};
    state.party[1] = PartyMember{"Other", 4, 75, 3, 37, 1, 1, 0, 100, 100, 103, true};
    WidgetContent w = BarWidgetContent(BarWidget::Party, state, settings);
    CHECK(w.subtitle[1] == Text("50%") && w.subtitle[4] == Colored("25%", kWarningColor));
    state.party[0]->hpPercent = 49, state.party[0]->mpPercent = 24;
    w = BarWidgetContent(BarWidget::Party, state, settings);
    CHECK(w.subtitle[1] == Colored("49%", kWarningColor) && w.subtitle[4] == Colored("24%", kAlarmColor));
}

TEST(a_party_with_everyone_else_away_shows_only_your_own_figures)
{
    Settings settings;
    settings.Widget(BarWidget::Party).on = true;
    BarState state;
    state.party[0] = PartyMember{"Tanyrus", 1, 75, 13, 37, 1800, 0, 0, 90, 0, 103, true};
    state.party[1] = PartyMember{"Away", 4, 75, 3, 37, 100, 50, 0, 10, 5, 50, false};
    const WidgetContent w = BarWidgetContent(BarWidget::Party, state, settings);
    CHECK(Joined(w.title) == "Party 2" && Joined(w.subtitle) == "HP 90%");
}

TEST(a_member_whose_data_has_not_arrived_is_left_out_of_the_lowest_and_shows_dashes)
{
    Settings settings;
    settings.Widget(BarWidget::Party).on = true;
    BarState state;
    state.party[0] = PartyMember{"Tanyrus", 4, 75, 3, 37, 1800, 900, 0, 80, 70, 103, true};
    state.party[1] = PartyMember{"New", 0, 0, 0, 0, 0, 0, 0, 0, 0, 103, true}; // no job yet
    const WidgetContent w = BarWidgetContent(BarWidget::Party, state, settings);
    CHECK(Joined(w.title) == "Party 2" && Joined(w.subtitle) == "HP 80% \xC2\xB7 MP 70%");
    const PopupContent p = BarPopupContent(BarWidget::Party, state, settings);
    const PopupRow& grid = p.sections[0].rows[0];
    CHECK(grid.cells.size() == 15 && CellText(grid.cells[10]) == "New");
    for (int c = 11; c < 15; ++c)
        CHECK(grid.cells[c].text == ListOf(Text("-", Tone::Dim)));
}

TEST(a_checked_player_shows_their_jobs_and_a_popup_of_the_check)
{
    Settings settings;
    BarState state;
    ActorInfo player;
    player.kind     = EntityKind::Player;
    player.serverId = 0x01006A2B;
    std::snprintf(player.name, sizeof(player.name), "%s", "Tanyrus");
    state.target    = &player;
    WidgetContent w = BarWidgetContent(BarWidget::Target, state, settings);
    CHECK(Joined(w.title) == "Tanyrus" && w.subtitle.empty() && !w.hasPopup); // never checked: as before
    PlayerCheck check{InspectGeneral{0x01006A2B, 0x410, "Phoenix", 1, 13, 75, 37, 0}, InspectMessage{"Tanyrus", 206}};
    state.targetCheck = check;
    w = BarWidgetContent(BarWidget::Target, state, settings);
    CHECK(Joined(w.subtitle) == "WAR 75 / NIN 37" && w.hasPopup);
    CHECK(Lines(BarPopupContent(BarWidget::Target, state, settings).sections[0]) ==
          (std::vector<std::string>{"Jobs WAR 75 / NIN 37", "Linkshell Phoenix", "Title New Adventurer",
              "As of your check"}));
    check.general.masterLevel = 5;
    check.general.linkshell.clear();
    state.targetCheck = check;
    CHECK(Lines(BarPopupContent(BarWidget::Target, state, settings).sections[0]) ==
          (std::vector<std::string>{"Jobs WAR 75 / NIN 37", "Master level 5", "Title New Adventurer",
              "As of your check"}));
    check.general.mainJob = check.general.subJob = check.general.mainLevel = check.general.subLevel = 0;
    state.targetCheck = check; // anonymous
    CHECK(Joined(BarWidgetContent(BarWidget::Target, state, settings).subtitle) == "? ? ?");
}

TEST(a_checked_players_title_shows_its_number_when_the_list_lacks_it_and_not_at_all_when_none)
{
    Settings settings;
    BarState state;
    ActorInfo player;
    player.kind     = EntityKind::Player;
    player.serverId = 0x01006A2B;
    state.target    = &player;
    state.targetCheck = PlayerCheck{InspectGeneral{0x01006A2B, 0x410, "", 1, 13, 75, 37, 0}, InspectMessage{"", 99999}};
    CHECK(Lines(BarPopupContent(BarWidget::Target, state, settings).sections[0]) ==
          (std::vector<std::string>{"Jobs WAR 75 / NIN 37", "Title #99999", "As of your check"}));
    state.targetCheck->message.title = 0;
    CHECK(Lines(BarPopupContent(BarWidget::Target, state, settings).sections[0]) ==
          (std::vector<std::string>{"Jobs WAR 75 / NIN 37", "As of your check"}));
}

TEST(the_signet_widget_shows_only_while_one_is_on_and_says_where_it_works)
{
    Settings settings;
    settings.Widget(BarWidget::Signet).on = true;
    BarState state;
    CHECK(!BarWidgetContent(BarWidget::Signet, state, settings).shown);
    state.signet    = SignetState{Influence::Signet, 6130.0};
    WidgetContent w = BarWidgetContent(BarWidget::Signet, state, settings);
    CHECK(w.shown && w.hasPopup && Joined(w.title) == "Signet" && w.subtitle == ListOf(Text("1h 42m", Tone::Dim)));
    state.signet->secondsLeft = 540.0;
    CHECK(BarWidgetContent(BarWidget::Signet, state, settings).subtitle == ListOf(Colored("9m 00s", kWarningColor)));
    state.zone = 103; // Valkurm Dunes, in Zulkheim
    // Short lines only: no sentence on where it works beyond the zone you are in.
    CHECK(Lines(BarPopupContent(BarWidget::Signet, state, settings).sections[0]) ==
          (std::vector<std::string>{"Time left 9m 00s", "Gives conquest points, crystals",
              "Here: Zulkheim, where it works"}));
    state.zone = 50; // Aht Urhgan Whitegate
    CHECK(Lines(BarPopupContent(BarWidget::Signet, state, settings).sections[0]).back() ==
          "Here: West Aht Urhgan, where it does not work");
    state.signet = SignetState{Influence::Sanction, std::nullopt};
    w            = BarWidgetContent(BarWidget::Signet, state, settings);
    CHECK(Joined(w.title) == "Sanction" && Joined(w.subtitle) == "-");
    CHECK(Lines(BarPopupContent(BarWidget::Signet, state, settings).sections[0]).back() ==
          "Here: West Aht Urhgan, where it works");
}

TEST(the_gil_widget_shows_the_picked_currency_and_its_popup_every_one_you_have)
{
    Settings settings;
    BarState state;
    state.gil = 4751;
    PopupContent p = BarPopupContent(BarWidget::Gil, state, settings);
    CHECK(Joined(p.title) == "Currencies" &&
          Lines(p.sections[0]) == (std::vector<std::string>{"Gil 4,751", "The rest show once the server sends them."}));

    const int notes = *CurrencyOfKey("allied_notes"), sparks = *CurrencyOfKey("sparks");
    settings.Widget(BarWidget::Gil).currency = notes;
    WidgetContent w      = BarWidgetContent(BarWidget::Gil, state, settings);
    CHECK(!w.coin && Joined(w.title) == "-" && Joined(w.subtitle) == "Allied Notes"); // not sent yet
    for (std::optional<uint32_t>& amount : state.currencies)
        amount = 0; // the server sent them all, most at none
    state.currencies[notes]  = 12000;
    state.currencies[sparks] = 350;
    w = BarWidgetContent(BarWidget::Gil, state, settings);
    CHECK(Joined(w.title) == "12,000" && w.hasPopup);
    p = BarPopupContent(BarWidget::Gil, state, settings);
    CHECK(Lines(p.sections[0]) == (std::vector<std::string>{"Gil 4,751", "Sparks of Eminence 350", "Allied Notes 12,000"}));
}

TEST(the_distance_widget_shows_how_far_the_target_is_and_hides_without_one)
{
    Settings settings;
    BarState state;
    CHECK(!BarWidgetContent(BarWidget::Distance, state, settings).shown);
    state.targetDistance = 12.34f;
    const WidgetContent w = BarWidgetContent(BarWidget::Distance, state, settings);
    CHECK(w.shown && !w.hasPopup && Joined(w.title) == "12.3 yalms" && w.subtitle.empty());
    CHECK(w.title[1] == Text(" yalms", Tone::Dim));
}

TEST(each_widget_can_drop_its_second_line_or_its_icons)
{
    Settings settings;
    const ActorInfo goblin = Goblin();
    BarState state;
    state.target = &goblin;
    state.gil    = 100;
    settings.Widget(BarWidget::Target).secondLine = false;
    WidgetContent w = BarWidgetContent(BarWidget::Target, state, settings);
    CHECK(w.subtitle.empty() && !w.icons.empty() && Joined(w.title) == "Goblin Bounty Hunter");
    settings.Widget(BarWidget::Target).icons = false;
    CHECK(BarWidgetContent(BarWidget::Target, state, settings).icons.empty());
    settings.Widget(BarWidget::Gil).icons = false;
    w = BarWidgetContent(BarWidget::Gil, state, settings);
    CHECK(!w.coin && Joined(w.title) == "100");
}

TEST(the_job_widget_puts_its_exp_bar_under_beside_or_nowhere_and_can_show_levels_alone)
{
    Settings settings;
    BarState state;
    JobSample s;
    s.mainJob = 1, s.mainLevel = 30, s.subJob = 13, s.subLevel = 15;
    s.exp = 3800, s.expNeeded = 7600;
    state.job = JobReport{s, std::nullopt};
    settings.Widget(BarWidget::Job).expBar = ExpBarPlace::Beside;
    WidgetContent w = BarWidgetContent(BarWidget::Job, state, settings);
    CHECK(w.progress == 0.5f && w.progressBeside);
    settings.Widget(BarWidget::Job).expBar = ExpBarPlace::Off;
    CHECK(!BarWidgetContent(BarWidget::Job, state, settings).progress);
    settings.Widget(BarWidget::Job).jobNames = false;
    CHECK(Joined(BarWidgetContent(BarWidget::Job, state, settings).title) == "Lv 30 / 15");
    s.subJob  = 0, s.subLevel = 0; // no support job yet
    state.job = JobReport{s, std::nullopt};
    CHECK(Joined(BarWidgetContent(BarWidget::Job, state, settings).title) == "Lv 30");
}

TEST(the_job_widget_can_put_its_exp_first_or_show_it_alone)
{
    Settings settings;
    BarState state;
    JobSample s;
    s.mainJob = 1, s.mainLevel = 30, s.subJob = 13, s.subLevel = 15;
    s.exp = 4140, s.expNeeded = 7600;
    state.job          = JobReport{s, std::nullopt};
    settings.Widget(BarWidget::Job).jobLayout = JobLayout::ExpFirst;
    WidgetContent w    = BarWidgetContent(BarWidget::Job, state, settings);
    CHECK(w.title == ListOf(Text("3,460 EXP to next"))); // the larger line, in the title's own tone
    CHECK(w.subtitle == ListOf(Text("WAR 30 / NIN 15", Tone::Dim)));
    CHECK(w.figures && w.hasPopup && w.progress && std::fabs(*w.progress - 4140.0f / 7600.0f) < 1e-6f);
    settings.Widget(BarWidget::Job).jobNames = false;
    CHECK(Joined(BarWidgetContent(BarWidget::Job, state, settings).subtitle) == "Lv 30 / 15");
    settings.Widget(BarWidget::Job).jobLayout = JobLayout::ExpOnly;
    w                  = BarWidgetContent(BarWidget::Job, state, settings);
    CHECK(Joined(w.title) == "3,460 EXP to next" && w.subtitle.empty() && w.progress.has_value());
    s.mainLevel = 75, s.subLevel = 37, s.merits = 7, s.meritsMax = 10, s.limitPoints = 4210;
    state.job = JobReport{s, std::nullopt};
    w         = BarWidgetContent(BarWidget::Job, state, settings);
    CHECK(w.title == ListOf(Text("7 merits")) && w.subtitle.empty() && w.progress == 0.421f);
    state.job.reset(); // the title screen
    w = BarWidgetContent(BarWidget::Job, state, settings);
    CHECK(Joined(w.title) == "-" && w.subtitle.empty());
}

// 2026-10-09 06:30:42 UTC is Friday 15:30:42 in Japan and still Thursday 23:30:42 on the US Pacific coast, on daylight
// time.
TEST(the_clock_reads_in_the_zone_picked_for_it_with_that_zones_date_in_its_popup)
{
    Settings settings;
    BarState state;
    state.unixMs    = 1791527400000 + 42000;
    state.earthYear = 2026, state.earthMonth = 10, state.earthDay = 9; // the computer's own: 08:30:42 in Berlin
    state.earthHour = 8, state.earthMinute = 30, state.earthSecond = 42;
    CHECK(!settings.Widget(BarWidget::EarthTime).clockZone.has_value());
    CHECK(Joined(BarWidgetContent(BarWidget::EarthTime, state, settings).title) == "08:30");
    CHECK(Joined(BarPopupContent(BarWidget::EarthTime, state, settings).title) == "2026-10-09");
    settings.Widget(BarWidget::EarthTime).clockZone = ClockZone::UsPacific;
    CHECK(Joined(BarWidgetContent(BarWidget::EarthTime, state, settings).title) == "PDT 23:30");
    CHECK(Joined(BarPopupContent(BarWidget::EarthTime, state, settings).title) == "2026-10-08");
    settings.Widget(BarWidget::EarthTime).clockZone    = ClockZone::Japan;
    settings.Widget(BarWidget::EarthTime).clock24      = false;
    settings.Widget(BarWidget::EarthTime).clockSeconds = true;
    CHECK(Joined(BarWidgetContent(BarWidget::EarthTime, state, settings).title) == "JST 3:30:42 PM");
    CHECK(Joined(BarPopupContent(BarWidget::EarthTime, state, settings).title) == "2026-10-09");
}

TEST(a_clock_in_a_picked_zone_is_read_from_the_time_alone_and_the_computers_from_its_own)
{
    Settings settings;
    for (WidgetOptions& each : settings.widgets)
        each.on = false;
    settings.Widget(BarWidget::EarthTime).on = true;
    const BarNeeds own = NeedsOf(settings, std::nullopt);
    CHECK(own.localTime && !own.unixTime);
    settings.Widget(BarWidget::EarthTime).clockZone   = ClockZone::Utc;
    const BarNeeds zoned = NeedsOf(settings, std::nullopt);
    CHECK(zoned.unixTime && !zoned.localTime);
    settings.Widget(BarWidget::EarthTime).clockZone.reset();
    settings.Widget(BarWidget::EarthTime).clockServerTime = true; // the server's line goes by the time, the computer's by its own
    const BarNeeds stacked   = NeedsOf(settings, std::nullopt);
    CHECK(stacked.unixTime && stacked.localTime);
    settings.Widget(BarWidget::EarthTime).clockServerTime = false;
    const BarNeeds opened    = NeedsOf(settings, BarWidget::EarthTime); // the popup counts down to the resets
    CHECK(opened.unixTime && opened.localTime);
}

TEST(the_clocks_read_on_a_twelve_hour_clock_when_asked)
{
    CHECK(ClockText(21, 7, true) == "21:07" && ClockText(21, 7, false) == "9:07 PM");
    CHECK(ClockText(0, 5, false) == "12:05 AM" && ClockText(12, 0, false) == "12:00 PM");
    Settings settings;
    settings.Widget(BarWidget::EarthTime).clock24 = false;
    BarState state;
    state.earthHour = 9, state.earthMinute = 30;
    CHECK(Joined(BarWidgetContent(BarWidget::EarthTime, state, settings).title) == "9:30 AM");
}

TEST(the_clocks_add_seconds_when_asked)
{
    CHECK(ClockText(21, 7, true, 5) == "21:07:05" && ClockText(22, 13, false, 24) == "10:13:24 PM");
    Settings settings;
    BarState state;
    state.earthHour = 9, state.earthMinute = 30, state.earthSecond = 7;
    CHECK(Joined(BarWidgetContent(BarWidget::EarthTime, state, settings).title) == "09:30");
    settings.Widget(BarWidget::EarthTime).clockSeconds = true;
    CHECK(Joined(BarWidgetContent(BarWidget::EarthTime, state, settings).title) == "09:30:07");
}

TEST(the_connection_widget_adds_its_latency_when_asked)
{
    Settings settings;
    BarState state;
    state.connection = SteadyLink();
    settings.Widget(BarWidget::Connection).netMeter = false;
    CHECK(Joined(BarWidgetContent(BarWidget::Connection, state, settings).title) == "S 175  R 227");
    settings.Widget(BarWidget::Connection).latency = true;
    CHECK(Joined(BarWidgetContent(BarWidget::Connection, state, settings).title) == "S 175  R 227  48 ms");
    state.connection->latency.reset(); // not measured yet
    CHECK(Joined(BarWidgetContent(BarWidget::Connection, state, settings).title) == "S 175  R 227");
}

TEST(a_mobs_popup_lists_what_it_drops_at_its_treasure_hunter_0_chance_and_what_can_be_stolen)
{
    Settings settings;
    ActorInfo billy = Goblin();
    billy.serverId  = 17191196; // Bigmouth Billy: iron and mythril ore very common, gold ore to steal
    billy.mob       = MobById(billy.serverId);
    BarState state;
    state.target   = &billy;
    state.itemName = [](uint16_t item) -> const char* {
        switch (item)
        {
        case 643: return "Iron Ore";
        case 644: return "Mythril Ore";
        case 737: return "Gold Ore";
        default: return nullptr; // an item the game has no name for is left out
        }
    };
    const PopupContent p = BarPopupContent(BarWidget::Target, state, settings);
    const PopupSection* drops = Section(p, "Drops");
    CHECK(drops != nullptr);
    if (drops != nullptr)
    {
        const std::vector<std::string> lines = Lines(*drops);
        CHECK(std::ranges::find(lines, "Iron Ore 24%") != lines.end());
        CHECK(std::ranges::find(lines, "Mythril Ore 24%") != lines.end());
    }
    const PopupSection* steal = Section(p, "Steal");
    CHECK(steal != nullptr && Lines(*steal) == (std::vector<std::string>{"Gold Ore"}));
    state.itemName = nullptr; // no names: no lists
    CHECK(Section(BarPopupContent(BarWidget::Target, state, settings), "Drops") == nullptr);
}

namespace
{
    ActorInfo NpcTarget(uint32_t serverId, const char* name)
    {
        ActorInfo npc;
        npc.kind     = EntityKind::Npc;
        npc.serverId = serverId;
        npc.index    = serverId & 0xFFF;
        std::snprintf(npc.name, sizeof(npc.name), "%s", name);
        return npc;
    }

    const char* NpcItemName(uint16_t item)
    {
        switch (item)
        {
        case 4373: return "Woozyshroom";
        case 4374: return "Sleepshroom";
        case 4375: return "Danceshroom";
        case 1823: return "Shrimp Lantern";
        case 4389: return "San d'Orian Carrot";
        case 4112: return "Potion";
        case 4128: return "Ether";
        case 4148: return "Antidote";
        case 4150: return "Eye Drops";
        case 4151: return "Echo Drops";
        case 17389: return "Bamboo Fishing Rod";
        case 17390: return "Yew Fishing Rod";
        case 17391: return "Willow Fishing Rod";
        case 17395: return "Lugworm";
        case 17396: return "Little Worm";
        case 5066: return "Scroll of Lightning Threnody";
        case 5068: return "Scroll of Light Threnody";
        default: return nullptr; // left out, as a drop the game has no name for is
        }
    }

    // A section's lines; none when the popup has no such section.
    std::vector<std::string> SectionLines(const PopupContent& popup, const std::string& heading)
    {
        const PopupSection* section = Section(popup, heading);
        return section != nullptr ? Lines(*section) : std::vector<std::string>{"(no " + heading + ")"};
    }

    std::vector<std::string> Headings(const PopupContent& popup)
    {
        std::vector<std::string> headings;
        for (const PopupSection& s : popup.sections)
            headings.push_back(s.heading);
        return headings;
    }
}

TEST(a_qms_popup_says_what_to_trade_it_and_what_that_pops)
{
    // Carpenters' Landing's qm_mycophile (its script): a Woozyshroom, Sleepshroom or Danceshroom pops Mycophile.
    Settings settings;
    const ActorInfo qm = NpcTarget(16785729, "???");
    BarState state;
    state.target   = &qm;
    state.itemName = NpcItemName;
    CHECK(BarWidgetContent(BarWidget::Target, state, settings).hasPopup);
    const PopupContent p = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(Joined(p.title) == "???");
    CHECK(Headings(p) == (std::vector<std::string>{"", "Click", "Trade", "Pops"}));
    CHECK(SectionLines(p, "") == (std::vector<std::string>{"ID 141"}));
    // Its hint in the game's words, and the three mushrooms as one trade, as the probe saw on the server.
    CHECK(SectionLines(p, "Click") ==
          (std::vector<std::string>{"There is a rotten mushroom here. There are 3", "openings in its cap."}));
    CHECK(SectionLines(p, "Trade") ==
          (std::vector<std::string>{"Woozyshroom + Sleepshroom + Danceshroom", "\xE2\x86\x92 Pops Mycophile"}));
    CHECK(SectionLines(p, "Pops") == (std::vector<std::string>{"Mycophile"}));
}

TEST(a_qm_that_pops_two_of_one_nm_names_it_once)
{
    // Al'Taieu's qm_jailer_of_prudence spawns both Jailers of Prudence, 16912846 and 16912847.
    Settings settings;
    const ActorInfo qm = NpcTarget(16912886, "???");
    BarState state;
    state.target = &qm;
    CHECK(SectionLines(BarPopupContent(BarWidget::Target, state, settings), "Pops") ==
          (std::vector<std::string>{"Jailer of Prudence"}));
}

TEST(a_spot_whose_script_works_out_what_it_pops_says_so)
{
    // Uleguerand Range's Rabbit Footprint takes a San d'Orian Carrot and pops a coney the moon picks.
    Settings settings;
    const ActorInfo footprint = NpcTarget(16798100, "Rabbit Footprint");
    BarState state;
    state.target   = &footprint;
    state.itemName = NpcItemName;
    const PopupContent p = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(SectionLines(p, "Trade") == (std::vector<std::string>{"San d'Orian Carrot"}));
    CHECK(SectionLines(p, "Pops") == (std::vector<std::string>{"Worked out by its script"}));
    if (const PopupSection* pops = Section(p, "Pops")) CHECK(pops->rows[0].cells[0].text[0].tone == Tone::Dim);
}

TEST(a_qm_shows_the_nm_the_probe_saw_it_pop_and_what_it_says)
{
    // Bibiki Bay's qm_shen, as the probe saw it on the server: its click's shrimp shells line, and a Shrimp Lantern's
    // skimming line and Shen, the words from its IDs.lua wrapped at 44 characters, the lantern in its <item>.
    Settings settings;
    const ActorInfo qm = NpcTarget(16793970, "???");
    BarState state;
    state.target   = &qm;
    state.itemName = NpcItemName;
    const PopupContent p = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(SectionLines(p, "Click") ==
          (std::vector<std::string>{"Innumerable shrimp shells are floating in", "the water here."}));
    CHECK(SectionLines(p, "Trade") ==
          (std::vector<std::string>{"Shrimp Lantern", "The Shrimp Lantern begins skimming quietly",
              "across the surface of the water. ...What's", "this!? An enormous creature is chasing after",
              "it from below!!!", "\xE2\x86\x92 Pops Shen"}));
    CHECK(SectionLines(p, "Pops") == (std::vector<std::string>{"Shen"}));
}

TEST(a_hidden_qm_says_when_it_appears_and_what_a_readied_one_does)
{
    // Carpenters' Landing's Hercules Beetle tree: its zone shows it at 22:00 and hides it at 07:00, and once honeyed
    // and ready it pops the Hercules Beetle (globals.lua and qm_hercules_beetle.lua).
    Settings settings;
    const ActorInfo tree = NpcTarget(16785730, "???");
    BarState state;
    state.target         = &tree;
    const PopupContent p = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(SectionLines(p, "") == (std::vector<std::string>{"ID 142", "Appears at 22:00, gone at 07:00"}));
    CHECK(SectionLines(p, "Click") == (std::vector<std::string>{"Once readied", "\xE2\x86\x92 Pops Hercules Beetle"}));
    if (const PopupSection* click = Section(p, "Click")) CHECK(click->rows[0].cells[0].text[0].tone == Tone::Dim);
}

TEST(a_quest_steps_acts_name_the_quest_its_step_and_what_it_needs)
{
    // Batallia Downs' qm_chocobotale: a click finds nothing; at step 4 of A Chocobo's Tale it calls Badshah's pack, and
    // once the NM is down it gives the Silver Comet's Collar (quests/jeuno/A_Chocobos_Tale.lua).
    Settings settings;
    const ActorInfo qm = NpcTarget(17207852, "???");
    BarState state;
    state.target = &qm;
    CHECK(SectionLines(BarPopupContent(BarWidget::Target, state, settings), "Click") ==
          (std::vector<std::string>{"You find nothing.", "In A Chocobos Tale, step 4", "You are being attacked!",
              "\xE2\x86\x92 Pops Badshah", "In A Chocobos Tale, step 4, after its NM falls",
              "\xE2\x86\x92 Gives key item Silver Comets Collar"}));
}

TEST(an_npcs_popup_names_the_key_items_it_gives_and_checks_for)
{
    // Carpenters' Landing's qm_para: with the Antique Amulet it pops Para, and it gives the Cathedral Medallion, for
    // the quest Elderly Pursuits (quests/otherAreas).
    Settings settings;
    const ActorInfo qm = NpcTarget(16785781, "???");
    BarState state;
    state.target         = &qm;
    const PopupContent p = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(Headings(p) == (std::vector<std::string>{"", "Click", "Pops", "Key items", "Checks for", "Quests"}));
    CHECK(SectionLines(p, "Click") ==
          (std::vector<std::string>{"There is nothing out of the ordinary here.", "In Elderly Pursuits, step 1",
              "You are overwhelmed by the putrid stench of", "decay!", "\xE2\x86\x92 Pops Para",
              "In Elderly Pursuits, step 2, holding its key items", "You polish the Antique Amulet with the",
              "glowing mushroom spores!", "Obtained key item: Cathedral Medallion.",
              "\xE2\x86\x92 Gives key item Cathedral Medallion"}));
    CHECK(SectionLines(p, "Quests") == (std::vector<std::string>{"Elderly Pursuits Other Areas"}));
    CHECK(SectionLines(p, "Pops") == (std::vector<std::string>{"Para"}));
    CHECK(SectionLines(p, "Key items") == (std::vector<std::string>{"Cathedral Medallion"}));
    CHECK(SectionLines(p, "Checks for") == (std::vector<std::string>{"Antique Amulet"}));
}

TEST(a_trade_of_gil_reads_gil_and_an_item_without_a_name_is_left_out)
{
    // Whitegate's Cacaroon takes an Imperial Bronze Piece (2184, which NpcItemName cannot name) and gil.
    Settings settings;
    const ActorInfo cacaroon = NpcTarget(16982252, "Cacaroon");
    BarState state;
    state.target   = &cacaroon;
    state.itemName = NpcItemName;
    CHECK(SectionLines(BarPopupContent(BarWidget::Target, state, settings), "Trade") == (std::vector<std::string>{"Gil"}));
}

TEST(an_npc_the_data_has_nothing_of_gets_only_its_name)
{
    Settings settings;
    const ActorInfo blank = NpcTarget(16781329, "Door");
    BarState state;
    state.target = &blank;
    const WidgetContent w = BarWidgetContent(BarWidget::Target, state, settings);
    CHECK(Joined(w.title) == "Door" && !w.hasPopup);
    CHECK(BarPopupContent(BarWidget::Target, state, settings).sections.empty());
}

TEST(a_shops_popup_lists_its_stock_with_prices)
{
    // Maera's stock, aboard the ship to Selbina (her script), at prices fame does not move.
    Settings settings;
    const ActorInfo maera = NpcTarget(17678362, "Maera");
    BarState state;
    state.target   = &maera;
    state.itemName = NpcItemName;
    CHECK(BarWidgetContent(BarWidget::Target, state, settings).hasPopup);
    const PopupContent p = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(Headings(p) == (std::vector<std::string>{"", "Shop"}));
    CHECK(SectionLines(p, "Shop") == (std::vector<std::string>{"Potion 910 gil", "Ether 4,832 gil", "Antidote 316 gil",
                                         "Eye Drops 2,595 gil", "Echo Drops 800 gil"}));
}

TEST(a_nation_shop_puts_what_conquest_holds_back_under_its_place_and_says_fame_moves_its_prices)
{
    // Southern San d'Oria's Lusiane, by xi.shop.nation: rank 3 always, 2 while San d'Oria is 1st or 2nd, 1 while it is
    // 1st and only to its citizens.
    Settings settings;
    const ActorInfo lusiane = NpcTarget(17719350, "Lusiane");
    BarState state;
    state.target   = &lusiane;
    state.itemName = NpcItemName;
    CHECK(SectionLines(BarPopupContent(BarWidget::Target, state, settings), "Shop") ==
          (std::vector<std::string>{"Little Worm 4 gil", "Willow Fishing Rod 72 gil", "Scroll of Light Threnody 120 gil",
              "Scroll of Lightning Threnody 1,376 gil", "When its nation is 1st or 2nd in conquest", "Lugworm 12 gil",
              "Yew Fishing Rod 236 gil", "When its nation is 1st in conquest, to its citizens",
              "Bamboo Fishing Rod 540 gil", "Prices before fame, which moves them up to 10% either way"}));
    // The items in the normal tone, so the dim lines naming a place read as headings over them.
    const PopupContent p = BarPopupContent(BarWidget::Target, state, settings);
    if (const PopupSection* shop = Section(p, "Shop"))
        CHECK(shop->rows[0].cells[0].text[0].tone == Tone::Normal && shop->rows[4].cells[0].text[0].tone == Tone::Dim);
}

TEST(a_shop_whose_stock_its_script_works_out_says_so)
{
    Settings settings;
    const ActorInfo dabih = NpcTarget(17772597, "Dabih Jajalioh"); // Ru'Lude Gardens: the stock follows the season
    BarState state;
    state.target   = &dabih;
    state.itemName = NpcItemName;
    CHECK(SectionLines(BarPopupContent(BarWidget::Target, state, settings), "Shop") ==
          (std::vector<std::string>{"Worked out by its script"}));
}

TEST(an_npcs_popup_lists_its_quests_and_missions_with_their_logs)
{
    // The Gate of the Gods (quests/jeuno/Apocalypse_Nigh, missions/cop/8_1 and 8_2).
    Settings settings;
    const ActorInfo gate = NpcTarget(16916828, "Gate of the Gods");
    BarState state;
    state.target = &gate;
    CHECK(BarWidgetContent(BarWidget::Target, state, settings).hasPopup);
    const PopupContent p = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(Headings(p) == (std::vector<std::string>{"", "Quests", "Missions"}));
    CHECK(SectionLines(p, "Quests") == (std::vector<std::string>{"Apocalypse Nigh Jeuno"}));
    CHECK(SectionLines(p, "Missions") == (std::vector<std::string>{"Garden of Antiquity CoP", "A Fate Decided CoP"}));
    if (const PopupSection* quests = Section(p, "Quests"))
        CHECK(quests->rows[0].kind == RowKind::Pair && quests->rows[0].cells[0].text[0].tone == Tone::Normal &&
              quests->rows[0].cells[1].text[0].tone == Tone::Dim);
}

TEST(battlefields_and_hidden_quests_have_sections_and_what_live_has_off_is_left_out)
{
    Settings settings;
    BarState state;
    const ActorInfo circle = NpcTarget(16818249, "Memento Circle"); // battlefields/The_Shrouded_Maw
    state.target           = &circle;
    CHECK(SectionLines(BarPopupContent(BarWidget::Target, state, settings), "Battlefields") ==
          (std::vector<std::string>{"Darkness Named", "Test Your Mite", "Waking Dreams"}));
    const ActorInfo luminant = NpcTarget(16806290, "Luminant"); // quests/hiddenQuests/Mimeo_Stone
    state.target             = &luminant;
    CHECK(SectionLines(BarPopupContent(BarWidget::Target, state, settings), "Quests") ==
          (std::vector<std::string>{"Mimeo Stone Hidden"}));
    // Misareaux Coast's Dilapidated Gate: three CoP missions, the three mobs of one's fight, and a trust quest live
    // has off.
    const ActorInfo gate = NpcTarget(16879936, "Dilapidated Gate");
    state.target         = &gate;
    const PopupContent p = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(Headings(p) == (std::vector<std::string>{"", "Pops", "Missions"}));
    CHECK(SectionLines(p, "Missions").size() == 3);
    const ActorInfo retriever = NpcTarget(17105705, "Mystic Retriever"); // only in an RoV mission
    state.target              = &retriever;
    CHECK(!BarWidgetContent(BarWidget::Target, state, settings).hasPopup);
}

TEST(what_an_npc_is_for_shows_as_tags_under_its_id)
{
    // Oldton Movalpolos' Twinkbrix takes a trade and teleports to the Mineshaft (its script).
    Settings settings;
    const ActorInfo twinkbrix = NpcTarget(16822535, "Twinkbrix");
    BarState state;
    state.target         = &twinkbrix;
    const PopupContent p = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(!p.sections.empty() && p.sections[0].heading.empty());
    if (p.sections.empty()) return;
    const std::vector<PopupRow>& rows = p.sections[0].rows;
    CHECK(rows.size() == 2 && rows.back().kind == RowKind::Tags);
    if (rows.size() != 2) return;
    CHECK(Joined(rows[1].cells[0].text) == "Teleports to Mineshaft" && rows[1].cells[0].text[0].tone == Tone::Tag);
}

TEST(each_copy_of_a_widget_shows_by_its_own_settings)
{
    Settings settings;
    BarState state;
    state.gil = 1234;
    state.currencies[3] = 50;
    const std::optional<PanelWidget> copy = AddWidgetCopy(settings, BarWidget::Gil);
    CHECK(copy.has_value());
    settings.Widget(*copy).currency = 3;
    const WidgetContent first = BarWidgetContent(BarWidget::Gil, state, settings), second = BarWidgetContent(*copy, state, settings);
    CHECK(first.coin && !second.coin && second.title.front().text == "50");
    // A copy not on the panel shows nothing, whatever it was set to.
    RemoveWidgetCopy(settings, *copy);
    settings.Widget(*copy).on = true;
    CHECK(!BarWidgetContent(*copy, state, settings).shown);
}

TEST(what_the_plugin_gathers_follows_every_copy)
{
    Settings settings;
    for (WidgetOptions& each : settings.widgets)
        each.on = false;
    const std::optional<PanelWidget> clock = AddWidgetCopy(settings, BarWidget::EarthTime);
    CHECK(clock.has_value());
    settings.Widget(*clock).clockZone = ClockZone::Utc;
    const BarNeeds needs = NeedsOf(settings, std::nullopt);
    CHECK(needs.unixTime && !needs.localTime); // only the copy shows, in a zone of its own
    settings.Widget(BarWidget::EarthTime).on = true;
    CHECK(NeedsOf(settings, std::nullopt).localTime);
    CHECK(NeedsOf(settings, PanelWidget{BarWidget::Bags, 2}).containers == 0); // Bags is off, any copy of it
}

TEST(the_bags_popup_lists_mog_safe_2_only_once_it_holds_something)
{
    // The server gives Mog Safe 2 its room before the second floor is open, so its room says nothing of whether
    // it can be used; what is in it does.
    Settings settings;
    BarState state;
    state.bags[0] = BagCount{26, 30};
    state.bags[kMogSafe2] = BagCount{0, 50};
    const auto listed = [&](const char* name) {
        for (const PopupSection& section : BarPopupContent(BarWidget::Bags, state, settings).sections)
            for (const PopupRow& row : section.rows)
                for (const PopupCell& cell : row.cells)
                    for (const Span& span : cell.text)
                        if (span.text == name) return true;
        return false;
    };
    CHECK(listed("Inventory") && !listed("Mog Safe 2"));
    state.bags[kMogSafe2] = BagCount{3, 50};
    CHECK(listed("Mog Safe 2"));
}

TEST(the_exp_widget_is_the_exp_alone_with_its_bar_by_its_own_settings)
{
    Settings settings;
    BarState state;
    JobSample s;
    s.mainJob = 1, s.mainLevel = 30, s.exp = 12345, s.expNeeded = 20000;
    state.job = JobReport{s, std::nullopt};
    CHECK(!BarWidgetContent(BarWidget::Exp, state, settings).shown); // off on a new install
    settings.Widget(BarWidget::Exp).on      = true;
    settings.Widget(BarWidget::Exp).expText = ExpTextBit(ExpText::Percent);
    settings.Widget(BarWidget::Exp).expBar  = ExpBarPlace::Beside;
    const WidgetContent w = BarWidgetContent(BarWidget::Exp, state, settings);
    CHECK(w.shown && w.title.front().text == "61%" && w.subtitle.empty());
    CHECK(w.progress.has_value() && w.progressBeside);
    CHECK(NeedsOf(settings, std::nullopt).job);
    // The Job widget keeps its own.
    CHECK_EQ(BarWidgetContent(BarWidget::Job, state, settings).subtitle.front().text, "7,655 EXP to next");
}

TEST(the_bags_widget_shows_the_bag_picked_for_it_and_the_plugin_reads_that_far)
{
    Settings settings;
    BarState state;
    state.bags[0] = BagCount{26, 30};
    state.bags[5] = BagCount{12, 80}; // the satchel
    CHECK_EQ(BarWidgetContent(BarWidget::Bags, state, settings).title.front().text, "26 / 30");
    settings.Widget(BarWidget::Bags).bag = 5;
    const WidgetContent satchel = BarWidgetContent(BarWidget::Bags, state, settings);
    CHECK(satchel.title.front().text == "12 / 80" && satchel.subtitle.front().text == ContainerName(5));
    CHECK_EQ(NeedsOf(settings, std::nullopt).containers, 6);
    // A bag the character has no room in reads as nothing yet.
    settings.Widget(BarWidget::Bags).bag = 6;
    CHECK_EQ(BarWidgetContent(BarWidget::Bags, state, settings).title.front().text, "-");
}

TEST(a_targeted_players_jobs_show_from_whatever_knows_them_unless_switched_off)
{
    Settings settings;
    BarState state;
    ActorInfo player;
    player.kind = EntityKind::Player;
    std::snprintf(player.name, sizeof(player.name), "%s", "Tanyrus");
    state.target = &player;
    CHECK(BarWidgetContent(BarWidget::Target, state, settings).subtitle.empty()); // nothing known of them yet
    player.jobs = PlayerJobs{1, 75, 13, 37}; // from a lookup, the party list or a check
    CHECK_EQ(BarWidgetContent(BarWidget::Target, state, settings).subtitle.front().text, "WAR 75 / NIN 37");
    settings.Widget(BarWidget::Target).targetJobs = false;
    CHECK(BarWidgetContent(BarWidget::Target, state, settings).subtitle.empty());
    // An anonymous player's are the game's question marks.
    settings.Widget(BarWidget::Target).targetJobs = true;
    player.jobs.reset();
    player.status = PlayerStatus{};
    player.status->anonymous = true;
    CHECK_EQ(BarWidgetContent(BarWidget::Target, state, settings).subtitle.front().text, "? ? ?");
}

TEST(the_conquest_widget_counts_the_regions_of_the_nations_picked_and_its_popup_names_them)
{
    Settings settings;
    BarState state;
    settings.Widget(BarWidget::Conquest).on = true;
    CHECK_EQ(Joined(BarWidgetContent(BarWidget::Conquest, state, settings).title), "-"); // before the standings come
    RegionOwners owners{};
    owners.fill(Nation::Beastmen);
    owners[0] = Nation::SanDoria;                         // Ronfaure
    owners[3] = owners[4] = Nation::Bastok;               // Gustaberg and Derfland
    owners[5] = Nation::Windurst;
    owners[18] = Nation::None;
    state.owners = owners;
    const WidgetContent all = BarWidgetContent(BarWidget::Conquest, state, settings);
    CHECK_EQ(Joined(all.title), "S 1  B 2  W 1  BM 14");
    CHECK(all.title[2].argb == NationColor(Nation::Bastok) && all.hasPopup);
    // Bastok alone: its name and count, and its regions in the popup.
    settings.Widget(BarWidget::Conquest).nations = NationBit(Nation::Bastok);
    CHECK_EQ(Joined(BarWidgetContent(BarWidget::Conquest, state, settings).title), "Bastok 2");
    const PopupContent popup = BarPopupContent(BarWidget::Conquest, state, settings);
    CHECK(popup.sections.size() == 1 && popup.sections[0].heading == "Bastok");
    CHECK(Lines(popup.sections[0]) == (std::vector<std::string>{"Gustaberg", "Derfland"}));
}
