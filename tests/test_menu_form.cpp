#include "layout_edit.h"
#include "menu_form.h"
#include "settings.h"
#include "test.h"

#include <algorithm>
#include <map>
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
        WindowLayout layout = kWindowLayout;

        Form Pass(std::vector<FormEvent> events = {})
        {
            Form form(std::move(events), state.form);
            BuildMenu(form, settings, readings, state, fonts, menuFonts, layout);
            return form;
        }
    };

    // The keys a save writes, with their values.
    class SavedKeys final : public SettingsStore
    {
    public:
        std::map<std::string, std::string> values;
        bool GetBool(const char*, bool fallback) override { return fallback; }
        float GetFloat(const char*, float fallback) override { return fallback; }
        std::string GetString(const char*, const char* fallback) override { return fallback; }
        void Set(const char* key, const char* value) override { values[key] = value; }
    };
    // The saved keys whose values differ between two settings.
    std::vector<std::string> KeysChanged(const Settings& before, const Settings& after)
    {
        SavedKeys a, b;
        SaveSettings(before, a);
        SaveSettings(after, b);
        std::vector<std::string> changed;
        for (const auto& [key, value] : b.values)
            if (a.values[key] != value) changed.push_back(key);
        return changed;
    }
    // Where the first of these ids is in a block's markup, past the end when it is not.
    size_t At(const FormBlock* block, const std::string& id)
    {
        return block != nullptr ? block->rml.find("id=\"" + id + "\"") : std::string::npos;
    }

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
    for (const LayoutPage& info : kWindowLayout.pages)
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
    // It stands under the cards on its own, with no card around it.
    const FormBlock* reset = Block(form, "configmenu-reset");
    CHECK(reset != nullptr && reset->bare && !Has(reset->rml, "card-head"));
    CHECK(!Block(form, "configmenu-settings-window")->bare);
}

TEST(the_debug_mode_card_shows_its_switch_alone_until_debug_mode_is_on)
{
    Window w;
    w.state.page   = Page::ConfigMenu;
    const Form off = w.Pass();
    CHECK(ClassOn(off, "configmenu-debug-mode-debug-report", kOffClass));
    CHECK(ClassOn(off, "configmenu-debug-mode-mobs-outlined-label-row", kOffClass));
    w.Pass({{"configmenu-debug-mode-debug-report", kClickEvent, ""}});
    CHECK(!w.state.debugRequested);
    const Form on = w.Pass({{"configmenu-debug-mode-on", kClickEvent, ""}});
    CHECK(w.settings.debugMode && on.changed);
    CHECK(!ClassOn(on, "configmenu-debug-mode-mobs-outlined-label-row", kOffClass));
    const Form asked = w.Pass({{"configmenu-debug-mode-debug-report", kClickEvent, ""}});
    CHECK(w.state.debugRequested);
    // What this frame read comes after the buttons.
    const FormBlock* card = Block(asked, "configmenu-debug-mode");
    CHECK(At(card, "configmenu-debug-mode-debug-report") < At(card, "configmenu-debug-mode-mobs-outlined"));
}

TEST(a_build_that_watches_has_a_watch_button_and_the_card_says_where_the_last_report_went)
{
    Window w;
    w.state.page         = Page::ConfigMenu;
    w.settings.debugMode = true;
    const char* watch    = "configmenu-debug-mode-watch";
    CHECK(!Has(Block(w.Pass(), "configmenu-debug-mode")->rml, watch)); // a build with nothing to watch
    w.readings.watching = false;
    const std::string start = Block(w.Pass(), "configmenu-debug-mode")->rml;
    CHECK(Has(start, w.layout.WordOf(WindowWord::DebugReportWatch).name));
    w.Pass({{watch, kClickEvent, ""}});
    CHECK(w.state.watchRequested);
    w.readings.watching = true;
    CHECK(Has(Block(w.Pass(), "configmenu-debug-mode")->rml, w.layout.WordOf(WindowWord::DebugReportStop).name));
    // No report yet, no line for one; then the file's name.
    CHECK(!Has(start, "debug-20261015"));
    const std::string report = "C:\\PhoenixXI\\logs\\headsup\\debug-20261015-101500.txt";
    w.readings.lastReport    = &report;
    const std::string saved  = Block(w.Pass(), "configmenu-debug-mode")->rml;
    CHECK(Has(saved, "debug-20261015-101500.txt") && !Has(saved, "PhoenixXI"));
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
        const std::string line = "mobs" + std::to_string(tab);
        CHECK(card != nullptr && Has(card->rml, ("mobs-line-style-text-size-" + line).c_str()));
        CHECK_EQ(Has(card->rml, ("mobs-line-style-alignment-" + line).c_str()), tab < kPlateLines - 1);
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
    // The HeadsUp Config page, which switches HeadsUp on, keeps its cards.
    w.state.page = Page::ConfigMenu;
    const Form config = w.Pass({{"configmenu-settings-window-enable-headsup", kClickEvent, ""}});
    CHECK(w.settings.enabled && config.changed);
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
    // Each of HeadsUp's own styles, then each file of the folder: six rows, the last the second file.
    CHECK(Has(open.popup->rml, "entities-target-cursor-cursor-style-o-5") && !Has(open.popup->rml, "entities-target-cursor-cursor-style-o-6"));
    CHECK(Has(open.popup->rml, ">Gold Arrow<") && Has(open.popup->rml, ">sword<"));
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

TEST(the_cursor_icon_list_has_vanilla_the_chocobo_then_the_folders_files)
{
    Window w;
    const std::vector<std::string> files{"Gold Arrow", "sword"};
    w.readings.cursorFiles = &files;
    w.state.page           = Page::MouseCursor;
    const std::string list = "mousecursor-mouse-pointer-pointer-icon";
    const auto option      = [&](int n) { return list + "-o-" + std::to_string(n); };
    const Form open        = w.Pass({{list, kClickEvent, ""}});
    CHECK(open.popup && w.state.cursorsRequested); // opening it asks for the folder to be read again
    CHECK(Has(open.popup->rml, option(3).c_str()) && !Has(open.popup->rml, option(4).c_str()));
    CHECK(Has(open.popup->rml, ">Gold Arrow<") && Has(open.popup->rml, ">sword<"));
    CHECK(ClassOn(open, option(0).c_str(), "picked")); // the game's own, a new install's
    CHECK(w.Pass({{option(1), kClickEvent, ""}}).changed);
    CHECK(w.settings.pointerIcon == PointerIcon::Chocobo && w.settings.pointerFile.empty());
    w.Pass({{list, kClickEvent, ""}});
    w.Pass({{option(3), kClickEvent, ""}});
    CHECK(w.settings.pointerIcon == PointerIcon::File && w.settings.pointerFile == "sword");
    // The target cursor's pick is its own.
    CHECK(w.settings.cursorStyle == kDefaultCursorStyle && w.settings.cursorFile.empty());
    // A file the folder no longer has stays chosen, and listed, until another is picked.
    w.settings.pointerFile = "gone";
    const Form kept        = w.Pass({{list, kClickEvent, ""}});
    CHECK(Has(kept.popup->rml, ">gone<") && ClassOn(kept, option(4).c_str(), "picked"));
    w.Pass({{option(0), kClickEvent, ""}});
    CHECK(w.settings.pointerIcon == PointerIcon::Vanilla && w.settings.pointerFile.empty());
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
        const std::string card = std::string(kWindowLayout.PageOf(page).key) + "-nameplate-opacity";
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
        const std::string list = std::string(kWindowLayout.PageOf(page).key) + "-nameplate-design-max-icons-shown";
        CHECK(w.Pass({{list, kClickEvent, ""}}).popup);
        CHECK(w.Pass({{list + "-o-2", kClickEvent, ""}}).changed); // the list starts at one icon
        CHECK_EQ(w.settings.Kind(*KindOfPage(page)).maxIcons, 3);
    }
    CHECK_EQ(w.settings.Kind(PlateKind::Mobs).maxIcons, kCountedIcons);
    for (const Page page : {Page::Mobs, Page::Npcs, Page::Pets})
    {
        w.state.page          = page;
        const std::string card = std::string(kWindowLayout.PageOf(page).key) + "-nameplate-design";
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
    const char* arrow = "entities-colors-claim-status-claimed-yours-claim-colors-reset";
    const Color original = w.settings.palette.claim[static_cast<int>(ClaimState::Yours)];
    CHECK(ClassOn(w.Pass(), arrow, "hidden")); // nothing to put back
    w.settings.palette.claim[static_cast<int>(ClaimState::Yours)] = Color{{0.1f, 0.2f, 0.3f}};
    const Form moved = w.Pass();
    CHECK(!ClassOn(moved, arrow, "hidden"));
    CHECK(ClassOn(moved, "entities-colors-claim-status-unclaimed-claim-colors-reset", "hidden")); // each color its own
    w.settings.palette.claim[static_cast<int>(ClaimState::CalledForHelp)] = Color{{0.1f, 0.2f, 0.3f}};
    w.Pass({{"entities-colors-claim-status-called-for-help-claim-colors-reset", kClickEvent, ""}});
    CHECK(w.settings.palette.claim[static_cast<int>(ClaimState::CalledForHelp)] == Settings{}.palette.claim[static_cast<int>(ClaimState::CalledForHelp)]);
    const Form back = w.Pass({{arrow, kClickEvent, ""}});
    CHECK(back.changed && w.settings.palette.claim[static_cast<int>(ClaimState::Yours)] == original);
    CHECK(ClassOn(back, arrow, "hidden"));
    // A badge color has one too.
    w.state.page = Page::Mobs;
    w.settings.weakBadgeColor = Color{{0.0f, 0.0f, 0.0f}};
    w.Pass({{"mobs-badges-weak-color-reset", kClickEvent, ""}});
    CHECK(w.settings.weakBadgeColor == Settings{}.weakBadgeColor);
}

TEST(the_dash_mark_is_picked_from_a_list_whose_first_row_is_none)
{
    Window w;
    const char* list = "entities-nameplates-replace-dashes";
    const Form open  = w.Pass({{list, kClickEvent, ""}});
    CHECK(open.popup);
    CHECK(ClassOn(open, "entities-nameplates-replace-dashes-o-3", "picked")); // the diamond, a new install's
    CHECK(w.Pass({{"entities-nameplates-replace-dashes-o-0", kClickEvent, ""}}).changed);
    CHECK(w.settings.dashMark == DashMark::None);
}

TEST(a_page_moved_in_the_layout_moves_in_the_sidebar_and_opens_by_its_key)
{
    test::EditedLayout edited;
    edited.MovePage("configmenu", 0);
    Window w;
    w.layout              = edited.Layout();
    const Form form       = w.Pass();
    const FormBlock& side = form.blocks.front();
    CHECK(At(&side, "nav-configmenu") < At(&side, "nav-entities"));
    w.Pass({{"nav-configmenu", kClickEvent, ""}});
    CHECK(w.state.page == Page::ConfigMenu);
    CHECK(w.layout.PageNamed("configmenu") == Page::ConfigMenu);
    CHECK(!w.layout.PageNamed("debug"));
}

TEST(renaming_a_page_a_card_and_a_setting_changes_no_control_id)
{
    test::EditedLayout edited;
    edited.RenamePage("mousecursor", "Pointer");
    edited.RenameCard("mouse-pointer", "Mouse");
    edited.RenameSetting("pointer-fix", "a fix of the pointer"); // shown as written, small letters and all
    Window w;
    w.layout     = edited.Layout();
    w.state.page = Page::MouseCursor;
    const Form form = w.Pass({{"mousecursor-mouse-pointer-pointer-fix", kClickEvent, ""}});
    CHECK(form.changed && !w.settings.fixPointer);
    const FormBlock* card = Block(form, "mousecursor-mouse-pointer");
    CHECK(card != nullptr && Has(card->rml, ">Mouse<") && Has(card->rml, ">a fix of the pointer<"));
    CHECK(Has(form.blocks.front().rml, ">Pointer<"));
}

TEST(a_setting_moved_to_another_card_works_there_and_sets_only_its_own_key)
{
    test::EditedLayout edited;
    edited.MoveSetting("pointer-fix", "settings-window", 0);
    Window w;
    w.layout              = edited.Layout();
    w.state.page          = Page::ConfigMenu;
    const Settings before = w.settings;
    const Form form       = w.Pass({{"configmenu-settings-window-pointer-fix", kClickEvent, ""}});
    CHECK(form.changed && !w.settings.fixPointer);
    CHECK_EQ(KeysChanged(before, w.settings).size(), size_t{1});
    // It comes first in its new card, and is gone from its old one.
    const FormBlock* card = Block(form, "configmenu-settings-window");
    CHECK(At(card, "configmenu-settings-window-pointer-fix") < At(card, "configmenu-settings-window-window-theme"));
    w.state.page = Page::MouseCursor;
    CHECK(At(Block(w.Pass(), "mousecursor-mouse-pointer"), "mousecursor-mouse-pointer-pointer-fix") == std::string::npos);
}

TEST(a_setting_moved_to_another_page_hides_as_it_would_at_home)
{
    test::EditedLayout edited;
    edited.MoveSetting("min-size", "debug-mode", 0);
    Window w;
    w.layout                     = edited.Layout();
    w.state.page                 = Page::ConfigMenu;
    w.settings.scaleWithDistance = false;
    CHECK(ClassOn(w.Pass(), "configmenu-debug-mode-min-size-label-row", kOffClass));
    w.settings.scaleWithDistance = true;
    const Form form = w.Pass({{"configmenu-debug-mode-min-size", kChangeEvent, "75"}});
    CHECK(!ClassOn(form, "configmenu-debug-mode-min-size-label-row", kOffClass));
    CHECK(form.changed && w.settings.scaleMin == 75);
}

TEST(a_card_moved_to_another_page_shows_there_with_its_settings)
{
    test::EditedLayout edited;
    edited.MoveCard("mouse-pointer", "configmenu", 0);
    Window w;
    w.layout     = edited.Layout();
    w.state.page = Page::ConfigMenu;
    const Form form = w.Pass({{"configmenu-mouse-pointer-pointer-fix", kClickEvent, ""}});
    CHECK(form.blocks.size() > 1 && form.blocks[1].id == "configmenu-mouse-pointer");
    CHECK(form.changed && !w.settings.fixPointer);
}

TEST(the_visibility_rows_come_in_the_layouts_order)
{
    test::EditedLayout edited;
    edited.SwapRows("visibility", 0, 1);
    Window w;
    w.state.page             = Page::Mobs;
    const auto order = [&](const Form& form) {
        const FormBlock* card = Block(form, "mobs-visibility");
        return At(card, "mobs-visibility-aggressive-label") < At(card, "mobs-visibility-passive-label");
    };
    CHECK(order(w.Pass()));
    w.layout = edited.Layout();
    CHECK(!order(w.Pass()));
}

TEST(a_palettes_rows_moved_in_the_layout_keep_their_own_colors)
{
    test::EditedLayout edited;
    edited.SwapRows("entity-type-colors", 0, 1);
    Window w;
    w.layout = edited.Layout();
    const char* mobArrow = "entities-colors-entity-type-mob-entity-type-colors-reset";
    const int mob        = static_cast<int>(TargetType::Mob);
    const Color original = w.settings.palette.type[mob];
    w.settings.palette.type[mob] = Color{{0.1f, 0.2f, 0.3f}};
    const Form moved             = w.Pass();
    CHECK(!ClassOn(moved, mobArrow, "hidden"));
    CHECK(ClassOn(moved, "entities-colors-entity-type-player-entity-type-colors-reset", "hidden"));
    const Form back = w.Pass({{mobArrow, kClickEvent, ""}});
    CHECK(back.changed && w.settings.palette.type[mob] == original);
}

TEST(a_status_rows_moved_in_the_layout_keep_their_own_switches)
{
    test::EditedLayout edited;
    edited.SwapRows("status-table", 0, 1);
    Window w;
    w.layout     = edited.Layout();
    w.state.page = Page::Mobs;
    w.settings.Kind(PlateKind::Mobs).lines[0].text = "$status";
    const StatusBadgeRule bind = w.settings.statusBadges[static_cast<int>(StatusTableRow::Bind)];
    const bool shown           = w.settings.statusBadges[static_cast<int>(StatusTableRow::Paralyze)].show;
    CHECK(w.Pass({{"mobs-statuses-status-paralyze", kClickEvent, ""}}).changed);
    CHECK(w.settings.statusBadges[static_cast<int>(StatusTableRow::Paralyze)].show != shown);
    CHECK(w.settings.statusBadges[static_cast<int>(StatusTableRow::Bind)] == bind);
}

TEST(a_card_kept_to_some_kinds_shows_on_their_pages_alone)
{
    test::EditedLayout edited;
    edited.SetKinds("placeholder-timers", KindBit(PlateKind::Mobs));
    Window w;
    w.layout     = edited.Layout();
    w.state.page = Page::Mobs;
    CHECK(Block(w.Pass(), "mobs-placeholder-timers") != nullptr);
    w.state.page = Page::Self;
    CHECK(Block(w.Pass(), "self-placeholder-timers") == nullptr);
}

namespace
{
    // The text the window gives a control this pass.
    std::string ValueOn(const Form& form, const std::string& id)
    {
        for (const FormValue& v : form.values)
            if (v.id == id && v.kind == FormValueKind::Value) return v.text;
        return {};
    }
    // Status, whose highest window, the portrait, the game puts at 16.
    const std::string kStatusShift = "userinterface-window-groups-status-shift";
    const std::string kStatusReset = "userinterface-window-groups-status-reset";
    constexpr int kPortraitTop     = 16;
    constexpr auto kOn = static_cast<size_t>(HelpMode::On), kOff = static_cast<size_t>(HelpMode::Off);
}

TEST(a_views_shift_shows_its_default_until_one_is_typed_for_the_help_mode_shown_and_its_arrow_puts_it_back)
{
    Window w;
    w.state.page = Page::UserInterface;
    const int on = w.settings.placedUnder[kOn], off = w.settings.placedUnder[kOff];
    CHECK(WindowHelpMode(w.settings) == HelpMode::On);
    CHECK_EQ(ValueOn(w.Pass(), kStatusShift), std::to_string(on - kPortraitTop));
    CHECK(ClassOn(w.Pass(), kStatusReset.c_str(), "hidden"));
    CHECK(w.Pass({{kStatusShift, kCommitEvent, "30"}}).changed);
    const std::vector<ViewShift> typed = {{"status", 30}};
    CHECK(w.settings.viewShifts[kOn] == typed && w.settings.viewShifts[kOff].empty());
    CHECK(!ClassOn(w.Pass(), kStatusReset.c_str(), "hidden"));
    // The other mode's shifts, picked by its button, are its own.
    w.Pass({{"userinterface-window-groups-help-off", kClickEvent, ""}});
    CHECK_EQ(ValueOn(w.Pass(), kStatusShift), std::to_string(off - kPortraitTop));
    w.Pass({{"userinterface-window-groups-help-on", kClickEvent, ""}});
    CHECK(w.Pass({{kStatusReset, kClickEvent, ""}}).changed);
    CHECK(w.settings.viewShifts[kOn].empty());
    // The default typed is no shift of the player's.
    w.Pass({{kStatusShift, kCommitEvent, "30"}});
    w.Pass({{kStatusShift, kCommitEvent, std::to_string(on - kPortraitTop)}});
    CHECK(w.settings.viewShifts[kOn].empty());
}

TEST(auto_adjust_places_the_views_under_the_line_the_windows_last_went_under)
{
    Window w;
    w.state.page = Page::UserInterface;
    const char* button = "userinterface-game-windows-auto-adjust";
    const Settings before = w.settings;
    w.Pass({{button, kClickEvent, ""}}); // no frame of windows placed yet
    CHECK(w.settings == before);
    w.readings.windowsBand = 80; // a taller panel's help line
    w.readings.barBand     = 50;
    w.settings.viewShifts[kOn] = {{"status", 3}};
    CHECK(w.Pass({{button, kClickEvent, ""}}).changed);
    CHECK(w.settings.placedUnder[kOn] == 80 && w.settings.placedUnder[kOff] == 50);
    CHECK(w.settings.viewShifts[kOn].empty());
    CHECK_EQ(ValueOn(w.Pass(), kStatusShift), std::to_string(80 - kPortraitTop));
}
