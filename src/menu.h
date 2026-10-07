#pragma once

#include "settings.h"

#include <cstdint>
#include <string>
#include <vector>

struct IGuiManager;

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
        bool drewInScene;       // nameplates went into the game's scene image, behind walls
        uint32_t lettersInScene; // the game's name letters
        uint32_t lettersFromMobs;
        uint32_t lettersHidden;
        int playerLevel;
        bool sitting;
    };

    // The /headsup settings window. Uses only IGuiManager functions that are safe across the MinGW/MSVC ABI
    // boundary (see tools/abi_check.py).
    class Menu
    {
    public:
        bool open = false;

        // Draws the window when open. Returns true when a change should be saved now.
        bool Draw(IGuiManager* gui, Settings& settings, const MenuStatus& status);
        // True once after the Debug page's button was pressed: write the /hu debug report.
        bool TakeDebugRequest();

    private:
        bool m_DebugRequested = false;
        int m_Page           = 0;     // the sidebar's selected page
        bool m_ColorsTab     = false; // that page's "Color Settings" tab instead of "Settings"
        uint32_t m_Collapsed = 0;     // one bit per collapsed section
        std::vector<std::string> m_Fonts; // installed in Windows, read when the menu first opens
    };
}
