#include "menu_form.h"
#include "test.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace headsup;

namespace
{
    bool Has(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }

    struct Window
    {
        Settings settings;
        MenuReadings readings;
        MenuFormState state;
        std::vector<std::string> fonts = {"Arial", "Barlow Condensed"}, menuFonts = {"Segoe UI", "Barlow Condensed"};

        Form Pass(std::vector<FormEvent> events = {})
        {
            Form form(std::move(events), state.form);
            BuildMenu(form, settings, readings, state, fonts, menuFonts);
            return form;
        }
    };

    const FormBlock* Block(const Form& form, const char* id)
    {
        const auto at = std::ranges::find(form.blocks, id, &FormBlock::id);
        return at != form.blocks.end() ? &*at : nullptr;
    }
    bool ClassOn(const Form& form, const char* id, const char* name)
    {
        for (const FormValue& v : form.values)
            if (v.id == id && v.kind == FormValueKind::Class && v.name == name) return v.on;
        return false;
    }
}

TEST(the_window_is_its_sidebar_and_the_open_pages_cards)
{
    Window w;
    const Form form = w.Pass();
    CHECK_EQ(form.blocks.front().id, kSidebarBlock);
    CHECK(Has(form.blocks.front().rml, "<div class=\"nav\" id=\"nav-entities\">") &&
          Has(form.blocks.front().rml, "<div class=\"nav child\" id=\"nav-mobs\">"));
    CHECK(ClassOn(form, "nav-entities", "picked") && !ClassOn(form, "nav-mobs", "picked"));
    CHECK(Block(form, "entities-nameplate-style") != nullptr && Block(form, "entities-colors-target-type") != nullptr);
}

TEST(a_click_in_the_sidebar_opens_that_page_at_its_top)
{
    Window w;
    const Form form = w.Pass({{"nav-toppanel", kClickEvent, ""}});
    CHECK(w.state.page == Page::TopPanel && form.scrollToTop);
    CHECK(Block(form, "toppanel-top-panel") != nullptr && Block(form, "entities-nameplate-style") == nullptr);
    CHECK(ClassOn(form, "nav-toppanel", "picked"));
}

TEST(every_page_builds_its_cards_with_ids_of_their_own)
{
    Window w;
    for (const PageInfo& info : kPages)
    {
        w.state.page    = info.page;
        const Form form = w.Pass();
        CHECK(form.blocks.size() >= 2);
        std::vector<std::string> ids;
        for (const FormBlock& block : form.blocks)
        {
            CHECK(std::ranges::find(ids, block.id) == ids.end());
            ids.push_back(block.id);
            // Every card closes what it opens.
            size_t opened = 0, closed = 0;
            for (size_t at = 0; (at = block.rml.find("<div", at)) != std::string::npos; ++at)
                ++opened;
            for (size_t at = 0; (at = block.rml.find("</div>", at)) != std::string::npos; ++at)
                ++closed;
            CHECK_EQ(opened, closed);
        }
    }
}

TEST(a_pages_controls_change_the_settings_they_show)
{
    Window w;
    w.state.page = Page::TopPanel;
    CHECK(w.settings.showBar && w.settings.barHeight == 45);
    const Form form = w.Pass({{"toppanel-top-panel-height", kChangeEvent, "52"},
        {"toppanel-top-panel-separators-s-2", kClickEvent, ""}});
    CHECK(form.changed && w.settings.barHeight == 52 && w.settings.barSeparators == BarSeparators::None);
    // With the panel switched off by its card's checkbox, its settings are faint and take nothing.
    const Form off = w.Pass({{"toppanel-top-panel-on", kClickEvent, ""}, {"toppanel-top-panel-height", kChangeEvent, "30"}});
    CHECK(off.changed && !w.settings.showBar && w.settings.barHeight == 52);
    CHECK(ClassOn(off, "toppanel-top-panel-height-box", "off"));
    w.Pass({{"toppanel-top-panel-on", kClickEvent, ""}});
    CHECK(w.settings.showBar);
    // The theme's dropdown lists every theme, and a click on one picks it.
    w.Pass({{"toppanel-top-panel-theme", kClickEvent, ""}});
    w.Pass({{"toppanel-top-panel-theme-o-1", kClickEvent, ""}});
    CHECK(w.settings.barTheme == BarTheme::UmbraDark);
}

TEST(a_widgets_chip_dropped_on_another_goes_before_it)
{
    Window w;
    w.state.page = Page::Widgets;
    const std::vector<BarWidget> left = w.settings.barSections[static_cast<int>(BarSection::Left)];
    CHECK(left.size() >= 2);
    const std::string first  = "widgets-layout-chip-" + std::to_string(static_cast<int>(left[0]));
    const std::string second = "widgets-layout-chip-" + std::to_string(static_cast<int>(left[1]));
    const Form form = w.Pass({{first, kDropEvent, second}});
    const std::vector<BarWidget>& now = w.settings.barSections[static_cast<int>(BarSection::Left)];
    CHECK(form.changed && now[0] == left[1] && now[1] == left[0]);
    // A click on a chip opens its tab, at the page's top.
    const Form clicked = w.Pass({{first, kClickEvent, ""}});
    CHECK(w.state.widget == static_cast<int>(left[0]) && clicked.scrollToTop);
}

TEST(a_friend_is_added_by_name_and_removed_by_its_chip)
{
    Window w;
    w.state.page = Page::Players;
    w.Pass({{"players-friends-add-friend", kCommitEvent, "Amutiyaal"}});
    CHECK(w.settings.friends == std::vector<std::string>{"Amutiyaal"});
    const Form form = w.Pass();
    CHECK(Has(Block(form, "players-friends")->rml, "Amutiyaal  x"));
    w.Pass({{"players-friends-friend-0", kClickEvent, ""}});
    CHECK(w.settings.friends.empty());
}

TEST(the_reset_card_asks_twice)
{
    Window w;
    w.state.page         = Page::ConfigMenu;
    w.settings.barHeight = 60;
    w.Pass({{"configmenu-reset-reset-all-settings", kClickEvent, ""}});
    CHECK(w.state.confirmReset && w.settings.barHeight == 60);
    const Form form = w.Pass({{"configmenu-reset-reset-everything", kClickEvent, ""}});
    CHECK(form.changed && !w.state.confirmReset && w.settings.barHeight == Settings{}.barHeight);
}

TEST(a_key_row_takes_the_next_key_pressed_that_was_not_held)
{
    Window w;
    w.state.page = Page::MouseCursor;
    std::vector<uint8_t> down = {0x11}; // Ctrl is held as the button is clicked
    const auto pass = [&](std::vector<FormEvent> events) {
        Form form(std::move(events), w.state.form,
            [&](uint8_t vk) { return std::ranges::find(down, vk) != down.end(); });
        BuildMenu(form, w.settings, w.readings, w.state, w.fonts, w.menuFonts);
        return form.changed;
    };
    const size_t before = w.settings.keepPointerKeys.size();
    pass({{"mousecursor-mouse-pointer-keep-pointer-add", kClickEvent, ""}});
    CHECK(w.state.keyCapture.on);
    pass({});
    CHECK(w.state.keyCapture.on && w.settings.keepPointerKeys.size() == before); // Ctrl was held already
    down.push_back(0x70); // F1
    const bool changed = pass({});
    CHECK(changed && !w.state.keyCapture.on && w.settings.keepPointerKeys.size() == before + 1);
    CHECK_EQ(w.settings.keepPointerKeys.back(), uint8_t{0x70});
}

TEST(only_the_lines_above_the_bottom_one_have_an_alignment)
{
    // The bottom line stays centered on the game's name and the others are set against it.
    Window w;
    w.state.page = Page::Mobs;
    for (int tab = 0; tab < kPlateLines; ++tab)
    {
        w.state.design.kind     = PlateKind::Mobs;
        w.state.design.styleTab = tab;
        w.settings.Kind(PlateKind::Mobs).lines[tab].syncWith = kNoSync;
        const Form form       = w.Pass();
        const FormBlock* card = Block(form, "mobs-line-style");
        CHECK(card != nullptr && Has(card->rml, ">Text Size<"));
        CHECK_EQ(Has(card->rml, ">Alignment<"), tab < kPlateLines - 1);
    }
}
