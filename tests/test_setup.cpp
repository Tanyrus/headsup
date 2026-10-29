#include "cursor_picture.h"
#include "setup.h"
#include "test.h"
#include "window_layout.h"

#include <set>
#include <string>

using namespace headsup;

namespace
{
    template <typename Each>
    void ForEachChoice(Each each)
    {
        for (const SetupPage& page : SetupPages())
            for (const SetupChoice& choice : page.choices)
                each(choice);
    }

    const RowSettings& MobRow(const Settings& s, Category row) { return s.Kind(PlateKind::Mobs).rows[CategoryIndex(row)]; }
    const SetupChoice& ChoiceNamed(const char* key)
    {
        for (const SetupPage& page : SetupPages())
            for (const SetupChoice& choice : page.choices)
                if (std::string(choice.key) == key) return choice;
        throw test::Failure{std::string("no choice ") + key};
    }
    const SetupOption& OptionNamed(const SetupChoice& choice, const char* key)
    {
        for (const SetupOption& option : choice.options)
            if (std::string(option.key) == key) return option;
        throw test::Failure{std::string("no option ") + key};
    }
    Settings With(const char* choice, const char* option, Settings s = {})
    {
        OptionNamed(ChoiceNamed(choice), option).apply(s);
        return s;
    }
}

TEST(setup_has_a_page_for_each_step_in_order)
{
    CHECK_EQ(SetupPages().size(), static_cast<size_t>(kSetupSteps));
    for (int i = 0; i < kSetupSteps; ++i)
    {
        CHECK(SetupPages()[static_cast<size_t>(i)].step == static_cast<SetupStep>(i));
        CHECK(&SetupPageOf(static_cast<SetupStep>(i)) == &SetupPages()[static_cast<size_t>(i)]);
    }
}

TEST(each_choice_and_each_of_its_options_has_a_key_of_its_own)
{
    std::set<std::string> choices;
    ForEachChoice([&](const SetupChoice& choice) {
        CHECK(choices.insert(choice.key).second);
        std::set<std::string> options;
        for (const SetupOption& option : choice.options)
            CHECK(options.insert(option.key).second);
        CHECK(choice.options.size() >= 2u);
    });
}

TEST(a_new_install_holds_one_of_each_choices_options)
{
    const Settings fresh;
    ForEachChoice([&](const SetupChoice& choice) { CHECK(PickedOption(choice, fresh).has_value()); });
}

TEST(an_option_picked_is_the_one_its_choice_holds_whatever_was_picked_before)
{
    ForEachChoice([&](const SetupChoice& choice) {
        for (size_t before = 0; before < choice.options.size(); ++before)
            for (size_t i = 0; i < choice.options.size(); ++i)
            {
                Settings s;
                choice.options[before].apply(s);
                choice.options[i].apply(s);
                CHECK_EQ(PickedOption(choice, s).value_or(99), i);
            }
    });
}

TEST(an_option_leaves_every_other_choice_as_it_was)
{
    ForEachChoice([&](const SetupChoice& choice) {
        for (const SetupOption& option : choice.options)
        {
            Settings s;
            option.apply(s);
            ForEachChoice([&](const SetupChoice& other) {
                if (&other != &choice) CHECK(PickedOption(other, s) == PickedOption(other, Settings{}));
            });
        }
    });
}

TEST(a_choice_set_some_other_way_holds_no_option)
{
    Settings s;
    s.barTheme = BarTheme::Custom;
    CHECK(!PickedOption(ChoiceNamed("theme"), s));
    s.outlineThickness = 135;
    CHECK(!PickedOption(ChoiceNamed("outline-thickness"), s));
}

TEST(mob_nameplates_for_mobs_that_attack_leave_the_rest_to_show_when_targeted)
{
    const Settings s = With("mob-plates", "attackers");
    for (Category shown : {Category::WillAttack, Category::NmWillAttack, Category::NmWontAttack, Category::Placeholder,
             Category::PartyClaim, Category::OtherClaim})
        CHECK(MobRow(s, shown).plate == PlateMode::Shown);
    for (Category targeted : {Category::WontAttack, Category::Unknown, Category::Dead})
        CHECK(MobRow(s, targeted).plate == PlateMode::WhenTargeted);
    CHECK(MobRow(With("mob-plates", "game"), Category::NmWillAttack).plate == PlateMode::Vanilla);
}

TEST(mob_designs_drop_lines_from_the_top_and_keep_the_name)
{
    const Settings full = With("mob-design", "full"), simple = With("mob-design", "simple"), name = With("mob-design", "name");
    const auto& f = full.Kind(PlateKind::Mobs).lines;
    CHECK(f[kTopLine].text == kTagDetect && f[kMiddleLine].text == kTagLevelCon && f[kBottomLine].text == kTagName);
    const auto& s = simple.Kind(PlateKind::Mobs);
    CHECK(s.lines[kTopLine].text.empty() && s.lines[kMiddleLine] == f[kMiddleLine] && s.lines[kBottomLine] == f[kBottomLine]);
    CHECK(s.ornament.place == OrnamentPlace::Disabled);
    const auto& n = name.Kind(PlateKind::Mobs).lines;
    CHECK(n[kTopLine].text.empty() && n[kMiddleLine].text.empty() && n[kBottomLine] == f[kBottomLine]);
}

TEST(outlines_for_every_mob_leave_the_dead_and_let_claims_follow_the_row_under_them)
{
    const Settings every = With("outlines", "all");
    CHECK(every.outlines);
    for (Category row : {Category::WillAttack, Category::WontAttack, Category::Unknown, Category::NmWillAttack, Category::NmWontAttack,
             Category::Placeholder})
        CHECK(MobRow(every, row).outline == OutlineMode::Shown);
    CHECK(MobRow(every, Category::PartyClaim).outline == OutlineMode::ByNature);
    CHECK(MobRow(every, Category::Dead).outline == OutlineMode::Disabled);
    CHECK(MobRow(With("outlines", "target"), Category::WontAttack).outline == OutlineMode::WhenTargeted);
    CHECK(!With("outlines", "none").outlines);
}

TEST(a_cursor_picked_takes_its_own_width_at_the_height_set)
{
    Settings s;
    s.cursorHeight = 32;
    const Settings arrow = With("cursor", "arrow", s);
    CHECK(arrow.cursorStyle == CursorStyle::Triangle);
    CHECK_EQ(arrow.cursorWidth, NaturalCursorWidth(CursorStyle::Triangle, 32));
    CHECK(With("cursor", "game", s).cursorStyle == CursorStyle::Vanilla);
}

TEST(the_widget_groups_hold_every_widget_once)
{
    std::set<BarWidget> seen;
    for (const SetupWidgetGroup& group : SetupWidgetGroups())
        for (BarWidget widget : group.widgets)
            CHECK(seen.insert(widget).second);
    CHECK_EQ(seen.size(), static_cast<size_t>(kBarWidgetCount));
}

TEST(a_widget_turned_off_takes_its_copies_with_it_and_turned_on_shows_its_first)
{
    Settings s;
    const auto copy = AddWidgetCopy(s, BarWidget::Gil);
    CHECK(copy.has_value() && Shows(s, *copy));
    SetWidgetShown(s, BarWidget::Gil, false);
    CHECK(!Shown(s, BarWidget::Gil));
    SetWidgetShown(s, BarWidget::Party, true);
    CHECK(Shows(s, PanelWidget{BarWidget::Party}));
}

TEST(each_choice_has_a_name_in_its_list_for_each_option)
{
    ForEachChoice([&](const SetupChoice& choice) { CHECK_EQ(kWindowLayout.OptionsOf(choice.list).size(), choice.options.size()); });
    CHECK_EQ(kWindowLayout.OptionsOf(kSetupWidgetGroupNames).size(), SetupWidgetGroups().size());
}

TEST(a_setup_backup_is_named_for_the_second_it_was_made_and_never_takes_anothers_name)
{
    const SetupTime at{2026, 10, 29, 15, 30, 7};
    const auto none = [](const std::string&) { return false; };
    CHECK_EQ(SetupBackupName(at, none), std::string("settings-before-setup-2026-10-29-153007.ini"));
    CHECK_EQ(SetupBackupName({2027, 1, 2, 3, 4, 5}, none), std::string("settings-before-setup-2027-01-02-030405.ini"));
    // Two runs in one second: the second takes a number of its own, and the first file is kept.
    const auto taken = [](const std::string& name) {
        return name == "settings-before-setup-2026-10-29-153007.ini" || name == "settings-before-setup-2026-10-29-153007-2.ini";
    };
    CHECK_EQ(SetupBackupName(at, taken), std::string("settings-before-setup-2026-10-29-153007-3.ini"));
}

namespace
{
    const SetupChoice& PlayerIconsChoice()
    {
        for (const SetupChoice& choice : SetupPageOf(SetupStep::PlateLines).choices)
            if (std::string(choice.key) == "player-icons") return choice;
        throw test::Failure{"no player-icons choice"};
    }
    // The player icons' tags, as a new install's player name line has them before the name.
    std::string Icons()
    {
        const std::string line = Settings{}.Kind(PlateKind::Players).lines[kBottomLine].text;
        return line.substr(0, line.size() - std::string(kTagName).size());
    }
}

TEST(player_icons_go_left_or_right_of_the_name_or_on_the_line_above_it_for_players_and_you)
{
    const SetupChoice& choice = PlayerIconsChoice();
    Settings s;
    CHECK_EQ(PickedOption(choice, s).value_or(99), 0u); // left of the name, as a new install has them
    choice.options[1].apply(s);
    for (PlateKind kind : {PlateKind::Players, PlateKind::Self})
    {
        CHECK_EQ(s.Kind(kind).lines[kBottomLine].text, std::string(kTagName) + Icons());
        CHECK(s.Kind(kind).lines[kMiddleLine].text.empty());
    }
    choice.options[2].apply(s);
    for (PlateKind kind : {PlateKind::Players, PlateKind::Self})
    {
        CHECK_EQ(s.Kind(kind).lines[kBottomLine].text, std::string(kTagName));
        CHECK_EQ(s.Kind(kind).lines[kMiddleLine].text, Icons());
    }
    // Back beside the name, the line above is left empty again.
    choice.options[0].apply(s);
    CHECK(s.Kind(PlateKind::Players) == Settings{}.Kind(PlateKind::Players));
    CHECK(s.Kind(PlateKind::Self) == Settings{}.Kind(PlateKind::Self));
}

TEST(player_icons_beside_the_name_keep_a_line_above_of_the_players_own)
{
    const SetupChoice& choice = PlayerIconsChoice();
    Settings s;
    s.Kind(PlateKind::Players).lines[kMiddleLine].text = "$job";
    choice.options[1].apply(s);
    CHECK_EQ(s.Kind(PlateKind::Players).lines[kMiddleLine].text, std::string("$job"));
    CHECK_EQ(PickedOption(choice, s).value_or(99), 1u);
}
