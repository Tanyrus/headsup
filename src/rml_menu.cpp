#include "rml_menu.h"

#include "bar_look.h"
#include "fonts.h"
#include "markup.h"
#include "menu_sizes.h"
#include "rml_pure.h"
#include "utf16.h"

#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Factory.h>
#include <RmlUi/Core/FontEngineInterface.h>
#include <RmlUi/Core/StyleSheetContainer.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <utility>

namespace headsup
{
    namespace
    {
        // The window's text font's lift at its size, or none while RmlUi has no such font.
        int LiftOf(const std::string& family, int pixels)
        {
            Rml::FontEngineInterface* fonts = Rml::GetFontEngineInterface();
            const Rml::FontFaceHandle face  = fonts->GetFontFaceHandle(Rml::StringUtilities::ToLower(family),
                Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Normal, pixels);
            if (face == 0) return 0;
            const Rml::FontMetrics& metrics = fonts->GetFontMetrics(face);
            return TextLift(metrics.ascent, metrics.descent, pixels);
        }

        constexpr const char* kDocumentName = "settings";
        constexpr float kPopupDrop  = 2.0f;  // pixels between a control and its popup
        constexpr float kTipDrop    = 18.0f; // pixels from the pointer to its tip
        constexpr char kKeySeparator = '\x1f';
        // A design line's box and the stretches of text in it, as menu_document styles them.
        constexpr float kCaretRoom         = 0.4f;     // ems past a stretch's last letter: its caret, and a place to click

        Rml::Element* WithId(Rml::Element* element)
        {
            while (element != nullptr && element->GetId().empty())
                element = element->GetParentNode();
            return element;
        }

        Rml::ElementFormControlInput* AsInput(Rml::Element* element)
        {
            return rmlui_dynamic_cast<Rml::ElementFormControlInput*>(element);
        }

        // Whether a drag ended on the control dragged: a click with an unsteady hand. RmlUi looks past that control
        // for what is under the pointer, so it would count as a drop on what the control stands in.
        bool LetGoOn(Rml::Element* dragged, const Rml::Event& event)
        {
            return dragged->IsPointWithinElement(
                Rml::Vector2f(event.GetParameter<float>("mouse_x", 0.0f), event.GetParameter<float>("mouse_y", 0.0f)));
        }

        bool IsText(Rml::ElementFormControlInput* input)
        {
            return input != nullptr && input->GetAttribute<Rml::String>("type", "text") == "text";
        }

        // A caret RmlUi counts in characters, as the byte it is before.
        size_t ByteOf(const std::string& text, int characters)
        {
            size_t at = 0;
            for (int n = 0; n < characters && at < text.size(); ++n)
                do
                    ++at;
                while (at < text.size() && ContinuesCharacter(text[at]));
            return at;
        }

        // A byte of a text as RmlUi counts its carets, in characters.
        int CharacterOf(const std::string& text, size_t byte)
        {
            int characters = 0;
            for (size_t at = 0; at < byte && at < text.size(); ++at)
                characters += ContinuesCharacter(text[at]) ? 0 : 1;
            return characters;
        }

        // The stretch before (-1) or after (1) this one in its box, past the tag between them; none at an end.
        Rml::ElementFormControlInput* RunBeside(Rml::Element* run, int step)
        {
            Rml::Element* at = run;
            for (int sibling = 0; sibling < 2 && at != nullptr; ++sibling)
                at = step < 0 ? at->GetPreviousSibling() : at->GetNextSibling();
            return at != nullptr && at->IsClassSet(kRunClass) ? AsInput(at) : nullptr;
        }

        void FocusAt(Rml::ElementFormControlInput* input, int character)
        {
            if (input == nullptr) return;
            input->Focus();
            input->SetSelectionRange(character, character);
        }

        std::string KeyOf(const FormValue& value)
        {
            return value.id + kKeySeparator + static_cast<char>('0' + static_cast<int>(value.kind)) + value.name;
        }

        void SetPx(Rml::Element* element, Rml::PropertyId property, float value)
        {
            element->SetProperty(property, Rml::Property(value, Rml::Unit::PX));
        }
    }

    void RmlMenu::Close()
    {
        if (m_Document != nullptr) m_Document->Close();
        m_Document = nullptr;
        m_Visible  = false;
        m_Placed   = false;
        m_OpenedAt.reset();
        m_Sidebar = m_Page = m_PageTitle = m_Popup = m_Tip = nullptr;
        m_SidebarRml.clear();
        m_PopupRml.clear();
        m_TitleShown.clear();
        m_TipShown.clear();
        m_Cards.clear();
        m_ById.clear();
        m_Applied.clear();
        m_Events.clear();
    }

    bool RmlMenu::TakeDebugRequest() { return std::exchange(m_State.debugRequested, false); }
    bool RmlMenu::TakeWatchRequest() { return std::exchange(m_State.watchRequested, false); }
    bool RmlMenu::TakeReloadRequest() { return std::exchange(m_Reload, false); }
    bool RmlMenu::TakeExportRequest() { return std::exchange(m_State.exportRequested, false); }
    bool RmlMenu::TakeCursorsRequest() { return std::exchange(m_State.cursorsRequested, false); }
    bool RmlMenu::TakeThemesRequest() { return std::exchange(m_State.themesRequested, false); }
    std::optional<std::string> RmlMenu::TakeThemeSave() { return std::exchange(m_State.themeSave, std::nullopt); }
    std::optional<std::string> RmlMenu::TakeThemeLoad() { return std::exchange(m_State.themeLoad, std::nullopt); }

    bool RmlMenu::OpenPage(const std::string& name)
    {
        const std::optional<Page> page = m_Layout->PageNamed(name);
        if (!page) return false;
        m_State.Leave();
        m_State.page = *page;
        open         = true;
        m_ScrollTo   = 0.0f;
        return true;
    }

    std::optional<Rml::Vector2f> RmlMenu::CenterOf(const std::string& id, bool scroll)
    {
        Rml::Element* element = m_Document != nullptr ? m_Document->GetElementById(id) : nullptr;
        if (element == nullptr) return std::nullopt;
        // What is in the popup or the frame is in view already; a card's control may be scrolled out of it, and in
        // a short window a page's name.
        for (Rml::Element* up = scroll ? element : nullptr; up != nullptr; up = up->GetParentNode())
            if (up == m_Page || up == m_Sidebar)
            {
                element->ScrollIntoView(Rml::ScrollIntoViewOptions(Rml::ScrollAlignment::Nearest));
                m_Document->GetContext()->Update();
                break;
            }
        return element->GetAbsoluteOffset(Rml::BoxArea::Border) + element->GetBox().GetSize(Rml::BoxArea::Border) / 2.0f;
    }

    Rml::Element* RmlMenu::Find(const std::string& id) const
    {
        const auto known = m_ById.find(id);
        return known != m_ById.end() ? known->second : nullptr;
    }

    void RmlMenu::Index()
    {
        m_ById.clear();
        std::vector<Rml::Element*> left{m_Document};
        while (!left.empty())
        {
            Rml::Element* element = left.back();
            left.pop_back();
            if (!element->GetId().empty()) m_ById.emplace(element->GetId(), element);
            for (int child = 0; child < element->GetNumChildren(); ++child)
                left.push_back(element->GetChild(child));
        }
    }

    bool RmlMenu::Document(const MenuHost& host, const Settings& settings)
    {
        const GameSkinLook* skin = host.skin ? &*host.skin : nullptr;
        const MenuTheme theme    = MenuThemeFor(settings, skin);
        MenuLook look{theme, host.textFont, kMarcellusSC, settings.menuFontSize, m_Look.textLift, host.developer,
            MenuColorsFor(settings, skin), theme == MenuTheme::GameWindow ? std::optional(skin->frame) : std::nullopt};
        if (m_Document != nullptr && look == m_Look) return true;
        look.textLift = LiftOf(host.textFont, settings.menuFontSize);
        if (m_Document != nullptr && look.SameFrame(m_Look))
        {
            m_Document->SetStyleSheetContainer(Rml::Factory::InstanceStyleSheetString(MenuStyle(look)));
            m_Look = std::move(look);
            return true;
        }
        Close();
        m_Document = host.context->LoadDocumentFromMemory(MenuDocument(look), kDocumentName);
        if (m_Document == nullptr) return false;
        m_Look       = std::move(look);
        m_FontPixels = 0.0f;
        m_Opacity    = 0;
        m_Sidebar    = m_Document->GetElementById(kMenuSidebarId);
        m_Page       = m_Document->GetElementById(kMenuPageId);
        m_PageTitle  = m_Document->GetElementById(kMenuPageTitleId);
        m_Popup      = m_Document->GetElementById(kMenuPopupId);
        m_Tip        = m_Document->GetElementById(kMenuTipId);
        for (const Rml::EventId id : {Rml::EventId::Click, Rml::EventId::Change, Rml::EventId::Dragdrop, Rml::EventId::Dragend,
                 Rml::EventId::Mouseover, Rml::EventId::Mousemove})
            m_Document->AddEventListener(id, this);
        // On their way down: taking and losing the keyboard do not come back up, and a text box keeps its keys.
        for (const Rml::EventId id : {Rml::EventId::Focus, Rml::EventId::Blur, Rml::EventId::Keydown})
            m_Document->AddEventListener(id, this, true);
        return m_Sidebar != nullptr && m_Page != nullptr && m_PageTitle != nullptr && m_Popup != nullptr && m_Tip != nullptr;
    }

    void RmlMenu::ApplyBlocks(const std::vector<FormBlock>& blocks)
    {
        bool replaced = false;
        size_t card   = 0;
        bool same     = true; // the cards are the ones shown, in their order
        for (const FormBlock& block : blocks)
        {
            if (block.id == kSidebarBlock) continue;
            same = same && card < m_Cards.size() && m_Cards[card].id == block.id;
            ++card;
        }
        same = same && card == m_Cards.size();
        card = 0;
        std::string page;
        for (const FormBlock& block : blocks)
        {
            if (block.id == kSidebarBlock)
            {
                if (block.rml == m_SidebarRml) continue;
                m_Sidebar->SetInnerRML(block.rml);
                m_SidebarRml = block.rml;
                replaced     = true;
                continue;
            }
            if (!same)
            {
                page += std::string("<div class=\"") + (block.bare ? kMenuFootClass : kMenuCardClass) + "\" id=\"" + block.id + "\">" +
                        block.rml + "</div>";
                continue;
            }
            Card& shown = m_Cards[card++];
            if (shown.rml == block.rml) continue;
            if (Rml::Element* element = Find(block.id)) element->SetInnerRML(block.rml);
            shown.rml = block.rml;
            replaced  = true;
        }
        if (!same)
        {
            m_Page->SetInnerRML(page);
            m_Cards.clear();
            for (const FormBlock& block : blocks)
                if (block.id != kSidebarBlock) m_Cards.push_back(Card{block.id, block.rml});
            replaced = true;
        }
        if (!replaced) return;
        Index();
        m_Applied.clear(); // new elements show nothing yet
    }

    void RmlMenu::ApplyValues(const std::vector<FormValue>& values)
    {
        Rml::Element* focus = m_Document->GetFocusLeafNode();
        for (const FormValue& value : values)
        {
            const std::string key = KeyOf(value);
            const Applied now{value.text, value.on};
            const auto known = m_Applied.find(key);
            if (known != m_Applied.end() && known->second == now) continue;
            Rml::Element* element = Find(value.id);
            if (element == nullptr) continue;
            switch (value.kind)
            {
            case FormValueKind::Class: element->SetClass(value.name, value.on); break;
            case FormValueKind::Text: element->SetInnerRML(EscapedRml(value.text)); break;
            case FormValueKind::Markup: element->SetInnerRML(value.text); break;
            case FormValueKind::Property: element->SetProperty(value.name, value.text); break;
            case FormValueKind::Attribute:
                if (value.text.empty())
                    element->RemoveAttribute(value.name);
                else
                    element->SetAttribute(value.name, value.text);
                break;
            case FormValueKind::Value:
            {
                auto* control = rmlui_dynamic_cast<Rml::ElementFormControl*>(element);
                if (control == nullptr) continue;
                // A text box being typed in keeps what is typed: it is set once it is left.
                if (element == focus && IsText(AsInput(element))) continue;
                if (control->GetValue() != value.text) control->SetValue(value.text);
                break;
            }
            }
            m_Applied[key] = now;
        }
    }

    void RmlMenu::ApplyPopup(const std::optional<FormPopup>& popup)
    {
        Rml::Element* anchor = popup ? Find(popup->anchor) : nullptr;
        m_Popup->SetClass(kHiddenClass, anchor == nullptr);
        m_Popup->SetClass("open", anchor != nullptr);
        if (anchor == nullptr)
        {
            if (!m_PopupRml.empty())
            {
                m_Popup->SetInnerRML("");
                m_PopupRml.clear();
                Index();
            }
            return;
        }
        if (popup->rml != m_PopupRml)
        {
            m_Popup->SetInnerRML(popup->rml);
            m_PopupRml = popup->rml;
            Index();
            m_Applied.clear();
        }
        // Under its control, at least as wide, in the window's own space.
        const Rml::Vector2f window = m_Document->GetAbsoluteOffset(Rml::BoxArea::Border);
        const Rml::Vector2f at     = anchor->GetAbsoluteOffset(Rml::BoxArea::Border) - window;
        const Rml::Vector2f size   = anchor->GetBox().GetSize(Rml::BoxArea::Border);
        SetPx(m_Popup, Rml::PropertyId::Left, std::round(at.x));
        SetPx(m_Popup, Rml::PropertyId::Top, std::round(at.y + size.y + kPopupDrop));
        SetPx(m_Popup, Rml::PropertyId::MinWidth, size.x);
    }

    void RmlMenu::ShowTip(Rml::Element* under, float x, float y)
    {
        while (under != nullptr && !under->HasAttribute(kMenuTipAttribute))
            under = under->GetParentNode();
        const std::string text = under != nullptr ? under->GetAttribute<Rml::String>(kMenuTipAttribute, "") : std::string();
        m_Tip->SetClass(kHiddenClass, text.empty());
        if (text.empty()) return;
        if (text != m_TipShown)
        {
            m_Tip->SetInnerRML(EscapedRml(text));
            m_TipShown = text;
        }
        // From the pointer toward the window's middle: there it has room whichever edge the pointer is near, and it
        // is never under the pointer.
        const Rml::Vector2f window = m_Document->GetAbsoluteOffset(Rml::BoxArea::Border);
        const Rml::Vector2f size   = m_Document->GetBox().GetSize(Rml::BoxArea::Border);
        const Rml::Vector2f in(x - window.x, y - window.y);
        const float gap     = kTipDrop * 0.5f;
        const bool leftward = in.x > size.x / 2.0f, upward = in.y > size.y / 2.0f;
        m_Tip->RemoveProperty(leftward ? Rml::PropertyId::Left : Rml::PropertyId::Right);
        m_Tip->RemoveProperty(upward ? Rml::PropertyId::Top : Rml::PropertyId::Bottom);
        if (leftward)
            SetPx(m_Tip, Rml::PropertyId::Right, std::round(size.x - in.x + gap));
        else
            SetPx(m_Tip, Rml::PropertyId::Left, std::round(in.x + gap));
        if (upward)
            SetPx(m_Tip, Rml::PropertyId::Bottom, std::round(size.y - in.y + gap));
        else
            SetPx(m_Tip, Rml::PropertyId::Top, std::round(in.y + kTipDrop));
    }

    // A key in a stretch that is not the stretch's own: Backspace at its start and Delete at its end are for the tag
    // beside it, the arrows step past it and Enter breaks the line at the caret. True when the stretch must not get
    // the key: the keyboard moved, or the line takes it.
    bool RmlMenu::RunKey(Rml::ElementFormControlInput* input, const std::string& id, Rml::Input::KeyIdentifier key)
    {
        const std::string value = input->GetValue();
        int start = 0, end = 0;
        input->GetSelection(&start, &end, nullptr);
        const bool atStart = start == 0 && end == 0, atEnd = start == end && ByteOf(value, start) == value.size();
        if (key == Rml::Input::KI_RETURN || key == Rml::Input::KI_NUMPADENTER)
        {
            m_Events.push_back(FormEvent{id, kBreakEvent, value, {}, ByteOf(value, start)});
            return true;
        }
        if (key == Rml::Input::KI_BACK && atStart)
            m_Events.push_back(FormEvent{id, kKillEvent, kKillBack + value});
        else if (key == Rml::Input::KI_DELETE && atEnd)
            m_Events.push_back(FormEvent{id, kKillEvent, kKillForward + value});
        else if (key == Rml::Input::KI_LEFT && atStart)
        {
            Rml::ElementFormControlInput* before = RunBeside(input, -1);
            if (before == nullptr) return false;
            FocusAt(before, CharacterOf(before->GetValue(), before->GetValue().size()));
            return true;
        }
        else if (key == Rml::Input::KI_RIGHT && atEnd)
        {
            Rml::ElementFormControlInput* after = RunBeside(input, 1);
            FocusAt(after, 0);
            return after != nullptr;
        }
        return false;
    }

    // Each stretch of a line box is as wide as what is typed in it, so that its tags sit against its letters.
    void RmlMenu::FitRuns()
    {
        Rml::ElementList runs;
        m_Page->GetElementsByClassName(runs, kRunClass);
        for (Rml::Element* run : runs)
        {
            Rml::ElementFormControlInput* input = AsInput(run);
            if (input == nullptr) continue;
            const float width = static_cast<float>(Rml::ElementUtilities::GetStringWidth(run, input->GetValue())) +
                                std::round(kCaretRoom * m_FontPixels);
            const Rml::Property* had = run->GetLocalProperty(Rml::PropertyId::Width);
            if (had == nullptr || had->Get<float>() != width) SetPx(run, Rml::PropertyId::Width, width);
        }
    }

    // As a filter, the window is drawn whole into a layer that is laid over the game at this opacity, so its parts do
    // not show through each other; at full opacity it needs no layer.
    void RmlMenu::ApplyOpacity()
    {
        if (m_Opacity >= kMaxOpacity)
            m_Document->RemoveProperty(Rml::PropertyId::Filter);
        else
            m_Document->SetProperty("filter", std::string(kOpacityFilter) + "(" + RcssNumber(static_cast<float>(m_Opacity) / kPercent) + ")");
    }

    void RmlMenu::ProcessEvent(Rml::Event& event)
    {
        if (m_Document == nullptr) return;
        Rml::Element* target = event.GetTargetElement();
        Rml::Element* named  = WithId(target);
        const std::string id = named != nullptr ? named->GetId() : std::string();
        switch (event.GetId())
        {
        case Rml::EventId::Click:
            if (id == kMenuCloseId) m_Closed = true;
            m_Reload |= id == kMenuReloadId;
            // The picker's own ground, between its sliders, is the picker: a click there is one on its control.
            // A click in a line box beside its stretches is a click at its end.
            if (target->IsClassSet(kLineBoxClass))
                if (Rml::ElementFormControlInput* last = AsInput(target->GetLastChild()))
                    FocusAt(last, CharacterOf(last->GetValue(), last->GetValue().size()));
            m_Events.push_back(FormEvent{named == m_Popup ? m_State.form.popup + kPopupGround : id, kClickEvent, {}});
            m_Tip->SetClass(kHiddenClass, true);
            break;
        case Rml::EventId::Change:
            if (AsInput(target) != nullptr)
                m_Events.push_back(FormEvent{id, kChangeEvent, event.GetParameter<Rml::String>("value", "")});
            break;
        case Rml::EventId::Focus:
            if (Rml::ElementFormControlInput* input = AsInput(target); IsText(input))
            {
                m_FocusValue[id] = input->GetValue();
                if (target->IsClassSet(kRunClass)) target->GetParentNode()->SetClass(kTypingClass, true);
            }
            break;
        case Rml::EventId::Blur:
            if (Rml::ElementFormControlInput* input = AsInput(target); IsText(input))
            {
                const std::string value = input->GetValue();
                int start = 0, end = 0;
                input->GetSelection(&start, &end, nullptr);
                m_Events.push_back(FormEvent{id, kCaretEvent, std::to_string(ByteOf(value, start))});
                if (const auto was = m_FocusValue.find(id); was == m_FocusValue.end() || was->second != value)
                    m_Events.push_back(FormEvent{id, kCommitEvent, value});
                // What it shows from here is the form's to set again.
                m_Applied.erase(id + kKeySeparator + static_cast<char>('0' + static_cast<int>(FormValueKind::Value)));
                if (target->IsClassSet(kRunClass)) target->GetParentNode()->SetClass(kTypingClass, false);
            }
            break;
        case Rml::EventId::Keydown:
            if (Rml::ElementFormControlInput* input = AsInput(target); IsText(input))
            {
                const auto key = static_cast<Rml::Input::KeyIdentifier>(event.GetParameter<int>("key_identifier", 0));
                if (key == Rml::Input::KI_ESCAPE)
                {
                    // Escape takes the typing back.
                    if (const auto was = m_FocusValue.find(id); was != m_FocusValue.end()) input->SetValue(was->second);
                    input->Blur();
                }
                // RmlUi's text box shows its caret for every arrow it is given, with or without the keyboard.
                else if (target->IsClassSet(kRunClass) && RunKey(input, id, key))
                    event.StopPropagation();
                else if (key == Rml::Input::KI_RETURN || key == Rml::Input::KI_NUMPADENTER)
                    input->Blur();
            }
            break;
        case Rml::EventId::Dragdrop:
            if (auto* dragged = static_cast<Rml::Element*>(event.GetParameter<void*>("drag_element", nullptr));
                dragged != nullptr && !LetGoOn(dragged, event))
            {
                FormEvent drop{id, kDropEvent, dragged->GetId(), dragged->GetAttribute<Rml::String>(kCarriesAttribute, "")};
                // A stretch is as wide as its letters from its content's left: the pointer says between which.
                if (Rml::ElementFormControlInput* run = named != nullptr && named->IsClassSet(kRunClass) ? AsInput(named) : nullptr)
                    drop.at = ByteUnder(run->GetValue(),
                        static_cast<float>(event.GetParameter<int>("mouse_x", 0)) - run->GetAbsoluteOffset(Rml::BoxArea::Content).x,
                        [&](std::string_view start) {
                            return static_cast<float>(Rml::ElementUtilities::GetStringWidth(run, Rml::String(start)));
                        });
                m_Events.push_back(std::move(drop));
            }
            break;
        case Rml::EventId::Dragend:
            if (!LetGoOn(target, event)) m_Events.push_back(FormEvent{id, kDragEndEvent, {}, {}});
            break;
        case Rml::EventId::Mouseover:
        case Rml::EventId::Mousemove:
            ShowTip(target, event.GetParameter<float>("mouse_x", 0.0f), event.GetParameter<float>("mouse_y", 0.0f));
            break;
        default: break;
        }
    }

    namespace
    {
        // The first click that comes after a text box said what was typed in it; the end when there is none.
        std::vector<FormEvent>::iterator ClickAfterTyping(std::vector<FormEvent>& events)
        {
            const auto typed = std::ranges::find(events, std::string_view(kCommitEvent), &FormEvent::type);
            return std::find_if(typed, events.end(), [](const FormEvent& event) { return event.type == kClickEvent; });
        }
    }

    Form RmlMenu::Pass(std::vector<FormEvent> events, const MenuHost& host, Settings& settings, const MenuReadings& readings,
        const std::vector<std::string>& fonts)
    {
        Form form(std::move(events), m_State.form, host.keyDown);
        if (Rml::Element* focus = host.context->GetFocusElement(); focus != nullptr && focus->GetOwnerDocument() == m_Document)
            form.focused = focus->GetId();
        BuildMenu(form, settings, readings, m_State, fonts, host.menuFonts, *m_Layout);
        return form;
    }

    bool RmlMenu::Update(const MenuHost& host, Settings& settings, const MenuReadings& readings,
        const std::vector<std::string>& fonts, double version)
    {
        m_Layout = host.layout;
        if (std::exchange(m_Closed, false)) open = false;
        if (!open || host.context == nullptr)
        {
            bool changed = false;
            if (m_Document != nullptr && m_Visible && host.context != nullptr)
            {
                // A box that has the keyboard as the window goes gives it up and says what was typed in it, which a
                // last pass takes with whatever else came this frame: the click on the close button comes after it.
                if (Rml::Element* focus = host.context->GetFocusElement(); focus != nullptr && focus->GetOwnerDocument() == m_Document)
                    focus->Blur();
                changed = Pass(std::exchange(m_Events, {}), host, settings, readings, fonts).changed;
                m_Document->SetClass("open", false);
                m_Document->Hide();
                m_Visible = false;
            }
            m_Events.clear();
            m_State.Leave();
            return changed;
        }
        if (!Document(host, settings)) return false;
        const auto pixels = static_cast<float>(settings.menuFontSize);
        if (pixels != m_FontPixels)
        {
            m_FontPixels = pixels;
            // What the pointer is on, a size arrow just clicked, stays under it as the window grows or shrinks, as
            // far as the screen lets the window move: the next click lands on the same arrow.
            Rml::Element* held = m_Visible ? host.context->GetHoverElement() : nullptr;
            if (held != nullptr && held->GetOwnerDocument() != m_Document) held = nullptr;
            const Rml::Vector2f before = held != nullptr ? held->GetAbsoluteOffset(Rml::BoxArea::Border) : Rml::Vector2f{};
            SetPx(m_Document, Rml::PropertyId::FontSize, pixels);
            if (held != nullptr)
            {
                m_Document->UpdateDocument();
                const Rml::Vector2f drift = held->GetAbsoluteOffset(Rml::BoxArea::Border) - before;
                const Rml::Vector2f at    = m_Document->GetAbsoluteOffset(Rml::BoxArea::Border);
                SetPx(m_Document, Rml::PropertyId::Left, std::max(0.0f, std::round(at.x - drift.x)));
                SetPx(m_Document, Rml::PropertyId::Top, std::max(0.0f, std::round(at.y - drift.y)));
            }
        }
        if (settings.menuOpacity != m_Opacity)
        {
            m_Opacity = settings.menuOpacity;
            ApplyOpacity();
        }
        if (m_Revealing) m_Document->RemoveProperty(Rml::PropertyId::Visibility);
        m_Revealing = false;
        if (!m_Visible)
        {
            m_Document->Show(Rml::ModalFlag::None, Rml::FocusFlag::Auto);
            // RmlUi applies an animation once its clock has passed the start, so the frame that sets one draws the
            // window whole: hidden that frame, it comes in already fading.
            m_Document->SetClass("open", true);
            m_Document->SetProperty(Rml::PropertyId::Visibility, Rml::Property(Rml::Style::Visibility::Hidden));
            m_Revealing = true;
            m_Visible   = true;
            char text[32];
            std::snprintf(text, sizeof(text), "v%g", version);
            if (Rml::Element* shown = m_Document->GetElementById(kMenuVersionId)) shown->SetInnerRML(text);
            if (Rml::Element* button = m_Document->GetElementById(kMenuReloadId))
            {
                button->SetInnerRML(EscapedRml(host.layout->WordOf(WindowWord::Reload).name));
                button->SetAttribute(kMenuTipAttribute, std::string(Said(host.layout->WordOf(WindowWord::Reload).tip)));
            }
        }
        const bool moved = Place(host, settings, pixels);

        // No tip stays once the pointer has left the window, and a slider gives up the keyboard it kept after a drag
        // for the arrows to nudge it: off the window those are the game's, and its camera's keys moved the slider.
        if (!m_Document->IsPseudoClassSet("hover"))
        {
            m_Tip->SetClass(kHiddenClass, true);
            // The keyboard is on a part of the slider, and from any part its keys reach the slider: the window
            // takes it whole.
            for (Rml::Element* part = host.context->GetFocusElement(); part != nullptr && part != m_Document; part = part->GetParentNode())
                if (Rml::ElementFormControlInput* input = AsInput(part); input != nullptr && !IsText(input) && input->GetOwnerDocument() == m_Document)
                {
                    m_Document->Focus();
                    break;
                }
        }
        // A click acts on what was typed before it. With both in one frame the typing gets a pass of its own first:
        // the click may open another page or fold the card, and the line typed in would never be built to take it.
        std::vector<FormEvent> events = std::exchange(m_Events, {});
        bool typed                    = false;
        if (const auto click = ClickAfterTyping(events); click != events.end())
        {
            typed = Pass(std::vector<FormEvent>(events.begin(), click), host, settings, readings, fonts).changed;
            events.erase(events.begin(), click);
        }
        const Form form = Pass(std::move(events), host, settings, readings, fonts);
        // A card built anew takes its boxes with it: the one that had the keyboard gets it back, where its caret was.
        std::string typingIn;
        int caret = 0;
        if (Rml::ElementFormControlInput* input = AsInput(host.context->GetFocusElement()); IsText(input) && !input->GetId().empty())
        {
            typingIn = input->GetId();
            input->GetSelection(&caret, nullptr, nullptr);
        }

        const char* title = m_Layout->PageOf(m_State.page).name;
        if (m_TitleShown != title)
        {
            m_PageTitle->SetInnerRML(EscapedRml(title));
            m_TitleShown = title;
        }
        ApplyBlocks(form.blocks);
        // Whatever a card said of its controls as it was replaced is of controls that are gone.
        m_Events.clear();
        ApplyPopup(form.popup);
        ApplyValues(form.values);
        if (form.focus)
        {
            if (Rml::ElementFormControlInput* input = AsInput(Find(form.focus->id)))
                FocusAt(input, CharacterOf(input->GetValue(), form.focus->caret));
        }
        else if (Rml::ElementFormControlInput* input = AsInput(Find(typingIn)); input != nullptr && input != host.context->GetFocusElement())
            FocusAt(input, caret);
        FitRuns();
        if (form.scrollToTop) m_ScrollTo = 0.0f;
        if (m_ScrollTo)
        {
            // A page just opened has no height yet: it is laid out first.
            host.context->Update();
            m_Page->SetScrollTop(*m_ScrollTo);
            m_ScrollTo.reset();
        }
        return form.changed || typed || moved;
    }

    bool RmlMenu::Place(const MenuHost& host, Settings& settings, float pixels)
    {
        if (!m_Placed)
        {
            m_Placed = true;
            const Rml::Vector2i screen = host.context->GetDimensions();
            if (settings.menuWidth > 0 && settings.menuHeight > 0)
            {
                SetPx(m_Document, Rml::PropertyId::Left, static_cast<float>(settings.menuX));
                SetPx(m_Document, Rml::PropertyId::Top, static_cast<float>(settings.menuY));
                SetPx(m_Document, Rml::PropertyId::Width, static_cast<float>(settings.menuWidth));
                SetPx(m_Document, Rml::PropertyId::Height, static_cast<float>(settings.menuHeight));
            }
            else
            {
                SetPx(m_Document, Rml::PropertyId::Left, std::round((static_cast<float>(screen.x) - kWindowWidth * pixels) / 2.0f));
                SetPx(m_Document, Rml::PropertyId::Top, std::round((static_cast<float>(screen.y) - kWindowHeight * pixels) / 2.0f));
            }
            return false;
        }
        // Once it is laid out: a window not yet measured has no size.
        const Rml::Vector2f size = m_Document->GetBox().GetSize(Rml::BoxArea::Border);
        if (size.x <= 0.0f || size.y <= 0.0f) return false;
        const Rml::Vector2f at = m_Document->GetAbsoluteOffset(Rml::BoxArea::Border);
        const int x = static_cast<int>(std::lround(at.x)), y = static_cast<int>(std::lround(at.y));
        const int width = static_cast<int>(std::lround(size.x)), height = static_cast<int>(std::lround(size.y));
        const std::array<int, 4> now{std::max(0, x), std::max(0, y), width, height};
        if (!m_OpenedAt)
        {
            m_OpenedAt = now;
            return false;
        }
        // Only a window the player moved or sized is kept: one left where it opened opens in the middle of any
        // screen.
        const bool kept = settings.menuWidth > 0;
        if (kept ? now == std::array<int, 4>{settings.menuX, settings.menuY, settings.menuWidth, settings.menuHeight}
                 : now == *m_OpenedAt)
            return false;
        settings.menuX      = now[0];
        settings.menuY      = now[1];
        settings.menuWidth  = now[2];
        settings.menuHeight = now[3];
        return true;
    }
}
