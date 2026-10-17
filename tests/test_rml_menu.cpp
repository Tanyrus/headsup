#include "rml_menu.h"

#include "fonts.h"
#include "game_font.h"
#include "layout_edit.h"
#include "menu_state.h"
#include "plate_template.h"
#include "rml_session.h"
#include "test.h"

#include <RmlUi/Core.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

using namespace headsup;

namespace
{
    constexpr float kEm       = static_cast<float>(kDefaultMenuFontSize); // the window's text, in pixels
    constexpr double kVersion = 0.65;
    constexpr int kLeftButton = 0;
    constexpr double kFrameSeconds = 1.0 / 30.0; // the game's frame
    constexpr const char* kLongFont = "Franklin Gothic Demi Condensed"; // a family too long for its dropdown

    // A bundled font's capital height in pixels at a size: its OS/2 table's sCapHeight over the head's units per em.
    float CapHeight(const char* family, float pixels)
    {
        size_t count             = 0;
        const BundledFont* fonts = BundledFonts(count);
        const BundledFont* font  = std::find_if(fonts, fonts + count, [&](const BundledFont& f) { return std::string(f.family) == family; });
        CHECK(font != fonts + count);
        const unsigned char* data = font->data;
        const auto u16 = [&](size_t at) { return static_cast<unsigned>(data[at] << 8 | data[at + 1]); };
        const auto table = [&](const char* tag) {
            for (unsigned i = 0; i < u16(4); ++i)
                if (std::equal(tag, tag + 4, data + 12 + 16 * i)) return static_cast<size_t>(u16(12 + 16 * i + 8)) << 16 | u16(12 + 16 * i + 10);
            return size_t{0};
        };
        const size_t head = table("head"), os2 = table("OS/2");
        CHECK(head != 0 && os2 != 0 && u16(os2) >= 2);
        return static_cast<float>(static_cast<int16_t>(u16(os2 + 88))) * pixels / static_cast<float>(u16(head + 18));
    }
    // How far below the middle of a box a line of capitals stands: RmlUi puts a line's baseline half the leading and
    // the font's ascent under the line's top.
    float CapsBelowMiddle(Rml::Element* text, Rml::Element* box, float capHeight)
    {
        const Rml::FontMetrics& m = Rml::GetFontEngineInterface()->GetFontMetrics(text->GetFontFaceHandle());
        const float baseline = text->GetAbsoluteOffset(Rml::BoxArea::Content).y + (text->GetLineHeight() - m.ascent - m.descent) / 2.0f + m.ascent;
        const float middle = box->GetAbsoluteOffset(Rml::BoxArea::Border).y + box->GetBox().GetSize(Rml::BoxArea::Border).y / 2.0f;
        return baseline - capHeight / 2.0f - middle;
    }

    // The settings window on a screen of its own, worked as a player works it: by where its controls are.
    class Window
    {
    public:
        Settings settings;
        RmlMenu menu;
        std::vector<uint8_t> held; // the keys that are down, by their virtual-key codes

        explicit Window(MenuTheme theme = MenuTheme::Phoenix, const char* font = kBarlowCondensed)
        {
            settings.menuTheme = theme;
            m_Host.context     = m_Rml.context;
            m_Host.textFont    = font;
            m_Host.menuFonts = {kBarlowCondensed};
            m_Host.keyDown   = [this](uint8_t key) { return std::ranges::find(held, key) != held.end(); };
            menu.open        = true;
            Frames(2);
        }
        ~Window() { menu.Close(); }
        // What RmlUi complained of since the window opened.
        std::string Said() const { return m_Rml.Said(); }
        const test::NoRender& Render() const { return m_Rml.Render(); }

        // True when a frame of these changed a setting.
        bool Frames(int count = 1)
        {
            bool changed = false;
            for (int i = 0; i < count; ++i)
            {
                m_Rml.complaints.seconds += kFrameSeconds;
                changed = menu.Update(m_Host, settings, m_Readings, m_Fonts, kVersion) || changed;
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

        // Puts the pointer on a control's middle and lets the control ease into its look under it.
        void PointAt(const std::string& id)
        {
            const Rml::Vector2f at = Center(id);
            m_Host.context->ProcessMouseMove(static_cast<int>(at.x), static_cast<int>(at.y), 0);
            Frames(kEaseFrames);
        }

        void PointerTo(Rml::Vector2f at)
        {
            m_Host.context->ProcessMouseMove(static_cast<int>(at.x), static_cast<int>(at.y), 0);
            Frames(2);
        }
        bool ClickAt(Rml::Vector2f at)
        {
            m_Host.context->ProcessMouseMove(static_cast<int>(at.x), static_cast<int>(at.y), 0);
            bool changed = Frames();
            m_Host.context->ProcessMouseButtonDown(kLeftButton, 0);
            changed = Frames() || changed;
            m_Host.context->ProcessMouseButtonUp(kLeftButton, 0);
            return Frames(3) || changed;
        }
        // A press and its release that both reach one frame, as a tap does, or any click through a long frame.
        bool Tap(const std::string& id)
        {
            Center(id);
            Frames();
            const Rml::Vector2f at = Center(id);
            m_Host.context->ProcessMouseMove(static_cast<int>(at.x), static_cast<int>(at.y), 0);
            m_Host.context->ProcessMouseButtonDown(kLeftButton, 0);
            m_Host.context->ProcessMouseButtonUp(kLeftButton, 0);
            return Frames(3);
        }
        // What a text box holds.
        std::string Typed(const std::string& id)
        {
            auto* input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(Find(id));
            if (input == nullptr) throw test::Failure{"the window has no text box " + id};
            return input->GetValue();
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

        // The window of a developer's build, which has its layout's Reload and Export buttons.
        void Developer()
        {
            m_Host.developer      = true;
            m_Readings.exportable = true;
            Frames(2);
        }
        // The window takes this layout from its next frame on, as after a reload of the layout's file.
        void Laid(const WindowLayout& layout)
        {
            m_Layout      = layout;
            m_Host.layout = &m_Layout;
        }

    private:
        static constexpr int kDragSteps    = 6;
        static constexpr int kEaseFrames   = 8; // longer than a control takes between two looks
        static constexpr float kFromTheEnd = 6.0f; // pixels
        test::RmlSession m_Rml; // before the menu: its document is closed while RmlUi still runs
        MenuHost m_Host;
        WindowLayout m_Layout;
        MenuReadings m_Readings;
        std::vector<std::string> m_Fonts = {"Arial", kBarlowCondensed, kMarcellusSC, kLongFont};
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

TEST(text_in_a_box_stands_in_its_middle_by_its_fonts_capitals)
{
    struct Size
    {
        const char* font;
        int pixels;
    };
    for (const Size size : {Size{kBarlowCondensed, 18}, Size{kBarlowCondensed, 24}, Size{kCinzel, 18}})
    {
        Window window(MenuTheme::Phoenix, size.font);
        window.settings.menuFontSize = size.pixels;
        window.Frames(2);
        const float cap = CapHeight(size.font, static_cast<float>(size.pixels));
        const std::pair<const char*, const char*> boxed[] = {
            {"entities-nameplates-vertical-offset-val", "entities-nameplates-vertical-offset-box"},
            {"entities-nameplates-default-font-val", "entities-nameplates-default-font"},
            {"entities-nameplates-stack-names-state", "entities-nameplates-stack-names"},
            {"entities-nameplates-stack-names-label", "entities-nameplates-stack-names-label-row"}};
        for (const auto& [text, box] : boxed)
            CHECK(std::abs(CapsBelowMiddle(window.Find(text), window.Find(box), cap)) <= 0.5f);
        window.Page("mobs");
        Rml::Element* chip = window.Find("mobs-nameplate-design-design-mobs-2-c-0");
        CHECK(std::abs(CapsBelowMiddle(chip, chip, cap)) <= 0.5f);
    }
}

TEST(a_pages_name_stays_on_one_line_under_the_pointer_and_its_highlight_goes_on_past_it)
{
    auto left  = [](Rml::Element* element) { return element->GetAbsoluteOffset(Rml::BoxArea::Border).x; };
    auto right = [&](Rml::Element* element) { return left(element) + element->GetBox().GetSize(Rml::BoxArea::Border).x; };
    // The narrowest and the widest of the fonts the window can be set in.
    for (const char* font : {kBarlowCondensed, kCinzel})
    {
        Window window(MenuTheme::Phoenix, font);
        const float page = left(window.Find("main"));
        for (const LayoutPage& info : kWindowLayout.pages)
        {
            const std::string id = std::string("nav-") + info.key;
            Rml::Element* entry  = window.Find(id);
            window.PointAt(id);
            const float height = entry->GetBox().GetSize(Rml::BoxArea::Border).y;
            CHECK(entry->GetScrollHeight() < 1.5f * entry->GetLineHeight()); // one line
            const float nameEnd = entry->GetAbsoluteOffset(Rml::BoxArea::Content).x +
                                  static_cast<float>(Rml::ElementUtilities::GetStringWidth(entry, Rml::String(info.name)));
            CHECK(right(entry) - nameEnd >= height / 4.0f);
            CHECK(right(entry) <= page);
            // The page beside the list stays where it is while the entry moves under the pointer.
            CHECK_EQ(left(window.Find("main")), page);
        }
    }
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
    CHECK(!window.Shows("mobs-line-style-text-color-mobs1-o-0"));
    window.Click("mobs-line-style-text-color-mobs1");
    CHECK(window.Shows("mobs-line-style-text-color-mobs1-o-0"));
    CHECK(window.Click("mobs-line-style-text-color-mobs1-o-0"));
    CHECK(window.settings.Kind(PlateKind::Mobs).lines[1].color.source == ColorSource::Custom);
    CHECK(!window.Shows("mobs-line-style-text-color-mobs1-o-0"));
}

TEST(badge_numbers_is_a_dropdown_whose_closed_control_holds_each_choice_on_one_line)
{
    for (const char* font : {kBarlowCondensed, kCinzel})
    {
        Window window(MenuTheme::Phoenix, font);
        window.Page("mobs");
        std::string control;
        for (Rml::Element* element : window.OfClass("dd"))
            if (element->GetId().find("badge-numbers") != std::string::npos) control = element->GetId();
        CHECK(!control.empty());
        if (control.empty()) continue;
        for (const RankDisplay each : {RankDisplay::Rank, RankDisplay::TotalChance, RankDisplay::IncreasedChance})
        {
            window.Click(control);
            CHECK(window.Click(control + "-o-" + std::to_string(static_cast<int>(each))));
            CHECK(window.settings.rankDisplay == each);
            // The closed control cuts off what is past its edge rather than wrapping it.
            Rml::Element* shown = window.Find(control + "-val");
            CHECK(shown != nullptr && !shown->GetInnerRML().empty());
            if (shown == nullptr) continue;
            const float text = static_cast<float>(Rml::ElementUtilities::GetStringWidth(shown, shown->GetInnerRML()));
            CHECK(text <= shown->GetBox().GetSize(Rml::BoxArea::Content).x);
        }
    }
}

TEST(a_list_of_checks_stays_open_while_its_rows_are_checked)
{
    Window window;
    window.Page("mobs");
    window.Click("mobs-line-style-line-style-t-1");
    window.Click("mobs-line-style-line-visibility-mobs1");
    CHECK(window.Click("mobs-line-style-line-visibility-mobs1-o-0"));
    CHECK(!window.settings.Kind(PlateKind::Mobs).lines[1].show.always);
    CHECK(window.Click("mobs-line-style-line-visibility-mobs1-o-1"));
    CHECK(window.Shows("mobs-line-style-line-visibility-mobs1-o-1"));
    window.Click("mobs-line-style-line-visibility-mobs1");
    CHECK(!window.Shows("mobs-line-style-line-visibility-mobs1-o-1"));
}

TEST(a_swatch_opens_the_color_picker_and_closes_it)
{
    Window window;
    window.Page("mobs");
    window.Click("mobs-line-style-line-style-t-1");
    window.Click("mobs-line-style-text-color-mobs1");
    window.Click("mobs-line-style-text-color-mobs1-o-0");
    window.Click("mobs-line-style-text-color-mobs1-swatch");
    CHECK(window.Shows("mobs-line-style-text-color-mobs1-swatch-p-hex"));
    window.Frames(60);
    window.Click("mobs-line-style-text-color-mobs1-swatch");
    CHECK(!window.Shows("mobs-line-style-text-color-mobs1-swatch-p-hex"));
}

TEST(a_color_looked_at_in_its_picker_is_the_color_it_was)
{
    Window window;
    const std::string swatch = "entities-colors-claim-status-claimed-yours-claim-colors";
    const Color before       = window.settings.palette.claim[static_cast<int>(ClaimState::Yours)];
    CHECK(!window.Click(swatch)); // opening it is no change
    CHECK(window.Shows(swatch + "-p-hex"));
    CHECK(!window.Frames(5));
    CHECK(window.settings.palette.claim[static_cast<int>(ClaimState::Yours)] == before);
    CHECK(!window.Shows(swatch.substr(0, swatch.size()) + "-reset")); // so no arrow to put it back
    window.Click(swatch);
    CHECK(window.settings.palette.claim[static_cast<int>(ClaimState::Yours)] == before);
}

TEST(a_click_on_the_pickers_own_ground_leaves_it_open)
{
    Window window;
    const std::string swatch = "entities-colors-claim-status-claimed-yours-claim-colors";
    window.Click(swatch);
    CHECK(window.Shows(swatch + "-p-hex"));
    // Its padding, beside the sliders: no control, and still the picker.
    Rml::Element* picker = window.Find("popup");
    window.ClickAt(picker->GetAbsoluteOffset(Rml::BoxArea::Border) + Rml::Vector2f(3.0f, 3.0f));
    CHECK(window.Shows(swatch + "-p-hex"));
    window.Click("page-title"); // anywhere else closes it
    CHECK(!window.Shows(swatch + "-p-hex"));
}

TEST(a_row_left_waiting_for_a_key_waits_no_longer_once_its_page_or_the_window_is_left)
{
    constexpr uint8_t kF12 = 0x7B, kLetterA = 0x41;
    Window window;
    window.Page("userinterface");
    const std::string add = "userinterface-screenshot-mode-key-add";
    window.Click(add);
    window.Page("mobs"); // a change of mind
    window.held = {kF12};
    window.Page("userinterface");
    CHECK(!window.Frames(2));
    CHECK_EQ(window.settings.screenshotKey, 0);
    window.held.clear();
    window.Click(add);
    window.menu.open = false;
    window.Frames(2);
    window.held      = {kF12};
    window.menu.open = true;
    CHECK(!window.Frames(3));
    CHECK_EQ(window.settings.screenshotKey, 0);
    // Waited for on its page, the key is taken; and one the row already holds is no change.
    window.held.clear();
    window.Page("mousecursor");
    const std::string keep = "mousecursor-mouse-pointer-keep-pointer-add";
    window.Click(keep);
    window.held = {kLetterA};
    CHECK(window.Frames(2));
    const std::vector<uint8_t> kept = window.settings.keepPointerKeys;
    CHECK(std::ranges::find(kept, kLetterA) != kept.end());
    window.held.clear();
    window.Frames(2);
    window.Click(keep);
    window.held = {kLetterA};
    CHECK(!window.Frames(2));
    CHECK(window.settings.keepPointerKeys == kept);
}

TEST(the_list_of_pages_scrolls_in_a_window_too_short_for_it)
{
    Window window;
    window.OpenAt(0, 0, 900, 100); // asked shorter than it goes, so it opens at its least height
    Rml::Element* body = window.Find("nav-configmenu")->GetOwnerDocument();
    const float bottom = body->GetAbsoluteOffset(Rml::BoxArea::Border).y + body->GetBox().GetSize(Rml::BoxArea::Border).y;
    // The last page's name is reached inside the window, and takes its click.
    window.Click("nav-configmenu");
    Rml::Element* last = window.Find("nav-configmenu");
    CHECK(last->GetAbsoluteOffset(Rml::BoxArea::Border).y + last->GetBox().GetSize(Rml::BoxArea::Border).y <= bottom);
    CHECK(window.Shows("configmenu-settings-window-head") || last->IsClassSet("picked"));
}

TEST(a_dash_mark_is_shown_as_its_shape_in_the_list_and_in_the_closed_control)
{
    Window window;
    const std::string list = "entities-nameplates-replace-dashes";
    const auto shape = [&](const std::string& id) {
        Rml::ElementList shapes;
        window.Find(id)->GetElementsByClassName(shapes, "dash");
        return shapes.size();
    };
    CHECK_EQ(shape(list), 1u); // the diamond a new install draws, on the control itself
    window.Click(list);
    CHECK_EQ(shape(list + "-o-0"), 0u); // no mark: a dash stays a dash
    for (int mark = 1; mark < kDashMarkCount; ++mark)
        CHECK_EQ(shape(list + "-o-" + std::to_string(mark)), 1u);
    window.Click(list + "-o-0");
    CHECK_EQ(shape(list), 0u);
}

TEST(rmlui_reads_every_page_of_both_themes_without_a_complaint)
{
    for (const MenuTheme theme : {MenuTheme::Phoenix, MenuTheme::Aurora})
    {
        Window window(theme);
        for (const LayoutPage& page : kWindowLayout.pages)
        {
            window.Page(page.key);
            CHECK_EQ(window.Said(), "");
        }
    }
}

TEST(auroras_window_works_as_phoenixs_does)
{
    Window window(MenuTheme::Aurora);
    window.Page("mobs");
    window.Click("mobs-line-style-line-style-t-1");
    window.Click("mobs-line-style-text-color-mobs1");
    CHECK(window.Click("mobs-line-style-text-color-mobs1-o-0"));
    window.Click("mobs-line-style-text-color-mobs1-swatch");
    CHECK(window.Shows("mobs-line-style-text-color-mobs1-swatch-p-hex"));
    window.Click("mobs-line-style-text-color-mobs1-swatch");
    CHECK(!window.Shows("mobs-line-style-text-color-mobs1-swatch-p-hex"));
    CHECK_EQ(window.Said(), "");
}

TEST(a_line_box_keeps_what_is_typed_once_it_is_left)
{
    Window window;
    window.Page("players");
    const std::string box = "players-nameplate-design-design-players-2";
    const std::string was = window.settings.Kind(PlateKind::Players).lines[2].text;
    // Its tags stand in it as chips, and the stretch after the last of them is typed in.
    const size_t tags = RunsOf(was).slots.size();
    CHECK(tags > 0 && window.Shows(box + "-c-0"));
    window.Click(box + "-t-" + std::to_string(tags));
    window.Type(" hi");
    // Nothing is saved while the box is typed in.
    CHECK_EQ(window.settings.Kind(PlateKind::Players).lines[2].text, was);
    CHECK(window.Click("page-title"));
    CHECK_EQ(window.settings.Kind(PlateKind::Players).lines[2].text, was + " hi");
}

TEST(a_tag_chip_goes_in_at_the_caret_by_click_and_after_what_it_is_dropped_on_by_drag)
{
    Window window;
    window.Page("players");
    const std::string box = "players-nameplate-design-design-players-1";
    std::string& text     = window.settings.Kind(PlateKind::Players).lines[1].text;
    text                  = "ab";
    window.Frames(2);
    window.Click(box + "-t-0");
    window.Key(Rml::Input::KI_HOME);
    window.Key(Rml::Input::KI_RIGHT);
    CHECK(window.Click("tag-1"));
    CHECK_EQ(text, "a${level}b"); // the name closed off from the letter after it
    CHECK(window.Shows(box + "-c-0") && window.Shows(box + "-t-1"));
    CHECK(window.Drag("tag-0", box + "-t-1"));
    CHECK_EQ(text, "a${level}b$name");
    // Dropped on a chip of the line, a tag shares that chip's slot.
    CHECK(window.Drag("tag-19", box + "-c-0"));
    CHECK_EQ(text, "a${level|job}b$name");
    // A chip of the line dragged onto another joins it.
    CHECK(window.Drag(box + "-c-1", box + "-c-0"));
    CHECK_EQ(text, "a${level|job|name}b");
    // Barely moved and let go where it stands, a tag stays: that was a click with an unsteady hand.
    CHECK(!window.Drag(box + "-c-0-1", Rml::Vector2f(2.0f, 0.0f)));
    CHECK_EQ(text, "a${level|job|name}b");
    // One tag of the chip dragged off every line goes alone, and one let go on another takes its place in the order.
    CHECK(window.Drag(box + "-c-0-1", "page-title"));
    CHECK_EQ(text, "a${level|name}b");
    CHECK(window.Drag(box + "-c-0-1", box + "-c-0-0"));
    CHECK_EQ(text, "a${name|level}b");
    CHECK(window.Drag(box + "-c-0-0", "page-title"));
    CHECK(window.Drag(box + "-c-0", "page-title")); // the one left is the chip itself
    CHECK_EQ(text, "ab");
}

TEST(backspace_and_delete_beside_a_tag_take_it_out_and_the_arrows_step_past_it)
{
    Window window;
    window.Page("players");
    const std::string box = "players-nameplate-design-design-players-1";
    std::string& text     = window.settings.Kind(PlateKind::Players).lines[1].text;
    text                  = "ab $level cd";
    window.Frames(2);
    // Backspace at the start of the stretch after the tag: the tag goes, and typing goes on where it stood.
    window.Click(box + "-t-1");
    window.Key(Rml::Input::KI_HOME);
    CHECK(window.Key(Rml::Input::KI_BACK));
    CHECK_EQ(text, "ab  cd");
    CHECK(!window.Shows(box + "-c-0"));
    window.Type("X");
    window.Click("page-title");
    CHECK_EQ(text, "ab X cd");
    // Delete at the end of the stretch before it does the same from the other side.
    text = "ab $level cd";
    window.Frames(2);
    window.Click(box + "-t-0");
    window.Key(Rml::Input::KI_END);
    CHECK(window.Key(Rml::Input::KI_DELETE));
    CHECK_EQ(text, "ab  cd");
    window.Click("page-title");
    // Inside a stretch they are the stretch's own keys.
    text = "ab $level cd";
    window.Frames(2);
    window.Click(box + "-t-1");
    window.Key(Rml::Input::KI_END);
    window.Key(Rml::Input::KI_BACK);
    window.Click("page-title");
    CHECK_EQ(text, "ab $level c");
    // Right at a stretch's end goes on into the next one, and Left at its start back into the one before.
    window.Click(box + "-t-0");
    window.Key(Rml::Input::KI_END);
    window.Key(Rml::Input::KI_RIGHT);
    window.Type("Y");
    window.Key(Rml::Input::KI_LEFT);
    window.Key(Rml::Input::KI_LEFT);
    window.Type("Z");
    window.Click("page-title");
    CHECK_EQ(text, "ab Z$levelY c");
}

TEST(what_is_typed_is_kept_when_the_click_that_ends_it_is_over_within_one_frame)
{
    Window window;
    window.Page("players");
    const std::string box = "players-nameplate-design-design-players-1";
    std::string& text     = window.settings.Kind(PlateKind::Players).lines[1].text;
    text                  = "ab";
    window.Frames(2);
    const auto type = [&](const char* letter) {
        window.Click(box + "-t-0");
        window.Key(Rml::Input::KI_END);
        window.Type(letter);
    };
    // On another page's name: the page opens, and the line it left has what was typed.
    type("c");
    CHECK(window.Tap("nav-mobs"));
    CHECK_EQ(text, "abc");
    CHECK(window.Shows("mobs-line-style-head"));
    // On its own card's title, which folds the card.
    window.Page("players");
    type("d");
    CHECK(window.Tap("players-nameplate-design-head"));
    CHECK_EQ(text, "abcd");
    window.Tap("players-nameplate-design-head");
    // On the close button.
    type("e");
    CHECK(window.Tap("close"));
    CHECK_EQ(text, "abcde");
    CHECK(!window.menu.open);
}

TEST(a_window_closed_while_a_box_has_the_keyboard_keeps_what_was_typed_and_takes_no_more)
{
    Window window;
    window.Page("players");
    const std::string box = "players-nameplate-design-design-players-1";
    std::string& text     = window.settings.Kind(PlateKind::Players).lines[1].text;
    text                  = "ab";
    window.Frames(2);
    window.Click(box + "-t-0");
    window.Key(Rml::Input::KI_END);
    window.Type("c");
    window.menu.open = false; // as a command closes it, with no click to take the keyboard from the box
    CHECK(window.Frames(2));  // saved as it closes
    CHECK_EQ(text, "abc");
    window.Type("wasd"); // typed at the game, the window gone
    window.menu.open = true;
    window.Frames(3);
    window.Click("page-title");
    CHECK_EQ(text, "abc");
}

TEST(a_box_typed_in_keeps_the_keyboard_when_what_it_filters_is_listed_anew)
{
    Window window;
    window.Page("userinterface");
    const std::string find = "userinterface-game-windows-find-window";
    window.Click(find);
    for (const char* letter : {"e", "q", "u"})
        window.Type(letter); // a frame after each: the list under the box changes with every letter
    CHECK_EQ(window.Typed(find), "equ");
}

TEST(a_tag_clicked_goes_to_the_line_that_last_had_the_keyboard)
{
    Window window;
    window.Page("players");
    KindSettings& players = window.settings.Kind(PlateKind::Players);
    players.lines[0].text = "top";
    players.lines[1].text = "ab";
    window.Frames(2);
    const std::string top = "players-nameplate-design-design-players-0", middle = "players-nameplate-design-design-players-1";
    // Typing that makes a tag, and then Backspace at the stretch's start: the middle line is built anew.
    window.Click(middle + "-t-0");
    window.Key(Rml::Input::KI_END);
    window.Type(" $job");
    window.Key(Rml::Input::KI_HOME);
    window.Key(Rml::Input::KI_BACK);
    // Then the top line is typed in, and a tag clicked: it is the top line's.
    window.Click(top + "-t-0");
    window.Click("tag-1");
    CHECK(players.lines[0].text.find("level") != std::string::npos);
    CHECK(players.lines[1].text.find("level") == std::string::npos);
}

TEST(escape_gives_a_box_back_what_it_held_and_a_click_beside_a_lines_stretches_types_at_its_end)
{
    Window window;
    window.Page("players");
    const std::string box = "players-nameplate-design-design-players-1";
    std::string& text     = window.settings.Kind(PlateKind::Players).lines[1].text;
    text                  = "ab $level";
    window.Frames(2);
    window.Click(box + "-t-0");
    window.Type("zz");
    CHECK(!window.Key(Rml::Input::KI_ESCAPE)); // nothing changed
    CHECK_EQ(text, "ab $level");
    CHECK_EQ(window.Typed(box + "-t-0"), "ab ");
    // The box's own ground, past its last chip: the keyboard goes to the end of the line.
    Rml::Element* line = window.Find(box);
    const Rml::Vector2f corner = line->GetAbsoluteOffset(Rml::BoxArea::Border) + line->GetBox().GetSize(Rml::BoxArea::Border);
    window.ClickAt(corner - Rml::Vector2f(3.0f, 3.0f));
    window.Type("!");
    window.Click("page-title");
    CHECK_EQ(text, "ab $level!");
}

TEST(every_switch_of_every_page_flips_a_setting_of_its_own_and_flips_it_back)
{
    // Not settings: what the window itself is showing.
    const std::vector<std::string> windowsOwn{"userinterface-game-windows-all-windows"};
    for (const LayoutPage& info : kWindowLayout.pages)
    {
        Window window;
        window.Page(info.key);
        const Settings fresh = window.settings;
        std::vector<std::string> switches;
        for (Rml::Element* element : window.OfClass("check"))
            if (!element->GetId().empty()) switches.push_back(element->GetId());
        std::vector<std::pair<std::string, Settings>> flipped;
        for (const std::string& id : switches)
        {
            if (std::ranges::find(windowsOwn, id) != windowsOwn.end()) continue;
            window.settings = fresh;
            window.Frames(2);
            if (!window.Shows(id)) continue; // a setting of something that is off on a new install
            if (!window.Click(id) || window.settings == fresh) throw test::Failure{"the switch " + id + " changes no setting"};
            flipped.emplace_back(id, window.settings);
            if (!window.Shows(id)) continue; // it switched off what it stands in
            window.Click(id);
            if (window.settings != fresh) throw test::Failure{"the switch " + id + " does not flip back"};
        }
        for (size_t a = 0; a < flipped.size(); ++a)
            for (size_t b = a + 1; b < flipped.size(); ++b)
                if (flipped[a].second == flipped[b].second)
                    throw test::Failure{"the switches " + flipped[a].first + " and " + flipped[b].first + " set the same setting"};
    }
}

namespace
{
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
    std::map<std::string, std::string> Saved(const Settings& s)
    {
        SavedKeys store;
        SaveSettings(s, store);
        return store.values;
    }
    // The keys whose values differ between two saves, with the second's value.
    std::map<std::string, std::string> KeysChanged(const std::map<std::string, std::string>& before, const Settings& after)
    {
        std::map<std::string, std::string> changed;
        for (const auto& [key, value] : Saved(after))
            if (const auto at = before.find(key); at == before.end() || at->second != value) changed[key] = value;
        return changed;
    }
}

TEST(every_dropdown_slider_row_of_choices_and_number_of_every_page_and_tab_sets_a_setting_of_its_own)
{
    std::vector<std::pair<std::string, std::map<std::string, std::string>>> set;
    const auto seen = [&](const std::string& id) {
        return std::ranges::any_of(set, [&](const auto& done) { return done.first == id; });
    };
    for (const LayoutPage& info : kWindowLayout.pages)
    {
        Window window;
        window.Page(info.key);
        // Every switch on, so that what a feature switched off on a new install hides is there to work.
        for (int pass = 0; pass < 3; ++pass)
            for (Rml::Element* element : window.OfClass("check"))
                if (!element->GetId().empty() && !element->IsClassSet("on") && window.Shows(element->GetId()) &&
                    element->GetId() != "userinterface-game-windows-all-windows")
                    window.Click(element->GetId());
        const Settings fresh = window.settings;
        const std::map<std::string, std::string> saved = Saved(fresh);
        std::vector<std::string> tabs{""};
        for (Rml::Element* element : window.OfClass("tab"))
            tabs.push_back(element->GetId());
        const auto reset = [&](const std::string& tab) {
            window.settings = fresh;
            window.Frames(2);
            if (!tab.empty()) window.Click(tab);
            window.Frames(2);
        };
        for (const std::string& tab : tabs)
        {
            reset(tab);
            std::vector<std::string> dropdowns, sliders, rows, numbers;
            for (Rml::Element* element : window.OfClass("num"))
                if (!element->GetId().empty() && !seen(element->GetId())) numbers.push_back(element->GetId());
            for (Rml::Element* element : window.OfClass("segs"))
                if (!element->GetId().empty() && !seen(element->GetId())) rows.push_back(element->GetId());
            for (Rml::Element* element : window.OfClass("dd"))
                if (!element->GetId().empty() && !seen(element->GetId())) dropdowns.push_back(element->GetId());
            for (Rml::Element* element : window.OfClass("slider"))
            {
                const std::string id = element->GetId().substr(0, element->GetId().size() - std::string("-box").size());
                if (!seen(id)) sliders.push_back(id);
            }
            for (const std::string& id : dropdowns)
            {
                reset(tab);
                if (!window.Shows(id)) continue; // a setting of something that is off on a new install
                window.Click(id);
                std::string other;
                for (int option = 0; window.Find(id + "-o-" + std::to_string(option)) != nullptr && other.empty(); ++option)
                {
                    const std::string row = id + "-o-" + std::to_string(option);
                    if (!window.Find(row)->IsClassSet("picked")) other = row;
                }
                if (other.empty() || !window.Click(other) || window.settings == fresh)
                    throw test::Failure{"the dropdown " + id + " changes no setting"};
                set.emplace_back(id, KeysChanged(saved, window.settings));
            }
            for (const std::string& id : sliders)
            {
                reset(tab);
                if (!window.Shows(id)) continue;
                window.Drag(id, Rml::Vector2f(60.0f, 0.0f));
                if (window.settings == fresh)
                {
                    reset(tab);
                    window.Drag(id, Rml::Vector2f(-60.0f, 0.0f));
                }
                if (window.settings == fresh) throw test::Failure{"the slider " + id + " changes no setting"};
                set.emplace_back(id, KeysChanged(saved, window.settings));
            }
            for (const std::string& id : rows)
            {
                reset(tab);
                if (!window.Shows(id)) continue;
                std::string other;
                for (int option = 0; window.Find(id + "-s-" + std::to_string(option)) != nullptr && other.empty(); ++option)
                {
                    const std::string choice = id + "-s-" + std::to_string(option);
                    if (!window.Find(choice)->IsClassSet("picked")) other = choice;
                }
                if (other.empty() || !window.Click(other) || window.settings == fresh)
                    throw test::Failure{"the row of choices " + id + " changes no setting"};
                set.emplace_back(id, KeysChanged(saved, window.settings));
            }
            for (const std::string& id : numbers)
            {
                reset(tab);
                if (!window.Shows(id)) continue;
                window.Click(id);
                window.Key(Rml::Input::KI_END);
                window.Type("1");
                window.Click("page-title");
                if (window.settings == fresh) throw test::Failure{"the number " + id + " changes no setting"};
                set.emplace_back(id, KeysChanged(saved, window.settings));
            }
        }
    }
    // Keys that hold many settings in one value, each window's offsets, each view's shift or each section's widgets:
    // two controls of these are the same setting only when they write the same value.
    const std::vector<std::string> lists{"windowOffsets", "viewShiftsHelpOn", "viewShiftsHelpOff", "barLeft", "barCenter",
        "barRight"};
    const auto same = [&](const std::map<std::string, std::string>& a, const std::map<std::string, std::string>& b) {
        if (!std::ranges::equal(a, b, {}, &std::pair<const std::string, std::string>::first,
                &std::pair<const std::string, std::string>::first))
            return false;
        const bool listed = std::ranges::all_of(a, [&](const auto& entry) { return std::ranges::find(lists, entry.first) != lists.end(); });
        return !listed || a == b;
    };
    for (size_t a = 0; a < set.size(); ++a)
        for (size_t b = a + 1; b < set.size(); ++b)
            if (same(set[a].second, set[b].second))
                throw test::Failure{set[a].first + " and " + set[b].first + " set the same setting"};
}

namespace
{
    struct KeysSeen final : Rml::EventListener
    {
        int count = 0;
        void ProcessEvent(Rml::Event&) override { ++count; }
    };
}

TEST(the_arrow_that_steps_past_a_tag_is_not_also_a_key_of_the_stretch_it_leaves)
{
    Window window;
    window.Page("players");
    const std::string box = "players-nameplate-design-design-players-1";
    window.settings.Kind(PlateKind::Players).lines[1].text = "ab $level cd";
    window.Frames(2);
    window.Click(box + "-t-0");
    window.Key(Rml::Input::KI_END);
    KeysSeen seen;
    Rml::Element* stretch = window.Find(box + "-t-0");
    stretch->AddEventListener(Rml::EventId::Keydown, &seen);
    // Inside its text an arrow is the stretch's own key.
    window.Key(Rml::Input::KI_LEFT);
    window.Key(Rml::Input::KI_RIGHT);
    CHECK_EQ(seen.count, 2);
    // The one that steps on from its end never reaches it: it would show its caret for a key, with the keyboard gone.
    window.Key(Rml::Input::KI_RIGHT);
    CHECK_EQ(seen.count, 2);
    window.Key(Rml::Input::KI_LEFT); // and back, from the start of the stretch after the tag
    Rml::Element* after = window.Find(box + "-t-1");
    after->AddEventListener(Rml::EventId::Keydown, &seen);
    window.Key(Rml::Input::KI_RIGHT);
    window.Key(Rml::Input::KI_LEFT);
    CHECK_EQ(seen.count, 2);
    stretch->RemoveEventListener(Rml::EventId::Keydown, &seen);
    after->RemoveEventListener(Rml::EventId::Keydown, &seen);
}

TEST(a_stretch_is_as_wide_as_its_text_and_the_last_takes_the_rest_of_the_box)
{
    Window window;
    window.Page("players");
    const std::string box = "players-nameplate-design-design-players-1";
    window.settings.Kind(PlateKind::Players).lines[1].text = "ab $level";
    window.Frames(3);
    auto width = [&](const std::string& id) { return window.Find(id)->GetBox().GetSize(Rml::BoxArea::Border).x; };
    auto left  = [&](const std::string& id) { return window.Find(id)->GetAbsoluteOffset(Rml::BoxArea::Border).x; };
    // Three letters wide and no more, so the chip sits against them; the empty stretch after it runs to the box's end.
    CHECK(width(box + "-t-0") > kEm && width(box + "-t-0") < 3.0f * kEm);
    CHECK(left(box + "-c-0") - (left(box + "-t-0") + width(box + "-t-0")) < kEm);
    CHECK(width(box + "-t-1") > 5.0f * kEm);
    CHECK(left(box + "-t-1") + width(box + "-t-1") > left(box) + width(box) - 2.0f * kEm);
    // Typed in, it grows with what is typed before it is ever left.
    const float before = width(box + "-t-0");
    window.Click(box + "-t-0");
    window.Type("wwwwwwww");
    window.Frames(2);
    CHECK(width(box + "-t-0") > before + 4.0f * kEm);
    CHECK_EQ(window.Said(), "");
}

TEST(the_font_arrows_step_through_the_fonts_and_a_slider_follows_the_pointer)
{
    Window window;
    window.Page("players");
    CHECK(window.settings.Kind(PlateKind::Players).lines[0].font.empty());
    CHECK(window.Click("players-line-style-font-players0-next"));
    CHECK_EQ(window.settings.Kind(PlateKind::Players).lines[0].font, kGameFontName); // the list's first
    CHECK(window.Click("players-line-style-font-players0-previous"));
    CHECK(window.settings.Kind(PlateKind::Players).lines[0].font.empty());

    window.Page("entities");
    const int was = window.settings.nameRaise;
    CHECK(window.Drag("entities-nameplates-vertical-offset", Rml::Vector2f(60.0f, 0.0f)));
    CHECK(window.settings.nameRaise > was);
    CHECK_EQ(window.TextOf("entities-nameplates-vertical-offset-val"), std::to_string(window.settings.nameRaise) + " px");
}

TEST(a_percent_slider_lands_on_steps_of_five_while_it_is_dragged_and_stepped)
{
    Window window;
    window.Page("entities");
    const std::string slider = "entities-nameplates-min-size";
    const int was            = window.settings.scaleMin;
    for (const float by : {7.0f, 3.0f, 11.0f})
    {
        CHECK(window.Drag(slider, Rml::Vector2f(by, 0.0f)));
        CHECK_EQ(window.settings.scaleMin % 5, 0);
        CHECK_EQ(window.TextOf(slider + "-val"), std::to_string(window.settings.scaleMin) + "%");
    }
    CHECK(window.settings.scaleMin != was);
    const int dragged = window.settings.scaleMin;
    CHECK(window.Key(Rml::Input::KI_RIGHT));
    CHECK_EQ(window.settings.scaleMin, dragged + 5);
}

TEST(a_slider_takes_the_arrow_keys_only_while_the_pointer_is_on_the_window)
{
    Window window;
    window.Page("entities");
    const std::string slider = "entities-nameplates-vertical-offset";
    window.Drag(slider, Rml::Vector2f(60.0f, 0.0f));
    // Still on the window, an arrow nudges the slider last moved.
    const int dragged = window.settings.nameRaise;
    CHECK(window.Key(Rml::Input::KI_RIGHT));
    CHECK_EQ(window.settings.nameRaise, dragged + 1);
    // With the pointer off the window the arrows are the game's, for its camera: the slider is left alone.
    window.PointerTo(Rml::Vector2f(5.0f, 5.0f));
    CHECK(!window.Key(Rml::Input::KI_RIGHT, 3));
    CHECK_EQ(window.settings.nameRaise, dragged + 1);
    // And it does not take them up again by itself when the pointer comes back.
    window.PointerTo(window.Center("page-title"));
    CHECK(!window.Key(Rml::Input::KI_RIGHT));
    CHECK_EQ(window.settings.nameRaise, dragged + 1);
    // A text box keeps the keyboard wherever the pointer goes: it is typed in.
    window.Page("players");
    const std::string box = "players-nameplate-design-design-players-1";
    window.settings.Kind(PlateKind::Players).lines[1].text = "ab";
    window.Frames(2);
    window.Click(box + "-t-0");
    window.Key(Rml::Input::KI_END);
    window.PointerTo(Rml::Vector2f(5.0f, 5.0f));
    window.Type("c");
    window.Click("page-title");
    CHECK_EQ(window.settings.Kind(PlateKind::Players).lines[1].text, "abc");
}

TEST(a_card_folds_on_its_title_and_a_switch_takes_a_click)
{
    Window window;
    CHECK(window.Shows("entities-nameplates-stack-names"));
    CHECK(window.Click("entities-nameplates-stack-names"));
    CHECK(window.settings.stackNames);
    window.Click("entities-nameplates-head");
    CHECK(!window.Shows("entities-nameplates-stack-names"));
    window.Click("entities-nameplates-head");
    CHECK(window.Shows("entities-nameplates-stack-names"));
}

TEST(hide_during_conversation_is_a_switch_of_the_nameplates_card)
{
    Window window;
    CHECK(window.Click("entities-nameplates-hide-during-conversation"));
    CHECK(window.settings.hideInEvent);
    window.Click("entities-nameplates-on");
    CHECK(!window.settings.nameplates);
    CHECK(!window.Shows("entities-nameplates-hide-during-conversation"));
}

TEST(a_color_is_typed_into_the_pickers_hex_box)
{
    Window window;
    window.Page("players");
    ColorPick& pick = window.settings.Kind(PlateKind::Players).lines[0].color;
    pick            = ColorPick{ColorSource::Custom, ColorFromRgb(0xFFFFFF)};
    window.Frames(2);
    window.Click("players-line-style-text-color-players0-swatch");
    const std::string hex = "players-line-style-text-color-players0-swatch-p-hex";
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
    CHECK(std::abs(now.x - was.x + 120.0f) < 0.01f);
    CHECK(std::abs(now.y - was.y - 40.0f) < 0.01f);
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
            if (!row->IsVisible(true)) continue; // a setting of something switched off
            Rml::Element* label = row->GetChild(0);
            Rml::Element* controls = row->GetChild(1);
            CHECK(top(controls) < top(label) + height(label)); // beside the label, not under it
            CHECK(height(label) < 1.5f * height(controls));    // on one line, cut where it ends
            CHECK(right(controls) <= right(row) + 0.5f);
        }
    }
}

TEST(a_font_list_holds_its_longest_name_whole_and_says_each_name_under_the_pointer)
{
    auto width = [](Rml::Element* element) { return element->GetBox().GetSize(Rml::BoxArea::Border).x; };
    Window window;
    window.settings.defaultFont = kLongFont;
    window.Frames(2);
    const std::string control = "entities-nameplates-default-font";
    // The closed control cuts the name, and says it whole.
    CHECK_EQ(window.Find(control)->GetAttribute<Rml::String>("tip", ""), kLongFont);
    window.Click(control);
    window.Frames(10);
    Rml::Element* row = window.Find(control + "-o-5"); // after the game's two fonts, Arial, Barlow Condensed and Marcellus SC
    CHECK(row != nullptr && row->IsVisible(true));
    CHECK_EQ(row->GetAttribute<Rml::String>("tip", ""), kLongFont);
    CHECK_EQ(window.Find(control + "-o-2")->GetAttribute<Rml::String>("tip", ""), "Arial");
    // The list is wider than its control, as wide as the name needs.
    CHECK(width(window.Find("popup")) > width(window.Find(control)) + 20.0f);
    CHECK(row->GetScrollWidth() <= row->GetClientWidth() + 0.5f);
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
        for (const LayoutPage& page : kWindowLayout.pages)
        {
            window.Page(page.key);
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
                window.Page(page.key);
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
        window.Click("mobs-line-style-text-color-mobs0");
        // A tenth of a second after the click it is still fading in, and a second later it is whole.
        const float early = window.Find("popup")->GetComputedValues().opacity();
        CHECK(early > 0.0f && early < 1.0f);
        window.Frames(30);
        CHECK_EQ(window.Find("popup")->GetComputedValues().opacity(), 1.0f);
    }
}

TEST(a_toggle_is_a_sliders_width_and_its_wide_peg_slides_across_its_track_in_both_themes)
{
    const std::string box = "entities-nameplates-stack-names", slider = "entities-nameplates-vertical-offset";
    auto size = [](Rml::Element* element) { return element->GetBox().GetSize(Rml::BoxArea::Border); };
    auto left = [](Rml::Element* mark) {
        return mark->GetAbsoluteOffset(Rml::BoxArea::Border).x - mark->GetParentNode()->GetAbsoluteOffset(Rml::BoxArea::Border).x;
    };
    auto middle = [](Rml::Element* element) {
        return element->GetAbsoluteOffset(Rml::BoxArea::Border).x + element->GetBox().GetSize(Rml::BoxArea::Border).x / 2.0f;
    };
    for (const MenuTheme theme : {MenuTheme::Phoenix, MenuTheme::Aurora})
    {
        Window window(theme);
        Rml::Element* track = window.Find(box);
        const float width = size(track).x, height = size(track).y, peg = size(track->GetChild(0)).x;
        // As wide as the slider above it and right under it, with a peg wider than the track is tall.
        CHECK(std::abs(width - size(window.Find(slider)).x) < 0.5f);
        CHECK(std::abs(middle(track) - middle(window.Find(slider))) < 1.0f);
        CHECK(peg > height && peg < width / 2.0f);
        // Rounded as a dropdown is, not as a pill.
        const float radius = track->GetComputedValues().border_top_left_radius();
        CHECK(radius > 0.0f && radius < height / 2.0f);
        CHECK_EQ(radius, window.Find("entities-nameplates-default-font")->GetComputedValues().border_top_left_radius());
        const float inset = left(track->GetChild(0));
        CHECK(inset > 0.0f && inset < height / 2.0f);
        CHECK(window.Click(box));
        const float early = left(window.Find(box)->GetChild(0));
        CHECK(early > inset + 0.5f && early < width - peg - inset - 0.5f); // on its way
        window.Frames(30);
        const float end = left(window.Find(box)->GetChild(0));
        CHECK(end > width - peg - inset - 0.5f && end < width - peg - inset + 0.5f);
    }
}

TEST(a_cards_title_toggle_is_about_twice_as_wide_as_before_and_a_tables_stays_small_and_wordless)
{
    constexpr float kOldTitleToggle = 3.2f; // ems, before the tester's fourth round
    Window window;
    const float title = window.Find("entities-nameplates-on")->GetBox().GetSize(Rml::BoxArea::Border).x;
    CHECK(title >= 2.0f * kOldTitleToggle * kEm && title <= 2.5f * kOldTitleToggle * kEm);
    window.Page("mobs");
    Rml::Element* cell = window.Find("mobs-statuses-status-paralyze");
    CHECK(cell != nullptr && window.Find("mobs-statuses-status-paralyze-state") == nullptr);
    CHECK(cell->GetBox().GetSize(Rml::BoxArea::Border).x < title);
}

TEST(a_toggles_word_says_its_state_in_capitals_inside_its_track_clear_of_the_peg)
{
    auto edges = [](Rml::Element* element) {
        const float x = element->GetAbsoluteOffset(Rml::BoxArea::Border).x;
        return std::pair{x, x + element->GetBox().GetSize(Rml::BoxArea::Border).x};
    };
    auto argb = [](const Rml::Colourb& c) {
        return static_cast<uint32_t>(c.alpha) << 24 | static_cast<uint32_t>(c.red) << 16 | static_cast<uint32_t>(c.green) << 8 | c.blue;
    };
    struct Toggle
    {
        const char* page;
        const char* id;
        const char* off;
        const char* on;
    };
    const Toggle toggles[] = {{"entities", "entities-nameplates-stack-names", "OFF", "ON"},
        {"entities", "entities-nameplates-on", "OFF", "ON"}, {"mobs", "mobs-detection-icons-links", "HIDDEN", "SHOWN"}};
    for (const MenuTheme theme : {MenuTheme::Phoenix, MenuTheme::Aurora})
        for (const char* font : {kBarlowCondensed, kCinzel})
            for (const Toggle& toggle : toggles)
            {
                Window window(theme, font);
                window.Page(toggle.page);
                const std::string box = toggle.id;
                for (int flip = 0; flip < 2; ++flip)
                {
                    Rml::Element* track = window.Find(box);
                    Rml::Element* word  = window.Find(box + "-state");
                    CHECK(word != nullptr);
                    const bool on = track->IsClassSet("on");
                    std::string shown = word->GetInnerRML();
                    std::ranges::transform(shown, shown.begin(), [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
                    CHECK_EQ(shown, on ? toggle.on : toggle.off);
                    CHECK(word->GetComputedValues().text_transform() == Rml::Style::TextTransform::Uppercase);
                    CHECK_EQ(argb(word->GetComputedValues().color()), MenuColorsOf(theme).text);
                    // Its capitals, centered in the word's box, stand inside the track and clear of the peg.
                    const auto [trackLeft, trackRight] = edges(track);
                    const auto [boxLeft, boxRight]     = edges(word);
                    const float half = static_cast<float>(Rml::ElementUtilities::GetStringWidth(word, shown)) / 2.0f;
                    const float left = (boxLeft + boxRight) / 2.0f - half, right = (boxLeft + boxRight) / 2.0f + half;
                    const auto [pegLeft, pegRight] = edges(track->GetChild(0));
                    CHECK(left > trackLeft && right < trackRight);
                    CHECK(right < pegLeft || left > pegRight);
                    window.Click(box);
                    window.Frames(30);
                }
            }
}

TEST(the_debug_report_button_on_the_headsup_config_page_asks_for_one_report)
{
    Window window;
    window.Page("configmenu");
    window.Click("configmenu-debug-mode-on");
    CHECK(!window.menu.TakeDebugRequest());
    window.Click("configmenu-debug-mode-debug-report");
    CHECK(window.menu.TakeDebugRequest());
    CHECK(!window.menu.TakeDebugRequest()); // once
    CHECK_EQ(window.Said(), "");
}

TEST(the_reset_button_stands_under_the_cards_as_wide_as_one_taller_and_in_capitals)
{
    Window window;
    window.Page("configmenu");
    auto size = [&](const char* id) { return window.Find(id)->GetBox().GetSize(Rml::BoxArea::Border); };
    const char* button = "configmenu-reset-reset-all-settings";
    CHECK(!window.Find("configmenu-reset")->IsClassSet(kMenuCardClass));
    CHECK(std::abs(size(button).x - size("configmenu-settings-window").x) < 1.0f);
    CHECK(size(button).y > size("configmenu-settings-window-size-next").y);
    CHECK(window.Find(button)->GetComputedValues().text_transform() == Rml::Style::TextTransform::Uppercase);
    window.Click(button);
    CHECK(window.Shows("configmenu-reset-reset-everything"));
    CHECK_EQ(window.Said(), "");
}

TEST(a_developers_window_has_reload_left_of_its_close_button_and_export_in_the_debug_card_and_each_asks_once)
{
    constexpr const char* kExport = "configmenu-debug-mode-export";
    Window window;
    window.settings.debugMode = true;
    window.Page("configmenu");
    CHECK(window.Find(kMenuReloadId) == nullptr && window.Find(kExport) == nullptr);
    window.Developer();
    auto left  = [&](const char* id) { return window.Find(id)->GetAbsoluteOffset(Rml::BoxArea::Border).x; };
    auto right = [&](const char* id) { return left(id) + window.Find(id)->GetBox().GetSize(Rml::BoxArea::Border).x; };
    CHECK(window.Shows(kMenuReloadId) && window.Shows(kExport));
    CHECK(right(kMenuReloadId) <= left(kMenuCloseId) && left(kMenuCloseId) - right(kMenuReloadId) < kEm);
    CHECK(!window.menu.TakeReloadRequest() && !window.menu.TakeExportRequest());
    window.Click(kMenuReloadId);
    CHECK(window.menu.TakeReloadRequest() && !window.menu.TakeReloadRequest() && !window.menu.TakeExportRequest());
    window.Click(kExport);
    CHECK(window.menu.TakeExportRequest() && !window.menu.TakeExportRequest() && window.menu.open);
    CHECK_EQ(window.Said(), "");
}

TEST(a_layout_read_anew_shows_on_the_windows_next_frame)
{
    Window window;
    test::EditedLayout edited;
    edited.RenamePage("entities", "Everything");
    edited.MoveCard("target-cursor", "entities", 0);
    window.Laid(edited.Layout());
    window.Frames(2);
    CHECK_EQ(window.Find("nav-entities")->GetInnerRML(), "Everything");
    CHECK_EQ(window.Find("page")->GetChild(0)->GetId(), "entities-target-cursor");
    window.menu.Close(); // and from scratch, as a reload builds it
    window.Frames(2);
    CHECK_EQ(window.Find("page")->GetChild(0)->GetId(), "entities-target-cursor");
    CHECK_EQ(window.Said(), "");
}

TEST(a_font_size_arrow_stays_under_the_pointer_as_the_window_grows_and_shrinks)
{
    // The tester's report: the slider ran away from the pointer as the window grew under it. An arrow clicked twice
    // where it was steps twice.
    Window window;
    window.Page("configmenu");
    const Rml::Vector2f next = window.Center("configmenu-settings-window-size-next");
    CHECK(window.ClickAt(next));
    CHECK(window.ClickAt(next));
    CHECK_EQ(window.settings.menuFontSize, kDefaultMenuFontSize + 2);
    const Rml::Vector2f previous = window.Center("configmenu-settings-window-size-previous");
    CHECK(window.ClickAt(previous));
    CHECK(window.ClickAt(previous));
    CHECK_EQ(window.settings.menuFontSize, kDefaultMenuFontSize);
    CHECK_EQ(window.Said(), "");
}

TEST(the_windows_font_size_is_its_settings_pixels_whatever_its_font_and_its_arrows_step_it)
{
    Window window; // in Barlow Condensed, which was drawn larger than the size asked
    const auto pixels = [&] { return window.Find("page")->GetOwnerDocument()->GetComputedValues().font_size(); };
    CHECK_EQ(pixels(), 18.0f);
    window.settings.menuFontSize = 24;
    window.Frames(2);
    CHECK_EQ(pixels(), 24.0f);
    window.Page("configmenu");
    CHECK(window.Click("configmenu-settings-window-size-next"));
    CHECK_EQ(window.settings.menuFontSize, 25);
    window.Frames(2);
    CHECK_EQ(pixels(), 25.0f);
    CHECK(window.Click("configmenu-settings-window-size-previous"));
    CHECK_EQ(window.settings.menuFontSize, 24);
    CHECK_EQ(window.Said(), "");
}

TEST(the_window_takes_its_opacity_once_as_a_whole_and_never_more_than_its_setting)
{
    Window window;
    window.menu.open = false;
    window.Frames(2);
    window.settings.menuOpacity = 50;
    window.menu.open            = true;
    // Drawn whole into a layer and laid over the game at its opacity once: its own parts are drawn opaque, so an open
    // list hides the rows under it. Opening, the parts come in from nothing under that.
    Rml::ElementDocument* document = window.Find("page")->GetOwnerDocument();
    for (int frame = 0; frame < 12; ++frame)
    {
        const size_t before = window.Render().composited.size();
        window.Frames();
        // RmlUi starts an animation a frame after it is set: that first frame shows nothing rather than all of it.
        if (frame == 0) CHECK(!document->IsVisible() && window.Render().composited.size() == before);
        if (frame > 0) CHECK(window.Render().composited.size() > before && window.Render().composited.back() == 0.5f);
        if (frame == 1) CHECK(document->IsVisible() && document->GetComputedValues().opacity() < 0.5f);
    }
    CHECK_EQ(document->GetComputedValues().opacity(), 1.0f);
    // The slider on the HeadsUp Config page sets it, and at full opacity no layer is drawn.
    window.Page("configmenu");
    CHECK(window.Drag("configmenu-settings-window-window-opacity", Rml::Vector2f(400.0f, 0.0f)));
    CHECK_EQ(window.settings.menuOpacity, 100);
    window.Frames(2);
    const size_t opaque = window.Render().composited.size();
    window.Frames(2);
    CHECK_EQ(window.Render().composited.size(), opaque);
    CHECK_EQ(document->GetComputedValues().opacity(), 1.0f);
    CHECK_EQ(window.Said(), "");
}

TEST(a_setting_of_something_switched_off_is_not_shown)
{
    Window window;
    const std::string least = "entities-nameplates-min-size", cursor = "entities-target-cursor-cursor-height";
    CHECK(window.Shows(least) && window.Shows(cursor));
    // Its own switch stays, to turn it back on.
    CHECK(window.Click("entities-nameplates-scale-with-distance"));
    CHECK(!window.settings.scaleWithDistance && !window.Shows(least) && window.Shows("entities-nameplates-scale-with-distance"));
    CHECK(!window.Shows(least + "-label-row")); // its row with it, label and all
    window.Click("entities-target-cursor-cursor-style");
    CHECK(window.Click("entities-target-cursor-cursor-style-o-0")); // the game's own cursor
    CHECK(!window.Shows(cursor) && window.Shows("entities-target-cursor-cursor-style"));
    CHECK(window.Click("entities-nameplates-scale-with-distance"));
    CHECK(window.Shows(least));
    // A card of what is off is gone whole.
    CHECK(window.Shows("entities-colors-aggression-head"));
    window.settings.enabled = false;
    window.Frames(2);
    CHECK(!window.Shows("entities-colors-aggression-head") && !window.Shows(least));
}
