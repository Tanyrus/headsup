#pragma once

#include "settings.h"

#include <cstdint>

struct IGuiManager;

namespace aggroglow
{
    struct MenuStatus
    {
        uint32_t outlinedMobs;
        uint32_t meshes;
        double frameMs;
        bool stencilAvailable;
    };

    // The /aggroglow settings window. Uses only IGuiManager functions that are safe across the MinGW/MSVC ABI
    // boundary (see tools/abi_check.py).
    class Menu
    {
    public:
        bool open = false;

        // Draws the window when open. Returns true when a change should be saved now.
        bool Draw(IGuiManager* gui, Settings& settings, const MenuStatus& status);
    };
}
