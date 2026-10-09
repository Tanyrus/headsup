#pragma once

#include "Ashita.h"
#include "native_hook.h"
#include "settings.h"
#include "tracker.h"

#include <cstdint>
#include <string>
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
    };

    // The "Add a key" button's wait for a key: those held when it was pressed do not count.
    struct KeyCapture
    {
        bool on = false;
        std::vector<uint8_t> held;
    };

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

    private:
        bool m_DebugRequested = false;
        int m_Page           = 0;
        bool m_ColorsTab     = false; // that page's "color settings" tab instead of "settings"
        uint32_t m_Collapsed = 0;     // one bit per collapsed section
        std::vector<std::string> m_Fonts; // FontChoices, read when the Global page's settings tab first opens
        KeyCapture m_KeyCapture;
    };
}
