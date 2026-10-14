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
    CHECK(Block(form, "entities-nameplates") != nullptr && Block(form, "entities-colors-entity-type") != nullptr);
}

TEST(a_click_in_the_sidebar_opens_that_page_at_its_top)
{
    Window w;
    const Form form = w.Pass({{"nav-toppanel", kClickEvent, ""}});
    CHECK(w.state.page == Page::TopPanel && form.scrollToTop);
    CHECK(Block(form, "toppanel-top-panel") != nullptr && Block(form, "entities-nameplates") == nullptr);
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

TEST(a_page_whose_cards_are_all_switched_off_says_what_switches_them_on)
{
    const auto shown = [](const Form& form) {
        std::vector<std::string> ids;
        for (const FormBlock& block : form.blocks)
            if (block.id != kSidebarBlock && !ClassOn(form, block.id.c_str(), "off")) ids.push_back(block.id);
        return ids;
    };
    Window w;
    CHECK(shown(w.Pass()).size() > 3u);
    w.settings.enabled = false;
    const std::vector<std::string> off = shown(w.Pass());
    CHECK_EQ(off.size(), size_t{1}); // the one card that says HeadsUp is off
    // The Debug page, which switches HeadsUp on, keeps its cards.
    w.state.page = Page::Debug;
    const Form debug = w.Pass({{"debug-headsup-on", kClickEvent, ""}});
    CHECK(w.settings.enabled && debug.changed);
    // With the top panel off, its widgets' page has nothing to set either.
    w.settings.showBar = false;
    w.state.page       = Page::Widgets;
    CHECK_EQ(shown(w.Pass()).size(), size_t{1});
    CHECK(shown(w.Pass()) != off);
}

TEST(the_cursor_style_list_has_headsups_styles_then_the_folders_files)
{
    Window w;
    const std::vector<std::string> files{"Gold Arrow", "sword"};
    w.readings.cursorFiles = &files;
    const char* list = "entities-target-cursor-cursor-style";
    const Form open  = w.Pass({{list, kClickEvent, ""}});
    CHECK(open.popup && w.state.cursorsRequested); // opening it asks for the folder to be read again
    CHECK(Has(open.popup->rml, ">Vanilla<") && Has(open.popup->rml, ">Phoenix Feather<") && Has(open.popup->rml, ">sword<"));
    CHECK(ClassOn(open, "entities-target-cursor-cursor-style-o-2", "picked")); // the feather, a new install's
    w.state.cursorsRequested = false;
    w.Pass();
    CHECK(!w.state.cursorsRequested); // once, when it opens: not every pass it stays open
    // A file is the File style with its name; HeadsUp's own styles have no file.
    CHECK(w.Pass({{"entities-target-cursor-cursor-style-o-5", kClickEvent, ""}}).changed);
    CHECK(w.settings.cursorStyle == CursorStyle::File && w.settings.cursorFile == "sword");
    w.Pass({{list, kClickEvent, ""}});
    w.Pass({{"entities-target-cursor-cursor-style-o-3", kClickEvent, ""}});
    CHECK(w.settings.cursorStyle == CursorStyle::Chocobo);
    // A file the folder no longer has stays chosen, and listed, until another is picked.
    w.settings.cursorStyle = CursorStyle::File;
    w.settings.cursorFile  = "gone";
    const Form kept = w.Pass({{list, kClickEvent, ""}});
    CHECK(Has(kept.popup->rml, ">gone<") && ClassOn(kept, "entities-target-cursor-cursor-style-o-6", "picked"));
    CHECK(w.settings.cursorStyle == CursorStyle::File && w.settings.cursorFile == "gone");
}

TEST(a_style_picked_comes_at_its_own_width_for_the_height_set)
{
    // The feather is slender, the triangle wide and a picture square: a style picked would otherwise be squeezed
    // into the width of the one before.
    Window w;
    const char* list = "entities-target-cursor-cursor-style";
    w.settings.cursorHeight = 40;
    w.settings.cursorWidth  = 11;
    const auto pick = [&](int option) {
        w.Pass({{list, kClickEvent, ""}});
        w.Pass({{(std::string(list) + "-o-" + std::to_string(option)).c_str(), kClickEvent, ""}});
        return w.settings.cursorWidth;
    };
    CHECK_EQ(pick(1), 50); // the triangle, 0.8 as tall as wide
    CHECK_EQ(pick(3), 40); // the chocobo
    CHECK_EQ(pick(2), 19); // the feather, 2.155 times as tall as wide
    // The width is the player's from there: the same style picked again, or a pass with no pick, leaves it.
    w.settings.cursorWidth = 30;
    CHECK_EQ(pick(2), 30);
    w.Pass();
    CHECK_EQ(w.settings.cursorWidth, 30);
    // The game's own cursor has no width to set.
    CHECK_EQ(pick(0), 30);
}

TEST(the_games_own_cursor_has_no_settings_and_a_picture_no_color)
{
    Window w;
    const auto hidden = [&](const Form& form, const char* row) { return ClassOn(form, row, "off"); };
    const Form feather = w.Pass();
    CHECK(!hidden(feather, "entities-target-cursor-width-label-row") && !hidden(feather, "entities-target-cursor-color-label-row"));
    w.settings.cursorStyle = CursorStyle::Chocobo;
    const Form chocobo = w.Pass();
    CHECK(!hidden(chocobo, "entities-target-cursor-width-label-row") && !hidden(chocobo, "entities-target-cursor-edge-color-label-row"));
    CHECK(hidden(chocobo, "entities-target-cursor-color-label-row")); // it keeps its own colors
    w.settings.cursorStyle = CursorStyle::Vanilla;
    const Form vanilla = w.Pass();
    for (const char* row : {"entities-target-cursor-width-label-row", "entities-target-cursor-float-label-row",
             "entities-target-cursor-color-label-row", "entities-target-cursor-edge-color-label-row"})
        CHECK(hidden(vanilla, row));
    CHECK(!hidden(vanilla, "entities-target-cursor-cursor-style-label-row")); // the list stays, to pick another
}

TEST(every_kind_has_a_nameplate_opacity_card_and_yours_none_of_its_fading)
{
    Window w;
    for (const Page page : {Page::Mobs, Page::Players, Page::Npcs, Page::Pets, Page::Self})
    {
        w.state.page = page;
        const std::string card = std::string(PageKey(page)) + "-nameplate-opacity";
        const Form form        = w.Pass();
        const FormBlock* block = Block(form, card.c_str());
        CHECK(block != nullptr);
        CHECK(Has(block->rml, (card + "-no-target").c_str()) && Has(block->rml, (card + "-not-targeted").c_str()) &&
              Has(block->rml, (card + "-is-targeted").c_str()));
        CHECK_EQ(Has(block->rml, (card + "-fade-in").c_str()), page != Page::Self);
        if (page == Page::Self) continue;
        // The range is the fade's own: it shows once Fade In is on.
        CHECK(ClassOn(form, (card + "-range-label-row").c_str(), "off"));
        const Form faded = w.Pass({{card + "-fade-in", kClickEvent, ""}});
        CHECK(faded.changed && w.settings.Kind(*KindOfPage(page)).opacity.fadeIn);
        CHECK(!ClassOn(faded, (card + "-range-label-row").c_str(), "off"));
        w.Pass({{card + "-not-targeted", kChangeEvent, "35"}});
        CHECK_EQ(w.settings.Kind(*KindOfPage(page)).opacity.notTargeted, 35);
    }
}

TEST(the_pages_of_the_kinds_with_status_icons_set_how_many_a_nameplate_shows)
{
    Window w;
    for (const Page page : {Page::Players, Page::Self})
    {
        w.state.page           = page;
        const std::string list = std::string(PageKey(page)) + "-nameplate-design-max-icons-shown";
        CHECK(w.Pass({{list, kClickEvent, ""}}).popup);
        CHECK(w.Pass({{list + "-o-2", kClickEvent, ""}}).changed); // the list starts at one icon
        CHECK_EQ(w.settings.Kind(*KindOfPage(page)).maxIcons, 3);
    }
    CHECK_EQ(w.settings.Kind(PlateKind::Mobs).maxIcons, kCountedIcons);
    for (const Page page : {Page::Mobs, Page::Npcs, Page::Pets})
    {
        w.state.page          = page;
        const std::string card = std::string(PageKey(page)) + "-nameplate-design";
        const Form form        = w.Pass();
        CHECK(!Has(Block(form, card.c_str())->rml, "max-icons-shown"));
    }
}

TEST(a_lines_glow_can_be_switched_off_for_combat_once_it_has_a_glow)
{
    Window w;
    w.state.page = Page::Players;
    const char* row = "players-line-style-disable-glow-in-combat-players0-label-row";
    CHECK(ClassOn(w.Pass(), row, "off")); // no glow, nothing to switch off
    w.settings.Kind(PlateKind::Players).lines[0].glow = 80;
    CHECK(!ClassOn(w.Pass(), row, "off"));
    CHECK(w.Pass({{"players-line-style-disable-glow-in-combat-players0", kClickEvent, ""}}).changed);
    CHECK(w.settings.Kind(PlateKind::Players).lines[0].glowOffInCombat);
}

TEST(a_color_off_its_default_has_an_arrow_that_puts_it_back)
{
    Window w;
    const char* arrow = "entities-colors-claim-status-claimed-yours-colors-claim-status-reset";
    const Color original = w.settings.palette.claim[static_cast<int>(ClaimState::Yours)];
    CHECK(ClassOn(w.Pass(), arrow, "hidden")); // nothing to put back
    w.settings.palette.claim[static_cast<int>(ClaimState::Yours)] = Color{{0.1f, 0.2f, 0.3f}};
    const Form moved = w.Pass();
    CHECK(!ClassOn(moved, arrow, "hidden"));
    CHECK(ClassOn(moved, "entities-colors-claim-status-unclaimed-colors-claim-status-reset", "hidden")); // each color its own
    const Form back = w.Pass({{arrow, kClickEvent, ""}});
    CHECK(back.changed && w.settings.palette.claim[static_cast<int>(ClaimState::Yours)] == original);
    CHECK(ClassOn(back, arrow, "hidden"));
    // A badge color has one too.
    w.state.page = Page::Mobs;
    w.settings.weakBadgeColor = Color{{0.0f, 0.0f, 0.0f}};
    w.Pass({{"mobs-badges-weak-color-reset", kClickEvent, ""}});
    CHECK(w.settings.weakBadgeColor == Settings{}.weakBadgeColor);
}

TEST(the_dash_marks_are_listed_as_shapes_and_no_words_but_no)
{
    Window w;
    const char* list = "entities-nameplates-replace-dashes";
    const Form open  = w.Pass({{list, kClickEvent, ""}});
    CHECK(open.popup);
    CHECK(Has(open.popup->rml, ">No<"));
    for (const char* shape : {"dash dot", "dash square", "dash diamond"})
        CHECK(Has(open.popup->rml, shape));
    CHECK(!Has(open.popup->rml, "Diamond") && !Has(open.popup->rml, "Square") && !Has(open.popup->rml, "Dot"));
    CHECK(ClassOn(open, "entities-nameplates-replace-dashes-o-3", "picked")); // the diamond, a new install's
    CHECK(w.Pass({{"entities-nameplates-replace-dashes-o-0", kClickEvent, ""}}).changed);
    CHECK(w.settings.dashMark == DashMark::None);
}
