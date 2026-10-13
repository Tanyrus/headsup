#pragma once

#include "form.h"
#include "menu_document.h"
#include "menu_form.h"
#include "settings.h"

#include <RmlUi/Core.h>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace headsup
{
    // What the settings window is given by where it runs.
    struct MenuHost
    {
        Rml::Context* context = nullptr;      // the one it is a document of; none while RmlUi is not running
        std::string textFont;                 // the family its text is set in, one RmlUi has
        std::vector<std::string> menuFonts;   // the fonts it can be set in, by their kMenuFonts names
        std::function<bool(uint8_t)> keyDown; // whether a key is down now, by its virtual-key code
    };

    // The /headsup settings window as an RmlUi document in the host's overlay. While it is open, each frame runs
    // the pages against the kit (menu_form, form) with what the player did since the last frame, replaces the
    // cards whose markup changed and sets the values that did.
    class RmlMenu : public Rml::EventListener
    {
    public:
        bool open = false;

        // True when a setting changed and should be saved. em: the size of the window's text in pixels, before the
        // window's own scale. fonts: FontChoices for the nameplates' and the bar's fonts.
        bool Update(const MenuHost& host, Settings& settings, const MenuReadings& readings,
            const std::vector<std::string>& fonts, float em, double version);
        // Closes the window's document; before the host stops.
        void Close();
        // True once after the Debug page's button was pressed: write the /hu debug report.
        bool TakeDebugRequest();
        // Opens the window at the page named, as its sidebar spells it in lower case without spaces ("toppanel");
        // false for a name no page has.
        bool OpenPage(const std::string& name);
        // Scrolls the page this far down when it is next drawn.
        void ScrollTo(float pixels) { m_ScrollTo = pixels; }

        void ProcessEvent(Rml::Event& event) override;
        // The middle of the element of this id, in the screen's pixels, scrolled into view first; nothing while the
        // window lacks it.
        std::optional<Rml::Vector2f> CenterOf(const std::string& id, bool scroll = true);

    private:
        struct Card
        {
            std::string id, rml;
        };
        struct Applied
        {
            std::string text;
            bool on = false;
            bool operator==(const Applied&) const = default;
        };

        // Builds the document again when its look changed. False when RmlUi refused it.
        bool Document(const MenuHost& host, const Settings& settings);
        // Puts the window where the settings have it, or in the screen's middle; then keeps the settings to where the
        // player moves and sizes it. True when they changed.
        bool Place(const MenuHost& host, Settings& settings, float pixels);
        void ApplyBlocks(const std::vector<FormBlock>& blocks);
        void ApplyValues(const std::vector<FormValue>& values);
        void ApplyPopup(const std::optional<FormPopup>& popup);
        // Every element with an id, found again after markup was replaced.
        void Index();
        void ShowTip(Rml::Element* under, float x, float y);
        Rml::Element* Find(const std::string& id) const;

        Rml::ElementDocument* m_Document = nullptr;
        MenuLook m_Look;              // what the document was built with
        float m_FontPixels = 0.0f;
        bool m_Visible     = false;
        bool m_Placed      = false;   // it has its first place
        std::optional<std::array<int, 4>> m_OpenedAt; // where it was first laid out: left, top, width, height
        Rml::Element* m_Sidebar   = nullptr;
        Rml::Element* m_Page      = nullptr;
        Rml::Element* m_PageTitle = nullptr;
        Rml::Element* m_Popup     = nullptr;
        Rml::Element* m_Tip       = nullptr;
        std::string m_SidebarRml, m_PopupRml, m_TitleShown, m_TipShown;
        std::vector<Card> m_Cards;
        std::unordered_map<std::string, Rml::Element*> m_ById;
        std::unordered_map<std::string, Applied> m_Applied; // what each value was last set to, by its id, kind and name
        std::unordered_map<std::string, std::string> m_FocusValue; // a text box's text as it took the keyboard
        std::vector<FormEvent> m_Events; // since the last pass
        MenuFormState m_State;
        std::optional<float> m_ScrollTo;
        bool m_Closed = false; // its close button was clicked
    };
}
