#pragma once

#include "Ashita.h"
#include "menu_state.h"
#include "native_hook.h"
#include "settings.h"
#include "tracker.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace headsup
{
    // What the Debug page shows, from the frame that just ended.
    struct MenuStatus
    {
        uint32_t outlinedMobs;
        uint32_t meshes;
        uint32_t nameplates;
        double frameMs;
        double lastSaveMs; // the last settings write, for the Debug page
        bool stencilAvailable;
        bool drewInScene; // nameplates went into the game's scene image, behind walls
        bool hooked; // the game's name routine
        NameStats names;
        PlayerState player;
        double version; // shown in the title bar, so a tester's build can be told apart
        int barTitlePixels; // the title size the bar drew last frame, 0 when it is off
        const std::vector<std::string>* seenWindows; // the game's windows seen under Game help's bar, by short name
    };

    // The Game Windows card's own state: its filter and whether it lists every window.
    struct WindowList
    {
        char filter[32] = {};
        bool all        = false;
    };

    // The "Add a key" button's wait for a key: those held when it was pressed do not count.
    struct KeyCapture
    {
        bool on = false;
        std::vector<uint8_t> held;
    };

    // A kind page's own state, begun anew when another kind's page opens: the line last typed in, which takes a
    // clicked tag, the three lines' boxes, the Line Style tab that is open and the name being typed for Friends.
    struct DesignEditor
    {
        PlateKind kind = PlateKind::Mobs;
        int line       = 0;
        int styleTab   = 0;
        LineDraft drafts[kPlateLines] = {};
        char friendName[kMaxCharacterName + 1] = {};
    };

    // A font file in Windows' Fonts folder.
    std::string WindowsFontPath(const char* file);

    // The /headsup settings window. Uses only IGuiManager functions that are safe across the MinGW/MSVC ABI
    // boundary (see tools/abi_check.py).
    class Menu
    {
    public:
        bool open = false;

        ~Menu();
        // Before the plugin unloads: the thread listing Windows' fonts runs the plugin's code.
        void WaitForFonts();

        // True when a change should be saved now.
        bool Draw(IGuiManager* gui, Settings& settings, const MenuStatus& status);
        // True once after the Debug page's button was pressed: write the /hu debug report.
        bool TakeDebugRequest();
        // The bar's fonts, loaded into ImGui: its text font and its title font. Without them the menu draws in
        // Ashita's.
        void SetFonts(ImFont* text, ImFont* title, float textScale);
        bool HasFonts() const { return m_TextFont != nullptr; }
        // Opens the window at the page named, as its sidebar spells it in lower case without spaces ("topbar");
        // false for a name no page has.
        bool OpenPage(const std::string& name);
        // Scrolls the page this far down when it is next drawn: to its top when the page changes, and where a
        // developer's command asks to see its lower cards.
        void ScrollTo(float pixels);

    private:
        bool m_DebugRequested = false;
        bool m_ConfirmReset   = false; // the reset button was pressed once and asks for a second press
        float m_ScrollTo      = 0.0f;  // where ScrollTo asked for
        int m_ScrollFrames    = 0;     // frames left to ask ImGui for it: a page just opened has no height yet
        int m_ShownPage       = 0;     // the page drawn last, to tell a change
        Page m_Page           = Page::Entities;
        std::unordered_set<std::string> m_Collapsed; // the ids of the collapsed cards: window state, never saved
        int m_Widget         = 0;     // the Widgets page's picked widget, by BarWidget
        // Windows takes up to a second to list its fonts, which froze the game when a page with a font dropdown first
        // showed: a thread lists them once the window first opens, and m_Fonts has the bundled ones until it is done.
        void ListFonts(const std::string& current);
        std::vector<std::string> m_Fonts;     // FontChoices
        std::vector<std::string> m_Installed; // the thread's, read once it has ended
        void* m_FontThread = nullptr;
        bool m_FontsAsked  = false;
        KeyCapture m_KeyCapture;
        WindowList m_Windows;
        DesignEditor m_Design;
        ImFont* m_TextFont  = nullptr;
        ImFont* m_TitleFont = nullptr;
        float m_TextScale   = 1.0f; // m_TextFont's size over Ashita's font size
        std::unordered_map<std::string, float> m_CardHeights; // each card's height last frame, for its ground
    };
}
