#include "rml_menu.h"

#include "fonts.h"
#include "plate_template.h"
#include "rml_session.h"
#include "test.h"

#include <RmlUi/Core.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

using namespace headsup;

namespace
{
    constexpr float kEm       = 18.0f; // the window's text, in pixels
    constexpr double kVersion = 0.65;
    constexpr int kLeftButton = 0;
    constexpr double kFrameSeconds = 1.0 / 30.0; // the game's frame

    // The settings window on a screen of its own, worked as a player works it: by where its controls are.
    class Window
    {
    public:
        Settings settings;
        RmlMenu menu;

        explicit Window(MenuTheme theme = MenuTheme::Phoenix)
        {
            settings.menuTheme = theme;
            m_Host.context     = m_Rml.context;
            m_Host.textFont    = kBarlowCondensed;
            m_Host.menuFonts = {kBarlowCondensed};
            m_Host.keyDown   = [](uint8_t) { return false; };
            menu.open        = true;
            Frames(2);
        }
        ~Window() { menu.Close(); }
        // What RmlUi complained of since the window opened.
        std::string Said() const { return m_Rml.Said(); }

        // True when a frame of these changed a setting.
        bool Frames(int count = 1)
        {
            bool changed = false;
            for (int i = 0; i < count; ++i)
            {
                m_Rml.complaints.seconds += kFrameSeconds;
                changed = menu.Update(m_Host, settings, m_Readings, m_Fonts, kEm, kVersion) || changed;
                m_Host.context->Update();
                m_Host.context->Render();
            }
            return changed;
        }
        Rml::Element* Find(const std::string& id)
        {
            Rml::ElementDocument* document = m_Host.context->GetDocument(0);
            return document != nullptr ? document->GetElementById(id) : nullptr;
        }
        bool Shows(const std::string& id)
        {
            Rml::Element* element = Find(id);
            return element != nullptr && element->IsVisible(true);
        }
        void Page(const char* name)
        {
            CHECK(menu.OpenPage(name));
            Frames(2);
        }
        // Scrolls the control into view and clicks its middle; true when that changed a setting.
        bool Click(const std::string& id)
        {
            menu.CenterOf(id);
            const std::optional<Rml::Vector2f> at = menu.CenterOf(id, false);
            if (!at) throw test::Failure{"the window has no control " + id};
            m_Host.context->ProcessMouseMove(static_cast<int>(at->x), static_cast<int>(at->y), 0);
            bool changed = Frames();
            m_Host.context->ProcessMouseButtonDown(kLeftButton, 0);
            changed = Frames() || changed;
            m_Host.context->ProcessMouseButtonUp(kLeftButton, 0);
            return Frames(3) || changed;
        }

        // Presses the left button on one control and lets it go on another, or that far from the first.
        bool Drag(const std::string& from, const std::string& onto) { return Drag(from, Center(onto) - Center(from)); }
        bool Drag(const std::string& from, Rml::Vector2f by)
        {
            const Rml::Vector2f start = Center(from);
            m_Host.context->ProcessMouseMove(static_cast<int>(start.x), static_cast<int>(start.y), 0);
            bool changed = Frames();
            m_Host.context->ProcessMouseButtonDown(kLeftButton, 0);
            for (int step = 1; step <= kDragSteps; ++step)
            {
                const Rml::Vector2f at = start + by * (static_cast<float>(step) / kDragSteps);
                m_Host.context->ProcessMouseMove(static_cast<int>(at.x), static_cast<int>(at.y), 0);
                changed = Frames() || changed;
            }
            m_Host.context->ProcessMouseButtonUp(kLeftButton, 0);
            return Frames(3) || changed;
        }
        void Type(const std::string& text)
        {
            for (const char c : text)
                m_Host.context->ProcessTextInput(c);
            Frames();
        }
        bool Key(Rml::Input::KeyIdentifier key, int times = 1)
        {
            bool changed = false;
            for (int i = 0; i < times; ++i)
            {
                m_Host.context->ProcessKeyDown(key, 0);
                m_Host.context->ProcessKeyUp(key, 0);
                changed = Frames(2) || changed;
            }
            return changed;
        }
        // The middle of a control, scrolled into view.
        Rml::Vector2f Center(const std::string& id)
        {
            menu.CenterOf(id);
            const std::optional<Rml::Vector2f> at = menu.CenterOf(id, false);
            if (!at) throw test::Failure{"the window has no control " + id};
            return *at;
        }
        // The lowest control in view that has a tip, pointed at near its right end.
        void PointAtTheLowestTip()
        {
            Rml::Element* lowest = nullptr;
            std::vector<Rml::Element*> left{m_Host.context->GetDocument(0)};
            while (!left.empty())
            {
                Rml::Element* element = left.back();
                left.pop_back();
                const float bottom = element->GetAbsoluteOffset(Rml::BoxArea::Border).y + element->GetBox().GetSize(Rml::BoxArea::Border).y;
                if (element->HasAttribute("tip") && element->IsVisible(true) && bottom < test::RmlSession::kHeight &&
                    (lowest == nullptr || bottom > lowest->GetAbsoluteOffset(Rml::BoxArea::Border).y + lowest->GetBox().GetSize(Rml::BoxArea::Border).y))
                    lowest = element;
                for (int i = 0; i < element->GetNumChildren(); ++i)
                    left.push_back(element->GetChild(i));
            }
            if (lowest == nullptr) throw test::Failure{"no control in view has a tip"};
            const Rml::Vector2f at   = lowest->GetAbsoluteOffset(Rml::BoxArea::Border);
            const Rml::Vector2f size = lowest->GetBox().GetSize(Rml::BoxArea::Border);
            pointer = Rml::Vector2f(at.x + size.x - kFromTheEnd, at.y + size.y / 2.0f);
            m_Host.context->ProcessMouseMove(static_cast<int>(pointer.x), static_cast<int>(pointer.y), 0);
            Frames(3);
        }
        Rml::Vector2f pointer; // where PointAtTheLowestTip left it
        // The ids of the page's controls that take a click: switches, choices, tabs, dropdowns, buttons, chips and
        // card titles.
        std::vector<std::string> Clickable()
        {
            std::vector<std::string> ids;
            std::vector<Rml::Element*> left{Find("page")};
            while (!left.empty())
            {
                Rml::Element* element = left.back();
                left.pop_back();
                if (element == nullptr) continue;
                for (const char* kind : {"check", "seg", "tab", "dd", "btn", "wchip", "card-head", "swatch"})
                    if (!element->GetId().empty() && element->IsClassSet(kind))
                    {
                        ids.push_back(element->GetId());
                        break;
                    }
                for (int i = 0; i < element->GetNumChildren(); ++i)
                    left.push_back(element->GetChild(i));
            }
            return ids;
        }
        std::string TextOf(const std::string& id)
        {
            Rml::Element* element = Find(id);
            return element != nullptr ? element->GetInnerRML() : std::string();
        }
        // Closes the window and opens it at this place and size, as one the player moved and sized comes back.
        void OpenAt(int x, int y, int width, int height)
        {
            menu.Close();
            settings.menuX      = x;
            settings.menuY      = y;
            settings.menuWidth  = width;
            settings.menuHeight = height;
            menu.open           = true;
            Frames(2);
        }
        // The page's elements of a class.
        std::vector<Rml::Element*> OfClass(const char* name)
        {
            std::vector<Rml::Element*> found;
            std::vector<Rml::Element*> left{Find("page")};
            while (!left.empty())
            {
                Rml::Element* element = left.back();
                left.pop_back();
                if (element == nullptr) continue;
                if (element->IsClassSet(name)) found.push_back(element);
                for (int i = 0; i < element->GetNumChildren(); ++i)
                    left.push_back(element->GetChild(i));
            }
            return found;
        }

    private:
        static constexpr int kDragSteps    = 6;
        static constexpr float kFromTheEnd = 6.0f; // pixels
        test::RmlSession m_Rml; // before the menu: its document is closed while RmlUi still runs
        MenuHost m_Host;
        MenuReadings m_Readings;
        std::vector<std::string> m_Fonts = {"Arial", kBarlowCondensed, kMarcellusSC};
    };
}

TEST(the_window_opens_on_its_first_page_and_the_sidebar_opens_the_others)
{
    Window window;
    CHECK(window.Shows("nav-entities") && window.Shows("close"));
    window.Click("nav-mobs");
    CHECK(window.Shows("mobs-line-style-head"));
    window.Click("close");
    CHECK(!window.menu.open);
}

TEST(the_close_buttons_cross_is_two_turned_bars_in_the_middle_of_its_circle)
{
    auto middle = [](Rml::Element* element) {
        return element->GetAbsoluteOffset(Rml::BoxArea::Border) + element->GetBox().GetSize(Rml::BoxArea::Border) / 2.0f;
    };
    Window window;
    window.Frames(10); // past its coming in, during which it is drawn a little small
    Rml::Element* close = window.Find("close");
    CHECK(close != nullptr && close->GetNumChildren() == 2);
    for (int bar = 0; bar < 2; ++bar)
    {
        Rml::Element* line       = close->GetChild(bar);
        const Rml::Vector2f off  = middle(line) - middle(close);
        const Rml::Vector2f size = line->GetBox().GetSize(Rml::BoxArea::Border);
        CHECK(off.x > -0.5f && off.x < 0.5f && off.y > -0.5f && off.y < 0.5f);
        CHECK(size.x > 4.0f * size.y && size.y >= 1.0f);
        CHECK(line->GetProperty(Rml::PropertyId::Transform)->Get<Rml::TransformPtr>() != nullptr);
    }
    window.Click("close");
    CHECK(!window.menu.open);
}

TEST(a_dropdown_lists_its_choices_under_it_and_a_pick_sets_the_setting)
{
    Window window;
    window.Page("mobs");
    window.Click("mobs-line-style-line-style-t-1");
    CHECK(!window.Shows("mobs-line-style-color-mobs1-o-0"));
    window.Click("mobs-line-style-color-mobs1");
    CHECK(window.Shows("mobs-line-style-color-mobs1-o-0"));
    CHECK(window.Click("mobs-line-style-color-mobs1-o-0"));
    CHECK(window.settings.Kind(PlateKind::Mobs).lines[1].color.source == ColorSource::Custom);
    CHECK(!window.Shows("mobs-line-style-color-mobs1-o-0"));
}

TEST(a_list_of_checks_stays_open_while_its_rows_are_checked)
{
    Window window;
    window.Page("mobs");
    window.Click("mobs-line-style-line-style-t-1");
    window.Click("mobs-line-style-show-mobs1");
    CHECK(window.Click("mobs-line-style-show-mobs1-o-0"));
    CHECK(!window.settings.Kind(PlateKind::Mobs).lines[1].show.always);
    CHECK(window.Click("mobs-line-style-show-mobs1-o-1"));
    CHECK(window.Shows("mobs-line-style-show-mobs1-o-1"));
    window.Click("mobs-line-style-show-mobs1");
    CHECK(!window.Shows("mobs-line-style-show-mobs1-o-1"));
}

TEST(a_swatch_opens_the_color_picker_and_closes_it)
{
    Window window;
    window.Page("mobs");
    window.Click("mobs-line-style-line-style-t-1");
    window.Click("mobs-line-style-color-mobs1");
    window.Click("mobs-line-style-color-mobs1-o-0");
    window.Click("mobs-line-style-color-mobs1-swatch");
    CHECK(window.Shows("mobs-line-style-color-mobs1-swatch-p-hex"));
    window.Frames(60);
    window.Click("mobs-line-style-color-mobs1-swatch");
    CHECK(!window.Shows("mobs-line-style-color-mobs1-swatch-p-hex"));
}

TEST(rmlui_reads_every_page_of_both_themes_without_a_complaint)
{
    for (const MenuTheme theme : {MenuTheme::Phoenix, MenuTheme::Aurora})
    {
        Window window(theme);
        for (const PageInfo& page : kPages)
        {
            window.Page(PageKey(page.page).c_str());
            CHECK_EQ(window.Said(), "");
        }
    }
}

TEST(auroras_window_works_as_phoenixs_does)
{
    Window window(MenuTheme::Aurora);
    window.Page("mobs");
    window.Click("mobs-line-style-line-style-t-1");
    window.Click("mobs-line-style-color-mobs1");
    CHECK(window.Click("mobs-line-style-color-mobs1-o-0"));
    window.Click("mobs-line-style-color-mobs1-swatch");
    CHECK(window.Shows("mobs-line-style-color-mobs1-swatch-p-hex"));
    window.Click("mobs-line-style-color-mobs1-swatch");
    CHECK(!window.Shows("mobs-line-style-color-mobs1-swatch-p-hex"));
    CHECK_EQ(window.Said(), "");
}

TEST(a_line_box_keeps_what_is_typed_once_it_is_left)
{
    Window window;
    window.Page("players");
    const std::string box = "players-nameplate-design-design-players-2";
    const std::string was = window.settings.Kind(PlateKind::Players).lines[2].text;
    window.Click(box);
    window.Key(Rml::Input::KI_END);
    window.Type(" hi");
    // Nothing is saved while the box is typed in.
    CHECK_EQ(window.settings.Kind(PlateKind::Players).lines[2].text, was);
    CHECK(window.Click("page-title"));
    CHECK_EQ(window.settings.Kind(PlateKind::Players).lines[2].text, was + " hi");
}

TEST(a_tag_chip_goes_in_at_the_caret_by_click_and_at_the_end_by_drag)
{
    Window window;
    window.Page("players");
    const std::string box = "players-nameplate-design-design-players-1";
    std::string& text     = window.settings.Kind(PlateKind::Players).lines[1].text;
    text                  = "ab";
    window.Frames(2);
    window.Click(box);
    window.Key(Rml::Input::KI_HOME);
    window.Key(Rml::Input::KI_RIGHT);
    CHECK(window.Click("tag-1"));
    CHECK_EQ(text, std::string("a") + TagName(Tag::Level) + "b");
    CHECK(window.Drag("tag-0", box));
    CHECK_EQ(text, std::string("a") + TagName(Tag::Level) + "b" + TagName(Tag::Name));
}

TEST(a_friend_is_added_with_enter_and_removed_by_a_click_on_the_name)
{
    Window window;
    window.Page("players");
    window.Click("players-friends-add-friend");
    window.Type("Tanyrus");
    CHECK(window.Key(Rml::Input::KI_RETURN));
    CHECK_EQ(window.settings.friends.size(), 1u);
    CHECK_EQ(window.settings.friends[0], "Tanyrus");
    CHECK(window.Click("players-friends-friend-0"));
    CHECK(window.settings.friends.empty());
}

TEST(the_font_arrows_step_through_the_fonts_and_a_slider_follows_the_pointer)
{
    Window window;
    window.Page("players");
    CHECK(window.settings.Kind(PlateKind::Players).lines[0].font.empty());
    CHECK(window.Click("players-line-style-font-players0-next"));
    CHECK_EQ(window.settings.Kind(PlateKind::Players).lines[0].font, "Arial");
    CHECK(window.Click("players-line-style-font-players0-previous"));
    CHECK(window.settings.Kind(PlateKind::Players).lines[0].font.empty());

    window.Page("entities");
    const int was = window.settings.nameRaise;
    CHECK(window.Drag("entities-nameplate-style-vertical-offset", Rml::Vector2f(60.0f, 0.0f)));
    CHECK(window.settings.nameRaise > was);
    CHECK_EQ(window.TextOf("entities-nameplate-style-vertical-offset-val"), std::to_string(window.settings.nameRaise) + " px");
}

TEST(a_card_folds_on_its_title_and_a_switch_takes_a_click)
{
    Window window;
    CHECK(window.Shows("entities-nameplate-style-stack-names"));
    CHECK(window.Click("entities-nameplate-style-stack-names"));
    CHECK(window.settings.stackNames);
    window.Click("entities-nameplate-style-head");
    CHECK(!window.Shows("entities-nameplate-style-stack-names"));
    window.Click("entities-nameplate-style-head");
    CHECK(window.Shows("entities-nameplate-style-stack-names"));
}

TEST(a_color_is_typed_into_the_pickers_hex_box)
{
    Window window;
    window.Page("players");
    ColorPick& pick = window.settings.Kind(PlateKind::Players).lines[0].color;
    pick            = ColorPick{ColorSource::Custom, ColorFromRgb(0xFFFFFF)};
    window.Frames(2);
    window.Click("players-line-style-color-players0-swatch");
    const std::string hex = "players-line-style-color-players0-swatch-p-hex";
    window.Click(hex);
    window.Key(Rml::Input::KI_END);
    window.Key(Rml::Input::KI_BACK, 6);
    window.Type("3366CC");
    CHECK(window.Key(Rml::Input::KI_RETURN));
    CHECK(pick.custom == ColorFromRgb(0x3366CC));
}

TEST(the_window_is_kept_where_it_is_dragged_by_its_title)
{
    Window window;
    CHECK_EQ(window.settings.menuWidth, 0);
    const Rml::Vector2f was = window.Center("title");
    CHECK(window.Drag("head", Rml::Vector2f(-120.0f, 40.0f)));
    const Rml::Vector2f now = window.Center("title");
    CHECK_EQ(now.x - was.x, -120.0f);
    CHECK_EQ(now.y - was.y, 40.0f);
    CHECK(window.settings.menuWidth > 0 && window.settings.menuHeight > 0);
    CHECK_EQ(window.settings.menuX, static_cast<int>((test::RmlSession::kWidth - window.settings.menuWidth) / 2) - 120);
}

TEST(a_narrow_window_cuts_a_rows_label_and_keeps_its_controls_beside_it)
{
    auto top    = [](Rml::Element* element) { return element->GetAbsoluteOffset(Rml::BoxArea::Border).y; };
    auto right  = [](Rml::Element* element) {
        return element->GetAbsoluteOffset(Rml::BoxArea::Border).x + element->GetBox().GetSize(Rml::BoxArea::Border).x;
    };
    auto height = [](Rml::Element* element) { return element->GetBox().GetSize(Rml::BoxArea::Border).y; };
    Window window;
    // Asked narrower than it goes, so it opens at its least width.
    window.OpenAt(0, 0, 100, 900);
    for (const char* page : {"mobs", "players"})
    {
        window.Page(page);
        const std::vector<Rml::Element*> rows = window.OfClass("row");
        CHECK(rows.size() > 4u);
        for (Rml::Element* row : rows)
        {
            Rml::Element* label = row->GetChild(0);
            Rml::Element* controls = row->GetChild(1);
            CHECK(top(controls) < top(label) + height(label)); // beside the label, not under it
            CHECK(height(label) < 1.5f * height(controls));    // on one line, cut where it ends
            CHECK(right(controls) <= right(row) + 0.5f);
        }
    }
}

TEST(a_tip_stays_on_the_screen_and_off_the_pointer)
{
    Window window;
    // The window in the screen's lower right corner, where a tip under the pointer and to its right would leave it.
    window.Drag("head", Rml::Vector2f(470.0f, 160.0f));
    window.PointAtTheLowestTip();
    Rml::Element* tip = window.Find("tip");
    CHECK(tip != nullptr && tip->IsVisible(true));
    const Rml::Vector2f at   = tip->GetAbsoluteOffset(Rml::BoxArea::Border);
    const Rml::Vector2f size = tip->GetBox().GetSize(Rml::BoxArea::Border);
    CHECK(size.x > 0.0f && size.y > 0.0f);
    CHECK(at.x >= 0.0f && at.x + size.x <= test::RmlSession::kWidth);
    CHECK(at.y >= 0.0f && at.y + size.y <= test::RmlSession::kHeight);
    const bool covers = window.pointer.x >= at.x && window.pointer.x < at.x + size.x && window.pointer.y >= at.y &&
                        window.pointer.y < at.y + size.y;
    CHECK(!covers);
}

TEST(every_control_of_every_page_takes_a_click_and_the_window_stands)
{
    for (const MenuTheme theme : {MenuTheme::Phoenix, MenuTheme::Aurora})
    {
        Window window(theme);
        for (const PageInfo& page : kPages)
        {
            window.Page(PageKey(page.page).c_str());
            const std::vector<std::string> ids = window.Clickable();
            CHECK(!ids.empty());
            for (const std::string& id : ids)
            {
                // A click may fold a card or change a tab, and take later controls away with it.
                if (!window.Shows(id)) continue;
                window.Click(id);
                // What opened a list or a picker closes it again.
                if (window.Find("popup") != nullptr && window.Find("popup")->IsVisible(true) && window.Shows(id)) window.Click(id);
                CHECK(window.menu.open);
                window.Page(PageKey(page.page).c_str());
            }
            CHECK_EQ(window.Said(), "");
        }
    }
}

TEST(a_list_drops_in_in_both_themes)
{
    for (const MenuTheme theme : {MenuTheme::Phoenix, MenuTheme::Aurora})
    {
        Window window(theme);
        window.Page("mobs");
        window.Click("mobs-line-style-color-mobs0");
        // A tenth of a second after the click it is still fading in, and a second later it is whole.
        const float early = window.Find("popup")->GetComputedValues().opacity();
        CHECK(early > 0.0f && early < 1.0f);
        window.Frames(30);
        CHECK_EQ(window.Find("popup")->GetComputedValues().opacity(), 1.0f);
    }
}

TEST(a_checkboxs_mark_grows_from_its_middle_and_auroras_knob_slides_across)
{
    const std::string box = "entities-nameplate-style-stack-names";
    auto near  = [](float value, float wanted) { return value > wanted - 0.5f && value < wanted + 0.5f; };
    auto size  = [](Rml::Element* element) { return element->GetBox().GetSize(Rml::BoxArea::Border); };
    auto left  = [](Rml::Element* mark) {
        return mark->GetAbsoluteOffset(Rml::BoxArea::Border).x - mark->GetParentNode()->GetAbsoluteOffset(Rml::BoxArea::Border).x;
    };
    {
        // Phoenix: no mark while off; on, it is half the box wide, a quarter in from its left.
        Window window(MenuTheme::Phoenix);
        const float side = size(window.Find(box)).x;
        CHECK_EQ(size(window.Find(box)->GetChild(0)).x, 0.0f);
        CHECK(window.Click(box));
        // On its way it grows a little past its size and comes back.
        float most = 0.0f;
        for (int frame = 0; frame < 8; ++frame)
        {
            most = std::max(most, size(window.Find(box)->GetChild(0)).x);
            window.Frames();
        }
        CHECK(most > side / 2.0f + 0.5f);
        Rml::Element* mark = window.Find(box)->GetChild(0);
        CHECK(near(size(mark).x, side / 2.0f) && near(size(mark).y, side / 2.0f) && near(left(mark), side / 4.0f));
    }
    // Aurora: a track 2.6 em long and a knob of 1 em, 0.15 em from the track's left while off and 1.45 em while on.
    Window window(MenuTheme::Aurora);
    const float em = size(window.Find(box)).x / 2.6f;
    CHECK(near(size(window.Find(box)->GetChild(0)).x, em) && near(left(window.Find(box)->GetChild(0)), 0.15f * em));
    CHECK(window.Click(box));
    const float early = left(window.Find(box)->GetChild(0));
    CHECK(early > 0.15f * em + 0.5f && early < 1.45f * em - 0.5f);
    window.Frames(30);
    Rml::Element* knob = window.Find(box)->GetChild(0);
    CHECK(near(size(knob).x, em) && near(size(knob).y, em) && near(left(knob), 1.45f * em));
}
