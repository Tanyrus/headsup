#pragma once

#include "form.h"
#include "menu_state.h"
#include "settings.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace headsup
{
    // What the pages read from the frame that just ended: the Debug page's figures, and what other pages ask.
    struct MenuReadings
    {
        uint32_t outlinedMobs = 0, meshes = 0, nameplates = 0;
        double frameMs = 0.0;
        double lastSaveMs = 0.0;   // the last settings write
        size_t lastSaveKeys = 0;   // and how many keys it wrote
        bool stencilAvailable = true;
        bool drewInScene = false;  // nameplates went into the game's scene image, behind walls
        bool hooked = false;       // the game's name routine
        uint32_t names = 0, replaced = 0, hidden = 0; // the game's names, those HeadsUp drew, those no one drew
        int level = 0;             // yours; 0 while unknown
        bool sitting = false;
        int barTitlePixels = 0;    // the title size the bar drew last frame, 0 when it is off
        const std::vector<std::string>* seenWindows = nullptr; // the game's windows seen under Game help's bar
        const std::vector<std::string>* cursorFiles = nullptr; // the cursors folder's files, without their extensions
    };

    // The Game Windows card's own state: its filter and whether it lists every window.
    struct WindowFilter
    {
        std::string text;
        bool all = false;
    };

    // What the settings window keeps between passes that is no setting.
    struct MenuFormState
    {
        FormState form;
        Page page          = Page::Entities;
        int widget         = 0;     // the Widgets page's picked widget, by BarWidget
        bool confirmReset  = false; // the reset button was pressed once and asks for a second press
        bool debugRequested = false;
        bool cursorsRequested = false; // the Cursor Style list was opened: the cursors folder is to be read again
        KeyCapture keyCapture;      // the Mouse Cursor page's
        KeyCapture shotKeyCapture;  // screenshot mode's
        WindowFilter windows;
        DesignEditor design;

        // What a page was in the middle of is dropped when it is left, by another page or by the window closing: a
        // row waiting for a key would take one pressed for the game, and a reset asked once would wait armed.
        void Leave()
        {
            form.popup.clear();
            confirmReset   = false;
            keyCapture     = {};
            shotKeyCapture = {};
        }
    };

    // One pass of the window: the sidebar, then the open page's cards, as the layout orders and words them. fonts:
    // FontChoices, for the nameplates' and the bar's fonts. menuFonts: the fonts the window itself can be set in.
    void BuildMenu(Form& form, Settings& settings, const MenuReadings& readings, MenuFormState& state,
        const std::vector<std::string>& fonts, const std::vector<std::string>& menuFonts,
        const WindowLayout& layout = kWindowLayout);
}
