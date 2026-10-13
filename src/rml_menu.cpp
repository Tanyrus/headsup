#include "rml_menu.h"

#include "fonts.h"
#include "menu_fonts.h"
#include "menu_sizes.h"
#include "utf16.h"

#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <utility>

namespace headsup
{
    namespace
    {
        constexpr const char* kDocumentName = "settings";
        constexpr float kPopupDrop  = 2.0f;  // pixels between a control and its popup
        constexpr float kTipDrop    = 18.0f; // pixels from the pointer to its tip
        constexpr char kKeySeparator = '\x1f';

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
    bool RmlMenu::TakeCursorsRequest() { return std::exchange(m_State.cursorsRequested, false); }

    bool RmlMenu::OpenPage(const std::string& name)
    {
        const std::optional<Page> page = PageNamed(name);
        if (!page) return false;
        m_State.page = *page;
        open         = true;
        m_ScrollTo   = 0.0f;
        return true;
    }

    std::optional<Rml::Vector2f> RmlMenu::CenterOf(const std::string& id, bool scroll)
    {
        Rml::Element* element = m_Document != nullptr ? m_Document->GetElementById(id) : nullptr;
        if (element == nullptr) return std::nullopt;
        // What is in the popup or the frame is in view already; a card's control may be scrolled out of it.
        for (Rml::Element* up = scroll ? element : nullptr; up != nullptr; up = up->GetParentNode())
            if (up == m_Page)
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
        MenuLook look{settings.menuTheme, host.textFont, kMarcellusSC};
        if (m_Document != nullptr && look == m_Look) return true;
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
        for (const Rml::EventId id : {Rml::EventId::Click, Rml::EventId::Change, Rml::EventId::Dragdrop,
                 Rml::EventId::Mouseover, Rml::EventId::Mousemove, Rml::EventId::Animationend})
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
                page += std::string("<div class=\"") + kMenuCardClass + "\" id=\"" + block.id + "\">" + block.rml + "</div>";
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
        m_Popup->SetClass("hidden", anchor == nullptr);
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
        m_Tip->SetClass("hidden", text.empty());
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

    void RmlMenu::ApplyOpacity()
    {
        m_Document->SetProperty(Rml::PropertyId::Opacity, Rml::Property(static_cast<float>(m_Opacity) / kPercent, Rml::Unit::NUMBER));
    }

    void RmlMenu::ProcessEvent(Rml::Event& event)
    {
        if (m_Document == nullptr) return;
        // The fade the window opens with ends by taking its opacity off the document: it is put back at once.
        if (event.GetId() == Rml::EventId::Animationend && event.GetTargetElement() == m_Document)
        {
            ApplyOpacity();
            return;
        }
        Rml::Element* target = event.GetTargetElement();
        Rml::Element* named  = WithId(target);
        const std::string id = named != nullptr ? named->GetId() : std::string();
        switch (event.GetId())
        {
        case Rml::EventId::Click:
            if (id == kMenuCloseId) m_Closed = true;
            m_Events.push_back(FormEvent{id, kClickEvent, {}});
            m_Tip->SetClass("hidden", true);
            break;
        case Rml::EventId::Change:
            if (AsInput(target) != nullptr)
                m_Events.push_back(FormEvent{id, kChangeEvent, event.GetParameter<Rml::String>("value", "")});
            break;
        case Rml::EventId::Focus:
            if (Rml::ElementFormControlInput* input = AsInput(target); IsText(input))
            {
                m_FocusValue[id] = input->GetValue();
                m_Events.push_back(FormEvent{id, kFocusEvent, {}});
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
                else if (key == Rml::Input::KI_RETURN || key == Rml::Input::KI_NUMPADENTER)
                    input->Blur();
            }
            break;
        case Rml::EventId::Dragdrop:
            if (auto* dragged = static_cast<Rml::Element*>(event.GetParameter<void*>("drag_element", nullptr)))
                m_Events.push_back(FormEvent{id, kDropEvent, dragged->GetId()});
            break;
        case Rml::EventId::Mouseover:
        case Rml::EventId::Mousemove:
            ShowTip(target, event.GetParameter<float>("mouse_x", 0.0f), event.GetParameter<float>("mouse_y", 0.0f));
            break;
        default: break;
        }
    }

    bool RmlMenu::Update(const MenuHost& host, Settings& settings, const MenuReadings& readings,
        const std::vector<std::string>& fonts, float em, double version)
    {
        if (std::exchange(m_Closed, false)) open = false;
        if (!open || host.context == nullptr)
        {
            if (m_Document != nullptr && m_Visible)
            {
                m_Document->SetClass("open", false);
                m_Document->Hide();
                m_Visible = false;
            }
            m_Events.clear();
            m_State.form.popup.clear();
            return false;
        }
        if (!Document(host, settings)) return false;
        const MenuFont* font = MenuFontNamed(m_Look.textFont);
        const float pixels   = std::round(em * static_cast<float>(settings.menuScale) / kPercent * (font != nullptr ? font->scale : 1.0f));
        if (pixels != m_FontPixels)
        {
            m_FontPixels = pixels;
            SetPx(m_Document, Rml::PropertyId::FontSize, pixels);
        }
        if (settings.menuOpacity != m_Opacity)
        {
            m_Opacity = settings.menuOpacity;
            ApplyOpacity();
        }
        if (!m_Visible)
        {
            m_Document->Show(Rml::ModalFlag::None, Rml::FocusFlag::Auto);
            m_Document->SetClass("open", true);
            m_Visible = true;
            char text[32];
            std::snprintf(text, sizeof(text), "v%g", version);
            if (Rml::Element* shown = m_Document->GetElementById(kMenuVersionId)) shown->SetInnerRML(text);
        }
        const bool moved = Place(host, settings, pixels);

        // No tip stays once the pointer has left the window.
        if (!m_Document->IsPseudoClassSet("hover")) m_Tip->SetClass("hidden", true);
        Form form(std::exchange(m_Events, {}), m_State.form, host.keyDown);
        BuildMenu(form, settings, readings, m_State, fonts, host.menuFonts);

        const char* title = kPages[static_cast<int>(m_State.page)].label;
        if (m_TitleShown != title)
        {
            m_PageTitle->SetInnerRML(EscapedRml(title));
            m_TitleShown = title;
        }
        ApplyBlocks(form.blocks);
        ApplyPopup(form.popup);
        ApplyValues(form.values);
        if (form.scrollToTop) m_ScrollTo = 0.0f;
        if (m_ScrollTo)
        {
            // A page just opened has no height yet: it is laid out first.
            host.context->Update();
            m_Page->SetScrollTop(*m_ScrollTo);
            m_ScrollTo.reset();
        }
        return form.changed || moved;
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
