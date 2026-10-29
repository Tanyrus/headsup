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
        CHECK(choice.options.size() >= 2u && choice.defaultOption < choice.options.size());
    });
}

TEST(a_new_install_holds_each_choices_default_option)
{
    const Settings fresh;
    ForEachChoice([&](const SetupChoice& choice) { CHECK_EQ(PickedOption(choice, fresh).value_or(99), choice.defaultOption); });
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
                if (&other != &choice) CHECK_EQ(PickedOption(other, s).value_or(99), other.defaultOption);
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

TEST(running_setup_again_puts_back_what_its_steps_set_and_keeps_the_rest)
{
    Settings s;
    ForEachChoice([&](const SetupChoice& choice) { choice.options[choice.defaultOption == 0 ? 1 : 0].apply(s); });
    s.defaultFont  = "Arial";
    s.baseSize     = 22;
    s.baseIconSize = 30;
    s.barTextFont  = "Arial";
    s.barTextSize  = 18;
    s.menuFontSize = 24;
    s.windowType   = 3;
    s.cursorColor  = {ColorSource::Custom, ColorFromRgb(0x123456)};
    s.autoCheck = s.lookUpPlayers = s.stackNames = s.hideInEvent = true;
    SetWidgetShown(s, BarWidget::Zone, false);
    SetWidgetShown(s, BarWidget::Party, true);
    // Not Setup's: kept.
    s.menuX         = 300;
    s.screenshotKey = 0x7B;
    s.Kind(PlateKind::Players).lines[kTopLine].text = "$job";

    UseSetupDefaults(s);
    ForEachChoice([&](const SetupChoice& choice) { CHECK_EQ(PickedOption(choice, s).value_or(99), choice.defaultOption); });
    const Settings fresh;
    CHECK(s.defaultFont == fresh.defaultFont && s.baseSize == fresh.baseSize && s.baseIconSize == fresh.baseIconSize);
    CHECK(s.barTextFont == fresh.barTextFont && s.barTextSize == fresh.barTextSize && s.menuFontSize == fresh.menuFontSize);
    CHECK(s.windowType == fresh.windowType && s.cursorColor == fresh.cursorColor);
    CHECK(!s.autoCheck && !s.lookUpPlayers && !s.stackNames && !s.hideInEvent);
    for (int w = 0; w < kBarWidgetCount; ++w)
        CHECK_EQ(Shown(s, static_cast<BarWidget>(w)), Shown(fresh, static_cast<BarWidget>(w)));
    CHECK_EQ(s.menuX, 300);
    CHECK_EQ(s.screenshotKey, 0x7B);
    CHECK_EQ(s.Kind(PlateKind::Players).lines[kTopLine].text, std::string("$job"));
}

TEST(setup_defaults_change_nothing_of_a_new_install)
{
    Settings s;
    UseSetupDefaults(s);
    CHECK(s == Settings{});
}

TEST(each_choice_has_a_name_in_its_list_for_each_option)
{
    ForEachChoice([&](const SetupChoice& choice) { CHECK_EQ(kWindowLayout.OptionsOf(choice.list).size(), choice.options.size()); });
    CHECK_EQ(kWindowLayout.OptionsOf(kSetupWidgetGroupNames).size(), SetupWidgetGroups().size());
}

TEST(setup_never_counts_itself_done_by_its_defaults)
{
    Settings s;
    s.setupDone = true;
    UseSetupDefaults(s);
    CHECK(s.setupDone);
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
