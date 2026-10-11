#pragma once

#include "Ashita.h"
#include "native_hook.h"
#include "settings.h"
#include "tracker.h"

#include <cstdint>
#include <string>
#include <unordered_map>
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

    // A font file in Windows' Fonts folder.
    std::string WindowsFontPath(const char* file);

    // The /headsup settings window. Uses only IGuiManager functions that are safe across the MinGW/MSVC ABI
    // boundary (see tools/abi_check.py).
    class Menu
    {
    public:
        bool open = false;

        // True when a change should be saved now.
        bool Draw(IGuiManager* gui, Settings& settings, const MenuStatus& status);
        // True once after the Debug page's button was pressed: write the /hu debug report.
        bool TakeDebugRequest();
        // The bar's fonts, loaded into ImGui: its text font and its title font. Without them the menu draws in
        // Ashita's.
        void SetFonts(ImFont* text, ImFont* title, float textScale);
        bool HasFonts() const { return m_TextFont != nullptr; }
        // Opens the window at the page named, as its sidebar spells it in lower case without spaces ("topbar"), on
        // its colors tab when colors; false for a name no page has.
        bool OpenPage(const std::string& name, bool colors);
        // Scrolls the page this far down when it is next drawn: to its top when the page changes, and where a
        // developer's command asks to see its lower cards.
        void ScrollTo(float pixels);

    private:
        bool m_DebugRequested = false;
        float m_ScrollTo      = 0.0f;  // where ScrollTo asked for
        int m_ScrollFrames    = 0;     // frames left to ask ImGui for it: a page just opened has no height yet
        int m_ShownPage       = 0;     // the page and tab drawn last, to tell a change
        int m_Page           = 0;
        bool m_ColorsTab     = false; // that page's "color settings" tab instead of "settings"
        uint64_t m_Collapsed = 0;     // one bit per collapsed section
        int m_Widget         = 0;     // the Widgets page's picked widget, by BarWidget
        std::vector<std::string> m_Fonts; // FontChoices, read when the settings window first opens
        KeyCapture m_KeyCapture;
        WindowList m_Windows;
        ImFont* m_TextFont  = nullptr;
        ImFont* m_TitleFont = nullptr;
        float m_TextScale   = 1.0f; // m_TextFont's size over Ashita's font size
        std::unordered_map<std::string, float> m_CardHeights; // each card's height last frame, for its ground
    };
}
